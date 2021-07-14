/*  Ekos capture counting widget
    Copyright (C) 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>
                  2021 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
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

    void shareCaptureProcess(const QSharedPointer<Ekos::Capture> &process);
    void shareSchedulerProcess(const QSharedPointer<Ekos::Scheduler> &process) {schedulerProcess = QSharedPointer<Ekos::Scheduler>(process);}

    void updateCaptureCountDown(int delta);

    void reset();

public slots:
    void updateCaptureStatus(Ekos::CaptureState status);
    void updateDownloadProgress(double timeLeft);
    void updateExposureProgress(Ekos::SequenceJob *job);
    void updateCaptureProgress(Ekos::SequenceJob *job);
    void setEnabled(bool enabled);

private:
    QSharedPointer<Ekos::Scheduler> schedulerProcess;
    QSharedPointer<Ekos::Capture> captureProcess;

    QTime imageCountDown;
    QTime sequenceCountDown;
    QTime overallCountDown;
};
