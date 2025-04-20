/*
    SPDX-FileCopyrightText: 2025 Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QObject>
#include <QQueue>
#include <QDateTime>

class LinearGuider
{

    public:
        LinearGuider(const QString &id);
        ~LinearGuider() {}

        struct Sample
        {
            double time;
            double offset;
            Sample(double t, double o) : time(t), offset(o) {}
        };

        void setLength(int length)
        {
            if (length > 0)
                m_Length = length;
        }
        void setGain(double gain)
        {
            if (gain >= 0 && gain <= 1.0)
                m_Gain = gain;
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
        double getSlope();
        void removeLastSample();
        void updateStats(const Sample &sample);
        void addSample(const Sample &sample);

        double m_Gain { 0.6 };
        double m_MinMove { 0.0 };

        QQueue<Sample> m_Samples;
        int m_Length = 10;
        double m_SumTime { 0 };
        double m_SumOffset { 0 };
        double m_SumTimeSq { 0 };
        double m_OffsetSq { 0 };
        double m_SumTimeOffset { 0 };
        int m_NumRejects { 0 };
        QString m_ID;
        QDateTime m_LastGuideTime;
        int m_GuiderIteration { 0 };
};
