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
#include "ekos/scheduler/schedulerjob.h"
#include "ekos/scheduler/schedulermodulestate.h"

using Ekos::SequenceJob;

CapturePreviewWidget::CapturePreviewWidget(QWidget *parent) : QWidget(parent)
{
    setupUi(this);
    m_overlay = QSharedPointer<CaptureProcessOverlay>(new CaptureProcessOverlay);
    m_overlay->setVisible(false);
    // capture device selection
    connect(trainSelectionCB, &QComboBox::currentTextChanged, this, &CapturePreviewWidget::selectedTrainChanged);
    // history navigation
    connect(m_overlay->historyBackwardButton, &QPushButton::clicked, this, &CapturePreviewWidget::showPreviousFrame);
    connect(m_overlay->historyForwardButton, &QPushButton::clicked, this, &CapturePreviewWidget::showNextFrame);
    // deleting of captured frames
    connect(m_overlay->deleteCurrentFrameButton, &QPushButton::clicked, this, &CapturePreviewWidget::deleteCurrentFrame);

    // make invisible until we have at least two cameras active
    trainSelectionCB->setVisible(false);
}

void CapturePreviewWidget::shareCaptureModule(Ekos::Capture *module)
{
    m_captureModule = module;
    captureCountsWidget->shareCaptureProcess(module);
    m_trainNames.clear();
    trainSelectionCB->clear();
    // make invisible until we have at least two cameras active
    trainSelectionCB->setVisible(false);
    captureLabel->setVisible(true);

    if (m_captureModule != nullptr)
    {
        connect(m_captureModule, &Ekos::Capture::newDownloadProgress, this, &CapturePreviewWidget::updateDownloadProgress);
        connect(m_captureModule, &Ekos::Capture::newExposureProgress, this, &CapturePreviewWidget::updateExposureProgress);
        connect(m_captureModule, &Ekos::Capture::captureTarget, this, &CapturePreviewWidget::setTargetName);
    }
}

void CapturePreviewWidget::shareSchedulerModuleState(QSharedPointer<Ekos::SchedulerModuleState> state)
{
    m_schedulerModuleState = state;
    captureCountsWidget->shareSchedulerState(state);
}

void CapturePreviewWidget::shareMountModule(Ekos::Mount *module)
{
    m_mountModule = module;
    connect(m_mountModule, &Ekos::Mount::newTargetName, this, &CapturePreviewWidget::setTargetName);
}

void CapturePreviewWidget::updateJobProgress(const QSharedPointer<Ekos::SequenceJob> &job,
        const QSharedPointer<FITSData> &data, const QString &trainname)
{
    // ensure that we have all camera device names in the selection
    if (!m_trainNames.contains(trainname))
    {
        m_trainNames.append(trainname);
        trainSelectionCB->addItem(trainname);

        trainSelectionCB->setVisible(m_trainNames.count() >= 2);
        captureLabel->setVisible(m_trainNames.count() < 2);
    }

    if (job != nullptr)
    {
        // cache frame meta data
        m_currentFrame[trainname].frameType = job->getFrameType();
        if (job->getFrameType() == FRAME_LIGHT)
        {
            if (m_schedulerModuleState != nullptr && m_schedulerModuleState->activeJob() != nullptr)
                m_currentFrame[trainname].target = m_schedulerModuleState->activeJob()->getName();
            else
                m_currentFrame[trainname].target = m_mountTarget;
        }
        else
        {
            m_currentFrame[trainname].target = "";
        }

        m_currentFrame[trainname].filterName  = job->getCoreProperty(SequenceJob::SJ_Filter).toString();
        m_currentFrame[trainname].exptime     = job->getCoreProperty(SequenceJob::SJ_Exposure).toDouble();
        m_currentFrame[trainname].targetdrift = -1.0; // will be updated later
        m_currentFrame[trainname].binning     = job->getCoreProperty(SequenceJob::SJ_Binning).toPoint();
        m_currentFrame[trainname].gain        = job->getCoreProperty(SequenceJob::SJ_Gain).toDouble();
        m_currentFrame[trainname].offset      = job->getCoreProperty(SequenceJob::SJ_Offset).toDouble();
        m_currentFrame[trainname].jobType     = job->jobType();
        m_currentFrame[trainname].frameType   = job->getFrameType();
        m_currentFrame[trainname].count       = job->getCoreProperty(SequenceJob::SJ_Count).toInt();
        m_currentFrame[trainname].completed   = job->getCompleted();

        if (data != nullptr)
        {
            m_currentFrame[trainname].filename    = data->filename();
            m_currentFrame[trainname].width       = data->width();
            m_currentFrame[trainname].height      = data->height();
        }

        const auto ISOIndex = job->getCoreProperty(SequenceJob::SJ_Offset).toInt();
        if (ISOIndex >= 0 && ISOIndex <= m_captureModule->mainCamera()->captureISOS->count())
            m_currentFrame[trainname].iso = m_captureModule->mainCamera()->captureISOS->itemText(ISOIndex);
        else
            m_currentFrame[trainname].iso = "";
    }

    // forward first to the counting widget
    captureCountsWidget->updateJobProgress(m_currentFrame[trainname], trainname);

    // add it to the overlay if data is present
    if (!data.isNull())
    {
        m_overlay->addFrameData(m_currentFrame[trainname], trainname);
        m_overlay->setVisible(true);
    }

    // load frame
    if (m_fitsPreview != nullptr && Options::useSummaryPreview() && trainSelectionCB->currentText() == trainname)
        m_fitsPreview->loadData(data);
}

void CapturePreviewWidget::updateJobPreview(const QString &filePath)
{
    // without FITS filePath, we do nothing
    if (filePath == "")
        return;

    // load frame
    if (m_fitsPreview != nullptr && Options::useSummaryPreview())
        m_fitsPreview->loadFile(filePath);
}

void CapturePreviewWidget::showNextFrame()
{
    m_overlay->setEnabled(false);
    if (m_overlay->showNextFrame())
        m_fitsPreview->loadFile(m_overlay->currentFrame().filename);
    // Hint: since the FITSView loads in the background, we have to wait for FITSView::load() to enable the layer
    else
        m_overlay->setEnabled(true);
}

void CapturePreviewWidget::showPreviousFrame()
{
    m_overlay->setEnabled(false);
    if (m_overlay->showPreviousFrame())
        m_fitsPreview->loadFile(m_overlay->currentFrame().filename);
    // Hint: since the FITSView loads in the background, we have to wait for FITSView::load() to enable the layer
    else
        m_overlay->setEnabled(true);
}

void CapturePreviewWidget::deleteCurrentFrame()
{
    m_overlay->setEnabled(false);
    if (m_overlay->hasFrames() == false)
        // nothing to delete
        return;

    // make sure that the history does not change inbetween
    int pos = m_overlay->currentPosition();
    CaptureProcessOverlay::FrameData current = m_overlay->getFrame(pos);
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
            qCInfo(KSTARS_EKOS_CAPTURE) << m_overlay->currentFrame().filename << "moved to Trash.";
        }
        else if (this->m_permanentlyDelete && (success = file->remove()))
        {
            qCInfo(KSTARS_EKOS_CAPTURE) << m_overlay->currentFrame().filename << "deleted.";
        }

        if (success)
        {
            // delete it from the history and update the FITS view
            if (m_overlay->deleteFrame(pos) && m_overlay->hasFrames())
            {
                m_fitsPreview->loadFile(m_overlay->currentFrame().filename);
                // Hint: since the FITSView loads in the background, we have to wait for FITSView::load() to enable the layer
            }
            else
            {
                m_fitsPreview->clearData();
                m_overlay->setEnabled(true);
            }
        }
        else
        {
            qCWarning(KSTARS_EKOS_CAPTURE) << "Deleting" << m_overlay->currentFrame().filename <<
                                           "failed!";
            // give up
            m_overlay->setEnabled(true);
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
        m_overlay->setEnabled(true);
    });

    // open the message box
    QFileInfo fileinfo(current.filename);
    KSMessageBox::Instance()->warningContinueCancel(i18n("Do you really want to delete %1 from the file system?",
            fileinfo.fileName()),
            i18n("Delete %1", fileinfo.fileName()), 0, false, i18n("Delete"));

}

void CapturePreviewWidget::setSummaryFITSView(const QSharedPointer<SummaryFITSView> &view)
{
    m_fitsPreview = view;
    QVBoxLayout * vlayout = new QVBoxLayout();
    vlayout->setContentsMargins(0, 0, 0, 0);
    vlayout->addWidget(view.get());
    previewWidget->setLayout(vlayout);
    previewWidget->setContentsMargins(0, 0, 0, 0);

    // initialize the FITS data overlay
    // create vertically info box as overlay
    QVBoxLayout *layout = new QVBoxLayout(view->processInfoWidget);
    layout->addWidget(m_overlay.get(), 0);

    view->processInfoWidget->setLayout(layout);
    // react upon signals
    connect(view.get(), &FITSView::loaded, [&]()
    {
        m_overlay->setEnabled(true);
    });
    connect(view.get(), &FITSView::failed, [&]()
    {
        m_overlay->setEnabled(true);
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
    if (m_overlay->hasFrames())
        m_overlay->captureHistory().reset();
    m_overlay->setVisible(false);
    // forward to sub widget
    captureCountsWidget->reset();
}

void CapturePreviewWidget::updateCaptureStatus(Ekos::CaptureState status, bool isPreview, const QString &trainname)
{
    // forward to sub widgets
    captureStatusWidget->setCaptureState(status);
    captureCountsWidget->updateCaptureStatus(status, isPreview, trainname);
}

void CapturePreviewWidget::updateTargetDistance(double targetDiff)
{
    // forward it to the overlay
    m_overlay->updateTargetDistance(targetDiff);
}

void CapturePreviewWidget::updateCaptureCountDown(int delta)
{
    // forward to sub widget
    captureCountsWidget->updateCaptureCountDown(delta);
}

void CapturePreviewWidget::selectedTrainChanged(QString newName)
{
    m_overlay->setCurrentTrainName(newName);
    captureCountsWidget->setCurrentTrainName(newName);

    m_overlay->setEnabled(false);
    if (m_overlay->hasFrames())
    {
        // Hint: since the FITSView loads in the background, we have to wait for FITSView::load() to enable the layer
        m_fitsPreview->loadFile(m_overlay->currentFrame().filename);
    }
    else
    {
        m_fitsPreview->clearData();
        m_overlay->setEnabled(true);
    }
}

void CapturePreviewWidget::updateExposureProgress(const QSharedPointer<Ekos::SequenceJob> &job, const QString &trainname)
{
    if (trainname == trainSelectionCB->currentText())
        captureCountsWidget->updateExposureProgress(job, trainname);
}

void CapturePreviewWidget::updateDownloadProgress(double downloadTimeLeft, const QString &trainname)
{
    if (trainname == trainSelectionCB->currentText())
        captureCountsWidget->updateDownloadProgress(downloadTimeLeft, trainname);
}

void CapturePreviewWidget::setTargetName(QString name)
{
    targetLabel->setVisible(!name.isEmpty());
    mountTarget->setVisible(!name.isEmpty());
    mountTarget->setText(name);
    m_mountTarget = name;
    m_currentFrame[trainSelectionCB->currentText()].target = name;
}

