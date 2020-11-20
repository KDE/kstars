/*  Ekos Internal Guider Class
    Copyright (C) 2016 Jasem Mutlaq <mutlaqja@ikarustech.com>.

    Based on lin_guider

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#include "internalguider.h"

#include "ekos_guide_debug.h"
#include "gmath.h"
#include "Options.h"
#include "auxiliary/kspaths.h"
#include "fitsviewer/fitsdata.h"
#include "fitsviewer/fitsview.h"
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
        pierSide, Options::reverseDecOnPierSideChange(), nullptr);

    state = GUIDE_IDLE;
}

bool InternalGuider::guide()
{
    if (state >= GUIDE_GUIDING)
    {
        if (m_ImageGuideEnabled)
            return processImageGuiding();
        else
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
    calibrationStage = CAL_IDLE;

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
    pmath->getReticleParameters(&cur_x, &cur_y);

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

        m_ProgressiveDither.enqueue(Vector(targetX, targetY, -1));

    }
    while (targetX != x || targetY != y);

    m_DitherTargetPosition = m_ProgressiveDither.dequeue();
    pmath->setReticleParameters(m_DitherTargetPosition.x, m_DitherTargetPosition.y);
    guideLog.ditherInfo(x, y, m_DitherTargetPosition.x, m_DitherTargetPosition.y);

    state = GUIDE_MANUAL_DITHERING;
    emit newStatus(state);

    processGuiding();

    return true;
}

bool InternalGuider::dither(double pixels)
{
    double ret_x, ret_y;
    pmath->getReticleParameters(&ret_x, &ret_y);

    // Just calling getStarScreenPosition() will get the position at the last time the guide star
    // was found, which is likely before the most recent guide pulse.
    // Instead we call findLocalStarPosition() which does the analysis from the image.
    // Unfortunately, processGuiding() will repeat that computation.
    // We currently don't cache it.
    Vector star_position = pmath->findLocalStarPosition();
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

        m_DitherTargetPosition = Vector(ret_x, ret_y, 0) + Vector(diff_x, diff_y, 0);

        qCDebug(KSTARS_EKOS_GUIDE) << "Dithering process started.. Reticle Target Pos X " << m_DitherTargetPosition.x << " Y " <<
                                   m_DitherTargetPosition.y;
        guideLog.ditherInfo(diff_x, diff_y, m_DitherTargetPosition.x, m_DitherTargetPosition.y);

        pmath->setReticleParameters(m_DitherTargetPosition.x, m_DitherTargetPosition.y);

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
        Vector(m_DitherTargetPosition.x, m_DitherTargetPosition.y, 0),
        &driftRA, &driftDEC);

    qCDebug(KSTARS_EKOS_GUIDE) << "Dithering in progress. Current" << star_position.x << star_position.y << "Target" <<
                               m_DitherTargetPosition.x <<
                               m_DitherTargetPosition.y << "Diff star X:" << driftRA << "Y:" << driftDEC;

    if (fabs(driftRA) < 1 && fabs(driftDEC) < 1)
    {
        pmath->setReticleParameters(star_position.x, star_position.y);
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
    pmath->getReticleParameters(&cur_x, &cur_y);
    pmath->getStarScreenPosition(&cur_x, &cur_y);

    // These will be the RA & DEC drifts of the current star position from the reticle position in pixels.
    double driftRA, driftDEC;
    pmath->getCalibration().computeDrift(
        Vector(cur_x, cur_y, 0),
        Vector(m_DitherTargetPosition.x, m_DitherTargetPosition.y, 0),
        &driftRA, &driftDEC);

    qCDebug(KSTARS_EKOS_GUIDE) << "Manual Dithering in progress. Diff star X:" << driftRA << "Y:" << driftDEC;

    if (fabs(driftRA) < guideBoxSize / 5.0 && fabs(driftDEC) < guideBoxSize / 5.0)
    {
        if (m_ProgressiveDither.empty() == false)
        {
            m_DitherTargetPosition = m_ProgressiveDither.dequeue();
            pmath->setReticleParameters(m_DitherTargetPosition.x, m_DitherTargetPosition.y);
            qCDebug(KSTARS_EKOS_GUIDE) << "Next Dither Jump X:" << m_DitherTargetPosition.x << "Jump Y:" << m_DitherTargetPosition.y;
            m_DitherRetries = 0;

            processGuiding();

            return true;
        }

        if (fabs(driftRA) < 1 && fabs(driftDEC) < 1)
        {
            pmath->setReticleParameters(cur_x, cur_y);
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
        calibrationStage = CAL_IDLE;
        state            = GUIDE_CALIBRATING;
        emit newStatus(GUIDE_CALIBRATING);
    }

    if (calibrationStage > CAL_START)
    {
        processCalibration();
        return true;
    }

    if (restoreCalibration())
    {
        calibrationStage = CAL_IDLE;
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

    // Also setup the temporary calibration used in calibrateRADECReticle().
    // In truth, these values won't be used, as conversions from pixels to arcseconds
    // aren't needed.
    m_CalibrationParams.tempCalibration.setParameters(
        ccdPixelSizeX / 1000.0, ccdPixelSizeY / 1000.0, mountFocalLength,
        subBinX, subBinY, pierSide, mountRA, mountDEC);

    guideFrame->disconnect(this);

    // Must reset dec swap before we run any calibration procedure!
    emit DESwapChanged(false);
    pmath->setLostStar(false);

    calibrationStage = CAL_START;
    if (Options::saveGuideLog())
        guideLog.enable();
    GuideLog::GuideInfo info;
    fillGuideInfo(&info);
    guideLog.startCalibration(info);

    // automatic
    // If two axies (RA/DEC) are required
    if (Options::twoAxisEnabled())
        calibrateRADECRecticle(false);
    else
        // Just RA
        calibrateRADECRecticle(true);

    return true;
}

void InternalGuider::processCalibration()
{
    pmath->performProcessing();

    if (pmath->isStarLost())
    {
        emit newLog(i18n("Lost track of the guide star. Try increasing the square size or reducing pulse duration."));
        reset();

        calibrationStage = CAL_ERROR;
        emit newStatus(Ekos::GUIDE_CALIBRATION_ERROR);
        emit calibrationUpdate(GuideInterface::CALIBRATION_MESSAGE_ONLY, i18n("Calibration Failed: Lost guide star."));
        return;
    }

    switch (calibrationType)
    {
        case CAL_NONE:
            break;

        case CAL_RA_AUTO:
            calibrateRADECRecticle(true);
            break;

        case CAL_RA_DEC_AUTO:
            calibrateRADECRecticle(false);
            break;
    }
}

void InternalGuider::setGuideView(GuideView *guideView)
{
    guideFrame = guideView;

    pmath->setGuideView(guideFrame);
}

void InternalGuider::reset()
{
    state = GUIDE_IDLE;
    //calibrationStage = CAL_IDLE;
    connect(guideFrame, SIGNAL(trackingStarSelected(int, int)), this, SLOT(trackingStarSelected(int, int)),
            Qt::UniqueConnection);
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
                       pierSide, Options::reverseDecOnPierSideChange(), &mountDEC);
    if (success)
        emit DESwapChanged(pmath->getCalibration().declinationSwapEnabled());
    return success;
}

void InternalGuider::calibrateRADECRecticle(bool ra_only)
{
    bool axis_calibration_complete = false;

    Q_ASSERT(pmath);

    //int totalPulse    = pulseDuration * Options::autoModeIterations();

    if (ra_only)
        calibrationType = CAL_RA_AUTO;
    else
        calibrationType = CAL_RA_DEC_AUTO;

    switch (calibrationStage)
    {
        case CAL_START:
            //----- automatic mode -----
            m_CalibrationParams.auto_drift_time = Options::autoModeIterations();
            m_CalibrationParams.turn_back_time = m_CalibrationParams.auto_drift_time * 7;

            m_CalibrationParams.ra_iterations = 0;
            m_CalibrationParams.dec_iterations = 0;
            m_CalibrationParams.backlash_iterations = 0;
            m_CalibrationParams.ra_total_pulse = m_CalibrationParams.de_total_pulse = 0;

            emit newLog(i18n("RA drifting forward..."));

            pmath->getReticleParameters(&m_CalibrationCoords.start_x1, &m_CalibrationCoords.start_y1);

            m_CalibrationParams.last_pulse = Options::calibrationPulseDuration();

            emit calibrationUpdate(GuideInterface::RA_OUT, i18n("Guide Star found."), 0, 0);
            qCDebug(KSTARS_EKOS_GUIDE) << "Auto Iteration #" << m_CalibrationParams.auto_drift_time << "Default pulse:" <<
                                       m_CalibrationParams.last_pulse;
            qCDebug(KSTARS_EKOS_GUIDE) << "Start X1 " << m_CalibrationCoords.start_x1 << " Start Y1 " << m_CalibrationCoords.start_y1;

            axis_calibration_complete = false;

            m_CalibrationCoords.last_x = m_CalibrationCoords.start_x1;
            m_CalibrationCoords.last_y = m_CalibrationCoords.start_x2;

            emit newPulse(RA_INC_DIR, m_CalibrationParams.last_pulse);

            m_CalibrationParams.ra_iterations++;

            calibrationStage = CAL_RA_INC;
            guideLog.addCalibrationData(
                RA_INC_DIR,
                m_CalibrationCoords.start_x1, m_CalibrationCoords.start_y1,
                m_CalibrationCoords.start_x1, m_CalibrationCoords.start_y1);

            break;

        case CAL_RA_INC:
        {
            // Star position resulting from LAST guiding pulse to mount
            double cur_x, cur_y;
            pmath->getStarScreenPosition(&cur_x, &cur_y);

            emit calibrationUpdate(GuideInterface::RA_OUT, i18n("Calibrating RA Out"),
                                   cur_x - m_CalibrationCoords.start_x1, cur_y - m_CalibrationCoords.start_y1);

            qCDebug(KSTARS_EKOS_GUIDE) << "Iteration #" << m_CalibrationParams.ra_iterations << ": STAR " << cur_x << "," << cur_y;
            qCDebug(KSTARS_EKOS_GUIDE) << "Iteration " << m_CalibrationParams.ra_iterations << " Direction: RA_INC_DIR" << " Duration: "
                                       << m_CalibrationParams.last_pulse << " ms.";

            guideLog.addCalibrationData(RA_INC_DIR, cur_x, cur_y,
                                        m_CalibrationCoords.start_x1, m_CalibrationCoords.start_y1);

            // Must pass at least 1.5 pixels to move on to the next stage.
            // If we've moved 15 pixels, we can cut short the requested number of iterations.
            const double xDrift = cur_x - m_CalibrationCoords.start_x1;
            const double yDrift = cur_y - m_CalibrationCoords.start_y1;
            constexpr double SUFFICIENT_RA_MOVE_PIXELS = 15.0;
            if (((m_CalibrationParams.ra_iterations >= m_CalibrationParams.auto_drift_time) ||
                    (std::hypot(xDrift, yDrift) > SUFFICIENT_RA_MOVE_PIXELS))
                    && (fabs(xDrift) > 1.5 || fabs(yDrift) > 1.5))
            {
                m_CalibrationParams.ra_total_pulse += m_CalibrationParams.last_pulse;

                calibrationStage = CAL_RA_DEC;

                m_CalibrationCoords.end_x1 = cur_x;
                m_CalibrationCoords.end_y1 = cur_y;

                m_CalibrationCoords.last_x = cur_x;
                m_CalibrationCoords.last_y = cur_y;

                qCDebug(KSTARS_EKOS_GUIDE) << "End X1 " << m_CalibrationCoords.end_x1 << " End Y1 " << m_CalibrationCoords.end_y1;

                // This temporary calibration is just used to help find our way back to the origin.
                // total_pulse is not used, but valid.
                m_CalibrationParams.tempCalibration.calculate1D(m_CalibrationCoords.end_x1 - m_CalibrationCoords.start_x1,
                        m_CalibrationCoords.end_y1 - m_CalibrationCoords.start_y1,
                        m_CalibrationParams.ra_total_pulse);

                m_CalibrationCoords.ra_distance = 0;
                m_CalibrationParams.backlash = 0;

                emit newPulse(RA_DEC_DIR, m_CalibrationParams.last_pulse);
                m_CalibrationParams.ra_iterations++;

                emit newLog(i18n("RA drifting reverse..."));
                guideLog.endCalibrationSection(RA_INC_DIR, m_CalibrationParams.tempCalibration.getAngle());
            }
            else if (m_CalibrationParams.ra_iterations > m_CalibrationParams.turn_back_time)
            {
                emit newLog(i18n("Calibration rejected. Star drift is too short. Check for mount, cable, or backlash problems."));

                calibrationStage = CAL_ERROR;

                emit newStatus(Ekos::GUIDE_CALIBRATION_ERROR);
                emit calibrationUpdate(GuideInterface::CALIBRATION_MESSAGE_ONLY, i18n("Calibration Failed: Drift too short."));
                KSNotification::event(QLatin1String("CalibrationFailed"), i18n("Guiding calibration failed"),
                                      KSNotification::EVENT_ALERT);
                guideLog.endCalibration(0, 0);

                reset();
            }
            else
            {
                // Aggressive pulse in case we're going slow
                if (fabs(cur_x - m_CalibrationCoords.last_x) < 0.5 && fabs(cur_y - m_CalibrationCoords.last_y) < 0.5)
                {
                    // 200%
                    m_CalibrationParams.last_pulse = Options::calibrationPulseDuration() * 2;
                }
                else
                {
                    m_CalibrationParams.ra_total_pulse += m_CalibrationParams.last_pulse;
                    m_CalibrationParams.last_pulse = Options::calibrationPulseDuration();
                }

                m_CalibrationCoords.last_x = cur_x;
                m_CalibrationCoords.last_y = cur_y;

                emit newPulse(RA_INC_DIR, m_CalibrationParams.last_pulse);

                m_CalibrationParams.ra_iterations++;
            }
        }
        break;

        case CAL_RA_DEC:
        {
            //----- Z-check (new!) -----
            double cur_x, cur_y;
            pmath->getStarScreenPosition(&cur_x, &cur_y);

            emit calibrationUpdate(GuideInterface::RA_IN, i18n("Calibrating RA In"),
                                   cur_x - m_CalibrationCoords.start_x1, cur_y - m_CalibrationCoords.start_y1);
            qCDebug(KSTARS_EKOS_GUIDE) << "Iteration #" << m_CalibrationParams.ra_iterations << ": STAR " << cur_x << "," << cur_y;
            qCDebug(KSTARS_EKOS_GUIDE) << "Iteration " << m_CalibrationParams.ra_iterations << " Direction: RA_DEC_DIR" << " Duration: "
                                       << m_CalibrationParams.last_pulse << " ms.";

            double driftRA, driftDEC;
            m_CalibrationParams.tempCalibration.computeDrift(
                Vector(cur_x, cur_y, 0),
                Vector(m_CalibrationCoords.start_x1, m_CalibrationCoords.start_y1, 0),
                &driftRA, &driftDEC);

            qCDebug(KSTARS_EKOS_GUIDE) << "Star x pos is " << driftRA << " from original point.";

            if (m_CalibrationCoords.ra_distance == 0.0)
                m_CalibrationCoords.ra_distance = driftRA;

            guideLog.addCalibrationData(RA_DEC_DIR, cur_x, cur_y,
                                        m_CalibrationCoords.start_x1, m_CalibrationCoords.start_y1);

            // start point reached... so exit
            if (driftRA < 1.5)
            {
                pmath->performProcessing();

                //m_CalibrationParams.ra_total_pulse += m_CalibrationParams.last_pulse;
                m_CalibrationParams.last_pulse = Options::calibrationPulseDuration();
                axis_calibration_complete = true;
            }
            // If we'not moving much, try increasing pulse to 200% to clear any backlash
            // Also increase pulse width if we are going FARTHER and not back to our original position
            else if ( (fabs(cur_x - m_CalibrationCoords.last_x) < 0.5 && fabs(cur_y - m_CalibrationCoords.last_y) < 0.5)
                      || driftRA > m_CalibrationCoords.ra_distance)
            {
                m_CalibrationParams.backlash++;

                // Increase pulse to 200% after we tried to fight against backlash 2 times at least
                if (m_CalibrationParams.backlash > 2)
                    m_CalibrationParams.last_pulse = Options::calibrationPulseDuration() * 2;
                else
                    m_CalibrationParams.last_pulse = Options::calibrationPulseDuration();
            }
            else
            {
                //m_CalibrationParams.ra_total_pulse += m_CalibrationParams.last_pulse;
                m_CalibrationParams.last_pulse = Options::calibrationPulseDuration();
                m_CalibrationParams.backlash = 0;
            }
            m_CalibrationCoords.last_x = cur_x;
            m_CalibrationCoords.last_y = cur_y;

            //----- Z-check end -----

            if (axis_calibration_complete == false)
            {
                if (m_CalibrationParams.ra_iterations < m_CalibrationParams.turn_back_time)
                {
                    emit newPulse(RA_DEC_DIR, m_CalibrationParams.last_pulse);
                    m_CalibrationParams.ra_iterations++;
                    break;
                }

                calibrationStage = CAL_ERROR;

                emit newStatus(Ekos::GUIDE_CALIBRATION_ERROR);
                emit calibrationUpdate(GuideInterface::CALIBRATION_MESSAGE_ONLY, i18n("Calibration Failed: couldn't reach start."));
                emit newLog(i18np("Guide RA: Scope cannot reach the start point after %1 iteration. Possible mount or "
                                  "backlash problems...",
                                  "GUIDE_RA: Scope cannot reach the start point after %1 iterations. Possible mount or "
                                  "backlash problems...",
                                  m_CalibrationParams.ra_iterations));

                KSNotification::event(QLatin1String("CalibrationFailed"), i18n("Guiding calibration failed"),
                                      KSNotification::EVENT_ALERT);
                reset();
                break;
            }

            if (ra_only == false)
            {
                if (Options::guideCalibrationBacklash())
                {
                    calibrationStage = CAL_BACKLASH;
                    m_CalibrationCoords.last_x = cur_x;
                    m_CalibrationCoords.last_y = cur_y;
                    m_CalibrationCoords.start_backlash_x = cur_x;
                    m_CalibrationCoords.start_backlash_y = cur_y;
                    emit newPulse(DEC_INC_DIR, Options::calibrationPulseDuration());
                    m_CalibrationParams.backlash_iterations++;
                    emit newLog(i18n("DEC backlash..."));
                }
                else
                {
                    // Copy of the section that ends CAL_BACKLASH
                    double cur_x, cur_y;
                    pmath->getStarScreenPosition(&cur_x, &cur_y);

                    calibrationStage = CAL_DEC_INC;
                    m_CalibrationCoords.start_x2         = cur_x;
                    m_CalibrationCoords.start_y2         = cur_y;
                    m_CalibrationCoords.last_x = cur_x;
                    m_CalibrationCoords.last_y = cur_y;

                    qCDebug(KSTARS_EKOS_GUIDE) << "Start X2 " << m_CalibrationCoords.start_x2 << " start Y2 " << m_CalibrationCoords.start_y2;
                    emit newPulse(DEC_INC_DIR, Options::calibrationPulseDuration());
                    m_CalibrationParams.dec_iterations++;
                    emit newLog(i18n("DEC drifting forward..."));
                }
                break;
            }
            // calc orientation
            if (pmath->calculateAndSetReticle1D(m_CalibrationCoords.start_x1, m_CalibrationCoords.start_y1, m_CalibrationCoords.end_x1,
                                                m_CalibrationCoords.end_y1, m_CalibrationParams.ra_total_pulse))
            {
                calibrationStage = CAL_IDLE;

                emit newStatus(Ekos::GUIDE_CALIBRATION_SUCESS);

                KSNotification::event(QLatin1String("CalibrationSuccessful"),
                                      i18n("Guiding calibration completed successfully"));
            }
            else
            {
                emit newLog(i18n("Calibration rejected. Star drift is too short. Check for mount, cable, or backlash problems."));

                calibrationStage = CAL_ERROR;

                emit newStatus(Ekos::GUIDE_CALIBRATION_ERROR);
                emit calibrationUpdate(GuideInterface::CALIBRATION_MESSAGE_ONLY, i18n("Calibration Failed: drift too short."));
                KSNotification::event(QLatin1String("CalibrationFailed"),
                                      i18n("Guiding calibration failed with errors"), KSNotification::EVENT_ALERT);
            }

            reset();
            break;
        }

        case CAL_BACKLASH:
        {
            double cur_x, cur_y;
            pmath->getStarScreenPosition(&cur_x, &cur_y);

            double driftRA, driftDEC;
            m_CalibrationParams.tempCalibration.computeDrift(
                Vector(cur_x, cur_y, 0),
                Vector(m_CalibrationCoords.start_backlash_x, m_CalibrationCoords.start_backlash_y, 0),
                &driftRA, &driftDEC);

            // Exit the backlash phase either after 5 pulses, or after we've moved sufficiently in the
            // DEC direction.
            constexpr int MIN_DEC_BACKLASH_MOVE_PIXELS = 3;
            if ((++m_CalibrationParams.backlash_iterations >= 5) ||
                    (fabs(driftDEC) > MIN_DEC_BACKLASH_MOVE_PIXELS))
            {
                // Copy of the section that ends CAL_RA_DEC when backlash is turned off.
                double cur_x, cur_y;
                pmath->getStarScreenPosition(&cur_x, &cur_y);
                emit calibrationUpdate(GuideInterface::BACKLASH, i18n("Calibrating DEC Backlash"),
                                       cur_x - m_CalibrationCoords.start_x1, cur_y - m_CalibrationCoords.start_y1);
                qCDebug(KSTARS_EKOS_GUIDE) << QString("Stopping dec backlash caibration after %1 iterations, offset %2")
                                           .arg(m_CalibrationParams.backlash_iterations - 1)
                                           .arg(driftDEC, 4, 'f', 2);
                calibrationStage = CAL_DEC_INC;
                m_CalibrationCoords.start_x2         = cur_x;
                m_CalibrationCoords.start_y2         = cur_y;
                m_CalibrationCoords.last_x = cur_x;
                m_CalibrationCoords.last_y = cur_y;

                qCDebug(KSTARS_EKOS_GUIDE) << "Start X2 " << m_CalibrationCoords.start_x2 << " start Y2 " << m_CalibrationCoords.start_y2;
                emit newPulse(DEC_INC_DIR, Options::calibrationPulseDuration());
                m_CalibrationParams.dec_iterations++;
                emit newLog(i18n("DEC drifting forward..."));
                break;
            }
            emit calibrationUpdate(GuideInterface::BACKLASH, i18n("Calibrating DEC Backlash"),
                                   cur_x - m_CalibrationCoords.start_x1, cur_y - m_CalibrationCoords.start_y1);
            qCDebug(KSTARS_EKOS_GUIDE) << "Backlash iter" << m_CalibrationParams.backlash_iterations << "position" << cur_x << cur_y;
            emit newPulse(DEC_INC_DIR, Options::calibrationPulseDuration());
            break;
        }
        case CAL_DEC_INC:
        {
            // Star position resulting from LAST guiding pulse to mount
            double cur_x, cur_y;
            pmath->getStarScreenPosition(&cur_x, &cur_y);

            emit calibrationUpdate(GuideInterface::DEC_OUT, i18n("Calibrating DEC Out"),
                                   cur_x - m_CalibrationCoords.start_x1, cur_y - m_CalibrationCoords.start_y1);

            qCDebug(KSTARS_EKOS_GUIDE) << "Iteration #" << m_CalibrationParams.dec_iterations << ": STAR " << cur_x << "," << cur_y;
            qCDebug(KSTARS_EKOS_GUIDE) << "Iteration " << m_CalibrationParams.dec_iterations << " Direction: DEC_INC_DIR" <<
                                       " Duration: " << m_CalibrationParams.last_pulse << " ms.";

            // Don't yet know how to tell NORTH vs SOUTH
            guideLog.addCalibrationData(DEC_INC_DIR, cur_x, cur_y,
                                        m_CalibrationCoords.start_x2, m_CalibrationCoords.start_y2);
            const double xDrift = cur_x - m_CalibrationCoords.start_x2;
            const double yDrift = cur_y - m_CalibrationCoords.start_y2;
            constexpr double SUFFICIENT_DEC_MOVE_PIXELS = 15.0;
            if (((m_CalibrationParams.dec_iterations >= m_CalibrationParams.auto_drift_time) ||
                    (std::hypot(xDrift, yDrift) > SUFFICIENT_DEC_MOVE_PIXELS))
                    && (fabs(xDrift) > 1.5 || fabs(yDrift) > 1.5))
            {
                calibrationStage = CAL_DEC_DEC;

                m_CalibrationParams.de_total_pulse += m_CalibrationParams.last_pulse;

                m_CalibrationCoords.end_x2 = cur_x;
                m_CalibrationCoords.end_y2 = cur_y;

                m_CalibrationCoords.last_x = cur_x;
                m_CalibrationCoords.last_y = cur_y;

                axis_calibration_complete = false;

                qCDebug(KSTARS_EKOS_GUIDE) << "End X2 " << m_CalibrationCoords.end_x2 << " End Y2 " << m_CalibrationCoords.end_y2;

                m_CalibrationParams.tempCalibration.calculate1D(m_CalibrationCoords.end_x2 - m_CalibrationCoords.start_x2,
                        m_CalibrationCoords.end_y2 - m_CalibrationCoords.start_y2,
                        m_CalibrationParams.de_total_pulse);

                m_CalibrationCoords.de_distance = 0;

                emit newPulse(DEC_DEC_DIR, m_CalibrationParams.last_pulse);
                emit newLog(i18n("DEC drifting reverse..."));
                m_CalibrationParams.dec_iterations++;
                guideLog.endCalibrationSection(DEC_INC_DIR, m_CalibrationParams.tempCalibration.getAngle());
            }
            else if (m_CalibrationParams.dec_iterations > m_CalibrationParams.turn_back_time)
            {
                calibrationStage = CAL_ERROR;

                emit newStatus(Ekos::GUIDE_CALIBRATION_ERROR);
                emit calibrationUpdate(GuideInterface::CALIBRATION_MESSAGE_ONLY, i18n("Calibration Failed: couldn't reach start point."));
                emit newLog(i18np("Guide DEC: Scope cannot reach the start point after %1 iteration.\nPossible mount "
                                  "or backlash problems...",
                                  "GUIDE DEC: Scope cannot reach the start point after %1 iterations.\nPossible mount "
                                  "or backlash problems...",
                                  m_CalibrationParams.dec_iterations));

                KSNotification::event(QLatin1String("CalibrationFailed"),
                                      i18n("Guiding calibration failed with errors"), KSNotification::EVENT_ALERT);
                guideLog.endCalibration(0, 0);
                reset();
            }
            else
            {
                if (fabs(cur_x - m_CalibrationCoords.last_x) < 0.5 && fabs(cur_y - m_CalibrationCoords.last_y) < 0.5)
                {
                    // Increase pulse by 200%
                    m_CalibrationParams.last_pulse = Options::calibrationPulseDuration() * 2;
                }
                else
                {
                    m_CalibrationParams.de_total_pulse += m_CalibrationParams.last_pulse;
                    m_CalibrationParams.last_pulse = Options::calibrationPulseDuration();
                }
                m_CalibrationCoords.last_x = cur_x;
                m_CalibrationCoords.last_y = cur_y;

                emit newPulse(DEC_INC_DIR, m_CalibrationParams.last_pulse);

                m_CalibrationParams.dec_iterations++;
            }
        }
        break;

        case CAL_DEC_DEC:
        {
            //----- Z-check (new!) -----
            double cur_x, cur_y;
            pmath->getStarScreenPosition(&cur_x, &cur_y);

            emit calibrationUpdate(GuideInterface::DEC_IN, i18n("Calibrating DEC In"),
                                   cur_x - m_CalibrationCoords.start_x1, cur_y - m_CalibrationCoords.start_y1);

            // Star position resulting from LAST guiding pulse to mount
            qCDebug(KSTARS_EKOS_GUIDE) << "Iteration #" << m_CalibrationParams.dec_iterations << ": STAR " << cur_x << "," << cur_y;
            qCDebug(KSTARS_EKOS_GUIDE) << "Iteration " << m_CalibrationParams.dec_iterations << " Direction: DEC_DEC_DIR" <<
                                       " Duration: " << m_CalibrationParams.last_pulse << " ms.";

            // Note: the way this temp calibration was set up above, with the DEC drifts, the ra axis is really dec.
            // This will help the dec find its way home. Could convert to a full RA/DEC calibration.
            double driftRA, driftDEC;
            m_CalibrationParams.tempCalibration.computeDrift(
                Vector(cur_x, cur_y, 0),
                Vector(m_CalibrationCoords.start_x1, m_CalibrationCoords.start_y1, 0),
                &driftRA, &driftDEC);

            qCDebug(KSTARS_EKOS_GUIDE) << "Currently " << driftRA << driftDEC << " from original point.";

            // Keep track of distance
            if (m_CalibrationCoords.de_distance == 0.0)
                m_CalibrationCoords.de_distance = driftRA;

            // South?
            guideLog.addCalibrationData(DEC_DEC_DIR, cur_x, cur_y,
                                        m_CalibrationCoords.start_x2, m_CalibrationCoords.start_y2);

            // start point reached... so exit
            if (driftRA < 1.5)
            {
                pmath->performProcessing();
                m_CalibrationParams.last_pulse = Options::calibrationPulseDuration();
                axis_calibration_complete = true;
            }
            // Increase pulse if we're not moving much or if we are moving _away_ from target.
            else if ( (fabs(cur_x - m_CalibrationCoords.last_x) < 0.5 && fabs(cur_y - m_CalibrationCoords.last_y) < 0.5)
                      || driftRA > m_CalibrationCoords.de_distance)
            {
                // Increase pulse by 200%
                m_CalibrationParams.last_pulse = Options::calibrationPulseDuration() * 2;
            }
            else
            {
                m_CalibrationParams.last_pulse = Options::calibrationPulseDuration();
            }

            if (axis_calibration_complete == false)
            {
                if (m_CalibrationParams.dec_iterations < m_CalibrationParams.turn_back_time)
                {
                    emit newPulse(DEC_DEC_DIR, m_CalibrationParams.last_pulse);
                    m_CalibrationParams.dec_iterations++;
                    break;
                }

                calibrationStage = CAL_ERROR;

                emit newStatus(Ekos::GUIDE_CALIBRATION_ERROR);
                emit calibrationUpdate(GuideInterface::CALIBRATION_MESSAGE_ONLY, i18n("Calibration Failed: couldn't reach start point."));

                emit newLog(i18np("Guide DEC: Scope cannot reach the start point after %1 iteration.\nPossible mount "
                                  "or backlash problems...",
                                  "Guide DEC: Scope cannot reach the start point after %1 iterations.\nPossible mount "
                                  "or backlash problems...",
                                  m_CalibrationParams.dec_iterations));

                KSNotification::event(QLatin1String("CalibrationFailed"),
                                      i18n("Guiding calibration failed with errors"), KSNotification::EVENT_ALERT);
                reset();
                break;
            }

            bool swap_dec = false;
            // calc orientation
            if (pmath->calculateAndSetReticle2D(m_CalibrationCoords.start_x1, m_CalibrationCoords.start_y1, m_CalibrationCoords.end_x1,
                                                m_CalibrationCoords.end_y1, m_CalibrationCoords.start_x2, m_CalibrationCoords.start_y2, m_CalibrationCoords.end_x2,
                                                m_CalibrationCoords.end_y2,
                                                &swap_dec, m_CalibrationParams.ra_total_pulse, m_CalibrationParams.de_total_pulse))
            {
                calibrationStage = CAL_IDLE;
                //fillInterface();
                if (swap_dec)
                    emit newLog(i18n("DEC swap enabled."));
                else
                    emit newLog(i18n("DEC swap disabled."));

                emit newStatus(Ekos::GUIDE_CALIBRATION_SUCESS);

                emit DESwapChanged(swap_dec);

                emit calibrationUpdate(GuideInterface::CALIBRATION_MESSAGE_ONLY, i18n("Calibration Successful"));
                KSNotification::event(QLatin1String("CalibrationSuccessful"),
                                      i18n("Guiding calibration completed successfully"));

                // Fill in mount
                // These rates are in ms/pixel. Convert to pixels/second
                guideLog.endCalibration(
                    1000.0 / pmath->getCalibration().raPulseMillisecondsPerPixel(),
                    1000.0 / pmath->getCalibration().decPulseMillisecondsPerPixel());
            }
            else
            {
                emit newLog(i18n("Calibration rejected. Star drift is too short. Check for mount, cable, or backlash problems."));
                emit calibrationUpdate(GuideInterface::CALIBRATION_MESSAGE_ONLY, i18n("Calibration Failed: drift too short."));

                emit newStatus(Ekos::GUIDE_CALIBRATION_ERROR);

                //ui.startCalibrationLED->setColor(alertColor);
                calibrationStage = CAL_ERROR;
                KSNotification::event(QLatin1String("CalibrationFailed"),
                                      i18n("Guiding calibration failed with errors"), KSNotification::EVENT_ALERT);
                guideLog.endCalibration(0, 0);
            }

            reset();

            break;
        }

        default:
            break;
    }
}

void InternalGuider::setStarPosition(QVector3D &starCenter)
{
    pmath->setReticleParameters(starCenter.x(), starCenter.y());
}

void InternalGuider::trackingStarSelected(int x, int y)
{
    if (calibrationStage == CAL_IDLE)
        return;

    //int square_size = guide_squares[pmath->getSquareIndex()].size;

    pmath->setReticleParameters(x, y);
    //pmath->moveSquare(x-square_size/(2*pmath->getBinX()), y-square_size/(2*pmath->getBinY()));

    //update_reticle_pos(x, y);

    //ui.selectStarLED->setColor(okColor);

    calibrationStage = CAL_START;

    //ui.pushButton_StartCalibration->setEnabled(true);

    /*QVector3D starCenter;
    starCenter.setX(x);
    starCenter.setY(y);
    emit newStarPosition(starCenter, true);*/

    //if (ui.autoStarCheck->isChecked())
    //if (Options::autoStarEnabled())
    //calibrate();
}

void InternalGuider::setDECSwap(bool enable)
{
    pmath->getMutableCalibration()->setDeclinationSwapEnabled(enable);
}

void InternalGuider::setSquareAlgorithm(int index)
{
    if (index == SEP_MULTISTAR && pmath->getSquareAlgorithmIndex() != SEP_MULTISTAR)
        m_isFirstFrame = true;
    pmath->setSquareAlgorithm(index);
}

void InternalGuider::setReticleParameters(double x, double y)
{
    pmath->setReticleParameters(x, y);
}

bool InternalGuider::getReticleParameters(double *x, double *y)
{
    return pmath->getReticleParameters(x, y);
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
    uint32_t tick = 0;

    // On first frame, center the box (reticle) around the star so we do not start with an offset the results in
    // unnecessary guiding pulses.
    if (m_isFirstFrame)
    {
        if (state == GUIDE_GUIDING)
        {
            Vector star_pos = pmath->findLocalStarPosition();
            pmath->setReticleParameters(star_pos.x, star_pos.y);
        }
        m_isFirstFrame = false;
    }
    // calc math. it tracks square
    pmath->performProcessing(&guideLog, state == GUIDE_GUIDING);

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

    // If within 95% of max pulse repeatedly, let's abort
    //    if (out->pulse_length[GUIDE_RA] >= (0.95 * Options::rAMaximumPulse()) ||
    //        out->pulse_length[GUIDE_DEC] >= (0.95 * Options::dECMaximumPulse()))
    //    {
    //        // Stop sending pulses in case we are guiding and we already sent one high pulse before
    //        // since we do not want to stray too much off the target to purse the guiding star
    //        if (state == GUIDE_GUIDING && m_highPulseCounter > 0)
    //            sendPulses = false;
    //        m_highPulseCounter++;
    //    }
    //    else
    //        m_highPulseCounter=0;

    double delta_rms = std::hypot(out->delta[GUIDE_RA], out->delta[GUIDE_DEC]);
    if (delta_rms > Options::guideMaxDeltaRMS())
    {
        // Stop sending pulses on the 3rd time the delta RMS is high
        // so that we don't stray too far off the main target.
        if (state == GUIDE_GUIDING && m_highRMSCounter > 2)
            sendPulses = false;
        m_highRMSCounter++;
    }
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

    tick = pmath->getTicks();

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

bool InternalGuider::processImageGuiding()
{
    static int maxPulseCounter = 0;
    const cproc_out_params *out;
    uint32_t tick = 0;

    // calc math. it tracks square
    pmath->performProcessing();

    if (pmath->isStarLost() && ++m_starLostCounter > 2)
    {
        emit newLog(i18n("Lost track of phase shift."));
        abort();
        return false;
    }
    else
        m_starLostCounter = 0;

    // do pulse
    out = pmath->getOutputParameters();

    // If within 90% of max pulse repeatedly, let's abort
    if (out->pulse_length[GUIDE_RA] >= (0.9 * Options::rAMaximumPulse()) ||
            out->pulse_length[GUIDE_DEC] >= (0.9 * Options::dECMaximumPulse()))
        maxPulseCounter++;
    else
        maxPulseCounter = 0;

    if (maxPulseCounter >= 3)
    {
        emit newLog(i18n("Lost track of phase shift. Aborting guiding..."));
        abort();
        maxPulseCounter = 0;
        return false;
    }

    emit newPulse(out->pulse_dir[GUIDE_RA], out->pulse_length[GUIDE_RA], out->pulse_dir[GUIDE_DEC],
                  out->pulse_length[GUIDE_DEC]);

    emit frameCaptureRequested();

    if (state == GUIDE_DITHERING || state == GUIDE_MANUAL_DITHERING)
        return true;

    tick = pmath->getTicks();

    if (tick & 1)
    {
        // draw some params in window
        emit newAxisDelta(out->delta[GUIDE_RA], out->delta[GUIDE_DEC]);

        emit newAxisPulse(out->pulse_length[GUIDE_RA], out->pulse_length[GUIDE_DEC]);

        emit newAxisSigma(out->sigma[GUIDE_RA], out->sigma[GUIDE_DEC]);
    }

    return true;
}

bool InternalGuider::isImageGuideEnabled() const
{
    return m_ImageGuideEnabled;
}

void InternalGuider::setImageGuideEnabled(bool value)
{
    m_ImageGuideEnabled = value;

    pmath->setImageGuideEnabled(value);
}

void InternalGuider::setRegionAxis(uint32_t value)
{
    pmath->setRegionAxis(value);
}

QList<Edge *> InternalGuider::getGuideStars()
{
    return pmath->PSFAutoFind();
}

bool InternalGuider::selectAutoStarSEPMultistar()
{
    guideFrame->updateFrame();
    QVector3D newStarCenter = pmath->selectGuideStar();
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

    FITSData *imageData = guideFrame->getImageData();

    if (imageData == nullptr)
        return false;

    bool useNativeDetection = false;

    QList<Edge *> starCenters;

    if (Options::guideAlgorithm() != SEP_THRESHOLD)
        starCenters = pmath->PSFAutoFind();

    if (starCenters.empty())
    {
        QVariantMap settings;
        settings["maxStarsCount"] = 50;
        settings["optionsProfileIndex"] = Options::guideOptionsProfile();
        settings["optionsProfileGroup"] = static_cast<int>(Ekos::GuideProfiles);
        imageData->setSourceExtractorSettings(settings);

        if (Options::guideAlgorithm() == SEP_THRESHOLD)
            imageData->findStars(ALGORITHM_SEP).waitForFinished();
        else
            imageData->findStars().waitForFinished();

        starCenters = imageData->getStarCenters();
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

    int maxX = imageData->width();
    int maxY = imageData->height();

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

    /*if (ui.autoSquareSizeCheck->isEnabled() && ui.autoSquareSizeCheck->isChecked())
    {
        // Select appropriate square size
        int idealSize = ceil(starCenters[maxScoreIndex]->width * 1.5);

        if (Options::guideLogging())
            qDebug() << "Guide: Ideal calibration box size for star width: " << starCenters[maxScoreIndex]->width << " is " << idealSize << " pixels";

        // TODO Set square size in GuideModule
    }*/

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
