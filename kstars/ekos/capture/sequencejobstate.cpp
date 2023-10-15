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
SequenceJobState::SequenceJobState(const QSharedPointer<CaptureModuleState> &sharedState)
{
    m_CaptureModuleState = sharedState;
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

void SequenceJobState::prepareLightFrameCapture(bool enforceCCDTemp, bool enforceInitialGuidingDrift, bool isPreview)
{
    // precondition: do not start while already being busy and conditions haven't changed
    if (m_status == JOB_BUSY && enforceCCDTemp == m_enforceTemperature && enforceInitialGuidingDrift == m_enforceInitialGuiding)
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

    // Check if we need to wait for guiding being initially below the target value
    m_enforceInitialGuiding = enforceInitialGuidingDrift;
    if (enforceInitialGuidingDrift && !isPreview)
        prepareActions[CaptureModuleState::ACTION_GUIDER_DRIFT] = false;

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
    QMutableMapIterator<CaptureModuleState::PrepareActions, bool> it(prepareActions);

    while (it.hasNext())
    {
        it.next();
        it.setValue(true);
    }
    // reset the initialization state
    for (CaptureModuleState::PrepareActions action :
            {
                CaptureModuleState::ACTION_FILTER, CaptureModuleState::ACTION_ROTATOR, CaptureModuleState::ACTION_TEMPERATURE,
                CaptureModuleState::ACTION_GUIDER_DRIFT
            })
        setInitialized(action, false);
}

void SequenceJobState::prepareTargetFilter(CCDFrameType frameType, bool isPreview)
{
    if (targetFilterID != INVALID_VALUE)
    {
        if (isInitialized(CaptureModuleState::ACTION_FILTER) == false)
        {
            prepareActions[CaptureModuleState::ACTION_FILTER] = false;

            // Don't perform autofocus on preview or calibration frames or if Autofocus is not ready yet.
            if (isPreview || frameType != FRAME_LIGHT || autoFocusReady == false)
                m_filterPolicy = static_cast<FilterManager::FilterPolicy>(m_filterPolicy & ~FilterManager::AUTOFOCUS_POLICY);

            emit readFilterPosition();
        }
        else if (targetFilterID != m_CaptureModuleState->currentFilterID)
        {
            // mark filter preparation action
            prepareActions[CaptureModuleState::ACTION_FILTER] = false;

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
        prepareActions[CaptureModuleState::ACTION_TEMPERATURE] = false;
        if (isInitialized(CaptureModuleState::ACTION_TEMPERATURE))
        {
            // ignore the next value since after setting temperature the next received value will be
            // exactly this value no matter what the CCD temperature
            ignoreNextValue[CaptureModuleState::ACTION_TEMPERATURE] = true;
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
        if (isInitialized(CaptureModuleState::ACTION_ROTATOR))
        {
            prepareActions[CaptureModuleState::ACTION_ROTATOR] = false;
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

    if (m_CalibrationPreAction & ACTION_WALL)
        result = checkWallPositionReady(FRAME_FLAT);

    if (result != IPS_OK)
        return result;

    if (m_CalibrationPreAction & ACTION_PARK_MOUNT)
        result = checkPreMountParkReady();

    if (result != IPS_OK)
        return result;

    if (m_CalibrationPreAction & ACTION_PARK_DOME)
        result = checkPreDomeParkReady();

    return result;
}

IPState SequenceJobState::checkFlatsCoverReady()
{
    auto result = checkCalibrationPreActionsReady();
    if (result == IPS_OK)
    {
        if (m_CaptureModuleState->hasDustCap && m_CaptureModuleState->hasLightBox)
            return checkDustCapReady(FRAME_FLAT);
        // In case we have a wall action then we are facing a flat light source and we can immediately continue to next step
        else if (m_CalibrationPreAction == ACTION_WALL)
            return IPS_OK;
        else
        {
            // In case we ONLY have a lightbox then we need to ensure it's toggled correctly first
            if (m_CaptureModuleState->hasLightBox)
            {
                auto lightBoxState = checkDustCapReady(FRAME_FLAT);
                if (lightBoxState != IPS_OK)
                    return lightBoxState;
            }

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

        if (m_CaptureModuleState->hasDustCap)
            return checkDustCapReady(FRAME_DARK);
        // In case we have a wall action then we are facing a designated location and we can immediately continue to next step
        else if (m_CalibrationPreAction == ACTION_WALL)
            return IPS_OK;
        else
            return checkManualCoverReady(false);
    }
    return result;
}

IPState SequenceJobState::checkManualCoverReady(bool lightSourceRequired)
{
    // Manual mode we need to cover mount with evenly illuminated field.
    if (lightSourceRequired && m_CaptureModuleState->m_ManualCoverState != CaptureModuleState::MANUAL_COVER_CLOSED_LIGHT)
    {
        if (coverQueryState == CAL_CHECK_CONFIRMATION)
            return IPS_BUSY;

        // request asking the user to cover the scope manually with a light source
        emit askManualScopeCover(i18n("Cover the telescope with an evenly illuminated light source."),
                                 i18n("Flat Frame"), true);
        coverQueryState = CAL_CHECK_CONFIRMATION;

        return IPS_BUSY;
    }
    else if (!lightSourceRequired && m_CaptureModuleState->m_ManualCoverState != CaptureModuleState::MANUAL_COVER_CLOSED_DARK &&
             m_CaptureModuleState->shutterStatus == CaptureModuleState::SHUTTER_NO)
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
    if (m_CaptureModuleState->getLightBoxLightState() == CaptureModuleState::CAP_LIGHT_BUSY  ||
            m_CaptureModuleState->getDustCapState() == CaptureModuleState::CAP_PARKING ||
            m_CaptureModuleState->getDustCapState() == CaptureModuleState::CAP_UNPARKING)
        return IPS_BUSY;
    // error occured
    if (m_CaptureModuleState->getDustCapState() == CaptureModuleState::CAP_ERROR)
        return IPS_ALERT;

    auto captureLights = (frameType == FRAME_LIGHT);

    // for flats open the cap and close it otherwise
    CaptureModuleState::CapState targetCapState = captureLights ? CaptureModuleState::CAP_IDLE : CaptureModuleState::CAP_PARKED;
    // If cap is parked, unpark it since dark cap uses external light source.
    if (m_CaptureModuleState->hasDustCap && m_CaptureModuleState->getDustCapState() != targetCapState)
    {
        m_CaptureModuleState->setDustCapState(captureLights ? CaptureModuleState::CAP_UNPARKING : CaptureModuleState::CAP_PARKING);
        emit parkDustCap(!captureLights);
        emit newLog(captureLights ? i18n("Unparking dust cap...") : i18n("Parking dust cap..."));
        return IPS_BUSY;
    }

    auto captureFlats = (frameType == FRAME_FLAT);
    CaptureModuleState::LightState targetLightBoxStatus = captureFlats ? CaptureModuleState::CAP_LIGHT_ON :
            CaptureModuleState::CAP_LIGHT_OFF;

    if (m_CaptureModuleState->hasLightBox && m_CaptureModuleState->getLightBoxLightState() != targetLightBoxStatus)
    {
        m_CaptureModuleState->setLightBoxLightState(CaptureModuleState::CAP_LIGHT_BUSY);
        emit setLightBoxLight(captureFlats);
        emit newLog(captureFlats ? i18n("Turn light box light on...") : i18n("Turn light box light off..."));
        return IPS_BUSY;
    }

    // nothing more to do
    return IPS_OK;
}

IPState SequenceJobState::checkWallPositionReady(CCDFrameType frametype)
{
    if (m_CaptureModuleState->hasTelescope)
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
        CaptureModuleState::LightState targetLightState = (captureFlats ? CaptureModuleState::CAP_LIGHT_ON :
                CaptureModuleState::CAP_LIGHT_OFF);

        if (m_CaptureModuleState->hasLightBox == true)
        {
            if (m_CaptureModuleState->getLightBoxLightState() != targetLightState)
            {
                m_CaptureModuleState->setLightBoxLightState(CaptureModuleState::CAP_LIGHT_BUSY);
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
    if (m_CaptureModuleState->hasTelescope)
    {
        if (m_CaptureModuleState->getScopeParkState() == ISD::PARK_ERROR)
        {
            emit newLog(i18n("Parking mount failed, aborting..."));
            emit abortCapture();
            return IPS_ALERT;
        }
        else if (m_CaptureModuleState->getScopeParkState() == ISD::PARK_PARKING)
            return IPS_BUSY;
        else if (m_CaptureModuleState->getScopeParkState() != ISD::PARK_PARKED)
        {
            m_CaptureModuleState->setScopeParkState(ISD::PARK_PARKING);
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
    if (m_CaptureModuleState->hasDome)
    {
        if (m_CaptureModuleState->getDomeState() == ISD::Dome::DOME_ERROR)
        {
            emit newLog(i18n("Parking dome failed, aborting..."));
            emit abortCapture();
            return IPS_ALERT;
        }
        else if (m_CaptureModuleState->getDomeState() == ISD::Dome::DOME_PARKING)
            return IPS_BUSY;
        else if (m_CaptureModuleState->getDomeState() != ISD::Dome::DOME_PARKED)
        {
            m_CaptureModuleState->setDomeState(ISD::Dome::DOME_PARKING);
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
    if (m_CaptureModuleState->shutterStatus == CaptureModuleState::SHUTTER_BUSY)
        return IPS_BUSY;
    if (m_CaptureModuleState->shutterStatus != CaptureModuleState::SHUTTER_UNKNOWN)
        return IPS_OK;
    // query the status
    m_CaptureModuleState->shutterStatus = CaptureModuleState::SHUTTER_BUSY;
    emit queryHasShutter();
    return IPS_BUSY;
}

IPState SequenceJobState::checkLightFrameScopeCoverOpen()
{
    // Account for light box only (no dust cap)
    if (m_CaptureModuleState->hasLightBox && m_CaptureModuleState->getLightBoxLightState() != CaptureModuleState::CAP_LIGHT_OFF)
    {
        if (m_CaptureModuleState->getLightBoxLightState() != CaptureModuleState::CAP_LIGHT_BUSY)
        {
            m_CaptureModuleState->setLightBoxLightState(CaptureModuleState::CAP_LIGHT_BUSY);
            emit setLightBoxLight(false);
            emit newLog(i18n("Turn light box light off..."));
        }
        return IPS_BUSY;
    }

    // If we have a dust cap, then we must unpark
    if (m_CaptureModuleState->hasDustCap)
    {
        if (m_CaptureModuleState->getDustCapState() != CaptureModuleState::CAP_IDLE)
        {
            if (m_CaptureModuleState->getDustCapState() != CaptureModuleState::CAP_UNPARKING)
            {
                m_CaptureModuleState->setDustCapState(CaptureModuleState::CAP_UNPARKING);
                emit parkDustCap(false);
                emit newLog(i18n("Unparking dust cap..."));
            }
            return IPS_BUSY;
        }

        return IPS_OK;
    }

    // If telescopes were MANUALLY covered before
    // we need to manually uncover them.
    if (m_CaptureModuleState->m_ManualCoverState != CaptureModuleState::MANAUL_COVER_OPEN)
    {
        // If we already asked for confirmation and waiting for it
        // let us see if the confirmation is fulfilled
        // otherwise we return.
        if (coverQueryState == CAL_CHECK_CONFIRMATION)
            return IPS_BUSY;

        emit askManualScopeOpen(m_CaptureModuleState->m_ManualCoverState == CaptureModuleState::MANUAL_COVER_CLOSED_LIGHT);

        return IPS_BUSY;
    }

    // scope cover open (or no scope cover)
    return IPS_OK;
}

bool SequenceJobState::isInitialized(CaptureModuleState::PrepareActions action)
{
    return m_CaptureModuleState.data()->isInitialized[action];
}

void SequenceJobState::setInitialized(CaptureModuleState::PrepareActions action, bool init)
{
    m_CaptureModuleState.data()->isInitialized[action] = init;
}

void SequenceJobState::setCurrentFilterID(int value)
{
    m_CaptureModuleState->currentFilterID = value;
    if (isInitialized(CaptureModuleState::ACTION_FILTER) == false && value != targetFilterID)
    {
        // mark filter preparation action
        // TODO introduce settle time
        prepareActions[CaptureModuleState::ACTION_FILTER] = false;

        emit changeFilterPosition(targetFilterID, m_filterPolicy);
        emit prepareState(CAPTURE_CHANGING_FILTER);
    }
    setInitialized(CaptureModuleState::ACTION_FILTER, true);

    if (value == targetFilterID)
        prepareActions[CaptureModuleState::ACTION_FILTER] = true;

    checkAllActionsReady();
}

void SequenceJobState::setCurrentCCDTemperature(double currentTemperature)
{
    // skip if next value should be ignored
    if (ignoreNextValue[CaptureModuleState::ACTION_TEMPERATURE])
    {
        ignoreNextValue[CaptureModuleState::ACTION_TEMPERATURE] = false;
        return;
    }

    if (isInitialized(CaptureModuleState::ACTION_TEMPERATURE))
    {
        if (m_enforceTemperature == false
                || fabs(targetTemperature - currentTemperature) <= Options::maxTemperatureDiff())
            prepareActions[CaptureModuleState::ACTION_TEMPERATURE] = true;

        checkAllActionsReady();
    }
    else
    {
        setInitialized(CaptureModuleState::ACTION_TEMPERATURE, true);
        if (m_enforceTemperature == false
                || fabs(targetTemperature - currentTemperature) <= Options::maxTemperatureDiff())
        {
            prepareActions[CaptureModuleState::ACTION_TEMPERATURE] = true;
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
    double currentPositionAngle = RotatorUtils::Instance()->calcCameraAngle(rotatorAngle, false);

    if (isInitialized(CaptureModuleState::ACTION_ROTATOR))
    {
        // TODO introduce settle time
        // TODO make sure rotator has fully stopped -> see 'align.cpp' captureAndSolve()
        if (fabs(currentPositionAngle - targetPositionAngle) * 60 <= Options::astrometryRotatorThreshold()
                && state != IPS_BUSY)
            prepareActions[CaptureModuleState::ACTION_ROTATOR] = true;

        checkAllActionsReady();
    }
    else
    {
        setInitialized(CaptureModuleState::ACTION_ROTATOR, true);
        if (fabs(currentPositionAngle - targetPositionAngle) * 60 <= Options::astrometryRotatorThreshold()
                && state != IPS_BUSY)
        {
            prepareActions[CaptureModuleState::ACTION_ROTATOR] = true;
            checkAllActionsReady();
        }
        else
        {
            prepareRotatorCheck();
        }
    }
}

void SequenceJobState::setCurrentGuiderDrift(double value)
{
    setInitialized(CaptureModuleState::ACTION_GUIDER_DRIFT, true);
    if (value <= targetStartGuiderDrift)
        prepareActions[CaptureModuleState::ACTION_GUIDER_DRIFT] = true;

    checkAllActionsReady();
}

void SequenceJobState::setFocusStatus(FocusState state)
{
    switch (state)
    {
        case FOCUS_COMPLETE:
            // did we wait for a successful autofocus run?
            if (prepareActions[CaptureModuleState::ACTION_AUTOFOCUS] == false)
            {
                prepareActions[CaptureModuleState::ACTION_AUTOFOCUS] = true;
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
    // covering confirmed
    if (success == true)
    {
        if (closed)
            m_CaptureModuleState->m_ManualCoverState = light ? CaptureModuleState::MANUAL_COVER_CLOSED_LIGHT :
                    CaptureModuleState::MANUAL_COVER_CLOSED_DARK;
        else
            m_CaptureModuleState->m_ManualCoverState = CaptureModuleState::MANAUL_COVER_OPEN;
        coverQueryState = CAL_CHECK_TASK;
        // re-run checks
        checkAllActionsReady();
    }
    // cancelled
    else
    {
        m_CaptureModuleState->shutterStatus = CaptureModuleState::SHUTTER_UNKNOWN;
        coverQueryState = CAL_CHECK_TASK;
        // abort, no further checks
        emit abortCapture();
    }
}

void SequenceJobState::lightBoxLight(bool on)
{
    m_CaptureModuleState->setLightBoxLightState(on ? CaptureModuleState::CAP_LIGHT_ON : CaptureModuleState::CAP_LIGHT_OFF);
    emit newLog(i18n(on ? "Light box on." : "Light box off."));
    // re-run checks
    checkAllActionsReady();
}

void SequenceJobState::dustCapStateChanged(ISD::DustCap::Status status)
{
    switch (status)
    {
        case ISD::DustCap::CAP_ERROR:
            m_CaptureModuleState->setDustCapState(CaptureModuleState::CAP_ERROR);
            emit abortCapture();
            break;
        case ISD::DustCap::CAP_PARKED:
            m_CaptureModuleState->setDustCapState(CaptureModuleState::CAP_PARKED);
            emit newLog(i18n("Dust cap parked."));
            break;
        case ISD::DustCap::CAP_IDLE:
            m_CaptureModuleState->setDustCapState(CaptureModuleState::CAP_IDLE);
            emit newLog(i18n("Dust cap unparked."));
            break;
        case ISD::DustCap::CAP_UNPARKING:
            m_CaptureModuleState->setDustCapState(CaptureModuleState::CAP_UNPARKING);
            break;
        case ISD::DustCap::CAP_PARKING:
            m_CaptureModuleState->setDustCapState(CaptureModuleState::CAP_PARKING);
            break;
    }

    // re-run checks
    checkAllActionsReady();
}

void SequenceJobState::scopeStatusChanged(ISD::Mount::Status status)
{
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
        default:
            // do nothing
            break;
    }
    m_CaptureModuleState->setScopeState(status);
    // re-run checks
    checkAllActionsReady();
}

void SequenceJobState::scopeParkStatusChanged(ISD::ParkStatus status)
{
    m_CaptureModuleState->setScopeParkState(status);
    // re-run checks
    checkAllActionsReady();
}

void SequenceJobState::domeStatusChanged(ISD::Dome::Status status)
{
    m_CaptureModuleState->setDomeState(status);
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
        m_CaptureModuleState->shutterStatus = CaptureModuleState::SHUTTER_YES;
    else
        m_CaptureModuleState->shutterStatus = CaptureModuleState::SHUTTER_NO;

    // re-run checks
    checkAllActionsReady();
}

void SequenceJobState::setEnforceInitialGuidingDrift(bool enforceInitialGuidingDrift)
{
    m_enforceInitialGuiding = enforceInitialGuidingDrift;
    // update the preparation action
    prepareActions[CaptureModuleState::ACTION_GUIDER_DRIFT] = !enforceInitialGuidingDrift || m_isPreview;
    // re-run checks
    checkAllActionsReady();
}

SequenceJobState::PreparationState SequenceJobState::getPreparationState() const
{
    return m_PreparationState;
}

void SequenceJobState::setFilterStatus(FilterState filterState)
{
    switch (filterState)
    {
        case FILTER_AUTOFOCUS:
            // we need to wait until focusing has completed
            prepareActions[CaptureModuleState::ACTION_AUTOFOCUS] = false;
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
