/*  Ekos state machine for a single capture job sequence
    SPDX-FileCopyrightText: Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "Options.h"
#include "sequencejobstate.h"

namespace Ekos
{
SequenceJobState::SequenceJobState() {
    // reset the initialization state
    QMutableMapIterator<PrepareActions, bool> it(isInitialized);

    while (it.hasNext())
    {
        it.next();
        it.setValue(false);
    }
}

void SequenceJobState::prepareCapture(CCDFrameType frameType, bool enforceCCDTemp, bool enforceStartGuiderDrift, bool isPreview)
{
    // precondition: do not start while already being busy and conditions haven't changed
    if (m_status == JOB_BUSY && enforceCCDTemp == m_enforceTemperature && enforceStartGuiderDrift == m_enforceStartGuiderDrift)
        return;

    m_status = JOB_BUSY;

    // Reset all prepare actions
    setAllActionsReady();

    // disable batch mode for previews
    emit setCCDBatchMode(!isPreview);

    // set the frame type
    m_frameType = frameType;
    // turn on CCD temperature enforcing if required
    m_enforceTemperature = enforceCCDTemp;
    // turn on enforcing guiding drift check if the guider is active
    m_enforceStartGuiderDrift = enforceStartGuiderDrift;

       // Filter changes are actually done in capture(), therefore prepareActions are always true
       prepareActions[ACTION_FILTER] = true;
       // nevertheless, emit an event so that Capture changes m_state
       if (targetFilterID != -1 && frameType == FRAME_LIGHT && targetFilterID != currentFilterID)
           emit prepareState(CAPTURE_CHANGING_FILTER);


    // Check if we need to update temperature (only skip if the value is initialized and within the limits)
    if (m_enforceTemperature && (fabs(targetTemperature - currentTemperature) > Options::maxTemperatureDiff() ||
                               isInitialized[ACTION_TEMPERATURE] == false))
    {
        prepareActions[ACTION_TEMPERATURE] = false;
        emit setCCDTemperature(targetTemperature);
        emit prepareState(CAPTURE_SETTING_TEMPERATURE);

        // trigger setting current value
        if (isInitialized[ACTION_TEMPERATURE] == false)
            emit readCurrentState(CAPTURE_SETTING_TEMPERATURE);
    }

    // Check if we need to wait for the guider to settle.
    if (!guiderDriftOkForStarting())
    {
        prepareActions[ACTION_GUIDER_DRIFT] = false;
        emit prepareState(CAPTURE_GUIDER_DRIFT);

        // trigger setting current value
        if (isInitialized[ACTION_GUIDER_DRIFT] == false)
            emit readCurrentState(CAPTURE_GUIDER_DRIFT);
    }

    // Check if we need to update rotator (only skip if the value is initialized and within the limits)
    if (targetRotation > Ekos::INVALID_VALUE &&
            (fabs(currentRotation - targetRotation) * 60 > Options::astrometryRotatorThreshold() ||
             isInitialized[ACTION_ROTATOR] == false))
    {
        prepareActions[ACTION_ROTATOR] = false;
        // PA = RawAngle * Multiplier + Offset
        double rawAngle = (targetRotation - Options::pAOffset()) / Options::pAMultiplier();
        emit prepareState(CAPTURE_SETTING_ROTATOR);
        emit setRotatorAngle(&rawAngle);

        // trigger setting current value
        if (isInitialized[ACTION_ROTATOR] == false)
            emit readCurrentState(CAPTURE_SETTING_ROTATOR);
    }
    // Hint: Filter changes are actually done in SequenceJob::capture();

    // Set the flag that the preparation has been initialized
    prepareReady = false;
    // check if the preparations are already completed
    checkAllActionsReady();
}

bool SequenceJobState::guiderDriftOkForStarting()
{
    return (!m_enforceStartGuiderDrift ||
            m_frameType != FRAME_LIGHT ||
            (currentGuiderDrift <= targetStartGuiderDrift && isInitialized[ACTION_GUIDER_DRIFT] == true));
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
    if (prepareReady == false && areActionsReady())
    {
        prepareReady = true;
        emit prepareComplete();
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
}

void SequenceJobState::setCurrentFilterID(int value)
{
    currentFilterID = value;
    isInitialized[ACTION_FILTER] = true;

    if (currentFilterID == targetFilterID)
        prepareActions[SequenceJobState::ACTION_FILTER] = true;

    checkAllActionsReady();
}

void SequenceJobState::setCurrentCCDTemperature(double value)
{
    currentTemperature = value;
    isInitialized[ACTION_TEMPERATURE] = true;

    if (m_enforceTemperature == false || fabs(targetTemperature - currentTemperature) <= Options::maxTemperatureDiff())
        prepareActions[SequenceJobState::ACTION_TEMPERATURE] = true;

    checkAllActionsReady();
}

void SequenceJobState::setCurrentRotatorAngle(double value)
{
    currentRotation = value;
    isInitialized[ACTION_ROTATOR] = true;

    if (fabs(currentRotation - targetRotation) * 60 <= Options::astrometryRotatorThreshold())
        prepareActions[SequenceJobState::ACTION_ROTATOR] = true;

    checkAllActionsReady();
}

void SequenceJobState::setCurrentGuiderDrift(double value)
{
    currentGuiderDrift = value;
    isInitialized[ACTION_GUIDER_DRIFT] = true;
    prepareActions[ACTION_GUIDER_DRIFT] = guiderDriftOkForStarting();

    checkAllActionsReady();
}
} // namespace
