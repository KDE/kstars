/*
    SPDX-FileCopyrightText: 2026 Pavan <pk6122004@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later

    Filter and statistics classes adapted from PHD2's guiding_stats.cpp,
    Copyright (c) 2018 Bruce Waddington, distributed under the BSD 3-Clause license.
*/

#pragma once

#include <QVector>

namespace Ekos
{

// Single-pole high-pass filter: passes star motion faster than the cutoff period.
class HighPassFilter
{
    public:
        void configure(double cutoffPeriodSec, double samplePeriodSec);
        double addValue(double value);

    private:
        double m_Alpha { 0.5 };
        double m_PrevValue { 0.0 };
        double m_Result { 0.0 };
        int m_Count { 0 };
};

// Single-pole low-pass filter (EMA): the slow drift component of star motion.
class LowPassFilter
{
    public:
        void configure(double cutoffPeriodSec, double samplePeriodSec);
        double addValue(double value);
        double current() const
        {
            return m_Result;
        }

    private:
        double m_Alpha { 0.5 };
        double m_Result { 0.0 };
        int m_Count { 0 };
};

// Time series statistics: sigma, linear fit, and drift-corrected sigma.
class AxisStats
{
    public:
        void reset();
        void add(double timeSec, double value);
        int count() const
        {
            return m_Values.size();
        }
        double sigma() const;
        bool linearFit(double *slope, double *intercept) const;
        double driftCorrectedSigma() const;

    private:
        QVector<double> m_Times;
        QVector<double> m_Values;
};

}
