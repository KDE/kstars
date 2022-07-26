/*  Ekos state machine for a single capture job sequence
    SPDX-FileCopyrightText: Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "sequencejobstate.h"

#include "Options.h"
#include "kstarsdata.h"
#include "indicom.h"

namespace Ekos
{
SequenceJobState::SequenceJobState(const QSharedPointer<SequenceJobState::CaptureState> &sharedState)
{
    m_CaptureState = sharedState;
}

void SequenceJobState::setFrameType(CCDFrameType frameType)
{
    // set the frame type
    m_frameType = frameType;
    // reset the preparation state
    m_PreparationState = PREP_NONE;
}

void SequenceJobState::prepareLightFrameCapture(bool enforceCCDTemp, bool enforceInitialGuidingDrift, bool isPreview)
{
    // precondition: do not start while already being busy and conditions haven't changed
    if (m_status == JOB_BUSY && enforceCCDTemp == m_enforceTemperature && enforceInitialGuidingDrift == m_enforceInitialGuiding)
        return;

    m_status    = JOB_BUSY;
    m_isPreview = isPreview;

    // Reset all prepare actions
    setAllActionsReady();

    // disable batch mode for previews
    emit setCCDBatchMode(!isPreview);

    // Filter changes are actually done in capture(), therefore prepareActions are always true
    prepareActions[ACTION_FILTER] = true;
    // nevertheless, emit an event so that Capture changes m_state
    if (targetFilterID != -1 && targetFilterID != m_CaptureState->currentFilterID)
        emit prepareState(CAPTURE_CHANGING_FILTER);


    // Check if we need to update temperature (only skip if the value is initialized and within the limits)
    prepareTemperatureCheck(enforceCCDTemp);

    // Check if we need to update rotator (only skip if the value is initialized and within the limits)
    prepareRotatorCheck();

    // Check if we need to wait for guiding being initially below the target value
    m_enforceInitialGuiding = enforceInitialGuidingDrift;
    if (enforceInitialGuidingDrift && !isPreview)
        prepareActions[ACTION_GUIDER_DRIFT] = false;

    // Hint: Filter changes are actually done in SequenceJob::capture();

    // preparation started
    m_PreparationState = PREP_BUSY;
    // check if the preparations are already completed
    checkAllActionsReady();
}

void SequenceJobState::prepareFlatFrameCapture(bool enforceCCDTemp, bool isPreview)
{
    // precondition: do not start while already being busy and conditions haven't changed
    if (m_status == JOB_BUSY && enforceCCDTemp == m_enforceTemperature)
        return;

    m_status    = JOB_BUSY;
    m_isPreview = isPreview;

    // Reset all prepare actions
    setAllActionsReady();

    // disable batch mode for previews
    emit setCCDBatchMode(!isPreview);

    // Filter changes are actually done in capture(), therefore prepareActions are always true
    prepareActions[ACTION_FILTER] = true;
    // nevertheless, emit an event so that Capture changes m_state
    if (targetFilterID != -1 && targetFilterID != m_CaptureState->currentFilterID)
        emit prepareState(CAPTURE_CHANGING_FILTER);

    // Check if we need to update temperature (only skip if the value is initialized and within the limits)
    prepareTemperatureCheck(enforceCCDTemp);

    // preparation started
    m_PreparationState = PREP_BUSY;
    // check if the preparations are already completed
    checkAllActionsReady();
}

void SequenceJobState::prepareDarkFrameCapture(bool enforceCCDTemp, bool isPreview)
{
    // precondition: do not start while already being busy and conditions haven't changed
    if (m_status == JOB_BUSY && enforceCCDTemp == m_enforceTemperature)
        return;

    m_status    = JOB_BUSY;
    m_isPreview = isPreview;

    // Reset all prepare actions
    setAllActionsReady();

    // disable batch mode for previews
    emit setCCDBatchMode(!isPreview);

    // Filter changes are actually done in capture(), therefore prepareActions are always true
    prepareActions[ACTION_FILTER] = true;

    // Check if we need to update temperature (only skip if the value is initialized and within the limits)
    prepareTemperatureCheck(enforceCCDTemp);

    // preparation started
    m_PreparationState = PREP_BUSY;
    // check if the preparations are already completed
    checkAllActionsReady();
}

void SequenceJobState::prepareBiasFrameCapture(bool enforceCCDTemp, bool isPreview)
{
    prepareDarkFrameCapture(enforceCCDTemp, isPreview);
}

bool SequenceJobState::areActionsReady()
{
    for (bool &ready : prepareActions.values())
    {
        if (ready == false)
            return false;
    }

    return true;
}

void SequenceJobState::checkAllActionsReady()
{
    // do nothing if preparation is not running
    if (m_PreparationState != PREP_BUSY)
        return;

    switch (m_frameType)
    {
        case FRAME_LIGHT:
            if (areActionsReady())
            {
                // as last step ensure that the scope is uncovered
                if (checkLightFrameScopeCoverOpen() != IPS_OK)
                    return;

                m_PreparationState = PREP_COMPLETED;
                emit prepareComplete();
            }
            break;
        case FRAME_FLAT:
            if (!areActionsReady())
                return;

            // 1. Check if the selected flats light source is ready
            if (checkFlatsLightSourceReady() != IPS_OK)
                return;

            // 2. Light source ready, now check if we need to perform mount prepark
            if (checkPreMountParkReady() != IPS_OK)
                return;

            // 3. Check if we need to perform dome prepark
            if (checkPreDomeParkReady() != IPS_OK)
                return;

            // 4. If we used AUTOFOCUS before for a specific frame (e.g. Lum)
            //    then the absolute focus position for Lum is recorded in the filter manager
            //    when we take flats again, we always go back to the same focus position as the light frames to ensure
            //    near identical focus for both frames.
            if (checkFlatSyncFocus() != IPS_OK)
                return;

            // all preparations ready, avoid doubled events
            if (m_PreparationState == PREP_BUSY)
            {
                m_PreparationState = PREP_COMPLETED;
                emit prepareComplete();
            }
            break;
        case FRAME_DARK:
            if (!areActionsReady())
                return;

            // 1. check if the CCD has a shutter
            if (checkHasShutter() != IPS_OK)
                return;
            switch (flatFieldSource)
            {
                // All these are manual when it comes to dark frames
                case SOURCE_MANUAL:
                case SOURCE_DAWN_DUSK:
                    // For cameras without a shutter, we need to ask the user to cover the telescope
                    // if the telescope is not already covered.
                    if (checkManualCover() != IPS_OK)
                        return;
                    break;
                case SOURCE_FLATCAP:
                case SOURCE_DARKCAP:
                    if (checkDustCapReady(FRAME_DARK) != IPS_OK)
                        return;
                    break;

                case SOURCE_WALL:
                    if (checkWallPositionReady(FRAME_DARK) != IPS_OK)
                        return;
                    break;
            }

            // avoid doubled events
            if (m_PreparationState == PREP_BUSY)
            {
                m_PreparationState = PREP_COMPLETED;
                emit prepareComplete();
            }
            break;
        case FRAME_BIAS:
            if (areActionsReady())
            {
                // avoid doubled events
                if (m_PreparationState == PREP_BUSY)
                {
                    m_PreparationState = PREP_COMPLETED;
                    emit prepareComplete();
                }
            }
            break;
        default:
            // all other cases not refactored yet, preparation immediately completed
            emit prepareComplete();
            break;
    }
}

void SequenceJobState::setAllActionsReady()
{
    QMutableMapIterator<PrepareActions, bool> it(prepareActions);

    while (it.hasNext())
    {
        it.next();
        it.setValue(true);
    }
    // reset the initialization state
    for (PrepareActions action :
            {
                ACTION_FILTER, ACTION_ROTATOR, ACTION_TEMPERATURE, ACTION_GUIDER_DRIFT
            })
        setInitialized(action, false);
}

void SequenceJobState::prepareTemperatureCheck(bool enforceCCDTemp)
{
    // turn on CCD temperature enforcing if required
    m_enforceTemperature = enforceCCDTemp;

    if (m_enforceTemperature)
    {
        prepareActions[ACTION_TEMPERATURE] = false;
        if (isInitialized(ACTION_TEMPERATURE))
        {
            // ignore the next value since after setting temperature the next received value will be
            // exactly this value no matter what the CCD temperature
            ignoreNextValue[ACTION_TEMPERATURE] = true;
            // request setting temperature
            emit setCCDTemperature(targetTemperature);
            emit prepareState(CAPTURE_SETTING_TEMPERATURE);
        }
        // trigger setting current value first if not initialized
        else
            emit readCurrentState(CAPTURE_SETTING_TEMPERATURE);

    }
}

void SequenceJobState::prepareRotatorCheck()
{
    if (targetPositionAngle > Ekos::INVALID_VALUE)
    {
        if (isInitialized(ACTION_ROTATOR) == false)
        {
            prepareActions[ACTION_ROTATOR] = false;
            // PA = RawAngle * Multiplier + Offset
            double rawAngle = (targetPositionAngle - Options::pAOffset()) / Options::pAMultiplier();
            emit prepareState(CAPTURE_SETTING_ROTATOR);
            emit setRotatorAngle(&rawAngle);
        }
        // trigger setting current value first if not initialized
        else
            emit readCurrentState(CAPTURE_SETTING_ROTATOR);
    }
}

IPState SequenceJobState::checkFlatsLightSourceReady()
{
    IPState result = IPS_OK;

    switch (flatFieldSource)
    {
        case SOURCE_MANUAL:
            result = checkManualFlatsCoverReady();
            break;
        case SOURCE_DAWN_DUSK:
            // Not implemented.
            result = IPS_ALERT;
            break;
        case SOURCE_FLATCAP:
            result = checkFlatCapReady();
            break;
        case SOURCE_WALL:
            result = checkWallPositionReady(FRAME_FLAT);
            break;
        case SOURCE_DARKCAP:
            result = checkDustCapReady(FRAME_FLAT);
            break;
    }
    return result;
}

IPState SequenceJobState::checkManualFlatsCoverReady()
{
    // Manual mode we need to cover mount with evenly illuminated field.
    if (m_CaptureState->telescopeCovered == false)
    {
        if (coverQueryState == CAL_CHECK_CONFIRMATION)
            return IPS_BUSY;

        // request asking the user to cover the scope manually with a light source
        emit askManualScopeLightCover(i18n("Cover telescope with an evenly illuminated light source."),
                                      i18n("Flat Frame"));
        coverQueryState = CAL_CHECK_CONFIRMATION;

        return IPS_BUSY;
    }
    return IPS_OK;
}

IPState SequenceJobState::checkFlatCapReady()
{
    // flat light is on
    if (lightBoxLightStatus == CAP_LIGHT_ON || dustCapLightStatus == CAP_LIGHT_ON)
        return IPS_OK;
    // turning on flat light running
    if (lightBoxLightStatus == CAP_LIGHT_BUSY || dustCapStatus == CAP_PARKING
            || dustCapLightStatus == CAP_LIGHT_BUSY)
        return IPS_BUSY;
    // error occured
    if (dustCapStatus == CAP_ERROR)
        return IPS_ALERT;

    if (m_CaptureState->hasLightBox == true && lightBoxLightStatus != CAP_LIGHT_ON)
    {
        lightBoxLightStatus = CAP_LIGHT_BUSY;
        emit setLightBoxLight(true);
        emit newLog(i18n("Turn light box light on..."));
        return IPS_BUSY;
    }

    if (m_CaptureState->hasDustCap == false)
    {
        if (m_CaptureState->hasLightBox == false)
            emit newLog("Skipping flat/dark cap since it is not connected.");
        return IPS_OK;
    }

    // if using the dust cap, first park the dust cap
    if (dustCapStatus != CAP_PARKED)
    {
        dustCapStatus = CAP_PARKING;
        emit parkDustCap(true);
        emit newLog(i18n("Parking dust cap..."));
        return IPS_BUSY;
    }
    // if the dust cap is parked, turn the light on
    else if (dustCapLightStatus != CAP_LIGHT_ON)
    {
        dustCapLightStatus = CAP_LIGHT_BUSY;
        emit setDustCapLight(true);
        emit newLog(i18n("Turn dust cap light on..."));
    }
    // nothing more to do
    return IPS_OK;
}

IPState SequenceJobState::checkDustCapReady(CCDFrameType frameType)
{
    // turning on flat light running
    if (lightBoxLightStatus == CAP_LIGHT_BUSY  || dustCapLightStatus == CAP_LIGHT_BUSY ||
            dustCapStatus == CAP_PARKING || dustCapStatus == CAP_UNPARKING)
        return IPS_BUSY;
    // error occured
    if (dustCapStatus == CAP_ERROR)
        return IPS_ALERT;

    bool captureFlats = (frameType == FRAME_FLAT);

    LightStatus targetLightBoxStatus = (frameType == FRAME_FLAT) ? CAP_LIGHT_ON : CAP_LIGHT_OFF;

    if (m_CaptureState->hasLightBox == true && lightBoxLightStatus != targetLightBoxStatus)
    {
        lightBoxLightStatus = CAP_LIGHT_BUSY;
        emit setLightBoxLight(captureFlats);
        emit newLog(captureFlats ? i18n("Turn light box light on...") : i18n("Turn light box light off..."));
        return IPS_BUSY;
    }

    if (m_CaptureState->hasDustCap == false)
    {
        // Only warn if we don't have already light box.
        if (m_CaptureState->hasLightBox == false)
            emit newLog("Skipping flat/dark cap since it is not connected.");
        return IPS_OK;
    }

    // for flats open the cap and close it otherwise
    CapState targetCapState = captureFlats ? CAP_IDLE : CAP_PARKED;
    // If cap is parked, unpark it since dark cap uses external light source.
    if (dustCapStatus != targetCapState)
    {
        dustCapStatus = captureFlats ? CAP_UNPARKING : CAP_PARKING;
        emit parkDustCap(!captureFlats);
        emit newLog(captureFlats ? i18n("Unparking dust cap...") : i18n("Parking dust cap..."));
        return IPS_BUSY;
    }

    // turn the light on for flats and off otherwise
    LightStatus targetLightStatus = (frameType == FRAME_FLAT) ? CAP_LIGHT_ON : CAP_LIGHT_OFF;
    if (dustCapLightStatus != targetLightStatus)
    {
        dustCapLightStatus = CAP_LIGHT_BUSY;
        emit setDustCapLight(captureFlats);
        emit newLog(captureFlats ? i18n("Turn dust cap light on...") : i18n("Turn dust cap light off..."));
        return IPS_BUSY;
    }
    // nothing more to do
    return IPS_OK;
}

IPState SequenceJobState::checkWallPositionReady(CCDFrameType frametype)
{
    if (m_CaptureState->hasTelescope)
    {
        if (wpScopeStatus < WP_SLEWING)
        {
            wallCoord.HorizontalToEquatorial(KStarsData::Instance()->lst(),
                                             KStarsData::Instance()->geo()->lat());
            wpScopeStatus = WP_SLEWING;
            emit slewTelescope(wallCoord);
            emit newLog(i18n("Mount slewing to wall position..."));
            return IPS_BUSY;
        }
        // wait until actions completed
        else if (wpScopeStatus == WP_SLEWING || wpScopeStatus == WP_TRACKING_BUSY)
            return IPS_BUSY;
        // Check if slewing is complete
        else if (wpScopeStatus == WP_SLEW_COMPLETED)
        {
            wpScopeStatus = WP_TRACKING_BUSY;
            emit setScopeTracking(false);
            emit newLog(i18n("Slew to wall position complete, stop tracking."));
            return IPS_BUSY;
        }
        else if (wpScopeStatus == WP_TRACKING_OFF)
            emit newLog(i18n("Slew to wall position complete, stop tracking."));

        // wall position reached, check if we have a light box to turn on for flats and off otherwise
        bool captureFlats = (frametype == FRAME_FLAT);
        LightStatus targetLightState = (captureFlats ? CAP_LIGHT_ON : CAP_LIGHT_OFF);

        if (m_CaptureState->hasLightBox == true)
        {
            if (lightBoxLightStatus != targetLightState)
            {
                lightBoxLightStatus = CAP_LIGHT_BUSY;
                emit setLightBoxLight(captureFlats);
                emit newLog(captureFlats ? i18n("Turn light box light on...") : i18n("Turn light box light off..."));
                return IPS_BUSY;
            }
        }
    }
    // everything ready
    return IPS_OK;
}

IPState SequenceJobState::checkPreMountParkReady()
{
    if (preMountPark && m_CaptureState->hasTelescope && flatFieldSource != SOURCE_WALL)
    {
        if (scopeParkStatus == ISD::PARK_ERROR)
        {
            emit newLog(i18n("Parking mount failed, aborting..."));
            emit abortCapture();
            return IPS_ALERT;
        }
        else if (scopeParkStatus == ISD::PARK_PARKING)
            return IPS_BUSY;
        else if (scopeParkStatus != ISD::PARK_PARKED)
        {
            scopeParkStatus = ISD::PARK_PARKING;
            emit setScopeParked(true);
            emit newLog(i18n("Parking mount prior to calibration frames capture..."));
            return IPS_BUSY;
        }
    }
    // everything ready
    return IPS_OK;
}

IPState SequenceJobState::checkPreDomeParkReady()
{
    if (preDomePark && m_CaptureState->hasDome)
    {
        if (domeStatus == ISD::Dome::DOME_ERROR)
        {
            emit newLog(i18n("Parking dome failed, aborting..."));
            emit abortCapture();
            return IPS_ALERT;
        }
        else if (domeStatus == ISD::Dome::DOME_PARKING)
            return IPS_BUSY;
        else if (domeStatus != ISD::Dome::DOME_PARKED)
        {
            domeStatus = ISD::Dome::DOME_PARKING;
            emit setDomeParked(true);
            emit newLog(i18n("Parking dome prior to calibration frames capture..."));
            return IPS_BUSY;
        }
    }
    // everything ready
    return IPS_OK;
}

IPState SequenceJobState::checkFlatSyncFocus()
{
    // check already running?
    if (flatSyncStatus == FS_BUSY)
    {
        QTimer::singleShot(1000, [&]
        {
            // wait for one second and repeat the request again
            emit flatSyncFocus(targetFilterID);
        });
        return IPS_BUSY;
    }

    if (m_frameType == FRAME_FLAT && autoFocusReady && Options::flatSyncFocus() &&
            flatSyncStatus != FS_COMPLETED)
    {
        flatSyncStatus = FS_BUSY;
        emit flatSyncFocus(targetFilterID);
        return IPS_BUSY;
    }
    // everything ready
    return IPS_OK;
}

IPState SequenceJobState::checkHasShutter()
{
    if (m_CaptureState->shutterStatus == SHUTTER_BUSY)
        return IPS_BUSY;
    if (m_CaptureState->shutterStatus != SHUTTER_UNKNOWN)
        return IPS_OK;
    // query the status
    m_CaptureState->shutterStatus = SHUTTER_BUSY;
    emit queryHasShutter();
    return IPS_BUSY;
}

IPState SequenceJobState::checkManualCover()
{
    if (m_CaptureState->shutterStatus == SHUTTER_NO && m_CaptureState->telescopeCovered == false)
    {
        // Already asked for confirmation? Then wait.
        if (coverQueryState == CAL_CHECK_CONFIRMATION)
            return IPS_BUSY;

        // Otherwise, we ask user to confirm manually
        coverQueryState = CAL_CHECK_CONFIRMATION;

        emit askManualScopeLightCover(i18n("Cover the telescope in order to take a dark exposure."),
                                      i18n("Dark Exposure"));
        return IPS_BUSY;
    }
    // everything ready
    return IPS_OK;
}

IPState SequenceJobState::checkLightFrameScopeCoverOpen()
{
    switch (flatFieldSource)
    {
        // All these are considered MANUAL when it comes to light frames
        case SOURCE_MANUAL:
        case SOURCE_DAWN_DUSK:
        case SOURCE_WALL:
            // If telescopes were MANUALLY covered before
            // we need to manually uncover them.
            if (m_CaptureState->telescopeCovered)
            {
                // If we already asked for confirmation and waiting for it
                // let us see if the confirmation is fulfilled
                // otherwise we return.
                if (coverQueryState == CAL_CHECK_CONFIRMATION)
                    return IPS_BUSY;

                // Otherwise, we ask user to confirm manually
                coverQueryState = CAL_CHECK_CONFIRMATION;
                emit askManualScopeLightOpen();

                return IPS_BUSY;
            }
            break;
        case SOURCE_FLATCAP:
        case SOURCE_DARKCAP:
            // if no state update happened, wait.
            if (lightBoxLightStatus == CAP_LIGHT_BUSY || dustCapLightStatus == CAP_LIGHT_BUSY ||
                    dustCapStatus == CAP_UNPARKING)
                return IPS_BUSY;

            // Account for light box only (no dust cap)
            if (m_CaptureState->hasLightBox && lightBoxLightStatus != CAP_LIGHT_OFF)
            {
                lightBoxLightStatus = CAP_LIGHT_BUSY;
                emit setLightBoxLight(false);
                emit newLog(i18n("Turn light box light off..."));
                return IPS_BUSY;
            }

            if (m_CaptureState->hasDustCap == false)
            {
                emit newLog("Skipping flat/dark cap since it is not connected.");
                return IPS_OK;
            }

            // If dust cap HAS light and light is ON, then turn it off.
            if (dustCapLightStatus != CAP_LIGHT_OFF)
            {
                dustCapLightStatus = CAP_LIGHT_BUSY;
                emit setDustCapLight(false);
                emit newLog(i18n("Turn dust cap light off..."));
                return IPS_BUSY;
            }

            // If cap is parked, we need to unpark it
            if (dustCapStatus != CAP_IDLE)
            {
                dustCapStatus = CAP_UNPARKING;
                emit parkDustCap(false);
                emit newLog(i18n("Unparking dust cap..."));
                return IPS_BUSY;
            }
            break;
    }
    // scope cover open (or no scope cover)
    return IPS_OK;
}

bool SequenceJobState::isInitialized(SequenceJobState::PrepareActions action)
{
    return m_CaptureState.data()->isInitialized[action];
}

void SequenceJobState::setInitialized(SequenceJobState::PrepareActions action, bool init)
{
    m_CaptureState.data()->isInitialized[action] = init;
}

void SequenceJobState::setCurrentFilterID(int value)
{
    m_CaptureState->currentFilterID = value;
    setInitialized(ACTION_FILTER, true);

    // TODO introduce settle time
    if (m_CaptureState->currentFilterID == targetFilterID)
        prepareActions[SequenceJobState::ACTION_FILTER] = true;

    checkAllActionsReady();
}

void SequenceJobState::setCurrentCCDTemperature(double currentTemperature)
{
    // skip if next value should be ignored
    if (ignoreNextValue[ACTION_TEMPERATURE])
    {
        ignoreNextValue[ACTION_TEMPERATURE] = false;
        return;
    }

    if (isInitialized(ACTION_TEMPERATURE))
    {
        if (m_enforceTemperature == false
                || fabs(targetTemperature - currentTemperature) <= Options::maxTemperatureDiff())
            prepareActions[SequenceJobState::ACTION_TEMPERATURE] = true;

        checkAllActionsReady();
    }
    else
    {
        setInitialized(ACTION_TEMPERATURE, true);
        if (m_enforceTemperature == false
                || fabs(targetTemperature - currentTemperature) <= Options::maxTemperatureDiff())
        {
            prepareActions[SequenceJobState::ACTION_TEMPERATURE] = true;
            checkAllActionsReady();
        }
        else
        {
            prepareTemperatureCheck(m_enforceTemperature);
        }
    }
}

void SequenceJobState::setCurrentRotatorPositionAngle(double rotatorAngle, IPState state)
{
    // position angle = rotatorAngle * paMul + paOffset
    // -180 < position angle <= 180
    double currentPositionAngle = range360(rotatorAngle * Options::pAMultiplier() + Options::pAOffset());
    if (currentPositionAngle > 180)
        currentPositionAngle -= 360.0;

    if (isInitialized(ACTION_ROTATOR))
    {
        // TODO introduce settle time
        // TODO make sure rotator has fully stopped
        if (fabs(currentPositionAngle - targetPositionAngle) * 60 <= Options::astrometryRotatorThreshold()
                && state != IPS_BUSY)
            prepareActions[SequenceJobState::ACTION_ROTATOR] = true;

        checkAllActionsReady();
    }
    else
    {
        setInitialized(ACTION_ROTATOR, true);
        prepareRotatorCheck();
    }
}

void SequenceJobState::setCurrentGuiderDrift(double value)
{
    setInitialized(ACTION_GUIDER_DRIFT, true);
    if (value <= targetStartGuiderDrift)
        prepareActions[ACTION_GUIDER_DRIFT] = true;

    checkAllActionsReady();
}

void SequenceJobState::manualScopeLightCover(bool closed, bool success)
{
    // covering confirmed
    if (success == true)
    {
        m_CaptureState->telescopeCovered = closed;
        coverQueryState = CAL_CHECK_TASK;
        // re-run checks
        checkAllActionsReady();
    }
    // cancelled
    else
    {
        coverQueryState = CAL_CHECK_TASK;
        // abort, no further checks
        emit abortCapture();
    }
}

void SequenceJobState::lightBoxLight(bool on)
{
    lightBoxLightStatus = on ? CAP_LIGHT_ON : CAP_LIGHT_OFF;
    emit newLog(i18n(on ? "Light box on." : "Light box off."));
    // re-run checks
    checkAllActionsReady();
}

void SequenceJobState::dustCapLight(bool on)
{
    dustCapLightStatus = on ? CAP_LIGHT_ON : CAP_LIGHT_OFF;
    emit newLog(i18n(on ? "Dust cap light on." : "Dust cap light off."));
    // re-run checks
    checkAllActionsReady();
}

void SequenceJobState::dustCapStatusChanged(ISD::DustCap::Status status)
{
    switch (status)
    {
        case ISD::DustCap::CAP_ERROR:
            dustCapStatus = CAP_ERROR;
            emit abortCapture();
            break;
        case ISD::DustCap::CAP_PARKED:
            dustCapStatus = CAP_PARKED;
            emit newLog(i18n("Dust cap parked."));
            break;
        case ISD::DustCap::CAP_IDLE:
            dustCapStatus = CAP_IDLE;
            emit newLog(i18n("Dust cap unparked."));
            break;
        case ISD::DustCap::CAP_UNPARKING:
            dustCapStatus = CAP_UNPARKING;
            break;
        case ISD::DustCap::CAP_PARKING:
            dustCapStatus = CAP_PARKING;
            break;
    }

    // re-run checks
    checkAllActionsReady();
}

void SequenceJobState::scopeStatusChanged(ISD::Telescope::Status status)
{
    // handle wall position
    switch (status)
    {
        case ISD::Telescope::MOUNT_TRACKING:
            if (wpScopeStatus == WP_SLEWING)
                wpScopeStatus = WP_SLEW_COMPLETED;
            break;
        case ISD::Telescope::MOUNT_IDLE:
            if (wpScopeStatus == WP_SLEWING || wpScopeStatus == WP_TRACKING_BUSY)
                wpScopeStatus = WP_TRACKING_OFF;
            break;
        default:
            // do nothing
            break;
    }
    scopeStatus = status;
    // re-run checks
    checkAllActionsReady();
}

void SequenceJobState::scopeParkStatusChanged(ISD::ParkStatus status)
{
    scopeParkStatus = status;
    // re-run checks
    checkAllActionsReady();
}

void SequenceJobState::domeStatusChanged(ISD::Dome::Status status)
{
    domeStatus = status;
    // re-run checks
    checkAllActionsReady();
}

void SequenceJobState::flatSyncFocusChanged(bool completed)
{
    flatSyncStatus = (completed ? FS_COMPLETED : FS_BUSY);
    // re-run checks
    checkAllActionsReady();
}

void SequenceJobState::hasShutter(bool present)
{
    if (present == true)
        m_CaptureState->shutterStatus = SHUTTER_YES;
    else
        m_CaptureState->shutterStatus = SHUTTER_NO;

    // re-run checks
    checkAllActionsReady();
}

void SequenceJobState::setEnforceInitialGuidingDrift(bool enforceInitialGuidingDrift)
{
    m_enforceInitialGuiding = enforceInitialGuidingDrift;
    // update the preparation action
    prepareActions[ACTION_GUIDER_DRIFT] = !enforceInitialGuidingDrift || m_isPreview;
    // re-run checks
    checkAllActionsReady();
}

SequenceJobState::PreparationState SequenceJobState::getPreparationState() const
{
    return m_PreparationState;
}
} // namespace
