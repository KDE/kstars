/*
    SPDX-FileCopyrightText: 2025 Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "hysteresisguider.h"

#include <cmath>
#include "ekos_guide_debug.h"

HysteresisGuider::HysteresisGuider(const QString &id) : m_ID(id)
{
    reset();
}

void HysteresisGuider::reset()
{
    m_LastOutput = 0.0;
    m_LastGuideTime = QDateTime();
    m_GuiderIteration = 0;
}
double HysteresisGuider::guide(double offset)
{
    QString comment;

    const double hysteresis = m_Hysteresis;

    // Reset the guider if we haven't guided recently.
    const QDateTime now = QDateTime::currentDateTime();
    if (m_LastGuideTime.isValid())
    {
        constexpr int MAX_GUIDE_LAG = 30;
        const int interval = m_LastGuideTime.secsTo(now);
        if (interval < 0 || interval > MAX_GUIDE_LAG)
        {
            reset();
            comment = QString("Reset: guide lag %1s").arg(interval);
        }
    }
    else
        reset();
    m_LastGuideTime = now;

    const double time = ++m_GuiderIteration;
    double guideVal = m_Gain * ((1.0 - hysteresis) * offset + hysteresis * m_LastOutput);

    if (fabs(guideVal) > 0 && fabs(guideVal) < m_MinMove)
    {

        comment.append(QString("%1%2 < minMove %3").arg(!comment.isEmpty() ? ", " : "")
                       .arg(guideVal, 0, 'f', 2).arg(m_MinMove, 0, 'f', 2));
        guideVal = 0;
    }

    qCDebug(KSTARS_EKOS_GUIDE) << QString("HysteresisGuide(%1,%2) %3 * ((%4 * %5) + (%6 * %7)) --> %8: %9")
                               .arg(m_ID, 3).arg(time, 3, 'f', 0).arg(m_Gain, 4, 'f', 2)
                               .arg((1.0 - hysteresis), 4, 'f', 2).arg(offset, 4, 'f', 2)
                               .arg(hysteresis, 4, 'f', 2).arg(m_LastOutput, 4, 'f', 2)
                               .arg(guideVal, 5, 'f', 2).arg(comment);
    m_LastOutput = guideVal;
    return guideVal;
}


