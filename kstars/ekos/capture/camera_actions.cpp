/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>
    SPDX-FileCopyrightText: 2024 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "camera.h"
#include "capturemodulestate.h"
#include "capturedeviceadaptor.h" // For devices()
#include "cameraprocess.h"     // For process()
#include "sequencejob.h"       // For SequenceJob and activeJob()
#include "ekos/auxiliary/darklibrary.h" // For DarkLibrary::Instance()
#include "Options.h" // For Options

#include <ekos_capture_debug.h>
#include <auxiliary/ksnotification.h>

// These macros are used by functions in this file.
// Ideally, they should be in a common header or camera.h
#define QCDEBUG   qCDebug(KSTARS_EKOS_CAPTURE) << QString("[%1]").arg(getCameraName())
#define QCINFO    qCInfo(KSTARS_EKOS_CAPTURE) << QString("[%1]").arg(getCameraName())
#define QCWARNING qCWarning(KSTARS_EKOS_CAPTURE) << QString("[%1]").arg(getCameraName())

namespace Ekos
{

// Helper function moved from camera.cpp (anonymous namespace)
QString frameLabel(CCDFrameType type, const QString &filter)
{
    switch(type)
    {
        case FRAME_LIGHT:
        case FRAME_VIDEO:
            if (filter.size() == 0)
                return CCDFrameTypeNames[type];
            else
                return filter;
            break;
        case FRAME_FLAT:
            if (filter.size() == 0)
                return CCDFrameTypeNames[type];
            else
                return QString("%1 %2").arg(filter).arg(CCDFrameTypeNames[type]);
            break;
        case FRAME_BIAS:
        case FRAME_DARK:
        case FRAME_NONE:
        default:
            return CCDFrameTypeNames[type];
    }
}

void Camera::start()
{
    process()->startNextPendingJob();
}

void Camera::stop(CaptureState targetState)
{
    process()->stopCapturing(targetState);
}

void Camera::pause()
{
    process()->pauseCapturing();
    updateStartButtons(false, true);
}

void Camera::toggleSequence()
{
    const CaptureState capturestate = state()->getCaptureState();
    if (capturestate == CAPTURE_PAUSE_PLANNED || capturestate == CAPTURE_PAUSED)
        updateStartButtons(true, false);

    process()->toggleSequence();
}

void Camera::capturePreview()
{
    process()->capturePreview();
}

void Camera::startFraming()
{
    process()->capturePreview(true);
}

void Camera::updateDownloadProgress(double downloadTimeLeft, const QString &devicename)
{
    frameRemainingTime->setText(state()->imageCountDown().toString("hh:mm:ss"));
    emit newDownloadProgress(downloadTimeLeft, devicename);
}

void Camera::updateCaptureCountDown(int deltaMillis)
{
    state()->imageCountDownAddMSecs(deltaMillis);
    state()->sequenceCountDownAddMSecs(deltaMillis);
    frameRemainingTime->setText(state()->imageCountDown().toString("hh:mm:ss"));
    if (!isActiveJobPreview())
        jobRemainingTime->setText(state()->sequenceCountDown().toString("hh:mm:ss"));
    else
        jobRemainingTime->setText("--:--:--");
}

void Camera::imageCapturingCompleted()
{
    if (activeJob().isNull())
        return;

    // In case we're framing, let's return quickly to continue the process.
    if (state()->isLooping())
    {
        captureStatusWidget->setStatus(i18n("Framing..."), Qt::darkGreen);
        return;
    }

    // If fast exposure is off, disconnect exposure progress
    // otherwise, keep it going since it fires off from driver continuous capture process.
    if (devices()->getActiveCamera()->isFastExposureEnabled() == false)
        DarkLibrary::Instance()->disconnect(this); // Assuming DarkLibrary is accessible

    // Do not display notifications for very short captures
    if (activeJob()->getCoreProperty(SequenceJob::SJ_Exposure).toDouble() >= 1)
        KSNotification::event(QLatin1String("EkosCaptureImageReceived"), i18n("Captured image received"),
                              KSNotification::Capture);

    // If it was initially set as pure preview job and NOT as preview for calibration
    if (activeJob()->jobType() == SequenceJob::JOBTYPE_PREVIEW)
        return;

    /* The image progress has now one more capture */
    imgProgress->setValue(activeJob()->getCompleted());
}

void Camera::captureStopped()
{
    imgProgress->reset();
    imgProgress->setEnabled(false);

    frameRemainingTime->setText("--:--:--");
    jobRemainingTime->setText("--:--:--");
    frameInfoLabel->setText(i18n("Expose (-/-):"));

    // stopping to CAPTURE_IDLE means that capturing will continue automatically
    auto captureState = state()->getCaptureState();
    if (captureState == CAPTURE_ABORTED || captureState == CAPTURE_COMPLETE)
        updateStartButtons(false, false);
}

void Camera::processingFITSfinished(bool success)
{
    // do nothing in case of failure
    if (success == false)
        return;

    // If this is a preview job, make sure to enable preview button after
    if (devices()->getActiveCamera()
            && devices()->getActiveCamera()->getUploadMode() != ISD::Camera::UPLOAD_REMOTE)
        previewB->setEnabled(true);

    imageCapturingCompleted();
}

void Camera::captureRunning()
{
    emit captureStarting(activeJob()->getCoreProperty(SequenceJob::SJ_Exposure).toDouble(),
                         activeJob()->getCoreProperty(SequenceJob::SJ_Filter).toString());
    if (isActiveJobPreview())
        frameInfoLabel->setText("Expose (-/-):");
    else if (activeJob()->getFrameType() != FRAME_VIDEO)
        frameInfoLabel->setText(QString("%1 (%L2/%L3):").arg(frameLabel(activeJob()->getFrameType(),
                                activeJob()->getCoreProperty(SequenceJob::SJ_Filter).toString()))
                                .arg(activeJob()->getCompleted()).arg(activeJob()->getCoreProperty(
                                            SequenceJob::SJ_Count).toInt()));
    else
        frameInfoLabel->setText(QString("%1 (%2x%L3s):").arg(frameLabel(activeJob()->getFrameType(),
                                activeJob()->getCoreProperty(SequenceJob::SJ_Filter).toString()))
                                .arg(activeJob()->getCoreProperty(SequenceJob::SJ_Count).toInt())
                                .arg(activeJob()->getCoreProperty(SequenceJob::SJ_Exposure).toDouble(), 0, 'f', 3));

    // ensure that the download time label is visible
    avgDownloadTime->setVisible(true);
    avgDownloadLabel->setVisible(true);
    secLabel->setVisible(true);
    // show estimated download time
    avgDownloadTime->setText(QString("%L1").arg(state()->averageDownloadTime(), 0, 'd', 2));

    // avoid logging that we captured a temporary file
    if (state()->isLooping() == false)
    {
        if (activeJob()->getFrameType() == FRAME_VIDEO)
            appendLogText(i18n("Capturing %1 x %2-second %3 video...",
                               activeJob()->getCoreProperty(SequenceJob::SJ_Count).toInt(),
                               QString("%L1").arg(activeJob()->getCoreProperty(SequenceJob::SJ_Exposure).toDouble(), 0, 'f', 3),
                               activeJob()->getCoreProperty(SequenceJob::SJ_Filter).toString()));

        else if (activeJob()->jobType() != SequenceJob::JOBTYPE_PREVIEW)
            appendLogText(i18n("Capturing %1-second %2 image...",
                               QString("%L1").arg(activeJob()->getCoreProperty(SequenceJob::SJ_Exposure).toDouble(), 0, 'f', 3),
                               activeJob()->getCoreProperty(SequenceJob::SJ_Filter).toString()));
    }
}

void Camera::captureImageStarted()
{
    if (devices()->filterWheel() != nullptr)
    {
        updateCurrentFilterPosition();
    }

    // necessary since the status widget doesn't store the calibration stage
    if (activeJob()->getCalibrationStage() == SequenceJobState::CAL_CALIBRATION)
        captureStatusWidget->setStatus(i18n("Calibrating..."), Qt::yellow);
}

void Camera::jobExecutionPreparationStarted()
{
    if (activeJob() == nullptr)
    {
        // this should never happen
        qWarning(KSTARS_EKOS_CAPTURE) << "jobExecutionPreparationStarted with null state()->getActiveJob().";
        return;
    }
    if (activeJob()->jobType() == SequenceJob::JOBTYPE_PREVIEW)
        updateStartButtons(true, false);
}

void Camera::jobPrepared(const QSharedPointer<SequenceJob> &job)
{
    int index = state()->allJobs().indexOf(job);
    if (index >= 0)
        queueTable->selectRow(index);

    if (activeJob()->jobType() != SequenceJob::JOBTYPE_PREVIEW)
    {
        // set the progress info
        imgProgress->setEnabled(true);
        imgProgress->setMaximum(activeJob()->getCoreProperty(SequenceJob::SJ_Count).toInt());
        imgProgress->setValue(activeJob()->getCompleted());
    }
}

void Camera::setTargetName(const QString &newTargetName)
{
    // target is changed only if no job is running
    if (activeJob() == nullptr)
    {
        // set the target name in the currently selected job
        targetNameT->setText(newTargetName);
        auto rows = queueTable->selectionModel()->selectedRows();
        if(rows.count() > 0)
        {
            // take the first one, since we are in single selection mode
            int pos = rows.constFirst().row();

            if (state()->allJobs().size() > pos)
                state()->allJobs().at(pos)->setCoreProperty(SequenceJob::SJ_TargetName, newTargetName);
        }

        emit captureTarget(newTargetName);
    }
}

void Camera::updateTargetDistance(double targetDiff)
{
    // ensure that the drift is visible
    targetDriftLabel->setVisible(true);
    targetDrift->setVisible(true);
    targetDriftUnit->setVisible(true);
    // update the drift value
    targetDrift->setText(QString("%L1").arg(targetDiff, 0, 'd', 1));
}

void Camera::updateStartButtons(bool start, bool pause)
{
    if (start)
    {
        // start capturing, therefore next possible action is stopping
        startB->setIcon(QIcon::fromTheme("media-playback-stop"));
        startB->setToolTip(i18n("Stop Sequence"));
    }
    else
    {
        // stop capturing, therefore next possible action is starting
        startB->setIcon(QIcon::fromTheme("media-playback-start"));
        startB->setToolTip(i18n(pause ? "Resume Sequence" : "Start Sequence"));
    }
    pauseB->setEnabled(start && !pause);
}

void Camera::setBusy(bool enable)
{
    previewB->setEnabled(!enable);
    loopB->setEnabled(!enable);
    opticalTrainCombo->setEnabled(!enable);
    trainB->setEnabled(!enable);

    foreach (QAbstractButton * button, queueEditButtonGroup->buttons())
        button->setEnabled(!enable);
}

void Camera::updatePrepareState(CaptureState prepareState)
{
    state()->setCaptureState(prepareState);

    if (activeJob() == nullptr)
    {
        qWarning(KSTARS_EKOS_CAPTURE) << "updatePrepareState with null activeJob().";
        // Everything below depends on activeJob(). Just return.
        return;
    }

    switch (prepareState)
    {
        case CAPTURE_SETTING_TEMPERATURE:
            appendLogText(i18n("Setting temperature to %1 °C...", activeJob()->getTargetTemperature()));
            captureStatusWidget->setStatus(i18n("Set Temp to %1 °C...", activeJob()->getTargetTemperature()),
                                           Qt::yellow);
            break;
        case CAPTURE_GUIDER_DRIFT:
            appendLogText(i18n("Waiting for guide drift below %1\"...", Options::startGuideDeviation()));
            captureStatusWidget->setStatus(i18n("Wait for Guider < %1\"...", Options::startGuideDeviation()), Qt::yellow);
            break;

        case CAPTURE_SETTING_ROTATOR:
            appendLogText(i18n("Setting camera to %1 degrees E of N...", activeJob()->getTargetRotation()));
            captureStatusWidget->setStatus(i18n("Set Camera to %1 deg...", activeJob()->getTargetRotation()),
                                           Qt::yellow);
            break;

        default:
            break;
    }
}

} // namespace Ekos
