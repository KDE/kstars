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

    typedef struct CaptureCountsStruct
    {
        int count { -1 };            /* total frames count */
        int completed { -1 };        /* captured frames count */
        double remainingTime { -1 }; /* remaining time to complete (in seconds) */
        double totalTime { -1 };     /* total exposure time (in seconds) */
        QTime countDown;             /* timer for countdown to completion */
        bool changed { true };       /* flag if some value has changed */
    } CaptureCounts;

    explicit CaptureCountsWidget(QWidget *parent = nullptr);

    void setCurrentTrainName(const QString &name);

public slots:

    /**
     * @brief display the progress of the current exposure (remaining time etc.)
     * @param job currently active job
     * @param trainname train name of the camera reporting exposure process
     */
    void updateExposureProgress(const QSharedPointer<Ekos::SequenceJob> &job, const QString &trainname);

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
    /**
     * @brief refreshCaptureCounters Refresh the display of the counters for sequence, job and overall
     */
    void refreshCaptureCounters(const QString &trainname);

    /**
     * @brief captureState Retrieve the currently known capture state of a given optical train
     */
    Ekos::CaptureState captureState(const QString &trainname);

    /**
     * @brief isCaptureActive Check if the given optical train is currently capturing
     */
    bool isCaptureActive(const QString &trainname);

    QSharedPointer<Ekos::SchedulerModuleState> m_schedulerModuleState;
    Ekos::Capture *m_captureProcess = nullptr;

    // current camera train name
    QString m_currentTrainName = "";
    // is a capture preview active=
    bool m_isPreview = false;
    // capture counters
    QMap<QString, CaptureCounts> imageCounts;
    QMap<QString, CaptureCounts> sequenceCounts;
    QMap<QString, CaptureCounts> jobCounts;
    QMap<QString, CaptureCounts> totalCounts;
    // capture status per train
    QMap<QString, Ekos::CaptureState> captureStates;
    // refresh display of counts
    void refreshImageCounts(const QString &trainname);

    // cache frame data
    QMap<QString, CaptureProcessOverlay::FrameData> m_currentFrame;
};
