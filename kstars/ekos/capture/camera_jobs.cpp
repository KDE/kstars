/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>
    SPDX-FileCopyrightText: 2024 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "camera.h"
#include "capturemodulestate.h"
#include "capturedeviceadaptor.h"
#include "cameraprocess.h"
#include "sequencejob.h"
#include "ekos/manager.h"
#include "auxiliary/ksmessagebox.h"
#include "kstarsdata.h"
#include "Options.h"
#include "placeholderpath.h"
#include "ui_calibrationoptions.h"
#include "scriptsmanager.h"

#include <ekos_capture_debug.h>

#include <QFileDialog>
#include <QTableWidgetItem>
#include <auxiliary/ksnotification.h>
#include <KStandardGuiItem>

// These macros are used by functions in this file.
// Ideally, they should be in a common header or camera.h
#define QCDEBUG   qCDebug(KSTARS_EKOS_CAPTURE) << QString("[%1]").arg(getCameraName())
#define QCINFO    qCInfo(KSTARS_EKOS_CAPTURE) << QString("[%1]").arg(getCameraName())
#define QCWARNING qCWarning(KSTARS_EKOS_CAPTURE) << QString("[%1]").arg(getCameraName())

// JobTableColumnIndex is defined in camera.h in an anonymous namespace.
// It should be accessible here.

namespace Ekos
{

QSharedPointer<SequenceJob> Camera::createJob(SequenceJob::SequenceJobType jobtype,
        FilenamePreviewType filenamePreview)
{
    QSharedPointer<SequenceJob> job = QSharedPointer<SequenceJob>(new SequenceJob(devices(), state(), jobtype));

    updateJobFromUI(job, filenamePreview);

    // Nothing more to do if preview or for placeholder calculations
    if (jobtype == SequenceJob::JOBTYPE_PREVIEW || filenamePreview != FILENAME_NOT_PREVIEW)
        return job;

    // check if the upload paths are correct
    if (checkUploadPaths(filenamePreview, job) == false)
        return QSharedPointer<SequenceJob>();

    // all other jobs will be added to the job list
    state()->allJobs().append(job);

    // create a new row
    createNewJobTableRow(job);

    return job;
}

bool Camera::removeJob(int index)
{
    if (state()->getCaptureState() != CAPTURE_IDLE && state()->getCaptureState() != CAPTURE_ABORTED
            && state()->getCaptureState() != CAPTURE_COMPLETE)
        return false;

    if (m_JobUnderEdit)
    {
        resetJobEdit(true);
        return false;
    }

    if (index < 0 || index >= state()->allJobs().count())
        return false;

    queueTable->removeRow(index);
    QJsonArray seqArray = state()->getSequence();
    seqArray.removeAt(index);
    state()->setSequence(seqArray);
    emit sequenceChanged(seqArray);

    if (state()->allJobs().empty())
        return true;

    QSharedPointer<SequenceJob> job = state()->allJobs().at(index);
    // remove completed frame counts from frame count map
    state()->removeCapturedFrameCount(job->getSignature(), job->getCompleted());
    // remove the job
    state()->allJobs().removeOne(job);
    if (job == activeJob())
        state()->setActiveJob(nullptr);

    if (queueTable->rowCount() == 0)
        removeFromQueueB->setEnabled(false);

    if (queueTable->rowCount() == 1)
    {
        queueUpB->setEnabled(false);
        queueDownB->setEnabled(false);
    }

    if (index < queueTable->rowCount())
        queueTable->selectRow(index);
    else if (queueTable->rowCount() > 0)
        queueTable->selectRow(queueTable->rowCount() - 1);

    if (queueTable->rowCount() == 0)
    {
        queueSaveAsB->setEnabled(false);
        queueSaveB->setEnabled(false);
        resetB->setEnabled(false);
    }

    state()->setDirty(true);

    return true;
}

bool Camera::modifyJob(int index)
{
    if (m_JobUnderEdit)
    {
        resetJobEdit(true);
        return false;
    }

    if (index < 0 || index >= state()->allJobs().count())
        return false;

    queueTable->selectRow(index);
    auto modelIndex = queueTable->model()->index(index, 0);
    editJob(modelIndex);
    return true;
}

void Camera::addJob(const QSharedPointer<SequenceJob> &job)
{
    // create a new row
    createNewJobTableRow(job);
}

void Camera::editJobFinished()
{
    if (queueTable->currentRow() < 0)
        QCWARNING << "Editing finished, but no row selected!";

    int currentRow = queueTable->currentRow();
    const QSharedPointer<SequenceJob> job = state()->allJobs().at(currentRow);
    updateJobFromUI(job);

    // full update to the job table row
    updateJobTable(job, true);

    // Update the JSON object for the current row. Needs to be called after the new row has been filled
    QJsonObject jsonJob = createJsonJob(job, currentRow);
    state()->getSequence().replace(currentRow, jsonJob);
    emit sequenceChanged(state()->getSequence());

    resetJobEdit();
    appendLogText(i18n("Job #%1 changes applied.", currentRow + 1));
}

void Camera::resetJobs()
{
    // Stop any running capture
    stop();

    // If a job is selected for edit, reset only that job
    if (m_JobUnderEdit == true)
    {
        QSharedPointer<SequenceJob> job = state()->allJobs().at(queueTable->currentRow());
        if (!job.isNull())
        {
            job->resetStatus();
            updateJobTable(job);
        }
    }
    else
    {
        if (KMessageBox::warningContinueCancel(
                    nullptr, i18n("Are you sure you want to reset status of all jobs?"), i18n("Reset job status"),
                    KStandardGuiItem::cont(), KStandardGuiItem::cancel(), "reset_job_status_warning") != KMessageBox::Continue)
        {
            return;
        }

        foreach (auto job, state()->allJobs())
        {
            job->resetStatus();
            updateJobTable(job);
        }
    }

    // Also reset the storage count for all jobs
    state()->clearCapturedFramesMap();

    // We're not controlled by the Scheduler, restore progress option
    state()->setIgnoreJobProgress(Options::alwaysResetSequenceWhenStarting());

    // enable start button
    startB->setEnabled(true);
}

bool Camera::selectJob(QModelIndex i)
{
    if (i.row() < 0 || (i.row() + 1) > state()->allJobs().size())
        return false;

    QSharedPointer<SequenceJob> job = state()->allJobs().at(i.row());

    if (job == nullptr || job->jobType() == SequenceJob::JOBTYPE_DARKFLAT)
        return false;

    syncGUIToJob(job);

    if (state()->isBusy())
        return false;

    if (state()->allJobs().size() >= 2)
    {
        queueUpB->setEnabled(i.row() > 0);
        queueDownB->setEnabled(i.row() + 1 < state()->allJobs().size());
    }

    return true;
}

void Camera::editJob(QModelIndex i)
{
    // Try to select a job. If job not found or not editable return.
    if (selectJob(i) == false)
        return;

    appendLogText(i18n("Editing job #%1...", i.row() + 1));

    addToQueueB->setIcon(QIcon::fromTheme("dialog-ok-apply"));
    addToQueueB->setToolTip(i18n("Apply job changes."));
    removeFromQueueB->setToolTip(i18n("Cancel job changes."));

    // Make it sure if user presses enter, the job is validated.
    previewB->setDefault(false);
    addToQueueB->setDefault(true);

    m_JobUnderEdit = true;

}

void Camera::resetJobEdit(bool cancelled)
{
    if (cancelled == true)
        appendLogText(i18n("Editing job canceled."));

    m_JobUnderEdit = false;
    addToQueueB->setIcon(QIcon::fromTheme("list-add"));

    addToQueueB->setToolTip(i18n("Add job to sequence queue"));
    removeFromQueueB->setToolTip(i18n("Remove job from sequence queue"));

    addToQueueB->setDefault(false);
    previewB->setDefault(true);
}

void Camera::moveJob(bool up)
{
    int currentRow = queueTable->currentRow();
    int destinationRow = up ? currentRow - 1 : currentRow + 1;

    int columnCount = queueTable->columnCount();

    if (currentRow < 0 || destinationRow < 0 || destinationRow >= queueTable->rowCount())
        return;

    for (int i = 0; i < columnCount; i++)
    {
        QTableWidgetItem * selectedLine = queueTable->takeItem(currentRow, i);
        QTableWidgetItem * counterpart  = queueTable->takeItem(destinationRow, i);

        queueTable->setItem(destinationRow, i, selectedLine);
        queueTable->setItem(currentRow, i, counterpart);
    }

    QSharedPointer<SequenceJob> job = state()->allJobs().takeAt(currentRow);

    state()->allJobs().removeOne(job);
    state()->allJobs().insert(destinationRow, job);

    QJsonArray seqArray = state()->getSequence();
    QJsonObject currentJob = seqArray[currentRow].toObject();
    seqArray.replace(currentRow, seqArray[destinationRow]);
    seqArray.replace(destinationRow, currentJob);
    emit sequenceChanged(seqArray);

    queueTable->selectRow(destinationRow);

    state()->setDirty(true);
}

void Camera::removeJobFromQueue()
{
    int currentRow = queueTable->currentRow();

    if (currentRow < 0)
        currentRow = queueTable->rowCount() - 1;

    removeJob(currentRow);

    // update selection
    if (queueTable->rowCount() == 0)
        return;

    if (currentRow > queueTable->rowCount()) // This condition seems off, should be >=
        queueTable->selectRow(queueTable->rowCount() - 1);
    else if (currentRow < queueTable->rowCount()) // Ensure currentRow is valid
        queueTable->selectRow(currentRow);
    else if (queueTable->rowCount() > 0) // Fallback if currentRow became invalid
        queueTable->selectRow(queueTable->rowCount() - 1);
}

void Camera::loadSequenceQueue()
{
    QUrl fileURL = QFileDialog::getOpenFileUrl(Manager::Instance(), i18nc("@title:window", "Open Ekos Sequence Queue"),
                   m_dirPath,
                   "Ekos Sequence Queue (*.esq)");
    if (fileURL.isEmpty())
        return;

    if (fileURL.isValid() == false)
    {
        QString message = i18n("Invalid URL: %1", fileURL.toLocalFile());
        KSNotification::sorry(message, i18n("Invalid URL"));
        return;
    }

    m_dirPath = QUrl(fileURL.url(QUrl::RemoveFilename));

    loadSequenceQueue(fileURL.toLocalFile());
}

bool Camera::loadSequenceQueue(const QString &fileURL, QString targetName)
{
    QFile sFile(fileURL);
    if (!sFile.open(QIODevice::ReadOnly))
    {
        QString message = i18n("Unable to open file %1", fileURL);
        KSNotification::sorry(message, i18n("Could Not Open File"));
        return false;
    }

    state()->clearCapturedFramesMap();
    clearSequenceQueue();

    // !m_standAlone so the stand-alone editor doesn't influence a live capture sesion.
    const bool result = process()->loadSequenceQueue(fileURL, targetName, !m_standAlone);
    // cancel if loading fails
    if (result == false)
        return result;

    // update general settings
    setObserverName(state()->observerName());

    // select the first one of the loaded jobs
    if (state()->allJobs().size() > 0)
        syncGUIToJob(state()->allJobs().first());

    // update save button tool tip
    queueSaveB->setToolTip("Save to " + sFile.fileName());

    return true;
}

void Camera::saveSequenceQueue()
{
    QUrl backupCurrent = state()->sequenceURL();

    if (state()->sequenceURL().toLocalFile().startsWith(QLatin1String("/tmp/"))
            || state()->sequenceURL().toLocalFile().contains("/Temp"))
        state()->setSequenceURL(QUrl(""));

    // If no changes made, return.
    if (state()->dirty() == false && !state()->sequenceURL().isEmpty())
        return;

    if (state()->sequenceURL().isEmpty())
    {
        state()->setSequenceURL(QFileDialog::getSaveFileUrl(Manager::Instance(), i18nc("@title:window",
                                "Save Ekos Sequence Queue"),
                                m_dirPath,
                                "Ekos Sequence Queue (*.esq)"));
        // if user presses cancel
        if (state()->sequenceURL().isEmpty())
        {
            state()->setSequenceURL(backupCurrent);
            return;
        }

        m_dirPath = QUrl(state()->sequenceURL().url(QUrl::RemoveFilename));

        if (state()->sequenceURL().toLocalFile().endsWith(QLatin1String(".esq")) == false)
            state()->setSequenceURL(QUrl("file:" + state()->sequenceURL().toLocalFile() + ".esq"));

    }

    if (state()->sequenceURL().isValid())
    {
        // !m_standAlone so the stand-alone editor doesn't influence a live capture sesion.
        if ((process()->saveSequenceQueue(state()->sequenceURL().toLocalFile(), !m_standAlone)) == false)
        {
            KSNotification::error(i18n("Failed to save sequence queue"), i18n("Save"));
            return;
        }

        state()->setDirty(false);
    }
    else
    {
        QString message = i18n("Invalid URL: %1", state()->sequenceURL().url());
        KSNotification::sorry(message, i18n("Invalid URL"));
    }

}

bool Camera::saveSequenceQueue(const QString &path)
{
    // forward it to the process engine
    return process()->saveSequenceQueue(path);
}

void Camera::saveSequenceQueueAs()
{
    state()->setSequenceURL(QUrl(""));
    saveSequenceQueue();
}

void Camera::clearSequenceQueue()
{
    state()->setActiveJob(nullptr);
    while (queueTable->rowCount() > 0)
        queueTable->removeRow(0);
    state()->allJobs().clear();

    while (state()->getSequence().count())
        state()->getSequence().pop_back();
    emit sequenceChanged(state()->getSequence());
}

void Camera::updateJobTable(const QSharedPointer<SequenceJob> &job, bool full)
{
    if (job.isNull())
    {
        QListIterator<QSharedPointer<SequenceJob>> iter(state()->allJobs());
        while (iter.hasNext())
            updateJobTable(iter.next(), full);
    }
    else
    {
        // find the job's row
        int row = state()->allJobs().indexOf(job);
        if (row >= 0 && row < queueTable->rowCount())
        {
            updateRowStyle(job);
            QTableWidgetItem *status = queueTable->item(row, JOBTABLE_COL_STATUS);
            QTableWidgetItem *count  = queueTable->item(row, JOBTABLE_COL_COUNTS);
            status->setText(job->getStatusString());
            updateJobTableCountCell(job, count);

            if (full)
            {
                bool isDarkFlat = job->jobType() == SequenceJob::JOBTYPE_DARKFLAT;

                QTableWidgetItem *filter = queueTable->item(row, JOBTABLE_COL_FILTER);
                if (FilterPosCombo->findText(job->getCoreProperty(SequenceJob::SJ_Filter).toString()) >= 0 &&
                        (captureTypeS->currentIndex() == FRAME_LIGHT || captureTypeS->currentIndex() == FRAME_FLAT
                         || captureTypeS->currentIndex() == FRAME_VIDEO || isDarkFlat) )
                    filter->setText(job->getCoreProperty(SequenceJob::SJ_Filter).toString());
                else
                    filter->setText("--");

                QTableWidgetItem *exp = queueTable->item(row, JOBTABLE_COL_EXP);
                exp->setText(QString("%L1").arg(job->getCoreProperty(SequenceJob::SJ_Exposure).toDouble(), 0, 'f',
                                                captureExposureN->decimals()));

                QTableWidgetItem *type = queueTable->item(row, JOBTABLE_COL_TYPE);
                type->setText(isDarkFlat ? i18n("Dark Flat") : CCDFrameTypeNames[job->getFrameType()]);

                QTableWidgetItem *bin = queueTable->item(row, JOBTABLE_COL_BINNING);
                QPoint binning = job->getCoreProperty(SequenceJob::SJ_Binning).toPoint();
                bin->setText(QString("%1x%2").arg(binning.x()).arg(binning.y()));

                QTableWidgetItem *iso = queueTable->item(row, JOBTABLE_COL_ISO);
                if (job->getCoreProperty(SequenceJob::SJ_ISOIndex).toInt() != -1)
                    iso->setText(captureISOS->itemText(job->getCoreProperty(SequenceJob::SJ_ISOIndex).toInt()));
                else if (job->getCoreProperty(SequenceJob::SJ_Gain).toDouble() >= 0)
                    iso->setText(QString::number(job->getCoreProperty(SequenceJob::SJ_Gain).toDouble(), 'f', 1));
                else
                    iso->setText("--");

                QTableWidgetItem *offset = queueTable->item(row, JOBTABLE_COL_OFFSET);
                if (job->getCoreProperty(SequenceJob::SJ_Offset).toDouble() >= 0)
                    offset->setText(QString::number(job->getCoreProperty(SequenceJob::SJ_Offset).toDouble(), 'f', 1));
                else
                    offset->setText("--");

            }
            // ensure that all contents are shown
            queueTable->resizeColumnsToContents();


            // update button enablement
            if (queueTable->rowCount() > 0)
            {
                queueSaveAsB->setEnabled(true);
                queueSaveB->setEnabled(true);
                resetB->setEnabled(true);
                state()->setDirty(true);
            }

            if (queueTable->rowCount() > 1)
            {
                queueUpB->setEnabled(true);
                queueDownB->setEnabled(true);
            }
        }
    }
}

void Camera::updateJobFromUI(const QSharedPointer<SequenceJob> &job, FilenamePreviewType filenamePreview)
{
    job->setCoreProperty(SequenceJob::SJ_Format, captureFormatS->currentText());
    job->setCoreProperty(SequenceJob::SJ_Encoding, captureEncodingS->currentText());

    if (captureISOS)
        job->setISO(captureISOS->currentIndex());

    job->setCoreProperty(SequenceJob::SJ_Gain, getGain());
    job->setCoreProperty(SequenceJob::SJ_Offset, getOffset());

    if (cameraTemperatureN->isEnabled())
    {
        job->setCoreProperty(SequenceJob::SJ_EnforceTemperature, cameraTemperatureS->isChecked());
        job->setTargetTemperature(cameraTemperatureN->value());
    }

    job->setScripts(m_scriptsManager->getScripts());
    job->setUploadMode(static_cast<ISD::Camera::UploadMode>(fileUploadModeS->currentIndex()));


    job->setFlatFieldDuration(m_CalibrationUI->captureCalibrationDurationManual->isChecked() ? DURATION_MANUAL :
                              DURATION_ADU);

    int action = CAPTURE_PREACTION_NONE;
    if (m_CalibrationUI->captureCalibrationParkMount->isChecked())
        action |= CAPTURE_PREACTION_PARK_MOUNT;
    if (m_CalibrationUI->captureCalibrationParkDome->isChecked())
        action |= CAPTURE_PREACTION_PARK_DOME;
    if (m_CalibrationUI->captureCalibrationWall->isChecked())
    {
        bool azOk = false, altOk = false;
        auto wallAz  = m_CalibrationUI->azBox->createDms(&azOk);
        auto wallAlt = m_CalibrationUI->altBox->createDms(&altOk);

        if (azOk && altOk)
        {
            action = (action & ~CAPTURE_PREACTION_PARK_MOUNT) | CAPTURE_PREACTION_WALL;
            SkyPoint wallSkyPoint;
            wallSkyPoint.setAz(wallAz);
            wallSkyPoint.setAlt(wallAlt);
            job->setWallCoord(wallSkyPoint);
        }
    }

    if (m_CalibrationUI->captureCalibrationUseADU->isChecked())
    {
        job->setCoreProperty(SequenceJob::SJ_TargetADU, m_CalibrationUI->captureCalibrationADUValue->value());
        job->setCoreProperty(SequenceJob::SJ_TargetADUTolerance,
                             m_CalibrationUI->captureCalibrationADUTolerance->value());
        job->setCoreProperty(SequenceJob::SJ_SkyFlat, m_CalibrationUI->captureCalibrationSkyFlats->isChecked());
    }

    job->setCalibrationPreAction(action);

    job->setFrameType(static_cast<CCDFrameType>(qMax(0, captureTypeS->currentIndex())));

    if (FilterPosCombo->currentIndex() != -1 && (m_standAlone || devices()->filterWheel() != nullptr))
        job->setTargetFilter(FilterPosCombo->currentIndex() + 1, FilterPosCombo->currentText());

    job->setCoreProperty(SequenceJob::SJ_Exposure, captureExposureN->value());

    if (captureTypeS->currentText() == CAPTURE_TYPE_VIDEO)
    {
        if (videoDurationUnitCB->currentIndex() == 0)
            // duration in seconds
            job->setCoreProperty(SequenceJob::SJ_Count, static_cast<int>(videoDurationSB->value() / captureExposureN->value()));
        else
            // duration in number of frames
            job->setCoreProperty(SequenceJob::SJ_Count, static_cast<int>(videoDurationSB->value()));
    }
    else
    {
        job->setCoreProperty(SequenceJob::SJ_Count, captureCountN->value());
    }
    job->setCoreProperty(SequenceJob::SJ_Binning, QPoint(captureBinHN->value(), captureBinVN->value()));

    /* in ms */
    job->setCoreProperty(SequenceJob::SJ_Delay, captureDelayN->value() * 1000);

    // Custom Properties
    job->setCustomProperties(customPropertiesDialog()->getCustomProperties());

    job->setCoreProperty(SequenceJob::SJ_ROI, QRect(captureFrameXN->value(), captureFrameYN->value(),
                         captureFrameWN->value(),
                         captureFrameHN->value()));
    job->setCoreProperty(SequenceJob::SJ_RemoteDirectory, fileRemoteDirT->text());
    job->setCoreProperty(SequenceJob::SJ_LocalDirectory, fileDirectoryT->text());
    job->setCoreProperty(SequenceJob::SJ_TargetName, targetNameT->text());
    job->setCoreProperty(SequenceJob::SJ_PlaceholderFormat, placeholderFormatT->text());
    job->setCoreProperty(SequenceJob::SJ_PlaceholderSuffix, formatSuffixN->value());

    job->setCoreProperty(SequenceJob::SJ_DitherPerJobEnabled, m_LimitsUI->enableDitherPerJob->isChecked());
    job->setCoreProperty(SequenceJob::SJ_DitherPerJobFrequency, m_LimitsUI->guideDitherPerJobFrequency->value());

    auto placeholderPath = PlaceholderPath();
    placeholderPath.updateFullPrefix(job, placeholderFormatT->text());

    QString signature = placeholderPath.generateSequenceFilename(*job,
                        filenamePreview != FILENAME_REMOTE_PREVIEW, true, 1,
                        ".fits", "", false, true);
    job->setCoreProperty(SequenceJob::SJ_Signature, signature);

}

void Camera::syncGUIToJob(const QSharedPointer<SequenceJob> &job)
{
    if (job == nullptr)
    {
        qWarning(KSTARS_EKOS_CAPTURE) << "syncGuiToJob with null job.";
        // Everything below depends on job. Just return.
        return;
    }

    auto roi = job->getCoreProperty(SequenceJob::SJ_ROI).toRect();
    // If ROI is not valid, use maximum resolution
    if (roi.isNull() || !roi.isValid())
    {
        roi = QRect(captureFrameXN->minimum(), captureFrameYN->minimum(), captureFrameWN->maximum(), captureFrameHN->maximum());
    }

    captureTypeS->setCurrentIndex(job->getFrameType());
    captureFormatS->setCurrentText(job->getCoreProperty(SequenceJob::SJ_Format).toString());
    captureEncodingS->setCurrentText(job->getCoreProperty(SequenceJob::SJ_Encoding).toString());
    captureExposureN->setValue(job->getCoreProperty(SequenceJob::SJ_Exposure).toDouble());
    captureBinHN->setValue(job->getCoreProperty(SequenceJob::SJ_Binning).toPoint().x());
    captureBinVN->setValue(job->getCoreProperty(SequenceJob::SJ_Binning).toPoint().y());
    captureFrameXN->setValue(roi.x());
    captureFrameYN->setValue(roi.y());
    captureFrameWN->setValue(roi.width());
    captureFrameHN->setValue(roi.height());
    FilterPosCombo->setCurrentIndex(job->getTargetFilter() - 1);
    captureCountN->setValue(job->getCoreProperty(SequenceJob::SJ_Count).toInt());
    captureDelayN->setValue(job->getCoreProperty(SequenceJob::SJ_Delay).toInt() / 1000);
    targetNameT->setText(job->getCoreProperty(SequenceJob::SJ_TargetName).toString());
    fileDirectoryT->setText(job->getCoreProperty(SequenceJob::SJ_LocalDirectory).toString());
    selectUploadMode(job->getUploadMode());
    fileRemoteDirT->setText(job->getCoreProperty(SequenceJob::SJ_RemoteDirectory).toString());
    placeholderFormatT->setText(job->getCoreProperty(SequenceJob::SJ_PlaceholderFormat).toString());
    formatSuffixN->setValue(job->getCoreProperty(SequenceJob::SJ_PlaceholderSuffix).toUInt());

    // Temperature Options
    cameraTemperatureS->setChecked(job->getCoreProperty(SequenceJob::SJ_EnforceTemperature).toBool());
    if (job->getCoreProperty(SequenceJob::SJ_EnforceTemperature).toBool())
        cameraTemperatureN->setValue(job->getTargetTemperature());

    // Start guider drift options
    m_LimitsUI->enableDitherPerJob->setChecked(job->getCoreProperty(SequenceJob::SJ_DitherPerJobEnabled).toBool());
    if (m_LimitsUI->enableDitherPerJob->isChecked())
        m_LimitsUI->guideDitherPerJobFrequency->setValue(job->getCoreProperty(SequenceJob::SJ_DitherPerJobFrequency).toInt());

    syncLimitSettings();

    // Flat field options
    calibrationB->setEnabled(job->getFrameType() != FRAME_LIGHT);
    generateDarkFlatsB->setEnabled(job->getFrameType() != FRAME_LIGHT);

    if (job->getFlatFieldDuration() == DURATION_MANUAL)
        m_CalibrationUI->captureCalibrationDurationManual->setChecked(true);
    else
        m_CalibrationUI->captureCalibrationUseADU->setChecked(true);

    // Calibration Pre-Action
    const auto action = job->getCalibrationPreAction();
    if (action & CAPTURE_PREACTION_WALL)
    {
        m_CalibrationUI->azBox->setText(job->getWallCoord().az().toDMSString());
        m_CalibrationUI->altBox->setText(job->getWallCoord().alt().toDMSString());
    }
    m_CalibrationUI->captureCalibrationWall->setChecked(action & CAPTURE_PREACTION_WALL);
    m_CalibrationUI->captureCalibrationParkMount->setChecked(action & CAPTURE_PREACTION_PARK_MOUNT);
    m_CalibrationUI->captureCalibrationParkDome->setChecked(action & CAPTURE_PREACTION_PARK_DOME);

    // Calibration Flat Duration
    switch (job->getFlatFieldDuration())
    {
        case DURATION_MANUAL:
            m_CalibrationUI->captureCalibrationDurationManual->setChecked(true);
            break;

        case DURATION_ADU:
            m_CalibrationUI->captureCalibrationUseADU->setChecked(true);
            m_CalibrationUI->captureCalibrationADUValue->setValue(job->getCoreProperty(SequenceJob::SJ_TargetADU).toUInt());
            m_CalibrationUI->captureCalibrationADUTolerance->setValue(job->getCoreProperty(
                        SequenceJob::SJ_TargetADUTolerance).toUInt());
            m_CalibrationUI->captureCalibrationSkyFlats->setChecked(job->getCoreProperty(SequenceJob::SJ_SkyFlat).toBool());
            break;
    }

    m_scriptsManager->setScripts(job->getScripts());

    // Custom Properties
    customPropertiesDialog()->setCustomProperties(job->getCustomProperties());

    if (captureISOS)
        captureISOS->setCurrentIndex(job->getCoreProperty(SequenceJob::SJ_ISOIndex).toInt());

    double gain = getGain();
    if (gain >= 0)
        captureGainN->setValue(gain);
    else
        captureGainN->setValue(GainSpinSpecialValue);

    double offset = getOffset();
    if (offset >= 0)
        captureOffsetN->setValue(offset);
    else
        captureOffsetN->setValue(OffsetSpinSpecialValue);

    // update place holder typ
    generatePreviewFilename();

    if (m_RotatorControlPanel) // only if rotator is registered
    {
        if (job->getTargetRotation() != Ekos::INVALID_VALUE)
        {
            // remove enforceJobPA m_RotatorControlPanel->setRotationEnforced(true);
            m_RotatorControlPanel->setCameraPA(job->getTargetRotation());
        }
        // remove enforceJobPA
        // else
        //    m_RotatorControlPanel->setRotationEnforced(false);
    }

    // hide target drift if align check frequency is == 0
    if (Options::alignCheckFrequency() == 0)
    {
        targetDriftLabel->setVisible(false);
        targetDrift->setVisible(false);
        targetDriftUnit->setVisible(false);
    }
}

void Camera::createNewJobTableRow(const QSharedPointer<SequenceJob> &job)
{
    int currentRow = queueTable->rowCount();
    queueTable->insertRow(currentRow);

    // create job table widgets
    QTableWidgetItem *status = new QTableWidgetItem();
    status->setTextAlignment(Qt::AlignHCenter);
    status->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

    QTableWidgetItem *filter = new QTableWidgetItem();
    filter->setTextAlignment(Qt::AlignHCenter);
    filter->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

    QTableWidgetItem *count = new QTableWidgetItem();
    count->setTextAlignment(Qt::AlignHCenter);
    count->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

    QTableWidgetItem *exp = new QTableWidgetItem();
    exp->setTextAlignment(Qt::AlignHCenter);
    exp->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

    QTableWidgetItem *type = new QTableWidgetItem();
    type->setTextAlignment(Qt::AlignHCenter);
    type->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

    QTableWidgetItem *bin = new QTableWidgetItem();
    bin->setTextAlignment(Qt::AlignHCenter);
    bin->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

    QTableWidgetItem *iso = new QTableWidgetItem();
    iso->setTextAlignment(Qt::AlignHCenter);
    iso->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

    QTableWidgetItem *offset = new QTableWidgetItem();
    offset->setTextAlignment(Qt::AlignHCenter);
    offset->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

    // add the widgets to the table
    queueTable->setItem(currentRow, JOBTABLE_COL_STATUS, status);
    queueTable->setItem(currentRow, JOBTABLE_COL_FILTER, filter);
    queueTable->setItem(currentRow, JOBTABLE_COL_COUNTS, count);
    queueTable->setItem(currentRow, JOBTABLE_COL_EXP, exp);
    queueTable->setItem(currentRow, JOBTABLE_COL_TYPE, type);
    queueTable->setItem(currentRow, JOBTABLE_COL_BINNING, bin);
    queueTable->setItem(currentRow, JOBTABLE_COL_ISO, iso);
    queueTable->setItem(currentRow, JOBTABLE_COL_OFFSET, offset);

    // full update to the job table row
    updateJobTable(job, true);

    // Create a new JSON object. Needs to be called after the new row has been filled
    QJsonObject jsonJob = createJsonJob(job, currentRow);
    state()->getSequence().append(jsonJob);
    emit sequenceChanged(state()->getSequence());

    removeFromQueueB->setEnabled(true);
}

void Camera::updateRowStyle(const QSharedPointer<SequenceJob> &job)
{
    if (job.isNull())
        return;

    // find the job's row
    int row = state()->allJobs().indexOf(job);
    if (row >= 0 && row < queueTable->rowCount())
    {
        updateCellStyle(queueTable->item(row, JOBTABLE_COL_STATUS), job->getStatus() == JOB_BUSY);
        updateCellStyle(queueTable->item(row, JOBTABLE_COL_FILTER), job->getStatus() == JOB_BUSY);
        updateCellStyle(queueTable->item(row, JOBTABLE_COL_COUNTS), job->getStatus() == JOB_BUSY);
        updateCellStyle(queueTable->item(row, JOBTABLE_COL_EXP), job->getStatus() == JOB_BUSY);
        updateCellStyle(queueTable->item(row, JOBTABLE_COL_TYPE), job->getStatus() == JOB_BUSY);
        updateCellStyle(queueTable->item(row, JOBTABLE_COL_BINNING), job->getStatus() == JOB_BUSY);
        updateCellStyle(queueTable->item(row, JOBTABLE_COL_ISO), job->getStatus() == JOB_BUSY);
        updateCellStyle(queueTable->item(row, JOBTABLE_COL_OFFSET), job->getStatus() == JOB_BUSY);
    }
}

void Camera::updateCellStyle(QTableWidgetItem * cell, bool active)
{
    if (cell == nullptr)
        return;

    QFont font(cell->font());
    font.setBold(active);
    font.setItalic(active);
    cell->setFont(font);
}

void Camera::selectedJobChanged(QModelIndex current, QModelIndex previous)
{
    Q_UNUSED(previous)
    selectJob(current);
}

QJsonObject Camera::createJsonJob(const QSharedPointer<SequenceJob> &job, int currentRow)
{
    if (job.isNull())
        return QJsonObject();

    QJsonObject jsonJob = {{"Status", "Idle"}};
    bool isDarkFlat = job->jobType() == SequenceJob::JOBTYPE_DARKFLAT;
    jsonJob.insert("Filter", FilterPosCombo->itemText(job->getTargetFilter() - 1));
    jsonJob.insert("Count", queueTable->item(currentRow, JOBTABLE_COL_COUNTS)->text());
    jsonJob.insert("Exp", queueTable->item(currentRow, JOBTABLE_COL_EXP)->text());
    jsonJob.insert("Type", isDarkFlat ? i18n("Dark Flat") : queueTable->item(currentRow, JOBTABLE_COL_TYPE)->text());
    jsonJob.insert("Bin", queueTable->item(currentRow, JOBTABLE_COL_BINNING)->text());
    jsonJob.insert("ISO/Gain", queueTable->item(currentRow, JOBTABLE_COL_ISO)->text());
    jsonJob.insert("Offset", queueTable->item(currentRow, JOBTABLE_COL_OFFSET)->text());
    jsonJob.insert("Encoding", job->getCoreProperty(SequenceJob::SJ_Encoding).toJsonValue());
    jsonJob.insert("Format", job->getCoreProperty(SequenceJob::SJ_Format).toJsonValue());
    jsonJob.insert("Temperature", job->getTargetTemperature());
    jsonJob.insert("EnforceTemperature", job->getCoreProperty(SequenceJob::SJ_EnforceTemperature).toJsonValue());
    jsonJob.insert("DitherPerJobEnabled", job->getCoreProperty(SequenceJob::SJ_DitherPerJobEnabled).toJsonValue());
    jsonJob.insert("DitherPerJobFrequency", job->getCoreProperty(SequenceJob::SJ_DitherPerJobFrequency).toJsonValue());

    return jsonJob;
}

void Camera::updateJobTableCountCell(const QSharedPointer<SequenceJob> &job, QTableWidgetItem * countCell)
{
    countCell->setText(QString("%L1/%L2").arg(job->getCompleted()).arg(job->getCoreProperty(SequenceJob::SJ_Count).toInt()));
}

void Camera::generateDarkFlats()
{
    const auto existingJobs = state()->allJobs().size();
    uint8_t jobsAdded = 0;

    for (int i = 0; i < existingJobs; i++)
    {
        if (state()->allJobs().at(i)->getFrameType() != FRAME_FLAT)
            continue;

        syncGUIToJob(state()->allJobs().at(i));

        captureTypeS->setCurrentIndex(FRAME_DARK);
        createJob(SequenceJob::JOBTYPE_DARKFLAT);
        jobsAdded++;
    }

    if (jobsAdded > 0)
    {
        appendLogText(i18np("One dark flats job was created.", "%1 dark flats jobs were created.", jobsAdded));
    }
}

} // namespace Ekos
