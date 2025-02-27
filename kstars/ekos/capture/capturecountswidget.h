/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>
    SPDX-FileCopyrightText: 2021 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QObject>
#include <QWidget>
#include <QTime>

#include "ui_capturecountswidget.h"
#include "captureprocessoverlay.h"
#include "ekos/ekos.h"

namespace Ekos
{
class Capture;
class SequenceJob;
class SchedulerModuleState;
}

class CaptureCountsWidget : public QWidget, public Ui::CaptureCountsWidget
{
    Q_OBJECT
public:
    friend class CapturePreviewWidget;

    explicit CaptureCountsWidget(QWidget *parent = nullptr);

    void setCurrentTrainName(const QString &name);

public slots:

    /**
     * @brief display the progress of the current exposure (remaining time etc.)
     * @param job currently active job
     * @param devicename device name of the camera reporting exposure process
     */
    void updateExposureProgress(const QSharedPointer<Ekos::SequenceJob> &job, const QString &devicename);

    /**
     * @brief display the download progress
     * @param timeLeft time left until the download is finished
     */
    void updateDownloadProgress(double timeLeft, const QString &devicename);


private:
    void shareCaptureProcess(Ekos::Capture *process) {m_captureProcess = process;}
    void shareSchedulerState(QSharedPointer<Ekos::SchedulerModuleState> state);

    /**
     * @brief update the count down value of the current exposure
     * @param delta time difference
     */
    void updateCaptureCountDown(int delta);

    /**
     * @brief update display when the capture status changes
     */
    void updateCaptureStatus(Ekos::CaptureState status, bool isPreview, const QString &devicename);

    /**
     * @brief display information about the currently running job
     * @param job currently active job
     * @param devicename name of the used camera device
     */
    void updateJobProgress(CaptureProcessOverlay::FrameData data, const QString &devicename);

    /**
     * @brief enable / disable display widgets
     */
    void setEnabled(bool enabled);

    /**
     * @brief reset display values
     */
    void reset();

    // informations about the current frame
    void setFrameInfo(const QString frametype, const QString filter = "", const double exptime = -1, const int xBin = -1, const int yBin = -1, const double gain = -1);

    /**
     * @brief showCurrentCameraInfo Display the capturing status informations for the selected camera device
     */
    void showCurrentCameraInfo();

    QSharedPointer<Ekos::SchedulerModuleState> m_schedulerModuleState;
    Ekos::Capture *m_captureProcess = nullptr;

    QMap<QString, QTime> imageCountDown;
    QMap<QString, QTime> sequenceCountDown;
    QMap<QString, QTime> jobCountDown;
    QMap<QString, QTime> overallCountDown;
    // current camera train name
    QString m_currentTrainName = "";

    // cache frame data
    QMap<QString, CaptureProcessOverlay::FrameData> m_currentFrame;
};
