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

bool CaptureProcessOverlay::addFrameData(CaptureHistory::FrameData data, const QString &devicename)
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
    const CaptureHistory::FrameData currentFrame = captureHistory().currentFrame();
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
    CaptureHistory::FrameData lastFrame = captureHistory().getFrame(captureHistory().size() - 1);
    lastFrame.targetdrift = targetDiff;
    captureHistory().deleteFrame(captureHistory().size() - 1);
    captureHistory().addFrame(lastFrame);
    updateFrameData();
}

bool CaptureProcessOverlay::hasFrames()
{
    if (m_captureHistory.contains(m_currentTrainName))
        return m_captureHistory[m_currentTrainName].size() > 0;

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

void CaptureProcessOverlay::setCurrentTrainName(const QString &trainname)
{
    m_currentTrainName = trainname;
    refresh();
}

