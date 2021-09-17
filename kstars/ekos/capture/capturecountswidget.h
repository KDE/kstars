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
#include "ekos/scheduler/scheduler.h"
#include "ekos/capture/capture.h"
#include "ekos/capture/sequencejob.h"

class CaptureCountsWidget : public QWidget, public Ui::CaptureCountsWidget
{
    Q_OBJECT
public:
    explicit CaptureCountsWidget(QWidget *parent = nullptr);

    void shareCaptureProcess(Ekos::Capture *process);
    void shareSchedulerProcess(Ekos::Scheduler *process) {schedulerProcess = process;}

    void updateCaptureCountDown(int delta);

    void reset();

public slots:
    void updateCaptureStatus(Ekos::CaptureState status);
    void updateDownloadProgress(double timeLeft);
    void updateExposureProgress(Ekos::SequenceJob *job);
    void updateCaptureProgress(Ekos::SequenceJob *job);
    void setEnabled(bool enabled);

private:
    Ekos::Scheduler *schedulerProcess = nullptr;
    Ekos::Capture *captureProcess = nullptr;

    QTime imageCountDown;
    QTime sequenceCountDown;
    QTime overallCountDown;
};
