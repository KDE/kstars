/*
    SPDX-FileCopyrightText: 2016 Jasem Mutlaq <mutlaqja@ikarustech.com>.

    Based on lin_guider

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "internalguider.h"

#include "ekos_guide_debug.h"
#include "gmath.h"
#include "Options.h"
#include "auxiliary/kspaths.h"
#include "fitsviewer/fitsdata.h"
#include "fitsviewer/fitsview.h"
#include "guidealgorithms.h"
#include "ksnotification.h"
#include "ekos/auxiliary/stellarsolverprofileeditor.h"

#include <KMessageBox>

#include <random>
#include <chrono>
#include <QTimer>

#define MAX_GUIDE_STARS           10

namespace Ekos
{
InternalGuider::InternalGuider()
{
    // Create math object
    pmath.reset(new cgmath());
    connect(pmath.get(), &cgmath::newStarPosition, this, &InternalGuider::newStarPosition);
    connect(pmath.get(), &cgmath::guideStats, this, &InternalGuider::guideStats);

    // Do this so that stored calibration will be visible on the
    // guide options menu. Calibration will get restored again when needed.
    pmath->getMutableCalibration()->restore(
        pierSide, Options::reverseDecOnPierSideChange(), subBinX, subBinY, nullptr);

    state = GUIDE_IDLE;
}

bool InternalGuider::guide()
{
    if (state >= GUIDE_GUIDING)
    {
        return processGuiding();
    }

    if (state == GUIDE_SUSPENDED)
    {
        return true;
    }
    guideFrame->disconnect(this);

    pmath->start();

    m_starLostCounter = 0;
    m_highRMSCounter = 0;

    m_isFirstFrame = true;

    if (state == GUIDE_IDLE)
    {
        if (Options::saveGuideLog())
            guideLog.enable();
        GuideLog::GuideInfo info;
        fillGuideInfo(&info);
        guideLog.startGuiding(info);
    }

    state = GUIDE_GUIDING;
    emit newStatus(state);

    emit frameCaptureRequested();

    return true;
}

/**
 * @brief InternalGuider::abort Abort all internal guider operations.
 * This includes calibration, dithering, guiding, capturing, and reaquiring.
 * The state is set to IDLE or ABORTED depending on the current state since
 * ABORTED can lead to different behavior by external actors than IDLE
 * @return True if abort succeeds, false otherwise.
 */
bool InternalGuider::abort()
{
    // calibrationStage = CAL_IDLE; remove totally when understand trackingStarSelected

    logFile.close();
    guideLog.endGuiding();

    if (state == GUIDE_CALIBRATING ||
            state == GUIDE_GUIDING ||
            state == GUIDE_DITHERING ||
            state == GUIDE_MANUAL_DITHERING ||
            state == GUIDE_REACQUIRE)
    {
        if (state == GUIDE_DITHERING || state == GUIDE_MANUAL_DITHERING)
            emit newStatus(GUIDE_DITHERING_ERROR);
        emit newStatus(GUIDE_ABORTED);

        qCDebug(KSTARS_EKOS_GUIDE) << "Aborting" << getGuideStatusString(state);
    }
    else
    {
        emit newStatus(GUIDE_IDLE);
        qCDebug(KSTARS_EKOS_GUIDE) << "Stopping internal guider.";
    }

    pmath->abort();

    m_ProgressiveDither.clear();
    m_starLostCounter = 0;
    m_highRMSCounter = 0;
    accumulator.first = accumulator.second = 0;

    pmath->suspend(false);
    state = GUIDE_IDLE;

    return true;
}

bool InternalGuider::suspend()
{
    guideLog.pauseInfo();
    state = GUIDE_SUSPENDED;
    emit newStatus(state);

    pmath->suspend(true);

    return true;
}

bool InternalGuider::resume()
{
    guideLog.resumeInfo();
    state = GUIDE_GUIDING;
    emit newStatus(state);

    pmath->suspend(false);

    emit frameCaptureRequested();

    return true;
}

bool InternalGuider::ditherXY(double x, double y)
{
    m_ProgressiveDither.clear();
    m_DitherRetries = 0;
    double cur_x, cur_y;
    pmath->getTargetPosition(&cur_x, &cur_y);

    // Find out how many "jumps" we need to perform in order to get to target.
    // The current limit is now 1/4 of the box size to make sure the star stays within detection
    // threashold inside the window.
    double oneJump = (guideBoxSize / 4.0);
    double targetX = cur_x, targetY = cur_y;
    int xSign = (x >= cur_x) ? 1 : -1;
    int ySign = (y >= cur_y) ? 1 : -1;

    do
    {
        if (fabs(targetX - x) > oneJump)
            targetX += oneJump * xSign;
        else if (fabs(targetX - x) < oneJump)
            targetX = x;

        if (fabs(targetY - y) > oneJump)
            targetY += oneJump * ySign;
        else if (fabs(targetY - y) < oneJump)
            targetY = y;

        m_ProgressiveDither.enqueue(GuiderUtils::Vector(targetX, targetY, -1));

    }
    while (targetX != x || targetY != y);

    m_DitherTargetPosition = m_ProgressiveDither.dequeue();
    pmath->setTargetPosition(m_DitherTargetPosition.x, m_DitherTargetPosition.y);
    guideLog.ditherInfo(x, y, m_DitherTargetPosition.x, m_DitherTargetPosition.y);

    state = GUIDE_MANUAL_DITHERING;
    emit newStatus(state);

    processGuiding();

    return true;
}

bool InternalGuider::dither(double pixels)
{
    double ret_x, ret_y;
    pmath->getTargetPosition(&ret_x, &ret_y);

    // Just calling getStarScreenPosition() will get the position at the last time the guide star
    // was found, which is likely before the most recent guide pulse.
    // Instead we call findLocalStarPosition() which does the analysis from the image.
    // Unfortunately, processGuiding() will repeat that computation.
    // We currently don't cache it.
    GuiderUtils::Vector star_position = pmath->findLocalStarPosition(m_ImageData, guideFrame);
    if (pmath->isStarLost() || (star_position.x == -1) || (star_position.y == -1))
    {
        // If the star position is lost, just lose this iteration.
        // If it happens too many time, abort.
        constexpr int abortStarLostThreshold = MAX_LOST_STAR_THRESHOLD * 3;
        if (++m_starLostCounter > abortStarLostThreshold)
        {
            qCDebug(KSTARS_EKOS_GUIDE) << "Too many consecutive lost stars." << m_starLostCounter << "Aborting dither.";
            return abortDither();
        }
        qCDebug(KSTARS_EKOS_GUIDE) << "Dither lost star. Trying again.";
        emit frameCaptureRequested();
        return true;
    }
    else
        m_starLostCounter = 0;

    if (state != GUIDE_DITHERING)
    {
        m_DitherRetries = 0;

        auto seed = std::chrono::system_clock::now().time_since_epoch().count();
        std::default_random_engine generator(seed);
        std::uniform_real_distribution<double> angleMagnitude(0, 360);

        double angle  = angleMagnitude(generator) * dms::DegToRad;
        double diff_x = pixels * cos(angle);
        double diff_y = pixels * sin(angle);

        if (pmath->getCalibration().declinationSwapEnabled())
            diff_y *= -1;

        if (fabs(diff_x + accumulator.first) > MAX_DITHER_TRAVEL)
            diff_x *= -1.5;
        accumulator.first += diff_x;
        if (fabs(diff_y + accumulator.second) > MAX_DITHER_TRAVEL)
            diff_y *= -1.5;
        accumulator.second += diff_y;

        m_DitherTargetPosition = GuiderUtils::Vector(ret_x, ret_y, 0) + GuiderUtils::Vector(diff_x, diff_y, 0);

        qCDebug(KSTARS_EKOS_GUIDE) << "Dithering process started.. Reticle Target Pos X " << m_DitherTargetPosition.x << " Y " <<
                                   m_DitherTargetPosition.y;
        guideLog.ditherInfo(diff_x, diff_y, m_DitherTargetPosition.x, m_DitherTargetPosition.y);

        pmath->setTargetPosition(m_DitherTargetPosition.x, m_DitherTargetPosition.y);

        if (Options::gPGEnabled())
            // This is the offset in image coordinates, but needs to be converted to RA.
            pmath->getGPG().startDithering(diff_x, diff_y, pmath->getCalibration());

        state = GUIDE_DITHERING;
        emit newStatus(state);

        processGuiding();

        return true;
    }

    // These will be the RA & DEC drifts of the current star position from the reticle position in pixels.
    double driftRA, driftDEC;
    pmath->getCalibration().computeDrift(
        star_position,
        GuiderUtils::Vector(m_DitherTargetPosition.x, m_DitherTargetPosition.y, 0),
        &driftRA, &driftDEC);

    qCDebug(KSTARS_EKOS_GUIDE) << "Dithering in progress. Current" << star_position.x << star_position.y << "Target" <<
                               m_DitherTargetPosition.x <<
                               m_DitherTargetPosition.y << "Diff star X:" << driftRA << "Y:" << driftDEC;

    if (fabs(driftRA) < 1 && fabs(driftDEC) < 1)
    {
        pmath->setTargetPosition(star_position.x, star_position.y);
        qCDebug(KSTARS_EKOS_GUIDE) << "Dither complete.";

        if (Options::ditherSettle() > 0)
        {
            state = GUIDE_DITHERING_SETTLE;
            guideLog.settleStartedInfo();
            emit newStatus(state);
        }

        if (Options::gPGEnabled())
            pmath->getGPG().ditheringSettled(true);

        QTimer::singleShot(Options::ditherSettle() * 1000, this, SLOT(setDitherSettled()));
    }
    else
    {
        if (++m_DitherRetries > Options::ditherMaxIterations())
            return abortDither();

        processGuiding();
    }

    return true;
}

bool InternalGuider::abortDither()
{
    if (Options::ditherFailAbortsAutoGuide())
    {
        emit newStatus(Ekos::GUIDE_DITHERING_ERROR);
        abort();
        return false;
    }
    else
    {
        emit newLog(i18n("Warning: Dithering failed. Autoguiding shall continue as set in the options in case "
                         "of dither failure."));

        if (Options::ditherSettle() > 0)
        {
            state = GUIDE_DITHERING_SETTLE;
            guideLog.settleStartedInfo();
            emit newStatus(state);
        }

        if (Options::gPGEnabled())
            pmath->getGPG().ditheringSettled(false);

        QTimer::singleShot(Options::ditherSettle() * 1000, this, SLOT(setDitherSettled()));
        return true;
    }
}

bool InternalGuider::processManualDithering()
{
    double cur_x, cur_y;
    pmath->getTargetPosition(&cur_x, &cur_y);
    pmath->getStarScreenPosition(&cur_x, &cur_y);

    // These will be the RA & DEC drifts of the current star position from the reticle position in pixels.
    double driftRA, driftDEC;
    pmath->getCalibration().computeDrift(
        GuiderUtils::Vector(cur_x, cur_y, 0),
        GuiderUtils::Vector(m_DitherTargetPosition.x, m_DitherTargetPosition.y, 0),
        &driftRA, &driftDEC);

    qCDebug(KSTARS_EKOS_GUIDE) << "Manual Dithering in progress. Diff star X:" << driftRA << "Y:" << driftDEC;

    if (fabs(driftRA) < guideBoxSize / 5.0 && fabs(driftDEC) < guideBoxSize / 5.0)
    {
        if (m_ProgressiveDither.empty() == false)
        {
            m_DitherTargetPosition = m_ProgressiveDither.dequeue();
            pmath->setTargetPosition(m_DitherTargetPosition.x, m_DitherTargetPosition.y);
            qCDebug(KSTARS_EKOS_GUIDE) << "Next Dither Jump X:" << m_DitherTargetPosition.x << "Jump Y:" << m_DitherTargetPosition.y;
            m_DitherRetries = 0;

            processGuiding();

            return true;
        }

        if (fabs(driftRA) < 1 && fabs(driftDEC) < 1)
        {
            pmath->setTargetPosition(cur_x, cur_y);
            qCDebug(KSTARS_EKOS_GUIDE) << "Manual Dither complete.";

            if (Options::ditherSettle() > 0)
            {
                state = GUIDE_DITHERING_SETTLE;
                guideLog.settleStartedInfo();
                emit newStatus(state);
            }

            QTimer::singleShot(Options::ditherSettle() * 1000, this, SLOT(setDitherSettled()));
        }
        else
        {
            processGuiding();
        }
    }
    else
    {
        if (++m_DitherRetries > Options::ditherMaxIterations())
        {
            emit newLog(i18n("Warning: Manual Dithering failed."));

            if (Options::ditherSettle() > 0)
            {
                state = GUIDE_DITHERING_SETTLE;
                guideLog.settleStartedInfo();
                emit newStatus(state);
            }

            QTimer::singleShot(Options::ditherSettle() * 1000, this, SLOT(setDitherSettled()));
            return true;
        }

        processGuiding();
    }

    return true;
}

void InternalGuider::setDitherSettled()
{
    guideLog.settleCompletedInfo();
    emit newStatus(Ekos::GUIDE_DITHERING_SUCCESS);

    // Back to guiding
    state = GUIDE_GUIDING;
}

bool InternalGuider::calibrate()
{
    bool ccdInfo = true, scopeInfo = true;
    QString errMsg;

    if (subW == 0 || subH == 0)
    {
        errMsg  = "CCD";
        ccdInfo = false;
    }

    if (mountAperture == 0.0 || mountFocalLength == 0.0)
    {
        scopeInfo = false;
        if (ccdInfo == false)
            errMsg += " & Telescope";
        else
            errMsg += "Telescope";
    }

    if (ccdInfo == false || scopeInfo == false)
    {
        KSNotification::error(i18n("%1 info are missing. Please set the values in INDI Control Panel.", errMsg),
                              i18n("Missing Information"));
        return false;
    }

    if (state != GUIDE_CALIBRATING)
    {
        pmath->getTargetPosition(&calibrationStartX, &calibrationStartY);
        calibrationProcess.reset(
            new CalibrationProcess(calibrationStartX, calibrationStartY,
                                   !Options::twoAxisEnabled()));
        state = GUIDE_CALIBRATING;
        emit newStatus(GUIDE_CALIBRATING);
    }

    if (calibrationProcess->inProgress())
    {
        iterateCalibration();
        return true;
    }

    if (restoreCalibration())
    {
        calibrationProcess.reset();
        emit newStatus(Ekos::GUIDE_CALIBRATION_SUCESS);
        KSNotification::event(QLatin1String("CalibrationRestored"),
                              i18n("Guiding calibration restored"));
        reset();
        return true;
    }

    // Initialize the calibration parameters.
    // CCD pixel values comes in in microns and we want mm.
    pmath->getMutableCalibration()->setParameters(
        ccdPixelSizeX / 1000.0, ccdPixelSizeY / 1000.0, mountFocalLength,
        subBinX, subBinY, pierSide, mountRA, mountDEC);

    calibrationProcess->useCalibration(pmath->getMutableCalibration());

    guideFrame->disconnect(this);

    // Must reset dec swap before we run any calibration procedure!
    emit DESwapChanged(false);
    pmath->setLostStar(false);

    if (Options::saveGuideLog())
        guideLog.enable();
    GuideLog::GuideInfo info;
    fillGuideInfo(&info);
    guideLog.startCalibration(info);

    calibrationProcess->startup();
    calibrationProcess->setGuideLog(&guideLog);
    iterateCalibration();

    return true;
}

void InternalGuider::iterateCalibration()
{
    if (calibrationProcess->inProgress())
    {
        pmath->performProcessing(GUIDE_CALIBRATING, m_ImageData, guideFrame);
        if (pmath->isStarLost())
        {
            emit newLog(i18n("Lost track of the guide star. Try increasing the square size or reducing pulse duration."));
            emit newStatus(Ekos::GUIDE_CALIBRATION_ERROR);
            emit calibrationUpdate(GuideInterface::CALIBRATION_MESSAGE_ONLY,
                                   i18n("Guide Star lost."));
            reset();
            return;
        }
    }
    double starX, starY;
    pmath->getStarScreenPosition(&starX, &starY);
    calibrationProcess->iterate(starX, starY);

    auto status = calibrationProcess->getStatus();
    if (status != GUIDE_CALIBRATING)
        emit newStatus(status);

    QString logStatus = calibrationProcess->getLogStatus();
    if (logStatus.length())
        emit newLog(logStatus);

    QString updateMessage;
    double x, y;
    GuideInterface::CalibrationUpdateType type;
    calibrationProcess->getCalibrationUpdate(&type, &updateMessage, &x, &y);
    if (updateMessage.length())
        emit calibrationUpdate(type, updateMessage, x, y);

    GuideDirection pulseDirection;
    int pulseMsecs;
    calibrationProcess->getPulse(&pulseDirection, &pulseMsecs);
    if (pulseDirection != NO_DIR)
        emit newPulse(pulseDirection, pulseMsecs);

    if (status == GUIDE_CALIBRATION_ERROR)
    {
        KSNotification::event(QLatin1String("CalibrationFailed"), i18n("Guiding calibration failed"),
                              KSNotification::EVENT_ALERT);
        reset();
    }
    else if (status == GUIDE_CALIBRATION_SUCESS)
    {
        KSNotification::event(QLatin1String("CalibrationSuccessful"),
                              i18n("Guiding calibration completed successfully"));
        emit DESwapChanged(pmath->getCalibration().declinationSwapEnabled());
        pmath->setTargetPosition(calibrationStartX, calibrationStartY);
        reset();
    }
}

void InternalGuider::setGuideView(GuideView *guideView)
{
    guideFrame = guideView;
}

void InternalGuider::setImageData(const QSharedPointer<FITSData> &data)
{
    m_ImageData = data;
}

void InternalGuider::reset()
{
    state = GUIDE_IDLE;

    connect(guideFrame, SIGNAL(trackingStarSelected(int, int)), this, SLOT(trackingStarSelected(int, int)),
            Qt::UniqueConnection);
    calibrationProcess.reset();
}

bool InternalGuider::clearCalibration()
{
    Options::setSerializedCalibration("");
    pmath->getMutableCalibration()->reset();
    return true;
}

bool InternalGuider::restoreCalibration()
{
    bool success = Options::reuseGuideCalibration() &&
                   pmath->getMutableCalibration()->restore(
                       pierSide, Options::reverseDecOnPierSideChange(),
                       subBinX, subBinY, &mountDEC);
    if (success)
        emit DESwapChanged(pmath->getCalibration().declinationSwapEnabled());
    return success;
}

void InternalGuider::setStarPosition(QVector3D &starCenter)
{
    pmath->setTargetPosition(starCenter.x(), starCenter.y());
}

void InternalGuider::trackingStarSelected(int x, int y)
{
    /*

      Not sure what's going on here--manual star selection for calibration?
      Don't really see how the logic works.

    if (calibrationStage == CAL_IDLE)
        return;

    pmath->setTargetPosition(x, y);

    calibrationStage = CAL_START;
    */
}

void InternalGuider::setDECSwap(bool enable)
{
    pmath->getMutableCalibration()->setDeclinationSwapEnabled(enable);
}

void InternalGuider::setSquareAlgorithm(int index)
{
    if (index == SEP_MULTISTAR && !pmath->usingSEPMultiStar())
        m_isFirstFrame = true;
    pmath->setAlgorithmIndex(index);
}

void InternalGuider::setReticleParameters(double x, double y)
{
    pmath->setTargetPosition(x, y);
}

bool InternalGuider::getReticleParameters(double *x, double *y)
{
    return pmath->getTargetPosition(x, y);
}

bool InternalGuider::setGuiderParams(double ccdPixelSizeX, double ccdPixelSizeY, double mountAperture,
                                     double mountFocalLength)
{
    this->ccdPixelSizeX    = ccdPixelSizeX;
    this->ccdPixelSizeY    = ccdPixelSizeY;
    this->mountAperture    = mountAperture;
    this->mountFocalLength = mountFocalLength;
    return pmath->setGuiderParameters(ccdPixelSizeX, ccdPixelSizeY, mountAperture, mountFocalLength);
}

bool InternalGuider::setFrameParams(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t binX, uint16_t binY)
{
    if (w <= 0 || h <= 0)
        return false;

    subX = x;
    subY = y;
    subW = w;
    subH = h;

    subBinX = binX;
    subBinY = binY;

    pmath->setVideoParameters(w, h, subBinX, subBinY);

    return true;
}

bool InternalGuider::processGuiding()
{
    const cproc_out_params *out;

    // On first frame, center the box (reticle) around the star so we do not start with an offset the results in
    // unnecessary guiding pulses.
    if (m_isFirstFrame)
    {
        if (state == GUIDE_GUIDING)
        {
            GuiderUtils::Vector star_pos = pmath->findLocalStarPosition(m_ImageData, guideFrame);
            pmath->setTargetPosition(star_pos.x, star_pos.y);
        }
        m_isFirstFrame = false;
    }
    // calc math. it tracks square
    pmath->performProcessing(state, m_ImageData, guideFrame, &guideLog);

    if (state == GUIDE_SUSPENDED)
    {
        if (Options::gPGEnabled())
            emit frameCaptureRequested();
        return true;
    }

    if (pmath->isStarLost())
        m_starLostCounter++;
    else
        m_starLostCounter = 0;

    // do pulse
    out = pmath->getOutputParameters();

    bool sendPulses = true;

    double delta_rms = std::hypot(out->delta[GUIDE_RA], out->delta[GUIDE_DEC]);
    if (delta_rms > Options::guideMaxDeltaRMS())
        m_highRMSCounter++;
    else
        m_highRMSCounter = 0;

    uint8_t abortStarLostThreshold = (state == GUIDE_DITHERING
                                      || state == GUIDE_MANUAL_DITHERING) ? MAX_LOST_STAR_THRESHOLD * 3 : MAX_LOST_STAR_THRESHOLD;
    uint8_t abortRMSThreshold = (state == GUIDE_DITHERING
                                 || state == GUIDE_MANUAL_DITHERING) ? MAX_RMS_THRESHOLD * 3 : MAX_RMS_THRESHOLD;
    if (m_starLostCounter > abortStarLostThreshold || m_highRMSCounter > abortRMSThreshold)
    {
        qCDebug(KSTARS_EKOS_GUIDE) << "m_starLostCounter" << m_starLostCounter
                                   << "m_highRMSCounter" << m_highRMSCounter
                                   << "delta_rms" << delta_rms;

        if (m_starLostCounter > abortStarLostThreshold)
            emit newLog(i18n("Lost track of the guide star. Searching for guide stars..."));
        else
            emit newLog(i18n("Delta RMS threshold value exceeded. Searching for guide stars..."));

        reacquireTimer.start();
        rememberState = state;
        state = GUIDE_REACQUIRE;
        emit newStatus(state);
        return true;
    }

    if (sendPulses)
    {
        emit newPulse(out->pulse_dir[GUIDE_RA], out->pulse_length[GUIDE_RA],
                      out->pulse_dir[GUIDE_DEC], out->pulse_length[GUIDE_DEC]);

        // Wait until pulse is over before capturing an image
        const int waitMS = qMax(out->pulse_length[GUIDE_RA], out->pulse_length[GUIDE_DEC]);
        // If less than MAX_IMMEDIATE_CAPTURE ms, then capture immediately
        if (waitMS > MAX_IMMEDIATE_CAPTURE)
            // Issue frame requests MAX_IMMEDIATE_CAPTURE ms before timeout to account for
            // propagation delays
            QTimer::singleShot(waitMS - PROPAGATION_DELAY, [&]()
        {
            emit frameCaptureRequested();
        });
        else
            emit frameCaptureRequested();
    }
    else
        emit frameCaptureRequested();

    if (state == GUIDE_DITHERING || state == GUIDE_MANUAL_DITHERING)
        return true;

    // Hy 9/13/21: Check above just looks for GUIDE_DITHERING or GUIDE_MANUAL_DITHERING
    // but not the other dithering possibilities (error, success, settle).
    // Not sure if they should be included above, so conservatively not changing the
    // code, but don't think they should broadcast the newAxisDelta which might
    // interrup a capture.
    if (state < GUIDE_DITHERING)
        emit newAxisDelta(out->delta[GUIDE_RA], out->delta[GUIDE_DEC]);

    double raPulse = out->pulse_length[GUIDE_RA];
    double dePulse = out->pulse_length[GUIDE_DEC];

    //If the pulse was not sent to the mount, it should have 0 value
    if(out->pulse_dir[GUIDE_RA] == NO_DIR)
        raPulse = 0;
    //If the pulse was not sent to the mount, it should have 0 value
    if(out->pulse_dir[GUIDE_DEC] == NO_DIR)
        dePulse = 0;
    //If the pulse was in the Negative direction, it should have a negative sign.
    if(out->pulse_dir[GUIDE_RA] == RA_INC_DIR)
        raPulse = -raPulse;
    //If the pulse was in the Negative direction, it should have a negative sign.
    if(out->pulse_dir[GUIDE_DEC] == DEC_INC_DIR)
        dePulse = -dePulse;

    emit newAxisPulse(raPulse, dePulse);

    emit newAxisSigma(out->sigma[GUIDE_RA], out->sigma[GUIDE_DEC]);
    if (SEPMultiStarEnabled())
        emit newSNR(pmath->getGuideStarSNR());

    return true;
}

bool InternalGuider::selectAutoStarSEPMultistar()
{
    guideFrame->updateFrame();
    QVector3D newStarCenter = pmath->selectGuideStar(m_ImageData);
    if (newStarCenter.x() >= 0)
    {
        emit newStarPosition(newStarCenter, true);
        return true;
    }
    return false;
}

bool InternalGuider::SEPMultiStarEnabled()
{
    return Options::guideAlgorithm() == SEP_MULTISTAR;
}

bool InternalGuider::selectAutoStar()
{
    if (Options::guideAlgorithm() == SEP_MULTISTAR)
        return selectAutoStarSEPMultistar();

    bool useNativeDetection = false;

    QList<Edge *> starCenters;

    if (Options::guideAlgorithm() != SEP_THRESHOLD)
        starCenters = GuideAlgorithms::detectStars(m_ImageData, guideFrame->getTrackingBox());

    if (starCenters.empty())
    {
        QVariantMap settings;
        settings["maxStarsCount"] = 50;
        settings["optionsProfileIndex"] = Options::guideOptionsProfile();
        settings["optionsProfileGroup"] = static_cast<int>(Ekos::GuideProfiles);
        m_ImageData->setSourceExtractorSettings(settings);

        if (Options::guideAlgorithm() == SEP_THRESHOLD)
            m_ImageData->findStars(ALGORITHM_SEP).waitForFinished();
        else
            m_ImageData->findStars().waitForFinished();

        starCenters = m_ImageData->getStarCenters();
        if (starCenters.empty())
            return false;

        useNativeDetection = true;
        // For SEP, prefer flux total
        if (Options::guideAlgorithm() == SEP_THRESHOLD)
            std::sort(starCenters.begin(), starCenters.end(), [](const Edge * a, const Edge * b)
        {
            return a->val > b->val;
        });
        else
            std::sort(starCenters.begin(), starCenters.end(), [](const Edge * a, const Edge * b)
        {
            return a->width > b->width;
        });

        guideFrame->setStarsEnabled(true);
        guideFrame->updateFrame();
    }

    int maxX = m_ImageData->width();
    int maxY = m_ImageData->height();

    int scores[MAX_GUIDE_STARS];

    int maxIndex = MAX_GUIDE_STARS < starCenters.count() ? MAX_GUIDE_STARS : starCenters.count();

    for (int i = 0; i < maxIndex; i++)
    {
        int score = 100;

        Edge *center = starCenters.at(i);

        if (useNativeDetection)
        {
            // Severely reject stars close to edges
            if (center->x < (center->width * 5) || center->y < (center->width * 5) ||
                    center->x > (maxX - center->width * 5) || center->y > (maxY - center->width * 5))
                score -= 1000;

            // Reject stars bigger than square
            if (center->width > float(guideBoxSize) / subBinX)
                score -= 1000;
            else
            {
                if (Options::guideAlgorithm() == SEP_THRESHOLD)
                    score += sqrt(center->val);
                else
                    // Moderately favor brighter stars
                    score += center->width * center->width;
            }

            // Moderately reject stars close to other stars
            foreach (Edge *edge, starCenters)
            {
                if (edge == center)
                    continue;

                if (fabs(center->x - edge->x) < center->width * 2 && fabs(center->y - edge->y) < center->width * 2)
                {
                    score -= 15;
                    break;
                }
            }
        }
        else
        {
            score = center->val;
        }

        scores[i] = score;
    }

    int maxScore      = -1;
    int maxScoreIndex = -1;
    for (int i = 0; i < maxIndex; i++)
    {
        if (scores[i] > maxScore)
        {
            maxScore      = scores[i];
            maxScoreIndex = i;
        }
    }

    if (maxScoreIndex < 0)
    {
        qCDebug(KSTARS_EKOS_GUIDE) << "No suitable star detected.";
        return false;
    }

    QVector3D newStarCenter(starCenters[maxScoreIndex]->x, starCenters[maxScoreIndex]->y, 0);

    if (useNativeDetection == false)
        qDeleteAll(starCenters);

    emit newStarPosition(newStarCenter, true);

    return true;
}

bool InternalGuider::reacquire()
{
    bool rc = selectAutoStar();
    if (rc)
    {
        m_highRMSCounter = m_starLostCounter = 0;
        m_isFirstFrame = true;
        pmath->reset();
        // If we were in the process of dithering, wait until settle and resume
        if (rememberState == GUIDE_DITHERING || state == GUIDE_MANUAL_DITHERING)
        {
            if (Options::ditherSettle() > 0)
            {
                state = GUIDE_DITHERING_SETTLE;
                guideLog.settleStartedInfo();
                emit newStatus(state);
            }

            QTimer::singleShot(Options::ditherSettle() * 1000, this, SLOT(setDitherSettled()));
        }
        else
        {
            state = GUIDE_GUIDING;
            emit newStatus(state);
        }

    }
    else if (reacquireTimer.elapsed() > static_cast<int>(Options::guideLostStarTimeout() * 1000))
    {
        emit newLog(i18n("Failed to find any suitable guide stars. Aborting..."));
        abort();
        return false;
    }

    emit frameCaptureRequested();
    return rc;
}

void InternalGuider::fillGuideInfo(GuideLog::GuideInfo *info)
{
    // NOTE: just using the X values, phd2logview assumes x & y the same.
    // pixel scale in arc-sec / pixel. The 2nd and 3rd values seem redundent, but are
    // in the phd2 logs.
    info->pixelScale = (206.26481 * this->ccdPixelSizeX * this->subBinX) / this->mountFocalLength;
    info->binning = this->subBinX;
    info->focalLength = this->mountFocalLength;
    info->ra = this->mountRA.Degrees();
    info->dec = this->mountDEC.Degrees();
    info->azimuth = this->mountAzimuth.Degrees();
    info->altitude = this->mountAltitude.Degrees();
    info->pierSide = this->pierSide;
    info->xangle = pmath->getCalibration().getRAAngle();
    info->yangle = pmath->getCalibration().getDECAngle();
    // Calibration values in ms/pixel, xrate is in pixels/second.
    info->xrate = 1000.0 / pmath->getCalibration().raPulseMillisecondsPerPixel();
    info->yrate = 1000.0 / pmath->getCalibration().decPulseMillisecondsPerPixel();
}

void InternalGuider::updateGPGParameters()
{
    pmath->getGPG().updateParameters();
}

void InternalGuider::resetGPG()
{
    pmath->getGPG().reset();
}

const Calibration &InternalGuider::getCalibration() const
{
    return pmath->getCalibration();
}
}
