/*
    SPDX-FileCopyrightText: 2025 Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QDateTime>

class HysteresisGuider
{
    public:
        HysteresisGuider(const QString &id);
        ~HysteresisGuider() {}

        void setGain(double gain)
        {
            if (gain >= 0 && gain <= 1.0)
                m_Gain = gain;
        }
        void setHysteresis(double hysteresis)
        {
            if (hysteresis >= 0 && hysteresis <= 1.0)
                m_Hysteresis = hysteresis;
        }
        void setMinMove(double minMove)
        {
            if (minMove >= 0)
                m_MinMove = minMove;
        }

        // Input is time in seconds and offset in arc-seconds.
        // Returns an arc-second correction 
        // (i.e. guide pulse should correct that many arcseconds)
        double guide(double offset);

        void reset();

    private:
        double m_Gain { 0.6 };
        double m_Hysteresis { 0.1 };
        double m_MinMove { 0.0 };

        QString m_ID;
        QDateTime m_LastGuideTime;
        int m_GuiderIteration { 0 };        
        double m_LastOutput { 0 };
};
