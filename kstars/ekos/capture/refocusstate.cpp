/*  Ekos state machine for refocusing
    SPDX-FileCopyrightText: 2022 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "refocusstate.h"
#include "klocalizedstring.h"

#include <ekos_capture_debug.h>

namespace Ekos
{

int RefocusState::getRefocusEveryNTimerElapsedSec()
{
    /* If timer isn't valid, consider there is no focus to be done, that is, that focus was just done */
    return m_refocusEveryNTimer.isValid() ? static_cast<int>(m_refocusEveryNTimer.elapsed() / 1000) : 0;
}

RefocusState::RefocusReason RefocusState::checkFocusRequired()
{
    setRefocusing(false);
    setInSequenceFocus(isAutoFocusReady() && Options::enforceAutofocusHFR());

    // 1. check if time limit based refocusing is necessary
    if (Options::enforceRefocusEveryN())
    {
        qCDebug(KSTARS_EKOS_CAPTURE) << "Focus elapsed time (secs): " << getRefocusEveryNTimerElapsedSec() <<
                                     ". Requested Interval (secs): " << Options::refocusEveryN() * 60;

        if (getRefocusEveryNTimerElapsedSec() >= Options::refocusEveryN() * 60)
        {
            setRefocusing(true);
            appendLogText(i18n("Scheduled refocus starting after %1 seconds...", getRefocusEveryNTimerElapsedSec()));
            return REFOCUS_TIME_ELAPSED;
        }
    }

    // 2. check if temperature based refocusing is necessary
    if (!isRefocusing() && Options::enforceAutofocusOnTemperature())
    {
        qCDebug(KSTARS_EKOS_CAPTURE) << "Focus temperature delta (°C): " << getFocusTemperatureDelta() <<
                                     ". Requested maximum delta (°C): " << Options::maxFocusTemperatureDelta();

        if (getFocusTemperatureDelta() > Options::maxFocusTemperatureDelta())
        {
            setRefocusing(true);
            appendLogText(i18n("Refocus starting because of temperature change of %1 °C...", getFocusTemperatureDelta()));
            return REFOCUS_TEMPERATURE;
        }
    }

    // 3. check if HFR based in sequence focusing is necessary
    if (!isRefocusing() && isInSequenceFocus() && getInSequenceFocusCounter() == 0)
    {
        setRefocusing(true);
        appendLogText(i18n("In sequence HFR based refocus starting..."));
        return REFOCUS_HFR;
    }

    // 4. check if post meridian flip refocusing is necessary
    if (!isRefocusing() && isRefocusAfterMeridianFlip())
    {
        setRefocusing(true);
        appendLogText(i18n("Refocus after meridian flip"));
        return REFOCUS_POST_MF;
    }

    // 5. no full refocus required so check if adaptive focus is necessary - no need to do both
    if (!isRefocusing() && Options::focusAdaptive() && !isAdaptiveFocusDone())
    {
        setRefocusing(true);
        appendLogText(i18n("Adaptive focus starting..."));
        return REFOCUS_ADAPTIVE;
    }
    // no refocusing necessary
    return REFOCUS_NONE;
}

void RefocusState::startRefocusTimer(bool forced)
{
    /* If refocus is requested, only restart timer if not already running in order to keep current elapsed time since last refocus */
    if (Options::enforceRefocusEveryN())
    {
        // How much time passed since we last started the time
        long elapsedSecs = getRefocusEveryNTimer().elapsed() / 1000;
        // How many seconds do we wait for between focusing (60 mins ==> 3600 secs)
        int totalSecs   = Options::refocusEveryN() * 60;

        if (!getRefocusEveryNTimer().isValid() || forced)
        {
            appendLogText(i18n("Ekos will refocus in %1 seconds.", totalSecs));
            restartRefocusEveryNTimer();
        }
        else if (elapsedSecs < totalSecs)
        {
            appendLogText(i18n("Ekos will refocus in %1 seconds, last procedure was %2 seconds ago.", totalSecs - elapsedSecs,
                               elapsedSecs));
        }
        else
        {
            appendLogText(i18n("Ekos will refocus as soon as possible, last procedure was %1 seconds ago.", elapsedSecs));
        }
    }
}

void RefocusState::decreaseInSequenceFocusCounter()
{
    if (inSequenceFocusCounter > 0)
        --inSequenceFocusCounter;
}

void RefocusState::addHFRValue(const QString &filter)
{
    QList<double> filterHFRList = m_HFRMap[filter];
    filterHFRList.append(m_focusHFR);
    m_HFRMap[filter] = filterHFRList;
}





void RefocusState::appendLogText(const QString &message)
{
    qCInfo(KSTARS_EKOS_CAPTURE()) << message;
    emit newLog(message);
}


} // namespace
