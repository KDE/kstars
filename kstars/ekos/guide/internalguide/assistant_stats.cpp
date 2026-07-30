/*
    SPDX-FileCopyrightText: 2026 Pavan <pk6122004@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later

    Filter and statistics classes adapted from PHD2's guiding_stats.cpp,
    Copyright (c) 2018 Bruce Waddington, distributed under the BSD 3-Clause license.
*/

#include "assistant_stats.h"

#include <algorithm>
#include <cmath>

namespace Ekos
{

void HighPassFilter::configure(double cutoffPeriodSec, double samplePeriodSec)
{
    // Sample periods below 1 s are clamped: the sensor already averages faster motion.
    m_Alpha = cutoffPeriodSec / (cutoffPeriodSec + std::max(1.0, samplePeriodSec));
    m_PrevValue = 0.0;
    m_Result = 0.0;
    m_Count = 0;
}

double HighPassFilter::addValue(double value)
{
    if (m_Count == 0)
        m_Result = value;
    else
        m_Result = m_Alpha * (m_Result + value - m_PrevValue);
    m_PrevValue = value;
    m_Count++;
    return m_Result;
}

void LowPassFilter::configure(double cutoffPeriodSec, double samplePeriodSec)
{
    m_Alpha = 1.0 - (cutoffPeriodSec / (cutoffPeriodSec + std::max(1.0, samplePeriodSec)));
    m_Result = 0.0;
    m_Count = 0;
}

double LowPassFilter::addValue(double value)
{
    if (m_Count == 0)
        m_Result = value;
    else
        m_Result += m_Alpha * (value - m_Result);
    m_Count++;
    return m_Result;
}

void AxisStats::reset()
{
    m_Times.clear();
    m_Values.clear();
}

void AxisStats::add(double timeSec, double value)
{
    m_Times.append(timeSec);
    m_Values.append(value);
}

double AxisStats::sigma() const
{
    const int n = m_Values.size();
    if (n < 2)
        return 0.0;
    double mean = 0.0;
    for (double v : m_Values)
        mean += v;
    mean /= n;
    double ss = 0.0;
    for (double v : m_Values)
        ss += (v - mean) * (v - mean);
    return std::sqrt(ss / (n - 1));
}

bool AxisStats::linearFit(double *slope, double *intercept) const
{
    const int n = m_Values.size();
    if (n < 3)
        return false;
    double sumX = 0.0, sumY = 0.0, sumXY = 0.0, sumXSq = 0.0;
    for (int i = 0; i < n; i++)
    {
        sumX += m_Times[i];
        sumY += m_Values[i];
        sumXY += m_Times[i] * m_Values[i];
        sumXSq += m_Times[i] * m_Times[i];
    }
    const double denom = n * sumXSq - sumX * sumX;
    if (std::abs(denom) < 1e-12)
        return false;
    *slope = (n * sumXY - sumX * sumY) / denom;
    *intercept = (sumY - *slope * sumX) / n;
    return true;
}

double AxisStats::driftCorrectedSigma() const
{
    double slope = 0.0, intercept = 0.0;
    if (!linearFit(&slope, &intercept))
        return sigma();
    const int n = m_Values.size();
    double mean = 0.0;
    QVector<double> residuals(n);
    for (int i = 0; i < n; i++)
    {
        residuals[i] = m_Values[i] - (slope * m_Times[i] + intercept);
        mean += residuals[i];
    }
    mean /= n;
    double ss = 0.0;
    for (double r : residuals)
        ss += (r - mean) * (r - mean);
    return std::sqrt(ss / (n - 1));
}

}
