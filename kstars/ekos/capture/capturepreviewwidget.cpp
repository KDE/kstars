/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>
    SPDX-FileCopyrightText: 2021 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "capturepreviewwidget.h"
#include "sequencejob.h"
#include <ekos_capture_debug.h>
#include "ksutils.h"
#include "ksmessagebox.h"
#include "ekos/mount/mount.h"
#include "Options.h"
#include "capture.h"
#include "sequencejob.h"
#include "fitsviewer/fitsdata.h"
#include "fitsviewer/summaryfitsview.h"
#include "ekos/scheduler/scheduler.h"

using Ekos::SequenceJob;

CapturePreviewWidget::CapturePreviewWidget(QWidget *parent) : QWidget(parent)
{
    setupUi(this);
    overlay = new CaptureProcessOverlay();
    overlay->setVisible(false);
    // history navigation
    connect(overlay->historyBackwardButton, &QPushButton::clicked, this, &CapturePreviewWidget::showPreviousFrame);
    connect(overlay->historyForwardButton, &QPushButton::clicked, this, &CapturePreviewWidget::showNextFrame);
    // deleting of captured frames
    connect(overlay->deleteCurrentFrameButton, &QPushButton::clicked, this, &CapturePreviewWidget::deleteCurrentFrame);
}

void CapturePreviewWidget::shareCaptureModule(Ekos::Capture *module)
{
    captureModule = module;
    captureCountsWidget->shareCaptureProcess(module);

    if (captureModule != nullptr)
    {
        connect(captureModule, &Ekos::Capture::newDownloadProgress, captureCountsWidget,
                &CaptureCountsWidget::updateDownloadProgress);
        connect(captureModule, &Ekos::Capture::newExposureProgress, captureCountsWidget,
                &CaptureCountsWidget::updateExposureProgress);
        connect(captureModule, &Ekos::Capture::captureTarget, this, &CapturePreviewWidget::setTargetName);
    }
}

void CapturePreviewWidget::shareSchedulerModule(Ekos::Scheduler *module)
{
    schedulerModule = module;
    captureCountsWidget->shareSchedulerProcess(module);
}

void CapturePreviewWidget::shareMountModule(Ekos::Mount *module)
{
    mountModule = module;
    connect(mountModule, &Ekos::Mount::newTargetName, this, &CapturePreviewWidget::setTargetName);
}

void CapturePreviewWidget::updateJobProgress(Ekos::SequenceJob *job, const QSharedPointer<FITSData> &data)
{
    // forward first to the counting widget
    captureCountsWidget->updateJobProgress(job);

    // without FITS data, we do nothing
    if (data == nullptr)
        return;

    // cache frame meta data
    m_currentFrame.frameType = job->getFrameType();
    if (job->getFrameType() == FRAME_LIGHT)
    {
        if (schedulerModule != nullptr && schedulerModule->activeJob() != nullptr)
            m_currentFrame.target = schedulerModule->activeJob()->getName();
        else
            m_currentFrame.target = m_mountTarget;
    }
    else
        m_currentFrame.target = "";

    m_currentFrame.filterName  = job->getCoreProperty(SequenceJob::SJ_Filter).toString();
    m_currentFrame.exptime     = job->getCoreProperty(SequenceJob::SJ_Exposure).toDouble();
    m_currentFrame.targetdrift = -1.0; // will be updated later
    m_currentFrame.binning     = job->getCoreProperty(SequenceJob::SJ_Binning).toPoint();
    m_currentFrame.gain        = job->getCoreProperty(SequenceJob::SJ_Gain).toDouble();
    m_currentFrame.offset      = job->getCoreProperty(SequenceJob::SJ_Offset).toDouble();
    m_currentFrame.filename    = data->filename();
    m_currentFrame.width       = data->width();
    m_currentFrame.height      = data->height();

    const auto ISOIndex = job->getCoreProperty(SequenceJob::SJ_Offset).toInt();
    if (ISOIndex >= 0 && ISOIndex <= captureModule->captureISOS->count())
        m_currentFrame.iso = captureModule->captureISOS->itemText(ISOIndex);
    else
        m_currentFrame.iso = "";

    // add it to the overlay
    overlay->addFrameData(m_currentFrame);
    overlay->setVisible(true);

    // load frame
    if (m_fitsPreview != nullptr && Options::useSummaryPreview())
        m_fitsPreview->loadData(data);
}

void CapturePreviewWidget::showNextFrame()
{
    overlay->setEnabled(false);
    if (overlay->showNextFrame())
        m_fitsPreview->loadFile(overlay->currentFrame().filename);
    // Hint: since the FITSView loads in the background, we have to wait for FITSView::load() to enable the layer
    else
        overlay->setEnabled(true);
}

void CapturePreviewWidget::showPreviousFrame()
{
    overlay->setEnabled(false);
    if (overlay->showPreviousFrame())
        m_fitsPreview->loadFile(overlay->currentFrame().filename);
    // Hint: since the FITSView loads in the background, we have to wait for FITSView::load() to enable the layer
    else
        overlay->setEnabled(true);
}

void CapturePreviewWidget::deleteCurrentFrame()
{
    overlay->setEnabled(false);
    if (overlay->hasFrames() == false)
        // nothing to delete
        return;

    // make sure that the history does not change inbetween
    int pos = overlay->currentPosition();
    CaptureProcessOverlay::FrameData current = overlay->getFrame(pos);
    QFile *file = new QFile(current.filename);

    // prepare a warning dialog
    // move to trash or delete permanently
    QCheckBox *permanentlyDeleteCB = new QCheckBox(i18n("Delete directly, do not move to trash."));
    permanentlyDeleteCB->setChecked(m_permanentlyDelete);
    KSMessageBox::Instance()->setCheckBox(permanentlyDeleteCB);
    connect(permanentlyDeleteCB, &QCheckBox::toggled, this, [this](bool checked)
    {
        this->m_permanentlyDelete = checked;
    });
    // Delete
    connect(KSMessageBox::Instance(), &KSMessageBox::accepted, this, [this, pos, file]()
    {
        KSMessageBox::Instance()->disconnect(this);
        bool success = false;
        if (this->m_permanentlyDelete == false && (success = file->moveToTrash()))
        {
            qCInfo(KSTARS_EKOS_CAPTURE) << overlay->currentFrame().filename << "moved to Trash.";
        }
        else if (this->m_permanentlyDelete && (success = file->remove()))
        {
            qCInfo(KSTARS_EKOS_CAPTURE) << overlay->currentFrame().filename << "deleted.";
        }

        if (success)
        {
            // delete it from the history and update the FITS view
            if (overlay->deleteFrame(pos) && overlay->hasFrames())
            {
                m_fitsPreview->loadFile(overlay->currentFrame().filename);
                // Hint: since the FITSView loads in the background, we have to wait for FITSView::load() to enable the layer
            }
            else
            {
                m_fitsPreview->clearData();
                overlay->setEnabled(true);
            }
        }
        else
        {
            qCWarning(KSTARS_EKOS_CAPTURE) << "Deleting" << overlay->currentFrame().filename << "failed!";
            // give up
            overlay->setEnabled(true);
        }
        // clear the check box
        KSMessageBox::Instance()->setCheckBox(nullptr);
    });

    // Cancel
    connect(KSMessageBox::Instance(), &KSMessageBox::rejected, this, [this]()
    {
        KSMessageBox::Instance()->disconnect(this);
        // clear the check box
        KSMessageBox::Instance()->setCheckBox(nullptr);
        //do nothing
        overlay->setEnabled(true);
    });

    // open the message box
    QFileInfo fileinfo(current.filename);
    KSMessageBox::Instance()->warningContinueCancel(i18n("Do you really want to delete %1 from the file system?",
            fileinfo.fileName()),
            i18n("Delete %1", fileinfo.fileName()), 0, false, i18n("Delete"));

}

void CapturePreviewWidget::setSummaryFITSView(SummaryFITSView *view)
{
    m_fitsPreview = view;
    QVBoxLayout * vlayout = new QVBoxLayout();
    vlayout->setContentsMargins(0, 0, 0, 0);
    vlayout->addWidget(view);
    previewWidget->setLayout(vlayout);
    previewWidget->setContentsMargins(0, 0, 0, 0);

    // initialize the FITS data overlay
    // create vertically info box as overlay
    QVBoxLayout *layout = new QVBoxLayout(view->processInfoWidget);
    layout->addWidget(overlay, 0);

    view->processInfoWidget->setLayout(layout);
    // react upon signals
    connect(view, &FITSView::loaded, [&]()
    {
        overlay->setEnabled(true);
    });
    connect(view, &FITSView::failed, [&]()
    {
        overlay->setEnabled(true);
    });
}

void CapturePreviewWidget::setEnabled(bool enabled)
{
    // forward to sub widget
    captureCountsWidget->setEnabled(enabled);
    QWidget::setEnabled(enabled);
}

void CapturePreviewWidget::reset()
{
    overlay->setVisible(false);
    // forward to sub widget
    captureCountsWidget->reset();
}

void CapturePreviewWidget::updateCaptureStatus(Ekos::CaptureState status)
{
    // forward to sub widgets
    captureStatusWidget->setCaptureState(status);
    captureCountsWidget->updateCaptureStatus(status);
}

void CapturePreviewWidget::updateTargetDistance(double targetDiff)
{
    // forward it to the overlay
    overlay->updateTargetDistance(targetDiff);
}

void CapturePreviewWidget::updateCaptureCountDown(int delta)
{
    // forward to sub widget
    captureCountsWidget->updateCaptureCountDown(delta);
}

void CapturePreviewWidget::setTargetName(QString name)
{
    targetLabel->setVisible(!name.isEmpty());
    mountTarget->setVisible(!name.isEmpty());
    mountTarget->setText(name);
    m_mountTarget = name;
    m_currentFrame.target = name;
}

