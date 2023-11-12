/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>
    SPDX-FileCopyrightText: 2021 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "capturecountswidget.h"
#include "Options.h"
#include "ekos/manager.h"
#include "ekos/scheduler/scheduler.h"
#include "ekos/capture/capture.h"
#include "ekos/capture/sequencejob.h"

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

void CaptureCountsWidget::updateExposureProgress(Ekos::SequenceJob *job)
{
    imageCountDown.setHMS(0, 0, 0);
    imageCountDown = imageCountDown.addSecs(int(std::round(job->getExposeLeft())));
    if (imageCountDown.hour() == 23)
        imageCountDown.setHMS(0, 0, 0);

    imageProgress->setRange(0, int(std::ceil(job->getCoreProperty(SequenceJob::SJ_Exposure).toDouble())));
    imageProgress->setValue(int(std::ceil(job->getCoreProperty(SequenceJob::SJ_Exposure).toDouble() - job->getExposeLeft())));
    gr_imageProgress->setRange(0, int(std::ceil(job->getCoreProperty(SequenceJob::SJ_Exposure).toDouble())));
    gr_imageProgress->setValue(imageProgress->value());

    frameRemainingTime->setText(imageCountDown.toString("hh:mm:ss"));
    gr_frameRemainingTime->setText(frameRemainingTime->text());
}

void CaptureCountsWidget::updateDownloadProgress(double timeLeft)
{
    imageCountDown.setHMS(0, 0, 0);
    imageCountDown = imageCountDown.addSecs(int(std::ceil(timeLeft)));
    frameRemainingTime->setText(imageCountDown.toString("hh:mm:ss"));
}

void CaptureCountsWidget::updateCaptureCountDown(int delta)
{
    overallCountDown  = overallCountDown.addSecs(delta);
    jobCountDown      = jobCountDown.addSecs(delta);
    sequenceCountDown = sequenceCountDown.addSecs(delta);

    // ensure that count downs do not overshoot
    if (overallCountDown.hour() == 23)
        overallCountDown.setHMS(0, 0, 0);
    if (jobCountDown.hour() == 23)
        jobCountDown.setHMS(0, 0, 0);
    if (sequenceCountDown.hour() == 23)
        sequenceCountDown.setHMS(0, 0, 0);

    // do not change overall remaining time if scheduler is in endless loop
    if (schedulerProcess == nullptr || schedulerProcess->activeJob() == nullptr ||
            schedulerProcess->activeJob()->getCompletionCondition() != Ekos::FINISH_LOOP)
    {
        overallRemainingTime->setText(overallCountDown.toString("hh:mm:ss"));
        gr_overallRemainingTime->setText(overallRemainingTime->text());
    }
    jobRemainingTime->setText(jobCountDown.toString("hh:mm:ss"));
    sequenceRemainingTime->setText(sequenceCountDown.toString("hh:mm:ss"));
    gr_sequenceRemainingTime->setText(sequenceRemainingTime->text());
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

void CaptureCountsWidget::updateCaptureStatus(Ekos::CaptureState status)
{
    overallCountDown.setHMS(0, 0, 0);
    bool infinite_loop = false;
    int total_remaining_time = 0, total_completed = 0, total_count = 0;
    double total_percentage = 0;
    // use this value if no scheduler is running and job name otherwise
    QString total_label = "Total";

    // determine total number of frames and completed ones - used either for
    // total numbers if scheduler is not used - and for job figures in the text
    // display if the scheduler is used
    double capture_total_percentage = captureProcess->getProgressPercentage();
    int    capture_remaining_time   = captureProcess->getOverallRemainingTime();
    int capture_total_count = 0, capture_total_completed = 0;
    for (int i = 0; i < captureProcess->getJobCount(); i++)
    {
        capture_total_count     += captureProcess->getJobImageCount(i);
        capture_total_completed += captureProcess->getJobImageProgress(i);
    }


    if (schedulerProcess != nullptr && schedulerProcess->activeJob() != nullptr)
    {
        total_label = schedulerProcess->getCurrentJobName();
        // FIXME: accessing the completed count might be one too low due to concurrency of updating the count and this loop
        total_completed = schedulerProcess->activeJob()->getCompletedCount();
        total_count     = schedulerProcess->activeJob()->getSequenceCount();
        infinite_loop   = (schedulerProcess->activeJob()->getCompletionCondition() == Ekos::FINISH_LOOP);
        if (total_count > 0)
            total_percentage = (100 * total_completed) / total_count;
        if (schedulerProcess->activeJob()->getEstimatedTime() > 0)
            total_remaining_time = int(schedulerProcess->activeJob()->getEstimatedTime());
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
                overallCountDown = overallCountDown.addSecs(total_remaining_time);
                gr_overallProgressBar->setValue(total_percentage);
            }

            // display overall remainings
            overallLabel->setText(QString("%1 (%2/%3)")
                                  .arg(total_label)
                                  .arg(total_completed)
                                  .arg(infinite_loop ? QString("-") : QString::number(total_count)));
            gr_overallLabel->setText(overallLabel->text());

            // update job remaining time if run from the scheduler
            bool show_job_progress = (schedulerProcess != nullptr && schedulerProcess->activeJob() != nullptr);
            jobLabel->setVisible(show_job_progress);
            jobRemainingTime->setVisible(show_job_progress);
            if (show_job_progress)
            {
                jobCountDown.setHMS(0, 0, 0);
                jobCountDown = jobCountDown.addSecs(captureProcess->getOverallRemainingTime());
                jobLabel->setText(QString("Job (%1/%2)")
                                  .arg(capture_total_completed)
                                  .arg(capture_total_count));
            }

            // update sequence remaining time
            sequenceCountDown.setHMS(0, 0, 0);
            sequenceCountDown = sequenceCountDown.addSecs(captureProcess->getActiveJobRemainingTime());
    }
}

void CaptureCountsWidget::updateJobProgress(Ekos::SequenceJob *job)
{
    // display informations about the current active capture
    if (job->jobType() == SequenceJob::JOBTYPE_PREVIEW)
        setFrameInfo(i18n("Preview"),  job->getCoreProperty(SequenceJob::SJ_Filter).toString(),
                     job->getCoreProperty(SequenceJob::SJ_Exposure).toDouble(), job->getCoreProperty(SequenceJob::SJ_Binning).toPoint().x(),
                     job->getCoreProperty(SequenceJob::SJ_Binning).toPoint().y(), job->getCoreProperty(SequenceJob::SJ_Gain).toDouble());
    else
        setFrameInfo(CCDFrameTypeNames[job->getFrameType()], job->getCoreProperty(SequenceJob::SJ_Filter).toString(),
                     job->getCoreProperty(SequenceJob::SJ_Exposure).toDouble(), job->getCoreProperty(SequenceJob::SJ_Binning).toPoint().x(),
                     job->getCoreProperty(SequenceJob::SJ_Binning).toPoint().y(), job->getCoreProperty(SequenceJob::SJ_Gain).toDouble());

    // display sequence progress in the graphical view
    gr_sequenceProgressBar->setRange(0, job->getCoreProperty(SequenceJob::SJ_Count).toInt());
    gr_sequenceProgressBar->setValue(job->getCompleted());
    sequenceLabel->setText(QString("%1 (%3/%4)")
                           .arg(frameLabel(CCDFrameTypeNames[job->getFrameType()],
                                           job->getCoreProperty(SequenceJob::SJ_Filter).toString()))
                           .arg(job->getCompleted()).arg(job->getCoreProperty(SequenceJob::SJ_Count).toInt()));
    gr_sequenceLabel->setText(sequenceLabel->text());
}

void CaptureCountsWidget::setEnabled(bool enabled)
{
    QWidget::setEnabled(enabled);
    overallLabel->setEnabled(enabled);
    gr_overallLabel->setEnabled(enabled);
}
