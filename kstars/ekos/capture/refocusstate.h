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
        typedef enum
        {
            REFOCUS_NONE,         /* no need to refocus                                 */
            REFOCUS_HFR,          /* refocusing due to HFR (in sequence) value          */
            REFOCUS_ADAPTIVE,     /* adaptive refocusing (in sequence)                  */
            REFOCUS_TEMPERATURE,  /* refocusing due to temperature change               */
            REFOCUS_TIME_ELAPSED, /* refocusing due to elapsed time since last focusing */
            REFOCUS_POST_MF       /* refocusing after a completed meridian flip         */
        } RefocusReason;

        explicit RefocusState(QObject *parent = nullptr): QObject{parent} {}

        /**
         * @brief Check if focusing is necessary:
         * 1. time limit based
         * 2. temperature based
         * 3. HFR based in sequence
         * 4. post meridian flip         *
         */
        RefocusReason checkFocusRequired();

        /**
         * @brief Start the timer triggering refosing after the configured time.
         * @param forced if true restart the timer even if it is already running
         */
        void startRefocusTimer(bool forced = false);

        const QElapsedTimer &getRefocusEveryNTimer() const
        {
            return m_refocusEveryNTimer;
        }
        qint64 restartRefocusEveryNTimer()
        {
            return m_refocusEveryNTimer.restart();
        }

        int getRefocusEveryNTimerElapsedSec();

        double getFocusHFR() const
        {
            return m_focusHFR;
        }
        void setFocusHFR(double newFocusHFR, bool inAutofocus)
        {
            m_focusHFR = newFocusHFR;
            m_focusHFRInAutofocus = inAutofocus;
        }
        bool getFocusHFRInAutofocus() const
        {
            return m_focusHFRInAutofocus;
        }

        /**
         * @brief Is the temperature or HFR based autofocus ready to start? This flag ensures that
         *        focusing has run at least once before the autofocus is triggered by the configured parameters.
         * @return true if focusing has been executed once
         */
        bool isAutoFocusReady() const
        {
            return m_AutoFocusReady;
        }
        void setAutoFocusReady(bool value)
        {
            m_AutoFocusReady = value;
        }

        bool isInSequenceFocus() const
        {
            return m_InSequenceFocus;
        }
        void setInSequenceFocus(bool value)
        {
            m_InSequenceFocus = value;
        }

        bool isAdaptiveFocusDone() const
        {
            return m_AdaptiveFocusDone;
        }
        void setAdaptiveFocusDone(bool value)
        {
            m_AdaptiveFocusDone = value;
        }

        uint getInSequenceFocusCounter() const
        {
            return inSequenceFocusCounter;
        }
        void decreaseInSequenceFocusCounter();
        void setInSequenceFocusCounter(uint value)
        {
            inSequenceFocusCounter = value;
        }
        void resetInSequenceFocusCounter()
        {
            inSequenceFocusCounter = Options::inSequenceCheckFrames();
        }

        double getFocusTemperatureDelta() const
        {
            return m_focusTemperatureDelta;
        }
        void setFocusTemperatureDelta(double value)
        {
            m_focusTemperatureDelta = value;
        }

        bool isRefocusAfterMeridianFlip() const
        {
            return m_refocusAfterMeridianFlip;
        }
        void setRefocusAfterMeridianFlip(bool value)
        {
            m_refocusAfterMeridianFlip = value;
        }

        bool isRefocusing() const
        {
            return m_refocusing;
        }
        void setRefocusing(bool value)
        {
            m_refocusing = value;
        }

        const QMap<QString, QList<double> > &getHFRMap() const
        {
            return m_HFRMap;
        }
        void setHFRMap(const QMap<QString, QList<double>> &newHFRMap)
        {
            m_HFRMap = newHFRMap;
        }

        /**
         * @brief Add the current HFR value measured for a frame with given filter
         */
        void addHFRValue(const QString &filter);

    signals:
        // new log text for the module log window
        void newLog(const QString &text);


    private:
        // HFR value as received from the Ekos focus module
        double m_focusHFR { 0 };
        // Whether m_focusHFR was taken during Autofocus
        bool m_focusHFRInAutofocus { false };
        // used to determine when next force refocus should occur
        QElapsedTimer m_refocusEveryNTimer;
        // Ready for running autofocus (HFR or temperature based)
        bool m_AutoFocusReady { false };
        // focusing during the capture sequence
        bool m_InSequenceFocus { false };
        // adaptive focusing complete
        bool m_AdaptiveFocusDone { false };
        // counter how many captures to be executed until focusing is triggered
        uint inSequenceFocusCounter { 0 };
        // Temperature change since last focusing
        double m_focusTemperatureDelta { 0 };
        // set to true at meridian flip to request refocus
        bool m_refocusAfterMeridianFlip { false };
        // map filter name --> list of HFR values
        QMap<QString, QList<double>> m_HFRMap;


        // Refocusing running
        bool m_refocusing { false };


        /**
         * @brief Add log message
         */
        void appendLogText(const QString &message);

};

}; // namespace
