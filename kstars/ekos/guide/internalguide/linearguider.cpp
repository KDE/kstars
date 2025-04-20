/*
    SPDX-FileCopyrightText: 2025 Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "linearguider.h"

#include <cmath>
#include "ekos_guide_debug.h"

LinearGuider::LinearGuider(const QString &id) : m_ID(id)
{
    reset();
}

void LinearGuider::reset()
{
    m_Samples.clear();
    m_SumTime = 0.0;
    m_SumOffset = 0.0;
    m_SumTimeSq = 0.0;
    m_OffsetSq = 0.0;
    m_SumTimeOffset = 0.0;
    m_NumRejects = 0;
    m_LastGuideTime = QDateTime();
    m_GuiderIteration = 0;
}
double LinearGuider::guide(double offset)
{
    QString comment;

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
    const Sample s(time, offset);
    double guideVal = m_Gain * offset;

    addSample(s);
    const int size = m_Samples.size();
    if (size >= 4)
    {
        // Seems extreme, should at least min the m_MinMove with 1 or 2 a-s
        constexpr double MIN_BAD_OFFSET = 2.0;
        if (fabs(offset) > std::min(MIN_BAD_OFFSET, 4 * m_MinMove))
        {
            comment = QString("%1Reset: offset > 4*minMove %2").arg(!comment.isEmpty() ? ", " : "").arg(m_MinMove, 0, 'f', 2);
            reset();
        }
        else
        {
            const double slope = getSlope();
            if (std::isfinite(slope))
            {
                guideVal = slope * size * m_Gain;
                // Don't allow the guide pulse to be the opposite direction of the offset.
                if (guideVal * offset < 0)
                {
                    guideVal = 0;
                    comment = QString("%1slope %2 opposite to offset").arg(!comment.isEmpty() ? ", " : "").arg(slope, 0, 'f', 2);
                }
                else
                    comment = QString("%1slope %2").arg(!comment.isEmpty() ? ", " : "").arg(slope, 0, 'f', 2);
            }
            else
                comment = QString("%1bad slope").arg(!comment.isEmpty() ? ", " : "");
        }
    }
    else
        comment.append(QString("%1Starting").arg(!comment.isEmpty() ? ", " : ""));

    if (fabs(guideVal) > fabs(offset))
    {
        comment.append(QString("%1Guideval %2 > offset").arg(!comment.isEmpty() ? ", " : "").arg(guideVal, 0, 'f', 2));
        guideVal = m_Gain * offset;
        m_NumRejects++;
        if (m_NumRejects >= 3)
        {
            comment.append(QString("%1Reset: rejects %2").arg(!comment.isEmpty() ? ", " : "").arg(m_NumRejects));
            reset();
        }
    }
    else
        m_NumRejects = 0;

    if (fabs(guideVal) > 0 && fabs(guideVal) < m_MinMove)
    {

        comment.append(QString("%1%2 < minMove %3").arg(!comment.isEmpty() ? ", " : "")
                       .arg(guideVal, 0, 'f', 2).arg(m_MinMove, 0, 'f', 2));
        guideVal = 0;
    }

    qCDebug(KSTARS_EKOS_GUIDE) << QString("LinearGuide(%1,%2) %3 * %4 --> %5: %6")
                               .arg(m_ID, 3).arg(time, 3, 'f', 0).arg(offset, 6, 'f', 2).arg(m_Gain, 4, 'f', 2)
                               .arg(guideVal, 5, 'f', 2).arg(comment);
    return guideVal;
}

void LinearGuider::addSample(const Sample &sample)
{
    m_Samples.enqueue(sample);
    updateStats(sample);
    if (m_Samples.size() > m_Length)
        removeLastSample();
}

void LinearGuider::updateStats(const Sample &sample)
{
    m_SumTime += sample.time;
    m_SumOffset += sample.offset;
    m_SumTimeSq += sample.time * sample.time;
    m_OffsetSq += sample.offset * sample.offset;
    m_SumTimeOffset += sample.time * sample.offset;
}

void LinearGuider::removeLastSample()
{
    if (m_Samples.size() <= 0)
        return;
    const Sample &sample = m_Samples.head();
    m_SumTime -= sample.time;
    m_SumOffset -= sample.offset;
    m_SumTimeSq -= sample.time * sample.time;
    m_OffsetSq -= sample.offset * sample.offset;
    m_SumTimeOffset -= sample.time * sample.offset;
    m_Samples.dequeue();
}

double LinearGuider::getSlope()
{
    const double sumTime = m_SumTime;
    const double denom = (m_Samples.size() * m_SumTimeSq) - (sumTime * sumTime);
    if (denom == 0) return 0;
    const double slope = ((m_Samples.size() * m_SumTimeOffset) - (sumTime * m_SumOffset)) / denom;
    return slope;
}

