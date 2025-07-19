/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>
    SPDX-FileCopyrightText: 2021 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "capturecountswidget.h"
#include "Options.h"
#include "ekos/manager.h"
#include "ekos/scheduler/schedulerjob.h"
#include "ekos/scheduler/schedulermodulestate.h"
#include "ekos/capture/capture.h"
#include "ekos/capture/sequencejob.h"

#include <ekos_capture_debug.h>

using Ekos::SequenceJob;

CaptureCountsWidget::CaptureCountsWidget(QWidget *parent) : QWidget(parent)
{
    setupUi(this);
    // switch between stacked views
    connect(switchToGraphicsButton, &QPushButton::clicked, this, [this]()
    {
        textView->setVisible(false);
        graphicalView->setVisible(true);
        Options::setUseGraphicalCountsDisplay(true);
    });
    connect(switchToTextButton, &QPushButton::clicked, this, [this]()
    {
        textView->setVisible(true);
        graphicalView->setVisible(false);
        Options::setUseGraphicalCountsDisplay(false);
    });

    // start with the last used view
    graphicalView->setVisible(Options::useGraphicalCountsDisplay());
    textView->setVisible(!Options::useGraphicalCountsDisplay());

    // setup graphical view
    gr_sequenceProgressBar->setDecimals(0);
    gr_overallProgressBar->setDecimals(0);

    reset();
}

void CaptureCountsWidget::setCurrentTrainName(const QString &name)
{
    m_currentTrainName = name;
    showCurrentCameraInfo();
    refreshCaptureCounters(name);
}

void CaptureCountsWidget::refreshImageCounts(const QString &trainname)
{
    if (imageCounts[trainname].changed)
    {
        imageProgress->setRange(0, int(std::ceil(imageCounts[trainname].totalTime)));
        imageProgress->setValue(int(std::ceil(imageCounts[trainname].totalTime - imageCounts[trainname].remainingTime)));
        gr_imageProgress->setRange(0, int(std::ceil(imageCounts[trainname].totalTime)));
        gr_imageProgress->setValue(imageProgress->value());

        frameRemainingTime->setText(imageCounts[trainname].countDown.toString("hh:mm:ss"));
        gr_frameRemainingTime->setText(frameRemainingTime->text());
        // clear the changed flag
        imageCounts[trainname].changed = false;
    }
    else if(isCaptureActive(trainname) == false)
    {
        imageProgress->setValue(0);
        gr_imageProgress->setValue(0);
    }
}

void CaptureCountsWidget::updateExposureProgress(const QSharedPointer<Ekos::SequenceJob> &job, const QString &trainname)
{
    imageCounts[trainname].countDown.setHMS(0, 0, 0);
    imageCounts[trainname].countDown = imageCounts[trainname].countDown.addSecs(int(std::round(job->getExposeLeft())));
    if (imageCounts[trainname].countDown.hour() == 23)
        imageCounts[trainname].countDown.setHMS(0, 0, 0);

    const double total = job->getCoreProperty(SequenceJob::SJ_Exposure).toDouble();
    const double remaining = job->getExposeLeft();

    // changes recognized only if the difference is at least one second (since we update once a second)
    imageCounts[trainname].changed = fabs(total - imageCounts[trainname].totalTime) >= 1.0 ||
                                     fabs(remaining - imageCounts[trainname].remainingTime) >= 1.0;
    imageCounts[trainname].totalTime = total;
    imageCounts[trainname].remainingTime = remaining;

    refreshImageCounts(trainname);
}

void CaptureCountsWidget::updateDownloadProgress(double timeLeft, const QString &trainname)
{
    imageCounts[trainname].countDown.setHMS(0, 0, 0);
    imageCounts[trainname].countDown = imageCounts[trainname].countDown.addSecs(int(std::ceil(timeLeft)));
    frameRemainingTime->setText(imageCounts[trainname].countDown.toString("hh:mm:ss"));
}

void CaptureCountsWidget::shareSchedulerState(QSharedPointer<Ekos::SchedulerModuleState> state)
{
    m_schedulerModuleState = state;
}

void CaptureCountsWidget::updateCaptureCountDown(int delta)
{
    // update counters of all devices
    for (const QString &trainname : totalCounts.keys())
    {
        totalCounts[trainname].countDown  = totalCounts[trainname].countDown.addSecs(delta);
        jobCounts[trainname].countDown      = jobCounts[trainname].countDown.addSecs(delta);
        sequenceCounts[trainname].countDown = sequenceCounts[trainname].countDown.addSecs(delta);

        // ensure that count downs do not overshoot
        if (totalCounts[trainname].countDown.hour() == 23)
            totalCounts[trainname].countDown.setHMS(0, 0, 0);
        if (jobCounts[trainname].countDown.hour() == 23)
            jobCounts[trainname].countDown.setHMS(0, 0, 0);
        if (sequenceCounts[trainname].countDown.hour() == 23)
            sequenceCounts[trainname].countDown.setHMS(0, 0, 0);
    }

    // do not change overall remaining time if scheduler is in endless loop
    if (m_schedulerModuleState == nullptr || m_schedulerModuleState->activeJob() == nullptr ||
            m_schedulerModuleState->activeJob()->getCompletionCondition() != Ekos::FINISH_LOOP)
    {
        overallRemainingTime->setText(totalCounts[m_currentTrainName].countDown.toString("hh:mm:ss"));
        gr_overallRemainingTime->setText(overallRemainingTime->text());
    }
    if (!m_captureProcess->isActiveJobPreview() && isCaptureActive(m_currentTrainName))
    {
        jobRemainingTime->setText(jobCounts[m_currentTrainName].countDown.toString("hh:mm:ss"));
        sequenceRemainingTime->setText(sequenceCounts[m_currentTrainName].countDown.toString("hh:mm:ss"));
        gr_sequenceRemainingTime->setText(sequenceRemainingTime->text());
    }
    else
    {
        jobRemainingTime->setText("--:--:--");
        sequenceRemainingTime->setText("--:--:--");
        gr_sequenceRemainingTime->setText("--:--:--");
    }
}

void CaptureCountsWidget::reset()
{
    // reset graphical view
    gr_imageProgress->setValue(0);
    gr_frameLabel->setText("");
    gr_frameRemainingTime->setText("--:--:--");
    gr_frameDetailsLabel->setText("");
    gr_sequenceLabel->setText(i18n("Sequence"));
    gr_sequenceProgressBar->setValue(0);
    gr_sequenceRemainingTime->setText("--:--:--");
    gr_overallLabel->setText(i18n("Overall"));
    gr_overallProgressBar->setValue(0);
    gr_overallRemainingTime->setText("--:--:--");

    // reset text view
    imageProgress->setValue(0);
    setFrameInfo("");
    frameRemainingTime->setText("");

    overallRemainingTime->setText("--:--:--");
    jobRemainingTime->setText("--:--:--");
    sequenceRemainingTime->setText("--:--:--");
}

namespace
{
QString frameLabel(const QString &type, const QString &filter)
{
    if (type == "Light")
    {
        if (filter.size() == 0)
            return type;
        else
            return filter;
    }
    else if (type == "Flat")
    {
        if (filter.size() == 0)
            return type;
        else
            return QString("%1 %2").arg(filter).arg(type);
    }
    else
        return type;
}
}

void CaptureCountsWidget::setFrameInfo(const QString frametype, const QString filter, const double exptime, const int xBin,
                                       const int yBin, const double gain)
{
    if (frametype == "")
    {
        frameInfoLabel->setText("");
        frameDetailsLabel->setText("");
        gr_frameRemainingTime->setText("");
    }
    else
    {
        frameInfoLabel->setText(QString("%1").arg(frameLabel(frametype, filter)));
        gr_frameLabel->setText(frameInfoLabel->text());
        QString details = "";
        if (exptime > 0)
            details.append(QString("%1: %2 sec").arg(i18n("Exposure")).arg(exptime, 0, 'f', exptime < 1 ? 2 : exptime < 5 ? 1 : 0));
        if (xBin > 0 && yBin > 0)
            details.append(QString(", bin: %1x%2").arg(xBin).arg(yBin));
        if (gain >= 0)
            details.append(QString(", gain: %1").arg(gain, 0, 'f', 1));

        frameDetailsLabel->setText(details);
        gr_frameDetailsLabel->setText(details);
    }
}

void CaptureCountsWidget::updateCaptureStatus(Ekos::CaptureState status, bool isPreview, const QString &trainname)
{
    // store current status
    captureStates[trainname] = status;
    // reset total counts
    totalCounts[trainname].countDown.setHMS(0, 0, 0);
    totalCounts[trainname].remainingTime = 0;
    // update the attribute whether the current capture is a preview
    m_isPreview = isPreview;

    // find the corresponding camera
    QSharedPointer<Ekos::Camera> selected_cam;
    for (QSharedPointer<Ekos::Camera> camera : m_captureProcess->cameras())
    {
        if (camera->opticalTrain() == trainname)
        {
            selected_cam = camera;
            break;
        }
    }

    if (selected_cam.isNull())
    {
        qCWarning(KSTARS_EKOS_CAPTURE) << "No matching camera found" << m_currentTrainName;
        return;
    }

    // determine total number of frames and completed ones - used either for
    // total numbers if scheduler is not used - and for job figures in the text
    // display if the scheduler is used
    jobCounts[trainname].remainingTime = selected_cam->state()->overallRemainingTime();
    jobCounts[trainname].count = 0, jobCounts[trainname].completed = 0;
    for (int i = 0; i < selected_cam->state()->allJobs().count(); i++)
    {
        jobCounts[trainname].count     += selected_cam->state()->jobImageCount(i);
        jobCounts[trainname].completed += selected_cam->state()->jobImageProgress(i);
    }

    Ekos::SchedulerJob *activeJob = m_schedulerModuleState->activeJob(trainname);
    if (m_schedulerModuleState != nullptr && activeJob != nullptr)
    {
        // FIXME: accessing the completed count might be one too low due to concurrency of updating the count and this loop
        totalCounts[trainname].completed = activeJob->getCompletedCount();
        totalCounts[trainname].count     = activeJob->getSequenceCount();
        if (activeJob->getEstimatedTime() > 0)
            totalCounts[trainname].remainingTime = int(activeJob->getEstimatedTime());
    }
    else
    {
        totalCounts[trainname].remainingTime = jobCounts[trainname].remainingTime;
        totalCounts[trainname].count         = jobCounts[trainname].count;
        totalCounts[trainname].completed     = jobCounts[trainname].completed;
    }

    // update sequence remaining time
    sequenceCounts[trainname].countDown.setHMS(0, 0, 0);
    sequenceCounts[trainname].countDown = sequenceCounts[trainname].countDown.addSecs(
            selected_cam->state()->activeJobRemainingTime());

    // update job remaining time if run from the scheduler
    if (m_schedulerModuleState != nullptr && activeJob != nullptr)
    {
        jobCounts[trainname].countDown.setHMS(0, 0, 0);
        jobCounts[trainname].countDown = jobCounts[trainname].countDown.addSecs(selected_cam->state()->overallRemainingTime());
    }

    switch (status)
    {
        case Ekos::CAPTURE_IDLE:
            // do nothing
            break;
        case Ekos::CAPTURE_ABORTED:
            refreshImageCounts(trainname);
            updateCaptureCountDown(0);
            [[fallthrough]];
        default:
            // update the counter display only for the currently selected train
            if (m_currentTrainName == trainname)
                refreshCaptureCounters(trainname);
    }
}

void CaptureCountsWidget::updateJobProgress(CaptureHistory::FrameData data, const QString &trainname)
{
    m_currentFrame[trainname] = data;

    // display informations if they come frome the currently selected camera device
    if (trainname == m_currentTrainName)
        showCurrentCameraInfo();
}

void CaptureCountsWidget::showCurrentCameraInfo()
{
    if (!m_currentFrame.contains(m_currentTrainName))
    {
        qCWarning(KSTARS_EKOS_CAPTURE) << "No frame info available for" << m_currentTrainName;
        return;
    }

    auto data = m_currentFrame[m_currentTrainName];

    if (data.jobType == SequenceJob::JOBTYPE_PREVIEW)
        setFrameInfo(i18n("Preview"), data.filterName, data.exptime, data.binning.x(), data.binning.y(), data.gain);
    else
        setFrameInfo(CCDFrameTypeNames[data.frameType], data.filterName, data.exptime, data.binning.x(),
                     data.binning.y(), data.gain);

    // display sequence progress in the graphical view
    gr_sequenceProgressBar->setRange(0, data.count);
    gr_sequenceProgressBar->setValue(data.completed);
    if (data.jobType == SequenceJob::JOBTYPE_PREVIEW)
        sequenceLabel->setText(QString("%1").arg(frameLabel(CCDFrameTypeNames[data.frameType], data.filterName)));
    else
        sequenceLabel->setText(QString("%1 (%3/%4)")
                               .arg(frameLabel(CCDFrameTypeNames[data.frameType], data.filterName)).arg(data.completed).arg(data.count));

    gr_sequenceLabel->setText(sequenceLabel->text());
}

void CaptureCountsWidget::refreshCaptureCounters(const QString &trainname)
{
    QString total_label = "Total";
    bool infinite_loop = false;
    Ekos::SchedulerJob *activeJob = m_schedulerModuleState->activeJob(trainname);
    bool isCapturing = isCaptureActive(trainname);
    if (m_schedulerModuleState != nullptr && activeJob != nullptr)
    {
        infinite_loop = (activeJob->getCompletionCondition() == Ekos::FINISH_LOOP);
        total_label = activeJob->getName();
    }

    if (infinite_loop == true || isCapturing == false)
    {
        overallRemainingTime->setText("--:--:--");
        gr_overallProgressBar->setRange(0, 1);
        gr_overallProgressBar->setValue(0);
        gr_overallRemainingTime->setText(overallRemainingTime->text());
    }
    else
    {
        totalCounts[trainname].countDown = totalCounts[trainname].countDown.addSecs(totalCounts[trainname].remainingTime);
        gr_overallProgressBar->setRange(0, totalCounts[trainname].count);
        gr_overallProgressBar->setValue(totalCounts[trainname].completed);
    }

    // display overall remainings
    if (m_isPreview)
        overallLabel->setText(QString("%1").arg(total_label));
    else
        overallLabel->setText(QString("%1 (%2/%3)")
                              .arg(total_label)
                              .arg(totalCounts[trainname].completed)
                              .arg(infinite_loop ? QString("-") : QString::number(totalCounts[trainname].count)));
    gr_overallLabel->setText(overallLabel->text());

    // update job remaining time if run from the scheduler
    bool show_job_progress = (m_schedulerModuleState != nullptr && activeJob != nullptr);
    jobLabel->setVisible(show_job_progress);
    jobRemainingTime->setVisible(show_job_progress);
    if (show_job_progress)
    {
        jobLabel->setText(QString("Job (%1/%2)")
                          .arg(jobCounts[trainname].completed)
                          .arg(jobCounts[trainname].count));
    }
}

Ekos::CaptureState CaptureCountsWidget::captureState(const QString &trainname)
{
    // initialize to a default state if unknown
    if (! captureStates.contains(trainname))
        captureStates[trainname] = Ekos::CAPTURE_IDLE;

    return captureStates[trainname];
}

bool CaptureCountsWidget::isCaptureActive(const QString &trainname)
{
    Ekos::CaptureState state = captureState(trainname);
    return (state == Ekos::CAPTURE_PROGRESS ||
            state == Ekos::CAPTURE_CAPTURING ||
            state == Ekos::CAPTURE_PAUSE_PLANNED ||
            state == Ekos::CAPTURE_IMAGE_RECEIVED);
}



void CaptureCountsWidget::setEnabled(bool enabled)
{
    QWidget::setEnabled(enabled);
    overallLabel->setEnabled(enabled);
    gr_overallLabel->setEnabled(enabled);
}
