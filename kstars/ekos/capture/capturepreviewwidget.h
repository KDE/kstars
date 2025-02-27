/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>
    SPDX-FileCopyrightText: 2021 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "ui_capturepreviewwidget.h"
#include "captureprocessoverlay.h"

#include <QObject>
#include <QWidget>

class FITSData;
class SummaryFITSView;

namespace Ekos
{
class Capture;
class Mount;
class SchedulerModuleState;
class SequenceJob;
}

class CapturePreviewWidget : public QWidget, public Ui::CapturePreviewWidget
{
    Q_OBJECT
public:
    explicit CapturePreviewWidget(QWidget *parent = nullptr);

    void shareCaptureModule(Ekos::Capture *module);
    void shareSchedulerModuleState(QSharedPointer<Ekos::SchedulerModuleState> state);
    void shareMountModule(Ekos::Mount *module);

    /**
     * @brief display information about the currently running job
     * @param currently active job
     */
    void updateJobProgress(const QSharedPointer<Ekos::SequenceJob> &job, const QSharedPointer<FITSData> &data, const QString &devicename);

    /**
     * @brief update the preview image from a file
     * @param filePath to load
     */
    void updateJobPreview(const QString &filePath);

    /**
     * @brief Show the next frame from the capture history
     */
    void showNextFrame();

    /**
     * @brief Show the previous frame from the capture history
     */
    void showPreviousFrame();

    /**
     * @brief Delete the currently displayed frame
     */
    void deleteCurrentFrame();

    /**
     * @brief Set the summary FITS view
     */
    void setSummaryFITSView(const QSharedPointer<SummaryFITSView> &view);

    /**
     * @brief enable / disable display widgets
     */
    void setEnabled(bool enabled);

    /**
     * @brief reset display values
     */
    void reset();

    /**
     * @brief set the target name
     */
    void setTargetName(QString name);

public slots:
    /**
     * @brief update display when the capture status changes
     */
    void updateCaptureStatus(Ekos::CaptureState status, bool isPreview, const QString &trainname);

    /**
     * @brief Slot receiving the update of the current target distance.
     * @param targetDiff distance to the target in arcseconds.
     */
    void updateTargetDistance(double targetDiff);

    /**
     * @brief update the count down value of the current exposure
     * @param delta time difference
     */
    void updateCaptureCountDown(int delta);

    /**
     * @brief React upon changed camera device selection
     */
    void selectedTrainChanged(QString newName);

private:

    void updateExposureProgress(const QSharedPointer<Ekos::SequenceJob> &job, const QString &devicename);
    void updateDownloadProgress(double downloadTimeLeft, const QString &devicename);

    QSharedPointer<Ekos::SchedulerModuleState> m_schedulerModuleState = nullptr;
    Ekos::Capture *m_captureModule = nullptr;
    Ekos::Mount *m_mountModule = nullptr;

    // cache frame data
    QMap<QString, CaptureProcessOverlay::FrameData> m_currentFrame;

    // known train names
    QList<QString> m_trainNames;

    // target the mount is pointing to (may be different to the scheduler job name)
    QString m_mountTarget = "";

    // summary FITS view
    QSharedPointer<SummaryFITSView> m_fitsPreview;
    // FITS data overlay
    QSharedPointer<CaptureProcessOverlay> m_overlay;

    // move to trash or delete finally
    bool m_permanentlyDelete {false};
};
