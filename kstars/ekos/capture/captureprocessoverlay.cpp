/*
    SPDX-FileCopyrightText: 2021 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "captureprocessoverlay.h"
#include "QTime"
#include "QFileInfo"

CaptureProcessOverlay::CaptureProcessOverlay(QWidget *parent) : QWidget(parent)
{
    setupUi(this);
    // use white as text color
    setStyleSheet("color: rgb(255, 255, 255);");
    connect(updateStatisticsButton, &QPushButton::clicked, [&]
    {
        captureHistory().updateTargetStatistics();
        // refresh the display for the current frame
        updateFrameData();
        // refresh the statistics display
        displayTargetStatistics();
    });
}

void CaptureProcessOverlay::refresh()
{
    updateFrameData();
    // refresh the statistics display
    displayTargetStatistics();
}

bool CaptureProcessOverlay::addFrameData(FrameData data, const QString &devicename)
{
    if (m_captureHistory[devicename].addFrame(data) == false)
        return false;

    refresh();
    return true;
}

void CaptureProcessOverlay::updateFrameData()
{
    // nothing to display
    if (hasFrames() == false)
    {
        frameDataWidget->setVisible(false);
        return;
    }
    frameDataWidget->setVisible(true);
    const FrameData currentFrame = captureHistory().currentFrame();
    frameTypeLabel->setText(QString("%1 %2").arg(CCDFrameTypeNames[currentFrame.frameType]).arg(currentFrame.filterName));
    exposureValue->setText(QString("%1 sec").arg(currentFrame.exptime, 0, 'f',
                           currentFrame.exptime < 1 ? 2 : currentFrame.exptime < 5 ? 1 : 0));
    binningValue->setText(QString("%1x%2").arg(currentFrame.binning.x()).arg(currentFrame.binning.y()));
    filenameValue->setText(currentFrame.filename);
    geometryValue->setText(QString("%1px x %2px").arg(currentFrame.width).arg(currentFrame.height));

    bool visible = (currentFrame.gain >= 0);
    gainLabel->setVisible(visible);
    gainValue->setVisible(visible);
    gainValue->setText(QString("%1").arg(currentFrame.gain, 0, 'f', 1));

    visible = (currentFrame.offset >= 0);
    offsetLabel->setVisible(visible);
    offsetValue->setVisible(visible);
    offsetValue->setText(QString("%1").arg(currentFrame.offset, 0, 'f', 1));

    visible = (currentFrame.iso != "");
    isoValue->setVisible(visible);
    isoValue->setText(QString("ISO %1").arg(currentFrame.iso));

    visible = (currentFrame.targetdrift >= 0);
    targetDriftLabel->setVisible(visible);
    targetDriftValue->setVisible(visible);
    targetDriftValue->setText(QString("%L1\"").arg(currentFrame.targetdrift, 0, 'f', 1));

    // determine file creation date
    QFileInfo fileinfo(currentFrame.filename);
    const QDateTime lastmodified = fileinfo.lastModified();
    captureDate->setText(lastmodified.toString("dd.MM.yyyy hh:mm:ss"));

    // update capture counts
    if (captureHistory().size() > 0)
        historyCountsLabel->setText(QString("(%1/%2)").arg(captureHistory().position() + 1).arg(captureHistory().size()));
    else
        historyCountsLabel->setText("");

    // update enabling of the navigation buttons
    historyBackwardButton->setEnabled(captureHistory().size() > 0 && captureHistory().position() > 0);
    historyForwardButton->setEnabled(captureHistory().size() > 0 && captureHistory().size() - captureHistory().position() > 1);
}

void CaptureProcessOverlay::updateTargetDistance(double targetDiff)
{
    // since the history is read only, we need to delete the last one and add it again.
    FrameData lastFrame = captureHistory().getFrame(captureHistory().size() - 1);
    lastFrame.targetdrift = targetDiff;
    captureHistory().deleteFrame(captureHistory().size() - 1);
    captureHistory().addFrame(lastFrame);
    updateFrameData();
}

bool CaptureProcessOverlay::hasFrames()
{
    if (m_captureHistory.contains(m_currentCameraDeviceName))
        return m_captureHistory[m_currentCameraDeviceName].size() > 0;

    return false;
}

void CaptureProcessOverlay::displayTargetStatistics()
{
    QString display = "";
    // iterate over all targets
    QList<QString> targets = captureHistory().statistics.keys();
    for (QList<QString>::iterator target_it = targets.begin(); target_it != targets.end(); target_it++)
    {
        display.append(QString("<p><b><u>%1</u></b><table border=0>").arg(*target_it == "" ? "<it>" + i18n("No target") + "</it>" :
                       *target_it));

        // iterate over all types of captured frames
        QList<QPair<CCDFrameType, QString>> keys = captureHistory().statistics[*target_it].keys();
        for (QList<QPair<CCDFrameType, QString>>::iterator key_it = keys.begin(); key_it != keys.end(); key_it++)
        {
            // add frame type x filter as header
            QString frame_type = CCDFrameTypeNames[key_it->first];
            QString filter     = key_it->second;

            // add statistics per exposure time
            QList<QPair<int, int>*> counts = captureHistory().statistics[*target_it].value(*key_it);
            for (QList<QPair<int, int>*>::iterator it = counts.begin(); it != counts.end(); it++)
            {
                double exptime = (*it)->first / 1000.0;
                QTime total(0, 0, 0, 0);
                total = total.addMSecs(int((*it)->first * (*it)->second));
                display.append(QString("<tr><td><b>%1 %2</b> (%3 x %4 sec):</td><td style=\"text-align: right\">%5</td></tr>")
                               .arg(frame_type).arg(filter)
                               .arg((*it)->second)
                               .arg(exptime, 0, 'f', exptime < 10 ? 2 : 0)
                               .arg(total.toString("hh:mm:ss")));
            }
        }
        display.append("</table></p>");
    }
    // display statistics
    captureStatisticsLabel->setText(display);
}

bool CaptureProcessOverlay::showNextFrame()
{
    if (captureHistory().forward())
    {
        updateFrameData();
        return true;
    }
    return false;
}

bool CaptureProcessOverlay::showPreviousFrame()
{
    if (captureHistory().backward())
    {
        updateFrameData();
        return true;
    }
    return false;
}

bool CaptureProcessOverlay::deleteFrame(int pos)
{
    if (captureHistory().deleteFrame(pos) == true)
    {
        // deleting succeeded, update overlay
        updateFrameData();
        displayTargetStatistics();
        return true;
    }
    return false;
}

void CaptureProcessOverlay::setCurrentCameraDeviceName(const QString &devicename)
{
    m_currentCameraDeviceName = devicename;
    refresh();
}

bool CaptureProcessOverlay::CaptureHistory::addFrame(CaptureProcessOverlay::FrameData data)
{
    // check if the file already exists in the history
    for (QList<FrameData>::iterator it = m_history.begin(); it != m_history.end(); it++)
        if (it->filename == data.filename)
            // already exists, ignore
            return false;
    // history is clean, simply append
    m_history.append(data);
    m_position = m_history.size() - 1;
    countNewFrame(data.target, data.frameType, data.filterName, data.exptime);
    return true;
}

bool CaptureProcessOverlay::CaptureHistory::deleteFrame(int pos)
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
        updateTargetStatistics();
        return true;
    }
    else
        return false;
}

void CaptureProcessOverlay::CaptureHistory::reset()
{
    m_position = -1;
    m_history.clear();
}

bool CaptureProcessOverlay::CaptureHistory::forward()
{
    if (m_position < m_history.size() - 1)
    {
        m_position++;
        return true;
    }
    else
        return false;
}

bool CaptureProcessOverlay::CaptureHistory::backward()
{
    if (m_position > 0)
    {
        m_position--;
        return true;
    }
    else
        return false;
}

void CaptureProcessOverlay::CaptureHistory::updateTargetStatistics()
{
    statistics.clear();
    QList<FrameData> new_history;
    // iterate over all entries in the history to update the statistics
    for (QList<FrameData>::iterator list_it = m_history.begin(); list_it != m_history.end(); list_it++)
    {
        // if the corresponding file exists, add it to the statistics
        if (QFile(list_it->filename).exists())
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

void CaptureProcessOverlay::CaptureHistory::countNewFrame(QString target, CCDFrameType frameType, QString filter,
        double exptime)
{
    // create search key
    QPair<CCDFrameType, QString> key(frameType, filter);
    // due to the equality problem with double, we use a milliseconds for exposure time
    int exptime_r = int(exptime * 1000);
    // create new target map if missing
    if (statistics.contains(target) == false)
        statistics.insert(target, CaptureProcessOverlay::FrameStatistics());
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
