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
    connect(pmath.get(), SIGNAL(newStarPosition(QVector3D,bool)), this, SIGNAL(newStarPosition(QVector3D,bool)));

    state = GUIDE_IDLE;    
}

bool InternalGuider::guide()
{
    if (state == GUIDE_SUSPENDED)
        return true;

    if (state >= GUIDE_GUIDING)
    {
        if (m_ImageGuideEnabled)
            return processImageGuiding();
        else
            return processGuiding();
    }

    guideFrame->disconnect(this);    

    pmath->start();

    m_starLostCounter = 0;
    m_highRMSCounter= 0;

    // TODO re-enable rapid check later on
#if 0
    m_isStarted = true;
    m_useRapidGuide = ui.rapidGuideCheck->isChecked();
    if (m_useRapidGuide)
        guideModule->startRapidGuide();

    emit newStatus(Ekos::GUIDE_GUIDING);

    guideModule->setSuspended(false);

    first_frame = true;

    if (ui.subFrameCheck->isEnabled() && ui.subFrameCheck->isChecked() && m_isSubFramed == false)
        first_subframe = true;

    capture();

#endif

    m_isFirstFrame = true;

    state = GUIDE_GUIDING;
    emit newStatus(state);

    emit frameCaptureRequested();

    return true;
}

bool InternalGuider::abort()
{
    calibrationStage = CAL_IDLE;

    logFile.close();

    if (state == GUIDE_CALIBRATING || state == GUIDE_GUIDING || state == GUIDE_DITHERING)
    {
        if (state == GUIDE_DITHERING)
            emit newStatus(GUIDE_DITHERING_ERROR);
        emit newStatus(GUIDE_ABORTED);

        qCDebug(KSTARS_EKOS_GUIDE) << "Aborting" << getGuideStatusString(state);
    }
    else
    {
        emit newStatus(GUIDE_IDLE);
        qCDebug(KSTARS_EKOS_GUIDE) << "Stopping internal guider.";
    }

    m_starLostCounter=0;
    m_highRMSCounter=0;
    accumulator.first = accumulator.second = 0;

    pmath->suspend(false);
    state = GUIDE_IDLE;

    return true;
}

bool InternalGuider::suspend()
{
    state = GUIDE_SUSPENDED;
    emit newStatus(state);

    pmath->suspend(true);

    return true;
}

bool InternalGuider::resume()
{
    state = GUIDE_GUIDING;
    emit newStatus(state);

    pmath->suspend(false);

    emit frameCaptureRequested();

    return true;
}

bool InternalGuider::ditherXY(int x, int y)
{
    m_DitherRetries=0;
    double cur_x, cur_y, ret_angle;
    pmath->getReticleParameters(&cur_x, &cur_y, &ret_angle);
    m_DitherTargetPosition = Vector(x, y, 0);
    pmath->setReticleParameters(m_DitherTargetPosition.x, m_DitherTargetPosition.y, ret_angle);

    state = GUIDE_DITHERING;
    emit newStatus(state);

    processGuiding();

    return true;
}

bool InternalGuider::dither(double pixels)
{        
    double cur_x, cur_y, ret_angle;
    pmath->getReticleParameters(&cur_x, &cur_y, &ret_angle);
    pmath->getStarScreenPosition(&cur_x, &cur_y);
    Ekos::Matrix ROT_Z = pmath->getROTZ();   

    if (state != GUIDE_DITHERING)
    {
        m_DitherRetries = 0;

        auto seed = std::chrono::system_clock::now().time_since_epoch().count();
        std::default_random_engine generator(seed);
        std::uniform_real_distribution<double> angleMagnitude(0, 360);

        double angle  = angleMagnitude(generator) * dms::DegToRad;
        double diff_x = pixels * cos(angle);
        double diff_y = pixels * sin(angle);

        if (pmath->declinationSwapEnabled())
            diff_y *= -1;

        if (fabs(diff_x + accumulator.first) > MAX_DITHER_TRAVEL)
            diff_x *= -1;
        accumulator.first += diff_x;
        if (fabs(diff_y + accumulator.second) > MAX_DITHER_TRAVEL)
            diff_y *= -1;
        accumulator.second += diff_y;

        m_DitherTargetPosition = Vector(cur_x, cur_y, 0) + Vector(diff_x, diff_y, 0);

        qCDebug(KSTARS_EKOS_GUIDE) << "Dithering process started.. Reticle Target Pos X " << m_DitherTargetPosition.x << " Y " << m_DitherTargetPosition.y;

        pmath->setReticleParameters(m_DitherTargetPosition.x, m_DitherTargetPosition.y, ret_angle);

        state = GUIDE_DITHERING;
        emit newStatus(state);

        processGuiding();

        return true;
    }

    Vector star_pos = Vector(cur_x, cur_y, 0) - Vector(m_DitherTargetPosition.x, m_DitherTargetPosition.y, 0);
    star_pos.y      = -star_pos.y;
    star_pos        = star_pos * ROT_Z;

    qCDebug(KSTARS_EKOS_GUIDE) << "Dithering in progress. Diff star X:" << star_pos.x << "Y:" << star_pos.y;

    if (fabs(star_pos.x) < 1 && fabs(star_pos.y) < 1)
    {
        pmath->setReticleParameters(cur_x, cur_y, ret_angle);
        qCDebug(KSTARS_EKOS_GUIDE) << "Dither complete.";

        if (Options::ditherSettle() > 0)
        {
            state = GUIDE_DITHERING_SETTLE;
            emit newStatus(state);
        }

        QTimer::singleShot(Options::ditherSettle()* 1000, this, SLOT(setDitherSettled()));
    }
    else
    {
        if (++m_DitherRetries > Options::ditherMaxIterations())
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
                    emit newStatus(state);
                }

                QTimer::singleShot(Options::ditherSettle()* 1000, this, SLOT(setDitherSettled()));
                return true;
            }
        }

        processGuiding();
    }

    return true;
}

void InternalGuider::setDitherSettled()
{
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
        KMessageBox::error(nullptr, i18n("%1 info are missing. Please set the values in INDI Control Panel.", errMsg),
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

    guideFrame->disconnect(this);

    // Must reset dec swap before we run any calibration procedure!
    emit DESwapChanged(false);
    pmath->setDeclinationSwapEnabled(false);
    pmath->setLostStar(false);

    calibrationStage = CAL_START;

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

void InternalGuider::setGuideView(FITSView *guideView)
{
    guideFrame = guideView;

    pmath->setGuideView(guideFrame);
}

void InternalGuider::reset()
{
    state = GUIDE_IDLE;
    //calibrationStage = CAL_IDLE;
    connect(guideFrame, SIGNAL(trackingStarSelected(int,int)), this, SLOT(trackingStarSelected(int,int)),
            Qt::UniqueConnection);
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
        m_CalibrationParams.ra_total_pulse = m_CalibrationParams.de_total_pulse = 0;

        emit newLog(i18n("RA drifting forward..."));

        pmath->getReticleParameters(&m_CalibrationCoords.start_x1, &m_CalibrationCoords.start_y1, nullptr);

        m_CalibrationParams.last_pulse = Options::calibrationPulseDuration();

        qCDebug(KSTARS_EKOS_GUIDE) << "Auto Iteration #" << m_CalibrationParams.auto_drift_time << "Default pulse:" << m_CalibrationParams.last_pulse;
        qCDebug(KSTARS_EKOS_GUIDE) << "Start X1 " << m_CalibrationCoords.start_x1 << " Start Y1 " << m_CalibrationCoords.start_y1;

        axis_calibration_complete = false;

        m_CalibrationCoords.last_x = m_CalibrationCoords.start_x1;
        m_CalibrationCoords.last_y = m_CalibrationCoords.start_x2;

        emit newPulse(RA_INC_DIR, m_CalibrationParams.last_pulse);

        m_CalibrationParams.ra_iterations++;

        calibrationStage = CAL_RA_INC;

        break;

    case CAL_RA_INC:
    {
        // Star position resulting from LAST guiding pulse to mount
        double cur_x, cur_y;
        pmath->getStarScreenPosition(&cur_x, &cur_y);

        qCDebug(KSTARS_EKOS_GUIDE) << "Iteration #" << m_CalibrationParams.ra_iterations << ": STAR " << cur_x << "," << cur_y;
        qCDebug(KSTARS_EKOS_GUIDE) << "Iteration " << m_CalibrationParams.ra_iterations << " Direction: RA_INC_DIR" << " Duration: " << m_CalibrationParams.last_pulse << " ms.";

        // Must pass at least 1.5 pixels to move on to the next stage
        if (m_CalibrationParams.ra_iterations >= m_CalibrationParams.auto_drift_time && (fabs(cur_x-m_CalibrationCoords.start_x1) > 1.5 || fabs(cur_y-m_CalibrationCoords.start_y1) > 1.5))
        {
            m_CalibrationParams.ra_total_pulse += m_CalibrationParams.last_pulse;

            calibrationStage = CAL_RA_DEC;

            m_CalibrationCoords.end_x1 = cur_x;
            m_CalibrationCoords.end_y1 = cur_y;

            m_CalibrationCoords.last_x = cur_x;
            m_CalibrationCoords.last_y = cur_y;

            qCDebug(KSTARS_EKOS_GUIDE) << "End X1 " << m_CalibrationCoords.end_x1 << " End Y1 " << m_CalibrationCoords.end_y1;

            m_CalibrationParams.phi   = pmath->calculatePhi(m_CalibrationCoords.start_x1, m_CalibrationCoords.start_y1, m_CalibrationCoords.end_x1, m_CalibrationCoords.end_y1);
            ROT_Z = RotateZ(-M_PI * m_CalibrationParams.phi / 180.0); // derotates...

            m_CalibrationCoords.ra_distance = 0;

            emit newPulse(RA_DEC_DIR, m_CalibrationParams.last_pulse);
            m_CalibrationParams.ra_iterations++;

            emit newLog(i18n("RA drifting reverse..."));
        }
        else if (m_CalibrationParams.ra_iterations > m_CalibrationParams.turn_back_time)
        {
            emit newLog(i18n("Calibration rejected. Star drift is too short. Check for mount, cable, or backlash problems."));

            calibrationStage = CAL_ERROR;

            emit newStatus(Ekos::GUIDE_CALIBRATION_ERROR);

            KSNotification::event(QLatin1String("CalibrationFailed"), i18n("Guiding calibration failed with errors"), KSNotification::EVENT_ALERT);

            reset();
        }
        else
        {
            // Aggressive pulse in case we're going slow
            if (fabs(cur_x-m_CalibrationCoords.last_x) < 0.5 && fabs(cur_y-m_CalibrationCoords.last_y) < 0.5)
            {
                // 200%
                m_CalibrationParams.last_pulse = Options::calibrationPulseDuration() *2;
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

        qCDebug(KSTARS_EKOS_GUIDE) << "Iteration #" << m_CalibrationParams.ra_iterations << ": STAR " << cur_x << "," << cur_y;
        qCDebug(KSTARS_EKOS_GUIDE) << "Iteration " << m_CalibrationParams.ra_iterations << " Direction: RA_DEC_DIR" << " Duration: " << m_CalibrationParams.last_pulse << " ms.";

        Vector star_pos = Vector(cur_x, cur_y, 0) - Vector(m_CalibrationCoords.start_x1, m_CalibrationCoords.start_y1, 0);
        star_pos.y      = -star_pos.y;
        star_pos        = star_pos * ROT_Z;

        qCDebug(KSTARS_EKOS_GUIDE) << "Star x pos is " << star_pos.x << " from original point.";

        if (m_CalibrationCoords.ra_distance == 0.0)
            m_CalibrationCoords.ra_distance = star_pos.x;

        // start point reached... so exit
        if (star_pos.x < 1.5)
        {
            pmath->performProcessing();

            m_CalibrationParams.ra_total_pulse += m_CalibrationParams.last_pulse;
            m_CalibrationParams.last_pulse = Options::calibrationPulseDuration();
            axis_calibration_complete = true;
        }
        // If we'not moving much, try increasing pulse to 200% to clear any backlash
        // Also increase pulse width if we are going FARTHER and not back to our original position
        else if ( (fabs(cur_x-m_CalibrationCoords.last_x) < 0.5 && fabs(cur_y-m_CalibrationCoords.last_y) < 0.5) || star_pos.x > m_CalibrationCoords.ra_distance)
        {
            // Increase pulse by 200%
            m_CalibrationParams.last_pulse = Options::calibrationPulseDuration()*2;
        }
        else
        {
            m_CalibrationParams.ra_total_pulse += m_CalibrationParams.last_pulse;
            m_CalibrationParams.last_pulse = Options::calibrationPulseDuration();
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

            emit newLog(i18np("Guide RA: Scope cannot reach the start point after %1 iteration. Possible mount or "
                              "backlash problems...",
                              "GUIDE_RA: Scope cannot reach the start point after %1 iterations. Possible mount or "
                              "backlash problems...",
                              m_CalibrationParams.ra_iterations));

            KSNotification::event(QLatin1String("CalibrationFailed"), i18n("Guiding calibration failed with errors"), KSNotification::EVENT_ALERT);
            reset();
            break;
        }

        if (ra_only == false)
        {
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
        // calc orientation
        if (pmath->calculateAndSetReticle1D(m_CalibrationCoords.start_x1, m_CalibrationCoords.start_y1, m_CalibrationCoords.end_x1, m_CalibrationCoords.end_y1, m_CalibrationParams.ra_total_pulse))
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

            KSNotification::event(QLatin1String("CalibrationFailed"),
                                 i18n("Guiding calibration failed with errors"), KSNotification::EVENT_ALERT);
        }

        reset();
        break;
    }

    case CAL_DEC_INC:
    {
        // Star position resulting from LAST guiding pulse to mount
        double cur_x, cur_y;
        pmath->getStarScreenPosition(&cur_x, &cur_y);

        qCDebug(KSTARS_EKOS_GUIDE) << "Iteration #" << m_CalibrationParams.dec_iterations << ": STAR " << cur_x << "," << cur_y;
        qCDebug(KSTARS_EKOS_GUIDE) << "Iteration " << m_CalibrationParams.dec_iterations << " Direction: DEC_INC_DIR" << " Duration: " << m_CalibrationParams.last_pulse << " ms.";

        if (m_CalibrationParams.dec_iterations >= m_CalibrationParams.auto_drift_time && (fabs(cur_x-m_CalibrationCoords.start_x2) > 1.5 || fabs(cur_y-m_CalibrationCoords.start_y2) > 1.5))
        {
            calibrationStage = CAL_DEC_DEC;

            m_CalibrationParams.de_total_pulse += m_CalibrationParams.last_pulse;

            m_CalibrationCoords.end_x2 = cur_x;
            m_CalibrationCoords.end_y2 = cur_y;

            m_CalibrationCoords.last_x = cur_x;
            m_CalibrationCoords.last_y = cur_y;

            axis_calibration_complete = false;

            qCDebug(KSTARS_EKOS_GUIDE) << "End X2 " << m_CalibrationCoords.end_x2 << " End Y2 " << m_CalibrationCoords.end_y2;

            m_CalibrationParams.phi   = pmath->calculatePhi(m_CalibrationCoords.start_x2, m_CalibrationCoords.start_y2, m_CalibrationCoords.end_x2, m_CalibrationCoords.end_y2);
            ROT_Z = RotateZ(-M_PI * m_CalibrationParams.phi / 180.0); // derotates...

            m_CalibrationCoords.de_distance = 0;

            emit newPulse(DEC_DEC_DIR, m_CalibrationParams.last_pulse);
            emit newLog(i18n("DEC drifting reverse..."));
            m_CalibrationParams.dec_iterations++;
        }
        else if (m_CalibrationParams.dec_iterations > m_CalibrationParams.turn_back_time)
        {
            calibrationStage = CAL_ERROR;

            emit newStatus(Ekos::GUIDE_CALIBRATION_ERROR);

            emit newLog(i18np("Guide DEC: Scope cannot reach the start point after %1 iteration.\nPossible mount "
                              "or backlash problems...",
                              "GUIDE DEC: Scope cannot reach the start point after %1 iterations.\nPossible mount "
                              "or backlash problems...",
                              m_CalibrationParams.dec_iterations));

            KSNotification::event(QLatin1String("CalibrationFailed"),
                                 i18n("Guiding calibration failed with errors"), KSNotification::EVENT_ALERT);
            reset();
        }
        else
        {
            if (fabs(cur_x-m_CalibrationCoords.last_x) < 0.5 && fabs(cur_y-m_CalibrationCoords.last_y) < 0.5)
            {
                // Increase pulse by 200%
                m_CalibrationParams.last_pulse = Options::calibrationPulseDuration()*2;
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

        // Star position resulting from LAST guiding pulse to mount
        qCDebug(KSTARS_EKOS_GUIDE) << "Iteration #" << m_CalibrationParams.dec_iterations << ": STAR " << cur_x << "," << cur_y;
        qCDebug(KSTARS_EKOS_GUIDE) << "Iteration " << m_CalibrationParams.dec_iterations << " Direction: DEC_DEC_DIR" << " Duration: " << m_CalibrationParams.last_pulse << " ms.";

        Vector star_pos = Vector(cur_x, cur_y, 0) - Vector(m_CalibrationCoords.start_x2, m_CalibrationCoords.start_y2, 0);
        star_pos.y      = -star_pos.y;
        star_pos        = star_pos * ROT_Z;

        qCDebug(KSTARS_EKOS_GUIDE) << "start Pos X " << star_pos.x << " from original point.";

        // Keep track of distance
        if (m_CalibrationCoords.de_distance == 0.0)
            m_CalibrationCoords.de_distance = star_pos.x;

        // start point reached... so exit
        if (star_pos.x < 1.5)
        {
            pmath->performProcessing();

            m_CalibrationParams.de_total_pulse += m_CalibrationParams.last_pulse;
            m_CalibrationParams.last_pulse = Options::calibrationPulseDuration();
            axis_calibration_complete = true;
        }
        // Increase pulse if we're not moving much or if we are moving _away_ from target.
        else if ( (fabs(cur_x-m_CalibrationCoords.last_x) < 0.5 && fabs(cur_y-m_CalibrationCoords.last_y) < 0.5) || star_pos.x > m_CalibrationCoords.de_distance)
        {
            // Increase pulse by 200%
            m_CalibrationParams.last_pulse = Options::calibrationPulseDuration()*2;
        }
        else
        {
            m_CalibrationParams.de_total_pulse += m_CalibrationParams.last_pulse;
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

            emit newLog(i18np("Guide DEC: Scope cannot reach the start point after %1 iteration.\nPossible mount "
                              "or backlash problems...",
                              "Guide DEC: Scope cannot reach the start point after %1 iterations.\nPossible mount "
                              "or backlash problems...",
                              m_CalibrationParams.dec_iterations));

            KSNotification::event(QLatin1String("CalibrationFailed"),
                                 i18n("Guiding calibration failed with errors"),KSNotification::EVENT_ALERT);
            reset();
            break;
        }

        bool swap_dec = false;
        // calc orientation
        if (pmath->calculateAndSetReticle2D(m_CalibrationCoords.start_x1, m_CalibrationCoords.start_y1, m_CalibrationCoords.end_x1, m_CalibrationCoords.end_y1, m_CalibrationCoords.start_x2, m_CalibrationCoords.start_y2, m_CalibrationCoords.end_x2, m_CalibrationCoords.end_y2,
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

            KSNotification::event(QLatin1String("CalibrationSuccessful"),
                                 i18n("Guiding calibration completed successfully"));

            //if (ui.autoStarCheck->isChecked())
            //guideModule->selectAutoStar();
        }
        else
        {
            emit newLog(i18n("Calibration rejected. Star drift is too short. Check for mount, cable, or backlash problems."));

            emit newStatus(Ekos::GUIDE_CALIBRATION_ERROR);

            //ui.startCalibrationLED->setColor(alertColor);
            calibrationStage = CAL_ERROR;
            KSNotification::event(QLatin1String("CalibrationFailed"),
                                 i18n("Guiding calibration failed with errors"),KSNotification::EVENT_ALERT);
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
    pmath->setReticleParameters(starCenter.x(), starCenter.y(), -1);
}

void InternalGuider::trackingStarSelected(int x, int y)
{
    if (calibrationStage == CAL_IDLE)
        return;

    //int square_size = guide_squares[pmath->getSquareIndex()].size;

    pmath->setReticleParameters(x, y, -1);
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
    pmath->setDeclinationSwapEnabled(enable);
}

void InternalGuider::setSquareAlgorithm(int index)
{
    pmath->setSquareAlgorithm(index);
}

void InternalGuider::setReticleParameters(double x, double y, double angle)
{
    pmath->setReticleParameters(x, y, angle);
}

bool InternalGuider::getReticleParameters(double *x, double *y, double *angle)
{
    return pmath->getReticleParameters(x, y, angle);
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
            pmath->setReticleParameters(star_pos.x, star_pos.y, -1);
        }
        m_isFirstFrame = false;
    }

    // calc math. it tracks square
    pmath->performProcessing();

    if (pmath->isStarLost())
        m_starLostCounter++;
    else
        m_starLostCounter=0;

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

    double delta_rms = sqrt(out->delta[GUIDE_RA]*out->delta[GUIDE_RA] + out->delta[GUIDE_DEC]*out->delta[GUIDE_DEC]);
    if (delta_rms > Options::guideMaxDeltaRMS())
    {
        // Stop sending pulses on the 3rd time the delta RMS is high
        // so that we don't stray too far off the main target.
        if (state == GUIDE_GUIDING && m_highRMSCounter > 2)
            sendPulses = false;
        m_highRMSCounter++;
    }
    else
        m_highRMSCounter=0;

    uint8_t abortStarLostThreshold = (state == GUIDE_DITHERING) ? MAX_LOST_STAR_THRESHOLD * 3 : MAX_LOST_STAR_THRESHOLD;
    uint8_t abortRMSThreshold = (state == GUIDE_DITHERING) ? MAX_RMS_THRESHOLD * 3 : MAX_RMS_THRESHOLD;
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
        emit newPulse(out->pulse_dir[GUIDE_RA] , out->pulse_length[GUIDE_RA],
                      out->pulse_dir[GUIDE_DEC], out->pulse_length[GUIDE_DEC]);

        // Wait until pulse is over before capturing an image
        const int waitMS = qMax(out->pulse_length[GUIDE_RA], out->pulse_length[GUIDE_DEC]);
        // If less than MAX_IMMEDIATE_CAPTURE ms, then capture immediately
        if (waitMS > MAX_IMMEDIATE_CAPTURE)
            // Issue frame requests MAX_IMMEDIATE_CAPTURE ms before timeout to account for
            // propogation delays
            QTimer::singleShot(waitMS - PROPAGATION_DELAY, [&]()
            {
                emit frameCaptureRequested();
            });
        else
            emit frameCaptureRequested();
    }
    else
        emit frameCaptureRequested();

    if (state == GUIDE_DITHERING)
        return true;

    tick = pmath->getTicks();

    if (tick & 1)
    {
        // draw some params in window
        emit newAxisDelta(out->delta[GUIDE_RA], out->delta[GUIDE_DEC]);

        double raPulse = out->pulse_length[GUIDE_RA];
        double dePulse = out->pulse_length[GUIDE_DEC];

        if(out->pulse_dir[GUIDE_RA]==NO_DIR)  //If the pulse was not sent to the mount, it should have 0 value
            raPulse = 0;
        if(out->pulse_dir[GUIDE_DEC]==NO_DIR) //If the pulse was not sent to the mount, it should have 0 value
            dePulse = 0;
        if(out->pulse_dir[GUIDE_RA]==RA_INC_DIR)  //If the pulse was in the Negative direction, it should have a negative sign.
            raPulse = -raPulse;  
        if(out->pulse_dir[GUIDE_DEC]==DEC_INC_DIR) //If the pulse was in the Negative direction, it should have a negative sign.
            dePulse = -dePulse;

        emit newAxisPulse(raPulse, dePulse);

        emit newAxisSigma(out->sigma[GUIDE_RA], out->sigma[GUIDE_DEC]);
    }

    // skip half frames
    //if( half_refresh_rate && (tick & 1) )
    //return;

    //drift_graph->on_paint();
    //pDriftOut->update();

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

    if (state == GUIDE_DITHERING)
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

bool InternalGuider::selectAutoStar()
{
        FITSData *imageData = guideFrame->getImageData();

        if (imageData == nullptr)
            return false;

        bool useNativeDetection = false;

        QList<Edge *> starCenters;

        if (Options::guideAlgorithm() != SEP_THRESHOLD)
            starCenters = pmath->PSFAutoFind();

        if (starCenters.empty())
        {
            if (Options::guideAlgorithm() == SEP_THRESHOLD)
                imageData->findStars(ALGORITHM_SEP);
            else
                imageData->findStars();

            starCenters = imageData->getStarCenters();
            if (starCenters.empty())
                return false;

            useNativeDetection = true;
            // For SEP, prefer flux total
            if (Options::guideAlgorithm() == SEP_THRESHOLD)
                qSort(starCenters.begin(), starCenters.end(), [](const Edge *a, const Edge *b) { return a->val > b->val; });
            else
                qSort(starCenters.begin(), starCenters.end(), [](const Edge *a, const Edge *b) { return a->width > b->width; });

            guideFrame->setStarsEnabled(true);
            guideFrame->updateFrame();
        }

        int maxX = imageData->getWidth();
        int maxY = imageData->getHeight();

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
        m_highRMSCounter=m_starLostCounter=0;
        m_isFirstFrame = true;
        pmath->reset();
        // If we were in the process of dithering, wait until settle and resume
        if (rememberState == GUIDE_DITHERING)
        {
            if (Options::ditherSettle() > 0)
            {
                state = GUIDE_DITHERING_SETTLE;
                emit newStatus(state);
            }

            QTimer::singleShot(Options::ditherSettle()* 1000, this, SLOT(setDitherSettled()));
        }
        else
        {
            state = GUIDE_GUIDING;
            emit newStatus(state);
        }

    }
    else if (reacquireTimer.elapsed() > static_cast<int>(Options::guideLostStarTimeout()*1000))
    {
        emit newLog(i18n("Failed to find any suitable guide stars. Aborting..."));
        abort();
        return false;
    }

    emit frameCaptureRequested();
    return rc;
}

}
