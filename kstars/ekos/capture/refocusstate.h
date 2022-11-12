/*  Ekos state machine for refocusing
    SPDX-FileCopyrightText: 2022 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QObject>
#include <QElapsedTimer>

#include "Options.h"

namespace Ekos
{

class RefocusState : public QObject
{
    Q_OBJECT
public:
    explicit RefocusState(QObject *parent = nullptr): QObject{parent} {}

    const QElapsedTimer &getRefocusEveryNTimer() const { return m_refocusEveryNTimer; }
    qint64 restartRefocusEveryNTimer() { return m_refocusEveryNTimer.restart(); }

    int getRefocusEveryNTimerElapsedSec();

    int getRefocusEveryNMinutes() const { return m_refocusEveryNMinutes; }
    void setRefocusEveryNMinutes(int value) { m_refocusEveryNMinutes = value; }

    /**
     * @brief Is the temperature or HFR based autofocus ready to start? This flag ensures that
     *        focusing has run at least once before the autofocus is triggered by the configured parameters.
     * @return true if focusing has been executed once
     */
    bool isAutoFocusReady() const { return m_AutoFocusReady; }
    void setAutoFocusReady(bool value) { m_AutoFocusReady = value; }

    bool isInSequenceFocus() const { return m_InSequenceFocus; }
    void setInSequenceFocus(bool value) { m_InSequenceFocus = value; }

    uint getInSequenceFocusCounter() const { return inSequenceFocusCounter; }
    void decreaseInSequenceFocusCounter();
    void setInSequenceFocusCounter(uint value) {  inSequenceFocusCounter = value; }
    void resetInSequenceFocusCounter() { inSequenceFocusCounter = Options::inSequenceCheckFrames(); }

    bool isTemperatureDeltaCheckActive() const { return m_temperatureDeltaCheckActive; }
    void setTemperatureDeltaCheckActive(bool value) { m_temperatureDeltaCheckActive = value; }

    double getFocusTemperatureDelta() const { return m_focusTemperatureDelta; }
    void setFocusTemperatureDelta(double value) { m_focusTemperatureDelta = value; }

    bool isRefocusAfterMeridianFlip() const { return m_refocusAfterMeridianFlip; }
    void setRefocusAfterMeridianFlip(bool value) { m_refocusAfterMeridianFlip = value; }

    bool isRefocusing() const { return m_refocusing; }
    void setRefocusing(bool value) { m_refocusing = value; }

private:
    // number of minutes between forced refocus
    int m_refocusEveryNMinutes { 0 };
    // used to determine when next force refocus should occur
    QElapsedTimer m_refocusEveryNTimer;
    // Ready for running autofocus (HFR or temperature based)
    bool m_AutoFocusReady { false };
    // focusing during the capture sequence
    bool m_InSequenceFocus { false };
    // counter how many captures to be executed until focusing is triggered
    uint inSequenceFocusCounter { 0 };
    // Focus on temperature change
    bool m_temperatureDeltaCheckActive { false };
    // Temperature change since last focusing
    double m_focusTemperatureDelta { 0 };
    // set to true at meridian flip to request refocus
    bool m_refocusAfterMeridianFlip { false };


    // Refocusing running
    bool m_refocusing { false };

};

}; // namespace
