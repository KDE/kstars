/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>
    SPDX-FileCopyrightText: 2021 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "capturecountswidget.h"

CaptureCountsWidget::CaptureCountsWidget(QWidget *parent) : QWidget(parent)
{
    setupUi(this);
    captureProgress->setValue(0);
    sequenceProgress->setValue(0);
    sequenceProgress->setDecimals(0);
    sequenceProgress->setFormat("%v");
    imageProgress->setValue(0);
    imageProgress->setDecimals(0);
    imageProgress->setFormat("%v");
    imageProgress->setBarStyle(QRoundProgressBar::StyleLine);
    captureProgress->setDecimals(0);
}

void CaptureCountsWidget::shareCaptureProcess(Ekos::Capture *process) {
    captureProcess = process;

    if (captureProcess != nullptr)
    {
        connect(captureProcess, &Ekos::Capture::newDownloadProgress, this, &CaptureCountsWidget::updateDownloadProgress);
        connect(captureProcess, &Ekos::Capture::newExposureProgress, this, &CaptureCountsWidget::updateExposureProgress);
    }
}

void CaptureCountsWidget::updateCaptureCountDown(int delta)
{
    overallCountDown = overallCountDown.addSecs(delta);
    if (overallCountDown.hour() == 23)
        overallCountDown.setHMS(0, 0, 0);

    sequenceCountDown = sequenceCountDown.addSecs(delta);
    if (sequenceCountDown.hour() == 23)
        sequenceCountDown.setHMS(0, 0, 0);

    overallRemainingTime->setText(overallCountDown.toString("hh:mm:ss"));
    sequenceRemainingTime->setText(sequenceCountDown.toString("hh:mm:ss"));
}

void CaptureCountsWidget::reset()
{
    imageLabel->setText(i18n("Image"));
    sequenceLabel->setText(i18n("Sequence"));
    overallLabel->setText(i18n("Overall"));
    imageProgress->setValue(0);
    sequenceProgress->setValue(0);
    captureProgress->setValue(0);
    overallRemainingTime->setText("--:--:--");
    sequenceRemainingTime->setText("--:--:--");
    imageRemainingTime->setText("--:--:--");
}

void CaptureCountsWidget::updateCaptureStatus(Ekos::CaptureState status)
{
    overallCountDown.setHMS(0, 0, 0);
    bool infinite_loop = false;
    int total_remaining_time = 0;
    double total_percentage = 0;

    if (schedulerProcess != nullptr && schedulerProcess->getCurrentJob() != nullptr)
    {
        // FIXME: accessing the completed count might be one too low due to concurrency of updating the count and this loop
        int total_completed = schedulerProcess->getCurrentJob()->getCompletedCount();
        int total_count     = schedulerProcess->getCurrentJob()->getSequenceCount();
        infinite_loop       = (schedulerProcess->getCurrentJob()->getCompletionCondition() == SchedulerJob::FINISH_LOOP);
        overallLabel->setText(QString("Schedule: %1 (%2/%3)")
                              .arg(schedulerProcess->getCurrentJobName())
                              .arg(total_completed)
                              .arg(infinite_loop ? QString("-") : QString::number(total_count)));
        if (total_count > 0)
            total_percentage = (100 * total_completed) / total_count;
        if (schedulerProcess->getCurrentJob()->getEstimatedTime() > 0)
            total_remaining_time = schedulerProcess->getCurrentJob()->getEstimatedTime();
    }
    else
    {
        total_percentage = captureProcess->getProgressPercentage();
        total_remaining_time = captureProcess->getOverallRemainingTime();
    }

    switch (status)
    {
        case Ekos::CAPTURE_IDLE:
        /* Fall through */
        case Ekos::CAPTURE_ABORTED:
        /* Fall through */
        case Ekos::CAPTURE_COMPLETE:
            imageProgress->setValue(0);
        break;
    default:
        if (infinite_loop == false)
        {
            captureProgress->setValue(total_percentage);
            overallCountDown = overallCountDown.addSecs(total_remaining_time);
        }
        else
        {
            captureProgress->setValue(0);
            overallRemainingTime->setText("--:--:--");
        }
        captureProgress->setEnabled(infinite_loop == false);
        overallRemainingTime->setEnabled(infinite_loop == false);

        sequenceCountDown.setHMS(0, 0, 0);
        sequenceCountDown = sequenceCountDown.addSecs(captureProcess->getActiveJobRemainingTime());
    }
}

void CaptureCountsWidget::updateDownloadProgress(double timeLeft)
{
    imageCountDown.setHMS(0, 0, 0);
    imageCountDown = imageCountDown.addSecs(timeLeft);
    imageRemainingTime->setText(imageCountDown.toString("hh:mm:ss"));
}

void CaptureCountsWidget::updateExposureProgress(Ekos::SequenceJob *job)
{
    imageCountDown.setHMS(0, 0, 0);
    imageCountDown = imageCountDown.addSecs(job->getExposeLeft() + captureProcess->getEstimatedDownloadTime());
    if (imageCountDown.hour() == 23)
        imageCountDown.setHMS(0, 0, 0);

    imageProgress->setRange(0, job->getExposure());
    imageProgress->setValue(job->getExposeLeft());

    imageRemainingTime->setText(imageCountDown.toString("hh:mm:ss"));
}

void CaptureCountsWidget::updateCaptureProgress(Ekos::SequenceJob *job)
{
    int completed = job->getCompleted();

    if (job->isPreview() == false)
    {
        imageLabel->setText(QString("%1%2 %3s")
                            .arg(CCDFrameTypeNames[job->getFrameType()])
                .arg(static_cast<QString>(job->getFilterName().isEmpty() ? QString("") : " " + job->getFilterName()))
                .arg(job->getExposure(), 0, 'f', job->getExposure() >= 1 ? 1 : 3));

        sequenceLabel->setText(QString("Frame %3/%4 - Job %1/%2")
                               .arg(captureProcess->getActiveJobID() + 1)
                               .arg(captureProcess->getJobCount())
                               .arg(std::min(completed+1, job->getCount()))
                               .arg(job->getCount()));
    }
    else
        sequenceLabel->setText(i18n("Preview"));

    sequenceProgress->setRange(0, job->getCount());
    sequenceProgress->setValue(completed);
}

void CaptureCountsWidget::setEnabled(bool enabled)
{
    QWidget::setEnabled(enabled);
    imageProgress->setEnabled(enabled);
    sequenceProgress->setEnabled(enabled);
    captureProgress->setEnabled(enabled);
}
