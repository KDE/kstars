/*  Ekos state machine for a single capture job sequence
    SPDX-FileCopyrightText: Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "sequencejobstate.h"

#include "Options.h"
#include "kstarsdata.h"
#include "indicom.h"
#include "ekos/auxiliary/rotatorutils.h"

namespace Ekos
{
SequenceJobState::SequenceJobState(const QSharedPointer<CameraState> &sharedState)
{
    m_CameraState = sharedState;
}

void SequenceJobState::setFrameType(CCDFrameType frameType)
{
    // set the frame type
    m_frameType = frameType;
    // reset the preparation state
    m_PreparationState = PREP_NONE;
}

void SequenceJobState::initPreparation(bool isPreview)
{
    m_status      = JOB_BUSY;
    m_isPreview   = isPreview;
    wpScopeStatus = WP_NONE;
}

void SequenceJobState::prepareLightFrameCapture(bool enforceCCDTemp, bool isPreview)
{
    // precondition: do not start while already being busy and conditions haven't changed
    if (m_status == JOB_BUSY && enforceCCDTemp == m_enforceTemperature)
        return;

    // initialize the states
    initPreparation(isPreview);

    // Reset all prepare actions
    setAllActionsReady();

    // disable batch mode for previews
    emit setCCDBatchMode(!isPreview);

    // Check if we need to update temperature (only skip if the value is initialized and within the limits)
    prepareTemperatureCheck(enforceCCDTemp);

    // Check if we need to update rotator (only skip if the value is initialized and within the limits)
    prepareRotatorCheck();

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

    // initialize the states
    initPreparation(isPreview);

    // Reset all prepare actions
    setAllActionsReady();

    // disable batch mode for previews
    emit setCCDBatchMode(!isPreview);

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

    // initialize the states
    initPreparation(isPreview);

    // Reset all prepare actions
    setAllActionsReady();

    // disable batch mode for previews
    emit setCCDBatchMode(!isPreview);

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

bool SequenceJobState::initCapture(CCDFrameType frameType, bool isPreview, bool isAutofocusReady, FITSMode mode)
{
    m_PreparationState = PREP_INIT_CAPTURE;
    autoFocusReady = isAutofocusReady;
    m_fitsMode = mode;

    //check for setting the target filter
    prepareTargetFilter(frameType, isPreview);
    checkAllActionsReady();

    return areActionsReady();
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
    // ignore events if preparation is already completed
    if (preparationCompleted())
        return;

    switch (m_PreparationState)
    {
        // capture preparation
        case PREP_BUSY:
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
                    if (checkFlatsCoverReady() != IPS_OK)
                        return;

                    // 2. If we used AUTOFOCUS before for a specific frame (e.g. Lum)
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
                // darks and bias frames are handled in the same way
                case FRAME_DARK:
                case FRAME_BIAS:
                    if (!areActionsReady())
                        return;

                    // 1. check if the scope is covered appropriately
                    if (checkDarksCoverReady() != IPS_OK)
                        return;

                    // 2. avoid doubled events
                    if (m_PreparationState == PREP_BUSY)
                    {
                        m_PreparationState = PREP_COMPLETED;
                        emit prepareComplete();
                    }
                    break;
                default:
                    // all other cases not refactored yet, preparation immediately completed
                    emit prepareComplete();
                    break;
            }
            break;

        // capture initialization (final preparation steps before starting frame capturing)
        case PREP_INIT_CAPTURE:
            if (areActionsReady())
            {
                // reset the state to avoid double events
                m_PreparationState = PREP_NONE;
                emit initCaptureComplete(m_fitsMode);
            }
            break;

        case PREP_NONE:
        case PREP_COMPLETED:
            // in all other cases do nothing
            break;
    }
}

void SequenceJobState::setAllActionsReady()
{
    QMutableMapIterator<CaptureWorkflowActionType, bool> it(prepareActions);

    while (it.hasNext())
    {
        it.next();
        it.setValue(true);
    }
    // reset the initialization state
    for (CaptureWorkflowActionType action :
            {
                CAPTURE_ACTION_FILTER, CAPTURE_ACTION_ROTATOR, CAPTURE_ACTION_TEMPERATURE
            })
        setInitialized(action, false);
}

void SequenceJobState::prepareTargetFilter(CCDFrameType frameType, bool isPreview)
{
    if (targetFilterID != INVALID_VALUE)
    {
        if (isInitialized(CAPTURE_ACTION_FILTER) == false)
        {
            prepareActions[CAPTURE_ACTION_FILTER] = false;

            // Don't perform autofocus on preview or calibration frames or if Autofocus is not ready yet.
            if (isPreview || frameType != FRAME_LIGHT || autoFocusReady == false)
                m_filterPolicy = static_cast<FilterManager::FilterPolicy>(m_filterPolicy & ~FilterManager::AUTOFOCUS_POLICY);

            emit readFilterPosition();
        }
        else if (targetFilterID != m_CameraState->currentFilterID)
        {
            // mark filter preparation action
            prepareActions[CAPTURE_ACTION_FILTER] = false;

            // determine policy
            m_filterPolicy = FilterManager::ALL_POLICIES;

            // Don't perform autofocus on preview or calibration frames or if Autofocus is not ready yet.
            if (isPreview || frameType != FRAME_LIGHT || autoFocusReady == false)
                m_filterPolicy = static_cast<FilterManager::FilterPolicy>(m_filterPolicy & ~FilterManager::AUTOFOCUS_POLICY);

            emit changeFilterPosition(targetFilterID, m_filterPolicy);
            emit prepareState(CAPTURE_CHANGING_FILTER);
        }
    }
}

void SequenceJobState::prepareTemperatureCheck(bool enforceCCDTemp)
{
    // turn on CCD temperature enforcing if required
    m_enforceTemperature = enforceCCDTemp;

    if (m_enforceTemperature)
    {
        prepareActions[CAPTURE_ACTION_TEMPERATURE] = false;
        if (isInitialized(CAPTURE_ACTION_TEMPERATURE))
        {
            // ignore the next value since after setting temperature the next received value will be
            // exactly this value no matter what the CCD temperature
            ignoreNextValue[CAPTURE_ACTION_TEMPERATURE] = true;
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
        if (isInitialized(CAPTURE_ACTION_ROTATOR))
        {
            prepareActions[CAPTURE_ACTION_ROTATOR] = false;
            double rawAngle = RotatorUtils::Instance()->calcRotatorAngle(targetPositionAngle);
            emit prepareState(CAPTURE_SETTING_ROTATOR);
            emit setRotatorAngle(rawAngle);
        }
        // trigger setting current value first if not initialized
        else
            emit readCurrentState(CAPTURE_SETTING_ROTATOR);
    }
}

IPState SequenceJobState::checkCalibrationPreActionsReady()
{
    IPState result = IPS_OK;

    if (m_CalibrationPreAction & CAPTURE_PREACTION_WALL)
        result = checkWallPositionReady(FRAME_FLAT);

    if (result != IPS_OK)
        return result;

    if (m_CalibrationPreAction & CAPTURE_PREACTION_PARK_MOUNT)
        result = checkPreMountParkReady();

    if (result != IPS_OK)
        return result;

    if (m_CalibrationPreAction & CAPTURE_PREACTION_PARK_DOME)
        result = checkPreDomeParkReady();

    return result;
}

IPState SequenceJobState::checkFlatsCoverReady()
{
    auto result = checkCalibrationPreActionsReady();
    if (result == IPS_OK)
    {
        if (m_CameraState->hasDustCap && m_CameraState->hasLightBox)
            return checkDustCapReady(FRAME_FLAT);
        // In case we have a wall action then we are facing a flat light source and we can immediately continue to next step
        else if (m_CalibrationPreAction & CAPTURE_PREACTION_WALL)
            return IPS_OK;
        else
        {
            // In case we ONLY have a lightbox then we need to ensure it's toggled correctly first
            if (m_CameraState->hasLightBox)
                return checkDustCapReady(FRAME_FLAT);

            return checkManualCoverReady(true);
        }
    }

    return result;
}

IPState SequenceJobState::checkDarksCoverReady()
{
    IPState result = checkCalibrationPreActionsReady();;

    if (result == IPS_OK)
    {
        // 1. check if the CCD has a shutter
        result = checkHasShutter();
        if (result != IPS_OK)
            return result;

        if (m_CameraState->hasDustCap)
            return checkDustCapReady(FRAME_DARK);
        // In case we have a wall action then we are facing a designated location and we can immediately continue to next step
        else if (m_CalibrationPreAction & CAPTURE_PREACTION_WALL)
            return IPS_OK;
        else
            return checkManualCoverReady(false);
    }
    return result;
}

IPState SequenceJobState::checkManualCoverReady(bool lightSourceRequired)
{
    // Manual mode we need to cover mount with evenly illuminated field.
    if (lightSourceRequired && m_CameraState->m_ManualCoverState != CameraState::MANUAL_COVER_CLOSED_LIGHT)
    {
        if (coverQueryState == CAL_CHECK_CONFIRMATION)
            return IPS_BUSY;

        // request asking the user to cover the scope manually with a light source
        emit askManualScopeCover(i18n("Cover the telescope with an evenly illuminated light source."),
                                 i18n("Flat Frame"), true);
        coverQueryState = CAL_CHECK_CONFIRMATION;

        return IPS_BUSY;
    }
    else if (!lightSourceRequired && m_CameraState->m_ManualCoverState != CameraState::MANUAL_COVER_CLOSED_DARK &&
             m_CameraState->shutterStatus == SHUTTER_NO)
    {
        if (coverQueryState == CAL_CHECK_CONFIRMATION)
            return IPS_BUSY;

        emit askManualScopeCover(i18n("Cover the telescope in order to take a dark exposure."),
                                 i18n("Dark Exposure"), false);

        coverQueryState = CAL_CHECK_CONFIRMATION;

        return IPS_BUSY;
    }
    return IPS_OK;
}

IPState SequenceJobState::checkDustCapReady(CCDFrameType frameType)
{
    // turning on flat light running
    if (m_CameraState->getLightBoxLightState() == CAP_LIGHT_BUSY  ||
            m_CameraState->getDustCapState() == CAP_PARKING ||
            m_CameraState->getDustCapState() == CAP_UNPARKING)
        return IPS_BUSY;
    // error occured
    if (m_CameraState->getDustCapState() == CAP_ERROR)
        return IPS_ALERT;

    auto captureLights = (frameType == FRAME_LIGHT);

    // for flats open the cap and close it otherwise
    CapState targetCapState = captureLights ? CAP_IDLE : CAP_PARKED;
    // If cap is parked, unpark it since dark cap uses external light source.
    if (m_CameraState->hasDustCap && m_CameraState->getDustCapState() != targetCapState)
    {
        m_CameraState->setDustCapState(captureLights ? CAP_UNPARKING : CAP_PARKING);
        emit parkDustCap(!captureLights);
        emit newLog(captureLights ? i18n("Unparking dust cap...") : i18n("Parking dust cap..."));
        return IPS_BUSY;
    }

    auto captureFlats = (frameType == FRAME_FLAT);
    LightState targetLightBoxStatus = captureFlats ? CAP_LIGHT_ON :
                                      CAP_LIGHT_OFF;

    if (m_CameraState->hasLightBox && m_CameraState->getLightBoxLightState() != targetLightBoxStatus)
    {
        m_CameraState->setLightBoxLightState(CAP_LIGHT_BUSY);
        emit setLightBoxLight(captureFlats);
        emit newLog(captureFlats ? i18n("Turn light box light on...") : i18n("Turn light box light off..."));
        return IPS_BUSY;
    }

    // nothing more to do
    return IPS_OK;
}

IPState SequenceJobState::checkWallPositionReady(CCDFrameType frametype)
{
    if (m_CameraState->hasTelescope)
    {
        if (wpScopeStatus < WP_SLEWING)
        {
            wallCoord.HorizontalToEquatorial(KStarsData::Instance()->lst(),
                                             KStarsData::Instance()->geo()->lat());
            wpScopeStatus = WP_SLEWING;
            emit slewTelescope(wallCoord);
            emit newLog(i18n("Mount slewing to wall position (az =%1 alt =%2)",
                             wallCoord.alt().toDMSString(), wallCoord.az().toDMSString()));
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
            emit newLog(i18n("Slew to wall position complete, tracking stopped."));

        // wall position reached, check if we have a light box to turn on for flats and off otherwise
        bool captureFlats = (frametype == FRAME_FLAT);
        LightState targetLightState = (captureFlats ? CAP_LIGHT_ON :
                                       CAP_LIGHT_OFF);

        if (m_CameraState->hasLightBox == true)
        {
            if (m_CameraState->getLightBoxLightState() != targetLightState)
            {
                m_CameraState->setLightBoxLightState(CAP_LIGHT_BUSY);
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
    if (m_CameraState->hasTelescope)
    {
        switch (m_CameraState->getScopeParkState())
        {
            case ISD::PARK_PARKED:
                break;
            case ISD::PARK_ERROR:
                emit newLog(i18n("Parking mount failed, aborting..."));
                emit abortCapture();
                return IPS_ALERT;
            case ISD::PARK_PARKING:
                return IPS_BUSY;
            case ISD::PARK_UNPARKED:
            case ISD::PARK_UNPARKING:
                // park the scope
                emit setScopeParked(true);
                emit newLog(i18n("Parking mount prior to calibration frames capture..."));
                return IPS_BUSY;
            case ISD::PARK_UNKNOWN:
                // retrieve the mount park state
                emit readCurrentMountParkState();
                return IPS_BUSY;
        }
    }
    // everything ready
    return IPS_OK;

}

IPState SequenceJobState::checkPreDomeParkReady()
{
    if (m_CameraState->hasDome)
    {
        if (m_CameraState->getDomeState() == ISD::Dome::DOME_ERROR)
        {
            emit newLog(i18n("Parking dome failed, aborting..."));
            emit abortCapture();
            return IPS_ALERT;
        }
        else if (m_CameraState->getDomeState() == ISD::Dome::DOME_PARKING)
            return IPS_BUSY;
        else if (m_CameraState->getDomeState() != ISD::Dome::DOME_PARKED)
        {
            m_CameraState->setDomeState(ISD::Dome::DOME_PARKING);
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
        QTimer::singleShot(1000, this, [this]
        {
            // wait for one second and repeat the request again
            emit flatSyncFocus(targetFilterID);
        });
        return IPS_BUSY;
    }

    if (m_frameType == FRAME_FLAT && Options::flatSyncFocus() && flatSyncStatus != FS_COMPLETED)
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
    if (m_CameraState->shutterStatus == SHUTTER_BUSY)
        return IPS_BUSY;
    if (m_CameraState->shutterStatus != SHUTTER_UNKNOWN)
        return IPS_OK;
    // query the status
    m_CameraState->shutterStatus = SHUTTER_BUSY;
    emit queryHasShutter();
    return IPS_BUSY;
}

IPState SequenceJobState::checkLightFrameScopeCoverOpen()
{
    // Account for light box only (no dust cap)
    if (m_CameraState->hasLightBox && m_CameraState->getLightBoxLightState() != CAP_LIGHT_OFF)
    {
        if (m_CameraState->getLightBoxLightState() != CAP_LIGHT_BUSY)
        {
            m_CameraState->setLightBoxLightState(CAP_LIGHT_BUSY);
            emit setLightBoxLight(false);
            emit newLog(i18n("Turn light box light off..."));
        }
        return IPS_BUSY;
    }

    // If we have a dust cap, then we must unpark
    if (m_CameraState->hasDustCap)
    {
        if (m_CameraState->getDustCapState() != CAP_IDLE)
        {
            if (m_CameraState->getDustCapState() != CAP_UNPARKING)
            {
                m_CameraState->setDustCapState(CAP_UNPARKING);
                emit parkDustCap(false);
                emit newLog(i18n("Unparking dust cap..."));
            }
            return IPS_BUSY;
        }

        return IPS_OK;
    }

    // If telescopes were MANUALLY covered before
    // we need to manually uncover them.
    if (m_CameraState->m_ManualCoverState != CameraState::MANAUL_COVER_OPEN)
    {
        // If we already asked for confirmation and waiting for it
        // let us see if the confirmation is fulfilled
        // otherwise we return.
        if (coverQueryState == CAL_CHECK_CONFIRMATION)
            return IPS_BUSY;

        emit askManualScopeOpen(m_CameraState->m_ManualCoverState == CameraState::MANUAL_COVER_CLOSED_LIGHT);

        return IPS_BUSY;
    }

    // scope cover open (or no scope cover)
    return IPS_OK;
}

bool SequenceJobState::isInitialized(CaptureWorkflowActionType action)
{
    return m_CameraState.data()->isInitialized[action];
}

void SequenceJobState::setInitialized(CaptureWorkflowActionType action, bool init)
{
    m_CameraState.data()->isInitialized[action] = init;
}

void SequenceJobState::setCurrentFilterID(int value)
{
    // ignore events if preparation is already completed
    if (preparationCompleted())
        return;

    m_CameraState->currentFilterID = value;
    if (isInitialized(CAPTURE_ACTION_FILTER) == false && value != targetFilterID)
    {
        // mark filter preparation action
        // TODO introduce settle time
        prepareActions[CAPTURE_ACTION_FILTER] = false;

        emit changeFilterPosition(targetFilterID, m_filterPolicy);
        emit prepareState(CAPTURE_CHANGING_FILTER);
    }
    setInitialized(CAPTURE_ACTION_FILTER, true);

    if (value == targetFilterID)
        prepareActions[CAPTURE_ACTION_FILTER] = true;

    checkAllActionsReady();
}

void SequenceJobState::setCurrentCCDTemperature(double currentTemperature)
{
    // ignore events if preparation is already completed
    if (preparationCompleted())
        return;

    // skip if next value should be ignored
    if (ignoreNextValue[CAPTURE_ACTION_TEMPERATURE])
    {
        ignoreNextValue[CAPTURE_ACTION_TEMPERATURE] = false;
        return;
    }

    if (isInitialized(CAPTURE_ACTION_TEMPERATURE))
    {
        if (m_enforceTemperature == false
                || fabs(targetTemperature - currentTemperature) <= Options::maxTemperatureDiff())
            prepareActions[CAPTURE_ACTION_TEMPERATURE] = true;

        checkAllActionsReady();
    }
    else
    {
        setInitialized(CAPTURE_ACTION_TEMPERATURE, true);
        if (m_enforceTemperature == false
                || fabs(targetTemperature - currentTemperature) <= Options::maxTemperatureDiff())
        {
            prepareActions[CAPTURE_ACTION_TEMPERATURE] = true;
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
    // ignore events if preparation is already completed
    if (preparationCompleted())
        return;

    double currentPositionAngle = RotatorUtils::Instance()->calcCameraAngle(rotatorAngle, false);

    if (isInitialized(CAPTURE_ACTION_ROTATOR))
    {
        // TODO introduce settle time
        // TODO make sure rotator has fully stopped -> see 'align.cpp' captureAndSolve()
        if (fabs(currentPositionAngle - targetPositionAngle) * 60 <= Options::astrometryRotatorThreshold()
                && state != IPS_BUSY)
            prepareActions[CAPTURE_ACTION_ROTATOR] = true;

        checkAllActionsReady();
    }
    else
    {
        setInitialized(CAPTURE_ACTION_ROTATOR, true);
        if (fabs(currentPositionAngle - targetPositionAngle) * 60 <= Options::astrometryRotatorThreshold()
                && state != IPS_BUSY)
        {
            prepareActions[CAPTURE_ACTION_ROTATOR] = true;
            checkAllActionsReady();
        }
        else
        {
            prepareRotatorCheck();
        }
    }
}

void SequenceJobState::setFocusStatus(FocusState state)
{
    // ignore events if preparation is already completed
    if (preparationCompleted())
        return;

    switch (state)
    {
        case FOCUS_COMPLETE:
            // did we wait for a successful autofocus run?
            if (prepareActions[CAPTURE_ACTION_AUTOFOCUS] == false)
            {
                prepareActions[CAPTURE_ACTION_AUTOFOCUS] = true;
                checkAllActionsReady();
            }
            break;
        case FOCUS_ABORTED:
        case FOCUS_FAILED:
            // finish preparation with failure
            emit prepareComplete(false);
            break;
        default:
            // in all other cases do nothing
            break;
    }
}

void SequenceJobState::updateManualScopeCover(bool closed, bool success, bool light)
{
    // ignore events if preparation is already completed
    if (preparationCompleted())
        return;

    // covering confirmed
    if (success == true)
    {
        if (closed)
            m_CameraState->m_ManualCoverState = light ? CameraState::MANUAL_COVER_CLOSED_LIGHT :
                                                CameraState::MANUAL_COVER_CLOSED_DARK;
        else
            m_CameraState->m_ManualCoverState = CameraState::MANAUL_COVER_OPEN;
        coverQueryState = CAL_CHECK_TASK;
        // re-run checks
        checkAllActionsReady();
    }
    // cancelled
    else
    {
        m_CameraState->shutterStatus = SHUTTER_UNKNOWN;
        coverQueryState = CAL_CHECK_TASK;
        // abort, no further checks
        emit abortCapture();
    }
}

void SequenceJobState::lightBoxLight(bool on)
{
    // ignore events if preparation is already completed
    if (preparationCompleted())
        return;

    m_CameraState->setLightBoxLightState(on ? CAP_LIGHT_ON : CAP_LIGHT_OFF);
    emit newLog(i18n(on ? "Light box on." : "Light box off."));
    // re-run checks
    checkAllActionsReady();
}

void SequenceJobState::dustCapStateChanged(ISD::DustCap::Status status)
{
    // ignore events if preparation is already completed
    if (preparationCompleted())
        return;

    switch (status)
    {
        case ISD::DustCap::CAP_ERROR:
            emit abortCapture();
            break;
        default:
            // do nothing
            break;
    }

    // re-run checks
    checkAllActionsReady();
}

void SequenceJobState::scopeStatusChanged(ISD::Mount::Status status)
{
    // ignore events if preparation is already completed
    if (preparationCompleted())
        return;

    // handle wall position
    switch (status)
    {
        case ISD::Mount::MOUNT_TRACKING:
            if (wpScopeStatus == WP_SLEWING)
                wpScopeStatus = WP_SLEW_COMPLETED;
            break;
        case ISD::Mount::MOUNT_IDLE:
            if (wpScopeStatus == WP_SLEWING || wpScopeStatus == WP_TRACKING_BUSY)
                wpScopeStatus = WP_TRACKING_OFF;
            break;
        case ISD::Mount::MOUNT_PARKING:
            // Ensure the parking state to avoid double park calls
            m_CameraState->setScopeParkState(ISD::PARK_PARKING);
            break;
        default:
            // do nothing
            break;
    }
    // re-run checks
    checkAllActionsReady();
}

void SequenceJobState::scopeParkStatusChanged(ISD::ParkStatus)
{
    // re-run checks
    checkAllActionsReady();
}

void SequenceJobState::domeStatusChanged(ISD::Dome::Status)
{
    // re-run checks
    checkAllActionsReady();
}

void SequenceJobState::flatSyncFocusChanged(bool completed)
{
    // ignore events if preparation is already completed
    if (preparationCompleted())
        return;

    flatSyncStatus = (completed ? FS_COMPLETED : FS_BUSY);
    // re-run checks
    checkAllActionsReady();
}

void SequenceJobState::hasShutter(bool present)
{
    // ignore events if preparation is already completed
    if (preparationCompleted())
        return;

    if (present == true)
        m_CameraState->shutterStatus = SHUTTER_YES;
    else
        m_CameraState->shutterStatus = SHUTTER_NO;

    // re-run checks
    checkAllActionsReady();
}

SequenceJobState::PreparationState SequenceJobState::getPreparationState() const
{
    return m_PreparationState;
}

void SequenceJobState::setFilterStatus(FilterState filterState)
{
    // ignore events if preparation is already completed
    if (preparationCompleted())
        return;

    switch (filterState)
    {
        case FILTER_AUTOFOCUS:
            // we need to wait until focusing has completed
            prepareActions[CAPTURE_ACTION_AUTOFOCUS] = false;
            emit prepareState(CAPTURE_FOCUSING);
            break;

        // nothing to do in all other cases
        case FILTER_IDLE:
        case FILTER_OFFSET:
        case FILTER_CHANGE:
            break;
    }

}
} // namespace
