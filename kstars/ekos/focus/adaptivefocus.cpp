/*
    SPDX-FileCopyrightText: 2023 John Evans <john.e.evans.email@googlemail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "adaptivefocus.h"
#include <kstars_debug.h>
#include "kstars.h"
#include "Options.h"

namespace Ekos
{

AdaptiveFocus::AdaptiveFocus(Focus *_focus) : m_focus(_focus)
{
    if (m_focus == nullptr)
        qCDebug(KSTARS_EKOS_FOCUS) << "AdaptiveFocus constructed with null focus ptr";
}

AdaptiveFocus::~AdaptiveFocus()
{
}

// Start an Adaptive Focus run
void AdaptiveFocus::runAdaptiveFocus(const int currentPosition, const QString &filter)
{
    if (!m_focus)
    {
        qCDebug(KSTARS_EKOS_FOCUS) << "runAdaptiveFocus called but focus ptr is null. Ignoring.";
        adaptiveFocusAdmin(currentPosition, false, false);
        return;
    }

    if (!m_focus->m_FilterManager)
    {
        qCDebug(KSTARS_EKOS_FOCUS) << "runAdaptiveFocus called but focus filterManager is null. Ignoring.";
        adaptiveFocusAdmin(currentPosition, false, false);
        return;
    }

    if (!m_focus->m_OpsFocusSettings->focusAdaptive->isChecked())
    {
        qCDebug(KSTARS_EKOS_FOCUS) << "runAdaptiveFocus called but focusAdaptive->isChecked is false. Ignoring.";
        adaptiveFocusAdmin(currentPosition, false, false);
        return;
    }

    if (inAdaptiveFocus())
    {
        qCDebug(KSTARS_EKOS_FOCUS) << "runAdaptiveFocus called whilst already inAdaptiveFocus. Ignoring.";
        adaptiveFocusAdmin(currentPosition, false, false);
        return;
    }

    if (m_focus->inAutoFocus || m_focus->inFocusLoop || m_focus->inAdjustFocus || m_focus->inBuildOffsets || m_focus->inAFOptimise)
    {
        qCDebug(KSTARS_EKOS_FOCUS) << "adaptiveFocus called whilst other focus activity in progress. Ignoring.";
        adaptiveFocusAdmin(currentPosition, false, false);
        return;
    }

    setInAdaptiveFocus(true);
    m_ThisAdaptiveFocusStartPos = currentPosition;

    // Get the reference data to be used by Adaptive Focus
    int refPosition;
    QString adaptiveFilter = getAdaptiveFilter(filter);

    // If the filter has been changed since the last Adaptive Focus iteration, reset
    if (filter != m_LastAdaptiveFilter)
    {
        resetAdaptiveFocusCounters();
        m_LastAdaptiveFilter = filter;
    }
    if (!m_focus->m_FilterManager->getFilterAbsoluteFocusDetails(adaptiveFilter, refPosition,
            m_focus->m_LastSourceAutofocusTemperature,
            m_focus->m_LastSourceAutofocusAlt))
    {
        qCDebug(KSTARS_EKOS_FOCUS) << "runAdaptiveFocus unable to get last Autofocus details. Ignoring.";
        adaptiveFocusAdmin(currentPosition, false, false);
        return;
    }

    // If we are using a lock filter then adjust the reference position by the offset
    refPosition += getAdaptiveFilterOffset(filter, adaptiveFilter);

    // Find out if there is anything to do for temperature, firstly do we have a valid temperature source
    double currentTemp = INVALID_VALUE;
    double tempTicksDelta = 0.0, tempTicksDeltaLast = 0.0;
    if (m_focus->currentTemperatureSourceElement && m_focus->m_LastSourceAutofocusTemperature != INVALID_VALUE)
    {
        if (m_LastAdaptiveFocusTemperature == INVALID_VALUE)
            // 1st Adaptive Focus run so no previous results
            m_LastAdaptiveFocusTemperature = m_focus->m_LastSourceAutofocusTemperature;

        currentTemp = m_focus->currentTemperatureSourceElement->value;

        // Calculate the deltas since the last Autofocus and Last Adaptive Focus
        const double tempDelta = currentTemp - m_focus->m_LastSourceAutofocusTemperature;
        const double tempDeltaLast = m_LastAdaptiveFocusTemperature - m_focus->m_LastSourceAutofocusTemperature;

        // Scale the temperature delta to number of ticks
        const double ticksPerTemp = m_focus->m_FilterManager->getFilterTicksPerTemp(adaptiveFilter);
        tempTicksDelta = ticksPerTemp * tempDelta;
        tempTicksDeltaLast = ticksPerTemp * tempDeltaLast;
    }

    // Now check for altitude
    double currentAlt = INVALID_VALUE;
    double altTicksDelta = 0.0, altTicksDeltaLast = 0.0;
    bool altDimension = false;
    if (m_focus->m_LastSourceAutofocusAlt != INVALID_VALUE)
    {
        if (m_LastAdaptiveFocusAlt == INVALID_VALUE)
            // 1st Adaptive Focus run so no previous results
            m_LastAdaptiveFocusAlt = m_focus->m_LastSourceAutofocusAlt;

        currentAlt = m_focus->mountAlt;

        const double altDelta = currentAlt - m_focus->m_LastSourceAutofocusAlt;
        const double altDeltaLast = m_LastAdaptiveFocusAlt - m_focus->m_LastSourceAutofocusAlt;

        // Scale the altitude delta to number of ticks
        const double ticksPerAlt = m_focus->m_FilterManager->getFilterTicksPerAlt(adaptiveFilter);
        altDimension = (abs(ticksPerAlt) > 0.001);
        altTicksDelta = ticksPerAlt * altDelta;
        altTicksDeltaLast = ticksPerAlt * altDeltaLast;
    }

    // proposedPosition is where focuser should move; proposedPositionLast is where focuser should have moved in last Adaptive iterative
    int proposedPosition = refPosition + static_cast<int>(round(tempTicksDelta + altTicksDelta));
    int proposedPositionLast = refPosition + static_cast<int>(round(tempTicksDeltaLast + altTicksDeltaLast));
    int proposedMove = proposedPosition - currentPosition;

    // We have the total movement, now work out the split by Temp, Alt and Pos Error
    m_ThisAdaptiveFocusTempTicks = tempTicksDelta - tempTicksDeltaLast;
    m_ThisAdaptiveFocusAltTicks = altTicksDelta - altTicksDeltaLast;

    // If everything is going to plan the currentPosition will equal proposedPositionLast. If the focuser hasn't moved exactly to the
    // requested position, e.g. its 1 or 2 ticks away then we need to account for this Positioning Error. It will have been reported
    // on the last Adaptive run but we now need to reverse that positioning error out in the accounting for this run
    m_LastAdaptiveFocusPosErrorReversal = proposedPositionLast - currentPosition;

    // There could be a rounding error, where, for example, a small change in temp/alt (e.g. 0.1 ticks) results in a 1 tick move
    m_ThisAdaptiveFocusRoundingError = proposedMove - m_LastAdaptiveFocusPosErrorReversal -
                                       static_cast<int>(round(m_ThisAdaptiveFocusTempTicks + m_ThisAdaptiveFocusAltTicks));

    // Check movement is above user defined minimum
    if (abs(proposedMove) < m_focus->m_OpsFocusSettings->focusAdaptiveMinMove->value())
    {
        m_focus->appendLogText(i18n("Adaptive Focus: No movement (below threshold)"));
        adaptiveFocusAdmin(currentPosition, true, false);
    }
    else
    {
        // Now do some checks that the movement is permitted
        if (abs(m_focus->initialFocuserAbsPosition - proposedPosition) > m_focus->m_OpsFocusMechanics->focusMaxTravel->value())
        {
            // We are about to move the focuser beyond focus max travel so don't
            // Suspend adaptive focusing, user can always re-enable, if required
            m_focus->m_OpsFocusSettings->focusAdaptive->setChecked(false);
            m_focus->appendLogText(i18n("Adaptive Focus suspended. Total movement would exceed Max Travel limit"));
            adaptiveFocusAdmin(currentPosition, false, false);
        }
        else if (abs(m_AdaptiveTotalMove + proposedMove) > m_focus->m_OpsFocusSettings->focusAdaptiveMaxMove->value())
        {
            // We are about to move the focuser beyond adaptive focus max move so don't
            // Suspend adaptive focusing. User can always re-enable, if required
            m_focus->m_OpsFocusSettings->focusAdaptive->setChecked(false);
            m_focus->appendLogText(i18n("Adaptive Focus suspended. Total movement would exceed adaptive limit"));
            adaptiveFocusAdmin(currentPosition, false, false);
        }
        else
        {
            // For most folks, most of the time there won't be alt data or positioning errors, so don't continually report 0
            QString tempStr = QString("%1").arg(m_ThisAdaptiveFocusTempTicks, 0, 'f', 1);
            QString altStr = QString("%1").arg(m_ThisAdaptiveFocusAltTicks, 0, 'f', 1);
            QString text = i18n("Adaptive Focus: Moving from %1 to %2 (TempΔ %3", currentPosition, proposedPosition, tempStr);
            if (altDimension)
                text += i18n("; AltΔ %1", altStr);
            text += (m_LastAdaptiveFocusPosErrorReversal == 0) ? ")" : i18n("; Pos Error %1)", m_LastAdaptiveFocusPosErrorReversal);
            m_focus->appendLogText(text);

            // Go ahead and try to move the focuser. Admin tasks will be completed when the focuser move completes
            if (m_focus->changeFocus(proposedMove))
            {
                // All good so update variables used after focuser move completes (see adaptiveFocusAdmin())
                m_ThisAdaptiveFocusTemperature = currentTemp;
                m_ThisAdaptiveFocusAlt = currentAlt;
                m_AdaptiveFocusPositionReq = proposedPosition;
                m_AdaptiveTotalMove += proposedMove;
            }
            else
            {
                // Problem moving the focuser
                m_focus->appendLogText(i18n("Adaptive Focus unable to move focuser"));
                adaptiveFocusAdmin(currentPosition, false, false);
            }
        }
    }
}

// When adaptiveFocus succeeds the focuser is moved and admin tasks are performed to inform other modules that adaptiveFocus is complete
void AdaptiveFocus::adaptiveFocusAdmin(const int currentPosition, const bool success, const bool focuserMoved)
{
    // Reset member variable
    setInAdaptiveFocus(false);

    int thisPosError = 0;
    if (focuserMoved)
    {
        // Signal Capture that we are done - honour the focuser settle time after movement.
        QTimer::singleShot(m_focus->m_OpsFocusMechanics->focusSettleTime->value() * 1000, m_focus, [ &, success]()
        {
            emit m_focus->focusAdaptiveComplete(success, m_focus->opticalTrain());
        });

        // Check whether the focuser moved to the requested position or whether we have a positioning error (1 or 2 ticks for example)
        // If there is a positioning error update the total ticks to include the error.
        if (m_AdaptiveFocusPositionReq != INVALID_VALUE)
        {
            thisPosError = currentPosition - m_AdaptiveFocusPositionReq;
            m_AdaptiveTotalMove += thisPosError;
        }
    }
    else
        emit m_focus->focusAdaptiveComplete(success, m_focus->opticalTrain());

    // Signal Analyze if success both for focuser moves and zero moves
    bool check = true;
    if (success)
    {
        // Note we send Analyze the active filter, not the Adaptive Filter (i.e. not the lock filter)
        int totalTicks = static_cast<int>(round(m_ThisAdaptiveFocusTempTicks + m_ThisAdaptiveFocusAltTicks)) +
                         m_LastAdaptiveFocusPosErrorReversal + m_ThisAdaptiveFocusRoundingError + thisPosError;

        emit m_focus->adaptiveFocusComplete(m_focus->filter(), m_ThisAdaptiveFocusTemperature, m_ThisAdaptiveFocusTempTicks,
                                            m_ThisAdaptiveFocusAlt, m_ThisAdaptiveFocusAltTicks, m_LastAdaptiveFocusPosErrorReversal,
                                            thisPosError, totalTicks, currentPosition, focuserMoved);

        // Check that totalTicks movement is above minimum
        if (totalTicks < m_focus->m_OpsFocusSettings->focusAdaptiveMinMove->value())
            totalTicks = 0;
        // Perform an accounting check that the numbers add up
        check = (m_ThisAdaptiveFocusStartPos + totalTicks == currentPosition);
    }

    if (success && focuserMoved)
    {
        // Reset member variables in prep for the next Adaptive Focus run
        m_LastAdaptiveFocusTemperature = m_ThisAdaptiveFocusTemperature;
        m_LastAdaptiveFocusAlt = m_ThisAdaptiveFocusAlt;
    }

    // Log a debug for each Adaptive Focus.
    qCDebug(KSTARS_EKOS_FOCUS) << "Adaptive Focus Stats: Filter:" << m_LastAdaptiveFilter
                               << ", Adaptive Filter:" << getAdaptiveFilter(m_LastAdaptiveFilter)
                               << ", Min Move:" << m_focus->m_OpsFocusSettings->focusAdaptiveMinMove->value()
                               << ", success:" << (success ? "Yes" : "No")
                               << ", focuserMoved:" << (focuserMoved ? "Yes" : "No")
                               << ", Temp:" << m_ThisAdaptiveFocusTemperature
                               << ", Temp ticks:" << m_ThisAdaptiveFocusTempTicks
                               << ", Alt:" << m_ThisAdaptiveFocusAlt
                               << ", Alt ticks:" << m_ThisAdaptiveFocusAltTicks
                               << ", Last Pos Error reversal:" << m_LastAdaptiveFocusPosErrorReversal
                               << ", Rounding Error:" << m_ThisAdaptiveFocusRoundingError
                               << ", New Pos Error:" << thisPosError
                               << ", Starting Pos:" << m_ThisAdaptiveFocusStartPos
                               << ", Current Position:" << currentPosition
                               << ", Accounting Check:" << (check ? "Passed" : "Failed");
}

// Get the filter to use for Adaptive Focus
QString AdaptiveFocus::getAdaptiveFilter(const QString filter)
{
    if (!m_focus->m_FilterManager)
        return QString();

    // If the active filter has a lock filter use that
    QString adaptiveFilter = filter;
    QString lockFilter = m_focus->m_FilterManager->getFilterLock(filter);
    if (lockFilter != NULL_FILTER)
        adaptiveFilter = lockFilter;

    return adaptiveFilter;
}

// Calc the filter offset between the active filter and the adaptive filter (either active filter or lock filter)
int AdaptiveFocus::getAdaptiveFilterOffset(const QString &activeFilter, const QString &adaptiveFilter)
{
    int offset = 0;
    if (!m_focus->m_FilterManager)
        return offset;

    if (activeFilter != adaptiveFilter)
    {
        int activeOffset = m_focus->m_FilterManager->getFilterOffset(activeFilter);
        int adaptiveOffset = m_focus->m_FilterManager->getFilterOffset(adaptiveFilter);
        if (activeOffset != INVALID_VALUE && adaptiveOffset != INVALID_VALUE)
            offset = activeOffset - adaptiveOffset;
        else
            qCDebug(KSTARS_EKOS_FOCUS) << "getAdaptiveFilterOffset unable to calculate filter offsets";
    }
    return offset;
}

// Reset the variables used by Adaptive Focus
void AdaptiveFocus::resetAdaptiveFocusCounters()
{
    m_LastAdaptiveFilter = NULL_FILTER;
    m_LastAdaptiveFocusTemperature = INVALID_VALUE;
    m_LastAdaptiveFocusAlt = INVALID_VALUE;
    m_AdaptiveTotalMove = 0;
}

// Function to set inAdaptiveFocus
void AdaptiveFocus::setInAdaptiveFocus(bool value)
{
    m_inAdaptiveFocus = value;
}

// Change the start position of an autofocus run based Adaptive Focus settings
// The start position uses the last successful AF run for the active filter and adapts that position
// based on the temperature and altitude delta between now and when the last successful AF run happened
int AdaptiveFocus::adaptStartPosition(int currentPosition, QString &AFfilter)
{
    // If the active filter has no lock then the AF run will happen on this filter so get the start point
    // Otherwise get the lock filter on which the AF run will happen and get the start point of this filter
    // An exception is when the BuildOffsets utility is being used as this ignores the lock filter
    if (!m_focus->m_FilterManager)
        return currentPosition;

    QString filterText;
    QString lockFilter = m_focus->m_FilterManager->getFilterLock(AFfilter);
    if (m_focus->inBuildOffsets || lockFilter == NULL_FILTER || lockFilter == AFfilter)
        filterText = AFfilter;
    else
    {
        filterText = AFfilter + " locked to " + lockFilter;
        AFfilter = lockFilter;
    }

    if (!m_focus->m_OpsFocusSettings->focusAdaptStart->isChecked())
        // Adapt start option disabled
        return currentPosition;

    if (m_focus->m_FocusAlgorithm != Focus::FOCUS_LINEAR1PASS)
        // Only enabled for LINEAR 1 PASS
        return currentPosition;

    // Start with the last AF run result for the active filter
    int lastPos;
    double lastTemp, lastAlt;
    if(!m_focus->m_FilterManager->getFilterAbsoluteFocusDetails(AFfilter, lastPos, lastTemp, lastAlt))
        // Unable to get the last AF run information for the filter so just use the currentPosition
        return currentPosition;

    // Only proceed if we have a sensible lastPos
    if (lastPos <= 0)
        return currentPosition;

    // Do some checks on the lastPos
    int minTravelLimit = qMax(0.0, currentPosition - m_focus->m_OpsFocusMechanics->focusMaxTravel->value());
    int maxTravelLimit = qMin(m_focus->absMotionMax, currentPosition + m_focus->m_OpsFocusMechanics->focusMaxTravel->value());
    if (lastPos < minTravelLimit || lastPos > maxTravelLimit)
    {
        // Looks like there is a potentially dodgy lastPos so just use currentPosition
        m_focus->appendLogText(i18n("Adaptive start point, last AF solution outside Max Travel, ignoring"));
        return currentPosition;
    }

    // Adjust temperature
    double ticksTemp = 0.0;
    double tempDelta = 0.0;
    if (!m_focus->currentTemperatureSourceElement)
        m_focus->appendLogText(i18n("Adaptive start point, no temperature source available"));
    else if (lastTemp == INVALID_VALUE)
        m_focus->appendLogText(i18n("Adaptive start point, no temperature for last AF solution"));
    else
    {
        double currentTemp = m_focus->currentTemperatureSourceElement->value;
        tempDelta = currentTemp - lastTemp;
        if (abs(tempDelta) > 30)
            // Sanity check on the temperature delta
            m_focus->appendLogText(i18n("Adaptive start point, very large temperature delta, ignoring"));
        else
            ticksTemp = tempDelta * m_focus->m_FilterManager->getFilterTicksPerTemp(AFfilter);
    }

    // Adjust altitude
    double ticksAlt = 0.0;
    double currentAlt = m_focus->mountAlt;
    double altDelta = currentAlt - lastAlt;

    // Sanity check on the altitude delta
    if (lastAlt == INVALID_VALUE)
        m_focus->appendLogText(i18n("Adaptive start point, no alt recorded for last AF solution"));
    else if (abs(altDelta) > 90.0)
        m_focus->appendLogText(i18n("Adaptive start point, very large altitude delta, ignoring"));
    else
        ticksAlt = altDelta * m_focus->m_FilterManager->getFilterTicksPerAlt(AFfilter);

    // We have all the elements to adjust the AF start position so final checks before the move
    const int ticksTotal = static_cast<int> (round(ticksTemp + ticksAlt));
    int targetPos = lastPos + ticksTotal;
    if (targetPos < minTravelLimit || targetPos > maxTravelLimit)
    {
        // targetPos is outside Max Travel
        m_focus->appendLogText(i18n("Adaptive start point, target position is outside Max Travel, ignoring"));
        return currentPosition;
    }

    if (abs(targetPos - currentPosition) > m_focus->m_OpsFocusSettings->focusAdaptiveMaxMove->value())
    {
        // Disallow excessive movement.
        // No need to check minimum movement
        m_focus->appendLogText(i18n("Adaptive start point [%1] excessive move disallowed", filterText));
        qCDebug(KSTARS_EKOS_FOCUS) << "Adaptive start point: " << filterText
                                   << " startPosition: " << currentPosition
                                   << " Last filter position: " << lastPos
                                   << " Temp delta: " << tempDelta << " Temp ticks: " << ticksTemp
                                   << " Alt delta: " << altDelta << " Alt ticks: " << ticksAlt
                                   << " Target position: " << targetPos
                                   << " Exceeds max allowed move: " << m_focus->m_OpsFocusSettings->focusAdaptiveMaxMove->value()
                                   << " Using startPosition.";
        return currentPosition;
    }
    else
    {
        // All good so report the move
        m_focus->appendLogText(i18n("Adapting start point [%1] from %2 to %3", filterText, currentPosition, targetPos));
        qCDebug(KSTARS_EKOS_FOCUS) << "Adaptive start point: " << filterText
                                   << " startPosition: " << currentPosition
                                   << " Last filter position: " << lastPos
                                   << " Temp delta: " << tempDelta << " Temp ticks: " << ticksTemp
                                   << " Alt delta: " << altDelta << " Alt ticks: " << ticksAlt
                                   << " Target position: " << targetPos;
        return targetPos;
    }
}

}
