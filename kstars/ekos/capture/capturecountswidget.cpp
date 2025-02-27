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
}

void CaptureCountsWidget::updateExposureProgress(const QSharedPointer<Ekos::SequenceJob> &job, const QString &devicename)
{
    imageCountDown[devicename].setHMS(0, 0, 0);
    imageCountDown[devicename] = imageCountDown[devicename].addSecs(int(std::round(job->getExposeLeft())));
    if (imageCountDown[devicename].hour() == 23)
        imageCountDown[devicename].setHMS(0, 0, 0);

    imageProgress->setRange(0, int(std::ceil(job->getCoreProperty(SequenceJob::SJ_Exposure).toDouble())));
    imageProgress->setValue(int(std::ceil(job->getCoreProperty(SequenceJob::SJ_Exposure).toDouble() - job->getExposeLeft())));
    gr_imageProgress->setRange(0, int(std::ceil(job->getCoreProperty(SequenceJob::SJ_Exposure).toDouble())));
    gr_imageProgress->setValue(imageProgress->value());

    frameRemainingTime->setText(imageCountDown[devicename].toString("hh:mm:ss"));
    gr_frameRemainingTime->setText(frameRemainingTime->text());
}

void CaptureCountsWidget::updateDownloadProgress(double timeLeft, const QString &trainname)
{
    imageCountDown[trainname].setHMS(0, 0, 0);
    imageCountDown[trainname] = imageCountDown[trainname].addSecs(int(std::ceil(timeLeft)));
    frameRemainingTime->setText(imageCountDown[trainname].toString("hh:mm:ss"));
}

void CaptureCountsWidget::shareSchedulerState(QSharedPointer<Ekos::SchedulerModuleState> state)
{
    m_schedulerModuleState = state;
}

void CaptureCountsWidget::updateCaptureCountDown(int delta)
{
    // update counters of all devices
    for (const QString &devicename : overallCountDown.keys())
    {
        overallCountDown[devicename]  = overallCountDown[devicename].addSecs(delta);
        jobCountDown[devicename]      = jobCountDown[devicename].addSecs(delta);
        sequenceCountDown[devicename] = sequenceCountDown[devicename].addSecs(delta);

        // ensure that count downs do not overshoot
        if (overallCountDown[devicename].hour() == 23)
            overallCountDown[devicename].setHMS(0, 0, 0);
        if (jobCountDown[devicename].hour() == 23)
            jobCountDown[devicename].setHMS(0, 0, 0);
        if (sequenceCountDown[devicename].hour() == 23)
            sequenceCountDown[devicename].setHMS(0, 0, 0);
    }

    // do not change overall remaining time if scheduler is in endless loop
    if (m_schedulerModuleState == nullptr || m_schedulerModuleState->activeJob() == nullptr ||
            m_schedulerModuleState->activeJob()->getCompletionCondition() != Ekos::FINISH_LOOP)
    {
        overallRemainingTime->setText(overallCountDown[m_currentTrainName].toString("hh:mm:ss"));
        gr_overallRemainingTime->setText(overallRemainingTime->text());
    }
    if (!m_captureProcess->isActiveJobPreview())
    {
        jobRemainingTime->setText(jobCountDown[m_currentTrainName].toString("hh:mm:ss"));
        sequenceRemainingTime->setText(sequenceCountDown[m_currentTrainName].toString("hh:mm:ss"));
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
    overallCountDown[trainname].setHMS(0, 0, 0);
    bool infinite_loop = false;
    int total_remaining_time = 0, total_completed = 0, total_count = 0;
    double total_percentage = 0;
    // use this value if no scheduler is running and job name otherwise
    QString total_label = "Total";

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
    double capture_total_percentage = selected_cam->state()->progressPercentage();
    int    capture_remaining_time   = selected_cam->state()->overallRemainingTime();
    int capture_total_count = 0, capture_total_completed = 0;
    for (int i = 0; i < selected_cam->state()->allJobs().count(); i++)
    {
        capture_total_count     += selected_cam->state()->jobImageCount(i);
        capture_total_completed += selected_cam->state()->jobImageProgress(i);
    }


    if (m_schedulerModuleState != nullptr && m_schedulerModuleState->activeJob() != nullptr)
    {
        total_label = m_schedulerModuleState->activeJob()->getName();
        // FIXME: accessing the completed count might be one too low due to concurrency of updating the count and this loop
        total_completed = m_schedulerModuleState->activeJob()->getCompletedCount();
        total_count     = m_schedulerModuleState->activeJob()->getSequenceCount();
        infinite_loop   = (m_schedulerModuleState->activeJob()->getCompletionCondition() == Ekos::FINISH_LOOP);
        if (total_count > 0)
            total_percentage = (100 * total_completed) / total_count;
        if (m_schedulerModuleState->activeJob()->getEstimatedTime() > 0)
            total_remaining_time = int(m_schedulerModuleState->activeJob()->getEstimatedTime());
    }
    else
    {
        total_percentage     = capture_total_percentage;
        total_remaining_time = capture_remaining_time;
        total_count          = capture_total_count;
        total_completed      = capture_total_completed;
    }

    switch (status)
    {
        case Ekos::CAPTURE_IDLE:
            // do nothing
            break;
        case Ekos::CAPTURE_ABORTED:
            reset();
            break;
        default:
            if (infinite_loop == true)
            {
                overallRemainingTime->setText("--:--:--");
                gr_overallProgressBar->setValue(0);
                gr_overallRemainingTime->setText(overallRemainingTime->text());
            }
            else
            {
                overallCountDown[trainname] = overallCountDown[trainname].addSecs(total_remaining_time);
                gr_overallProgressBar->setValue(total_percentage);
            }

            // display overall remainings
            if (isPreview)
                overallLabel->setText(QString("%1").arg(total_label));
            else
                overallLabel->setText(QString("%1 (%2/%3)")
                                      .arg(total_label)
                                      .arg(total_completed)
                                      .arg(infinite_loop ? QString("-") : QString::number(total_count)));
            gr_overallLabel->setText(overallLabel->text());

            // update job remaining time if run from the scheduler
            bool show_job_progress = (m_schedulerModuleState != nullptr && m_schedulerModuleState->activeJob() != nullptr);
            jobLabel->setVisible(show_job_progress);
            jobRemainingTime->setVisible(show_job_progress);
            if (show_job_progress)
            {
                jobCountDown[trainname].setHMS(0, 0, 0);
                jobCountDown[trainname] = jobCountDown[trainname].addSecs(selected_cam->state()->overallRemainingTime());
                jobLabel->setText(QString("Job (%1/%2)")
                                  .arg(capture_total_completed)
                                  .arg(capture_total_count));
            }

            // update sequence remaining time
            sequenceCountDown[trainname].setHMS(0, 0, 0);
            sequenceCountDown[trainname] = sequenceCountDown[trainname].addSecs(selected_cam->state()->activeJobRemainingTime());
    }
}

void CaptureCountsWidget::updateJobProgress(CaptureProcessOverlay::FrameData data, const QString &trainname)
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


void CaptureCountsWidget::setEnabled(bool enabled)
{
    QWidget::setEnabled(enabled);
    overallLabel->setEnabled(enabled);
    gr_overallLabel->setEnabled(enabled);
}
