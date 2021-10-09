/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>
    SPDX-FileCopyrightText: 2021 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "capturepreviewwidget.h"
#include <ekos_capture_debug.h>
#include "ksutils.h"
#include "ksmessagebox.h"
#include "Options.h"

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

void CapturePreviewWidget::shareCaptureProcess(Ekos::Capture *process)
{
    captureProcess = process;
    captureCountsWidget->shareCaptureProcess(process);

    if (captureProcess != nullptr)
    {
        connect(captureProcess, &Ekos::Capture::newDownloadProgress, captureCountsWidget, &CaptureCountsWidget::updateDownloadProgress);
        connect(captureProcess, &Ekos::Capture::newExposureProgress, captureCountsWidget, &CaptureCountsWidget::updateExposureProgress);
    }
}

void CapturePreviewWidget::shareSchedulerProcess(Ekos::Scheduler *process)
{
    schedulerProcess = process;
    captureCountsWidget->shareSchedulerProcess(process);
}

void CapturePreviewWidget::shareMountProcess(Ekos::Mount *process)
{
    mountProcess = process;
    connect(mountProcess, &Ekos::Mount::newTarget, [&](SkyObject currentObject){
        m_mountTarget = currentObject.name();
    });
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
        if (schedulerProcess != nullptr && schedulerProcess->getCurrentJob() != nullptr)
            m_currentFrame.target = schedulerProcess->getCurrentJob()->getName();
        else
            m_currentFrame.target = m_mountTarget;
    }
    else
        m_currentFrame.target = "";

    m_currentFrame.filterName = job->getFilterName();
    m_currentFrame.exptime   = job->getExposure();
    m_currentFrame.xBin       = job->getXBin();
    m_currentFrame.yBin       = job->getYBin();
    m_currentFrame.gain       = job->getGain();
    m_currentFrame.offset     = job->getOffset();
    m_currentFrame.filename   = data->filename();
    m_currentFrame.width      = data->width();
    m_currentFrame.height     = data->height();

    if (job->getISOIndex() >= 0 && job->getISOIndex() <= captureProcess->captureISOS->count())
        m_currentFrame.iso = captureProcess->captureISOS->itemText(job->getISOIndex());
    else
        m_currentFrame.iso = "";

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
    // Delete
    connect(KSMessageBox::Instance(), &KSMessageBox::accepted, this, [this, pos, file]()
    {
        KSMessageBox::Instance()->disconnect(this);

        if (file->remove() == true)
        {
            qCInfo(KSTARS_EKOS_CAPTURE) << overlay->currentFrame().filename << "deleted.";
        }
        else
        {
            qCWarning(KSTARS_EKOS_CAPTURE) << "Deleting" << overlay->currentFrame().filename << "failed!";
            // give up
            overlay->setEnabled(true);
            return;
        }

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
    });

    // Cancel
    connect(KSMessageBox::Instance(), &KSMessageBox::rejected, this, [this]()
    {
        KSMessageBox::Instance()->disconnect(this);
        //do nothing
        overlay->setEnabled(true);
    });

    // open the message box
    QFileInfo fileinfo(current.filename);
    KSMessageBox::Instance()->warningContinueCancel(i18n("Do you really want to delete %1 from the file system?", fileinfo.fileName()),
            i18n("Delete %1", fileinfo.fileName()), 15, false, i18n("Delete"));

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
    connect(view, &FITSView::loaded, [&](){overlay->setEnabled(true);});
    connect(view, &FITSView::failed, [&](){overlay->setEnabled(true);});
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
    captureStatus->setText(Ekos::getCaptureStatusString(status));

    // update the data of the overlay
    if (status == Ekos::CAPTURE_IMAGE_RECEIVED)
    {
        overlay->addFrameData(m_currentFrame);
        overlay->setVisible(true);
    }

    // forward to sub widget
    captureCountsWidget->updateCaptureStatus(status);
}

void CapturePreviewWidget::updateCaptureCountDown(int delta)
{
    // forward to sub widget
    captureCountsWidget->updateCaptureCountDown(delta);
}
