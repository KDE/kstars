/*  Ekos
    Copyright (C) 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include <QFileDialog>
#include <QDirIterator>
#include <QStandardPaths>

#include <KMessageBox>
#include <KDirWatch>
#include <KLocalizedString>
#include <KNotifications/KNotification>

#include <basedevice.h>
#include <lilxml.h>

#include "Options.h"

#include "oal/log.h"

#include "kstars.h"
#include "skymap.h"
#include "kstarsdata.h"

#include "capture.h"
#include "sequencejob.h"
#include "dslrinfodialog.h"

#include "indi/driverinfo.h"
#include "indi/indifilter.h"
#include "indi/clientmanager.h"

#include "fitsviewer/fitsviewer.h"
#include "fitsviewer/fitsview.h"

#include "ekos/auxiliary/darklibrary.h"
#include "ekos/ekosmanager.h"
#include "captureadaptor.h"
#include "ui_calibrationoptions.h"

#include "auxiliary/QProgressIndicator.h"
#include "rotatorsettings.h"

#define INVALID_VALUE -1e6
#define MF_TIMER_TIMEOUT    90000
#define GD_TIMER_TIMEOUT    60000
#define MF_RA_DIFF_LIMIT    4
#define MAX_CAPTURE_RETRIES 3

// Current Sequence File Format:
#define SQ_FORMAT_VERSION 1.9
// We accept file formats with version back to:
#define SQ_COMPAT_VERSION 1.6

namespace Ekos
{
Capture::Capture()
{
    setupUi(this);

    new CaptureAdaptor(this);
    QDBusConnection::sessionBus().registerObject("/KStars/Ekos/Capture", this);

    dirPath = QUrl::fromLocalFile(QDir::homePath());

    state      = CAPTURE_IDLE;
    focusState = FOCUS_IDLE;
    guideState = GUIDE_IDLE;
    alignState = ALIGN_IDLE;

    currentCCD       = nullptr;
    currentTelescope = nullptr;
    currentFilter    = nullptr;
    dustCap          = nullptr;
    lightBox         = nullptr;
    dome             = nullptr;

    filterSlot = nullptr;
    filterName = nullptr;
    activeJob  = nullptr;

    targetChip = nullptr;
    guideChip  = nullptr;

    targetADU          = 0;
    targetADUTolerance = 1000;
    flatFieldDuration  = DURATION_MANUAL;
    flatFieldSource    = SOURCE_MANUAL;
    calibrationStage   = CAL_NONE;
    preMountPark       = false;
    preDomePark        = false;

    deviationDetected = false;
    spikeDetected     = false;
    isBusy            = false;

    ignoreJobProgress = true;

    dustCapLightEnabled = lightBoxLightEnabled = false;

    //isAutoGuiding   = false;
    isAutoFocus              = false;
    autoFocusStatus          = false;
    resumeAlignmentAfterFlip = false;

    isRefocus             = false;

    mDirty                = false;
    jobUnderEdit          = false;
    currentFilterPosition = -1;

    //calibrationState  = CALIBRATE_NONE;
    meridianFlipStage      = MF_NONE;
    resumeGuidingAfterFlip = false;

    //ADURaw1 = ADURaw2 = ExpRaw1 = ExpRaw2 = -1;
    //ADUSlope = 0;

    rotatorSettings = new RotatorSettings(this);

    pi = new QProgressIndicator(this);

    progressLayout->addWidget(pi, 0, 4, 1, 1);

    seqFileCount = 0;
    //seqWatcher		= new KDirWatch();
    seqTimer = new QTimer(this);
    connect(seqTimer, SIGNAL(timeout()), this, SLOT(captureImage()));

    connect(startB, SIGNAL(clicked()), this, SLOT(toggleSequence()));
    connect(pauseB, SIGNAL(clicked()), this, SLOT(pause()));

    startB->setIcon(QIcon::fromTheme("media-playback-start", QIcon(":/icons/breeze/default/media-playback-start.svg")));
    startB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    pauseB->setIcon(QIcon::fromTheme("media-playback-pause", QIcon(":/icons/breeze/default/media-playback-pause.svg")));
    pauseB->setAttribute(Qt::WA_LayoutUsesWidgetRect);

    filterOffsetB->setIcon(QIcon::fromTheme("view-filter", QIcon(":/icons/breeze/default/view-filter.svg")));
    connect(filterOffsetB, SIGNAL(clicked()), this, SLOT(showFilterOffsetDialog()));
    filterOffsetB->setAttribute(Qt::WA_LayoutUsesWidgetRect);

    connect(binXIN, SIGNAL(valueChanged(int)), binYIN, SLOT(setValue(int)));

    connect(CCDCaptureCombo, SIGNAL(activated(QString)), this, SLOT(setDefaultCCD(QString)));
    connect(CCDCaptureCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(checkCCD(int)));

    connect(liveVideoB, SIGNAL(clicked(bool)), this, SLOT(toggleVideoStream(bool)));

    guideDeviationTimer.setInterval(GD_TIMER_TIMEOUT);
    connect(&guideDeviationTimer, SIGNAL(timeout()), this, SLOT(checkGuideDeviationTimeout()));

    connect(FilterCaptureCombo, SIGNAL(activated(int)), this, SLOT(checkFilter(int)));

    connect(previewB, SIGNAL(clicked()), this, SLOT(captureOne()));

    //connect( seqWatcher, SIGNAL(dirty(QString)), this, SLOT(checkSeqFile(QString)));

    connect(addToQueueB, SIGNAL(clicked()), this, SLOT(addJob()));
    connect(removeFromQueueB, SIGNAL(clicked()), this, SLOT(removeJob()));
    connect(queueUpB, SIGNAL(clicked()), this, SLOT(moveJobUp()));
    connect(queueDownB, SIGNAL(clicked()), this, SLOT(moveJobDown()));
    connect(selectFITSDirB, SIGNAL(clicked()), this, SLOT(saveFITSDirectory()));
    connect(queueSaveB, SIGNAL(clicked()), this, SLOT(saveSequenceQueue()));
    connect(queueSaveAsB, SIGNAL(clicked()), this, SLOT(saveSequenceQueueAs()));
    connect(queueLoadB, SIGNAL(clicked()), this, SLOT(loadSequenceQueue()));
    connect(resetB, SIGNAL(clicked()), this, SLOT(resetJobs()));
    connect(queueTable, SIGNAL(doubleClicked(QModelIndex)), this, SLOT(editJob(QModelIndex)));
    connect(queueTable, SIGNAL(itemSelectionChanged()), this, SLOT(resetJobEdit()));
    connect(setTemperatureB, SIGNAL(clicked()), this, SLOT(setTemperature()));
    connect(temperatureIN, SIGNAL(editingFinished()), setTemperatureB, SLOT(setFocus()));
    connect(frameTypeCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(checkFrameType(int)));
    connect(resetFrameB, SIGNAL(clicked()), this, SLOT(resetFrame()));
    connect(calibrationB, SIGNAL(clicked()), this, SLOT(openCalibrationDialog()));
    connect(rotatorB, SIGNAL(clicked()), rotatorSettings, SLOT(show()));

    addToQueueB->setIcon(QIcon::fromTheme("list-add", QIcon(":/icons/breeze/default/list-add.svg")));
    addToQueueB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    removeFromQueueB->setIcon(QIcon::fromTheme("list-remove", QIcon(":/icons/breeze/default/list-remove.svg")));
    removeFromQueueB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    queueUpB->setIcon(QIcon::fromTheme("go-up", QIcon(":/icons/breeze/default/go-up.svg")));
    queueUpB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    queueDownB->setIcon(QIcon::fromTheme("go-down", QIcon(":/icons/breeze/default/go-down.svg")));
    queueDownB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    selectFITSDirB->setIcon(
        QIcon::fromTheme("document-open-folder", QIcon(":/icons/breeze/default/document-open-folder.svg")));
    selectFITSDirB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    queueLoadB->setIcon(QIcon::fromTheme("document-open", QIcon(":/icons/breeze/default/document-open.svg")));
    queueLoadB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    queueSaveB->setIcon(QIcon::fromTheme("document-save", QIcon(":/icons/breeze/default/document-save.svg")));
    queueSaveB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    queueSaveAsB->setIcon(QIcon::fromTheme("document-save-as", QIcon(":/icons/breeze/default/document-save-as.svg")));
    queueSaveAsB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    resetB->setIcon(QIcon::fromTheme("system-reboot", QIcon(":/icons/breeze/default/system-reboot.svg")));
    resetB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    resetFrameB->setIcon(QIcon::fromTheme("view-refresh", QIcon(":/icons/breeze/default/view-refresh.svg")));
    resetFrameB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    calibrationB->setIcon(QIcon::fromTheme("run-build", QIcon(":/icons/breeze/default/run-build.svg")));
    calibrationB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    rotatorB->setIcon(QIcon::fromTheme("kstars_solarsystem", QIcon(":/icons/breeze/default/kstars_solarsystem.svg")));
    rotatorB->setAttribute(Qt::WA_LayoutUsesWidgetRect);

    addToQueueB->setToolTip(i18n("Add job to sequence queue"));
    removeFromQueueB->setToolTip(i18n("Remove job from sequence queue"));

    fitsDir->setText(Options::fitsDir());

    seqExpose       = 0;
    seqTotalCount   = 0;
    seqCurrentCount = 0;
    seqDelay        = 0;
    fileHFR         = 0;
    useGuideHead    = false;
    firstAutoFocus  = true;

    foreach (QString filter, FITSViewer::filterTypes)
        filterCombo->addItem(filter);

    guideDeviationCheck->setChecked(Options::enforceGuideDeviation());
    guideDeviation->setValue(Options::guideDeviation());
    autofocusCheck->setChecked(Options::enforceAutofocus());
    refocusEveryNCheck->setChecked(Options::enforceRefocusEveryN());
    meridianCheck->setChecked(Options::autoMeridianFlip());
    meridianHours->setValue(Options::autoMeridianHours());
    useFITSViewerInCapture->setChecked(Options::useFITSViewerInCapture());

    connect(autofocusCheck, SIGNAL(toggled(bool)), this, SLOT(setDirty()));
    connect(refocusEveryNCheck, SIGNAL(toggled(bool)), this, SLOT(setDirty()));
    connect(HFRPixels, SIGNAL(valueChanged(double)), this, SLOT(setDirty()));
    connect(guideDeviationCheck, SIGNAL(toggled(bool)), this, SLOT(setDirty()));
    connect(guideDeviation, SIGNAL(valueChanged(double)), this, SLOT(setDirty()));
    connect(meridianCheck, SIGNAL(toggled(bool)), this, SLOT(setDirty()));
    connect(meridianHours, SIGNAL(valueChanged(double)), this, SLOT(setDirty()));
    connect(uploadModeCombo, SIGNAL(activated(int)), this, SLOT(setDirty()));
    connect(remoteDirIN, SIGNAL(editingFinished()), this, SLOT(setDirty()));

    observerName = Options::defaultObserver();
    observerB->setIcon(QIcon::fromTheme("im-user", QIcon(":/icons/breeze/default/im-user.svg")));
    observerB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    connect(observerB, SIGNAL(clicked()), this, SLOT(showObserverDialog()));

    // Post capture script
    connect(&postCaptureScript, SIGNAL(finished(int)), this, SLOT(postScriptFinished(int)));

    // Remote directory
    connect(uploadModeCombo, static_cast<void (QComboBox::*)(int)>(&QComboBox::activated), this,
            [&](int index) { remoteDirIN->setEnabled(index != 0); });

    // Load FIlter Offets
    loadFilterOffsets();
}

Capture::~Capture()
{
    qDeleteAll(jobs);
    delete (rotatorSettings);
}

void Capture::setDefaultCCD(QString ccd)
{
    Options::setDefaultCaptureCCD(ccd);
}

void Capture::addCCD(ISD::GDInterface *newCCD)
{
    ISD::CCD *ccd = static_cast<ISD::CCD *>(newCCD);

    if (CCDs.contains(ccd))
        return;

    CCDs.append(ccd);

    CCDCaptureCombo->addItem(ccd->getDeviceName());

    if (Filters.count() > 0)
        syncFilterInfo();
}

void Capture::addGuideHead(ISD::GDInterface *newCCD)
{
    QString guiderName = newCCD->getDeviceName() + QString(" Guider");

    if (CCDCaptureCombo->findText(guiderName) == -1)
    {
        CCDCaptureCombo->addItem(guiderName);
        CCDs.append(static_cast<ISD::CCD *>(newCCD));
    }
}

void Capture::addFilter(ISD::GDInterface *newFilter)
{
    foreach (ISD::GDInterface *filter, Filters)
    {
        if (!strcmp(filter->getDeviceName(), newFilter->getDeviceName()))
            return;
    }

    FilterCaptureCombo->addItem(newFilter->getDeviceName());

    Filters.append(static_cast<ISD::Filter *>(newFilter));

    filterOffsetB->setEnabled(true);

    checkFilter(0);

    FilterCaptureCombo->setCurrentIndex(0);
}

void Capture::pause()
{
    pauseFunction = nullptr;
    state         = CAPTURE_PAUSED;
    emit newStatus(Ekos::CAPTURE_PAUSED);
    appendLogText(i18n("Sequence shall be paused after current exposure is complete."));
    pauseB->setEnabled(false);

    startB->setIcon(QIcon::fromTheme("media-playback-start", QIcon(":/icons/breeze/default/media-playback-start.svg")));
    startB->setToolTip(i18n("Resume Sequence"));
}

void Capture::toggleSequence()
{
    if (state == CAPTURE_PAUSED)
    {
        startB->setIcon(
            QIcon::fromTheme("media-playback-stop", QIcon(":/icons/breeze/default/media-playback-stop.svg")));
        startB->setToolTip(i18n("Stop Sequence"));
        pauseB->setEnabled(true);

        state = CAPTURE_CAPTURING;
        emit newStatus(Ekos::CAPTURE_CAPTURING);

        appendLogText(i18n("Sequence resumed."));

        // Call from where ever we have left of when we paused
        if (pauseFunction)
            (this->*pauseFunction)();
    }
    else if (state == CAPTURE_IDLE || state == CAPTURE_COMPLETE)
    {
        start();
    }
    else
    {
        abort();
    }
}

void Capture::start()
{
    if (darkSubCheck->isChecked())
    {
        KMessageBox::error(this, i18n("Auto dark subtract is not supported in batch mode."));
        return;
    }

    Options::setGuideDeviation(guideDeviation->value());
    Options::setEnforceGuideDeviation(guideDeviationCheck->isChecked());
    Options::setEnforceAutofocus(autofocusCheck->isChecked());
    Options::setEnforceRefocusEveryN(refocusEveryNCheck->isChecked());

    Options::setAutoMeridianFlip(meridianCheck->isChecked());
    Options::setAutoMeridianHours(meridianHours->value());
    Options::setUseFITSViewerInCapture(useFITSViewerInCapture->isChecked());

    if (queueTable->rowCount() == 0)
        addJob();

    SequenceJob *first_job = nullptr;

    foreach (SequenceJob *job, jobs)
    {
        if (job->getStatus() == SequenceJob::JOB_IDLE || job->getStatus() == SequenceJob::JOB_ABORTED)
        {
            first_job = job;
            break;
        }
    }

    if (first_job == nullptr)
    {
        foreach (SequenceJob *job, jobs)
        {
            if (job->getStatus() != SequenceJob::JOB_DONE)
            {
                appendLogText(i18n("No pending jobs found. Please add a job to the sequence queue."));
                return;
            }
        }

        if (KMessageBox::warningContinueCancel(
                nullptr,
                i18n("All jobs are complete. Do you want to reset the status of all jobs and restart capturing?"),
                i18n("Reset job status"), KStandardGuiItem::cont(), KStandardGuiItem::cancel(),
                "reset_job_complete_status_warning") != KMessageBox::Continue)
            return;

        foreach (SequenceJob *job, jobs)
            job->resetStatus();

        first_job = jobs.first();
    }

    deviationDetected = false;
    spikeDetected     = false;

    lastFilterOffset = 0;
    // lastFilterOffset should be set to the offset of the current used filter so that any subsequent filter change
    // is made against this startup value
    if (currentFilterPosition > 0)
    {
        QString currentFilterName = FilterPosCombo->itemText(currentFilterPosition - 1);
        foreach (FocusOffset *offset, filterFocusOffsets)
        {
            if (offset->filter == currentFilterName)
            {
                lastFilterOffset = offset->offset;
                break;
            }
        }
    }

    ditherCounter     = Options::ditherFrames();
    initialHA         = getCurrentHA();
    meridianFlipStage = MF_NONE;


    // start timer to measure time until next forced refocus
    startRefocusEveryNTimer();

    // Check if we need to update the sequence directory numbers before starting
    /*for (int i=0; i < jobs.count(); i++)
    {
        QString firstDir = jobs.at(i)->getFITSDir();
        int sequenceID=1;

        for (int j=i+1; j < jobs.count(); j++)
        {
            if (firstDir == jobs.at(j)->getFITSDir())
            {
                jobs.at(i)->setFITSDir(QString("%1/Sequence_1").arg(firstDir));
                jobs.at(j)->setFITSDir(QString("%1/Sequence_%2").arg(jobs.at(j)->getFITSDir()).arg(++sequenceID));
            }
        }
    }*/

    state = CAPTURE_PROGRESS;
    emit newStatus(Ekos::CAPTURE_PROGRESS);

    startB->setIcon(QIcon::fromTheme("media-playback-stop", QIcon(":/icons/breeze/default/media-playback-stop.svg")));
    startB->setToolTip(i18n("Stop Sequence"));
    pauseB->setEnabled(true);

    setBusy(true);

    prepareJob(first_job);
}

void Capture::stop(bool abort)
{
    retries         = 0;
    seqTotalCount   = 0;
    seqCurrentCount = 0;
    //ADURaw1 = ADURaw2 = ExpRaw1 = ExpRaw2 = -1;
    //ADUSlope = 0;
    ADURaw.clear();
    ExpRaw.clear();

    if (activeJob)
    {
        if (activeJob->getStatus() == SequenceJob::JOB_BUSY)
        {
            KNotification::event(QLatin1String("CaptureFailed"), i18n("CCD capture failed with errors"));
            activeJob->abort();
            emit newStatus(Ekos::CAPTURE_ABORTED);
        }

        // In case
        if (activeJob->isPreview() == false)
        {
            activeJob->disconnect(this);
            activeJob->reset();
        }
        else if (calibrationStage == CAL_CALIBRATION)
        {
            activeJob->disconnect(this);
            activeJob->reset();
            activeJob->setPreview(false);
            currentCCD->setUploadMode(rememberUploadMode);
        }
        else // Delete preview job
        {
            jobs.removeOne(activeJob);
            delete (activeJob);
            activeJob = nullptr;
        }
    }

    calibrationStage = CAL_NONE;
    state            = CAPTURE_IDLE;

    // Turn off any calibration light, IF they were turned on by Capture module
    if (dustCap && dustCapLightEnabled)
    {
        dustCapLightEnabled = false;
        dustCap->SetLightEnabled(false);
    }
    if (lightBox && lightBoxLightEnabled)
    {
        lightBoxLightEnabled = false;
        lightBox->SetLightEnabled(false);
    }

    secondsLabel->clear();
    disconnect(currentCCD, SIGNAL(BLOBUpdated(IBLOB *)), this, SLOT(newFITS(IBLOB *)));
    disconnect(currentCCD, SIGNAL(newImage(QImage *, ISD::CCDChip *)), this,
               SLOT(sendNewImage(QImage *, ISD::CCDChip *)));
    disconnect(currentCCD, SIGNAL(newExposureValue(ISD::CCDChip *, double, IPState)), this,
               SLOT(setExposureProgress(ISD::CCDChip *, double, IPState)));

    currentCCD->setFITSDir("");

    imgProgress->reset();
    imgProgress->setEnabled(false);

    fullImgCountOUT->setText(QString());
    currentImgCountOUT->setText(QString());
    exposeOUT->setText(QString());

    setBusy(false);

    if (abort)
    {
        startB->setIcon(
            QIcon::fromTheme("media-playback-start", QIcon(":/icons/breeze/default/media-playback-start.svg")));
        startB->setToolTip(i18n("Start Sequence"));
        pauseB->setEnabled(false);
    }

    //foreach (QAbstractButton *button, queueEditButtonGroup->buttons())
    //button->setEnabled(true);

    seqTimer->stop();
}

void Capture::sendNewImage(QImage *image, ISD::CCDChip *myChip)
{
    if (activeJob && myChip != guideChip)
        emit newImage(image, activeJob);
}

bool Capture::setCCD(QString device)
{
    for (int i = 0; i < CCDCaptureCombo->count(); i++)
        if (device == CCDCaptureCombo->itemText(i))
        {
            CCDCaptureCombo->setCurrentIndex(i);
            return true;
        }

    return false;
}

void Capture::checkCCD(int ccdNum)
{
    if (ccdNum == -1)
    {
        ccdNum = CCDCaptureCombo->currentIndex();

        if (ccdNum == -1)
            return;
    }

    foreach (ISD::CCD *ccd, CCDs)
    {
        disconnect(ccd, SIGNAL(numberUpdated(INumberVectorProperty *)), this,
                   SLOT(processCCDNumber(INumberVectorProperty *)));
        disconnect(ccd, SIGNAL(newTemperatureValue(double)), this, SLOT(updateCCDTemperature(double)));
        disconnect(ccd, SIGNAL(newRemoteFile(QString)), this, SLOT(setNewRemoteFile(QString)));
        disconnect(ccd, SIGNAL(videoStreamToggled(bool)), this, SLOT(setVideoStreamEnabled(bool)));
    }

    if (ccdNum <= CCDs.count())
    {
        // Check whether main camera or guide head only
        currentCCD = CCDs.at(ccdNum);

        if (CCDCaptureCombo->itemText(ccdNum).right(6) == QString("Guider"))
        {
            useGuideHead = true;
            targetChip   = currentCCD->getChip(ISD::CCDChip::GUIDE_CCD);
        }
        else
        {
            currentCCD   = CCDs.at(ccdNum);
            targetChip   = currentCCD->getChip(ISD::CCDChip::PRIMARY_CCD);
            useGuideHead = false;
        }

        if (currentCCD->hasCooler())
        {
            temperatureCheck->setEnabled(true);
            temperatureIN->setEnabled(true);

            if (currentCCD->getBaseDevice()->getPropertyPermission("CCD_TEMPERATURE") != IP_RO)
            {
                double min, max, step;
                setTemperatureB->setEnabled(true);
                temperatureIN->setReadOnly(false);
                temperatureCheck->setEnabled(true);
                currentCCD->getMinMaxStep("CCD_TEMPERATURE", "CCD_TEMPERATURE_VALUE", &min, &max, &step);
                temperatureIN->setMinimum(min);
                temperatureIN->setMaximum(max);
                temperatureIN->setSingleStep(1);
            }
            else
            {
                setTemperatureB->setEnabled(false);
                temperatureIN->setReadOnly(true);
                temperatureCheck->setEnabled(false);
            }

            double temperature = 0;
            if (currentCCD->getTemperature(&temperature))
            {
                temperatureOUT->setText(QString::number(temperature, 'f', 2));
                if (temperatureIN->cleanText().isEmpty())
                    temperatureIN->setValue(temperature);
            }
        }
        else
        {
            temperatureCheck->setEnabled(false);
            temperatureIN->setEnabled(false);
            temperatureIN->clear();
            temperatureOUT->clear();
            setTemperatureB->setEnabled(false);
        }

        updateFrameProperties();

        QStringList frameTypes = targetChip->getFrameTypes();

        frameTypeCombo->clear();

        if (frameTypes.isEmpty())
            frameTypeCombo->setEnabled(false);
        else
        {
            frameTypeCombo->setEnabled(true);
            frameTypeCombo->addItems(frameTypes);
            frameTypeCombo->setCurrentIndex(targetChip->getFrameType());
        }

        QStringList isoList = targetChip->getISOList();
        ISOCombo->clear();

        transferFormatCombo->disconnect();
        transferFormatCombo->clear();

        if (isoList.isEmpty())
        {
            ISOCombo->setEnabled(false);
            ISOLabel->setEnabled(false);
            // Only one trasnfer format
            transferFormatCombo->addItem(i18n("FITS"));
        }
        else
        {
            ISOCombo->setEnabled(true);
            ISOLabel->setEnabled(true);
            ISOCombo->addItems(isoList);
            ISOCombo->setCurrentIndex(targetChip->getISOIndex());

            // DSLRs have two transfer formats
            transferFormatCombo->addItem(i18n("FITS"));
            transferFormatCombo->addItem(i18n("Native"));
            transferFormatCombo->setCurrentIndex(currentCCD->getTargetTransferFormat());
            // Keep track of TARGET transfer format when changing CCDs (FITS or NATIVE). Actual format is not changed until capture
            connect(
                transferFormatCombo, static_cast<void (QComboBox::*)(int)>(&QComboBox::activated), this,
                [&](int index) { currentCCD->setTargetTransferFormat(static_cast<ISD::CCD::TransferFormat>(index)); });

            double pixelX = 0, pixelY = 0;
            bool rc = targetChip->getPixelSize(pixelX, pixelY);
            if (rc == false || pixelX == 0 || pixelY == 0)
            {
                DSLRInfo infoDialog(this, currentCCD);
                infoDialog.exec();
                //appendLogText(i18n("DSLR CCD Information is incomplete. Please update CCD Information in Image Info section in the INDI Control Panel."));
            }
        }

        gainLabel->setEnabled(currentCCD->hasGain());
        gainIN->setEnabled(currentCCD->hasGain());
        if (gainIN->isEnabled())
        {
            double gain = 0, min = 0, max = 0, step = 1;
            currentCCD->getGainMinMaxStep(&min, &max, &step);
            if (currentCCD->getGain(&gain))
            {
                gainIN->setMinimum(min);
                gainIN->setMaximum(max);
                if (step > 0)
                    gainIN->setSingleStep(step);
                gainIN->setValue(gain);
            }
        }
        else
            gainIN->clear();

        liveVideoB->setEnabled(currentCCD->hasVideoStream());
        setVideoStreamEnabled(currentCCD->isStreamingEnabled());

        connect(currentCCD, SIGNAL(numberUpdated(INumberVectorProperty *)), this,
                SLOT(processCCDNumber(INumberVectorProperty *)), Qt::UniqueConnection);
        connect(currentCCD, SIGNAL(newTemperatureValue(double)), this, SLOT(updateCCDTemperature(double)),
                Qt::UniqueConnection);
        connect(currentCCD, SIGNAL(newRemoteFile(QString)), this, SLOT(setNewRemoteFile(QString)));
        connect(currentCCD, SIGNAL(videoStreamToggled(bool)), this, SLOT(setVideoStreamEnabled(bool)));
    }
}

void Capture::setGuideChip(ISD::CCDChip *chip)
{
    guideChip = chip;
    // We should suspend guide in two scenarios:
    // 1. If guide chip is within the primary CCD, then we cannot download any data from guide chip while primary CCD is downloading.
    // 2. If we have two CCDs running from ONE driver (Multiple-Devices-Per-Driver mpdp is true). Same issue as above, only one download
    // at a time.
    // After primary CCD download is complete, we resume guiding.
    suspendGuideOnDownload =
        (currentCCD->getChip(ISD::CCDChip::GUIDE_CCD) == guideChip) ||
        (guideChip->getCCD() == currentCCD && currentCCD->getDriverInfo()->getAuxInfo().value("mdpd", false).toBool());
}

void Capture::resetFrameToZero()
{
    frameXIN->setMinimum(0);
    frameXIN->setMaximum(0);
    frameXIN->setValue(0);

    frameYIN->setMinimum(0);
    frameYIN->setMaximum(0);
    frameYIN->setValue(0);

    frameWIN->setMinimum(0);
    frameWIN->setMaximum(0);
    frameWIN->setValue(0);

    frameHIN->setMinimum(0);
    frameHIN->setMaximum(0);
    frameHIN->setValue(0);
}

void Capture::updateFrameProperties(int reset)
{
    int x, y, w, h;
    int binx = 1, biny = 1;
    double min, max, step;
    int xstep = 0, ystep = 0;

    QString frameProp    = useGuideHead ? QString("GUIDER_FRAME") : QString("CCD_FRAME");
    QString exposureProp = useGuideHead ? QString("GUIDER_EXPOSURE") : QString("CCD_EXPOSURE");
    QString exposureElem = useGuideHead ? QString("GUIDER_EXPOSURE_VALUE") : QString("CCD_EXPOSURE_VALUE");
    targetChip =
        useGuideHead ? currentCCD->getChip(ISD::CCDChip::GUIDE_CCD) : currentCCD->getChip(ISD::CCDChip::PRIMARY_CCD);

    frameWIN->setEnabled(targetChip->canSubframe());
    frameHIN->setEnabled(targetChip->canSubframe());
    frameXIN->setEnabled(targetChip->canSubframe());
    frameYIN->setEnabled(targetChip->canSubframe());

    binXIN->setEnabled(targetChip->canBin());
    binYIN->setEnabled(targetChip->canBin());

    if (currentCCD->getMinMaxStep(exposureProp, exposureElem, &min, &max, &step))
    {
        exposureIN->setMinimum(min);
        exposureIN->setMaximum(max);
        exposureIN->setSingleStep(step);

        if (min < 0.001)
            exposureIN->setDecimals(6);
        else
            exposureIN->setDecimals(3);
    }

    if (currentCCD->getMinMaxStep(frameProp, "WIDTH", &min, &max, &step))
    {
        if (min >= max)
        {
            resetFrameToZero();
            return;
        }

        if (step == 0)
            xstep = (int)max * 0.05;
        else
            xstep = step;

        if (min >= 0 && max > 0)
        {
            frameWIN->setMinimum(min);
            frameWIN->setMaximum(max);
            frameWIN->setSingleStep(xstep);
        }
    }
    else
        return;

    if (currentCCD->getMinMaxStep(frameProp, "HEIGHT", &min, &max, &step))
    {
        if (min >= max)
        {
            resetFrameToZero();
            return;
        }

        if (step == 0)
            ystep = (int)max * 0.05;
        else
            ystep = step;

        if (min >= 0 && max > 0)
        {
            frameHIN->setMinimum(min);
            frameHIN->setMaximum(max);
            frameHIN->setSingleStep(ystep);
        }
    }
    else
        return;

    if (currentCCD->getMinMaxStep(frameProp, "X", &min, &max, &step))
    {
        if (min >= max)
        {
            resetFrameToZero();
            return;
        }

        if (step == 0)
            step = xstep;

        if (min >= 0 && max > 0)
        {
            frameXIN->setMinimum(min);
            frameXIN->setMaximum(max);
            frameXIN->setSingleStep(step);
        }
    }
    else
        return;

    if (currentCCD->getMinMaxStep(frameProp, "Y", &min, &max, &step))
    {
        if (min >= max)
        {
            resetFrameToZero();
            return;
        }

        if (step == 0)
            step = ystep;

        if (min >= 0 && max > 0)
        {
            frameYIN->setMinimum(min);
            frameYIN->setMaximum(max);
            frameYIN->setSingleStep(step);
        }
    }
    else
        return;

    if (reset == 1 || frameSettings.contains(targetChip) == false)
    {
        QVariantMap settings;

        settings["x"]    = 0;
        settings["y"]    = 0;
        settings["w"]    = frameWIN->maximum();
        settings["h"]    = frameHIN->maximum();
        settings["binx"] = 1;
        settings["biny"] = 1;

        frameSettings[targetChip] = settings;
    }
    else if (reset == 2 && frameSettings.contains(targetChip))
    {
        QVariantMap settings = frameSettings[targetChip];
        int x, y, w, h;

        x = settings["x"].toInt();
        y = settings["y"].toInt();
        w = settings["w"].toInt();
        h = settings["h"].toInt();

        // Bound them
        x = qBound(frameXIN->minimum(), x, frameXIN->maximum() - 1);
        y = qBound(frameYIN->minimum(), y, frameYIN->maximum() - 1);
        w = qBound(frameWIN->minimum(), w, frameWIN->maximum());
        h = qBound(frameHIN->minimum(), h, frameHIN->maximum());

        settings["x"] = x;
        settings["y"] = y;
        settings["w"] = w;
        settings["h"] = h;

        frameSettings[targetChip] = settings;
    }

    if (frameSettings.contains(targetChip))
    {
        QVariantMap settings = frameSettings[targetChip];

        if (targetChip->canBin())
        {
            targetChip->getMaxBin(&binx, &biny);
            binXIN->setMaximum(binx);
            binYIN->setMaximum(biny);

            binXIN->setValue(settings["binx"].toInt());
            binYIN->setValue(settings["biny"].toInt());
        }
        else
        {
            binXIN->setValue(1);
            binYIN->setValue(1);
        }

        x = settings["x"].toInt();
        y = settings["y"].toInt();
        w = settings["w"].toInt();
        h = settings["h"].toInt();

        if (x >= 0)
            frameXIN->setValue(x);
        if (y >= 0)
            frameYIN->setValue(y);
        if (w > 0)
            frameWIN->setValue(w);
        if (h > 0)
            frameHIN->setValue(h);
    }
}

void Capture::processCCDNumber(INumberVectorProperty *nvp)
{
    if (currentCCD == nullptr)
        return;

    if ((!strcmp(nvp->name, "CCD_FRAME") && useGuideHead == false) ||
        (!strcmp(nvp->name, "GUIDER_FRAME") && useGuideHead))
        updateFrameProperties();
    else if ((!strcmp(nvp->name, "CCD_INFO") && useGuideHead == false) ||
             (!strcmp(nvp->name, "GUIDER_INFO") && useGuideHead))
        updateFrameProperties(2);
}

void Capture::resetFrame()
{
    targetChip =
        useGuideHead ? currentCCD->getChip(ISD::CCDChip::GUIDE_CCD) : currentCCD->getChip(ISD::CCDChip::PRIMARY_CCD);
    targetChip->resetFrame();
    updateFrameProperties(1);
}

void Capture::syncFrameType(ISD::GDInterface *ccd)
{
    if (strcmp(ccd->getDeviceName(), CCDCaptureCombo->currentText().toLatin1()))
        return;

    ISD::CCDChip *tChip = (static_cast<ISD::CCD *>(ccd))->getChip(ISD::CCDChip::PRIMARY_CCD);

    QStringList frameTypes = tChip->getFrameTypes();

    frameTypeCombo->clear();

    if (frameTypes.isEmpty())
        frameTypeCombo->setEnabled(false);
    else
    {
        frameTypeCombo->setEnabled(true);
        frameTypeCombo->addItems(frameTypes);
        frameTypeCombo->setCurrentIndex(tChip->getFrameType());
    }
}

bool Capture::setFilter(QString device, int filterSlot)
{
    bool deviceFound = false;

    for (int i = 0; i < FilterCaptureCombo->count(); i++)
        if (device == FilterCaptureCombo->itemText(i))
        {
            checkFilter(i);
            deviceFound = true;
            break;
        }

    if (deviceFound == false)
        return false;

    if (filterSlot < FilterCaptureCombo->count())
        FilterCaptureCombo->setCurrentIndex(filterSlot);

    return true;
}

void Capture::checkFilter(int filterNum)
{
    if (filterNum == -1)
    {
        filterNum = FilterCaptureCombo->currentIndex();
        if (filterNum == -1)
            return;
    }

    QStringList filterAlias = Options::filterAlias();

    if (filterNum <= Filters.count())
        currentFilter = Filters.at(filterNum);

    syncFilterInfo();

    FilterPosCombo->clear();

    filterName = currentFilter->getBaseDevice()->getText("FILTER_NAME");
    filterSlot = currentFilter->getBaseDevice()->getNumber("FILTER_SLOT");

    if (filterSlot == nullptr)
    {
        KMessageBox::error(0, i18n("Unable to find FILTER_SLOT property in driver %1",
                                   currentFilter->getBaseDevice()->getDeviceName()));
        return;
    }

    for (int i = 0; i < filterSlot->np[0].max; i++)
    {
        QString item;

        if (filterName != nullptr && (i < filterName->ntp))
            item = filterName->tp[i].text;
        else if (i < filterAlias.count() && filterAlias[i].isEmpty() == false)
            item = filterAlias.at(i);
        else
            item = QString("Filter_%1").arg(i + 1);

        FilterPosCombo->addItem(item);
    }

    FilterPosCombo->setCurrentIndex((int)filterSlot->np[0].value - 1);

    currentFilterPosition = (int)filterSlot->np[0].value;

    if (activeJob &&
        (activeJob->getStatus() == SequenceJob::JOB_ABORTED || activeJob->getStatus() == SequenceJob::JOB_IDLE))
        activeJob->setCurrentFilter(currentFilterPosition);
}

void Capture::syncFilterInfo()
{
    if (currentCCD && currentFilter)
    {
        ITextVectorProperty *activeDevices = currentCCD->getBaseDevice()->getText("ACTIVE_DEVICES");
        if (activeDevices)
        {
            IText *activeFilter = IUFindText(activeDevices, "ACTIVE_FILTER");
            if (activeFilter && strcmp(activeFilter->text, currentFilter->getDeviceName()))
            {
                IUSaveText(activeFilter, currentFilter->getDeviceName());
                currentCCD->getDriverInfo()->getClientManager()->sendNewText(activeDevices);
            }
        }
    }
}

bool Capture::startNextExposure()
{
    if (state == CAPTURE_PAUSED)
    {
        pauseFunction = &Capture::startNextExposure;
        appendLogText(i18n("Sequence paused."));
        secondsLabel->setText(i18n("Paused..."));
        return false;
    }

    if (seqDelay > 0)
    {
        secondsLabel->setText(i18n("Waiting..."));
        state = CAPTURE_WAITING;
        emit newStatus(Ekos::CAPTURE_WAITING);
    }

    seqTimer->start(seqDelay);

    return true;
}

void Capture::newFITS(IBLOB *bp)
{
    ISD::CCDChip *tChip = nullptr;

    // If there is no active job, ignore
    if (activeJob == nullptr || meridianFlipStage >= MF_ALIGNING)
        return;

    if (currentCCD->getUploadMode() != ISD::CCD::UPLOAD_LOCAL)
    {
        if (bp == nullptr)
        {
            abort();
            return;
        }

        if (!strcmp(bp->name, "CCD2"))
            tChip = currentCCD->getChip(ISD::CCDChip::GUIDE_CCD);
        else
            tChip = currentCCD->getChip(ISD::CCDChip::PRIMARY_CCD);

        if (tChip != targetChip)
            return;

        if (targetChip->getCaptureMode() == FITS_FOCUS || targetChip->getCaptureMode() == FITS_GUIDE)
            return;

        // If this is a preview job, make sure to enable preview button after
        // we receive the FITS
        if (activeJob->isPreview() && previewB->isEnabled() == false)
            previewB->setEnabled(true);

        // If the FITS is not for our device, simply ignore
        //if (QString(bp->bvp->device)  != currentCCD->getDeviceName() || (startB->isEnabled() && previewB->isEnabled()))
        if (QString(bp->bvp->device) != currentCCD->getDeviceName() || state == CAPTURE_IDLE)
            return;

        disconnect(currentCCD, SIGNAL(BLOBUpdated(IBLOB *)), this, SLOT(newFITS(IBLOB *)));
        disconnect(currentCCD, SIGNAL(newImage(QImage *, ISD::CCDChip *)), this,
                   SLOT(sendNewImage(QImage *, ISD::CCDChip *)));

        if (useGuideHead == false && darkSubCheck->isChecked() && activeJob->isPreview())
        {
            FITSView *currentImage = targetChip->getImageView(FITS_NORMAL);
            FITSData *darkData     = nullptr;
            uint16_t offsetX       = activeJob->getSubX() / activeJob->getXBin();
            uint16_t offsetY       = activeJob->getSubY() / activeJob->getYBin();

            darkData = DarkLibrary::Instance()->getDarkFrame(targetChip, activeJob->getExposure());

            connect(DarkLibrary::Instance(), SIGNAL(darkFrameCompleted(bool)), this, SLOT(setCaptureComplete()));
            connect(DarkLibrary::Instance(), SIGNAL(newLog(QString)), this, SLOT(appendLogText(QString)));

            if (darkData)
                DarkLibrary::Instance()->subtract(darkData, currentImage, activeJob->getCaptureFilter(), offsetX,
                                                  offsetY);
            else
                DarkLibrary::Instance()->captureAndSubtract(targetChip, currentImage, activeJob->getExposure(), offsetX,
                                                            offsetY);

            return;
        }
    }
    else
        sendNewImage(nullptr, activeJob->getActiveChip());

    setCaptureComplete();
}

bool Capture::setCaptureComplete()
{
    disconnect(currentCCD, SIGNAL(newExposureValue(ISD::CCDChip *, double, IPState)), this,
               SLOT(setExposureProgress(ISD::CCDChip *, double, IPState)));
    DarkLibrary::Instance()->disconnect(this);
    secondsLabel->setText(i18n("Complete."));

    // If it was initially set as preview job
    if (seqTotalCount <= 0)
    {
        jobs.removeOne(activeJob);
        delete (activeJob);
        // Reset active job pointer
        activeJob = nullptr;
        abort();
        return true;
    }

    if (state == CAPTURE_PAUSED)
    {
        pauseFunction = &Capture::setCaptureComplete;
        appendLogText(i18n("Sequence paused."));
        secondsLabel->setText(i18n("Paused..."));
        return false;
    }

    if (activeJob->getFrameType() != FRAME_LIGHT)
    {
        if (processPostCaptureCalibrationStage() == false)
            return true;

        if (calibrationStage == CAL_CALIBRATION_COMPLETE)
            calibrationStage = CAL_CAPTURING;
    }

    seqCurrentCount++;
    activeJob->setCompleted(seqCurrentCount);
    imgProgress->setValue(seqCurrentCount);

    appendLogText(i18n("Received image %1 out of %2.", seqCurrentCount, seqTotalCount));

    state = CAPTURE_IMAGE_RECEIVED;
    emit newStatus(Ekos::CAPTURE_IMAGE_RECEIVED);

    currentImgCountOUT->setText(QString::number(seqCurrentCount));

    // if we're done
    if (seqCurrentCount >= seqTotalCount)
    {
        processJobCompletion();
        return true;
    }

    // Check if meridian condition is met
    if (checkMeridianFlip())
        return true;

    if (activeJob->getPostCaptureScript().isEmpty() == false)
    {
        postCaptureScript.start(activeJob->getPostCaptureScript());
        appendLogText(i18n("Executing post capture script %1", activeJob->getPostCaptureScript()));
    }
    else
        resumeSequence();

    return true;
}

void Capture::processJobCompletion()
{
    activeJob->done();

    stop();

    // Check if meridian condition is met IF there are more pending jobs in the queue
    // Otherwise, no need to check meridian flip is all jobs are over.
    if (getPendingJobCount() > 0 && checkMeridianFlip())
        return;

    // Check if there are more pending jobs and execute them
    if (resumeSequence())
        return;
    // Otherwise, we're done. We park if required and resume guiding if no parking is done and autoguiding was engaged before.
    else
    {
        KNotification::event(QLatin1String("CaptureSuccessful"), i18n("CCD capture sequence completed"));

        abort();

        state = CAPTURE_COMPLETE;
        emit newStatus(Ekos::CAPTURE_COMPLETE);

        //Resume guiding if it was suspended before
        //if (isAutoGuiding && currentCCD->getChip(ISD::CCDChip::GUIDE_CCD) == guideChip)
        if (guideState == GUIDE_SUSPENDED && suspendGuideOnDownload)
            emit resumeGuiding();
    }
}

bool Capture::resumeSequence()
{
    if (state == CAPTURE_PAUSED)
    {
        pauseFunction = &Capture::resumeSequence;
        appendLogText(i18n("Sequence paused."));
        secondsLabel->setText(i18n("Paused..."));
        return false;
    }

    // If seqTotalCount is zero, we have to find if there are more pending jobs in the queue
    if (seqTotalCount == 0)
    {
        SequenceJob *next_job = nullptr;

        foreach (SequenceJob *job, jobs)
        {
            if (job->getStatus() == SequenceJob::JOB_IDLE || job->getStatus() == SequenceJob::JOB_ABORTED)
            {
                next_job = job;
                break;
            }
        }

        if (next_job)
        {
            prepareJob(next_job);

            //Resume guiding if it was suspended before
            //if (isAutoGuiding && currentCCD->getChip(ISD::CCDChip::GUIDE_CCD) == guideChip)
            if (guideState == GUIDE_SUSPENDED && suspendGuideOnDownload)
                emit resumeGuiding();

            return true;
        }
        else
            return false;
    }
    // Otherwise, let's prepare for next exposure after making sure in-sequence focus and dithering are complete if applicable.
    else
    {
        isAutoFocus = (autofocusCheck->isEnabled() && autofocusCheck->isChecked() && HFRPixels->value() > 0);
        if (isAutoFocus)
            autoFocusStatus = false;

        // Reset HFR pixels to zero after merdian flip
        if (isAutoFocus && meridianFlipStage != MF_NONE)
        {
            firstAutoFocus = true;
            HFRPixels->setValue(fileHFR);
        }

        // check if time for forced refocus

        qDebug() << "Elapsed Time (secs): " << getRefocusEveryNTimerElapsedSec() << " Requested Interval (secs): " << refocusEveryN->value()*60;
        if (refocusEveryNCheck->isEnabled() && getRefocusEveryNTimerElapsedSec() >= refocusEveryN->value()*60)
            isRefocus = true;
        else
            isRefocus = false;

        // If we suspended guiding due to primary chip download, resume guide chip guiding now
        if (guideState == GUIDE_SUSPENDED && suspendGuideOnDownload)
            emit resumeGuiding();

        if (guideState == GUIDE_GUIDING && Options::ditherEnabled() && activeJob->getFrameType() == FRAME_LIGHT &&
            --ditherCounter == 0)
        {
            ditherCounter = Options::ditherFrames();

            secondsLabel->setText(i18n("Dithering..."));

            state = CAPTURE_DITHERING;
            emit newStatus(Ekos::CAPTURE_DITHERING);
        }
        else if (isRefocus && activeJob->getFrameType() == FRAME_LIGHT)
        {
            appendLogText(i18n("Scheduled refocus started..."));

            secondsLabel->setText(i18n("Focusing..."));

            // force refocus
            emit checkFocus(0.1);

            state = CAPTURE_FOCUSING;
            emit newStatus(Ekos::CAPTURE_FOCUSING);
        }
        else if (isAutoFocus && activeJob->getFrameType() == FRAME_LIGHT)
        {
            secondsLabel->setText(i18n("Focusing..."));
            emit checkFocus(HFRPixels->value());

            state = CAPTURE_FOCUSING;
            emit newStatus(Ekos::CAPTURE_FOCUSING);
        }
        else
            startNextExposure();
    }

    return true;
}

void Capture::captureOne()
{
    Options::setUseFITSViewerInCapture(useFITSViewerInCapture->isChecked());

    //if (currentCCD->getUploadMode() == ISD::CCD::UPLOAD_LOCAL)
    if (uploadModeCombo->currentIndex() != 0)
    {
        appendLogText(i18n("Cannot take preview image while CCD upload mode is set to local or both. Please change "
                           "upload mode to client and try again."));
        return;
    }

    if (transferFormatCombo->currentIndex() == ISD::CCD::FORMAT_NATIVE && darkSubCheck->isChecked())
    {
        appendLogText(i18n("Cannot perform auto dark subtraction of native DSLR formats."));
        return;
    }

    addJob(true);

    prepareJob(jobs.last());
}

void Capture::captureImage()
{
    if (activeJob == nullptr)
        return;

    seqTimer->stop();
    SequenceJob::CAPTUREResult rc = SequenceJob::CAPTURE_OK;

    if (currentCCD->isConnected() == false)
    {
        appendLogText(i18n("Error: Lost connection to CCD."));
        abort();
        return;
    }

    if (focusState >= FOCUS_PROGRESS)
    {
        appendLogText(i18n("Cannot capture while focus module is busy."));
        abort();
        return;
    }

    if (filterSlot != nullptr)
    {
        currentFilterPosition = (int)filterSlot->np[0].value;
        activeJob->setCurrentFilter(currentFilterPosition);
    }

    if (currentCCD->hasCooler())
    {
        double temperature = 0;
        currentCCD->getTemperature(&temperature);
        activeJob->setCurrentTemperature(temperature);
    }

    connect(currentCCD, SIGNAL(BLOBUpdated(IBLOB *)), this, SLOT(newFITS(IBLOB *)), Qt::UniqueConnection);
    connect(currentCCD, SIGNAL(newImage(QImage *, ISD::CCDChip *)), this, SLOT(sendNewImage(QImage *, ISD::CCDChip *)),
            Qt::UniqueConnection);

    if (activeJob->getFrameType() == FRAME_FLAT)
    {
        // If we have to calibrate ADU levels, first capture must be preview and not in batch mode
        if (activeJob->isPreview() == false && activeJob->getFlatFieldDuration() == DURATION_ADU &&
            calibrationStage == CAL_PRECAPTURE_COMPLETE)
        {
            if (currentCCD->getTransferFormat() == ISD::CCD::FORMAT_NATIVE)
            {
                appendLogText(i18n("Cannot calculate ADU levels in non-FITS images."));
                abort();
                return;
            }

            calibrationStage = CAL_CALIBRATION;
            activeJob->setPreview(true);
            // We need to be in preview mode and in client mode for this to work
            rememberUploadMode = currentCCD->getUploadMode();
            if (currentCCD->getUploadMode() != ISD::CCD::UPLOAD_CLIENT)
            {
                currentCCD->setUploadMode(ISD::CCD::UPLOAD_CLIENT);
            }
        }
    }

    if (currentCCD->getUploadMode() != ISD::CCD::UPLOAD_LOCAL)
    {
        checkSeqBoundary(activeJob->getFITSDir());
        currentCCD->setNextSequenceID(nextSequenceID);
    }

    state = CAPTURE_CAPTURING;

    if (activeJob->isPreview() == false)
        emit newStatus(Ekos::CAPTURE_CAPTURING);

    if (frameSettings.contains(activeJob->getActiveChip()))
    {
        QVariantMap settings;
        settings["x"]    = activeJob->getSubX();
        settings["y"]    = activeJob->getSubY();
        settings["w"]    = activeJob->getSubW();
        settings["h"]    = activeJob->getSubH();
        settings["binx"] = activeJob->getXBin();
        settings["biny"] = activeJob->getYBin();

        frameSettings[activeJob->getActiveChip()] = settings;
    }

    // If using DSLR, make sure it is set to correct transfer format
    currentCCD->setTransformFormat(activeJob->getTransforFormat());

    rc = activeJob->capture(darkSubCheck->isChecked() ? true : false);

    switch (rc)
    {
        case SequenceJob::CAPTURE_OK:
            connect(currentCCD, SIGNAL(newExposureValue(ISD::CCDChip *, double, IPState)), this,
                    SLOT(setExposureProgress(ISD::CCDChip *, double, IPState)), Qt::UniqueConnection);
            appendLogText(i18n("Capturing image..."));
            break;

        case SequenceJob::CAPTURE_FRAME_ERROR:
            appendLogText(i18n("Failed to set sub frame."));
            abort();
            break;

        case SequenceJob::CAPTURE_BIN_ERROR:
            appendLogText(i18n("Failed to set binning."));
            abort();
            break;

        case SequenceJob::CAPTURE_FILTER_BUSY:
            // Try again in 1 second if filter is busy
            QTimer::singleShot(1000, this, SLOT(captureImage()));
            break;

        case SequenceJob::CAPTURE_FOCUS_ERROR:
            appendLogText(i18n("Cannot capture while focus module is busy."));
            abort();
            break;
    }
}

bool Capture::resumeCapture()
{
    if (state == CAPTURE_PAUSED)
    {
        pauseFunction = &Capture::resumeCapture;
        appendLogText(i18n("Sequence paused."));
        secondsLabel->setText(i18n("Paused..."));
        return false;
    }

    appendLogText(i18n("Dither complete."));

    // FIXME ought to be able to combine these - only different is value passed
    //       to checkFocus()
    if (isAutoFocus && autoFocusStatus == false)
    {
        secondsLabel->setText(i18n("Focusing..."));
        emit checkFocus(HFRPixels->value());
        state = CAPTURE_FOCUSING;
        emit newStatus(Ekos::CAPTURE_FOCUSING);
        return true;
    }
    else if (isRefocus)
    {
        appendLogText(i18n("Scheduled refocus started..."));

        secondsLabel->setText(i18n("Focusing..."));
        emit checkFocus(0.1);
        state = CAPTURE_FOCUSING;
        emit newStatus(Ekos::CAPTURE_FOCUSING);
        return true;
    }

    startNextExposure();

    return true;
}

/*******************************************************************************/
/* Update the prefix for the sequence of images to be captured                 */
/*******************************************************************************/
void Capture::updateSequencePrefix(const QString &newPrefix, const QString &dir)
{
    seqPrefix = newPrefix;

    // If it doesn't exist, create it
    QDir().mkpath(dir);

    nextSequenceID = 1;
}

/*******************************************************************************/
/* Determine the next file number sequence. That is, if we have file1.png      */
/* and file2.png, then the next sequence should be file3.png		       */
/*******************************************************************************/
void Capture::checkSeqBoundary(const QString &path)
{
    int newFileIndex = -1;
    QString tempName;
    seqFileCount = 0;

    // No updates during meridian flip
    if (meridianFlipStage >= MF_ALIGNING)
        return;

    QDirIterator it(path, QDir::Files);

    while (it.hasNext())
    {
        tempName = it.next();
        QFileInfo info(tempName);
        //tempName = info.baseName();
        tempName = info.completeBaseName();

        QString finalSeqPrefix = seqPrefix;
        finalSeqPrefix.remove("_ISO8601");
        // find the prefix first
        if (tempName.startsWith(finalSeqPrefix) == false)
            continue;

        seqFileCount++;

        int lastUnderScoreIndex = tempName.lastIndexOf("_");
        if (lastUnderScoreIndex > 0)
        {
            bool indexOK = false;

            newFileIndex = tempName.mid(lastUnderScoreIndex + 1).toInt(&indexOK);
            if (indexOK && newFileIndex >= nextSequenceID)
                nextSequenceID = newFileIndex + 1;
        }
    }
}

void Capture::appendLogText(const QString &text)
{
    logText.insert(0, i18nc("log entry; %1 is the date, %2 is the text", "%1 %2",
                            QDateTime::currentDateTime().toString("yyyy-MM-ddThh:mm:ss"), text));

    if (Options::captureLogging())
        qDebug() << "Capture: " << text;

    emit newLog();
}

void Capture::clearLog()
{
    logText.clear();
    emit newLog();
}

void Capture::setExposureProgress(ISD::CCDChip *tChip, double value, IPState state)
{
    if (targetChip != tChip || targetChip->getCaptureMode() != FITS_NORMAL || meridianFlipStage >= MF_ALIGNING)
        return;

    exposeOUT->setText(QString::number(value, 'd', 2));

    if (activeJob)
    {
        activeJob->setExposeLeft(value);

        emit newExposureProgress(activeJob);
    }

    if (activeJob && state == IPS_ALERT)
    {
        int retries = activeJob->getCaptureRetires() + 1;

        activeJob->setCaptureRetires(retries);

        appendLogText(i18n("Capture failed. Check INDI Control Panel for details."));

        if (retries == 3)
        {
            abort();
            return;
        }

        appendLogText(i18n("Restarting capture attempt #%1", retries));

        nextSequenceID = 1;

        captureImage();
        return;
    }

    //qDebug() << "Exposure with value " << value;

    if (state == IPS_OK)
    {
        activeJob->setCaptureRetires(0);
        activeJob->setExposeLeft(0);

        if (currentCCD && currentCCD->getUploadMode() == ISD::CCD::UPLOAD_LOCAL)
        {
            if (activeJob && activeJob->getStatus() == SequenceJob::JOB_BUSY)
            {
                newFITS(0);
                return;
            }
        }

        //if (isAutoGuiding && Options::useEkosGuider() && currentCCD->getChip(ISD::CCDChip::GUIDE_CCD) == guideChip)
        if (guideState == GUIDE_GUIDING && Options::guiderType() == 0 && suspendGuideOnDownload)
        {
            if (Options::captureLogging())
                qDebug() << "Capture: Autoguiding suspended until primary CCD chip completes downloading...";
            emit suspendGuiding();
        }

        secondsLabel->setText(i18n("Downloading..."));

        //disconnect(currentCCD, SIGNAL(newExposureValue(ISD::CCDChip*,double,IPState)), this, SLOT(updateCaptureProgress(ISD::CCDChip*,double,IPState)));
    }
    // JM: Don't change to i18np, value is DOUBLE, not Integer.
    else if (value <= 1)
        secondsLabel->setText(i18n("second left"));
    else
        secondsLabel->setText(i18n("seconds left"));
}

void Capture::updateCCDTemperature(double value)
{
    if (temperatureCheck->isEnabled() == false)
    {
        if (currentCCD->getBaseDevice()->getPropertyPermission("CCD_TEMPERATURE") != IP_RO)
            checkCCD();
    }

    temperatureOUT->setText(QString::number(value, 'f', 2));

    if (temperatureIN->cleanText().isEmpty())
        temperatureIN->setValue(value);

    if (activeJob &&
        (activeJob->getStatus() == SequenceJob::JOB_ABORTED || activeJob->getStatus() == SequenceJob::JOB_IDLE))
        activeJob->setCurrentTemperature(value);
}

void Capture::updateRotatorNumber(INumberVectorProperty *nvp)
{
    if (!strcmp(nvp->name, "ABS_ROTATOR_POSITION"))
    {
        // Update widget rotator position
        rotatorSettings->setCurrentTicks(static_cast<int32_t>(nvp->np[0].value));

        if (activeJob && (activeJob->getStatus() == SequenceJob::JOB_ABORTED || activeJob->getStatus() == SequenceJob::JOB_IDLE))
            activeJob->setCurrentRotation(static_cast<int32_t>(nvp->np[0].value));
    }
}

void Capture::addJob(bool preview)
{
    SequenceJob *job = nullptr;
    QString imagePrefix;

    if (preview == false && darkSubCheck->isChecked())
    {
        KMessageBox::error(this, i18n("Auto dark subtract is not supported in batch mode."));
        return;
    }

    if (uploadModeCombo->currentIndex() != ISD::CCD::UPLOAD_CLIENT && remoteDirIN->text().isEmpty())
    {
        KMessageBox::error(this, i18n("You must set remote directory for Local & Both modes."));
        return;
    }

    if (jobUnderEdit)
        job = jobs.at(queueTable->currentRow());
    else
        job = new SequenceJob();

    if (job == nullptr)
    {
        qWarning() << "Job is nullptr!" << endl;
        return;
    }

    if (ISOCombo->isEnabled())
        job->setISOIndex(ISOCombo->currentIndex());

    if (gainIN->isEnabled())
        job->setGain(gainIN->value());

    job->setTransforFormat(static_cast<ISD::CCD::TransferFormat>(transferFormatCombo->currentIndex()));

    job->setPreview(preview);

    if (temperatureIN->isEnabled())
    {
        double currentTemperature;
        currentCCD->getTemperature(&currentTemperature);
        job->setEnforceTemperature(temperatureCheck->isChecked());
        job->setTargetTemperature(temperatureIN->value());
        job->setCurrentTemperature(currentTemperature);
    }

    job->setCaptureFilter((FITSScale)filterCombo->currentIndex());

    job->setUploadMode(static_cast<ISD::CCD::UploadMode>(uploadModeCombo->currentIndex()));
    job->setRemoteDir(remoteDirIN->text());
    job->setPostCaptureScript(postCaptureScriptIN->text());
    job->setFlatFieldDuration(flatFieldDuration);
    job->setFlatFieldSource(flatFieldSource);
    job->setPreMountPark(preMountPark);
    job->setPreDomePark(preDomePark);
    job->setWallCoord(wallCoord);
    job->setTargetADU(targetADU);
    job->setTargetADUTolerance(targetADUTolerance);

    imagePrefix = prefixIN->text();

    constructPrefix(imagePrefix);

    job->setPrefixSettings(prefixIN->text(), filterCheck->isChecked(), expDurationCheck->isChecked(),
                           ISOCheck->isChecked());
    job->setFrameType(static_cast<CCDFrameType>(frameTypeCombo->currentIndex()));
    job->setFullPrefix(imagePrefix);

    if (filterSlot != nullptr && currentFilter != nullptr)
        job->setTargetFilter(FilterPosCombo->currentIndex() + 1, FilterPosCombo->currentText());

    job->setExposure(exposureIN->value());

    job->setCount(countIN->value());

    job->setBin(binXIN->value(), binYIN->value());

    job->setDelay(delayIN->value() * 1000); /* in ms */

    job->setActiveChip(targetChip);
    job->setActiveCCD(currentCCD);
    job->setActiveFilter(currentFilter);

    if (currentRotator && rotatorSettings->isRotationEnforced())
    {
        job->setActiveRotator(currentRotator);
        job->setTargetRotation(rotatorSettings->getTargetRotationTicks());
        job->setCurrentRotation(rotatorSettings->getCurrentRotationTicks());
    }

    job->setFrame(frameXIN->value(), frameYIN->value(), frameWIN->value(), frameHIN->value());

    job->setRootFITSDir(fitsDir->text());

    if (jobUnderEdit == false)
    {
        jobs.append(job);

        // Nothing more to do if preview
        if (preview)
            return;
    }

    QString finalFITSDir = fitsDir->text();

    if (targetName.isEmpty())
        finalFITSDir += QLatin1Literal("/") + frameTypeCombo->currentText();
    else
        finalFITSDir += QLatin1Literal("/") + targetName + QLatin1Literal("/") + frameTypeCombo->currentText();
    if ((job->getFrameType() == FRAME_LIGHT || job->getFrameType() == FRAME_FLAT) &&
        job->getFilterName().isEmpty() == false)
        finalFITSDir += QLatin1Literal("/") + job->getFilterName();

    job->setFITSDir(finalFITSDir);

    int currentRow = 0;
    if (jobUnderEdit == false)
    {
        currentRow = queueTable->rowCount();
        queueTable->insertRow(currentRow);
    }
    else
        currentRow = queueTable->currentRow();

    QTableWidgetItem *status = jobUnderEdit ? queueTable->item(currentRow, 0) : new QTableWidgetItem();
    status->setText(job->getStatusString());
    status->setTextAlignment(Qt::AlignHCenter);
    status->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

    job->setStatusCell(status);

    QTableWidgetItem *filter = jobUnderEdit ? queueTable->item(currentRow, 1) : new QTableWidgetItem();
    filter->setText("--");
    /*if (frameTypeCombo->currentText().compare("Bias", Qt::CaseInsensitive) &&
            frameTypeCombo->currentText().compare("Dark", Qt::CaseInsensitive) &&
            FilterPosCombo->count() > 0)*/
    if (FilterPosCombo->count() > 0 &&
        (frameTypeCombo->currentIndex() == FRAME_LIGHT || frameTypeCombo->currentIndex() == FRAME_FLAT))
        filter->setText(FilterPosCombo->currentText());

    filter->setTextAlignment(Qt::AlignHCenter);
    filter->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

    QTableWidgetItem *type = jobUnderEdit ? queueTable->item(currentRow, 2) : new QTableWidgetItem();
    type->setText(frameTypeCombo->currentText());
    type->setTextAlignment(Qt::AlignHCenter);
    type->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

    QTableWidgetItem *bin = jobUnderEdit ? queueTable->item(currentRow, 3) : new QTableWidgetItem();
    bin->setText(QString("%1x%2").arg(binXIN->value()).arg(binYIN->value()));
    bin->setTextAlignment(Qt::AlignHCenter);
    bin->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

    QTableWidgetItem *exp = jobUnderEdit ? queueTable->item(currentRow, 4) : new QTableWidgetItem();
    exp->setText(QString::number(exposureIN->value()));
    exp->setTextAlignment(Qt::AlignHCenter);
    exp->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

    QTableWidgetItem *iso = jobUnderEdit ? queueTable->item(currentRow, 5) : new QTableWidgetItem();
    if (ISOCombo->currentIndex() != -1)
        iso->setText(ISOCombo->currentText());
    else
        iso->setText("--");
    iso->setTextAlignment(Qt::AlignHCenter);
    iso->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

    QTableWidgetItem *count = jobUnderEdit ? queueTable->item(currentRow, 6) : new QTableWidgetItem();
    count->setText(QString::number(countIN->value()));
    count->setTextAlignment(Qt::AlignHCenter);
    count->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

    if (jobUnderEdit == false)
    {
        queueTable->setItem(currentRow, 0, status);
        queueTable->setItem(currentRow, 1, filter);
        queueTable->setItem(currentRow, 2, type);
        queueTable->setItem(currentRow, 3, bin);
        queueTable->setItem(currentRow, 4, exp);
        queueTable->setItem(currentRow, 5, iso);
        queueTable->setItem(currentRow, 6, count);
    }

    removeFromQueueB->setEnabled(true);

    if (queueTable->rowCount() > 0)
    {
        queueSaveAsB->setEnabled(true);
        queueSaveB->setEnabled(true);
        resetB->setEnabled(true);
        mDirty = true;
    }

    if (queueTable->rowCount() > 1)
    {
        queueUpB->setEnabled(true);
        queueDownB->setEnabled(true);
    }

    if (jobUnderEdit)
    {
        jobUnderEdit = false;
        resetJobEdit();
        appendLogText(i18n("Job #%1 changes applied.", currentRow + 1));
    }
}

void Capture::removeJob()
{
    if (jobUnderEdit)
    {
        resetJobEdit();
        return;
    }

    int currentRow = queueTable->currentRow();

    if (currentRow < 0)
    {
        currentRow = queueTable->rowCount() - 1;
        if (currentRow < 0)
            return;
    }

    queueTable->removeRow(currentRow);

    SequenceJob *job = jobs.at(currentRow);
    jobs.removeOne(job);
    if (job == activeJob)
        activeJob = nullptr;

    delete job;

    if (queueTable->rowCount() == 0)
        removeFromQueueB->setEnabled(false);

    if (queueTable->rowCount() == 1)
    {
        queueUpB->setEnabled(false);
        queueDownB->setEnabled(false);
    }

    for (int i = 0; i < jobs.count(); i++)
        jobs.at(i)->setStatusCell(queueTable->item(i, 0));

    queueTable->selectRow(queueTable->currentRow());

    if (queueTable->rowCount() == 0)
    {
        queueSaveAsB->setEnabled(false);
        queueSaveB->setEnabled(false);
        resetB->setEnabled(false);
    }

    mDirty = true;
}

void Capture::moveJobUp()
{
    int currentRow = queueTable->currentRow();

    int columnCount = queueTable->columnCount();

    if (currentRow <= 0 || queueTable->rowCount() == 1)
        return;

    int destinationRow = currentRow - 1;

    for (int i = 0; i < columnCount; i++)
    {
        QTableWidgetItem *downItem = queueTable->takeItem(currentRow, i);
        QTableWidgetItem *upItem   = queueTable->takeItem(destinationRow, i);

        queueTable->setItem(destinationRow, i, downItem);
        queueTable->setItem(currentRow, i, upItem);
    }

    SequenceJob *job = jobs.takeAt(currentRow);

    jobs.removeOne(job);
    jobs.insert(destinationRow, job);

    queueTable->selectRow(destinationRow);

    for (int i = 0; i < jobs.count(); i++)
        jobs.at(i)->setStatusCell(queueTable->item(i, 0));

    mDirty = true;
}

void Capture::moveJobDown()
{
    int currentRow = queueTable->currentRow();

    int columnCount = queueTable->columnCount();

    if (currentRow < 0 || queueTable->rowCount() == 1 || (currentRow + 1) == queueTable->rowCount())
        return;

    int destinationRow = currentRow + 1;

    for (int i = 0; i < columnCount; i++)
    {
        QTableWidgetItem *downItem = queueTable->takeItem(currentRow, i);
        QTableWidgetItem *upItem   = queueTable->takeItem(destinationRow, i);

        queueTable->setItem(destinationRow, i, downItem);
        queueTable->setItem(currentRow, i, upItem);
    }

    SequenceJob *job = jobs.takeAt(currentRow);

    jobs.removeOne(job);
    jobs.insert(destinationRow, job);

    queueTable->selectRow(destinationRow);

    for (int i = 0; i < jobs.count(); i++)
        jobs.at(i)->setStatusCell(queueTable->item(i, 0));

    mDirty = true;
}

void Capture::setBusy(bool enable)
{
    isBusy = enable;

    enable ? pi->startAnimation() : pi->stopAnimation();
    previewB->setEnabled(!enable);

    foreach (QAbstractButton *button, queueEditButtonGroup->buttons())
        button->setEnabled(!enable);
}

void Capture::prepareJob(SequenceJob *job)
{
    activeJob = job;

    if (activeJob->getActiveCCD() != currentCCD)
    {
        setCCD(activeJob->getActiveCCD()->getDeviceName());
    }

    if (currentCCD->getDriverInfo()->getClientManager()->getBLOBMode(currentCCD->getDeviceName(), "CCD1") == B_NEVER)
    {
        if (KMessageBox::questionYesNo(
                0, i18n("Image transfer is disabled for this camera. Would you like to enable it?")) ==
            KMessageBox::Yes)
        {
            currentCCD->getDriverInfo()->getClientManager()->setBLOBMode(B_ALSO, currentCCD->getDeviceName(), "CCD1");
            currentCCD->getDriverInfo()->getClientManager()->setBLOBMode(B_ALSO, currentCCD->getDeviceName(), "CCD2");
        }
        else
        {
            setBusy(false);
            return;
        }
    }

    // Just notification of active job stating up
    emit newImage(nullptr, activeJob);

    connect(job, SIGNAL(checkFocus()), this, SLOT(startPostFilterAutoFocus()));

    // Reset calibration stage
    if (calibrationStage == CAL_CAPTURING)
    {
        if (job->getFrameType() != FRAME_LIGHT)
            calibrationStage = CAL_PRECAPTURE_COMPLETE;
        else
            calibrationStage = CAL_NONE;
    }

    // If we haven't performed a single autofocus yet, we stop
    if (Options::enforceRefocusEveryN() && (isAutoFocus == false && firstAutoFocus == true))
    {
        appendLogText(i18n(
            "Manual scheduled focusing is not supported. Run Autofocus process before trying again."));
        abort();
        return;
    }

    if (currentFilterPosition > 0)
    {
        // If we haven't performed a single autofocus yet, we stop
        if (Options::autoFocusOnFilterChange() && (isAutoFocus == false && firstAutoFocus == true))
        {
            appendLogText(i18n(
                "Manual focusing post filter change is not supported. Run Autofocus process before trying again."));
            abort();
            return;
        }

        if (currentFilterPosition != activeJob->getTargetFilter() && filterFocusOffsets.empty() == false)
        {
            int16_t targetFilterOffset = 0;
            foreach (FocusOffset *offset, filterFocusOffsets)
            {
                if (offset->filter == activeJob->getFilterName())
                {
                    targetFilterOffset = offset->offset - lastFilterOffset;
                    lastFilterOffset   = offset->offset;
                    break;
                }
            }

            if (targetFilterOffset != 0 &&
                (activeJob->getFrameType() == FRAME_LIGHT || activeJob->getFrameType() == FRAME_FLAT))
            {
                appendLogText(i18n("Adjust focus offset by %1 steps", targetFilterOffset));
                secondsLabel->setText(i18n("Adjusting filter offset"));

                if (activeJob->isPreview() == false)
                {
                    state = CAPTURE_FILTER_FOCUS;
                    emit newStatus(Ekos::CAPTURE_FILTER_FOCUS);
                }

                setBusy(true);

                emit newFocusOffset(targetFilterOffset);

                return;
            }
        }
    }

    preparePreCaptureActions();
}

void Capture::preparePreCaptureActions()
{
    if (currentFilterPosition > 0)
    {
        activeJob->setCurrentFilter(currentFilterPosition);

        if (currentFilterPosition != activeJob->getTargetFilter())
        {
            appendLogText(i18n("Changing filter to %1...", FilterPosCombo->itemText(activeJob->getTargetFilter() - 1)));
            secondsLabel->setText(i18n("Set filter..."));

            if (activeJob->isPreview() == false)
            {
                state = CAPTURE_CHANGING_FILTER;
                emit newStatus(Ekos::CAPTURE_CHANGING_FILTER);
            }

            setBusy(true);
        }
    }

    if (currentCCD->hasCooler() && activeJob->getEnforceTemperature())
    {
        if (activeJob->getCurrentTemperature() != INVALID_VALUE &&
            fabs(activeJob->getCurrentTemperature() - activeJob->getTargetTemperature()) >
                Options::maxTemperatureDiff())
        {
            appendLogText(i18n("Setting temperature to %1 C...", activeJob->getTargetTemperature()));
            secondsLabel->setText(i18n("Set %1 C...", activeJob->getTargetTemperature()));

            if (activeJob->isPreview() == false)
            {
                state = CAPTURE_SETTING_TEMPERATURE;
                emit newStatus(Ekos::CAPTURE_SETTING_TEMPERATURE);
            }

            setBusy(true);
        }
    }

    if (activeJob->getTargetRotation() != INVALID_VALUE)
    {
        activeJob->setCurrentRotation(rotatorSettings->getCurrentRotationTicks());

        if (rotatorSettings->getCurrentRotationTicks() != activeJob->getTargetRotation())
        {
            appendLogText(i18n("Setting rotation to %1 ticks...", activeJob->getTargetRotation()));
            secondsLabel->setText(i18n("Set Rotator %1...", activeJob->getTargetRotation()));

            if (activeJob->isPreview() == false)
            {
                state = CAPTURE_SETTING_ROTATOR;
                emit newStatus(Ekos::CAPTURE_SETTING_ROTATOR);
            }

            setBusy(true);
        }
    }

    connect(activeJob, SIGNAL(prepareComplete()), this, SLOT(executeJob()));

    activeJob->prepareCapture();
}

void Capture::executeJob()
{
    activeJob->disconnect(this);

    if (activeJob->isPreview())
        seqTotalCount = -1;
    else
        seqTotalCount = activeJob->getCount();

    seqDelay = activeJob->getDelay();

    seqCurrentCount = activeJob->getCompleted();

    if (activeJob->isPreview() == false)
    {
        fullImgCountOUT->setText(QString::number(seqTotalCount));
        currentImgCountOUT->setText(QString::number(seqCurrentCount));

        // set the progress info
        imgProgress->setEnabled(true);
        imgProgress->setMaximum(seqTotalCount);
        imgProgress->setValue(seqCurrentCount);

        if (currentCCD->getUploadMode() != ISD::CCD::UPLOAD_LOCAL)
            updateSequencePrefix(activeJob->getFullPrefix(), activeJob->getFITSDir());
    }

    // We check if the job is already fully or partially complete by checking how many files of its type exist on the file system unless ignoreJobProgress is set to true
    //if (ignoreJobProgress == false && Options::rememberJobProgress() && activeJob->isPreview() == false)
    if (ignoreJobProgress == false && activeJob->isPreview() == false)
    {
        checkSeqBoundary(activeJob->getFITSDir());

        if (seqFileCount > 0)
        {
            // Fully complete
            if (seqFileCount >= seqTotalCount)
            {
                seqCurrentCount = seqTotalCount;
                activeJob->setCompleted(seqCurrentCount);
                imgProgress->setValue(seqCurrentCount);
                processJobCompletion();
                return;
            }

            // Partially complete
            seqCurrentCount = seqFileCount;
            activeJob->setCompleted(seqCurrentCount);
            currentImgCountOUT->setText(QString::number(seqCurrentCount));
            imgProgress->setValue(seqCurrentCount);

            // Emit progress update
            emit newImage(nullptr, activeJob);
        }

        currentCCD->setNextSequenceID(nextSequenceID);
    }

    QMap<QString, QString> FITSHeader;
    if (observerName.isEmpty() == false)
        FITSHeader["FITS_OBSERVER"] = observerName;
    if (targetName.isEmpty() == false)
        FITSHeader["FITS_OBJECT"] = targetName;
    else if (activeJob->getRawPrefix().isEmpty() == false)
    {
        FITSHeader["FITS_OBJECT"] = activeJob->getRawPrefix();
    }

    if (FITSHeader.count() > 0)
        currentCCD->setFITSHeader(FITSHeader);

    // Update button status
    setBusy(true);

    if (activeJob->isPreview())
    {
        startB->setIcon(
            QIcon::fromTheme("media-playback-stop", QIcon(":/icons/breeze/default/media-playback-stop.svg")));
        startB->setToolTip(i18n("Stop"));
    }

    useGuideHead = (activeJob->getActiveChip()->getType() == ISD::CCDChip::PRIMARY_CCD) ? false : true;

    syncGUIToJob(activeJob);

    // Check flat field frame requirements
    if (activeJob->getFrameType() != FRAME_LIGHT && activeJob->isPreview() == false)
    {
        updatePreCaptureCalibrationStatus();
        return;
    }

    captureImage();
}

void Capture::updatePreCaptureCalibrationStatus()
{
    // If process was aborted or stopped by the user
    if (isBusy == false)
    {
        appendLogText(i18n("Warning: Calibration process was prematurely terminated."));
        return;
    }

    IPState rc = processPreCaptureCalibrationStage();

    if (rc == IPS_ALERT)
        return;
    else if (rc == IPS_BUSY)
    {
        secondsLabel->clear();
        QTimer::singleShot(1000, this, SLOT(updatePreCaptureCalibrationStatus()));
        return;
    }

    captureImage();
}

void Capture::setGuideDeviation(double delta_ra, double delta_dec)
{
    if (guideDeviationCheck->isChecked() == false || activeJob == nullptr)
        return;

    // If guiding is started after a meridian flip we will start getting guide deviations again
    // if the guide deviations are within our limits, we resume the sequence
    if (meridianFlipStage == MF_GUIDING)
    {
        double deviation_rms = sqrt(delta_ra * delta_ra + delta_dec * delta_dec);
        if (deviation_rms < guideDeviation->value())
        {
            initialHA = getCurrentHA();
            appendLogText(i18n("Post meridian flip calibration completed successfully."));
            resumeSequence();
            // N.B. Set meridian flip stage AFTER resumeSequence() always
            meridianFlipStage = MF_NONE;
            return;
        }
    }

    // We don't enforce limit on previews
    if (activeJob->isPreview() || activeJob->getExposeLeft() == 0)
        return;

    double deviation_rms = sqrt(delta_ra * delta_ra + delta_dec * delta_dec);

    QString deviationText = QString("%1").arg(deviation_rms, 0, 'g', 3);

    if (activeJob->getStatus() == SequenceJob::JOB_BUSY && activeJob->getFrameType() == FRAME_LIGHT)
    {
        if (deviation_rms > guideDeviation->value())
        {
            // Ignore spikes ONCE
            if (spikeDetected == false)
            {
                spikeDetected = true;
                return;
            }

            spikeDetected     = false;
            deviationDetected = true;
            appendLogText(i18n("Guiding deviation %1 exceeded limit value of %2 arcsecs, aborting exposure.",
                               deviationText, guideDeviation->value()));
            abort();
            guideDeviationTimer.start();
        }
        return;
    }

    if (activeJob->getStatus() == SequenceJob::JOB_ABORTED && deviationDetected)
    {
        if (deviation_rms <= guideDeviation->value())
        {
            guideDeviationTimer.stop();

            if (seqDelay == 0)
                appendLogText(
                    i18n("Guiding deviation %1 is now lower than limit value of %2 arcsecs, resuming exposure.",
                         deviationText, guideDeviation->value()));
            else
                appendLogText(i18n("Guiding deviation %1 is now lower than limit value of %2 arcsecs, resuming "
                                   "exposure in %3 seconds.",
                                   deviationText, guideDeviation->value(), seqDelay / 1000.0));

            activeJob = nullptr;

            QTimer::singleShot(seqDelay, this, SLOT(start()));
            return;
        }
    }
}

void Capture::setFocusStatus(FocusState state)
{
    focusState = state;

    if (focusState > FOCUS_ABORTED)
        return;

    if (focusState == FOCUS_COMPLETE)
    {
        autofocusCheck->setEnabled(true);
        HFRPixels->setEnabled(true);
        if (focusHFR > 0 && firstAutoFocus && HFRPixels->value() == 0 && fileHFR == 0)
        {
            firstAutoFocus = false;
            // Add 2.5% (default) to the automatic initial HFR value to allow for minute changes in HFR without need to refocus
            // in case in-sequence-focusing is used.
            HFRPixels->setValue(focusHFR + (focusHFR * (Options::hFRThresholdPercentage() / 100.0)));
        }

        // successful focus so reset elapsed time
        restartRefocusEveryNTimer();
    }

    if (activeJob &&
        (activeJob->getStatus() == SequenceJob::JOB_ABORTED || activeJob->getStatus() == SequenceJob::JOB_IDLE))
    {
        if (focusState == FOCUS_COMPLETE)
        {
            HFRPixels->setValue(focusHFR + (focusHFR * 0.025));
            appendLogText(i18n("Focus complete."));
        }
        else if (focusState == FOCUS_FAILED)
        {
            appendLogText(i18n("Autofocus failed. Aborting exposure..."));
            secondsLabel->setText("");
            abort();
        }

        activeJob->setFilterPostFocusReady(focusState == FOCUS_COMPLETE);
        return;
    }

    if ((isRefocus || isAutoFocus) && activeJob && activeJob->getStatus() == SequenceJob::JOB_BUSY)
    {
        if (focusState == FOCUS_COMPLETE)
        {
            appendLogText(i18n("Focus complete."));
            startNextExposure();
        }
        else if (focusState == FOCUS_FAILED)
        {
            appendLogText(i18n("Autofocus failed. Aborting exposure..."));
            secondsLabel->setText("");
            abort();
        }
    }
}

void Capture::setRotator(ISD::GDInterface *newRotator)
{
    currentRotator = newRotator;
    connect(currentRotator, SIGNAL(numberUpdated(INumberVectorProperty*)), this, SLOT(updateRotatorNumber(INumberVectorProperty*)));
    rotatorB->setEnabled(true);

    rotatorSettings->setRotator(newRotator);

    INumberVectorProperty *nvp = newRotator->getBaseDevice()->getNumber("ABS_ROTATOR_POSITION");
    rotatorSettings->setTicksMinMaxStep(static_cast<int32_t>(nvp->np[0].min), static_cast<int32_t>(nvp->np[0].max), static_cast<int32_t>(nvp->np[0].step));
    rotatorSettings->setCurrentTicks(static_cast<int32_t>(nvp->np[0].value));
}

void Capture::setTelescope(ISD::GDInterface *newTelescope)
{
    currentTelescope = static_cast<ISD::Telescope *>(newTelescope);

    connect(currentTelescope, SIGNAL(numberUpdated(INumberVectorProperty *)), this,
            SLOT(processTelescopeNumber(INumberVectorProperty *)), Qt::UniqueConnection);

    meridianCheck->setEnabled(true);
    meridianHours->setEnabled(true);

    syncTelescopeInfo();
}

void Capture::syncTelescopeInfo()
{
    if (currentCCD && currentTelescope && currentTelescope->isConnected())
    {
        ITextVectorProperty *activeDevices = currentCCD->getBaseDevice()->getText("ACTIVE_DEVICES");
        if (activeDevices)
        {
            IText *activeTelescope = IUFindText(activeDevices, "ACTIVE_TELESCOPE");
            if (activeTelescope)
            {
                IUSaveText(activeTelescope, currentTelescope->getDeviceName());

                currentCCD->getDriverInfo()->getClientManager()->sendNewText(activeDevices);
            }
        }
    }
}

void Capture::saveFITSDirectory()
{
    QString dir =
        QFileDialog::getExistingDirectory(KStars::Instance(), i18n("FITS Save Directory"), dirPath.toLocalFile());

    if (dir.isEmpty())
        return;

    fitsDir->setText(dir);
}

void Capture::loadSequenceQueue()
{
    QUrl fileURL = QFileDialog::getOpenFileUrl(KStars::Instance(), i18n("Open Ekos Sequence Queue"), dirPath,
                                               "Ekos Sequence Queue (*.esq)");
    if (fileURL.isEmpty())
        return;

    if (fileURL.isValid() == false)
    {
        QString message = i18n("Invalid URL: %1", fileURL.toLocalFile());
        KMessageBox::sorry(0, message, i18n("Invalid URL"));
        return;
    }

    dirPath = QUrl(fileURL.url(QUrl::RemoveFilename));

    loadSequenceQueue(fileURL.toLocalFile());
}

bool Capture::loadSequenceQueue(const QString &fileURL)
{
    QFile sFile;
    sFile.setFileName(fileURL);

    if (!sFile.open(QIODevice::ReadOnly))
    {
        QString message = i18n("Unable to open file %1", fileURL);
        KMessageBox::sorry(0, message, i18n("Could Not Open File"));
        return false;
    }

    //QTextStream instream(&sFile);

    qDeleteAll(jobs);
    jobs.clear();
    while (queueTable->rowCount() > 0)
        queueTable->removeRow(0);

    LilXML *xmlParser = newLilXML();

    char errmsg[MAXRBUF];
    XMLEle *root = nullptr;
    XMLEle *ep   = nullptr;
    char c;

    while (sFile.getChar(&c))
    {
        root = readXMLEle(xmlParser, c, errmsg);

        if (root)
        {
            double sqVersion = atof(findXMLAttValu(root, "version"));
            if (sqVersion < SQ_COMPAT_VERSION)
            {
                appendLogText(i18n("Deprecated sequence file format version %1. Please construct a new sequence file.",
                                   sqVersion));
                return false;
            }

            for (ep = nextXMLEle(root, 1); ep != nullptr; ep = nextXMLEle(root, 0))
            {
                if (!strcmp(tagXMLEle(ep), "Observer"))
                {
                    observerName = QString(pcdataXMLEle(ep));
                }
                else if (!strcmp(tagXMLEle(ep), "GuideDeviation"))
                {
                    if (!strcmp(findXMLAttValu(ep, "enabled"), "true"))
                    {
                        guideDeviationCheck->setChecked(true);
                        guideDeviation->setValue(atof(pcdataXMLEle(ep)));
                    }
                    else
                        guideDeviationCheck->setChecked(false);
                }
                else if (!strcmp(tagXMLEle(ep), "Autofocus"))
                {
                    if (!strcmp(findXMLAttValu(ep, "enabled"), "true"))
                    {
                        autofocusCheck->setChecked(true);
                        float HFRValue = atof(pcdataXMLEle(ep));
                        if (HFRValue > 0)
                        {
                            fileHFR = HFRValue;
                            HFRPixels->setValue(HFRValue);
                        }
                        else
                            fileHFR = 0;
                    }
                    else
                        autofocusCheck->setChecked(false);
                }
                else if (!strcmp(tagXMLEle(ep), "RefocusEveryN"))
                {
                    if (!strcmp(findXMLAttValu(ep, "enabled"), "true"))
                    {
                        refocusEveryNCheck->setChecked(true);
                        int minutesValue = atof(pcdataXMLEle(ep));
                        if (minutesValue > 0)
                        {
                            refocusEveryNMinutesValue = minutesValue;
                            refocusEveryN->setValue(refocusEveryNMinutesValue);
                        }
                        else
                            refocusEveryNMinutesValue = 0;
                    }
                    else
                        refocusEveryNCheck->setChecked(false);

                    qDebug() << "Read Requested Interval (min): " << refocusEveryNMinutesValue;

                }
                else if (!strcmp(tagXMLEle(ep), "MeridianFlip"))
                {
                    if (!strcmp(findXMLAttValu(ep, "enabled"), "true"))
                    {
                        meridianCheck->setChecked(true);
                        meridianHours->setValue(atof(pcdataXMLEle(ep)));
                    }
                    else
                        meridianCheck->setChecked(false);
                }
                else
                {
                    processJobInfo(ep);
                }
            }
            delXMLEle(root);
        }
        else if (errmsg[0])
        {
            appendLogText(QString(errmsg));
            delLilXML(xmlParser);
            return false;
        }
    }

    sequenceURL = QUrl::fromLocalFile(fileURL);
    mDirty      = false;
    delLilXML(xmlParser);

    ignoreJobProgress = !(Options::rememberJobProgress());

    return true;
}

bool Capture::processJobInfo(XMLEle *root)
{
    XMLEle *ep;
    XMLEle *subEP;
    rotatorSettings->setRotationEnforced(false);

    for (ep = nextXMLEle(root, 1); ep != nullptr; ep = nextXMLEle(root, 0))
    {
        if (!strcmp(tagXMLEle(ep), "Exposure"))
            exposureIN->setValue(atof(pcdataXMLEle(ep)));
        else if (!strcmp(tagXMLEle(ep), "Binning"))
        {
            subEP = findXMLEle(ep, "X");
            if (subEP)
                binXIN->setValue(atoi(pcdataXMLEle(subEP)));
            subEP = findXMLEle(ep, "Y");
            if (subEP)
                binYIN->setValue(atoi(pcdataXMLEle(subEP)));
        }
        else if (!strcmp(tagXMLEle(ep), "Frame"))
        {
            subEP = findXMLEle(ep, "X");
            if (subEP)
                frameXIN->setValue(atoi(pcdataXMLEle(subEP)));
            subEP = findXMLEle(ep, "Y");
            if (subEP)
                frameYIN->setValue(atoi(pcdataXMLEle(subEP)));
            subEP = findXMLEle(ep, "W");
            if (subEP)
                frameWIN->setValue(atoi(pcdataXMLEle(subEP)));
            subEP = findXMLEle(ep, "H");
            if (subEP)
                frameHIN->setValue(atoi(pcdataXMLEle(subEP)));
        }
        else if (!strcmp(tagXMLEle(ep), "Temperature"))
        {
            if (temperatureIN->isEnabled())
                temperatureIN->setValue(atof(pcdataXMLEle(ep)));

            // If force attribute exist, we change temperatureCheck, otherwise do nothing.
            if (!strcmp(findXMLAttValu(ep, "force"), "true"))
                temperatureCheck->setChecked(true);
            else if (!strcmp(findXMLAttValu(ep, "force"), "false"))
                temperatureCheck->setChecked(false);
        }
        else if (!strcmp(tagXMLEle(ep), "Filter"))
        {
            //FilterPosCombo->setCurrentIndex(atoi(pcdataXMLEle(ep))-1);
            FilterPosCombo->setCurrentText(pcdataXMLEle(ep));
        }
        else if (!strcmp(tagXMLEle(ep), "Type"))
        {
            frameTypeCombo->setCurrentText(pcdataXMLEle(ep));
        }
        else if (!strcmp(tagXMLEle(ep), "Prefix"))
        {
            subEP = findXMLEle(ep, "RawPrefix");
            if (subEP)
                prefixIN->setText(pcdataXMLEle(subEP));
            subEP = findXMLEle(ep, "FilterEnabled");
            if (subEP)
                filterCheck->setChecked(!strcmp("1", pcdataXMLEle(subEP)));
            subEP = findXMLEle(ep, "ExpEnabled");
            if (subEP)
                expDurationCheck->setChecked(!strcmp("1", pcdataXMLEle(subEP)));
            subEP = findXMLEle(ep, "TimeStampEnabled");
            if (subEP)
                ISOCheck->setChecked(!strcmp("1", pcdataXMLEle(subEP)));
        }
        else if (!strcmp(tagXMLEle(ep), "Count"))
        {
            countIN->setValue(atoi(pcdataXMLEle(ep)));
        }
        else if (!strcmp(tagXMLEle(ep), "Delay"))
        {
            delayIN->setValue(atoi(pcdataXMLEle(ep)));
        }
        else if (!strcmp(tagXMLEle(ep), "PostCaptureScript"))
        {
            postCaptureScriptIN->setText(pcdataXMLEle(ep));
        }
        else if (!strcmp(tagXMLEle(ep), "FITSDirectory"))
        {
            fitsDir->setText(pcdataXMLEle(ep));
        }
        else if (!strcmp(tagXMLEle(ep), "RemoteDirectory"))
        {
            remoteDirIN->setText(pcdataXMLEle(ep));
        }
        else if (!strcmp(tagXMLEle(ep), "UploadMode"))
        {
            uploadModeCombo->setCurrentIndex(atoi(pcdataXMLEle(ep)));
        }
        else if (!strcmp(tagXMLEle(ep), "ISOIndex"))
        {
            if (ISOCombo->isEnabled())
                ISOCombo->setCurrentIndex(atoi(pcdataXMLEle(ep)));
        }
        else if (!strcmp(tagXMLEle(ep), "Gain"))
        {
            if (gainIN->isEnabled())
                gainIN->setValue(atof(pcdataXMLEle(ep)));
        }
        else if (!strcmp(tagXMLEle(ep), "FormatIndex"))
        {
            transferFormatCombo->setCurrentIndex(atoi(pcdataXMLEle(ep)));
        }
        else if (!strcmp(tagXMLEle(ep), "Rotation"))
        {
            rotatorSettings->setRotationEnforced(true);
            rotatorSettings->setTargetRotationTicks(atoi(pcdataXMLEle(ep)));
        }
        else if (!strcmp(tagXMLEle(ep), "Calibration"))
        {
            subEP = findXMLEle(ep, "FlatSource");
            if (subEP)
            {
                XMLEle *typeEP = findXMLEle(subEP, "Type");
                if (typeEP)
                {
                    if (!strcmp(pcdataXMLEle(typeEP), "Manual"))
                        flatFieldSource = SOURCE_MANUAL;
                    else if (!strcmp(pcdataXMLEle(typeEP), "FlatCap"))
                        flatFieldSource = SOURCE_FLATCAP;
                    else if (!strcmp(pcdataXMLEle(typeEP), "DarkCap"))
                        flatFieldSource = SOURCE_DARKCAP;
                    else if (!strcmp(pcdataXMLEle(typeEP), "Wall"))
                    {
                        XMLEle *azEP  = findXMLEle(subEP, "Az");
                        XMLEle *altEP = findXMLEle(subEP, "Alt");

                        if (azEP && altEP)
                        {
                            flatFieldSource = SOURCE_WALL;
                            wallCoord.setAz(atof(pcdataXMLEle(azEP)));
                            wallCoord.setAlt(atof(pcdataXMLEle(altEP)));
                        }
                    }
                    else
                        flatFieldSource = SOURCE_DAWN_DUSK;
                }
            }

            subEP = findXMLEle(ep, "FlatDuration");
            if (subEP)
            {
                XMLEle *typeEP = findXMLEle(subEP, "Type");
                if (typeEP)
                {
                    if (!strcmp(pcdataXMLEle(typeEP), "Manual"))
                        flatFieldDuration = DURATION_MANUAL;
                }

                XMLEle *aduEP = findXMLEle(subEP, "Value");
                if (aduEP)
                {
                    flatFieldDuration = DURATION_ADU;
                    targetADU         = atof(pcdataXMLEle(aduEP));
                }

                aduEP = findXMLEle(subEP, "Tolerance");
                if (aduEP)
                {
                    targetADUTolerance = atof(pcdataXMLEle(aduEP));
                }
            }

            subEP = findXMLEle(ep, "PreMountPark");
            if (subEP)
            {
                if (!strcmp(pcdataXMLEle(subEP), "True"))
                    preMountPark = true;
                else
                    preMountPark = false;
            }

            subEP = findXMLEle(ep, "PreDomePark");
            if (subEP)
            {
                if (!strcmp(pcdataXMLEle(subEP), "True"))
                    preDomePark = true;
                else
                    preDomePark = false;
            }
        }
    }

    addJob(false);

    return true;
}

void Capture::saveSequenceQueue()
{
    QUrl backupCurrent = sequenceURL;

    if (sequenceURL.toLocalFile().startsWith("/tmp/") || sequenceURL.toLocalFile().contains("/Temp"))
        sequenceURL.clear();

    // If no changes made, return.
    if (mDirty == false && !sequenceURL.isEmpty())
        return;

    if (sequenceURL.isEmpty())
    {
        sequenceURL = QFileDialog::getSaveFileUrl(KStars::Instance(), i18n("Save Ekos Sequence Queue"), dirPath,
                                                  "Ekos Sequence Queue (*.esq)");
        // if user presses cancel
        if (sequenceURL.isEmpty())
        {
            sequenceURL = backupCurrent;
            return;
        }

        dirPath = QUrl(sequenceURL.url(QUrl::RemoveFilename));

        if (sequenceURL.toLocalFile().endsWith(".esq") == false)
            sequenceURL.setPath(sequenceURL.toLocalFile() + ".esq");

        if (QFile::exists(sequenceURL.toLocalFile()))
        {
            int r = KMessageBox::warningContinueCancel(0,
                                                       i18n("A file named \"%1\" already exists. "
                                                            "Overwrite it?",
                                                            sequenceURL.fileName()),
                                                       i18n("Overwrite File?"), KStandardGuiItem::overwrite());
            if (r == KMessageBox::Cancel)
                return;
        }
    }

    if (sequenceURL.isValid())
    {
        if ((saveSequenceQueue(sequenceURL.toLocalFile())) == false)
        {
            KMessageBox::error(KStars::Instance(), i18n("Failed to save sequence queue"), i18n("Save"));
            return;
        }

        mDirty = false;
    }
    else
    {
        QString message = i18n("Invalid URL: %1", sequenceURL.url());
        KMessageBox::sorry(KStars::Instance(), message, i18n("Invalid URL"));
    }
}

void Capture::saveSequenceQueueAs()
{
    sequenceURL.clear();
    saveSequenceQueue();
}

bool Capture::saveSequenceQueue(const QString &path)
{
    QFile file;
    QString rawPrefix;
    bool filterEnabled, expEnabled, tsEnabled;
    const QMap<QString, CCDFrameType> frameTypes = {
        { "Light", FRAME_LIGHT }, { "Dark", FRAME_DARK }, { "Bias", FRAME_BIAS }, { "Flat", FRAME_FLAT }
    };

    file.setFileName(path);

    if (!file.open(QIODevice::WriteOnly))
    {
        QString message = i18n("Unable to write to file %1", path);
        KMessageBox::sorry(0, message, i18n("Could Not Open File"));
        return false;
    }

    QTextStream outstream(&file);

    outstream << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << endl;
    outstream << "<SequenceQueue version='" << SQ_FORMAT_VERSION << "'>" << endl;
    if (observerName.isEmpty() == false)
        outstream << "<Observer>" << observerName << "</Observer>" << endl;
    outstream << "<GuideDeviation enabled='" << (guideDeviationCheck->isChecked() ? "true" : "false") << "'>"
              << guideDeviation->value() << "</GuideDeviation>" << endl;
    outstream << "<Autofocus enabled='" << (autofocusCheck->isChecked() ? "true" : "false") << "'>"
              << HFRPixels->value() << "</Autofocus>" << endl;
    outstream << "<RefocusEveryN enabled='" << (refocusEveryNCheck->isChecked() ? "true" : "false") << "'>"
              << refocusEveryN->value() << "</RefocusEveryN>" << endl;
    outstream << "<MeridianFlip enabled='" << (meridianCheck->isChecked() ? "true" : "false") << "'>"
              << meridianHours->value() << "</MeridianFlip>" << endl;
    foreach (SequenceJob *job, jobs)
    {
        job->getPrefixSettings(rawPrefix, filterEnabled, expEnabled, tsEnabled);

        outstream << "<Job>" << endl;

        outstream << "<Exposure>" << job->getExposure() << "</Exposure>" << endl;
        outstream << "<Binning>" << endl;
        outstream << "<X>" << job->getXBin() << "</X>" << endl;
        outstream << "<Y>" << job->getXBin() << "</Y>" << endl;
        outstream << "</Binning>" << endl;
        outstream << "<Frame>" << endl;
        outstream << "<X>" << job->getSubX() << "</X>" << endl;
        outstream << "<Y>" << job->getSubY() << "</Y>" << endl;
        outstream << "<W>" << job->getSubW() << "</W>" << endl;
        outstream << "<H>" << job->getSubH() << "</H>" << endl;
        outstream << "</Frame>" << endl;
        if (job->getTargetTemperature() != INVALID_VALUE)
            outstream << "<Temperature force='" << (job->getEnforceTemperature() ? "true" : "false") << "'>"
                      << job->getTargetTemperature() << "</Temperature>" << endl;
        if (job->getTargetFilter() >= 0)
            //outstream << "<Filter>" << job->getTargetFilter() << "</Filter>" << endl;
            outstream << "<Filter>" << job->getFilterName() << "</Filter>" << endl;
        outstream << "<Type>" << frameTypes.key(job->getFrameType()) << "</Type>" << endl;
        outstream << "<Prefix>" << endl;
        //outstream << "<CompletePrefix>" << job->getPrefix() << "</CompletePrefix>" << endl;
        outstream << "<RawPrefix>" << rawPrefix << "</RawPrefix>" << endl;
        outstream << "<FilterEnabled>" << (filterEnabled ? 1 : 0) << "</FilterEnabled>" << endl;
        outstream << "<ExpEnabled>" << (expEnabled ? 1 : 0) << "</ExpEnabled>" << endl;
        outstream << "<TimeStampEnabled>" << (tsEnabled ? 1 : 0) << "</TimeStampEnabled>" << endl;
        outstream << "</Prefix>" << endl;
        outstream << "<Count>" << job->getCount() << "</Count>" << endl;
        // ms to seconds
        outstream << "<Delay>" << job->getDelay() / 1000 << "</Delay>" << endl;
        if (job->getPostCaptureScript().isEmpty() == false)
            outstream << "<PostCaptureScript>" << job->getPostCaptureScript() << "</PostCaptureScript>" << endl;
        QString rootDir = job->getRootFITSDir();
        outstream << "<FITSDirectory>" << rootDir << "</FITSDirectory>" << endl;
        outstream << "<UploadMode>" << job->getUploadMode() << "</UploadMode>" << endl;
        if (job->getRemoteDir().isEmpty() == false)
            outstream << "<RemoteDirectory>" << job->getRemoteDir() << "</RemoteDirectory>" << endl;
        if (job->getISOIndex() != -1)
            outstream << "<ISOIndex>" << (job->getISOIndex()) << "</ISOIndex>" << endl;
        if (job->getGain() != -1)
            outstream << "<Gain>" << (job->getGain()) << "</Gain>" << endl;
        outstream << "<FormatIndex>" << (job->getTransforFormat()) << "</FormatIndex>" << endl;
        if (job->getTargetRotation() != INVALID_VALUE)
            outstream << "<Rotation>" << (job->getTargetRotation()) << "</Rotation>" << endl;

        outstream << "<Calibration>" << endl;
        outstream << "<FlatSource>" << endl;
        if (job->getFlatFieldSource() == SOURCE_MANUAL)
            outstream << "<Type>Manual</Type>" << endl;
        else if (job->getFlatFieldSource() == SOURCE_FLATCAP)
            outstream << "<Type>FlatCap</Type>" << endl;
        else if (job->getFlatFieldSource() == SOURCE_DARKCAP)
            outstream << "<Type>DarkCap</Type>" << endl;
        else if (job->getFlatFieldSource() == SOURCE_WALL)
        {
            outstream << "<Type>Wall</Type>" << endl;
            outstream << "<Az>" << job->getWallCoord().az().Degrees() << "</Az>" << endl;
            outstream << "<Alt>" << job->getWallCoord().alt().Degrees() << "</Alt>" << endl;
        }
        else
            outstream << "<Type>DawnDust</Type>" << endl;
        outstream << "</FlatSource>" << endl;

        outstream << "<FlatDuration>" << endl;
        if (job->getFlatFieldDuration() == DURATION_MANUAL)
            outstream << "<Type>Manual</Type>" << endl;
        else
        {
            outstream << "<Type>ADU</Type>" << endl;
            outstream << "<Value>" << job->getTargetADU() << "</Value>" << endl;
            outstream << "<Tolerance>" << job->getTargetADUTolerance() << "</Tolerance>" << endl;
        }
        outstream << "</FlatDuration>" << endl;

        outstream << "<PreMountPark>" << (job->isPreMountPark() ? "True" : "False") << "</PreMountPark>" << endl;
        outstream << "<PreDomePark>" << (job->isPreDomePark() ? "True" : "False") << "</PreDomePark>" << endl;
        outstream << "</Calibration>" << endl;

        outstream << "</Job>" << endl;
    }

    outstream << "</SequenceQueue>" << endl;

    appendLogText(i18n("Sequence queue saved to %1", path));
    file.close();
    return true;
}

void Capture::resetJobs()
{
    if (KMessageBox::warningContinueCancel(
            nullptr, i18n("Are you sure you want to reset status of all jobs?"), i18n("Reset job status"),
            KStandardGuiItem::cont(), KStandardGuiItem::cancel(), "reset_job_status_warning") != KMessageBox::Continue)
    {
        return;
    }

    foreach (SequenceJob *job, jobs)
        job->resetStatus();

    // Reset active job pointer
    activeJob = nullptr;

    stop();

    ignoreJobProgress = true;
}

void Capture::ignoreSequenceHistory()
{
    ignoreJobProgress = true;
}

void Capture::syncGUIToJob(SequenceJob *job)
{
    QString rawPrefix;
    bool filterEnabled, expEnabled, tsEnabled;

    job->getPrefixSettings(rawPrefix, filterEnabled, expEnabled, tsEnabled);

    exposureIN->setValue(job->getExposure());
    binXIN->setValue(job->getXBin());
    binYIN->setValue(job->getYBin());
    frameXIN->setValue(job->getSubX());
    frameYIN->setValue(job->getSubY());
    frameWIN->setValue(job->getSubW());
    frameHIN->setValue(job->getSubH());
    FilterPosCombo->setCurrentIndex(job->getTargetFilter() - 1);
    frameTypeCombo->setCurrentIndex(job->getFrameType());
    prefixIN->setText(rawPrefix);
    filterCheck->setChecked(filterEnabled);
    expDurationCheck->setChecked(expEnabled);
    ISOCheck->setChecked(tsEnabled);
    countIN->setValue(job->getCount());
    delayIN->setValue(job->getDelay() / 1000);
    postCaptureScriptIN->setText(job->getPostCaptureScript());
    uploadModeCombo->setCurrentIndex(job->getUploadMode());
    remoteDirIN->setEnabled(uploadModeCombo->currentIndex() != 0);
    remoteDirIN->setText(job->getRemoteDir());

    // Temperature Options
    temperatureCheck->setChecked(job->getEnforceTemperature());

    // Flat field options
    calibrationB->setEnabled(job->getFrameType() != FRAME_LIGHT);
    flatFieldDuration  = job->getFlatFieldDuration();
    flatFieldSource    = job->getFlatFieldSource();
    targetADU          = job->getTargetADU();
    targetADUTolerance = job->getTargetADUTolerance();
    wallCoord          = job->getWallCoord();
    preMountPark       = job->isPreMountPark();
    preDomePark        = job->isPreDomePark();

    fitsDir->setText(job->getRootFITSDir());

    if (ISOCombo->isEnabled())
        ISOCombo->setCurrentIndex(job->getISOIndex());

    if (gainIN->isEnabled())
        gainIN->setValue(job->getGain());

    transferFormatCombo->setCurrentIndex(job->getTransforFormat());

    if (job->getTargetRotation() != INVALID_VALUE)
    {
        rotatorSettings->setRotationEnforced(true);
        rotatorSettings->setTargetRotationTicks(job->getTargetRotation());
    }
    else
        rotatorSettings->setRotationEnforced(false);
}

void Capture::editJob(QModelIndex i)
{
    SequenceJob *job = jobs.at(i.row());

    if (job == nullptr)
        return;

    syncGUIToJob(job);

    appendLogText(i18n("Editing job #%1...", i.row() + 1));

    addToQueueB->setIcon(QIcon::fromTheme("dialog-ok-apply", QIcon(":/icons/breeze/default/dialog-ok-apply.svg")));
    addToQueueB->setToolTip(i18n("Apply job changes."));
    removeFromQueueB->setToolTip(i18n("Cancel job changes."));

    jobUnderEdit = true;
}

void Capture::resetJobEdit()
{
    if (jobUnderEdit)
        appendLogText(i18n("Editing job canceled."));

    jobUnderEdit = false;
    addToQueueB->setIcon(QIcon::fromTheme("list-add", QIcon(":/icons/breeze/default/list-add.svg")));

    addToQueueB->setToolTip(i18n("Add job to sequence queue"));
    removeFromQueueB->setToolTip(i18n("Remove job from sequence queue"));
}

void Capture::constructPrefix(QString &imagePrefix)
{
    if (imagePrefix.isEmpty() == false)
        imagePrefix += '_';

    imagePrefix += frameTypeCombo->currentText();

    /*if (filterCheck->isChecked() && FilterPosCombo->currentText().isEmpty() == false &&
            frameTypeCombo->currentText().compare("Bias", Qt::CaseInsensitive) &&
            frameTypeCombo->currentText().compare("Dark", Qt::CaseInsensitive))*/
    if (filterCheck->isChecked() && FilterPosCombo->currentText().isEmpty() == false &&
        (frameTypeCombo->currentIndex() == FRAME_LIGHT || frameTypeCombo->currentIndex() == FRAME_FLAT))
    {
        imagePrefix += '_';
        imagePrefix += FilterPosCombo->currentText();
    }
    if (expDurationCheck->isChecked())
    {
        //if (imagePrefix.isEmpty() == false || frameTypeCheck->isChecked())
        imagePrefix += '_';

        double exposureValue = exposureIN->value();

        if (exposureValue == static_cast<int>(exposureValue))
            // Whole number
            imagePrefix += QString::number(exposureIN->value(), 'd', 0) + QString("_secs");
        // Decimal
        else
            imagePrefix += QString::number(exposureIN->value(), 'f', 3) + QString("_secs");
    }
    if (ISOCheck->isChecked())
    {
        imagePrefix += "_ISO8601";
    }
}

double Capture::getProgressPercentage()
{
    int totalImageCount     = 0;
    int totalImageCompleted = 0;

    foreach (SequenceJob *job, jobs)
    {
        totalImageCount += job->getCount();
        totalImageCompleted += job->getCompleted();
    }

    if (totalImageCount != 0)
        return (((double)totalImageCompleted / totalImageCount) * 100.0);
    else
        return -1;
}

int Capture::getActiveJobID()
{
    if (activeJob == nullptr)
        return -1;

    for (int i = 0; i < jobs.count(); i++)
    {
        if (activeJob == jobs[i])
            return i;
    }

    return -1;
}

int Capture::getPendingJobCount()
{
    int completedJobs = 0;

    foreach (SequenceJob *job, jobs)
    {
        if (job->getStatus() == SequenceJob::JOB_DONE)
            completedJobs++;
    }

    return (jobs.count() - completedJobs);
}

QString Capture::getJobState(int id)
{
    if (id < jobs.count())
    {
        SequenceJob *job = jobs.at(id);
        return job->getStatusString();
    }

    return QString();
}

int Capture::getJobImageProgress(int id)
{
    if (id < jobs.count())
    {
        SequenceJob *job = jobs.at(id);
        return job->getCompleted();
    }

    return -1;
}

int Capture::getJobImageCount(int id)
{
    if (id < jobs.count())
    {
        SequenceJob *job = jobs.at(id);
        return job->getCount();
    }

    return -1;
}

double Capture::getJobExposureProgress(int id)
{
    if (id < jobs.count())
    {
        SequenceJob *job = jobs.at(id);
        return job->getExposeLeft();
    }

    return -1;
}

double Capture::getJobExposureDuration(int id)
{
    if (id < jobs.count())
    {
        SequenceJob *job = jobs.at(id);
        return job->getExposure();
    }

    return -1;
}

int Capture::getJobRemainingTime(SequenceJob *job)
{
    int remaining = 0;

    if (job->getStatus() == SequenceJob::JOB_BUSY)
        remaining += (job->getExposure() + job->getDelay() / 1000) * (job->getCount() - job->getCompleted()) +
                     job->getExposeLeft();
    else
        remaining += (job->getExposure() + job->getDelay() / 1000) * (job->getCount() - job->getCompleted());

    return remaining;
}

int Capture::getOverallRemainingTime()
{
    double remaining = 0;

    foreach (SequenceJob *job, jobs)
        remaining += getJobRemainingTime(job);

    return remaining;
}

int Capture::getActiveJobRemainingTime()
{
    if (activeJob == nullptr)
        return -1;

    return getJobRemainingTime(activeJob);
}

void Capture::setMaximumGuidingDeviaiton(bool enable, double value)
{
    if (guideDeviationCheck->isEnabled())
    {
        guideDeviationCheck->setChecked(enable);
        if (enable)
            guideDeviation->setValue(value);
    }
}

void Capture::setInSequenceFocus(bool enable, double HFR)
{
    if (autofocusCheck->isEnabled())
    {
        autofocusCheck->setChecked(enable);
        if (enable)
            HFRPixels->setValue(HFR);
    }
}

void Capture::setTemperature()
{
    if (currentCCD)
        currentCCD->setTemperature(temperatureIN->value());
}

void Capture::clearSequenceQueue()
{
    activeJob = nullptr;
    targetName.clear();
    stop();
    while (queueTable->rowCount() > 0)
        queueTable->removeRow(0);
    jobs.clear();
    qDeleteAll(jobs);
    ignoreJobProgress = true;
}

QString Capture::getSequenceQueueStatus()
{
    if (jobs.count() == 0)
        return "Invalid";

    if (isBusy)
        return "Running";

    int idle = 0, error = 0, complete = 0, aborted = 0, running = 0;

    foreach (SequenceJob *job, jobs)
    {
        switch (job->getStatus())
        {
            case SequenceJob::JOB_ABORTED:
                aborted++;
                break;
            case SequenceJob::JOB_BUSY:
                running++;
                break;
            case SequenceJob::JOB_DONE:
                complete++;
                break;
            case SequenceJob::JOB_ERROR:
                error++;
                break;
            case SequenceJob::JOB_IDLE:
                idle++;
                break;
        }
    }

    if (error > 0)
        return "Error";

    if (aborted > 0)
    {
        if (guideState >= GUIDE_GUIDING && deviationDetected)
            return "Suspended";
        else
            return "Aborted";
    }

    if (running > 0)
        return "Running";

    if (idle == jobs.count())
        return "Idle";

    if (complete == jobs.count())
        return "Complete";

    return "Invalid";
}

void Capture::processTelescopeNumber(INumberVectorProperty *nvp)
{
    // If it is not ours, return.
    if (strcmp(nvp->device, currentTelescope->getDeviceName()) || strcmp(nvp->name, "EQUATORIAL_EOD_COORD"))
        return;

    switch (meridianFlipStage)
    {
        case MF_NONE:
            break;
        case MF_INITIATED:
        {
            if (nvp->s == IPS_BUSY)
                meridianFlipStage = MF_FLIPPING;
        }
        break;

        case MF_FLIPPING:
        {
            double ra, dec;
            currentTelescope->getEqCoords(&ra, &dec);
            double diffRA = initialRA - ra;
            // If the mount is actually flipping then we should see a difference in RA
            // which if it exceeded MF_RA_DIFF_LIMIT (4 hours) then we consider it to be
            // undertaking the flip. Otherwise, it's not flipping and let timeout takes care of
            // of that
            // Are there any mounts that do NOT change RA while flipping? i.e. do it silently?
            // Need to investigate that bit
            if (fabs(diffRA) > MF_RA_DIFF_LIMIT /* || nvp->s == IPS_OK*/)
                meridianFlipStage = MF_SLEWING;
        }
        break;

        case MF_SLEWING:

            if (nvp->s != IPS_OK)
                break;

            // If dome is syncing, wait until it stops
            if (dome && dome->isMoving())
                break;

            // We are at a new initialHA
            initialHA = getCurrentHA();

            appendLogText(i18n("Telescope completed the meridian flip."));

            KNotification::event(QLatin1String("MeridianFlipCompleted"),
                                 i18n("Meridian flip is successfully completed"));

            if (resumeAlignmentAfterFlip == true)
            {
                appendLogText(i18n("Performing post flip re-alignment..."));
                secondsLabel->setText(i18n("Aligning..."));

                retries = 0;
                state   = CAPTURE_ALIGNING;
                emit newStatus(Ekos::CAPTURE_ALIGNING);

                meridianFlipStage = MF_ALIGNING;
                //QTimer::singleShot(Options::settlingTime(), [this]() {emit meridialFlipTracked();});
                //emit meridialFlipTracked();
                return;
            }

            retries = 0;
            checkGuidingAfterFlip();
            break;

        default:
            break;
    }
}

void Capture::checkGuidingAfterFlip()
{
    // If we're not autoguiding then we're done
    if (resumeGuidingAfterFlip == false)
    {
        resumeSequence();
        // N.B. Set meridian flip stage AFTER resumeSequence() always
        meridianFlipStage = MF_NONE;
    }
    else
    {
        appendLogText(i18n("Performing post flip re-calibration and guiding..."));
        secondsLabel->setText(i18n("Calibrating..."));

        state = CAPTURE_CALIBRATING;
        emit newStatus(Ekos::CAPTURE_CALIBRATING);

        meridianFlipStage = MF_GUIDING;
        emit meridianFlipCompleted();
    }
}

double Capture::getCurrentHA()
{
    double currentRA, currentDEC;

    if (currentTelescope == nullptr)
        return INVALID_VALUE;

    if (currentTelescope->getEqCoords(&currentRA, &currentDEC) == false)
    {
        appendLogText(i18n("Failed to retrieve telescope coordinates. Unable to calculate telescope's hour angle."));
        return INVALID_VALUE;
    }

    dms lst = KStarsData::Instance()->geo()->GSTtoLST(KStarsData::Instance()->clock()->utc().gst());

    dms ha(lst.Degrees() - currentRA * 15.0);

    double HA = ha.Hours();

    if (HA > 12)
        HA -= 24;

    return HA;
}

bool Capture::checkMeridianFlip()
{
    if (meridianCheck->isEnabled() == false || meridianCheck->isChecked() == false || initialHA > 0)
        return false;

    double currentHA = getCurrentHA();

    //appendLogText(i18n("Current hour angle %1", currentHA));

    if (currentHA == INVALID_VALUE)
        return false;

    if (currentHA > meridianHours->value())
    {
        //NOTE: DO NOT make the follow sentence PLURAL as the value is in double
        appendLogText(i18n(
            "Current hour angle %1 hours exceeds meridian flip limit of %2 hours. Auto meridian flip is initiated.",
            QString::number(currentHA, 'f', 2), meridianHours->value()));
        meridianFlipStage = MF_INITIATED;

        KNotification::event(QLatin1String("MeridianFlipStarted"), i18n("Meridian flip started"));

        // Suspend guiding first before commanding a meridian flip
        //if (isAutoGuiding && currentCCD->getChip(ISD::CCDChip::GUIDE_CCD) == guideChip)
        //            emit suspendGuiding(false);

        // If we are autoguiding, we should resume autoguiding after flip
        resumeGuidingAfterFlip = (guideState == GUIDE_GUIDING);

        if ((guideState == GUIDE_GUIDING) || isAutoFocus)
            emit meridianFlipStarted();

        double dec;
        currentTelescope->getEqCoords(&initialRA, &dec);
        currentTelescope->Slew(initialRA, dec);
        secondsLabel->setText(i18n("Meridian Flip..."));

        retries = 0;

        state = CAPTURE_MERIDIAN_FLIP;
        emit newStatus(Ekos::CAPTURE_MERIDIAN_FLIP);

        QTimer::singleShot(MF_TIMER_TIMEOUT, this, SLOT(checkMeridianFlipTimeout()));
        return true;
    }

    return false;
}

void Capture::checkMeridianFlipTimeout()
{
    if (meridianFlipStage == MF_NONE)
        return;

    if (meridianFlipStage < MF_ALIGNING)
    {
        appendLogText(i18n("Telescope meridian flip timed out. Please make sure your mount supports meridian flip."));

        if (++retries == 3)
        {
            KNotification::event(QLatin1String("MeridianFlipFailed"), i18n("Meridian flip failed"));
            abort();
        }
        else
        {
            double dec;
            currentTelescope->getEqCoords(&initialRA, &dec);
            currentTelescope->Slew(initialRA, dec);
            appendLogText(i18n("Retrying meridian flip again..."));
        }
    }
}

void Capture::checkGuideDeviationTimeout()
{
    if (activeJob && activeJob->getStatus() == SequenceJob::JOB_ABORTED && deviationDetected)
    {
        appendLogText(i18n("Guide module timed out."));
        deviationDetected = false;
    }
}

void Capture::setAlignStatus(AlignState state)
{
    alignState = state;

    resumeAlignmentAfterFlip = true;

    switch (state)
    {
        case ALIGN_COMPLETE:
            if (meridianFlipStage == MF_ALIGNING)
            {
                appendLogText(i18n("Post flip re-alignment completed successfully."));
                retries = 0;
                checkGuidingAfterFlip();
            }
            break;

        case ALIGN_FAILED:
            // TODO run it 3 times before giving up
            if (meridianFlipStage == MF_ALIGNING)
            {
                if (++retries == 3)
                {
                    appendLogText(i18n("Post-flip alignment failed."));
                    abort();
                }
                else
                {
                    appendLogText(i18n("Post-flip alignment failed. Retrying..."));
                    secondsLabel->setText(i18n("Aligning..."));

                    this->state = CAPTURE_ALIGNING;
                    emit newStatus(Ekos::CAPTURE_ALIGNING);

                    meridianFlipStage = MF_ALIGNING;
                }
            }
            break;

        default:
            break;
    }
}

void Capture::setGuideStatus(GuideState state)
{
    switch (state)
    {
        case GUIDE_IDLE:
        case GUIDE_ABORTED:
            // If Autoguiding was started before and now stopped, let's abort (unless we're doing a meridian flip)
            if (guideState == GUIDE_GUIDING && meridianFlipStage == MF_NONE && activeJob &&
                activeJob->getStatus() == SequenceJob::JOB_BUSY)
            {
                appendLogText(i18n("Autoguiding stopped. Aborting..."));
                abort();
            }
            //isAutoGuiding = false;
            break;

        case GUIDE_GUIDING:
            break;
        //isAutoGuiding = true;

        case GUIDE_CALIBRATION_SUCESS:
            guideDeviationCheck->setEnabled(true);
            guideDeviation->setEnabled(true);
            break;

        case GUIDE_CALIBRATION_ERROR:
            // TODO try restarting calibration a couple of times before giving up
            if (meridianFlipStage == MF_GUIDING)
            {
                if (++retries == 3)
                {
                    appendLogText(i18n("Post meridian flip calibration error. Aborting..."));
                    abort();
                }
                else
                {
                    appendLogText(i18n("Post meridian flip calibration error. Restarting..."));
                    checkGuidingAfterFlip();
                }
            }
            break;

        case GUIDE_DITHERING_SUCCESS:
            resumeCapture();
            break;

        case GUIDE_DITHERING_ERROR:
            appendLogText(i18n("Warning: Dithering failed. Resuming capture..."));
            resumeCapture();
            //abort();
            break;

        default:
            break;
    }

    guideState = state;
}

void Capture::checkFrameType(int index)
{
    if (index == FRAME_LIGHT)
        calibrationB->setEnabled(false);
    else
        calibrationB->setEnabled(true);
}

double Capture::setCurrentADU(double value)
{
    double nextExposure = 0;
    double targetADU    = activeJob->getTargetADU();
    std::vector<double> coeff;

    ExpRaw.append(activeJob->getExposure());
    ADURaw.append(value);

    if (Options::captureLogging())
        qDebug() << "Capture: Current ADU = " << value << " targetADU = " << targetADU
                 << " Exposure Count: " << ExpRaw.count();

    // Most CCDs are quite linear so 1st degree polynomial is quite sufficient
    // But DSLRs can exhibit non-linear response curve and so a 2nd degree polynomial is more appropiate
    if (ExpRaw.count() >= 2)
    {
        double chisq = 0;

        if (ExpRaw.count() >= 4)
        {
            coeff = gsl_polynomial_fit(ADURaw.data(), ExpRaw.data(), ExpRaw.count(), 2, chisq);
            if (Options::captureLogging())
            {
                qDebug() << "Capture: Running polynomial fitting. Found " << coeff.size() << " coefficients.";
                for (size_t i = 0; i < coeff.size(); i++)
                    qDebug() << "Capture: Coeff #" << i << "=" << coeff[i];
            }
        }

        bool looping = false;
        if (ExpRaw.count() >= 10)
        {
            int size = ExpRaw.count();
            looping  = (ExpRaw[size - 1] == ExpRaw[size - 2]) && (ExpRaw[size - 2] == ExpRaw[size - 3]);
            if (looping)
                qDebug() << "Capture: Detected looping in polynomial results. Falling back to llsqr.";
        }

        // If we get invalid data, let's fall back to llsq
        // Since polyfit can be unreliable at low counts, let's only use it at the 4th exposure
        // if we don't have results already.
        if (looping || ExpRaw.count() < 4 || std::isnan(coeff[0]) || std::isinf(coeff[0]))
        {
            double a = 0, b = 0;
            llsq(ExpRaw, ADURaw, a, b);

            if (Options::captureLogging())
                qDebug() << "Capture: polynomial fitting invalid, faling back to llsq. a=" << a << " b=" << b;

            // If we have valid results, let's calculate next exposure
            if (a != 0)
            {
                nextExposure = (targetADU - b) / a;
                // If we get invalid value, let's just proceed iteratively
                if (nextExposure < 0)
                    nextExposure = 0;
            }
        }
        else if (coeff.size() == 3)
            nextExposure = coeff[0] + (coeff[1] * targetADU) + (coeff[2] * pow(targetADU, 2));
    }

    if (nextExposure == 0)
    {
        if (value < targetADU)
            nextExposure = activeJob->getExposure() * 1.25;
        else
            nextExposure = activeJob->getExposure() * .75;
    }

    if (Options::captureLogging())
        qDebug() << "Capture: next FLAT exposure is " << nextExposure;

    return nextExposure;
}

//  Based on  John Burkardt LLSQ (LGPL)

void Capture::llsq(QVector<double> x, QVector<double> y, double &a, double &b)
{
    double bot;
    int i;
    double top;
    double xbar;
    double ybar;
    int n = x.count();
    //
    //  Special case.
    //
    if (n == 1)
    {
        a = 0.0;
        b = y[0];
        return;
    }
    //
    //  Average X and Y.
    //
    xbar = 0.0;
    ybar = 0.0;
    for (i = 0; i < n; i++)
    {
        xbar = xbar + x[i];
        ybar = ybar + y[i];
    }
    xbar = xbar / (double)n;
    ybar = ybar / (double)n;
    //
    //  Compute Beta.
    //
    top = 0.0;
    bot = 0.0;
    for (i = 0; i < n; i++)
    {
        top = top + (x[i] - xbar) * (y[i] - ybar);
        bot = bot + (x[i] - xbar) * (x[i] - xbar);
    }

    a = top / bot;

    b = ybar - a * xbar;

    return;
}

void Capture::setDirty()
{
    mDirty = true;
}

void Capture::setMeridianFlip(bool enable)
{
    meridianCheck->setChecked(enable);
}

void Capture::setMeridianFlipHour(double hours)
{
    meridianHours->setValue(hours);
}

bool Capture::hasCoolerControl()
{
    if (currentCCD && currentCCD->hasCoolerControl())
        return true;

    return false;
}

bool Capture::setCoolerControl(bool enable)
{
    if (currentCCD && currentCCD->hasCoolerControl())
        return currentCCD->setCoolerControl(enable);

    return false;
}

void Capture::clearAutoFocusHFR()
{
    // If HFR limit was set from file, we cannot overide it.
    if (fileHFR > 0)
        return;

    HFRPixels->setValue(0);
    firstAutoFocus = true;
}

void Capture::openCalibrationDialog()
{
    QDialog calibrationDialog;

    Ui_calibrationOptions calibrationOptions;
    calibrationOptions.setupUi(&calibrationDialog);

    if (currentTelescope)
    {
        calibrationOptions.parkMountC->setEnabled(currentTelescope->canPark());
        calibrationOptions.parkMountC->setChecked(preMountPark);
    }
    else
        calibrationOptions.parkMountC->setEnabled(false);

    if (dome)
    {
        calibrationOptions.parkDomeC->setEnabled(dome->canPark());
        calibrationOptions.parkDomeC->setChecked(preDomePark);
    }
    else
        calibrationOptions.parkDomeC->setEnabled(false);

    //connect(calibrationOptions.wallSourceC, SIGNAL(toggled(bool)), calibrationOptions.parkC, SLOT(setDisabled(bool)));

    switch (flatFieldSource)
    {
        case SOURCE_MANUAL:
            calibrationOptions.manualSourceC->setChecked(true);
            break;

        case SOURCE_FLATCAP:
            calibrationOptions.flatDeviceSourceC->setChecked(true);
            break;

        case SOURCE_DARKCAP:
            calibrationOptions.darkDeviceSourceC->setChecked(true);
            break;

        case SOURCE_WALL:
            calibrationOptions.wallSourceC->setChecked(true);
            calibrationOptions.azBox->setText(wallCoord.az().toDMSString());
            calibrationOptions.altBox->setText(wallCoord.alt().toDMSString());
            break;

        case SOURCE_DAWN_DUSK:
            calibrationOptions.dawnDuskFlatsC->setChecked(true);
            break;
    }

    switch (flatFieldDuration)
    {
        case DURATION_MANUAL:
            calibrationOptions.manualDurationC->setChecked(true);
            break;

        case DURATION_ADU:
            calibrationOptions.ADUC->setChecked(true);
            calibrationOptions.ADUValue->setValue(targetADU);
            calibrationOptions.ADUTolerance->setValue(targetADUTolerance);
            break;
    }

    if (calibrationDialog.exec() == QDialog::Accepted)
    {
        if (calibrationOptions.manualSourceC->isChecked())
            flatFieldSource = SOURCE_MANUAL;
        else if (calibrationOptions.flatDeviceSourceC->isChecked())
            flatFieldSource = SOURCE_FLATCAP;
        else if (calibrationOptions.darkDeviceSourceC->isChecked())
            flatFieldSource = SOURCE_DARKCAP;
        else if (calibrationOptions.wallSourceC->isChecked())
        {
            dms wallAz, wallAlt;
            bool azOk = false, altOk = false;

            wallAz  = calibrationOptions.azBox->createDms(true, &azOk);
            wallAlt = calibrationOptions.altBox->createDms(true, &altOk);

            if (azOk && altOk)
            {
                flatFieldSource = SOURCE_WALL;
                wallCoord.setAz(wallAz);
                wallCoord.setAlt(wallAlt);
            }
            else
            {
                calibrationOptions.manualSourceC->setChecked(true);
                KMessageBox::error(this, i18n("Wall coordinates are invalid."));
            }
        }
        else
            flatFieldSource = SOURCE_DAWN_DUSK;

        if (calibrationOptions.manualDurationC->isChecked())
            flatFieldDuration = DURATION_MANUAL;
        else
        {
            flatFieldDuration  = DURATION_ADU;
            targetADU          = calibrationOptions.ADUValue->value();
            targetADUTolerance = calibrationOptions.ADUTolerance->value();
        }

        preMountPark = calibrationOptions.parkMountC->isChecked();
        preDomePark  = calibrationOptions.parkDomeC->isChecked();

        setDirty();
    }
}

IPState Capture::processPreCaptureCalibrationStage()
{
    if (guideState == GUIDE_GUIDING)
    {
        appendLogText(i18n("Autoguiding suspended."));
        emit suspendGuiding();
    }

    // Let's check what actions to be taken, if any, for the flat field source
    switch (activeJob->getFlatFieldSource())
    {
        case SOURCE_MANUAL:
        case SOURCE_DAWN_DUSK: // Not yet implemented
            break;

        // Park cap, if not parked
        // Turn on Light
        case SOURCE_FLATCAP:
            if (dustCap)
            {
                // If cap is not park, park it
                if (calibrationStage < CAL_DUSTCAP_PARKING && dustCap->isParked() == false)
                {
                    if (dustCap->Park())
                    {
                        calibrationStage = CAL_DUSTCAP_PARKING;
                        appendLogText(i18n("Parking dust cap..."));
                        return IPS_BUSY;
                    }
                    else
                    {
                        appendLogText(i18n("Parking dust cap failed, aborting..."));
                        abort();
                        return IPS_ALERT;
                    }
                }

                // Wait until  cap is parked
                if (calibrationStage == CAL_DUSTCAP_PARKING)
                {
                    if (dustCap->isParked() == false)
                        return IPS_BUSY;
                    else
                    {
                        calibrationStage = CAL_DUSTCAP_PARKED;
                        appendLogText(i18n("Dust cap parked."));
                    }
                }

                // If light is not on, turn it on. For flat frames only
                if (activeJob->getFrameType() == FRAME_FLAT && dustCap->isLightOn() == false)
                {
                    dustCapLightEnabled = true;
                    dustCap->SetLightEnabled(true);
                }
                else if (activeJob->getFrameType() != FRAME_FLAT && dustCap->isLightOn() == true)
                {
                    dustCapLightEnabled = false;
                    dustCap->SetLightEnabled(false);
                }
            }
            break;

        // Park cap, if not parked and not flat frame
        // Unpark cap, if flat frame
        // Turn on Light
        case SOURCE_DARKCAP:
            if (dustCap)
            {
                // If cap is not park, park it if not flat frame. (external lightsource)
                if (calibrationStage < CAL_DUSTCAP_PARKING && dustCap->isParked() == false &&
                    activeJob->getFrameType() != FRAME_FLAT)
                {
                    if (dustCap->Park())
                    {
                        calibrationStage = CAL_DUSTCAP_PARKING;
                        appendLogText(i18n("Parking dust cap..."));
                        return IPS_BUSY;
                    }
                    else
                    {
                        appendLogText(i18n("Parking dust cap failed, aborting..."));
                        abort();
                        return IPS_ALERT;
                    }
                }

                // Wait until  cap is parked
                if (calibrationStage == CAL_DUSTCAP_PARKING)
                {
                    if (dustCap->isParked() == false)
                        return IPS_BUSY;
                    else
                    {
                        calibrationStage = CAL_DUSTCAP_PARKED;
                        appendLogText(i18n("Dust cap parked."));
                    }
                }

                // If cap is parked, unpark it if flat frame. (external lightsource)
                if (calibrationStage < CAL_DUSTCAP_UNPARKING && dustCap->isParked() == true &&
                    activeJob->getFrameType() == FRAME_FLAT)
                {
                    if (dustCap->UnPark())
                    {
                        calibrationStage = CAL_DUSTCAP_UNPARKING;
                        appendLogText(i18n("UnParking dust cap..."));
                        return IPS_BUSY;
                    }
                    else
                    {
                        appendLogText(i18n("UnParking dust cap failed, aborting..."));
                        abort();
                        return IPS_ALERT;
                    }
                }

                // Wait until  cap is parked
                if (calibrationStage == CAL_DUSTCAP_UNPARKING)
                {
                    if (dustCap->isParked() == true)
                        return IPS_BUSY;
                    else
                    {
                        calibrationStage = CAL_DUSTCAP_UNPARKED;
                        appendLogText(i18n("Dust cap unparked."));
                    }
                }

                // If light is not on, turn it on. For flat frames only
                if (activeJob->getFrameType() == FRAME_FLAT && dustCap->isLightOn() == false)
                {
                    dustCapLightEnabled = true;
                    dustCap->SetLightEnabled(true);
                }
                else if (activeJob->getFrameType() != FRAME_FLAT && dustCap->isLightOn() == true)
                {
                    dustCapLightEnabled = false;
                    dustCap->SetLightEnabled(false);
                }
            }
            break;

        // Go to wall coordinates
        case SOURCE_WALL:
            if (currentTelescope)
            {
                if (calibrationStage < CAL_SLEWING)
                {
                    wallCoord = activeJob->getWallCoord();
                    wallCoord.HorizontalToEquatorial(KStarsData::Instance()->lst(),
                                                     KStarsData::Instance()->geo()->lat());
                    currentTelescope->Slew(&wallCoord);
                    appendLogText(i18n("Mount slewing to wall position..."));
                    calibrationStage = CAL_SLEWING;
                    return IPS_BUSY;
                }

                // Check if slewing is complete
                if (calibrationStage == CAL_SLEWING)
                {
                    if (currentTelescope->isSlewing() == false)
                    {
                        calibrationStage = CAL_SLEWING_COMPLETE;
                        appendLogText(i18n("Slew to wall position complete."));
                    }
                    else
                        return IPS_BUSY;
                }

                if (lightBox)
                {
                    // Check if we have a light box to turn on
                    if (activeJob->getFrameType() == FRAME_FLAT && lightBox->isLightOn() == false)
                    {
                        lightBoxLightEnabled = true;
                        lightBox->SetLightEnabled(true);
                    }
                    else if (activeJob->getFrameType() != FRAME_FLAT && lightBox->isLightOn() == true)
                    {
                        lightBoxLightEnabled = false;
                        lightBox->SetLightEnabled(false);
                    }
                }
            }
            break;
    }

    // Check if we need to perform mount prepark
    if (preMountPark && currentTelescope && activeJob->getFlatFieldSource() != SOURCE_WALL)
    {
        if (calibrationStage < CAL_MOUNT_PARKING && currentTelescope->isParked() == false)
        {
            if (currentTelescope->Park())
            {
                calibrationStage = CAL_MOUNT_PARKING;
                //emit mountParking();
                appendLogText(i18n("Parking mount prior to calibration frames capture..."));
                return IPS_BUSY;
            }
            else
            {
                appendLogText(i18n("Parking mount failed, aborting..."));
                abort();
                return IPS_ALERT;
            }
        }

        if (calibrationStage == CAL_MOUNT_PARKING)
        {
            // If not parked yet, check again in 1 second
            // Otherwise proceed to the rest of the algorithm
            if (currentTelescope->isParked() == false)
                return IPS_BUSY;
            else
            {
                calibrationStage = CAL_MOUNT_PARKED;
                appendLogText(i18n("Mount parked."));
            }
        }
    }

    // Check if we need to perform dome prepark
    if (preDomePark && dome)
    {
        if (calibrationStage < CAL_DOME_PARKING && dome->isParked() == false)
        {
            if (dome->Park())
            {
                calibrationStage = CAL_DOME_PARKING;
                //emit mountParking();
                appendLogText(i18n("Parking dome..."));
                return IPS_BUSY;
            }
            else
            {
                appendLogText(i18n("Parking dome failed, aborting..."));
                abort();
                return IPS_ALERT;
            }
        }

        if (calibrationStage == CAL_DOME_PARKING)
        {
            // If not parked yet, check again in 1 second
            // Otherwise proceed to the rest of the algorithm
            if (dome->isParked() == false)
                return IPS_BUSY;
            else
            {
                calibrationStage = CAL_DOME_PARKED;
                appendLogText(i18n("Dome parked."));
            }
        }
    }

    calibrationStage = CAL_PRECAPTURE_COMPLETE;

    return IPS_OK;
}

bool Capture::processPostCaptureCalibrationStage()
{
    // If there are no more images to capture, do not bother calculating next exposure
    if (calibrationStage == CAL_CALIBRATION_COMPLETE && (seqCurrentCount + 1) >= seqTotalCount)
    {
        return true;
    }

    // Check if we need to do flat field slope calculation if the user specified a desired ADU value
    if (activeJob->getFrameType() == FRAME_FLAT && activeJob->getFlatFieldDuration() == DURATION_ADU &&
        activeJob->getTargetADU() > 0)
    {
        FITSData *image_data   = nullptr;
        FITSView *currentImage = targetChip->getImageView(FITS_NORMAL);

        if (currentImage)
        {
            image_data        = currentImage->getImageData();
            double currentADU = image_data->getADU();
            //double currentSlope = ADUSlope;

            double ADUDiff = fabs(currentADU - activeJob->getTargetADU());

            // If it is within tolerance range of target ADU
            if (ADUDiff <= targetADUTolerance)
            {
                if (calibrationStage == CAL_CALIBRATION)
                {
                    appendLogText(
                        i18n("Current ADU %1 within target ADU tolerance range.", QString::number(currentADU, 'f', 0)));
                    activeJob->setPreview(false);
                    currentCCD->setUploadMode(rememberUploadMode);

                    // Get raw prefix
                    exposureIN->setValue(activeJob->getExposure());
                    QString imagePrefix = activeJob->getRawPrefix();
                    constructPrefix(imagePrefix);
                    activeJob->setFullPrefix(imagePrefix);
                    seqPrefix = imagePrefix;
                    currentCCD->setSeqPrefix(imagePrefix);

                    QString finalRemoteDir = activeJob->getFITSDir();
                    finalRemoteDir.replace(activeJob->getRootFITSDir(), activeJob->getRemoteDir()).replace("//", "/");
                    currentCCD->updateUploadSettings(finalRemoteDir);

                    calibrationStage = CAL_CALIBRATION_COMPLETE;
                    startNextExposure();
                    return false;
                }

                return true;
            }

            double nextExposure = -1;

            // If value is saturdated, try to reduce it to valid range first
            if (image_data->getMax(0) == image_data->getMin(0))
                nextExposure = activeJob->getExposure() * 0.5;
            else
                nextExposure = setCurrentADU(currentADU);

            if (nextExposure <= 0)
            {
                appendLogText(
                    i18n("Unable to calculate optimal exposure settings, please capture the flats manually."));
                //activeJob->setTargetADU(0);
                //targetADU = 0;
                abort();
                return false;
            }

            appendLogText(i18n("Current ADU is %1 Next exposure is %2 seconds.", QString::number(currentADU, 'f', 0),
                               QString::number(nextExposure, 'f', 3)));

            calibrationStage = CAL_CALIBRATION;
            activeJob->setExposure(nextExposure);
            activeJob->setPreview(true);
            rememberUploadMode = activeJob->getUploadMode();
            if (currentCCD->getUploadMode() != ISD::CCD::UPLOAD_CLIENT)
            {
                currentCCD->setUploadMode(ISD::CCD::UPLOAD_CLIENT);
            }

            startNextExposure();
            return false;

            // Start next exposure in case ADU Slope is not calculated yet
            /*if (currentSlope == 0)
            {
                startNextExposure();
                return;
            }*/
        }
        else
        {
            appendLogText(i18n("An empty image is received, aborting..."));
            abort();
            return false;
        }
    }

    calibrationStage = CAL_CALIBRATION_COMPLETE;
    return true;
}

void Capture::setNewRemoteFile(QString file)
{
    appendLogText(i18n("Remote image saved to %1", file));
}

void Capture::startPostFilterAutoFocus()
{
    if (focusState >= FOCUS_PROGRESS)
        return;

    secondsLabel->setText(i18n("Focusing..."));

    state = CAPTURE_FOCUSING;
    emit newStatus(Ekos::CAPTURE_FOCUSING);

    appendLogText(i18n("Post filter change Autofocus..."));

    // Force it to always run autofocus routine
    emit checkFocus(0.1);
}

void Capture::postScriptFinished(int exitCode)
{
    appendLogText(i18n("Post capture script finished with code %1. Resuming sequence...", exitCode));
    resumeSequence();
}

void Capture::loadFilterOffsets()
{
    // Get all OAL equipment filter list
    KStarsData::Instance()->userdb()->GetAllFilters(m_filterList);
    filterFocusOffsets.clear();

    for (int i = 0; i < FilterPosCombo->count(); i++)
    {
        FocusOffset *oneOffset = new FocusOffset;
        oneOffset->filter      = FilterPosCombo->itemText(i);
        oneOffset->offset      = 0;

        // Find matching filter if any and loads its offset
        foreach (OAL::Filter *o, m_filterList)
        {
            if (o->vendor() == FilterCaptureCombo->currentText() && o->color() == oneOffset->filter)
            {
                oneOffset->offset = o->offset().toInt();
                break;
            }
        }

        filterFocusOffsets.append(oneOffset);
    }
}

void Capture::showFilterOffsetDialog()
{
    loadFilterOffsets();

    QDialog filterOffsetDialog;

    filterOffsetDialog.setWindowTitle(i18n("Filter Focus Offsets"));

    QDialogButtonBox *buttonBox =
        new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, &filterOffsetDialog);

    connect(buttonBox, SIGNAL(accepted()), &filterOffsetDialog, SLOT(accept()));
    connect(buttonBox, SIGNAL(rejected()), &filterOffsetDialog, SLOT(reject()));

    QVBoxLayout *mainLayout = new QVBoxLayout(&filterOffsetDialog);
    QGridLayout *grid       = new QGridLayout(&filterOffsetDialog);
    QHBoxLayout *tipLayout  = new QHBoxLayout(&filterOffsetDialog);

    QLabel *tipIcon = new QLabel(&filterOffsetDialog);
    QLabel *tipText = new QLabel(&filterOffsetDialog);

    tipIcon->setPixmap(
        QIcon::fromTheme("kstars_flag", QIcon(":/icons/breeze/default/kstars_flag.svg")).pixmap(QSize(32, 32)));
    tipIcon->setFixedSize(32, 32);

    tipText->setText(i18n("Set <em>relative</em> filter focus offset in steps."));

    tipLayout->addWidget(tipIcon);
    tipLayout->addWidget(tipText);

    mainLayout->addLayout(grid);
    mainLayout->addLayout(tipLayout);
    mainLayout->addWidget(buttonBox);

    //filterOffsetDialog.setLayout(mainLayout);

    for (int i = 0; i < filterFocusOffsets.count(); i++)
    {
        FocusOffset *oneOffset = filterFocusOffsets.at(i);

        QLabel *label  = new QLabel(oneOffset->filter, &filterOffsetDialog);
        QSpinBox *spin = new QSpinBox(&filterOffsetDialog);
        spin->setMinimum(-1000);
        spin->setMaximum(1000);
        spin->setValue(oneOffset->offset);

        grid->addWidget(label, i, 0);
        grid->addWidget(spin, i, 1);
    }

    if (filterOffsetDialog.exec() == QDialog::Accepted)
    {
        for (int i = 0; i < filterFocusOffsets.count(); i++)
        {
            FocusOffset *oneOffset = filterFocusOffsets.at(i);
            oneOffset->offset      = static_cast<QSpinBox *>(grid->itemAtPosition(i, 1)->widget())->value();

            // Find matching filter if any and save its offset
            OAL::Filter *matchedFilter = nullptr;

            foreach (OAL::Filter *o, m_filterList)
            {
                if (o->vendor() == FilterCaptureCombo->currentText() && o->color() == oneOffset->filter)
                {
                    o->setOffset(QString::number(oneOffset->offset));
                    matchedFilter = o;
                    break;
                }
            }

            // If no filter exists, let's create one
            if (matchedFilter == nullptr)
            {
                KStarsData::Instance()->userdb()->AddFilter(FilterCaptureCombo->currentText(), "", "",
                                                            QString::number(oneOffset->offset), oneOffset->filter, "1");
            }
            // Or update Existing one
            else
            {
                KStarsData::Instance()->userdb()->AddFilter(FilterCaptureCombo->currentText(), "", "",
                                                            QString::number(oneOffset->offset), oneOffset->filter,
                                                            matchedFilter->exposure(), matchedFilter->id());
            }
        }
    }
}

void Capture::toggleVideoStream(bool enable)
{
    if (currentCCD == nullptr)
        return;

    currentCCD->setVideoStreamEnabled(enable);
}

void Capture::setVideoStreamEnabled(bool enabled)
{
    if (enabled)
    {
        liveVideoB->setChecked(true);
        liveVideoB->setStyleSheet("color:red;");
    }
    else
    {
        liveVideoB->setChecked(false);
        liveVideoB->setStyleSheet(QString());
    }
}

void Capture::setMountStatus(ISD::Telescope::TelescopeStatus newState)
{
    switch (newState)
    {
        case ISD::Telescope::MOUNT_PARKING:
        case ISD::Telescope::MOUNT_SLEWING:
        case ISD::Telescope::MOUNT_MOVING:
            previewB->setEnabled(false);
            liveVideoB->setEnabled(false);
            // Only disable when button is "Start", and not "Stopped"
            // If mount is in motion, Stopped button should always be enabled to terminate
            // the sequence
            if (pi->isAnimated() == false)
                startB->setEnabled(false);
            break;

        default:
            if (pi->isAnimated() == false)
            {
                previewB->setEnabled(true);
                if (currentCCD)
                    liveVideoB->setEnabled(currentCCD->hasVideoStream());
                startB->setEnabled(true);
            }

            break;
    }
}

void Capture::showObserverDialog()
{
    QList<OAL::Observer *> m_observerList;
    KStars::Instance()->data()->userdb()->GetAllObservers(m_observerList);
    QStringList observers;
    foreach (OAL::Observer *o, m_observerList)
        observers << QString("%1 %2").arg(o->name()).arg(o->surname());

    QDialog observersDialog(this);
    observersDialog.setWindowTitle(i18n("Select Current Observer"));

    QLabel label(i18n("Current Observer:"));

    QComboBox observerCombo(&observersDialog);
    observerCombo.addItems(observers);
    observerCombo.setCurrentText(observerName);
    observerCombo.setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    QPushButton manageObserver(&observersDialog);
    manageObserver.setFixedSize(QSize(32, 32));
    manageObserver.setIcon(QIcon::fromTheme("document-edit", QIcon(":/icons/breeze/default/document-edit.svg")));
    manageObserver.setAttribute(Qt::WA_LayoutUsesWidgetRect);
    manageObserver.setToolTip(i18n("Manage Observers"));
    connect(&manageObserver, &QPushButton::clicked, this, [&]() {
        ObserverAdd add;
        add.exec();

        QList<OAL::Observer *> m_observerList;
        KStars::Instance()->data()->userdb()->GetAllObservers(m_observerList);
        QStringList observers;
        foreach (OAL::Observer *o, m_observerList)
            observers << QString("%1 %2").arg(o->name()).arg(o->surname());

        observerCombo.clear();
        observerCombo.addItems(observers);
        observerCombo.setCurrentText(observerName);

    });

    QHBoxLayout *layout = new QHBoxLayout;
    layout->addWidget(&label);
    layout->addWidget(&observerCombo);
    layout->addWidget(&manageObserver);

    observersDialog.setLayout(layout);

    observersDialog.exec();

    observerName = observerCombo.currentText();

    Options::setDefaultObserver(observerName);
}


void Capture::startRefocusEveryNTimer()
{
    refocusEveryNTimer.restart();
}

void Capture::restartRefocusEveryNTimer()
{
    refocusEveryNTimer.restart();
}

int Capture::getRefocusEveryNTimerElapsedSec()
{
    return refocusEveryNTimer.elapsed()/1000;
}
}
