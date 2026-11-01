/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>
    Based on the Alt-Az derotation work by Toni Schriber (MR !1490).

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "fieldderotation.h"

#include "ksutils.h"

#include <cmath>

namespace Ekos
{

double FieldDerotation::parallacticAngle(double hourAngle, double declination, double latitude)
{
    // Meeus, Astronomical Algorithms, eq. 14.1, written with atan2 so it stays defined at the pole
    // tan q = sin H / (tan phi cos delta - sin delta cos H)
    const double H = hourAngle * M_PI / 180.0;
    const double delta = declination * M_PI / 180.0;
    const double phi = latitude * M_PI / 180.0;
    const double q = std::atan2(std::sin(H),
                                std::tan(phi) * std::cos(delta) - std::sin(delta) * std::cos(H));
    return KSUtils::rangePA(q * 180.0 / M_PI);
}

void FieldDerotation::hold(double pa, double ra, double dec)
{
    m_HeldPA = KSUtils::rangePA(pa);
    m_HeldRA = ra;
    m_HeldDec = dec;
    m_HasHold = true;
}

void FieldDerotation::clearHold()
{
    m_HasHold = false;
}

FieldDerotation::Decision FieldDerotation::update(const Sample &sample)
{
    Decision decision;

    if (!sample.tracking)
    {
        m_WasTracking = false;
        return decision;
    }

    const bool trackingStarted = !m_WasTracking;
    m_WasTracking = true;

    if (m_HasHold && trackingStarted)
    {
        // Angular distance between the pointing of the held PA and the new pointing
        const double d1 = m_HeldDec * M_PI / 180.0, d2 = sample.dec * M_PI / 180.0;
        const double dRA = (sample.ra - m_HeldRA) * M_PI / 180.0;
        const double cosDistance = std::sin(d1) * std::sin(d2) + std::cos(d1) * std::cos(d2) * std::cos(dRA);
        const double distance = std::acos(std::max(-1.0, std::min(1.0, cosDistance))) * 180.0 / M_PI;
        if (distance > newTargetDistance)
            m_HasHold = false;
    }

    if (!m_HasHold)
    {
        hold(sample.cameraPA, sample.ra, sample.dec);
        return decision;
    }

    decision.error = KSUtils::rangePA(sample.cameraPA - m_HeldPA);
    if (!sample.rotatorIdle || std::abs(decision.error) < m_Threshold)
        return decision;

    if (std::abs(decision.error) > m_MaximumCorrection)
    {
        decision.action = LIMIT_EXCEEDED;
        decision.targetPA = sample.cameraPA;
        hold(sample.cameraPA, sample.ra, sample.dec);
        return decision;
    }

    decision.action = ROTATE;
    decision.targetPA = m_HeldPA;
    return decision;
}

}
