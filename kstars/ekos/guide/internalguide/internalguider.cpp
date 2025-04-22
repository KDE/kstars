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
#include "fitsviewer/fitsdata.h"
#include "../guideview.h"
#include "ekos/guide/opsguide.h"

#include <KMessageBox>

#include <random>
#include <chrono>
#include <QTimer>
#include <QString>

#define MAX_GUIDE_STARS           10

using namespace std::chrono_literals;

namespace Ekos
{
InternalGuider::InternalGuider()
{
    // Create math object
    pmath.reset(new cgmath());
    connect(pmath.get(), &cgmath::newStarPosition, this, &InternalGuider::newStarPosition);
    connect(pmath.get(), &cgmath::guideStats, this, &InternalGuider::guideStats);

    state = GUIDE_IDLE;
    m_DitherOrigin = QVector3D(0, 0, 0);

    emit guideInfo("");

    m_darkGuideTimer = std::make_unique<QTimer>(this);
    m_captureTimer = std::make_unique<QTimer>(this);
    m_ditherSettleTimer = std::make_unique<QTimer>(this);

    setDarkGuideTimerInterval();

    setExposureTime();

    connect(this, &Ekos::GuideInterface::frameCaptureRequested, this, [ = ]()
    {
        this->m_captureTimer->start();
    });
}

void InternalGuider::setExposureTime()
{
    Seconds seconds(Options::guideExposure());
    setTimer(m_captureTimer, seconds);
}

void InternalGuider::setTimer(std::unique_ptr<QTimer> &timer, Seconds seconds)
{
    const std::chrono::duration<double, std::milli> inMilliseconds(seconds);
    timer->setInterval((int)(inMilliseconds.count()));
}

void InternalGuider::setDarkGuideTimerInterval()
{
    constexpr double kMinInterval = 0.5;  // 0.5s is the shortest allowed dark-guiding period.
    const Seconds seconds(std::max(kMinInterval, Options::gPGDarkGuidingInterval()));
    setTimer(m_darkGuideTimer, seconds);
}

void InternalGuider::resetDarkGuiding()
{
    m_darkGuideTimer->stop();
    m_captureTimer->stop();
}

bool InternalGuider::isInferencePeriodFinished()
{
    auto const contribution = pmath->getGPG().predictionContribution();
    return contribution >= 0.99;
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
    m_GuideFrame->disconnect(this);

    pmath->start();
    emit guideInfo("");

    m_starLostCounter = 0;
    m_highRMSCounter = 0;
    m_DitherOrigin = QVector3D(0, 0, 0);

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

    startDarkGuiding();

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

    disableDitherSettleTimer();

    logFile.close();
    guideLog.endGuiding();
    emit guideInfo("");

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

    resetDarkGuiding();
    disconnect(m_darkGuideTimer.get(), nullptr, nullptr, nullptr);

    pmath->abort();



    m_ProgressiveDither.clear();
    m_starLostCounter = 0;
    m_highRMSCounter = 0;

    m_DitherOrigin = QVector3D(0, 0, 0);

    pmath->suspend(false);
    state = GUIDE_IDLE;
    qCDebug(KSTARS_EKOS_GUIDE) << "Guiding aborted.";

    return true;
}

bool InternalGuider::suspend()
{
    disableDitherSettleTimer();
    guideLog.pauseInfo();
    state = GUIDE_SUSPENDED;

    resetDarkGuiding();
    emit newStatus(state);

    pmath->suspend(true);
    emit guideInfo("");

    return true;
}

void InternalGuider::startDarkGuiding()
{
    if (Options::gPGDarkGuiding())
    {
        connect(m_darkGuideTimer.get(), &QTimer::timeout, this, &InternalGuider::darkGuide, Qt::UniqueConnection);

        // Start the two dark guide timers. The capture timer is started automatically by a signal.
        m_darkGuideTimer->start();

        qCDebug(KSTARS_EKOS_GUIDE) << "Starting dark guiding.";
    }
}

bool InternalGuider::resume()
{
    qCDebug(KSTARS_EKOS_GUIDE) << "Resuming...";
    emit guideInfo("");
    guideLog.resumeInfo();
    state = GUIDE_GUIDING;
    emit newStatus(state);

    pmath->suspend(false);

    startDarkGuiding();

    setExposureTime();

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
    if (Options::ditherWithOnePulse() )
        return onePulseDither(pixels);

    double ret_x, ret_y;
    pmath->getTargetPosition(&ret_x, &ret_y);

    // Just calling getStarScreenPosition() will get the position at the last time the guide star
    // was found, which is likely before the most recent guide pulse.
    // Instead we call findLocalStarPosition() which does the analysis from the image.
    // Unfortunately, processGuiding() will repeat that computation.
    // We currently don't cache it.
    GuiderUtils::Vector star_position = pmath->findLocalStarPosition(m_ImageData, m_GuideFrame, false);
    if (pmath->isStarLost() || (star_position.x == -1) || (star_position.y == -1))
    {
        // If the star position is lost, just lose this iteration.
        // If it happens too many time, abort.
        if (++m_starLostCounter > MAX_LOST_STAR_THRESHOLD)
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

        if (m_DitherOrigin.x() == 0 && m_DitherOrigin.y() == 0)
        {
            m_DitherOrigin = QVector3D(ret_x, ret_y, 0);
        }
        double totalXOffset = ret_x - m_DitherOrigin.x();
        double totalYOffset = ret_y - m_DitherOrigin.y();

        // if we've dithered too far, and diff_x or diff_y is pushing us even further away, then change its direction.
        // Note: it is possible that we've dithered too far, but diff_x/y is pointing in the right direction.
        // Don't change it in that 2nd case.
        if (((diff_x + totalXOffset > MAX_DITHER_TRAVEL) && (diff_x > 0)) ||
                ((diff_x + totalXOffset < -MAX_DITHER_TRAVEL) && (diff_x < 0)))
        {
            qCDebug(KSTARS_EKOS_GUIDE)
                    << QString("Dithering target off by too much in X (abs(%1 + %2) > %3), adjust diff_x from %4 to %5")
                    .arg(diff_x).arg(totalXOffset).arg(MAX_DITHER_TRAVEL).arg(diff_x).arg(diff_x * -1.5);
            diff_x *= -1.5;
        }
        if (((diff_y + totalYOffset > MAX_DITHER_TRAVEL) && (diff_y > 0)) ||
                ((diff_y + totalYOffset < -MAX_DITHER_TRAVEL) && (diff_y < 0)))
        {
            qCDebug(KSTARS_EKOS_GUIDE)
                    << QString("Dithering target off by too much in Y (abs(%1 + %2) > %3), adjust diff_y from %4 to %5")
                    .arg(diff_y).arg(totalYOffset).arg(MAX_DITHER_TRAVEL).arg(diff_y).arg(diff_y * -1.5);
            diff_y *= -1.5;
        }

        m_DitherTargetPosition = GuiderUtils::Vector(ret_x, ret_y, 0) + GuiderUtils::Vector(diff_x, diff_y, 0);

        qCDebug(KSTARS_EKOS_GUIDE)
                << QString("Dithering by %1 pixels. Target:  %2,%3 Current: %4,%5 Move: %6,%7 Wander: %8,%9")
                .arg(pixels, 3, 'f', 1)
                .arg(m_DitherTargetPosition.x, 5, 'f', 1).arg(m_DitherTargetPosition.y, 5, 'f', 1)
                .arg(ret_x, 5, 'f', 1).arg(ret_y, 5, 'f', 1)
                .arg(diff_x, 4, 'f', 1).arg(diff_y, 4, 'f', 1)
                .arg(totalXOffset + diff_x, 5, 'f', 1).arg(totalYOffset + diff_y, 5, 'f', 1);
        guideLog.ditherInfo(diff_x, diff_y, m_DitherTargetPosition.x, m_DitherTargetPosition.y);

        pmath->setTargetPosition(m_DitherTargetPosition.x, m_DitherTargetPosition.y);

        if (Options::rAGuidePulseAlgorithm() == OpsGuide::GPG_ALGORITHM)
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

    double pixelOffsetX = m_DitherTargetPosition.x - star_position.x;
    double pixelOffsetY = m_DitherTargetPosition.y - star_position.y;

    qCDebug(KSTARS_EKOS_GUIDE)
            << QString("Dithering in progress.   Current: %1,%2 Target:  %3,%4 Diff: %5,%6 Wander: %8,%9")
            .arg(star_position.x, 5, 'f', 1).arg(star_position.y, 5, 'f', 1)
            .arg(m_DitherTargetPosition.x, 5, 'f', 1).arg(m_DitherTargetPosition.y, 5, 'f', 1)
            .arg(pixelOffsetX, 4, 'f', 1).arg(pixelOffsetY, 4, 'f', 1)
            .arg(star_position.x - m_DitherOrigin.x(), 5, 'f', 1)
            .arg(star_position.y - m_DitherOrigin.y(), 5, 'f', 1);

    if (Options::ditherWithOnePulse() || (fabs(driftRA) < 1 && fabs(driftDEC) < 1))
    {
        pmath->setTargetPosition(star_position.x, star_position.y);

        // In one-pulse dithering we want the target to be whereever we end up
        // after the pulse. So, the first guide frame should not send any pulses
        // and should reset the reticle to the position it finds.
        if (Options::ditherWithOnePulse())
            m_isFirstFrame = true;

        qCDebug(KSTARS_EKOS_GUIDE) << "Dither complete.";

        if (Options::ditherSettle() > 0)
        {
            state = GUIDE_DITHERING_SETTLE;
            guideLog.settleStartedInfo();
            emit newStatus(state);
        }

        if (Options::rAGuidePulseAlgorithm() == OpsGuide::GPG_ALGORITHM)

            pmath->getGPG().ditheringSettled(true);

        startDitherSettleTimer(Options::ditherSettle() * 1000);
    }
    else
    {
        if (++m_DitherRetries > Options::ditherMaxIterations())
            return abortDither();

        processGuiding();
    }

    return true;
}
bool InternalGuider::onePulseDither(double pixels)
{
    qCDebug(KSTARS_EKOS_GUIDE) << "OnePulseDither(" << "pixels" << ")";

    // Cancel any current guide exposures.
    emit abortExposure();

    double ret_x, ret_y;
    pmath->getTargetPosition(&ret_x, &ret_y);

    auto seed = std::chrono::system_clock::now().time_since_epoch().count();
    std::default_random_engine generator(seed);
    std::uniform_real_distribution<double> angleMagnitude(0, 360);

    double angle  = angleMagnitude(generator) * dms::DegToRad;
    double diff_x = pixels * cos(angle);
    double diff_y = pixels * sin(angle);

    if (pmath->getCalibration().declinationSwapEnabled())
        diff_y *= -1;

    if (m_DitherOrigin.x() == 0 && m_DitherOrigin.y() == 0)
    {
        m_DitherOrigin = QVector3D(ret_x, ret_y, 0);
    }
    double totalXOffset = ret_x - m_DitherOrigin.x();
    double totalYOffset = ret_y - m_DitherOrigin.y();

    // If we've dithered too far, and diff_x or diff_y is pushing us even further away, then change its direction.
    // Note: it is possible that we've dithered too far, but diff_x/y is pointing in the right direction.
    // Don't change it in that 2nd case.
    if (((diff_x + totalXOffset > MAX_DITHER_TRAVEL) && (diff_x > 0)) ||
            ((diff_x + totalXOffset < -MAX_DITHER_TRAVEL) && (diff_x < 0)))
    {
        qCDebug(KSTARS_EKOS_GUIDE)
                << QString("OPD: Dithering target off by too much in X (abs(%1 + %2) > %3), adjust diff_x from %4 to %5")
                .arg(diff_x).arg(totalXOffset).arg(MAX_DITHER_TRAVEL).arg(diff_x).arg(diff_x * -1.5);
        diff_x *= -1.5;
    }
    if (((diff_y + totalYOffset > MAX_DITHER_TRAVEL) && (diff_y > 0)) ||
            ((diff_y + totalYOffset < -MAX_DITHER_TRAVEL) && (diff_y < 0)))
    {
        qCDebug(KSTARS_EKOS_GUIDE)
                << QString("OPD: Dithering target off by too much in Y (abs(%1 + %2) > %3), adjust diff_y from %4 to %5")
                .arg(diff_y).arg(totalYOffset).arg(MAX_DITHER_TRAVEL).arg(diff_y).arg(diff_y * -1.5);
        diff_y *= -1.5;
    }

    m_DitherTargetPosition = GuiderUtils::Vector(ret_x, ret_y, 0) + GuiderUtils::Vector(diff_x, diff_y, 0);

    qCDebug(KSTARS_EKOS_GUIDE)
            << QString("OPD: Dithering by %1 pixels. Target:  %2,%3 Current: %4,%5 Move: %6,%7 Wander: %8,%9")
            .arg(pixels, 3, 'f', 1)
            .arg(m_DitherTargetPosition.x, 5, 'f', 1).arg(m_DitherTargetPosition.y, 5, 'f', 1)
            .arg(ret_x, 5, 'f', 1).arg(ret_y, 5, 'f', 1)
            .arg(diff_x, 4, 'f', 1).arg(diff_y, 4, 'f', 1)
            .arg(totalXOffset + diff_x, 5, 'f', 1).arg(totalYOffset + diff_y, 5, 'f', 1);
    guideLog.ditherInfo(diff_x, diff_y, m_DitherTargetPosition.x, m_DitherTargetPosition.y);

    pmath->setTargetPosition(m_DitherTargetPosition.x, m_DitherTargetPosition.y);

    if (Options::rAGuidePulseAlgorithm() == OpsGuide::GPG_ALGORITHM)
        // This is the offset in image coordinates, but needs to be converted to RA.
        pmath->getGPG().startDithering(diff_x, diff_y, pmath->getCalibration());

    state = GUIDE_DITHERING;
    emit newStatus(state);

    const GuiderUtils::Vector xyMove(diff_x, diff_y, 0);
    const GuiderUtils::Vector raDecMove = pmath->getCalibration().rotateToRaDec(xyMove);
    double raPulse = fabs(raDecMove.x * pmath->getCalibration().raPulseMillisecondsPerArcsecond());
    double decPulse = fabs(raDecMove.y * pmath->getCalibration().decPulseMillisecondsPerArcsecond());
    auto raDir = raDecMove.x > 0 ? RA_INC_DIR : RA_DEC_DIR;
    auto decDir = raDecMove.y > 0 ? DEC_DEC_DIR : DEC_INC_DIR;

    m_isFirstFrame = true;

    // Send pulse if we have one active direction at least.
    QString raDirString = raDir == RA_DEC_DIR ? "RA_DEC" : "RA_INC";
    QString decDirString = decDir == DEC_INC_DIR ? "DEC_INC" : "DEC_DEC";

    qCDebug(KSTARS_EKOS_GUIDE) << "OnePulseDither RA: " << raPulse << "ms" << raDirString << " DEC: " << decPulse << "ms " <<
                               decDirString;

    // Don't capture because the single shot timer below will trigger a capture.
    emit newMultiPulse(raDir, raPulse, decDir, decPulse, DontCaptureAfterPulses);

    double totalMSecs = 1000.0 * Options::ditherSettle() + std::max(raPulse, decPulse) + 100;

    state = GUIDE_DITHERING_SETTLE;
    guideLog.settleStartedInfo();
    emit newStatus(state);

    if (Options::rAGuidePulseAlgorithm() == OpsGuide::GPG_ALGORITHM)
        pmath->getGPG().ditheringSettled(true);

    startDitherSettleTimer(totalMSecs);
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

        if (Options::rAGuidePulseAlgorithm() == OpsGuide::GPG_ALGORITHM)
            pmath->getGPG().ditheringSettled(false);

        startDitherSettleTimer(Options::ditherSettle() * 1000);
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

            startDitherSettleTimer(Options::ditherSettle() * 1000);
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

            startDitherSettleTimer(Options::ditherSettle() * 1000);
            return true;
        }

        processGuiding();
    }

    return true;
}

void InternalGuider::startDitherSettleTimer(int ms)
{
    m_ditherSettleTimer->setSingleShot(true);
    connect(m_ditherSettleTimer.get(), &QTimer::timeout, this, &InternalGuider::setDitherSettled, Qt::UniqueConnection);
    m_ditherSettleTimer->start(ms);
}

void InternalGuider::disableDitherSettleTimer()
{
    disconnect(m_ditherSettleTimer.get());
    m_ditherSettleTimer->stop();
}

void InternalGuider::setDitherSettled()
{
    disableDitherSettleTimer();

    // Shouldn't be in these states, but just in case...
    if (state != GUIDE_IDLE && state != GUIDE_ABORTED && state != GUIDE_SUSPENDED)
    {
        guideLog.settleCompletedInfo();
        emit newStatus(Ekos::GUIDE_DITHERING_SUCCESS);

        // Back to guiding
        state = GUIDE_GUIDING;
    }
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
        emit newStatus(Ekos::GUIDE_CALIBRATION_SUCCESS);
        KSNotification::event(QLatin1String("CalibrationRestored"),
                              i18n("Guiding calibration restored"), KSNotification::Guide);
        reset();
        return true;
    }

    // Initialize the calibration parameters.
    // CCD pixel values comes in in microns and we want mm.
    pmath->getMutableCalibration()->setParameters(
        ccdPixelSizeX / 1000.0, ccdPixelSizeY / 1000.0, mountFocalLength,
        subBinX, subBinY, pierSide, mountRA, mountDEC);

    calibrationProcess->useCalibration(pmath->getMutableCalibration());

    m_GuideFrame->disconnect(this);

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
        auto const timeStep = calculateGPGTimeStep();
        pmath->performProcessing(GUIDE_CALIBRATING, m_ImageData, m_GuideFrame, timeStep);

        QString info = "";
        if (pmath->usingSEPMultiStar())
        {
            auto gs = pmath->getGuideStars();
            info = QString("%1 stars, %2/%3 refs")
                   .arg(gs.getNumStarsDetected())
                   .arg(gs.getNumReferencesFound())
                   .arg(gs.getNumReferences());
        }
        emit guideInfo(info);

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
        emit newSinglePulse(pulseDirection, pulseMsecs, StartCaptureAfterPulses);

    if (status == GUIDE_CALIBRATION_ERROR)
    {
        KSNotification::event(QLatin1String("CalibrationFailed"), i18n("Guiding calibration failed"),
                              KSNotification::Guide, KSNotification::Alert);
        reset();
    }
    else if (status == GUIDE_CALIBRATION_SUCCESS)
    {
        KSNotification::event(QLatin1String("CalibrationSuccessful"),
                              i18n("Guiding calibration completed successfully"), KSNotification::Guide);
        emit DESwapChanged(pmath->getCalibration().declinationSwapEnabled());
        pmath->setTargetPosition(calibrationStartX, calibrationStartY);
        reset();
    }
}

void InternalGuider::setGuideView(const QSharedPointer<GuideView> &guideView)
{
    m_GuideFrame = guideView;
}

void InternalGuider::setImageData(const QSharedPointer<FITSData> &data)
{
    m_ImageData = data;
    if (Options::saveGuideImages())
    {
        QDateTime now(QDateTime::currentDateTime());
        QString path = QDir(KSPaths::writableLocation(QStandardPaths::AppLocalDataLocation)).filePath("guide/" +
                       now.toString("yyyy-MM-dd"));
        QDir dir;
        dir.mkpath(path);
        // IS8601 contains colons but they are illegal under Windows OS, so replacing them with '-'
        // The timestamp is no longer ISO8601 but it should solve interoperality issues between different OS hosts
        QString name     = "guide_frame_" + now.toString("HH-mm-ss") + ".fits";
        QString filename = path + QStringLiteral("/") + name;
        m_ImageData->saveImage(filename);
    }
}

void InternalGuider::reset()
{
    qCDebug(KSTARS_EKOS_GUIDE) << "Resetting internal guider...";
    state = GUIDE_IDLE;

    resetDarkGuiding();

    connect(m_GuideFrame.get(), &FITSView::trackingStarSelected, this, &InternalGuider::trackingStarSelected,
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
    Q_UNUSED(x);
    Q_UNUSED(y);
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

void InternalGuider::setStarDetectionAlgorithm(int index)
{
    if (index == SEP_MULTISTAR && !pmath->usingSEPMultiStar())
        m_isFirstFrame = true;
    pmath->setStarDetectionAlgorithmIndex(index);
}

bool InternalGuider::getReticleParameters(double * x, double * y)
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
    return pmath->setGuiderParameters(mountAperture);
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

void InternalGuider::emitAxisPulse(const cproc_out_params * out)
{
    double raPulse = out->pulse_length[GUIDE_RA];
    double dePulse = out->pulse_length[GUIDE_DEC];

    //If the pulse was not sent to the mount, it should have 0 value
    if(out->pulse_dir[GUIDE_RA] == NO_DIR)
        raPulse = 0;
    //If the pulse was not sent to the mount, it should have 0 value
    if(out->pulse_dir[GUIDE_DEC] == NO_DIR)
        dePulse = 0;
    //If the pulse was to the east, it should have a negative sign.
    //(Tracking pulse has to be decreased.)
    if(out->pulse_dir[GUIDE_RA] == RA_INC_DIR)
        raPulse = -raPulse;
    //If the pulse was to the south, it should have a negative sign.
    if(out->pulse_dir[GUIDE_DEC] == DEC_DEC_DIR)
        dePulse = -dePulse;

    emit newAxisPulse(raPulse, dePulse);
}

bool InternalGuider::processGuiding()
{
    const cproc_out_params *out;

    // On first frame, center the box (reticle) around the star so we do not start with an offset the results in
    // unnecessary guiding pulses.
    bool process = true;

    if (m_isFirstFrame)
    {
        m_isFirstFrame = false;
        if (state == GUIDE_GUIDING)
        {
            GuiderUtils::Vector star_pos = pmath->findLocalStarPosition(m_ImageData, m_GuideFrame, true);
            if (star_pos.x != -1 && star_pos.y != -1)
                pmath->setTargetPosition(star_pos.x, star_pos.y);
            else
            {
                // We were not able to get started.
                process = false;
                m_isFirstFrame = true;
            }
        }
    }

    if (process)
    {
        auto const timeStep = calculateGPGTimeStep();
        pmath->performProcessing(state, m_ImageData, m_GuideFrame, timeStep, &guideLog);
        if (pmath->usingSEPMultiStar())
        {
            QString info = "";
            auto gs = pmath->getGuideStars();
            info = QString("%1 stars, %2/%3 refs")
                   .arg(gs.getNumStarsDetected())
                   .arg(gs.getNumReferencesFound())
                   .arg(gs.getNumReferences());

            emit guideInfo(info);
        }

        // Restart the dark-guiding timer, so we get the full interval on its 1st timeout.
        if (this->m_darkGuideTimer->isActive())
            this->m_darkGuideTimer->start();
    }

    if (state == GUIDE_SUSPENDED)
    {
        if (Options::rAGuidePulseAlgorithm() == OpsGuide::GPG_ALGORITHM)
            emit frameCaptureRequested();
        return true;
    }
    else
    {
        if (pmath->isStarLost())
            m_starLostCounter++;
        else
            m_starLostCounter = 0;
    }

    // do pulse
    out = pmath->getOutputParameters();

    if (isPoorGuiding(out))
        return true;

    // This is an old guide computaion that should be ignored.
    // One-pulse-dither sends out its own pulse signal.
    if ((state == GUIDE_DITHERING_SETTLE || state == GUIDE_DITHERING) && Options::ditherWithOnePulse())
        return true;

    bool sendPulses = !pmath->isStarLost();

    // Send pulse if we have one active direction at least.
    if (sendPulses && (out->pulse_dir[GUIDE_RA] != NO_DIR || out->pulse_dir[GUIDE_DEC] != NO_DIR))
    {
        emit newMultiPulse(out->pulse_dir[GUIDE_RA], out->pulse_length[GUIDE_RA],
                           out->pulse_dir[GUIDE_DEC], out->pulse_length[GUIDE_DEC], StartCaptureAfterPulses);
    }
    else
        emit frameCaptureRequested();

    if (state == GUIDE_DITHERING || state == GUIDE_MANUAL_DITHERING || state == GUIDE_DITHERING_SETTLE)
        return true;

    // Hy 9/13/21: Check above just looks for GUIDE_DITHERING or GUIDE_MANUAL_DITHERING or GUIDE_DITHERING_SETTLE
    // but not the other dithering possibilities (error, success).
    // Not sure if they should be included above, so conservatively not changing the
    // code, but don't think they should broadcast the newAxisDelta which might
    // interrup a capture.
    if (state < GUIDE_DITHERING)
        // out->delta[] is saved as STAR drift in the camera sensor coordinate system in
        // gmath->processAxis(). To get these values in the RADEC system they have to be negated.
        // But we want the MOUNT drift (cf. PHD2) and hence the values have to be negated once more! So...
        emit newAxisDelta(out->delta[GUIDE_RA], out->delta[GUIDE_DEC]);

    emitAxisPulse(out);
    emit newAxisSigma(out->sigma[GUIDE_RA], out->sigma[GUIDE_DEC]);
    if (SEPMultiStarEnabled())
        emit newSNR(pmath->getGuideStarSNR());

    return true;
}


// Here we calculate the time until the next time we will be emitting guiding corrections.
std::pair<Seconds, Seconds> InternalGuider::calculateGPGTimeStep()
{
    Seconds timeStep;

    const Seconds guideDelay{(Options::guideDelay())};

    auto const captureInterval = Seconds(m_captureTimer->intervalAsDuration()) + guideDelay;
    auto const darkGuideInterval = Seconds(m_darkGuideTimer->intervalAsDuration());

    if (!Options::gPGDarkGuiding() || !isInferencePeriodFinished())
    {
        return std::pair<Seconds, Seconds>(captureInterval, captureInterval);
    }
    auto const captureTimeRemaining = Seconds(m_captureTimer->remainingTimeAsDuration()) + guideDelay;
    auto const darkGuideTimeRemaining = Seconds(m_darkGuideTimer->remainingTimeAsDuration());
    // Are both firing at the same time (or at least, both due)?
    if (captureTimeRemaining <= Seconds::zero()
            && darkGuideTimeRemaining <= Seconds::zero())
    {
        timeStep = std::min(captureInterval, darkGuideInterval);
    }
    else if (captureTimeRemaining <= Seconds::zero())
    {
        timeStep = std::min(captureInterval, darkGuideTimeRemaining);
    }
    else if (darkGuideTimeRemaining <= Seconds::zero())
    {
        timeStep = std::min(captureTimeRemaining, darkGuideInterval);
    }
    else
    {
        timeStep = std::min(captureTimeRemaining, darkGuideTimeRemaining);
    }
    return std::pair<Seconds, Seconds>(timeStep, captureInterval);
}



void InternalGuider::darkGuide()
{
    // Only dark guide when guiding--e.g. don't dark guide if dithering.
    if (state != GUIDE_GUIDING)
        return;

    if(Options::gPGDarkGuiding() && isInferencePeriodFinished())
    {
        const cproc_out_params *out;
        auto const timeStep = calculateGPGTimeStep();
        pmath->performDarkGuiding(state, timeStep);

        out = pmath->getOutputParameters();
        emit newSinglePulse(out->pulse_dir[GUIDE_RA], out->pulse_length[GUIDE_RA], DontCaptureAfterPulses);

        emitAxisPulse(out);
    }
}

bool InternalGuider::isPoorGuiding(const cproc_out_params * out)
{
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
    return false;
}
bool InternalGuider::selectAutoStarSEPMultistar()
{
    m_GuideFrame->updateFrame();
    m_DitherOrigin = QVector3D(0, 0, 0);
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
    m_DitherOrigin = QVector3D(0, 0, 0);
    if (Options::guideAlgorithm() == SEP_MULTISTAR)
        return selectAutoStarSEPMultistar();

    bool useNativeDetection = false;

    QList<Edge *> starCenters;

    if (Options::guideAlgorithm() != SEP_THRESHOLD)
        starCenters = GuideAlgorithms::detectStars(m_ImageData, m_GuideFrame->getTrackingBox());

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

        m_GuideFrame->setStarsEnabled(true);
        m_GuideFrame->updateFrame();
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

            startDitherSettleTimer(Options::ditherSettle() * 1000);
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

void InternalGuider::fillGuideInfo(GuideLog::GuideInfo * info)
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
    info->xrate = 1000.0 / pmath->getCalibration().raPulseMillisecondsPerArcsecond();
    info->yrate = 1000.0 / pmath->getCalibration().decPulseMillisecondsPerArcsecond();
}

void InternalGuider::updateGPGParameters()
{
    setDarkGuideTimerInterval();
    pmath->getGPG().updateParameters();
}

void InternalGuider::resetGPG()
{
    pmath->getGPG().reset();
    resetDarkGuiding();
}

const Calibration &InternalGuider::getCalibration() const
{
    return pmath->getCalibration();
}
}
