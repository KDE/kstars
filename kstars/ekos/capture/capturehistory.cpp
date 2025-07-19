/*
    SPDX-FileCopyrightText: 2021/2025 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "capturehistory.h"



bool CaptureHistory::addFrame(FrameData data, bool noduplicates)
{
    if (noduplicates)
    {
        // check if the file already exists in the history
        for (QList<FrameData>::iterator it = m_history.begin(); it != m_history.end(); it++)
            if (it->filename == data.filename)
                // already exists, ignore
                return false;
    }

    // history is clean, simply append
    m_history.append(data);
    m_position = m_history.size() - 1;
    countNewFrame(data.target, data.frameType, data.filterName, data.exptime);
    return true;
}

bool CaptureHistory::deleteFrame(int pos, bool existingFilesOnly)
{
    if (m_history.size() != 0 && pos < m_history.size())
    {
        m_history.removeAt(pos);
        // adapt the current position if the deleted frame was deleted before it or itself
        if (m_position >= pos)
            m_position -= 1;
        // ensure staying in range
        if (m_position < 0 && m_history.size() > 0)
            m_position = 0;
        else if (m_position >= m_history.size())
            m_position = m_history.size() - 1;
        // update statistics, since one file is missing
        updateTargetStatistics(existingFilesOnly);
        return true;
    }
    else
        return false;
}

const CaptureHistory::FrameData CaptureHistory::firstFrame()
{
    if (m_history.size() > 0)
        return getFrame(0);

    // return an empty value
    CaptureHistory::FrameData dummy;
    return dummy;
}

const CaptureHistory::FrameData CaptureHistory::lastFrame()
{
    if (m_history.size() > 0)
        return getFrame(m_history.size() - 1);

    // return an empty value
    CaptureHistory::FrameData dummy;
    return dummy;
}

void CaptureHistory::reset()
{
    m_position = -1;
    m_history.clear();
}

bool CaptureHistory::forward()
{
    if (m_position < m_history.size() - 1)
    {
        m_position++;
        return true;
    }
    else
        return false;
}

bool CaptureHistory::backward()
{
    if (m_position > 0)
    {
        m_position--;
        return true;
    }
    else
        return false;
}

bool CaptureHistory::first()
{
    if (m_history.size() <= 0)
        return false;

    m_position = 0;
    return true;
}

bool CaptureHistory::last()
{
    if (m_history.size() <= 0)
        return false;

    m_position = m_history.size() - 1;
    return true;
}

void CaptureHistory::updateTargetStatistics(bool existingFilesOnly)
{
    statistics.clear();
    QList<FrameData> new_history;
    // iterate over all entries in the history to update the statistics
    for (QList<FrameData>::iterator list_it = m_history.begin(); list_it != m_history.end(); list_it++)
    {
        // if the corresponding file exists, add it to the statistics
        if (!existingFilesOnly || QFile(list_it->filename).exists())
        {
            countNewFrame(list_it->target, list_it->frameType, list_it->filterName, list_it->exptime);
            new_history.append(*list_it);
        }
    }
    // switch history lists
    m_history.clear();
    m_history = new_history;

    // check if the position is correct, if not move to the last element
    if (m_position >= m_history.size())
        m_position = m_history.size() - 1;
}

void CaptureHistory::countNewFrame(QString target, CCDFrameType frameType, QString filter,
                                   double exptime)
{
    // create search key
    QPair<CCDFrameType, QString> key(frameType, filter);
    // due to the equality problem with double, we use a milliseconds for exposure time
    int exptime_r = int(exptime * 1000);
    // create new target map if missing
    if (statistics.contains(target) == false)
        statistics.insert(target, FrameStatistics());
    // create new filter list if missing
    if (statistics[target].contains(key) == false)
        statistics[target].insert(key, QList<QPair<int, int>*>());

    QPair<int, int>* count = nullptr;
    QList<QPair<int, int>*> *counts = &statistics[target][key];
    for (QList<QPair<int, int>*>::iterator it = counts->begin(); it != counts->end(); it++)
    {
        // search for matching exposure time
        if ((*it)->first == exptime_r)
        {
            count = *it;
            break;
        }
    }
    // nothing found, initialize
    if (count == nullptr)
    {
        count = new QPair<int, int>(exptime_r, 0);
        counts->append(count);
    }
    // increase the counter
    count->second = count->second + 1;
}
