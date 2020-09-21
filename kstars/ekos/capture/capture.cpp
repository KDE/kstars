/*  Ekos
    Copyright (C) 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include "capture.h"

#include "captureadaptor.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "Options.h"
#include "rotatorsettings.h"
#include "sequencejob.h"
#include "skymap.h"
#include "ui_calibrationoptions.h"
#include "auxiliary/QProgressIndicator.h"
#include "auxiliary/ksmessagebox.h"
#include "ekos/manager.h"
#include "ekos/auxiliary/darklibrary.h"
#include "fitsviewer/fitsdata.h"
#include "fitsviewer/fitsview.h"
#include "indi/driverinfo.h"
#include "indi/indifilter.h"
#include "indi/clientmanager.h"
#include "oal/observeradd.h"

#include <basedevice.h>

#include <ekos_capture_debug.h>

#define MF_TIMER_TIMEOUT    90000
#define GD_TIMER_TIMEOUT    60000
#define MF_RA_DIFF_LIMIT    4

// Wait 3-minutes as maximum beyond exposure
// value.
#define CAPTURE_TIMEOUT_THRESHOLD  180000

// Current Sequence File Format:
#define SQ_FORMAT_VERSION 2.0
// We accept file formats with version back to:
#define SQ_COMPAT_VERSION 2.0

namespace Ekos
{
Capture::Capture()
{
    setupUi(this);

    qRegisterMetaType<Ekos::CaptureState>("Ekos::CaptureState");
    qDBusRegisterMetaType<Ekos::CaptureState>();

    new CaptureAdaptor(this);
    QDBusConnection::sessionBus().registerObject("/KStars/Ekos/Capture", this);
    QPointer<QDBusInterface> ekosInterface = new QDBusInterface("org.kde.kstars", "/KStars/Ekos", "org.kde.kstars.Ekos",
            QDBusConnection::sessionBus(), this);

    // Connecting DBus signals
    QDBusConnection::sessionBus().connect("org.kde.kstars", "/KStars/Ekos", "org.kde.kstars.Ekos", "newModule", this,
                                          SLOT(registerNewModule(QString)));

    // ensure that the mount interface is present
    registerNewModule("Mount");

    KStarsData::Instance()->userdb()->GetAllDSLRInfos(DSLRInfos);

    if (DSLRInfos.count() > 0)
    {
        qCDebug(KSTARS_EKOS_CAPTURE) << "DSLR Cameras Info:";
        qCDebug(KSTARS_EKOS_CAPTURE) << DSLRInfos;
    }

    dirPath = QUrl::fromLocalFile(QDir::homePath());

    //isAutoGuiding   = false;

    rotatorSettings.reset(new RotatorSettings(this));

    pi = new QProgressIndicator(this);

    progressLayout->addWidget(pi, 0, 4, 1, 1);

    seqFileCount = 0;
    //seqWatcher		= new KDirWatch();
    seqTimer = new QTimer(this);
    connect(seqTimer, &QTimer::timeout, this, &Ekos::Capture::captureImage);

    connect(startB, &QPushButton::clicked, this, &Ekos::Capture::toggleSequence);
    connect(pauseB, &QPushButton::clicked, this, &Ekos::Capture::pause);

    startB->setIcon(QIcon::fromTheme("media-playback-start"));
    startB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    pauseB->setIcon(QIcon::fromTheme("media-playback-pause"));
    pauseB->setAttribute(Qt::WA_LayoutUsesWidgetRect);

    filterManagerB->setIcon(QIcon::fromTheme("view-filter"));
    filterManagerB->setAttribute(Qt::WA_LayoutUsesWidgetRect);

    filterWheelS->addItem("--");

    connect(captureBinHN, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), captureBinVN, &QSpinBox::setValue);

    connect(cameraS, static_cast<void (QComboBox::*)(const QString &)>(&QComboBox::activated), this,
            &Ekos::Capture::setDefaultCCD);
    connect(cameraS, static_cast<void (QComboBox::*)(int)>(&QComboBox::activated), this, &Ekos::Capture::checkCCD);

    connect(liveVideoB, &QPushButton::clicked, this, &Ekos::Capture::toggleVideo);

    guideDeviationTimer.setInterval(GD_TIMER_TIMEOUT);
    connect(&guideDeviationTimer, &QTimer::timeout, this, &Ekos::Capture::checkGuideDeviationTimeout);

    connect(clearConfigurationB, &QPushButton::clicked, this, &Ekos::Capture::clearCameraConfiguration);

    connect(filterWheelS, static_cast<void (QComboBox::*)(const QString &)>(&QComboBox::activated), this,
            &Ekos::Capture::setDefaultFilterWheel);
    connect(filterWheelS, static_cast<void (QComboBox::*)(int)>(&QComboBox::activated), this,
            &Ekos::Capture::checkFilter);

    connect(restartCameraB, &QPushButton::clicked, [this]()
    {
        restartCamera(cameraS->currentText());
    });

    connect(cameraTemperatureS, &QCheckBox::toggled, [this](bool toggled)
    {
        if (currentCCD)
        {
            QVariantMap auxInfo = currentCCD->getDriverInfo()->getAuxInfo();
            auxInfo[QString("%1_TC").arg(currentCCD->getDeviceName())] = toggled;
            currentCCD->getDriverInfo()->setAuxInfo(auxInfo);
        }
    });

    connect(filterEditB, &QPushButton::clicked, this, &Ekos::Capture::editFilterName);

    connect(captureFilterS, static_cast<void(QComboBox::*)(const QString &)>(&QComboBox::currentIndexChanged),
            [ = ]()
    {
        updateHFRThreshold();
    });
    connect(previewB, &QPushButton::clicked, this, &Ekos::Capture::captureOne);
    connect(loopB, &QPushButton::clicked, this, &Ekos::Capture::startFraming);

    //connect( seqWatcher, SIGNAL(dirty(QString)), this, &Ekos::Capture::checkSeqFile(QString)));

    connect(addToQueueB, &QPushButton::clicked, this, &Ekos::Capture::addJob);
    connect(removeFromQueueB, &QPushButton::clicked, this, &Ekos::Capture::removeJobFromQueue);
    connect(queueUpB, &QPushButton::clicked, this, &Ekos::Capture::moveJobUp);
    connect(queueDownB, &QPushButton::clicked, this, &Ekos::Capture::moveJobDown);
    connect(selectFileDirectoryB, &QPushButton::clicked, this, &Ekos::Capture::saveFITSDirectory);
    connect(queueSaveB, &QPushButton::clicked, this, static_cast<void(Ekos::Capture::*)()>(&Ekos::Capture::saveSequenceQueue));
    connect(queueSaveAsB, &QPushButton::clicked, this, &Ekos::Capture::saveSequenceQueueAs);
    connect(queueLoadB, &QPushButton::clicked, this, static_cast<void(Ekos::Capture::*)()>(&Ekos::Capture::loadSequenceQueue));
    connect(resetB, &QPushButton::clicked, this, &Ekos::Capture::resetJobs);
    connect(queueTable->selectionModel(), &QItemSelectionModel::currentRowChanged, this, &Ekos::Capture::selectedJobChanged);
    connect(queueTable, &QAbstractItemView::doubleClicked, this, &Ekos::Capture::editJob);
    connect(queueTable, &QTableWidget::itemSelectionChanged, this, &Ekos::Capture::resetJobEdit);
    connect(setTemperatureB, &QPushButton::clicked, [&]()
    {
        if (currentCCD)
            currentCCD->setTemperature(cameraTemperatureN->value());
    });
    connect(coolerOnB, &QPushButton::clicked, [&]()
    {
        if (currentCCD)
            currentCCD->setCoolerControl(true);
    });
    connect(coolerOffB, &QPushButton::clicked, [&]()
    {
        if (currentCCD)
            currentCCD->setCoolerControl(false);
    });
    connect(cameraTemperatureN, &QDoubleSpinBox::editingFinished, setTemperatureB,
            static_cast<void (QPushButton::*)()>(&QPushButton::setFocus));
    connect(captureTypeS, static_cast<void (QComboBox::*)(int)>(&QComboBox::activated), this, &Ekos::Capture::checkFrameType);
    connect(resetFrameB, &QPushButton::clicked, this, &Ekos::Capture::resetFrame);
    connect(calibrationB, &QPushButton::clicked, this, &Ekos::Capture::openCalibrationDialog);
    connect(rotatorB, &QPushButton::clicked, rotatorSettings.get(), &Ekos::Capture::show);

    addToQueueB->setIcon(QIcon::fromTheme("list-add"));
    addToQueueB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    removeFromQueueB->setIcon(QIcon::fromTheme("list-remove"));
    removeFromQueueB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    queueUpB->setIcon(QIcon::fromTheme("go-up"));
    queueUpB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    queueDownB->setIcon(QIcon::fromTheme("go-down"));
    queueDownB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    selectFileDirectoryB->setIcon(QIcon::fromTheme("document-open-folder"));
    selectFileDirectoryB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    queueLoadB->setIcon(QIcon::fromTheme("document-open"));
    queueLoadB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    queueSaveB->setIcon(QIcon::fromTheme("document-save"));
    queueSaveB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    queueSaveAsB->setIcon(QIcon::fromTheme("document-save-as"));
    queueSaveAsB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    resetB->setIcon(QIcon::fromTheme("system-reboot"));
    resetB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    resetFrameB->setIcon(QIcon::fromTheme("view-refresh"));
    resetFrameB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    calibrationB->setIcon(QIcon::fromTheme("run-build"));
    calibrationB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    rotatorB->setIcon(QIcon::fromTheme("kstars_solarsystem"));
    rotatorB->setAttribute(Qt::WA_LayoutUsesWidgetRect);

    addToQueueB->setToolTip(i18n("Add job to sequence queue"));
    removeFromQueueB->setToolTip(i18n("Remove job from sequence queue"));

    fileDirectoryT->setText(Options::fitsDir());

    for (auto &filter : FITSViewer::filterTypes)
        filterCombo->addItem(filter);

    ////////////////////////////////////////////////////////////////////////
    /// Settings
    ////////////////////////////////////////////////////////////////////////
    // #1 Guide Deviation Check
    limitGuideDeviationS->setChecked(Options::enforceGuideDeviation());
    connect(limitGuideDeviationS, &QCheckBox::toggled, [ = ](bool checked)
    {
        Options::setEnforceGuideDeviation(checked);
    });

    // #2 Guide Deviation Value
    limitGuideDeviationN->setValue(Options::guideDeviation());
    connect(limitGuideDeviationN, &QDoubleSpinBox::editingFinished, [ = ]()
    {
        Options::setGuideDeviation(limitGuideDeviationN->value());
    });

    // 3. Autofocus HFR Check
    limitFocusHFRS->setChecked(Options::enforceAutofocus());
    connect(limitFocusHFRS, &QCheckBox::toggled, [ = ](bool checked)
    {
        Options::setEnforceAutofocus(checked);
        if (checked == false)
            isInSequenceFocus = false;
    });

    // 4. Autofocus HFR Deviation
    limitFocusHFRN->setValue(Options::hFRDeviation());
    connect(limitFocusHFRN, &QDoubleSpinBox::editingFinished, [ = ]()
    {
        Options::setHFRDeviation(limitFocusHFRN->value());
    });

    // 5. Autofocus temperature Check
    limitFocusDeltaTS->setChecked(Options::enforceAutofocusOnTemperature());
    connect(limitFocusDeltaTS, &QCheckBox::toggled, [ = ](bool checked)
    {
        Options::setEnforceAutofocusOnTemperature(checked);
        if (checked == false)
            isTemperatureDeltaCheckActive = false;
    });

    // 6. Autofocus temperature Delta
    limitFocusDeltaTN->setValue(Options::maxFocusTemperatureDelta());
    connect(limitFocusDeltaTN, &QDoubleSpinBox::editingFinished, [ = ]()
    {
        Options::setMaxFocusTemperatureDelta(limitFocusDeltaTN->value());
    });

    // 7. Refocus Every Check
    limitRefocusS->setChecked(Options::enforceRefocusEveryN());
    connect(limitRefocusS, &QCheckBox::toggled, [ = ](bool checked)
    {
        Options::setEnforceRefocusEveryN(checked);
    });

    // 8. Refocus Every Value
    limitRefocusN->setValue(static_cast<int>(Options::refocusEveryN()));
    connect(limitRefocusN, &QDoubleSpinBox::editingFinished, [ = ]()
    {
        Options::setRefocusEveryN(static_cast<uint>(limitRefocusN->value()));
    });

    // 9. File settings: filter name
    fileFilterS->setChecked(Options::fileSettingsUseFilter());
    connect(fileFilterS, &QCheckBox::toggled, [ = ](bool checked)
    {
        Options::setFileSettingsUseFilter(checked);
    });

    // 10. File settings: duration
    fileDurationS->setChecked(Options::fileSettingsUseDuration());
    connect(fileDurationS, &QCheckBox::toggled, [ = ](bool checked)
    {
        Options::setFileSettingsUseDuration(checked);
    });

    // 11. File settings: timestamp
    fileTimestampS->setChecked(Options::fileSettingsUseTimestamp());
    connect(fileTimestampS, &QCheckBox::toggled, [ = ](bool checked)
    {
        Options::setFileSettingsUseTimestamp(checked);
    });

    QCheckBox * const checkBoxes[] =
    {
        limitGuideDeviationS,
        limitRefocusS,
        limitGuideDeviationS,
    };
    for (const QCheckBox * control : checkBoxes)
        connect(control, &QCheckBox::toggled, this, &Ekos::Capture::setDirty);

    QDoubleSpinBox * const dspinBoxes[]
    {
        limitFocusHFRN,
        limitFocusDeltaTN,
        limitGuideDeviationN,
    };
    for (const QDoubleSpinBox * control : dspinBoxes)
        connect(control, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), this,
                &Ekos::Capture::setDirty);

    connect(fileUploadModeS, static_cast<void (QComboBox::*)(int)>(&QComboBox::activated), this, &Ekos::Capture::setDirty);
    connect(fileRemoteDirT, &QLineEdit::editingFinished, this, &Ekos::Capture::setDirty);

    m_ObserverName = Options::defaultObserver();
    observerB->setIcon(QIcon::fromTheme("im-user"));
    observerB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    connect(observerB, &QPushButton::clicked, this, &Ekos::Capture::showObserverDialog);

    // Exposure Timeout
    captureTimeout.setSingleShot(true);
    connect(&captureTimeout, &QTimer::timeout, this, &Ekos::Capture::processCaptureTimeout);

    // Post capture script
    connect(&postCaptureScript, static_cast<void (QProcess::*)(int exitCode, QProcess::ExitStatus status)>(&QProcess::finished),
            this, &Ekos::Capture::postScriptFinished);

    // Remote directory
    connect(fileUploadModeS, static_cast<void (QComboBox::*)(int)>(&QComboBox::activated), this,
            [&](int index)
    {
        fileRemoteDirT->setEnabled(index != 0);
    });

    customPropertiesDialog.reset(new CustomProperties());
    connect(customValuesB, &QPushButton::clicked, [&]()
    {
        customPropertiesDialog.get()->show();
        customPropertiesDialog.get()->raise();
    });
    connect(customPropertiesDialog.get(), &CustomProperties::valueChanged, [&]()
    {
        const double newGain = getGain();
        if (captureGainN && newGain >= 0)
            captureGainN->setValue(newGain);
        const int newOffset = getOffset();
        if (newOffset >= 0)
            captureOffsetN->setValue(newOffset);
    });

    flatFieldSource = static_cast<FlatFieldSource>(Options::calibrationFlatSourceIndex());
    flatFieldDuration = static_cast<FlatFieldDuration>(Options::calibrationFlatDurationIndex());
    wallCoord.setAz(Options::calibrationWallAz());
    wallCoord.setAlt(Options::calibrationWallAlt());
    targetADU = Options::calibrationADUValue();
    targetADUTolerance = Options::calibrationADUValueTolerance();

    fileDirectoryT->setText(Options::captureDirectory());
    connect(fileDirectoryT, &QLineEdit::textChanged, [&]()
    {
        Options::setCaptureDirectory(fileDirectoryT->text());
    });

    if (Options::remoteCaptureDirectory().isEmpty() == false)
    {
        fileRemoteDirT->setText(Options::remoteCaptureDirectory());
    }
    connect(fileRemoteDirT, &QLineEdit::editingFinished, [&]()
    {
        Options::setRemoteCaptureDirectory(fileRemoteDirT->text());
    });

    // Keep track of TARGET transfer format when changing CCDs (FITS or NATIVE). Actual format is not changed until capture
    connect(
        captureFormatS, static_cast<void (QComboBox::*)(int)>(&QComboBox::activated), this,
        [&](uint index)
    {
        if (currentCCD)
            currentCCD->setTargetTransferFormat(static_cast<ISD::CCD::TransferFormat>(index));
        Options::setCaptureFormatIndex(index);
    });

    // Load FIlter Offets
    //loadFilterOffsets();

    //Note:  This is to prevent a button from being called the default button
    //and then executing when the user hits the enter key such as when on a Text Box
    QList<QPushButton *> qButtons = findChildren<QPushButton *>();
    for (auto &button : qButtons)
        button->setAutoDefault(false);

    //This Timer will update the Exposure time in the capture module to display the estimated download time left
    //It will also update the Exposure time left in the Summary Screen.
    //It fires every 100 ms while images are downloading.
    downloadProgressTimer.setInterval(100);
    connect(&downloadProgressTimer, &QTimer::timeout, this, &Ekos::Capture::setDownloadProgress);

}

Capture::~Capture()
{
    qDeleteAll(jobs);
}

void Capture::setDefaultCCD(QString ccd)
{
    Options::setDefaultCaptureCCD(ccd);
}

void Capture::setDefaultFilterWheel(QString filterWheel)
{
    Options::setDefaultCaptureFilterWheel(filterWheel);
}

void Capture::addCCD(ISD::GDInterface * newCCD)
{
    ISD::CCD * ccd = static_cast<ISD::CCD *>(newCCD);

    if (CCDs.contains(ccd))
        return;

    CCDs.append(ccd);

    cameraS->addItem(ccd->getDeviceName());

    if (Filters.count() > 0)
        syncFilterInfo();

    checkCCD();

    emit settingsUpdated(getPresetSettings());
}

void Capture::addGuideHead(ISD::GDInterface * newCCD)
{
    QString guiderName = newCCD->getDeviceName() + QString(" Guider");

    if (cameraS->findText(guiderName) == -1)
    {
        cameraS->addItem(guiderName);
        CCDs.append(static_cast<ISD::CCD *>(newCCD));
    }
}

void Capture::addFilter(ISD::GDInterface * newFilter)
{
    foreach (ISD::GDInterface * filter, Filters)
    {
        if (filter->getDeviceName() == newFilter->getDeviceName())
            return;
    }

    filterWheelS->addItem(newFilter->getDeviceName());

    Filters.append(static_cast<ISD::Filter *>(newFilter));

    filterManagerB->setEnabled(true);

    int filterWheelIndex = 1;
    if (Options::defaultCaptureFilterWheel().isEmpty() == false)
        filterWheelIndex = filterWheelS->findText(Options::defaultCaptureFilterWheel());

    if (filterWheelIndex < 1)
        filterWheelIndex = 1;

    checkFilter(filterWheelIndex);
    filterWheelS->setCurrentIndex(filterWheelIndex);

    emit settingsUpdated(getPresetSettings());
}

void Capture::pause()
{
    if (m_State != CAPTURE_CAPTURING)
    {
        // Ensure that the pause function is only called during frame capturing
        // Handling it this way is by far easier than trying to enable/disable the pause button
        // Fixme: make pausing possible at all stages. This makes it necessary to separate the pausing states from CaptureState.
        appendLogText(i18n("Pausing only possible while frame capture is running."));
        qCInfo(KSTARS_EKOS_CAPTURE) << "Pause button pressed while not capturing.";
        return;
    }
    pauseFunction = nullptr;
    m_State         = CAPTURE_PAUSE_PLANNED;
    emit newStatus(Ekos::CAPTURE_PAUSE_PLANNED);
    appendLogText(i18n("Sequence shall be paused after current exposure is complete."));
    pauseB->setEnabled(false);

    startB->setIcon(QIcon::fromTheme("media-playback-start"));
    startB->setToolTip(i18n("Resume Sequence"));
}

void Capture::toggleSequence()
{
    if (m_State == CAPTURE_PAUSE_PLANNED || m_State == CAPTURE_PAUSED)
    {
        startB->setIcon(
            QIcon::fromTheme("media-playback-stop"));
        startB->setToolTip(i18n("Stop Sequence"));
        pauseB->setEnabled(true);

        m_State = CAPTURE_CAPTURING;
        emit newStatus(Ekos::CAPTURE_CAPTURING);

        appendLogText(i18n("Sequence resumed."));

        // Call from where ever we have left of when we paused
        if (pauseFunction)
            (this->*pauseFunction)();
    }
    else if (m_State == CAPTURE_IDLE || m_State == CAPTURE_ABORTED || m_State == CAPTURE_COMPLETE)
    {
        start();
    }
    else
    {
        abort();
    }
}

void Capture::registerNewModule(const QString &name)
{
    qCDebug(KSTARS_EKOS_CAPTURE) << "Registering new Module (" << name << ")";
    if (name == "Mount" && mountInterface == nullptr)
    {
        mountInterface = new QDBusInterface("org.kde.kstars", "/KStars/Ekos/Mount",
                                            "org.kde.kstars.Ekos.Mount", QDBusConnection::sessionBus(), this);

    }
}

/**
 * @brief Start the execution of the Capture::SequenceJob list #jobs.
 *
 * Starting the execution of the Capture::SequenceJob list selects the first job
 * from the ist that may be executed and starts to prepare the job (@see prepareJob()).
 *
 * Several factors determine, which of the jobs will be selected:
 * - First, the list is searched to find the first job that is marked as idle or aborted.
 * -  If none is found, it is checked whether ignoring job progress is set. If yes,
 *    all jobs are are reset (@see reset()) and the first one from the list is selected.
 *    If no, the user is asked whether the jobs should be reset. If the user declines,
 *    starting is aborted.
 */
void Capture::start()
{
    if (darkSubCheck->isChecked())
    {
        KSNotification::error(i18n("Auto dark subtract is not supported in batch mode."));
        return;
    }

    // Reset progress option if there is no captured frame map set at the time of start - fixes the end-user setting the option just before starting
    ignoreJobProgress = !capturedFramesMap.count() && Options::alwaysResetSequenceWhenStarting();

    if (queueTable->rowCount() == 0)
    {
        if (addJob() == false)
            return;
    }

    SequenceJob * first_job = nullptr;

    foreach (SequenceJob * job, jobs)
    {
        if (job->getStatus() == SequenceJob::JOB_IDLE || job->getStatus() == SequenceJob::JOB_ABORTED)
        {
            first_job = job;
            break;
        }
    }

    // If there are no idle nor aborted jobs, question is whether to reset and restart
    // Scheduler will start a non-empty new job each time and doesn't use this execution path
    if (first_job == nullptr)
    {
        // If we have at least one job that are in error, bail out, even if ignoring job progress
        foreach (SequenceJob * job, jobs)
        {
            if (job->getStatus() != SequenceJob::JOB_DONE)
            {
                appendLogText(i18n("No pending jobs found. Please add a job to the sequence queue."));
                return;
            }
        }

        // If we only have completed jobs and we don't ignore job progress, ask the end-user what to do
        if (!ignoreJobProgress)
            if(KMessageBox::warningContinueCancel(
                        nullptr,
                        i18n("All jobs are complete. Do you want to reset the status of all jobs and restart capturing?"),
                        i18n("Reset job status"), KStandardGuiItem::cont(), KStandardGuiItem::cancel(),
                        "reset_job_complete_status_warning") != KMessageBox::Continue)
                return;

        // If the end-user accepted to reset, reset all jobs and restart
        foreach (SequenceJob * job, jobs)
            job->resetStatus();

        first_job = jobs.first();
    }
    // If we need to ignore job progress, systematically reset all jobs and restart
    // Scheduler will never ignore job progress and doesn't use this path
    else if (ignoreJobProgress)
    {
        appendLogText(i18n("Warning: option \"Always Reset Sequence When Starting\" is enabled and resets the sequence counts."));
        foreach (SequenceJob * job, jobs)
            job->resetStatus();
    }

    // Refocus timer should not be reset on deviation error
    if (m_DeviationDetected == false && m_State != CAPTURE_SUSPENDED)
    {
        // start timer to measure time until next forced refocus
        startRefocusEveryNTimer();
    }

    // Only reset these counters if we are NOT restarting from deviation errors
    // So when starting a new job or fresh then we reset them.
    if (m_DeviationDetected == false)
    {
        ditherCounter     = Options::ditherFrames();
        inSequenceFocusCounter = Options::inSequenceCheckFrames();
    }

    m_DeviationDetected = false;
    m_SpikeDetected     = false;

    m_State = CAPTURE_PROGRESS;
    emit newStatus(Ekos::CAPTURE_PROGRESS);

    startB->setIcon(QIcon::fromTheme("media-playback-stop"));
    startB->setToolTip(i18n("Stop Sequence"));
    pauseB->setEnabled(true);

    setBusy(true);

    if (limitGuideDeviationS->isChecked() && autoGuideReady == false)
        appendLogText(i18n("Warning: Guide deviation is selected but autoguide process was not started."));
    if (limitFocusHFRS->isChecked() && m_AutoFocusReady == false)
        appendLogText(i18n("Warning: in-sequence focusing is selected but autofocus process was not started."));
    if (limitFocusDeltaTS->isChecked() && m_AutoFocusReady == false)
        appendLogText(i18n("Warning: temperature delta check is selected but autofocus process was not started."));
    prepareJob(first_job);
}

/**
 * @brief Stop, suspend or abort the currently active job.
 * @param targetState
 */
void Capture::stop(CaptureState targetState)
{
    retries         = 0;
    //seqTotalCount   = 0;
    //seqCurrentCount = 0;

    captureTimeout.stop();

    ADURaw.clear();
    ExpRaw.clear();

    if (activeJob)
    {
        if (activeJob->getStatus() == SequenceJob::JOB_BUSY)
        {
            QString stopText;
            switch (targetState)
            {
                case CAPTURE_IDLE:
                    stopText = i18n("CCD capture stopped");
                    break;

                case CAPTURE_SUSPENDED:
                    stopText = i18n("CCD capture suspended");
                    break;

                default:
                    stopText = i18n("CCD capture aborted");
                    break;
            }
            emit captureAborted(activeJob->getExposure());
            KSNotification::event(QLatin1String("CaptureFailed"), stopText);
            appendLogText(stopText);
            activeJob->abort();
            if (activeJob->isPreview() == false)
            {
                int index = jobs.indexOf(activeJob);
                QJsonObject oneSequence = m_SequenceArray[index].toObject();
                oneSequence["Status"] = "Aborted";
                m_SequenceArray.replace(index, oneSequence);
                emit sequenceChanged(m_SequenceArray);
            }
        }

        // In case of batch job
        if (activeJob->isPreview() == false)
        {
            activeJob->disconnect(this);
            activeJob->reset();
        }
        // or preview job in calibration stage
        else if (calibrationStage == CAL_CALIBRATION)
        {
            activeJob->disconnect(this);
            activeJob->reset();
            activeJob->setPreview(false);
            currentCCD->setUploadMode(rememberUploadMode);
        }
        // or regular preview job
        else
        {
            currentCCD->setUploadMode(rememberUploadMode);
            jobs.removeOne(activeJob);
            // Delete preview job
            delete (activeJob);
            activeJob = nullptr;

            emit newStatus(targetState);
        }
    }

    // Only emit a new status if there is an active job or if capturing is suspended.
    // The latter is necessary since suspending clears the active job, but the Capture
    // module keeps the control.
    if (activeJob != nullptr || m_State == CAPTURE_SUSPENDED)
        emit newStatus(targetState);

    calibrationStage = CAL_NONE;
    m_State            = targetState;

    // Turn off any calibration light, IF they were turned on by Capture module
    if (currentDustCap && dustCapLightEnabled)
    {
        dustCapLightEnabled = false;
        currentDustCap->SetLightEnabled(false);
    }
    if (currentLightBox && lightBoxLightEnabled)
    {
        lightBoxLightEnabled = false;
        currentLightBox->SetLightEnabled(false);
    }

    if (meridianFlipStage == MF_NONE || meridianFlipStage >= MF_COMPLETED)
        secondsLabel->clear();
    disconnect(currentCCD, &ISD::CCD::BLOBUpdated, this, &Ekos::Capture::newFITS);
    disconnect(currentCCD, &ISD::CCD::newExposureValue, this,  &Ekos::Capture::setExposureProgress);
    disconnect(currentCCD, &ISD::CCD::previewFITSGenerated, this, &Ekos::Capture::setGeneratedPreviewFITS);
    disconnect(currentCCD, &ISD::CCD::ready, this, &Ekos::Capture::ready);

    currentCCD->setFITSDir(QString());

    // In case of exposure looping, let's abort
    if (currentCCD->isLooping())
        targetChip->abortExposure();

    imgProgress->reset();
    imgProgress->setEnabled(false);

    fullImgCountOUT->setText(QString());
    currentImgCountOUT->setText(QString());
    exposeOUT->setText(QString());
    m_isLooping = false;

    setBusy(false);

    if (m_State == CAPTURE_ABORTED || m_State == CAPTURE_SUSPENDED)
    {
        startB->setIcon(
            QIcon::fromTheme("media-playback-start"));
        startB->setToolTip(i18n("Start Sequence"));
        pauseB->setEnabled(false);
    }

    //foreach (QAbstractButton *button, queueEditButtonGroup->buttons())
    //button->setEnabled(true);

    seqTimer->stop();

    activeJob = nullptr;
    // meridian flip may take place if requested
    setMeridianFlipStage(MF_READY);
}

void Capture::sendNewImage(const QString &filename, ISD::CCDChip * myChip)
{
    if (activeJob && (myChip == nullptr || myChip == targetChip))
    {
        activeJob->setProperty("filename", filename);
        emit newImage(activeJob);
        // We only emit this for client/both images since remote images already send this automatically
        if (currentCCD->getUploadMode() != ISD::CCD::UPLOAD_LOCAL && activeJob->isPreview() == false)
        {
            emit newSequenceImage(filename, m_GeneratedPreviewFITS);
            m_GeneratedPreviewFITS.clear();
        }
    }
}

bool Capture::setCamera(const QString &device)
{
    for (int i = 0; i < cameraS->count(); i++)
        if (device == cameraS->itemText(i))
        {
            cameraS->setCurrentIndex(i);
            checkCCD(i);
            return true;
        }

    return false;
}

QString Capture::camera()
{
    if (currentCCD)
        return currentCCD->getDeviceName();

    return QString();
}

void Capture::checkCCD(int ccdNum)
{
    if (ccdNum == -1)
    {
        ccdNum = cameraS->currentIndex();

        if (ccdNum == -1)
            return;
    }

    if (ccdNum < CCDs.count())
    {
        // Check whether main camera or guide head only
        currentCCD = CCDs.at(ccdNum);

        if (cameraS->itemText(ccdNum).right(6) == QString("Guider"))
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

        // Make sure we have a valid chip and valid base device.
        // Make sure we are not in capture process.
        if (!targetChip || !targetChip->getCCD() || !targetChip->getCCD()->getBaseDevice() ||
                targetChip->isCapturing())
            return;

        for (auto &ccd : CCDs)
        {
            disconnect(ccd, &ISD::CCD::numberUpdated, this, &Ekos::Capture::processCCDNumber);
            disconnect(ccd, &ISD::CCD::newTemperatureValue, this, &Ekos::Capture::updateCCDTemperature);
            disconnect(ccd, &ISD::CCD::coolerToggled, this, &Ekos::Capture::setCoolerToggled);
            disconnect(ccd, &ISD::CCD::newRemoteFile, this, &Ekos::Capture::setNewRemoteFile);
            disconnect(ccd, &ISD::CCD::videoStreamToggled, this, &Ekos::Capture::setVideoStreamEnabled);
            disconnect(ccd, &ISD::CCD::ready, this, &Ekos::Capture::ready);
        }

        if (currentCCD->hasCoolerControl())
        {
            coolerOnB->setEnabled(true);
            coolerOffB->setEnabled(true);
            coolerOnB->setChecked(currentCCD->isCoolerOn());
            coolerOffB->setChecked(!currentCCD->isCoolerOn());
        }
        else
        {
            coolerOnB->setEnabled(false);
            coolerOnB->setChecked(false);
            coolerOffB->setEnabled(false);
            coolerOffB->setChecked(false);
        }


        if (currentCCD->hasCooler())
        {
            cameraTemperatureS->setEnabled(true);
            cameraTemperatureN->setEnabled(true);

            if (currentCCD->getBaseDevice()->getPropertyPermission("CCD_TEMPERATURE") != IP_RO)
            {
                double min, max, step;
                setTemperatureB->setEnabled(true);
                cameraTemperatureN->setReadOnly(false);
                cameraTemperatureS->setEnabled(true);
                currentCCD->getMinMaxStep("CCD_TEMPERATURE", "CCD_TEMPERATURE_VALUE", &min, &max, &step);
                cameraTemperatureN->setMinimum(min);
                cameraTemperatureN->setMaximum(max);
                cameraTemperatureN->setSingleStep(1);
                bool isChecked = currentCCD->getDriverInfo()->getAuxInfo().value(QString("%1_TC").arg(currentCCD->getDeviceName()),
                                 false).toBool();
                cameraTemperatureS->setChecked(isChecked);
            }
            else
            {
                setTemperatureB->setEnabled(false);
                cameraTemperatureN->setReadOnly(true);
                cameraTemperatureS->setEnabled(false);
                cameraTemperatureS->setChecked(false);
            }

            double temperature = 0;
            if (currentCCD->getTemperature(&temperature))
            {
                temperatureOUT->setText(QString("%L1").arg(temperature, 0, 'f', 2));
                if (cameraTemperatureN->cleanText().isEmpty())
                    cameraTemperatureN->setValue(temperature);
            }
        }
        else
        {
            cameraTemperatureS->setEnabled(false);
            cameraTemperatureN->setEnabled(false);
            cameraTemperatureN->clear();
            temperatureOUT->clear();
            setTemperatureB->setEnabled(false);
        }

        updateFrameProperties();

        QStringList frameTypes = targetChip->getFrameTypes();

        captureTypeS->clear();

        if (frameTypes.isEmpty())
            captureTypeS->setEnabled(false);
        else
        {
            captureTypeS->setEnabled(true);
            captureTypeS->addItems(frameTypes);
            captureTypeS->setCurrentIndex(targetChip->getFrameType());
        }

        QStringList isoList = targetChip->getISOList();
        captureISOS->clear();

        captureFormatS->blockSignals(true);
        captureFormatS->clear();

        if (isoList.isEmpty())
        {
            // Only one transfer format
            captureFormatS->addItem(i18n("FITS"));
            captureISOS->setEnabled(false);
        }
        else
        {
            captureISOS->setEnabled(true);
            captureISOS->addItems(isoList);
            captureISOS->setCurrentIndex(targetChip->getISOIndex());

            // DSLRs have two transfer formats
            captureFormatS->addItem(i18n("FITS"));
            captureFormatS->addItem(i18n("Native"));

            //captureFormatS->setCurrentIndex(currentCCD->getTargetTransferFormat());
            // 2018-05-07 JM: Set value to the value in options
            captureFormatS->setCurrentIndex(static_cast<int>(Options::captureFormatIndex()));

            uint16_t w, h;
            uint8_t bbp {8};
            double pixelX = 0, pixelY = 0;
            bool rc = targetChip->getImageInfo(w, h, pixelX, pixelY, bbp);
            bool isModelInDB = isModelinDSLRInfo(QString(currentCCD->getDeviceName()));
            // If rc == true, then the property has been defined by the driver already
            // Only then we check if the pixels are zero
            if (rc == true && (pixelX == 0.0 || pixelY == 0.0 || isModelInDB == false))
            {
                // If model is already in database, no need to show dialog
                // The zeros above are the initial packets so we can safely ignore them
                if (isModelInDB == false)
                {
                    createDSLRDialog();
                }
                else
                {
                    QString model = QString(currentCCD->getDeviceName());
                    syncDSLRToTargetChip(model);
                }
            }
        }

        captureFormatS->blockSignals(false);

        // Gain Check
        if (currentCCD->hasGain())
        {
            double min, max, step, value, targetCustomGain;
            currentCCD->getGainMinMaxStep(&min, &max, &step);

            // Allow the possibility of no gain value at all.
            GainSpinSpecialValue = min - step;
            captureGainN->setRange(GainSpinSpecialValue, max);
            captureGainN->setSpecialValueText(i18n("--"));
            captureGainN->setEnabled(true);
            captureGainN->setSingleStep(step);
            currentCCD->getGain(&value);

            targetCustomGain = getGain();

            // Set the custom gain if we have one
            // otherwise it will not have an effect.
            if (targetCustomGain > 0)
                captureGainN->setValue(targetCustomGain);
            else
                captureGainN->setValue(GainSpinSpecialValue);

            captureGainN->setReadOnly(currentCCD->getGainPermission() == IP_RO);

            connect(captureGainN, &QDoubleSpinBox::editingFinished, [this]()
            {
                if (captureGainN->value() != GainSpinSpecialValue)
                    setGain(captureGainN->value());
            });
        }
        else
            captureGainN->setEnabled(false);

        // Offset checks
        if (currentCCD->hasOffset())
        {
            double min, max, step, value, targetCustomOffset;
            currentCCD->getOffsetMinMaxStep(&min, &max, &step);

            // Allow the possibility of no Offset value at all.
            OffsetSpinSpecialValue = min - step;
            captureOffsetN->setRange(OffsetSpinSpecialValue, max);
            captureOffsetN->setSpecialValueText(i18n("--"));
            captureOffsetN->setEnabled(true);
            captureOffsetN->setSingleStep(step);
            currentCCD->getOffset(&value);

            targetCustomOffset = getOffset();

            // Set the custom Offset if we have one
            // otherwise it will not have an effect.
            if (targetCustomOffset > 0)
                captureOffsetN->setValue(targetCustomOffset);
            else
                captureOffsetN->setValue(OffsetSpinSpecialValue);

            captureOffsetN->setReadOnly(currentCCD->getOffsetPermission() == IP_RO);

            connect(captureOffsetN, &QDoubleSpinBox::editingFinished, [this]()
            {
                if (captureOffsetN->value() != OffsetSpinSpecialValue)
                    setOffset(captureOffsetN->value());
            });
        }
        else
            captureOffsetN->setEnabled(false);

        customPropertiesDialog->setCCD(currentCCD);

        liveVideoB->setEnabled(currentCCD->hasVideoStream());
        if (currentCCD->hasVideoStream())
            setVideoStreamEnabled(currentCCD->isStreamingEnabled());
        else
            liveVideoB->setIcon(QIcon::fromTheme("camera-off"));

        connect(currentCCD, &ISD::CCD::numberUpdated, this, &Ekos::Capture::processCCDNumber, Qt::UniqueConnection);
        connect(currentCCD, &ISD::CCD::newTemperatureValue, this, &Ekos::Capture::updateCCDTemperature, Qt::UniqueConnection);
        connect(currentCCD, &ISD::CCD::coolerToggled, this, &Ekos::Capture::setCoolerToggled, Qt::UniqueConnection);
        connect(currentCCD, &ISD::CCD::newRemoteFile, this, &Ekos::Capture::setNewRemoteFile);
        connect(currentCCD, &ISD::CCD::videoStreamToggled, this, &Ekos::Capture::setVideoStreamEnabled);
        connect(currentCCD, &ISD::CCD::ready, this, &Ekos::Capture::ready);
    }
}

void Capture::setGuideChip(ISD::CCDChip * chip)
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
    captureFrameXN->setMinimum(0);
    captureFrameXN->setMaximum(0);
    captureFrameXN->setValue(0);

    captureFrameYN->setMinimum(0);
    captureFrameYN->setMaximum(0);
    captureFrameYN->setValue(0);

    captureFrameWN->setMinimum(0);
    captureFrameWN->setMaximum(0);
    captureFrameWN->setValue(0);

    captureFrameHN->setMinimum(0);
    captureFrameHN->setMaximum(0);
    captureFrameHN->setValue(0);
}

void Capture::updateFrameProperties(int reset)
{
    int binx = 1, biny = 1;
    double min, max, step;
    int xstep = 0, ystep = 0;

    QString frameProp    = useGuideHead ? QString("GUIDER_FRAME") : QString("CCD_FRAME");
    QString exposureProp = useGuideHead ? QString("GUIDER_EXPOSURE") : QString("CCD_EXPOSURE");
    QString exposureElem = useGuideHead ? QString("GUIDER_EXPOSURE_VALUE") : QString("CCD_EXPOSURE_VALUE");
    targetChip =
        useGuideHead ? currentCCD->getChip(ISD::CCDChip::GUIDE_CCD) : currentCCD->getChip(ISD::CCDChip::PRIMARY_CCD);

    captureFrameWN->setEnabled(targetChip->canSubframe());
    captureFrameHN->setEnabled(targetChip->canSubframe());
    captureFrameXN->setEnabled(targetChip->canSubframe());
    captureFrameYN->setEnabled(targetChip->canSubframe());

    captureBinHN->setEnabled(targetChip->canBin());
    captureBinVN->setEnabled(targetChip->canBin());

    QList<double> exposureValues;
    exposureValues << 0.01 << 0.02 << 0.05 << 0.1 << 0.2 << 0.25 << 0.5 << 1 << 1.5 << 2 << 2.5 << 3 << 5 << 6 << 7 << 8 << 9 <<
                   10 << 20 << 30 << 40 << 50 << 60 << 120 << 180 << 300 << 600 << 900 << 1200 << 1800;

    if (currentCCD->getMinMaxStep(exposureProp, exposureElem, &min, &max, &step))
    {
        if (min < 0.001)
            captureExposureN->setDecimals(6);
        else
            captureExposureN->setDecimals(3);
        for(int i = 0; i < exposureValues.count(); i++)
        {
            double value = exposureValues.at(i);
            if(value < min || value > max)
            {
                exposureValues.removeAt(i);
                i--; //So we don't skip one
            }
        }

        exposureValues.prepend(min);
        exposureValues.append(max);
    }

    captureExposureN->setRecommendedValues(exposureValues);

    if (currentCCD->getMinMaxStep(frameProp, "WIDTH", &min, &max, &step))
    {
        if (min >= max)
        {
            resetFrameToZero();
            return;
        }

        if (step == 0.0)
            xstep = static_cast<int>(max * 0.05);
        else
            xstep = static_cast<int>(step);

        if (min >= 0 && max > 0)
        {
            captureFrameWN->setMinimum(static_cast<int>(min));
            captureFrameWN->setMaximum(static_cast<int>(max));
            captureFrameWN->setSingleStep(xstep);
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

        if (step == 0.0)
            ystep = static_cast<int>(max * 0.05);
        else
            ystep = static_cast<int>(step);

        if (min >= 0 && max > 0)
        {
            captureFrameHN->setMinimum(static_cast<int>(min));
            captureFrameHN->setMaximum(static_cast<int>(max));
            captureFrameHN->setSingleStep(ystep);
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

        if (step == 0.0)
            step = xstep;

        if (min >= 0 && max > 0)
        {
            captureFrameXN->setMinimum(static_cast<int>(min));
            captureFrameXN->setMaximum(static_cast<int>(max));
            captureFrameXN->setSingleStep(static_cast<int>(step));
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

        if (step == 0.0)
            step = ystep;

        if (min >= 0 && max > 0)
        {
            captureFrameYN->setMinimum(static_cast<int>(min));
            captureFrameYN->setMaximum(static_cast<int>(max));
            captureFrameYN->setSingleStep(static_cast<int>(step));
        }
    }
    else
        return;

    // cull to camera limits, if there are any
    if (useGuideHead == false)
        cullToDSLRLimits();

    if (reset == 1 || frameSettings.contains(targetChip) == false)
    {
        QVariantMap settings;

        settings["x"]    = 0;
        settings["y"]    = 0;
        settings["w"]    = captureFrameWN->maximum();
        settings["h"]    = captureFrameHN->maximum();
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
        x = qBound(captureFrameXN->minimum(), x, captureFrameXN->maximum() - 1);
        y = qBound(captureFrameYN->minimum(), y, captureFrameYN->maximum() - 1);
        w = qBound(captureFrameWN->minimum(), w, captureFrameWN->maximum());
        h = qBound(captureFrameHN->minimum(), h, captureFrameHN->maximum());

        settings["x"] = x;
        settings["y"] = y;
        settings["w"] = w;
        settings["h"] = h;

        frameSettings[targetChip] = settings;
    }

    if (frameSettings.contains(targetChip))
    {
        QVariantMap settings = frameSettings[targetChip];
        int x = settings["x"].toInt();
        int y = settings["y"].toInt();
        int w = settings["w"].toInt();
        int h = settings["h"].toInt();

        if (targetChip->canBin())
        {
            targetChip->getMaxBin(&binx, &biny);
            captureBinHN->setMaximum(binx);
            captureBinVN->setMaximum(biny);

            captureBinHN->setValue(settings["binx"].toInt());
            captureBinVN->setValue(settings["biny"].toInt());
        }
        else
        {
            captureBinHN->setValue(1);
            captureBinVN->setValue(1);
        }

        if (x >= 0)
            captureFrameXN->setValue(x);
        if (y >= 0)
            captureFrameYN->setValue(y);
        if (w > 0)
            captureFrameWN->setValue(w);
        if (h > 0)
            captureFrameHN->setValue(h);
    }
}

void Capture::processCCDNumber(INumberVectorProperty * nvp)
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

void Capture::syncFrameType(ISD::GDInterface * ccd)
{
    if (ccd->getDeviceName() != cameraS->currentText().toLatin1())
        return;

    ISD::CCDChip * tChip = (static_cast<ISD::CCD *>(ccd))->getChip(ISD::CCDChip::PRIMARY_CCD);

    QStringList frameTypes = tChip->getFrameTypes();

    captureTypeS->clear();

    if (frameTypes.isEmpty())
        captureTypeS->setEnabled(false);
    else
    {
        captureTypeS->setEnabled(true);
        captureTypeS->addItems(frameTypes);
        captureTypeS->setCurrentIndex(tChip->getFrameType());
    }
}

bool Capture::setFilterWheel(const QString &device)
{
    bool deviceFound = false;

    for (int i = 0; i < filterWheelS->count(); i++)
        if (device == filterWheelS->itemText(i))
        {
            // Check Combo if it was set to something else.
            if (filterWheelS->currentIndex() != i)
            {
                filterWheelS->blockSignals(true);
                filterWheelS->setCurrentIndex(i);
                filterWheelS->blockSignals(false);
            }

            checkFilter(i);
            deviceFound = true;
            break;
        }

    if (deviceFound == false)
        return false;

    return true;
}

QString Capture::filterWheel()
{
    if (filterWheelS->currentIndex() >= 1)
        return filterWheelS->currentText();

    return QString();
}

bool Capture::setFilter(const QString &filter)
{
    if (filterWheelS->currentIndex() >= 1)
    {
        captureFilterS->setCurrentText(filter);
        return true;
    }

    return false;
}

QString Capture::filter()
{
    return captureFilterS->currentText();
}

void Capture::checkFilter(int filterNum)
{
    if (filterNum == -1)
    {
        filterNum = filterWheelS->currentIndex();
        if (filterNum == -1)
            return;
    }

    // "--" is no filter
    if (filterNum == 0)
    {
        currentFilter = nullptr;
        m_CurrentFilterPosition = -1;
        filterEditB->setEnabled(false);
        captureFilterS->clear();
        syncFilterInfo();
        return;
    }

    if (filterNum <= Filters.count())
        currentFilter = Filters.at(filterNum - 1);

    filterManager->setCurrentFilterWheel(currentFilter);

    syncFilterInfo();

    captureFilterS->clear();

    captureFilterS->addItems(filterManager->getFilterLabels());

    m_CurrentFilterPosition = filterManager->getFilterPosition();

    filterEditB->setEnabled(m_CurrentFilterPosition > 0);

    captureFilterS->setCurrentIndex(m_CurrentFilterPosition - 1);


    /*if (activeJob &&
        (activeJob->getStatus() == SequenceJob::JOB_ABORTED || activeJob->getStatus() == SequenceJob::JOB_IDLE))
        activeJob->setCurrentFilter(currentFilterPosition);*/
}

void Capture::syncFilterInfo()
{
    if (currentCCD)
    {
        ITextVectorProperty * activeDevices = currentCCD->getBaseDevice()->getText("ACTIVE_DEVICES");
        if (activeDevices)
        {
            IText * activeFilter = IUFindText(activeDevices, "ACTIVE_FILTER");
            if (activeFilter)
            {
                if (currentFilter != nullptr && (activeFilter->text != currentFilter->getDeviceName()))
                {
                    m_FilterOverride = true;
                    activeFilter->aux0 = &m_FilterOverride;
                    IUSaveText(activeFilter, currentFilter->getDeviceName().toLatin1().constData());
                    currentCCD->getDriverInfo()->getClientManager()->sendNewText(activeDevices);
                }
                // Reset filter name in CCD driver
                else if (currentFilter == nullptr && strlen(activeFilter->text) > 0)
                {
                    m_FilterOverride = true;
                    activeFilter->aux0 = &m_FilterOverride;
                    IUSaveText(activeFilter, "");
                    currentCCD->getDriverInfo()->getClientManager()->sendNewText(activeDevices);
                }
            }
        }
    }
}

/**
 * @brief Ensure that all pending preparation tasks are be completed (focusing, dithering, etc.)
 *        and start the next exposure.
 *
 * Checks of pending preparations depends upon the frame type:
 *
 * - For light frames, pending preparations like focusing, dithering etc. needs
 *   to be checked before each single frame capture. efore starting to capture the next light frame,
 *   checkLightFramePendingTasks() is called to check if all pending preparation tasks have
 *   been completed successfully. As soon as this is the case, the sequence timer
 *   #seqTimer is started to wait the configured delay and starts capturing the next image.
 *
 * - For bias, dark and flat frames, preparation jobs are only executed when starting a sequence.
 *   Hence, for these frames we directly start the sequence timer #seqTimer.
 *
 * @return IPS_OK, iff all pending preparation jobs are completed (@see checkLightFramePendingTasks()).
 *         In that case, the #seqTimer is started to wait for the configured settling delay and then
 *         capture the next image (@see Capture::captureImage). In case that a pending task aborted,
 *         IPS_IDLE is returned.
 */
IPState Capture::startNextExposure()
{
    // Since this function is looping while pending tasks are running in parallel
    // it might happen that one of them leads to abort() which sets the #activeJob to nullptr.
    // In this case we terminate the loop by returning #IPS_IDLE without starting a new capture.
    if (activeJob == nullptr)
        return IPS_IDLE;

    // check pending jobs for light frames. All other frame types do not contain mid-sequence checks.
    if (activeJob->getFrameType() == FRAME_LIGHT)
    {
        IPState pending = checkLightFramePendingTasks();
        if (pending != IPS_OK)
            // there are still some jobs pending
            return pending;
    }

    // nothing pending, let's start the next exposure
    if (seqDelay > 0)
    {
        secondsLabel->setText(i18n("Waiting..."));
        m_State = CAPTURE_WAITING;
        emit newStatus(Ekos::CAPTURE_WAITING);
    }
    seqTimer->start(seqDelay);

    return IPS_OK;
}

/**
 * @brief Try to start capturing the next exposure (@see startNextExposure()).
 *        If startNextExposure() returns, that there are still some jobs pending,
 *        we wait for 1 second and retry to start it again.
 *        If one of the pending preparation jobs has problems, the looping stops.
 */
void Capture::checkNextExposure()
{
    IPState started = startNextExposure();
    // if starting the next exposure did not succeed due to pending jobs running,
    // we retry after 1 second
    if (started == IPS_BUSY)
        QTimer::singleShot(1000, this, &Ekos::Capture::checkNextExposure);
}


void Capture::newFITS(IBLOB * bp)
{
    ISD::CCDChip * tChip = nullptr;

    // If there is no active job, ignore
    if (activeJob == nullptr)
    {
        qCWarning(KSTARS_EKOS_CAPTURE) << "Ignoring received FITS" << bp->name << "as active job is null.";
        return;
    }

    if (meridianFlipStage >= MF_ALIGNING)
    {
        qCWarning(KSTARS_EKOS_CAPTURE) << "Ignoring Received FITS" << bp->name << "as meridian flip stage is" << meridianFlipStage;
        return;
    }

    if (currentCCD->getUploadMode() != ISD::CCD::UPLOAD_LOCAL)
    {
        if (bp == nullptr)
        {
            appendLogText(i18n("Failed to save file to %1", activeJob->getSignature()));
            abort();
            return;
        }

        if (m_State == CAPTURE_IDLE || m_State == CAPTURE_ABORTED)
        {
            qCWarning(KSTARS_EKOS_CAPTURE) << "Ignoring Received FITS" << bp->name << "as current capture state is not active" <<
                                           m_State;
            return;
        }

        if (!strcmp(bp->name, "CCD2"))
            tChip = currentCCD->getChip(ISD::CCDChip::GUIDE_CCD);
        else
            tChip = currentCCD->getChip(ISD::CCDChip::PRIMARY_CCD);

        if (tChip != targetChip)
        {
            if (guideState == GUIDE_IDLE)
                qCWarning(KSTARS_EKOS_CAPTURE) << "Ignoring Received FITS" << bp->name << "as it does not correspond to the target chip" <<
                                               targetChip->getType();
            return;
        }

        if (targetChip->getCaptureMode() == FITS_FOCUS || targetChip->getCaptureMode() == FITS_GUIDE)
        {
            qCWarning(KSTARS_EKOS_CAPTURE) << "Ignoring Received FITS" << bp->name << "as it has the wrong capture mode" <<
                                           targetChip->getCaptureMode();
            return;
        }

        // If the FITS is not for our device, simply ignore
        //if (QString(bp->bvp->device)  != currentCCD->getDeviceName() || (startB->isEnabled() && previewB->isEnabled()))
        if (QString(bp->bvp->device) != currentCCD->getDeviceName())
        {
            qCWarning(KSTARS_EKOS_CAPTURE) << "Ignoring Received FITS" << bp->name << "as the blob device name" << bp->bvp->device
                                           << "does not equal active camera" << currentCCD->getDeviceName();
            return;
        }

        // If this is a preview job, make sure to enable preview button after
        // we receive the FITS
        if (activeJob->isPreview() && previewB->isEnabled() == false)
            previewB->setEnabled(true);

        // m_isLooping client-side looping (next capture starts after image is downloaded to client)
        // currentCCD->isLooping driver side looping (without any delays, next capture starts after driver reads data)
        if (m_isLooping == false && currentCCD->isLooping() == false)
        {
            disconnect(currentCCD, &ISD::CCD::BLOBUpdated, this, &Ekos::Capture::newFITS);

            if (useGuideHead == false && darkSubCheck->isChecked() && activeJob->isPreview())
            {
                FITSView * currentImage = targetChip->getImageView(FITS_NORMAL);
                FITSData * darkData     = DarkLibrary::Instance()->getDarkFrame(targetChip, activeJob->getExposure());
                uint16_t offsetX       = static_cast<uint16_t>(activeJob->getSubX() / activeJob->getXBin());
                uint16_t offsetY       = static_cast<uint16_t>(activeJob->getSubY() / activeJob->getYBin());

                connect(DarkLibrary::Instance(), &DarkLibrary::darkFrameCompleted, this, [&](bool completed)
                {
                    if (currentCCD->isLooping() == false)
                        DarkLibrary::Instance()->disconnect(this);
                    if (completed)
                        setCaptureComplete();
                    else
                        abort();
                });
                connect(DarkLibrary::Instance(), &DarkLibrary::newLog, this, &Ekos::Capture::appendLogText);

                if (darkData)
                    DarkLibrary::Instance()->subtract(darkData, currentImage, activeJob->getCaptureFilter(), offsetX, offsetY);
                else
                    DarkLibrary::Instance()->captureAndSubtract(targetChip, currentImage, activeJob->getExposure(), offsetX, offsetY);

                return;
            }
        }
    }

    blobChip    = bp ? static_cast<ISD::CCDChip *>(bp->aux0) : nullptr;
    blobFilename = bp ? static_cast<const char *>(bp->aux2) : QString();

    setCaptureComplete();
}

/**
 * @brief Manage the capture process after a captured image has been successfully downloaded from the camera.
 *
 * When a image frame has been captured and downloaded successfully, send the image to the client (if configured)
 * and execute the book keeping for the captured frame. After this, either processJobCompletion() is executed
 * in case that the job is completed, and resumeSequence() otherwise.
 *
 * Book keeping means:
 * - increase / decrease the counters for focusing and dithering
 * - increase the frame counter
 * - update the average download time
 *
 * @return IPS_BUSY iff pausing is requested, IPS_OK otherwise.
 */
IPState Capture::setCaptureComplete()
{
    captureTimeout.stop();
    m_CaptureTimeoutCounter = 0;

    downloadProgressTimer.stop();

    // In case we're framing, let's start
    if (m_isLooping)
    {
        sendNewImage(blobFilename, blobChip);
        secondsLabel->setText(i18n("Framing..."));
        activeJob->capture(darkSubCheck->isChecked() ? true : false);
        return IPS_OK;
    }

    if (currentCCD->isLooping() == false)
    {
        disconnect(currentCCD, &ISD::CCD::newExposureValue, this, &Ekos::Capture::setExposureProgress);
        DarkLibrary::Instance()->disconnect(this);
    }

    // Do not calculate download time for images stored on server.
    // Only calculate for longer exposures.
    if (currentCCD->getUploadMode() != ISD::CCD::UPLOAD_LOCAL)
    {
        //This determines the time since the image started downloading
        //Then it gets the estimated time left and displays it in the log.
        double currentDownloadTime = downloadTimer.elapsed() / 1000.0;
        downloadTimes << currentDownloadTime;
        QString dLTimeString = QString::number(currentDownloadTime, 'd', 2);
        QString estimatedTimeString = QString::number(getEstimatedDownloadTime(), 'd', 2);
        appendLogText(i18n("Download Time: %1 s, New Download Time Estimate: %2 s.", dLTimeString, estimatedTimeString));
    }


    secondsLabel->setText(i18n("Complete."));
    // Do not display notifications for very short captures
    if (activeJob->getExposure() >= 1)
        KSNotification::event(QLatin1String("EkosCaptureImageReceived"), i18n("Captured image received"),
                              KSNotification::EVENT_INFO);

    // If it was initially set as pure preview job and NOT as preview for calibration
    if (activeJob->isPreview() && calibrationStage != CAL_CALIBRATION)
    {
        sendNewImage(blobFilename, blobChip);
        jobs.removeOne(activeJob);
        // Reset upload mode if it was changed by preview
        currentCCD->setUploadMode(rememberUploadMode);
        delete (activeJob);
        // Reset active job pointer
        activeJob = nullptr;
        abort();
        if (guideState == GUIDE_SUSPENDED && suspendGuideOnDownload)
            emit resumeGuiding();

        m_State = CAPTURE_IDLE;
        emit newStatus(Ekos::CAPTURE_IDLE);
        return IPS_OK;
    }

    // check if pausing has been requested
    if (checkPausing() == true)
    {
        pauseFunction = &Capture::setCaptureComplete;
        return IPS_BUSY;
    }

    if (! activeJob->isPreview())
    {
        /* Increase the sequence's current capture count */
        activeJob->setCompleted(activeJob->getCompleted() + 1);
        /* Decrease the counter for in-sequence focusing */
        inSequenceFocusCounter--;
    }

    /* Decrease the dithering counter */
    ditherCounter--;

    // JM 2020-06-17: Emit newImage for LOCAL images (stored on remote host)
    if (currentCCD->getUploadMode() == ISD::CCD::UPLOAD_LOCAL)
        emit newImage(activeJob);
    // For Client/Both images, send file name.
    else
        sendNewImage(blobFilename, blobChip);


    /* If we were assigned a captured frame map, also increase the relevant counter for prepareJob */
    SchedulerJob::CapturedFramesMap::iterator frame_item = capturedFramesMap.find(activeJob->getSignature());
    if (capturedFramesMap.end() != frame_item)
        frame_item.value()++;

    if (activeJob->getFrameType() != FRAME_LIGHT)
    {
        if (processPostCaptureCalibrationStage() == false)
            return IPS_OK;

        if (calibrationStage == CAL_CALIBRATION_COMPLETE)
            calibrationStage = CAL_CAPTURING;
    }

    /* The image progress has now one more capture */
    imgProgress->setValue(activeJob->getCompleted());

    appendLogText(i18n("Received image %1 out of %2.", activeJob->getCompleted(), activeJob->getCount()));

    FITSView * currentImage = targetChip->getImageView(FITS_NORMAL);
    double hfr = currentImage ? currentImage->getImageData()->getHFR(HFR_AVERAGE) : 0;
    emit captureComplete(blobFilename, activeJob->getExposure(), activeJob->getFilterName(), hfr);

    m_State = CAPTURE_IMAGE_RECEIVED;
    emit newStatus(Ekos::CAPTURE_IMAGE_RECEIVED);

    currentImgCountOUT->setText(QString("%L1").arg(activeJob->getCompleted()));

    // Check if we need to execute post capture script first
    if (activeJob->getPostCaptureScript().isEmpty() == false)
    {
        postCaptureScript.start(activeJob->getPostCaptureScript());
        appendLogText(i18n("Executing post capture script %1", activeJob->getPostCaptureScript()));
        return IPS_OK;
    }

    // if we're done
    if (activeJob->getCount() <= activeJob->getCompleted())
    {
        processJobCompletion();
        return IPS_OK;
    }

    return resumeSequence();
}

/**
 * @brief Stop execution of the current sequence and check whether there exists a next sequence
 *        and start it, if there is a next one to be started (@see resumeSequence()).
 */
void Capture::processJobCompletion()
{
    activeJob->done();

    if (activeJob->isPreview() == false)
    {
        int index = jobs.indexOf(activeJob);
        QJsonObject oneSequence = m_SequenceArray[index].toObject();
        oneSequence["Status"] = "Complete";
        m_SequenceArray.replace(index, oneSequence);
        emit sequenceChanged(m_SequenceArray);
    }

    stop();

    // Check if there are more pending jobs and execute them
    if (resumeSequence() == IPS_OK)
        return;
    // Otherwise, we're done. We park if required and resume guiding if no parking is done and autoguiding was engaged before.
    else
    {
        //KNotification::event(QLatin1String("CaptureSuccessful"), i18n("CCD capture sequence completed"));
        KSNotification::event(QLatin1String("CaptureSuccessful"), i18n("CCD capture sequence completed"),
                              KSNotification::EVENT_INFO);

        abort();

        m_State = CAPTURE_COMPLETE;
        emit newStatus(Ekos::CAPTURE_COMPLETE);

        //Resume guiding if it was suspended before
        //if (isAutoGuiding && currentCCD->getChip(ISD::CCDChip::GUIDE_CCD) == guideChip)
        if (guideState == GUIDE_SUSPENDED && suspendGuideOnDownload)
            emit resumeGuiding();
    }
}

/**
 * @brief Check, whether dithering is necessary and, in that case initiate it.
 *
 *  Dithering is only required for batch images and does not apply for PREVIEW.
 *
 * There are several situations that determine, if dithering is necessary:
 * 1. the current job captures light frames AND the dither counter has reached 0 AND
 * 2. guiding is running OR the manual dithering option is selected AND
 * 3. there is a guiding camera active AND
 * 4. there hasn't just a meridian flip been finised.
 *
 * @return true iff dithering is necessary.
 */
bool Capture::checkDithering()
{
    // No need if preview only
    if (activeJob && activeJob->isPreview())
        return false;

    if ( (Options::ditherEnabled() || Options::ditherNoGuiding())
            // 2017-09-20 Jasem: No need to dither after post meridian flip guiding
            && meridianFlipStage != MF_GUIDING
            // If CCD is looping, we cannot dither UNLESS a different camera and NOT a guide chip is doing the guiding for us.
            && (currentCCD->isLooping() == false || guideChip == nullptr)
            // We must be either in guide mode or if non-guide dither (via pulsing) is enabled
            && (guideState == GUIDE_GUIDING || Options::ditherNoGuiding())
            // Must be only done for light frames
            && activeJob->getFrameType() == FRAME_LIGHT
            // Check dither counter
            && ditherCounter == 0)
    {
        ditherCounter = Options::ditherFrames();

        secondsLabel->setText(i18n("Dithering..."));

        qCInfo(KSTARS_EKOS_CAPTURE) << "Dithering...";
        appendLogText(i18n("Dithering..."));

        if (currentCCD->isLooping())
            targetChip->abortExposure();

        m_State = CAPTURE_DITHERING;
        ditheringState = IPS_BUSY;
        emit newStatus(Ekos::CAPTURE_DITHERING);

        return true;
    }
    // no dithering required
    return false;
}

/**
 * @brief Try to continue capturing.
 *
 * Take the active job, if there is one, or search for the next one that is either
 * idle or aborted. If a new job is selected, call prepareJob(*SequenceJob) to prepare it and
 * resume guiding (TODO: is this not part of the preparation?). If the current job is still active,
 * initiate checkNextExposure().
 *
 * @return IPS_OK if there is a job that may be continued, IPS_BUSY otherwise.
 */
IPState Capture::resumeSequence()
{
    // If no job is active, we have to find if there are more pending jobs in the queue
    if (!activeJob)
    {
        SequenceJob * next_job = nullptr;

        foreach (SequenceJob * job, jobs)
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
            {
                qCDebug(KSTARS_EKOS_CAPTURE) << "Resuming guiding...";
                emit resumeGuiding();
            }

            return IPS_OK;
        }
        else
        {
            qCDebug(KSTARS_EKOS_CAPTURE) << "All capture jobs complete.";
            return IPS_BUSY;
        }
    }
    // Otherwise, let's prepare for next exposure.
    else
    {
        isTemperatureDeltaCheckActive = (m_AutoFocusReady && limitFocusDeltaTS->isChecked());

        // If we suspended guiding due to primary chip download, resume guide chip guiding now
        if (guideState == GUIDE_SUSPENDED && suspendGuideOnDownload)
        {
            qCInfo(KSTARS_EKOS_CAPTURE) << "Resuming guiding...";
            emit resumeGuiding();
        }

        // If looping, we just increment the file system image count
        if (currentCCD->isLooping())
        {
            if (currentCCD->getUploadMode() != ISD::CCD::UPLOAD_LOCAL)
            {
                checkSeqBoundary(activeJob->getSignature());
                currentCCD->setNextSequenceID(nextSequenceID);
            }
        }
        // otherwise we loop starting the next exposure until all pending
        // jobs are completed
        else
            checkNextExposure();
    }

    return IPS_OK;
}

/**
 * @brief Check, if re-focusing is required and initiate it in that case.
 * @return true iff re-focusing is necessary.
 */
bool Capture::startFocusIfRequired()
{
    // Do not start focus if:
    // 1. There is no active job, or
    // 2. Target frame is not LIGHT
    // 3. Capture is preview only
    if (activeJob == nullptr || activeJob->getFrameType() != FRAME_LIGHT || activeJob->isPreview())
        return false;

    isRefocus = false;
    isInSequenceFocus = (m_AutoFocusReady && limitFocusHFRS->isChecked());

    // check if time for forced refocus
    if (limitRefocusS->isChecked())
    {
        qCDebug(KSTARS_EKOS_CAPTURE) << "Focus elapsed time (secs): " << getRefocusEveryNTimerElapsedSec() <<
                                     ". Requested Interval (secs): " << limitRefocusN->value() * 60;

        if (getRefocusEveryNTimerElapsedSec() >= limitRefocusN->value() * 60)
        {
            isRefocus = true;
            appendLogText(i18n("Scheduled refocus starting after %1 seconds...", getRefocusEveryNTimerElapsedSec()));
        }
    }

    if (!isRefocus && isTemperatureDeltaCheckActive)
    {
        qCDebug(KSTARS_EKOS_CAPTURE) << "Focus temperature delta (°C): " << focusTemperatureDelta <<
                                     ". Requested maximum delta (°C): " << limitFocusDeltaTN->value();

        if (focusTemperatureDelta > limitFocusDeltaTN->value())
        {
            isRefocus = true;
            appendLogText(i18n("Refocus starting because of temperature change of %1 °C...", focusTemperatureDelta));
        }
    }

    // Either it is time to force autofocus or temperature has changed
    if (isRefocus)
    {
        secondsLabel->setText(i18n("Focusing..."));

        if (currentCCD->isLooping())
            targetChip->abortExposure();

        // If we are over 30 mins since last autofocus, we'll reset frame.
        if (limitRefocusN->value() >= 30)
            emit resetFocus();

        // force refocus
        qCDebug(KSTARS_EKOS_CAPTURE) << "Capture is triggering autofocus on line " << __LINE__;
        setFocusStatus(FOCUS_PROGRESS);
        emit checkFocus(0.1);

        m_State = CAPTURE_FOCUSING;
        emit newStatus(Ekos::CAPTURE_FOCUSING);
        return true;
    }
    else if (isInSequenceFocus && inSequenceFocusCounter == 0)
    {
        inSequenceFocusCounter = Options::inSequenceCheckFrames();

        // Post meridian flip we need to reset filter _before_ running in-sequence focusing
        // as it could have changed for whatever reason (e.g. alignment used a different filter).
        // Then when focus process begins with the _target_ filter in place, it should take all the necessary actions to make it
        // work for the next set of captures. This is direct reset to the filter device, not via Filter Manager.
        if (meridianFlipStage != MF_NONE && currentFilter)
        {
            int targetFilterPosition = activeJob->getTargetFilter();
            int currentFilterPosition = filterManager->getFilterPosition();
            if (targetFilterPosition > 0 && targetFilterPosition != currentFilterPosition)
                currentFilter->runCommand(INDI_SET_FILTER, &targetFilterPosition);
        }

        secondsLabel->setText(i18n("Focusing..."));

        if (currentCCD->isLooping())
            targetChip->abortExposure();

        setFocusStatus(FOCUS_PROGRESS);
        emit checkFocus(limitFocusHFRN->value() == 0.0 ? 0.1 : limitFocusHFRN->value());

        qCDebug(KSTARS_EKOS_CAPTURE) << "In-sequence focusing started...";
        m_State = CAPTURE_FOCUSING;
        emit newStatus(Ekos::CAPTURE_FOCUSING);
        return true;
    }

    return false;
}

void Capture::captureOne()
{

    //if (currentCCD->getUploadMode() == ISD::CCD::UPLOAD_LOCAL)
    /*if (fileUploadModeS->currentIndex() != ISD::CCD::UPLOAD_CLIENT)
    {
        appendLogText(i18n("Cannot take preview image while CCD upload mode is set to local or both. Please change "
                           "upload mode to client and try again."));
        return;
    }*/

    if (captureFormatS->currentIndex() == ISD::CCD::FORMAT_NATIVE && darkSubCheck->isChecked())
    {
        appendLogText(i18n("Cannot perform auto dark subtraction of native DSLR formats."));
        return;
    }

    if (addJob(true))
    {
        m_State = CAPTURE_PROGRESS;
        prepareJob(jobs.last());
    }
}

void Capture::startFraming()
{
    m_isLooping = true;
    appendLogText(i18n("Starting framing..."));
    captureOne();
}

void Capture::captureImage()
{
    if (activeJob == nullptr)
        return;

    captureTimeout.stop();

    seqTimer->stop();
    SequenceJob::CAPTUREResult rc = SequenceJob::CAPTURE_OK;

    if (currentCCD->isConnected() == false)
    {
        appendLogText(i18n("Error: Lost connection to CCD."));
        abort();
        return;
    }
    // This test must be placed before the FOCUS_PROGRESS test,
    // as sometimes the FilterManager can cause an auto-focus.
    // If the filterManager is not IDLE, then try again in 1 second.
    switch (filterManagerState)
    {
        case FILTER_IDLE:
            secondsLabel->clear();
            break;

        case FILTER_AUTOFOCUS:
            secondsLabel->setText(i18n("Focusing..."));
            QTimer::singleShot(1000, this, &Ekos::Capture::captureImage);
            return;

        case FILTER_CHANGE:
            secondsLabel->setText(i18n("Changing Filters..."));
            QTimer::singleShot(1000, this, &Ekos::Capture::captureImage);
            return;

        case FILTER_OFFSET:
            secondsLabel->setText(i18n("Adjusting Filter Offset..."));
            QTimer::singleShot(1000, this, &Ekos::Capture::captureImage);
            return;
    }
    if (focusState >= FOCUS_PROGRESS)
    {
        appendLogText(i18n("Cannot capture while focus module is busy."));
        abort();
        return;
    }

    /*
    if (filterSlot != nullptr)
    {
        currentFilterPosition = (int)filterSlot->np[0].value;
        activeJob->setCurrentFilter(currentFilterPosition);
    }*/

    if (currentFilter != nullptr)
    {
        m_CurrentFilterPosition = filterManager->getFilterPosition();
        activeJob->setCurrentFilter(m_CurrentFilterPosition);
    }

    if (currentCCD->hasCooler())
    {
        double temperature = 0;
        currentCCD->getTemperature(&temperature);
        activeJob->setCurrentTemperature(temperature);
    }

    if (currentCCD->isLooping())
    {
        int remaining = activeJob->getCount() - activeJob->getCompleted();
        if (remaining > 1)
            currentCCD->setExposureLoopCount(static_cast<uint>(remaining));
    }

    connect(currentCCD, &ISD::CCD::BLOBUpdated, this, &Ekos::Capture::newFITS, Qt::UniqueConnection);
    connect(currentCCD, &ISD::CCD::previewFITSGenerated, this, &Ekos::Capture::setGeneratedPreviewFITS, Qt::UniqueConnection);

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
            // We need to be in preview mode and in client mode for this to work
            activeJob->setPreview(true);
        }
    }

    // Temporary change upload mode to client when requesting previews
    if (activeJob->isPreview())
    {
        rememberUploadMode = activeJob->getUploadMode();
        activeJob->setUploadMode(ISD::CCD::UPLOAD_CLIENT);
    }

    if (currentCCD->getUploadMode() != ISD::CCD::UPLOAD_LOCAL)
    {
        checkSeqBoundary(activeJob->getSignature());
        currentCCD->setNextSequenceID(nextSequenceID);
    }

    m_State = CAPTURE_CAPTURING;

    //if (activeJob->isPreview() == false)
    // NOTE: Why we didn't emit this before for preview?
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

    connect(currentCCD, &ISD::CCD::newExposureValue, this, &Ekos::Capture::setExposureProgress, Qt::UniqueConnection);

    rc = activeJob->capture(darkSubCheck->isChecked() ? true : false);

    if (rc != SequenceJob::CAPTURE_OK)
    {
        disconnect(currentCCD, &ISD::CCD::newExposureValue, this, &Ekos::Capture::setExposureProgress);
    }
    switch (rc)
    {
        case SequenceJob::CAPTURE_OK:
        {
            emit captureStarting(activeJob->getExposure(), activeJob->getFilterName());
            appendLogText(i18n("Capturing %1-second %2 image...", QString("%L1").arg(activeJob->getExposure(), 0, 'f', 3),
                               activeJob->getFilterName()));
            captureTimeout.start(static_cast<int>(activeJob->getExposure()) * 1000 + CAPTURE_TIMEOUT_THRESHOLD);
            if (activeJob->isPreview() == false)
            {
                int index = jobs.indexOf(activeJob);
                QJsonObject oneSequence = m_SequenceArray[index].toObject();
                oneSequence["Status"] = "In Progress";
                m_SequenceArray.replace(index, oneSequence);
                emit sequenceChanged(m_SequenceArray);
            }
        }
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
            secondsLabel->setText(i18n("Changing filter..."));
            QTimer::singleShot(1000, this, &Ekos::Capture::captureImage);
            break;

        case SequenceJob::CAPTURE_FOCUS_ERROR:
            appendLogText(i18n("Cannot capture while focus module is busy."));
            abort();
            break;
    }
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
/* and file2.png, then the next sequence should be file3.png		           */
/*******************************************************************************/
void Capture::checkSeqBoundary(const QString &path)
{
    int newFileIndex = -1;
    QFileInfo const path_info(path);
    QString const sig_dir(path_info.dir().path());
    QString const sig_file(path_info.completeBaseName());
    QString tempName;
    // seqFileCount = 0;

    // No updates during meridian flip
    if (meridianFlipStage >= MF_ALIGNING)
        return;

    QDirIterator it(sig_dir, QDir::Files);

    while (it.hasNext())
    {
        tempName = it.next();
        QFileInfo info(tempName);

        // This returns the filename without the extension
        tempName = info.completeBaseName();

        // This remove any additional extension (e.g. m42_001.fits.fz)
        // the completeBaseName() would return m42_001.fits
        // and this remove .fits so we end up with m42_001
        tempName = tempName.remove(".fits");

        QString finalSeqPrefix = seqPrefix;
        finalSeqPrefix.remove(SequenceJob::ISOMarker);
        // find the prefix first
        if (tempName.startsWith(finalSeqPrefix, Qt::CaseInsensitive) == false)
            continue;

        /* Do not change the number of captures.
         * - If the sequence is required by the end-user, unconditionally run what each sequence item is requiring.
         * - If the sequence is required by the scheduler, use capturedFramesMap to determine when to stop capturing.
         */
        //seqFileCount++;

        int lastUnderScoreIndex = tempName.lastIndexOf("_");
        if (lastUnderScoreIndex > 0)
        {
            bool indexOK = false;

            newFileIndex = tempName.midRef(lastUnderScoreIndex + 1).toInt(&indexOK);
            if (indexOK && newFileIndex >= nextSequenceID)
                nextSequenceID = newFileIndex + 1;
        }
    }
}

void Capture::appendLogText(const QString &text)
{
    m_LogText.insert(0, i18nc("log entry; %1 is the date, %2 is the text", "%1 %2",
                              KStarsData::Instance()->lt().toString("yyyy-MM-ddThh:mm:ss"), text));

    qCInfo(KSTARS_EKOS_CAPTURE) << text;

    emit newLog(text);
}

void Capture::clearLog()
{
    m_LogText.clear();
    emit newLog(QString());
}

//This method will update the Capture Module and Summary Screen's estimate of how much time is left in the download
void Capture::setDownloadProgress()
{
    if (activeJob)
    {
        double downloadTimeLeft = getEstimatedDownloadTime() - downloadTimer.elapsed() / 1000.0;
        if(downloadTimeLeft > 0)
        {
            exposeOUT->setText(QString("%L1").arg(downloadTimeLeft, 0, 'd', 2));
            emit newDownloadProgress(downloadTimeLeft);
        }
    }
}

void Capture::setExposureProgress(ISD::CCDChip * tChip, double value, IPState state)
{
    if (targetChip != tChip || targetChip->getCaptureMode() != FITS_NORMAL || meridianFlipStage >= MF_ALIGNING)
        return;

    exposeOUT->setText(QString("%L1").arg(value, 0, 'd', 2));

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

    //qDebug() << "Exposure with value " << value << "state" << pstateStr(state);

    if (activeJob != nullptr && state == IPS_OK)
    {
        activeJob->setCaptureRetires(0);
        activeJob->setExposeLeft(0);

        if (currentCCD && currentCCD->getUploadMode() == ISD::CCD::UPLOAD_LOCAL)
        {
            if (activeJob && activeJob->getStatus() == SequenceJob::JOB_BUSY)
            {
                newFITS(nullptr);
                return;
            }
        }

        //if (isAutoGuiding && Options::useEkosGuider() && currentCCD->getChip(ISD::CCDChip::GUIDE_CCD) == guideChip)
        if (guideState == GUIDE_GUIDING && Options::guiderType() == 0 && suspendGuideOnDownload)
        {
            qCDebug(KSTARS_EKOS_CAPTURE) << "Autoguiding suspended until primary CCD chip completes downloading...";
            emit suspendGuiding();
        }

        secondsLabel->setText(i18n("Downloading..."));

        //This will start the clock to see how long the download takes.
        downloadTimer.start();
        downloadProgressTimer.start();


        //disconnect(currentCCD, &ISD::CCD::newExposureValue(ISD::CCDChip*,double,IPState)), this, &Ekos::Capture::updateCaptureProgress(ISD::CCDChip*,double,IPState)));
    }
    // JM: Don't change to i18np, value is DOUBLE, not Integer.
    else if (value <= 1)
        secondsLabel->setText(i18n("second left"));
    else
        secondsLabel->setText(i18n("seconds left"));
}

void Capture::updateCCDTemperature(double value)
{
    if (cameraTemperatureS->isEnabled() == false)
    {
        if (currentCCD->getBaseDevice()->getPropertyPermission("CCD_TEMPERATURE") != IP_RO)
            checkCCD();
    }

    temperatureOUT->setText(QString("%L1").arg(value, 0, 'f', 2));

    if (cameraTemperatureN->cleanText().isEmpty())
        cameraTemperatureN->setValue(value);

    //if (activeJob && (activeJob->getStatus() == SequenceJob::JOB_ABORTED || activeJob->getStatus() == SequenceJob::JOB_IDLE))
    if (activeJob)
        activeJob->setCurrentTemperature(value);
}

void Capture::updateRotatorNumber(INumberVectorProperty * nvp)
{
    if (!strcmp(nvp->name, "ABS_ROTATOR_ANGLE"))
    {
        // Update widget rotator position
        rotatorSettings->setCurrentAngle(nvp->np[0].value);

        //if (activeJob && (activeJob->getStatus() == SequenceJob::JOB_ABORTED || activeJob->getStatus() == SequenceJob::JOB_IDLE))
        if (activeJob)
            activeJob->setCurrentRotation(rotatorSettings->getCurrentRotationPA());
    }
}

bool Capture::addJob(bool preview)
{
    if (m_State != CAPTURE_IDLE && m_State != CAPTURE_ABORTED && m_State != CAPTURE_COMPLETE)
        return false;

    SequenceJob * job = nullptr;
    QString imagePrefix;

    if (preview == false && darkSubCheck->isChecked())
    {
        KSNotification::error(i18n("Auto dark subtract is not supported in batch mode."));
        return false;
    }

    if (fileUploadModeS->currentIndex() != ISD::CCD::UPLOAD_CLIENT && fileRemoteDirT->text().isEmpty())
    {
        KSNotification::error(i18n("You must set remote directory for Local & Both modes."));
        return false;
    }

    if (fileUploadModeS->currentIndex() != ISD::CCD::UPLOAD_LOCAL && fileDirectoryT->text().isEmpty())
    {
        KSNotification::error(i18n("You must set local directory for Client & Both modes."));
        return false;
    }

    if (m_JobUnderEdit)
        job = jobs.at(queueTable->currentRow());
    else
    {
        job = new SequenceJob();
        job->setFilterManager(filterManager);
    }

    if (job == nullptr)
    {
        qWarning() << "Job is nullptr!" << endl;
        return false;
    }

    if (captureISOS)
        job->setISOIndex(captureISOS->currentIndex());

    if (getGain() >= 0)
        job->setGain(getGain());

    if (getOffset() >= 0)
        job->setOffset(getOffset());

    job->setTransforFormat(static_cast<ISD::CCD::TransferFormat>(captureFormatS->currentIndex()));

    job->setPreview(preview);

    if (cameraTemperatureN->isEnabled())
    {
        double currentTemperature;
        currentCCD->getTemperature(&currentTemperature);
        job->setEnforceTemperature(cameraTemperatureS->isChecked());
        job->setTargetTemperature(cameraTemperatureN->value());
        job->setCurrentTemperature(currentTemperature);
    }

    job->setCaptureFilter(static_cast<FITSScale>(filterCombo->currentIndex()));

    job->setUploadMode(static_cast<ISD::CCD::UploadMode>(fileUploadModeS->currentIndex()));
    job->setPostCaptureScript(fileScriptT->text());
    job->setFlatFieldDuration(flatFieldDuration);
    job->setFlatFieldSource(flatFieldSource);
    job->setPreMountPark(preMountPark);
    job->setPreDomePark(preDomePark);
    job->setWallCoord(wallCoord);
    job->setTargetADU(targetADU);
    job->setTargetADUTolerance(targetADUTolerance);

    imagePrefix = filePrefixT->text();

    // JM 2019-11-26: In case there is no raw prefix set
    // BUT target name is set, we update the prefix to include
    // the target name, which is usually set by the scheduler.
    if (imagePrefix.isEmpty() && !m_TargetName.isEmpty())
    {
        filePrefixT->setText(m_TargetName);
        imagePrefix = m_TargetName;
    }

    constructPrefix(imagePrefix);

    job->setPrefixSettings(filePrefixT->text(), fileFilterS->isChecked(), fileDurationS->isChecked(),
                           fileTimestampS->isChecked());
    job->setFrameType(static_cast<CCDFrameType>(captureTypeS->currentIndex()));
    job->setFullPrefix(imagePrefix);

    //if (filterSlot != nullptr && currentFilter != nullptr)
    if (captureFilterS->currentIndex() != -1 && currentFilter != nullptr)
        job->setTargetFilter(captureFilterS->currentIndex() + 1, captureFilterS->currentText());

    job->setExposure(captureExposureN->value());

    job->setCount(captureCountN->value());

    job->setBin(captureBinHN->value(), captureBinVN->value());

    job->setDelay(captureDelayN->value() * 1000); /* in ms */

    job->setActiveChip(targetChip);
    job->setActiveCCD(currentCCD);
    job->setActiveFilter(currentFilter);

    // Custom Properties
    job->setCustomProperties(customPropertiesDialog->getCustomProperties());

    if (currentRotator && rotatorSettings->isRotationEnforced())
    {
        job->setActiveRotator(currentRotator);
        job->setTargetRotation(rotatorSettings->getTargetRotationPA());
        job->setCurrentRotation(rotatorSettings->getCurrentRotationPA());
    }

    job->setFrame(captureFrameXN->value(), captureFrameYN->value(), captureFrameWN->value(), captureFrameHN->value());
    job->setRemoteDir(fileRemoteDirT->text());
    job->setLocalDir(fileDirectoryT->text());

    if (m_JobUnderEdit == false)
    {
        // JM 2018-09-24: If this is the first job added
        // We always ignore job progress by default.
        if (jobs.isEmpty() && preview == false)
            ignoreJobProgress = true;

        jobs.append(job);

        // Nothing more to do if preview
        if (preview)
            return true;
    }

    QJsonObject jsonJob = {{"Status", "Idle"}};

    QString directoryPostfix;

    /* FIXME: Refactor directoryPostfix assignment, whose code is duplicated in scheduler.cpp */
    if (m_TargetName.isEmpty())
        directoryPostfix = QLatin1String("/") + captureTypeS->currentText();
    else
        directoryPostfix = QLatin1String("/") + m_TargetName + QLatin1String("/") + captureTypeS->currentText();
    if ((job->getFrameType() == FRAME_LIGHT || job->getFrameType() == FRAME_FLAT) &&  job->getFilterName().isEmpty() == false)
        directoryPostfix += QLatin1String("/") + job->getFilterName();

    job->setDirectoryPostfix(directoryPostfix);

    int currentRow = 0;
    if (m_JobUnderEdit == false)
    {
        currentRow = queueTable->rowCount();
        queueTable->insertRow(currentRow);
    }
    else
        currentRow = queueTable->currentRow();

    QTableWidgetItem * status = m_JobUnderEdit ? queueTable->item(currentRow, 0) : new QTableWidgetItem();
    job->setStatusCell(status);

    QTableWidgetItem * filter = m_JobUnderEdit ? queueTable->item(currentRow, 1) : new QTableWidgetItem();
    filter->setText("--");
    jsonJob.insert("Filter", "--");
    /*if (captureTypeS->currentText().compare("Bias", Qt::CaseInsensitive) &&
            captureTypeS->currentText().compare("Dark", Qt::CaseInsensitive) &&
            captureFilterS->count() > 0)*/
    if (captureFilterS->count() > 0 &&
            (captureTypeS->currentIndex() == FRAME_LIGHT || captureTypeS->currentIndex() == FRAME_FLAT))
    {
        filter->setText(captureFilterS->currentText());
        jsonJob.insert("Filter", captureFilterS->currentText());
    }

    filter->setTextAlignment(Qt::AlignHCenter);
    filter->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

    QTableWidgetItem * type = m_JobUnderEdit ? queueTable->item(currentRow, 2) : new QTableWidgetItem();
    type->setText(captureTypeS->currentText());
    type->setTextAlignment(Qt::AlignHCenter);
    type->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    jsonJob.insert("Type", type->text());

    QTableWidgetItem * bin = m_JobUnderEdit ? queueTable->item(currentRow, 3) : new QTableWidgetItem();
    bin->setText(QString("%1x%2").arg(captureBinHN->value()).arg(captureBinVN->value()));
    bin->setTextAlignment(Qt::AlignHCenter);
    bin->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    jsonJob.insert("Bin", bin->text());

    QTableWidgetItem * exp = m_JobUnderEdit ? queueTable->item(currentRow, 4) : new QTableWidgetItem();
    exp->setText(QString("%L1").arg(captureExposureN->value(), 0, 'f', captureExposureN->decimals()));
    exp->setTextAlignment(Qt::AlignHCenter);
    exp->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    jsonJob.insert("Exp", exp->text());

    QTableWidgetItem * iso = m_JobUnderEdit ? queueTable->item(currentRow, 5) : new QTableWidgetItem();
    if (captureISOS && captureISOS->currentIndex() != -1)
    {
        iso->setText(captureISOS->currentText());
        jsonJob.insert("ISO/Gain", iso->text());
    }
    else if (captureGainN->value() >= 0 && std::fabs(captureGainN->value() - GainSpinSpecialValue) > 0)
    {
        iso->setText(captureGainN->cleanText());
        jsonJob.insert("ISO/Gain", iso->text());
    }
    else
    {
        iso->setText("--");
        jsonJob.insert("ISO/Gain", "--");
    }
    iso->setTextAlignment(Qt::AlignHCenter);
    iso->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

    QTableWidgetItem * offset = m_JobUnderEdit ? queueTable->item(currentRow, 6) : new QTableWidgetItem();
    if (captureOffsetN->value() >= 0 && std::fabs(captureOffsetN->value() - OffsetSpinSpecialValue) > 0)
    {
        offset->setText(captureOffsetN->cleanText());
        jsonJob.insert("Offset", offset->text());
    }
    else
    {
        offset->setText("--");
        jsonJob.insert("Offset", "--");
    }
    offset->setTextAlignment(Qt::AlignHCenter);
    offset->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

    QTableWidgetItem * count = m_JobUnderEdit ? queueTable->item(currentRow, 7) : new QTableWidgetItem();
    job->setCountCell(count);
    jsonJob.insert("Count", count->text());

    if (m_JobUnderEdit == false)
    {
        queueTable->setItem(currentRow, 0, status);
        queueTable->setItem(currentRow, 1, filter);
        queueTable->setItem(currentRow, 2, type);
        queueTable->setItem(currentRow, 3, bin);
        queueTable->setItem(currentRow, 4, exp);
        queueTable->setItem(currentRow, 5, iso);
        queueTable->setItem(currentRow, 6, offset);
        queueTable->setItem(currentRow, 7, count);

        m_SequenceArray.append(jsonJob);
        emit sequenceChanged(m_SequenceArray);
    }

    removeFromQueueB->setEnabled(true);

    if (queueTable->rowCount() > 0)
    {
        queueSaveAsB->setEnabled(true);
        queueSaveB->setEnabled(true);
        resetB->setEnabled(true);
        m_Dirty = true;
    }

    if (queueTable->rowCount() > 1)
    {
        queueUpB->setEnabled(true);
        queueDownB->setEnabled(true);
    }

    if (m_JobUnderEdit)
    {
        m_JobUnderEdit = false;
        resetJobEdit();
        appendLogText(i18n("Job #%1 changes applied.", currentRow + 1));

        m_SequenceArray.replace(currentRow, jsonJob);
        emit sequenceChanged(m_SequenceArray);
    }

    return true;
}

void Capture::removeJobFromQueue()
{
    int currentRow = queueTable->currentRow();

    if (currentRow < 0)
        currentRow = queueTable->rowCount() - 1;

    removeJob(currentRow);

    // update selection
    if (queueTable->rowCount() == 0)
        return;

    if (currentRow > queueTable->rowCount())
        queueTable->selectRow(queueTable->rowCount() - 1);
    else
        queueTable->selectRow(currentRow);
}

void Capture::removeJob(int index)
{
    if (m_State != CAPTURE_IDLE && m_State != CAPTURE_ABORTED && m_State != CAPTURE_COMPLETE)
        return;

    if (m_JobUnderEdit)
    {
        resetJobEdit();
        return;
    }

    if (index < 0)
        return;


    queueTable->removeRow(index);

    m_SequenceArray.removeAt(index);
    emit sequenceChanged(m_SequenceArray);

    SequenceJob * job = jobs.at(index);
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

    m_Dirty = true;
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
        QTableWidgetItem * downItem = queueTable->takeItem(currentRow, i);
        QTableWidgetItem * upItem   = queueTable->takeItem(destinationRow, i);

        queueTable->setItem(destinationRow, i, downItem);
        queueTable->setItem(currentRow, i, upItem);
    }

    SequenceJob * job = jobs.takeAt(currentRow);

    jobs.removeOne(job);
    jobs.insert(destinationRow, job);

    QJsonObject currentJob = m_SequenceArray[currentRow].toObject();
    m_SequenceArray.replace(currentRow, m_SequenceArray[destinationRow]);
    m_SequenceArray.replace(destinationRow, currentJob);
    emit sequenceChanged(m_SequenceArray);

    queueTable->selectRow(destinationRow);

    for (int i = 0; i < jobs.count(); i++)
        jobs.at(i)->setStatusCell(queueTable->item(i, 0));

    m_Dirty = true;
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
        QTableWidgetItem * downItem = queueTable->takeItem(currentRow, i);
        QTableWidgetItem * upItem   = queueTable->takeItem(destinationRow, i);

        queueTable->setItem(destinationRow, i, downItem);
        queueTable->setItem(currentRow, i, upItem);
    }

    SequenceJob * job = jobs.takeAt(currentRow);

    jobs.removeOne(job);
    jobs.insert(destinationRow, job);

    QJsonObject currentJob = m_SequenceArray[currentRow].toObject();
    m_SequenceArray.replace(currentRow, m_SequenceArray[destinationRow]);
    m_SequenceArray.replace(destinationRow, currentJob);
    emit sequenceChanged(m_SequenceArray);

    queueTable->selectRow(destinationRow);

    for (int i = 0; i < jobs.count(); i++)
        jobs.at(i)->setStatusCell(queueTable->item(i, 0));

    m_Dirty = true;
}

void Capture::setBusy(bool enable)
{
    isBusy = enable;

    enable ? pi->startAnimation() : pi->stopAnimation();
    previewB->setEnabled(!enable);
    loopB->setEnabled(!enable);

    foreach (QAbstractButton * button, queueEditButtonGroup->buttons())
        button->setEnabled(!enable);
}

/**
 * @brief Update the counters of existing frames and continue with prepareActiveJob(), if there exist less
 *        images than targeted. If enough images exist, continue with processJobCompletion().
 */
void Capture::prepareJob(SequenceJob * job)
{
    activeJob = job;

    if (m_isLooping == false)
        qCDebug(KSTARS_EKOS_CAPTURE) << "Preparing capture job" << job->getSignature() << "for execution.";

    int index = jobs.indexOf(job);
    if (index >= 0)
        queueTable->selectRow(index);

    if (activeJob->getActiveCCD() != currentCCD)
    {
        setCamera(activeJob->getActiveCCD()->getDeviceName());
    }

    /*if (activeJob->isPreview())
        seqTotalCount = -1;
    else
        seqTotalCount = activeJob->getCount();*/

    seqDelay = activeJob->getDelay();

    // seqCurrentCount = activeJob->getCompleted();

    if (activeJob->isPreview() == false)
    {
        fullImgCountOUT->setText(QString("%L1").arg(activeJob->getCount()));
        currentImgCountOUT->setText(QString("%L1").arg(activeJob->getCompleted()));

        // set the progress info
        imgProgress->setEnabled(true);
        imgProgress->setMaximum(activeJob->getCount());
        imgProgress->setValue(activeJob->getCompleted());

        if (currentCCD->getUploadMode() != ISD::CCD::UPLOAD_LOCAL)
            updateSequencePrefix(activeJob->getFullPrefix(), QFileInfo(activeJob->getSignature()).path());
    }

    // We check if the job is already fully or partially complete by checking how many files of its type exist on the file system
    if (activeJob->isPreview() == false)
    {
        // The signature is the unique identification path in the system for a particular job. Format is "<storage path>/<target>/<frame type>/<filter name>".
        // If the Scheduler is requesting the Capture tab to process a sequence job, a target name will be inserted after the sequence file storage field (e.g. /path/to/storage/target/Light/...)
        // If the end-user is requesting the Capture tab to process a sequence job, the sequence file storage will be used as is (e.g. /path/to/storage/Light/...)
        QString signature = activeJob->getSignature();

        // Now check on the file system ALL the files that exist with the above signature
        // If 29 files exist for example, then nextSequenceID would be the NEXT file number (30)
        // Therefore, we know how to number the next file.
        // However, we do not deduce the number of captures to process from this function.
        checkSeqBoundary(signature);

        // Captured Frames Map contains a list of signatures:count of _already_ captured files in the file system.
        // This map is set by the Scheduler in order to complete efficiently the required captures.
        // When the end-user requests a sequence to be processed, that map is empty.
        //
        // Example with a 5xL-5xR-5xG-5xB sequence
        //
        // When the end-user loads and runs this sequence, each filter gets to capture 5 frames, then the procedure stops.
        // When the Scheduler executes a job with this sequence, the procedure depends on what is in the storage.
        //
        // Let's consider the Scheduler has 3 instances of this job to run.
        //
        // When the first job completes the sequence, there are 20 images in the file system (5 for each filter).
        // When the second job starts, Scheduler finds those 20 images but requires 20 more images, thus sets the frames map counters to 0 for all LRGB frames.
        // When the third job starts, Scheduler now has 40 images, but still requires 20 more, thus again sets the frames map counters to 0 for all LRGB frames.
        //
        // Now let's consider something went wrong, and the third job was aborted before getting to 60 images, say we have full LRG, but only 1xB.
        // When Scheduler attempts to run the aborted job again, it will count captures in storage, subtract previous job requirements, and set the frames map counters to 0 for LRG, and 4 for B.
        // When the sequence runs, the procedure will bypass LRG and proceed to capture 4xB.
        if (capturedFramesMap.contains(signature))
        {
            // Get the current capture count from the map
            int count = capturedFramesMap[signature];

            // Count how many captures this job has to process, given that previous jobs may have done some work already
            foreach (SequenceJob * a_job, jobs)
                if (a_job == activeJob)
                    break;
                else if (a_job->getSignature() == activeJob->getSignature())
                    count -= a_job->getCompleted();

            // This is the current completion count of the current job
            activeJob->setCompleted(count);
        }
        // JM 2018-09-24: Only set completed jobs to 0 IF the scheduler set captured frames map to begin with
        // If the map is empty, then no scheduler is used and it should proceed as normal.
        else if (capturedFramesMap.count() > 0)
        {
            // No preliminary information, we reset the job count and run the job unconditionally to clarify the behavior
            activeJob->setCompleted(0);
        }
        // JM 2018-09-24: In case ignoreJobProgress is enabled
        // We check if this particular job progress ignore flag is set. If not,
        // then we set it and reset completed to zero. Next time it is evaluated here again
        // It will maintain its count regardless
        else if (ignoreJobProgress && activeJob->getJobProgressIgnored() == false)
        {
            activeJob->setJobProgressIgnored(true);
            activeJob->setCompleted(0);
        }
        // We cannot rely on sequenceID to give us a count - if we don't ignore job progress, we leave the count as it was originally
#if 0
        // If we cannot ignore job progress, then we set completed job number according to what
        // was found on the file system.
        else if (ignoreJobProgress == false)
        {
            int count = nextSequenceID - 1;
            if (count < activeJob->getCount())
                activeJob->setCompleted(count);
            else
                activeJob->setCompleted(activeJob->getCount());
        }
#endif

        // Check whether active job is complete by comparing required captures to what is already available
        if (activeJob->getCount() <= activeJob->getCompleted())
        {
            activeJob->setCompleted(activeJob->getCount());
            appendLogText(i18n("Job requires %1-second %2 images, has already %3/%4 captures and does not need to run.",
                               QString("%L1").arg(job->getExposure(), 0, 'f', 3), job->getFilterName(),
                               activeJob->getCompleted(), activeJob->getCount()));
            processJobCompletion();

            /* FIXME: find a clearer way to exit here */
            return;
        }
        else
        {
            // There are captures to process
            currentImgCountOUT->setText(QString("%L1").arg(activeJob->getCompleted()));
            appendLogText(i18n("Job requires %1-second %2 images, has %3/%4 frames captured and will be processed.",
                               QString("%L1").arg(job->getExposure(), 0, 'f', 3), job->getFilterName(),
                               activeJob->getCompleted(), activeJob->getCount()));

            // Emit progress update - done a few lines below
            // emit newImage(nullptr, activeJob);

            currentCCD->setNextSequenceID(nextSequenceID);
        }
    }

    if (currentCCD->isBLOBEnabled() == false)
    {
        // FIXME: Move this warning pop-up elsewhere, it will interfere with automation.
        //        if (Options::guiderType() != Ekos::Guide::GUIDE_INTERNAL || KMessageBox::questionYesNo(nullptr, i18n("Image transfer is disabled for this camera. Would you like to enable it?")) ==
        //                KMessageBox::Yes)
        if (Options::guiderType() != Ekos::Guide::GUIDE_INTERNAL)
        {
            currentCCD->setBLOBEnabled(true);
        }
        else
        {
            connect(KSMessageBox::Instance(), &KSMessageBox::accepted, this, [this]()
            {
                //QObject::disconnect(KSMessageBox::Instance(), &KSMessageBox::accepted, this, nullptr);
                KSMessageBox::Instance()->disconnect(this);
                currentCCD->setBLOBEnabled(true);
                prepareActiveJob();

            });
            connect(KSMessageBox::Instance(), &KSMessageBox::rejected, this, [this]()
            {
                //QObject::disconnect(KSMessageBox::Instance(), &KSMessageBox::rejected, this, nullptr);
                KSMessageBox::Instance()->disconnect(this);
                currentCCD->setBLOBEnabled(true);
                setBusy(false);
            });

            KSMessageBox::Instance()->questionYesNo(i18n("Image transfer is disabled for this camera. Would you like to enable it?"),
                                                    i18n("Image Transfer"), 15);

            return;
        }
    }

    prepareActiveJob();

}

/**
 * @brief Reset #calibrationStage and continue with preparePreCaptureActions().
 */
void Capture::prepareActiveJob()
{
    // Just notification of active job stating up
    emit newImage(activeJob);

    //connect(job, SIGNAL(checkFocus()), this, &Ekos::Capture::startPostFilterAutoFocus()));

    // Reset calibration stage
    if (calibrationStage == CAL_CAPTURING)
    {
        if (activeJob->getFrameType() != FRAME_LIGHT)
            calibrationStage = CAL_PRECAPTURE_COMPLETE;
        else
            calibrationStage = CAL_NONE;
    }

    /* Disable this restriction, let the sequence run even if focus did not run prior to the capture.
     * Besides, this locks up the Scheduler when the Capture module starts a sequence without any prior focus procedure done.
     * This is quite an old code block. The message "Manual scheduled" seems to even refer to some manual intervention?
     * With the new HFR threshold, it might be interesting to prevent the execution because we actually need an HFR value to
     * begin capturing, but even there, on one hand it makes sense for the end-user to know what HFR to put in the edit box,
     * and on the other hand the focus procedure will deduce the next HFR automatically.
     * But in the end, it's not entirely clear what the intent was. Note there is still a warning that a preliminary autofocus
     * procedure is important to avoid any surprise that could make the whole schedule ineffective.
     */

    preparePreCaptureActions();
}

/**
 * @brief Trigger setting the filter, temperature, (if existing) the rotator angle and
 *        let the #activeJob execute the preparation actions before a capture may
 *        take place (@see SequenceJob::prepareCapture()).
 *
 * After triggering the settings, this method returns. This mechanism is slightly tricky, since it
 * asynchronous and event based and works as collaboration between Capture and SequenceJob. Capture has
 * the connection to devices and SequenceJob knows the target values.
 *
 * Each time Capture receives an updated value - e.g. the current CCD temperature
 * (@see updateCCDTemperature()) - it informs the #activeJob about the current CCD temperature.
 * SequenceJob checks, if it has reached the target value and if yes, sets this action as as completed.
 *
 * As soon as all actions are completed, SequenceJob emits a prepareComplete() event, which triggers
 * executeJob() from the Capture module.
 */
void Capture::preparePreCaptureActions()
{
    // Update position
    if (m_CurrentFilterPosition > 0)
        activeJob->setCurrentFilter(m_CurrentFilterPosition);

    // update temperature
    if (currentCCD->hasCooler() && activeJob->getEnforceTemperature())
    {
        double temperature = 0;
        currentCCD->getTemperature(&temperature);
        activeJob->setCurrentTemperature(temperature);
    }

    // update rotator angle
    if (currentRotator != nullptr && activeJob->getTargetRotation() != Ekos::INVALID_VALUE)
        activeJob->setCurrentRotation(rotatorSettings->getCurrentRotationPA());

    setBusy(true);

    if (activeJob->isPreview())
    {
        startB->setIcon(
            QIcon::fromTheme("media-playback-stop"));
        startB->setToolTip(i18n("Stop"));
    }

    connect(activeJob, &SequenceJob::prepareState, this, &Ekos::Capture::updatePrepareState);
    connect(activeJob, &SequenceJob::prepareComplete, this, &Ekos::Capture::executeJob);

    activeJob->prepareCapture();
}

/**
 * @brief Listen to device property changes (temperature, rotator) that are triggered by
 *        SequenceJob.
 */
void Capture::updatePrepareState(Ekos::CaptureState prepareState)
{
    m_State = prepareState;
    emit newStatus(prepareState);

    switch (prepareState)
    {
        case CAPTURE_SETTING_TEMPERATURE:
            appendLogText(i18n("Setting temperature to %1 C...", activeJob->getTargetTemperature()));
            secondsLabel->setText(i18n("Set %1 C...", activeJob->getTargetTemperature()));
            break;

        case CAPTURE_SETTING_ROTATOR:
            appendLogText(i18n("Setting rotation to %1 degrees E of N...", activeJob->getTargetRotation()));
            secondsLabel->setText(i18n("Set Rotator %1...", activeJob->getTargetRotation()));
            break;

        default:
            break;

    }
}

/**
 * @brief Start the execution of #activeJob by initiating updatePreCaptureCalibrationStatus().
 */
void Capture::executeJob()
{
    activeJob->disconnect(this);

    QMap<QString, QString> FITSHeader;
    QString rawPrefix = activeJob->property("rawPrefix").toString();
    if (m_ObserverName.isEmpty() == false)
        FITSHeader["FITS_OBSERVER"] = m_ObserverName;
    if (m_TargetName.isEmpty() == false)
        FITSHeader["FITS_OBJECT"] = m_TargetName;
    else if (rawPrefix.isEmpty() == false)
    {
        FITSHeader["FITS_OBJECT"] = rawPrefix;
    }

    if (FITSHeader.count() > 0)
        currentCCD->setFITSHeader(FITSHeader);

    // Update button status
    setBusy(true);

    useGuideHead = (activeJob->getActiveChip()->getType() == ISD::CCDChip::PRIMARY_CCD) ? false : true;

    syncGUIToJob(activeJob);

    calibrationCheckType = CAL_CHECK_TASK;

    updatePreCaptureCalibrationStatus();

    // Check calibration frame requirements
#if 0
    if (activeJob->getFrameType() != FRAME_LIGHT && activeJob->isPreview() == false)
    {
        updatePreCaptureCalibrationStatus();
        return;
    }

    captureImage();
#endif
}

/**
 * @brief This is a wrapping loop for processPreCaptureCalibrationStage(), which contains
 *        all checks before captureImage() may be called.
 *
 * If processPreCaptureCalibrationStage() returns IPS_OK (i.e. everything is ready so that
 * capturing may be started), captureImage() is called. Otherwise, it waits for a second and
 * calls itself again.
 */
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
        // Clear the label if we are neither executing a meridian flip nor re-focusing
        if ((meridianFlipStage == MF_NONE || meridianFlipStage == MF_READY) && m_State != CAPTURE_FOCUSING)
            secondsLabel->clear();
        QTimer::singleShot(1000, this, &Ekos::Capture::updatePreCaptureCalibrationStatus);
        return;
    }

    captureImage();
}

void Capture::setFocusTemperatureDelta(double focusTemperatureDelta)
{
    // This produces too much log spam
    // Maybe add a threshold to report later?
    //qCDebug(KSTARS_EKOS_CAPTURE) << "setFocusTemperatureDelta: " << focusTemperatureDelta;
    this->focusTemperatureDelta = focusTemperatureDelta;
}

/**
 * @brief Slot that listens to guiding deviations reported by the Guide module.
 *
 * Depending on the current status, it triggers several actions:
 * - If there is no active job, it calls checkMeridianFlipReady(), which may initiate a meridian flip.
 * - If guiding has been started after a meridian flip and the deviation is within the expected limits,
 *   the meridian flip is regarded as completed by setMeridianFlipStage(MF_NONE) (@see setMeridianFlipStage()).
 * - If the deviation is beyond the defined limit, capturing is suspended (@see suspend()) and the
 *   #guideDeviationTimer is started.
 * - Otherwise, it checks if there has been a job suspended and restarts it, since guiding is within the limits.
 */
void Capture::setGuideDeviation(double delta_ra, double delta_dec)
{
    //    if (activeJob == nullptr)
    //    {
    //        if (deviationDetected == false)
    //            return;

    //        // Try to find first job that was aborted due to deviation
    //        for(SequenceJob *job : jobs)
    //        {
    //            if (job->getStatus() == SequenceJob::JOB_ABORTED)
    //            {
    //                activeJob = job;
    //                break;
    //            }
    //        }

    //        if (activeJob == nullptr)
    //            return;
    //    }
    // if guiding deviations occur and no job is active, check if a meridian flip is ready to be executed
    if (activeJob == nullptr && checkMeridianFlipReady())
        return;

    // If guiding is started after a meridian flip we will start getting guide deviations again
    // if the guide deviations are within our limits, we resume the sequence
    if (meridianFlipStage == MF_GUIDING)
    {
        double deviation_rms = std::hypot(delta_ra, delta_dec);
        // If the user didn't select any guiding deviation, we fall through
        // otherwise we can for deviation RMS
        if (limitGuideDeviationS->isChecked() == false || deviation_rms < limitGuideDeviationN->value())
        {
            appendLogText(i18n("Post meridian flip calibration completed successfully."));
            // N.B. Set meridian flip stage AFTER resumeSequence() always
            setMeridianFlipStage(MF_NONE);
            return;
        }
    }

    // We don't enforce limit on previews
    if (limitGuideDeviationS->isChecked() == false || (activeJob && (activeJob->isPreview()
            || activeJob->getExposeLeft() == 0.0)))
        return;

    double deviation_rms = sqrt( (delta_ra * delta_ra + delta_dec * delta_dec) / 2.0);

    QString deviationText = QString("%1").arg(deviation_rms, 0, 'f', 3);

    // If we have an active busy job, let's abort it if guiding deviation is exceeded.
    // And we accounted for the spike
    if (activeJob && activeJob->getStatus() == SequenceJob::JOB_BUSY && activeJob->getFrameType() == FRAME_LIGHT)
    {
        if (deviation_rms > limitGuideDeviationN->value())
        {
            // Ignore spikes ONCE
            if (m_SpikeDetected == false)
            {
                m_SpikeDetected = true;
                return;
            }

            appendLogText(i18n("Guiding deviation %1 exceeded limit value of %2 arcsecs, "
                               "suspending exposure and waiting for guider up to %3 seconds.",
                               deviationText, limitGuideDeviationN->value(),
                               QString("%L1").arg(guideDeviationTimer.interval() / 1000.0, 0, 'f', 3)));

            suspend();

            m_SpikeDetected     = false;

            // Check if we need to start meridian flip
            if (checkMeridianFlipReady())
                return;

            m_DeviationDetected = true;

            guideDeviationTimer.start();
        }
        return;
    }

    // Find the first aborted job
    SequenceJob * abortedJob = nullptr;
    for(SequenceJob * job : jobs)
    {
        if (job->getStatus() == SequenceJob::JOB_ABORTED)
        {
            abortedJob = job;
            break;
        }
    }

    if (abortedJob && m_DeviationDetected)
    {
        if (deviation_rms <= limitGuideDeviationN->value())
        {
            guideDeviationTimer.stop();

            if (seqDelay == 0)
                appendLogText(i18n("Guiding deviation %1 is now lower than limit value of %2 arcsecs, "
                                   "resuming exposure.",
                                   deviationText, limitGuideDeviationN->value()));
            else
                appendLogText(i18n("Guiding deviation %1 is now lower than limit value of %2 arcsecs, "
                                   "resuming exposure in %3 seconds.",
                                   deviationText, limitGuideDeviationN->value(), seqDelay / 1000.0));

            QTimer::singleShot(seqDelay, this, &Ekos::Capture::start);
            return;
        }
        else appendLogText(i18n("Guiding deviation %1 is still higher than limit value of %2 arcsecs.",
                                    deviationText, limitGuideDeviationN->value()));
    }
}

void Capture::setFocusStatus(FocusState state)
{
    qCDebug(KSTARS_EKOS_CAPTURE) << "setFocusStatus: " << state;
    focusState = state;

    if (focusState > FOCUS_ABORTED)
        return;

    if (focusState == FOCUS_COMPLETE)
    {
        // enable option to have a refocus event occur if HFR goes over threshold
        m_AutoFocusReady = true;

        //if (limitFocusHFRN->value() == 0.0 && fileHFR == 0.0)
        if (fileHFR == 0.0)
        {
            QList<double> filterHFRList;
            if (m_CurrentFilterPosition > 0)
            {
                // If we are using filters, then we retrieve which filter is currently active.
                // We check if filter lock is used, and store that instead of the current filter.
                // e.g. If current filter HA, but lock filter is L, then the HFR value is stored for L filter.
                // If no lock filter exists, then we store as is (HA)
                QString currentFilterText = captureFilterS->itemText(m_CurrentFilterPosition - 1);
                //QString filterLock = filterManager.data()->getFilterLock(currentFilterText);
                //QString finalFilter = (filterLock == "--" ? currentFilterText : filterLock);

                //filterHFRList = HFRMap[finalFilter];
                filterHFRList = HFRMap[currentFilterText];
                filterHFRList.append(focusHFR);
                //HFRMap[finalFilter] = filterHFRList;
                HFRMap[currentFilterText] = filterHFRList;
            }
            // No filters
            else
            {
                filterHFRList = HFRMap["--"];
                filterHFRList.append(focusHFR);
                HFRMap["--"] = filterHFRList;
            }

            double median = focusHFR;
            int count = filterHFRList.size();
            if (Options::useMedianFocus() && count > 1)
                median = (count % 2) ? filterHFRList[count / 2] : (filterHFRList[count / 2 - 1] + filterHFRList[count / 2]) / 2.0;

            // Add 2.5% (default) to the automatic initial HFR value to allow for minute changes in HFR without need to refocus
            // in case in-sequence-focusing is used.
            limitFocusHFRN->setValue(median + (median * (Options::hFRThresholdPercentage() / 100.0)));
        }

        // successful focus so reset elapsed time
        restartRefocusEveryNTimer();
    }

    if ((isRefocus || isInSequenceFocus) && activeJob && activeJob->getStatus() == SequenceJob::JOB_BUSY)
    {
        // if the focusing has been started during the post-calibration, return to the calibration
        if (calibrationStage < CAL_PRECAPTURE_COMPLETE && m_State == CAPTURE_FOCUSING)
        {
            if (focusState == FOCUS_COMPLETE)
            {
                appendLogText(i18n("Focus complete."));
                secondsLabel->setText(i18n("Focus complete."));
                m_State = CAPTURE_PROGRESS;
            }
            else if (focusState == FOCUS_FAILED || focusState == FOCUS_ABORTED)
            {
                appendLogText(i18n("Autofocus failed."));
                secondsLabel->setText(i18n("Autofocus failed."));
                abort();
            }
        }
        else if (focusState == FOCUS_COMPLETE)
        {
            appendLogText(i18n("Focus complete."));
            secondsLabel->setText(i18n("Focus complete."));
        }
        else if (focusState == FOCUS_FAILED || focusState == FOCUS_ABORTED)
        {
            appendLogText(i18n("Autofocus failed. Aborting exposure..."));
            secondsLabel->setText(i18n("Autofocus failed."));
            abort();
        }
    }
}

void Capture::updateHFRThreshold()
{
    if (fileHFR != 0.0)
        return;

    QList<double> filterHFRList;
    if (captureFilterS->currentIndex() != -1)
    {
        // If we are using filters, then we retrieve which filter is currently active.
        // We check if filter lock is used, and store that instead of the current filter.
        // e.g. If current filter HA, but lock filter is L, then the HFR value is stored for L filter.
        // If no lock filter exists, then we store as is (HA)
        QString currentFilterText = captureFilterS->currentText();
        QString filterLock = filterManager.data()->getFilterLock(currentFilterText);
        QString finalFilter = (filterLock == "--" ? currentFilterText : filterLock);

        filterHFRList = HFRMap[finalFilter];
    }
    // No filters
    else
    {
        filterHFRList = HFRMap["--"];
    }

    if (filterHFRList.empty())
    {
        limitFocusHFRN->setValue(Options::hFRDeviation());
        return;
    }

    double median = 0;
    int count = filterHFRList.size();
    if (count > 1)
        median = (count % 2) ? filterHFRList[count / 2] : (filterHFRList[count / 2 - 1] + filterHFRList[count / 2]) / 2.0;
    else if (count == 1)
        median = filterHFRList[0];

    // Add 2.5% (default) to the automatic initial HFR value to allow for minute changes in HFR without need to refocus
    // in case in-sequence-focusing is used.
    limitFocusHFRN->setValue(median + (median * (Options::hFRThresholdPercentage() / 100.0)));
}

void Capture::setMeridianFlipStage(MFStage stage)
{
    qCDebug(KSTARS_EKOS_CAPTURE) << "setMeridianFlipStage: " << MFStageString(stage);
    if (meridianFlipStage != stage)
    {
        switch (stage)
        {
            case MF_NONE:
                if (m_State == CAPTURE_PAUSED)
                    secondsLabel->setText(i18n("Paused..."));
                meridianFlipStage = stage;
                emit newMeridianFlipStatus(Mount::FLIP_NONE);
                break;

            case MF_READY:
                if (meridianFlipStage == MF_REQUESTED)
                {
                    // we keep the stage on requested until the mount starts the meridian flip
                    emit newMeridianFlipStatus(Mount::FLIP_ACCEPTED);
                }
                else if (m_State == CAPTURE_PAUSED)
                {
                    // paused after meridian flip requested
                    secondsLabel->setText(i18n("Paused..."));
                    meridianFlipStage = stage;
                    emit newMeridianFlipStatus(Mount::FLIP_ACCEPTED);
                }
                else if (!checkMeridianFlipRunning())
                {
                    // if beither a MF has been requested (checked above) or is in a post
                    // MF calibration phase, no MF needs to take place.
                    // Hence we set to the stage to NONE
                    meridianFlipStage = MF_NONE;
                    break;
                }
                // in any other case, ignore it
                break;

            case MF_INITIATED:
                meridianFlipStage = MF_INITIATED;
                emit meridianFlipStarted();
                secondsLabel->setText(i18n("Meridian Flip..."));
                KSNotification::event(QLatin1String("MeridianFlipStarted"), i18n("Meridian flip started"), KSNotification::EVENT_INFO);
                break;

            case MF_REQUESTED:
                if (m_State == CAPTURE_PAUSED)
                    // paused before meridian flip requested
                    emit newMeridianFlipStatus(Mount::FLIP_ACCEPTED);
                else
                    emit newMeridianFlipStatus(Mount::FLIP_WAITING);
                meridianFlipStage = stage;
                break;

            case MF_COMPLETED:
                secondsLabel->setText(i18n("Flip complete."));
                meridianFlipStage = MF_COMPLETED;

                // Reset HFR pixels to file value after meridian flip
                if (isInSequenceFocus)
                {
                    qCDebug(KSTARS_EKOS_CAPTURE) << "Resetting HFR value to file value of" << fileHFR << "pixels after meridian flip.";
                    //firstAutoFocus = true;
                    limitFocusHFRN->setValue(fileHFR);
                }

                // after a meridian flip we do not need to dither
                if ( Options::ditherEnabled() || Options::ditherNoGuiding())
                    ditherCounter = Options::ditherFrames();

                break;

            default:
                meridianFlipStage = stage;
                break;
        }
    }
}


void Capture::meridianFlipStatusChanged(Mount::MeridianFlipStatus status)
{
    qCDebug(KSTARS_EKOS_CAPTURE) << "meridianFlipStatusChanged: " << Mount::meridianFlipStatusString(status);
    switch (status)
    {
        case Mount::FLIP_NONE:
            // MF_NONE as external signal ignored so that re-alignment and guiding are processed first
            if (meridianFlipStage < MF_COMPLETED)
                setMeridianFlipStage(MF_NONE);
            break;

        case Mount::FLIP_PLANNED:
            if (meridianFlipStage > MF_NONE)
            {
                // it seems like the meridian flip had been postponed
                resumeSequence();
                return;
            }
            else
            {
                // If we are autoguiding, we should resume autoguiding after flip
                resumeGuidingAfterFlip = isGuidingOn();

                if (m_State == CAPTURE_IDLE || m_State == CAPTURE_ABORTED || m_State == CAPTURE_COMPLETE || m_State == CAPTURE_PAUSED)
                {
                    setMeridianFlipStage(MF_INITIATED);
                    emit newMeridianFlipStatus(Mount::FLIP_ACCEPTED);
                }
                else
                    setMeridianFlipStage(MF_REQUESTED);
            }
            break;

        case Mount::FLIP_RUNNING:
            setMeridianFlipStage(MF_INITIATED);
            emit newStatus(Ekos::CAPTURE_MERIDIAN_FLIP);
            break;

        case Mount::FLIP_COMPLETED:
            setMeridianFlipStage(MF_COMPLETED);
            emit newStatus(Ekos::CAPTURE_IDLE);
            processFlipCompleted();
            break;

        default:
            break;
    }
}

int Capture::getTotalFramesCount(QString signature)
{
    int  result = 0;
    bool found  = false;

    foreach (SequenceJob * job, jobs)
    {
        // FIXME: this should be part of SequenceJob
        QString sig = job->getSignature();
        if (sig == signature)
        {
            result += job->getCount();
            found = true;
        }
    }

    if (found)
        return result;
    else
        return -1;
}


void Capture::setRotator(ISD::GDInterface * newRotator)
{
    currentRotator = newRotator;
    connect(currentRotator, &ISD::GDInterface::numberUpdated, this, &Ekos::Capture::updateRotatorNumber, Qt::UniqueConnection);
    rotatorB->setEnabled(true);

    rotatorSettings->setRotator(newRotator);

    INumberVectorProperty * nvp = newRotator->getBaseDevice()->getNumber("ABS_ROTATOR_ANGLE");
    rotatorSettings->setCurrentAngle(nvp->np[0].value);
}

void Capture::setTelescope(ISD::GDInterface * newTelescope)
{
    currentTelescope = static_cast<ISD::Telescope *>(newTelescope);

    currentTelescope->disconnect(this);
    connect(currentTelescope, &ISD::GDInterface::numberUpdated, this, &Ekos::Capture::processTelescopeNumber);
    connect(currentTelescope, &ISD::Telescope::newTarget, [&](const QString & target)
    {
        if (m_State == CAPTURE_IDLE || m_State == CAPTURE_COMPLETE)
        {
            QString sanitized = target;
            // Remove illegal characters that can be problematic
            sanitized = sanitized.replace( QRegularExpression("\\s|/|\\(|\\)|:|\\*|~|\"" ), "_" )
                        // Remove any two or more __
                        .replace( QRegularExpression("_{2,}"), "_")
                        // Remove any _ at the end
                        .replace( QRegularExpression("_$"), "");
            filePrefixT->setText(sanitized);
        }
    });

    syncTelescopeInfo();
}

void Capture::syncTelescopeInfo()
{
    if (currentTelescope && currentTelescope->isConnected())
    {
        // Sync ALL CCDs to current telescope
        for (ISD::CCD * oneCCD : CCDs)
        {
            ITextVectorProperty * activeDevices = oneCCD->getBaseDevice()->getText("ACTIVE_DEVICES");
            if (activeDevices)
            {
                IText * activeTelescope = IUFindText(activeDevices, "ACTIVE_TELESCOPE");
                if (activeTelescope)
                {
                    IUSaveText(activeTelescope, currentTelescope->getDeviceName().toLatin1().constData());
                    oneCCD->getDriverInfo()->getClientManager()->sendNewText(activeDevices);
                }
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

    fileDirectoryT->setText(dir);
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
        KSNotification::sorry(message, i18n("Invalid URL"));
        return;
    }

    dirPath = QUrl(fileURL.url(QUrl::RemoveFilename));

    loadSequenceQueue(fileURL.toLocalFile());
}

bool Capture::loadSequenceQueue(const QString &fileURL)
{
    QFile sFile(fileURL);
    if (!sFile.open(QIODevice::ReadOnly))
    {
        QString message = i18n("Unable to open file %1", fileURL);
        KSNotification::sorry(message, i18n("Could Not Open File"));
        return false;
    }

    capturedFramesMap.clear();
    clearSequenceQueue();

    LilXML * xmlParser = newLilXML();

    char errmsg[MAXRBUF];
    XMLEle * root = nullptr;
    XMLEle * ep   = nullptr;
    char c;

    // We expect all data read from the XML to be in the C locale - QLocale::c().
    QLocale cLocale = QLocale::c();

    while (sFile.getChar(&c))
    {
        root = readXMLEle(xmlParser, c, errmsg);

        if (root)
        {
            double sqVersion = cLocale.toDouble(findXMLAttValu(root, "version"));
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
                    m_ObserverName = QString(pcdataXMLEle(ep));
                }
                else if (!strcmp(tagXMLEle(ep), "limitGuideDeviationN"))
                {
                    limitGuideDeviationS->setChecked(!strcmp(findXMLAttValu(ep, "enabled"), "true"));
                    limitGuideDeviationN->setValue(cLocale.toDouble(pcdataXMLEle(ep)));
                }
                else if (!strcmp(tagXMLEle(ep), "Autofocus"))
                {
                    limitFocusHFRS->setChecked(!strcmp(findXMLAttValu(ep, "enabled"), "true"));
                    double const HFRValue = cLocale.toDouble(pcdataXMLEle(ep));
                    // Set the HFR value from XML, or reset it to zero, don't let another unrelated older HFR be used
                    // Note that HFR value will only be serialized to XML when option "Save Sequence HFR to File" is enabled
                    fileHFR = HFRValue > 0.0 ? HFRValue : 0.0;
                    limitFocusHFRN->setValue(fileHFR);
                }
                else if (!strcmp(tagXMLEle(ep), "RefocusOnTemperatureDelta"))
                {
                    limitFocusDeltaTS->setChecked(!strcmp(findXMLAttValu(ep, "enabled"), "true"));
                    double const deltaValue = cLocale.toDouble(pcdataXMLEle(ep));
                    limitFocusDeltaTN->setValue(deltaValue);
                }
                else if (!strcmp(tagXMLEle(ep), "limitRefocusN"))
                {
                    limitRefocusS->setChecked(!strcmp(findXMLAttValu(ep, "enabled"), "true"));
                    int const minutesValue = cLocale.toInt(pcdataXMLEle(ep));
                    // Set the refocus period from XML, or reset it to zero, don't let another unrelated older refocus period be used.
                    refocusEveryNMinutesValue = minutesValue > 0 ? minutesValue : 0;
                    limitRefocusN->setValue(refocusEveryNMinutesValue);
                }
                else if (!strcmp(tagXMLEle(ep), "MeridianFlip"))
                {
                    // meridian flip is managed by the mount only
                    // older files might nevertheless contain MF settings
                    if (! strcmp(findXMLAttValu(ep, "enabled"), "true"))
                        appendLogText(
                            i18n("Meridian flip configuration has been shifted to the mount module. Please configure the meridian flip there."));
                }
                else if (!strcmp(tagXMLEle(ep), "CCD"))
                {
                    cameraS->setCurrentText(pcdataXMLEle(ep));
                    // Signal "activated" of QComboBox does not fire when changing the text programmatically
                    setCamera(pcdataXMLEle(ep));
                }
                else if (!strcmp(tagXMLEle(ep), "FilterWheel"))
                {
                    filterWheelS->setCurrentText(pcdataXMLEle(ep));
                    checkFilter();
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

    m_SequenceURL = QUrl::fromLocalFile(fileURL);
    m_Dirty      = false;
    delLilXML(xmlParser);
    // update save button tool tip
    queueSaveB->setToolTip("Save to " + sFile.fileName());

    return true;
}

bool Capture::processJobInfo(XMLEle * root)
{
    XMLEle * ep;
    XMLEle * subEP;
    rotatorSettings->setRotationEnforced(false);

    QLocale cLocale = QLocale::c();

    for (ep = nextXMLEle(root, 1); ep != nullptr; ep = nextXMLEle(root, 0))
    {
        if (!strcmp(tagXMLEle(ep), "Exposure"))
            captureExposureN->setValue(cLocale.toDouble(pcdataXMLEle(ep)));
        else if (!strcmp(tagXMLEle(ep), "Binning"))
        {
            subEP = findXMLEle(ep, "X");
            if (subEP)
                captureBinHN->setValue(cLocale.toInt(pcdataXMLEle(subEP)));
            subEP = findXMLEle(ep, "Y");
            if (subEP)
                captureBinVN->setValue(cLocale.toInt(pcdataXMLEle(subEP)));
        }
        else if (!strcmp(tagXMLEle(ep), "Frame"))
        {
            subEP = findXMLEle(ep, "X");
            if (subEP)
                captureFrameXN->setValue(cLocale.toInt(pcdataXMLEle(subEP)));
            subEP = findXMLEle(ep, "Y");
            if (subEP)
                captureFrameYN->setValue(cLocale.toInt(pcdataXMLEle(subEP)));
            subEP = findXMLEle(ep, "W");
            if (subEP)
                captureFrameWN->setValue(cLocale.toInt(pcdataXMLEle(subEP)));
            subEP = findXMLEle(ep, "H");
            if (subEP)
                captureFrameHN->setValue(cLocale.toInt(pcdataXMLEle(subEP)));
        }
        else if (!strcmp(tagXMLEle(ep), "Temperature"))
        {
            if (cameraTemperatureN->isEnabled())
                cameraTemperatureN->setValue(cLocale.toDouble(pcdataXMLEle(ep)));

            // If force attribute exist, we change cameraTemperatureS, otherwise do nothing.
            if (!strcmp(findXMLAttValu(ep, "force"), "true"))
                cameraTemperatureS->setChecked(true);
            else if (!strcmp(findXMLAttValu(ep, "force"), "false"))
                cameraTemperatureS->setChecked(false);
        }
        else if (!strcmp(tagXMLEle(ep), "Filter"))
        {
            //captureFilterS->setCurrentIndex(atoi(pcdataXMLEle(ep))-1);
            captureFilterS->setCurrentText(pcdataXMLEle(ep));
        }
        else if (!strcmp(tagXMLEle(ep), "Type"))
        {
            captureTypeS->setCurrentText(pcdataXMLEle(ep));
        }
        else if (!strcmp(tagXMLEle(ep), "Prefix"))
        {
            subEP = findXMLEle(ep, "RawPrefix");
            if (subEP)
                filePrefixT->setText(pcdataXMLEle(subEP));
            subEP = findXMLEle(ep, "FilterEnabled");
            if (subEP)
                fileFilterS->setChecked(!strcmp("1", pcdataXMLEle(subEP)));
            subEP = findXMLEle(ep, "ExpEnabled");
            if (subEP)
                fileDurationS->setChecked(!strcmp("1", pcdataXMLEle(subEP)));
            subEP = findXMLEle(ep, "TimeStampEnabled");
            if (subEP)
                fileTimestampS->setChecked(!strcmp("1", pcdataXMLEle(subEP)));
        }
        else if (!strcmp(tagXMLEle(ep), "Count"))
        {
            captureCountN->setValue(cLocale.toInt(pcdataXMLEle(ep)));
        }
        else if (!strcmp(tagXMLEle(ep), "Delay"))
        {
            captureDelayN->setValue(cLocale.toInt(pcdataXMLEle(ep)));
        }
        else if (!strcmp(tagXMLEle(ep), "PostCaptureScript"))
        {
            fileScriptT->setText(pcdataXMLEle(ep));
        }
        else if (!strcmp(tagXMLEle(ep), "fileDirectoryTectory"))
        {
            fileDirectoryT->setText(pcdataXMLEle(ep));
        }
        else if (!strcmp(tagXMLEle(ep), "RemoteDirectory"))
        {
            fileRemoteDirT->setText(pcdataXMLEle(ep));
        }
        else if (!strcmp(tagXMLEle(ep), "UploadMode"))
        {
            fileUploadModeS->setCurrentIndex(cLocale.toInt(pcdataXMLEle(ep)));
        }
        else if (!strcmp(tagXMLEle(ep), "ISOIndex"))
        {
            if (captureISOS)
                captureISOS->setCurrentIndex(cLocale.toInt(pcdataXMLEle(ep)));
        }
        else if (!strcmp(tagXMLEle(ep), "FormatIndex"))
        {
            captureFormatS->setCurrentIndex(cLocale.toInt(pcdataXMLEle(ep)));
        }
        else if (!strcmp(tagXMLEle(ep), "Rotation"))
        {
            rotatorSettings->setRotationEnforced(true);
            rotatorSettings->setTargetRotationPA(cLocale.toDouble(pcdataXMLEle(ep)));
        }
        else if (!strcmp(tagXMLEle(ep), "Properties"))
        {
            QMap<QString, QMap<QString, double>> propertyMap;

            for (subEP = nextXMLEle(ep, 1); subEP != nullptr; subEP = nextXMLEle(ep, 0))
            {
                QMap<QString, double> numbers;
                XMLEle * oneNumber = nullptr;
                for (oneNumber = nextXMLEle(subEP, 1); oneNumber != nullptr; oneNumber = nextXMLEle(subEP, 0))
                {
                    const char * name = findXMLAttValu(oneNumber, "name");
                    numbers[name] = cLocale.toDouble(pcdataXMLEle(oneNumber));
                }

                const char * name = findXMLAttValu(subEP, "name");
                propertyMap[name] = numbers;
            }

            customPropertiesDialog->setCustomProperties(propertyMap);
            const double gain = getGain();
            if (gain >= 0)
                captureGainN->setValue(gain);
            const double offset = getOffset();
            if (offset >= 0)
                captureOffsetN->setValue(offset);
        }
        else if (!strcmp(tagXMLEle(ep), "Calibration"))
        {
            subEP = findXMLEle(ep, "FlatSource");
            if (subEP)
            {
                XMLEle * typeEP = findXMLEle(subEP, "Type");
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
                        XMLEle * azEP  = findXMLEle(subEP, "Az");
                        XMLEle * altEP = findXMLEle(subEP, "Alt");

                        if (azEP && altEP)
                        {
                            flatFieldSource = SOURCE_WALL;
                            wallCoord.setAz(cLocale.toDouble(pcdataXMLEle(azEP)));
                            wallCoord.setAlt(cLocale.toDouble(pcdataXMLEle(altEP)));
                        }
                    }
                    else
                        flatFieldSource = SOURCE_DAWN_DUSK;
                }
            }

            subEP = findXMLEle(ep, "FlatDuration");
            if (subEP)
            {
                XMLEle * typeEP = findXMLEle(subEP, "Type");
                if (typeEP)
                {
                    if (!strcmp(pcdataXMLEle(typeEP), "Manual"))
                        flatFieldDuration = DURATION_MANUAL;
                }

                XMLEle * aduEP = findXMLEle(subEP, "Value");
                if (aduEP)
                {
                    flatFieldDuration = DURATION_ADU;
                    targetADU         = cLocale.toDouble(pcdataXMLEle(aduEP));
                }

                aduEP = findXMLEle(subEP, "Tolerance");
                if (aduEP)
                {
                    targetADUTolerance = cLocale.toDouble(pcdataXMLEle(aduEP));
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
    QUrl backupCurrent = m_SequenceURL;

    if (m_SequenceURL.toLocalFile().startsWith(QLatin1String("/tmp/")) || m_SequenceURL.toLocalFile().contains("/Temp"))
        m_SequenceURL.clear();

    // If no changes made, return.
    if (m_Dirty == false && !m_SequenceURL.isEmpty())
        return;

    if (m_SequenceURL.isEmpty())
    {
        m_SequenceURL = QFileDialog::getSaveFileUrl(KStars::Instance(), i18n("Save Ekos Sequence Queue"), dirPath,
                        "Ekos Sequence Queue (*.esq)");
        // if user presses cancel
        if (m_SequenceURL.isEmpty())
        {
            m_SequenceURL = backupCurrent;
            return;
        }

        dirPath = QUrl(m_SequenceURL.url(QUrl::RemoveFilename));

        if (m_SequenceURL.toLocalFile().endsWith(QLatin1String(".esq")) == false)
            m_SequenceURL.setPath(m_SequenceURL.toLocalFile() + ".esq");

        /*if (QFile::exists(sequenceURL.toLocalFile()))
        {
            int r = KMessageBox::warningContinueCancel(0,
                                                       i18n("A file named \"%1\" already exists. "
                                                            "Overwrite it?",
                                                            sequenceURL.fileName()),
                                                       i18n("Overwrite File?"), KStandardGuiItem::overwrite());
            if (r == KMessageBox::Cancel)
                return;
        }*/
    }

    if (m_SequenceURL.isValid())
    {
        if ((saveSequenceQueue(m_SequenceURL.toLocalFile())) == false)
        {
            KSNotification::error(i18n("Failed to save sequence queue"), i18n("Save"));
            return;
        }

        m_Dirty = false;
    }
    else
    {
        QString message = i18n("Invalid URL: %1", m_SequenceURL.url());
        KSNotification::sorry(message, i18n("Invalid URL"));
    }
}

void Capture::saveSequenceQueueAs()
{
    m_SequenceURL.clear();
    saveSequenceQueue();
}

bool Capture::saveSequenceQueue(const QString &path)
{
    QFile file;
    QString rawPrefix;
    bool filterEnabled, expEnabled, tsEnabled;
    const QMap<QString, CCDFrameType> frameTypes =
    {
        { "Light", FRAME_LIGHT }, { "Dark", FRAME_DARK }, { "Bias", FRAME_BIAS }, { "Flat", FRAME_FLAT }
    };

    file.setFileName(path);

    if (!file.open(QIODevice::WriteOnly))
    {
        QString message = i18n("Unable to write to file %1", path);
        KSNotification::sorry(message, i18n("Could not open file"));
        return false;
    }

    QTextStream outstream(&file);

    // We serialize sequence data to XML using the C locale
    QLocale cLocale = QLocale::c();

    outstream << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << endl;
    outstream << "<SequenceQueue version='" << SQ_FORMAT_VERSION << "'>" << endl;
    if (m_ObserverName.isEmpty() == false)
        outstream << "<Observer>" << m_ObserverName << "</Observer>" << endl;
    outstream << "<CCD>" << cameraS->currentText() << "</CCD>" << endl;
    outstream << "<FilterWheel>" << filterWheelS->currentText() << "</FilterWheel>" << endl;
    outstream << "<limitGuideDeviationN enabled='" << (limitGuideDeviationS->isChecked() ? "true" : "false") << "'>"
              << cLocale.toString(limitGuideDeviationN->value()) << "</limitGuideDeviationN>" << endl;
    // Issue a warning when autofocus is enabled but Ekos options prevent HFR value from being written
    if (limitFocusHFRS->isChecked() && !Options::saveHFRToFile())
        appendLogText(i18n(
                          "Warning: HFR-based autofocus is set but option \"Save Sequence HFR Value to File\" is not enabled. "
                          "Current HFR value will not be written to sequence file."));
    outstream << "<Autofocus enabled='" << (limitFocusHFRS->isChecked() ? "true" : "false") << "'>"
              << cLocale.toString(Options::saveHFRToFile() ? limitFocusHFRN->value() : 0) << "</Autofocus>" << endl;
    outstream << "<RefocusOnTemperatureDelta enabled='" << (limitFocusDeltaTS->isChecked() ? "true" : "false") << "'>"
              << cLocale.toString(limitFocusDeltaTN->value()) << "</RefocusOnTemperatureDelta>" << endl;
    outstream << "<limitRefocusN enabled='" << (limitRefocusS->isChecked() ? "true" : "false") << "'>"
              << cLocale.toString(limitRefocusN->value()) << "</limitRefocusN>" << endl;
    foreach (SequenceJob * job, jobs)
    {
        job->getPrefixSettings(rawPrefix, filterEnabled, expEnabled, tsEnabled);

        outstream << "<Job>" << endl;

        outstream << "<Exposure>" << cLocale.toString(job->getExposure()) << "</Exposure>" << endl;
        outstream << "<Binning>" << endl;
        outstream << "<X>" << cLocale.toString(job->getXBin()) << "</X>" << endl;
        outstream << "<Y>" << cLocale.toString(job->getXBin()) << "</Y>" << endl;
        outstream << "</Binning>" << endl;
        outstream << "<Frame>" << endl;
        outstream << "<X>" << cLocale.toString(job->getSubX()) << "</X>" << endl;
        outstream << "<Y>" << cLocale.toString(job->getSubY()) << "</Y>" << endl;
        outstream << "<W>" << cLocale.toString(job->getSubW()) << "</W>" << endl;
        outstream << "<H>" << cLocale.toString(job->getSubH()) << "</H>" << endl;
        outstream << "</Frame>" << endl;
        if (job->getTargetTemperature() != Ekos::INVALID_VALUE)
            outstream << "<Temperature force='" << (job->getEnforceTemperature() ? "true" : "false") << "'>"
                      << cLocale.toString(job->getTargetTemperature()) << "</Temperature>" << endl;
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
        outstream << "<Count>" << cLocale.toString(job->getCount()) << "</Count>" << endl;
        // ms to seconds
        outstream << "<Delay>" << cLocale.toString(job->getDelay() / 1000.0) << "</Delay>" << endl;
        if (job->getPostCaptureScript().isEmpty() == false)
            outstream << "<PostCaptureScript>" << job->getPostCaptureScript() << "</PostCaptureScript>" << endl;
        outstream << "<fileDirectoryTectory>" << job->getLocalDir() << "</fileDirectoryTectory>" << endl;
        outstream << "<UploadMode>" << job->getUploadMode() << "</UploadMode>" << endl;
        if (job->getRemoteDir().isEmpty() == false)
            outstream << "<RemoteDirectory>" << job->getRemoteDir() << "</RemoteDirectory>" << endl;
        if (job->getISOIndex() != -1)
            outstream << "<ISOIndex>" << (job->getISOIndex()) << "</ISOIndex>" << endl;
        outstream << "<FormatIndex>" << (job->getTransforFormat()) << "</FormatIndex>" << endl;
        if (job->getTargetRotation() != Ekos::INVALID_VALUE)
            outstream << "<Rotation>" << (job->getTargetRotation()) << "</Rotation>" << endl;
        QMapIterator<QString, QMap<QString, double>> customIter(job->getCustomProperties());
        outstream << "<Properties>" << endl;
        while (customIter.hasNext())
        {
            customIter.next();
            outstream << "<NumberVector name='" << customIter.key() << "'>" << endl;
            QMap<QString, double> numbers = customIter.value();
            QMapIterator<QString, double> numberIter(numbers);
            while (numberIter.hasNext())
            {
                numberIter.next();
                outstream << "<OneNumber name='" << numberIter.key()
                          << "'>" << cLocale.toString(numberIter.value()) << "</OneNumber>" << endl;
            }
            outstream << "</NumberVector>" << endl;
        }
        outstream << "</Properties>" << endl;

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
            outstream << "<Az>" << cLocale.toString(job->getWallCoord().az().Degrees()) << "</Az>" << endl;
            outstream << "<Alt>" << cLocale.toString(job->getWallCoord().alt().Degrees()) << "</Alt>" << endl;
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
            outstream << "<Value>" << cLocale.toString(job->getTargetADU()) << "</Value>" << endl;
            outstream << "<Tolerance>" << cLocale.toString(job->getTargetADUTolerance()) << "</Tolerance>" << endl;
        }
        outstream << "</FlatDuration>" << endl;

        outstream << "<PreMountPark>" << (job->isPreMountPark() ? "True" : "False") << "</PreMountPark>" << endl;
        outstream << "<PreDomePark>" << (job->isPreDomePark() ? "True" : "False") << "</PreDomePark>" << endl;
        outstream << "</Calibration>" << endl;

        outstream << "</Job>" << endl;
    }

    outstream << "</SequenceQueue>" << endl;

    appendLogText(i18n("Sequence queue saved to %1", path));
    file.flush();
    file.close();
    // update save button tool tip
    queueSaveB->setToolTip("Save to " + file.fileName());

    return true;
}

void Capture::resetJobs()
{
    // Stop any running capture
    stop();

    // If a job is selected for edit, reset only that job
    if (m_JobUnderEdit == true)
    {
        SequenceJob * job = jobs.at(queueTable->currentRow());
        if (nullptr != job)
            job->resetStatus();
    }
    else
    {
        if (KMessageBox::warningContinueCancel(
                    nullptr, i18n("Are you sure you want to reset status of all jobs?"), i18n("Reset job status"),
                    KStandardGuiItem::cont(), KStandardGuiItem::cancel(), "reset_job_status_warning") != KMessageBox::Continue)
        {
            return;
        }

        foreach (SequenceJob * job, jobs)
            job->resetStatus();
    }

    // Also reset the storage count for all jobs
    capturedFramesMap.clear();

    // We're not controlled by the Scheduler, restore progress option
    ignoreJobProgress = Options::alwaysResetSequenceWhenStarting();
}

void Capture::ignoreSequenceHistory()
{
    // This function is called independently from the Scheduler or the UI, so honor the change
    ignoreJobProgress = true;
}

void Capture::syncGUIToJob(SequenceJob * job)
{
    QString rawPrefix;
    bool filterEnabled, expEnabled, tsEnabled;

    job->getPrefixSettings(rawPrefix, filterEnabled, expEnabled, tsEnabled);

    captureExposureN->setValue(job->getExposure());
    captureBinHN->setValue(job->getXBin());
    captureBinVN->setValue(job->getYBin());
    captureFrameXN->setValue(job->getSubX());
    captureFrameYN->setValue(job->getSubY());
    captureFrameWN->setValue(job->getSubW());
    captureFrameHN->setValue(job->getSubH());
    captureFilterS->setCurrentIndex(job->getTargetFilter() - 1);
    captureTypeS->setCurrentIndex(job->getFrameType());
    filePrefixT->setText(rawPrefix);
    fileFilterS->setChecked(filterEnabled);
    fileDurationS->setChecked(expEnabled);
    fileTimestampS->setChecked(tsEnabled);
    captureCountN->setValue(job->getCount());
    captureDelayN->setValue(job->getDelay() / 1000);
    fileScriptT->setText(job->getPostCaptureScript());
    fileUploadModeS->setCurrentIndex(job->getUploadMode());
    fileRemoteDirT->setEnabled(fileUploadModeS->currentIndex() != 0);
    fileRemoteDirT->setText(job->getRemoteDir());
    fileDirectoryT->setText(job->getLocalDir());

    // Temperature Options
    cameraTemperatureS->setChecked(job->getEnforceTemperature());
    if (job->getEnforceTemperature())
        cameraTemperatureN->setValue(job->getTargetTemperature());

    // Flat field options
    calibrationB->setEnabled(job->getFrameType() != FRAME_LIGHT);
    flatFieldDuration  = job->getFlatFieldDuration();
    flatFieldSource    = job->getFlatFieldSource();
    targetADU          = job->getTargetADU();
    targetADUTolerance = job->getTargetADUTolerance();
    wallCoord          = job->getWallCoord();
    preMountPark       = job->isPreMountPark();
    preDomePark        = job->isPreDomePark();

    // Custom Properties
    customPropertiesDialog->setCustomProperties(job->getCustomProperties());

    if (captureISOS)
        captureISOS->setCurrentIndex(job->getISOIndex());

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

    captureFormatS->setCurrentIndex(job->getTransforFormat());

    if (job->getTargetRotation() != Ekos::INVALID_VALUE)
    {
        rotatorSettings->setRotationEnforced(true);
        rotatorSettings->setTargetRotationPA(job->getTargetRotation());
    }
    else
        rotatorSettings->setRotationEnforced(false);

    emit settingsUpdated(getPresetSettings());
}

QJsonObject Capture::getPresetSettings()
{
    QJsonObject settings;

    // Try to get settings value
    // if not found, fallback to camera value
    double gain = -1;
    if (captureGainN->value() != GainSpinSpecialValue)
        gain = captureGainN->value();
    else if (currentCCD && currentCCD->hasGain())
        currentCCD->getGain(&gain);

    double offset = -1;
    if (captureOffsetN->value() != OffsetSpinSpecialValue)
        offset = captureOffsetN->value();
    else if (currentCCD && currentCCD->hasOffset())
        currentCCD->getOffset(&offset);

    int iso = -1;
    if (captureISOS)
        iso = captureISOS->currentIndex();
    else if (currentCCD)
        iso = currentCCD->getChip(ISD::CCDChip::PRIMARY_CCD)->getISOIndex();

    settings.insert("camera", cameraS->currentText());
    settings.insert("fw", filterWheelS->currentText());
    settings.insert("filter", captureFilterS->currentText());
    settings.insert("exp", captureExposureN->value());
    settings.insert("bin", captureBinHN->value());
    settings.insert("iso", iso);
    settings.insert("frameType", captureTypeS->currentIndex());
    settings.insert("format", captureFormatS->currentIndex());
    settings.insert("gain", gain);
    settings.insert("offset", offset);
    settings.insert("temperature", cameraTemperatureN->value());

    return settings;
}

void Capture::selectedJobChanged(QModelIndex current, QModelIndex previous)
{
    Q_UNUSED(previous)
    selectJob(current);
}

void Capture::selectJob(QModelIndex i)
{
    if (i.row() < 0 || (i.row() + 1) > jobs.size())
        return;

    SequenceJob * job = jobs.at(i.row());

    if (job == nullptr)
        return;

    syncGUIToJob(job);

    if (isBusy || jobs.size() < 2)
        return;

    queueUpB->setEnabled(i.row() > 0);
    queueDownB->setEnabled(i.row() + 1 < jobs.size());
}

void Capture::editJob(QModelIndex i)
{
    selectJob(i);
    appendLogText(i18n("Editing job #%1...", i.row() + 1));

    addToQueueB->setIcon(QIcon::fromTheme("dialog-ok-apply"));
    addToQueueB->setToolTip(i18n("Apply job changes."));
    removeFromQueueB->setToolTip(i18n("Cancel job changes."));

    m_JobUnderEdit = true;
}

void Capture::resetJobEdit()
{
    if (m_JobUnderEdit)
        appendLogText(i18n("Editing job canceled."));

    m_JobUnderEdit = false;
    addToQueueB->setIcon(QIcon::fromTheme("list-add"));

    addToQueueB->setToolTip(i18n("Add job to sequence queue"));
    removeFromQueueB->setToolTip(i18n("Remove job from sequence queue"));
}

void Capture::constructPrefix(QString &imagePrefix)
{
    if (imagePrefix.isEmpty() == false)
        imagePrefix += '_';

    imagePrefix += captureTypeS->currentText();

    /*if (fileFilterS->isChecked() && captureFilterS->currentText().isEmpty() == false &&
            captureTypeS->currentText().compare("Bias", Qt::CaseInsensitive) &&
            captureTypeS->currentText().compare("Dark", Qt::CaseInsensitive))*/
    if (fileFilterS->isChecked() && captureFilterS->currentText().isEmpty() == false &&
            (captureTypeS->currentIndex() == FRAME_LIGHT || captureTypeS->currentIndex() == FRAME_FLAT))
    {
        imagePrefix += '_';
        imagePrefix += captureFilterS->currentText();
    }
    if (fileDurationS->isChecked())
    {
        //if (imagePrefix.isEmpty() == false || frameTypeCheck->isChecked())
        imagePrefix += '_';

        double exposureValue = captureExposureN->value();

        // Don't use the locale for exposure value in the capture file name, so that we get a "." as decimal separator
        if (exposureValue == static_cast<int>(exposureValue))
            // Whole number
            imagePrefix += QString::number(captureExposureN->value(), 'd', 0) + QString("_secs");
        else
        {
            // Decimal
            if (captureExposureN->value() >= 0.001)
                imagePrefix += QString::number(captureExposureN->value(), 'f', 3) + QString("_secs");
            else
                imagePrefix += QString::number(captureExposureN->value(), 'f', 6) + QString("_secs");
        }
    }
    if (fileTimestampS->isChecked())
    {
        imagePrefix += SequenceJob::ISOMarker;
    }
}

double Capture::getProgressPercentage()
{
    int totalImageCount     = 0;
    int totalImageCompleted = 0;

    foreach (SequenceJob * job, jobs)
    {
        totalImageCount += job->getCount();
        totalImageCompleted += job->getCompleted();
    }

    if (totalImageCount != 0)
        return ((static_cast<double>(totalImageCompleted) / totalImageCount) * 100.0);
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

    foreach (SequenceJob * job, jobs)
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
        SequenceJob * job = jobs.at(id);
        return job->getStatusString();
    }

    return QString();
}

int Capture::getJobImageProgress(int id)
{
    if (id < jobs.count())
    {
        SequenceJob * job = jobs.at(id);
        return job->getCompleted();
    }

    return -1;
}

int Capture::getJobImageCount(int id)
{
    if (id < jobs.count())
    {
        SequenceJob * job = jobs.at(id);
        return job->getCount();
    }

    return -1;
}

double Capture::getJobExposureProgress(int id)
{
    if (id < jobs.count())
    {
        SequenceJob * job = jobs.at(id);
        return job->getExposeLeft();
    }

    return -1;
}

double Capture::getJobExposureDuration(int id)
{
    if (id < jobs.count())
    {
        SequenceJob * job = jobs.at(id);
        return job->getExposure();
    }

    return -1;
}

int Capture::getJobRemainingTime(SequenceJob * job)
{
    int remaining = static_cast<int>((job->getExposure() + getEstimatedDownloadTime() + job->getDelay() / 1000) *
                                     (job->getCount() - job->getCompleted()));

    if (job->getStatus() == SequenceJob::JOB_BUSY)
        remaining += job->getExposeLeft() + getEstimatedDownloadTime();

    return remaining;
}

int Capture::getOverallRemainingTime()
{
    int remaining = 0;

    foreach (SequenceJob * job, jobs)
        remaining += getJobRemainingTime(job);

    return remaining;
}

int Capture::getActiveJobRemainingTime()
{
    if (activeJob == nullptr)
        return -1;

    return getJobRemainingTime(activeJob);
}

void Capture::setMaximumGuidingDeviation(bool enable, double value)
{
    limitGuideDeviationS->setChecked(enable);
    if (enable)
        limitGuideDeviationN->setValue(value);
}

void Capture::setInSequenceFocus(bool enable, double HFR)
{
    limitFocusHFRS->setChecked(enable);
    if (enable)
        limitFocusHFRN->setValue(HFR);
}

void Capture::setTargetTemperature(double temperature)
{
    cameraTemperatureN->setValue(temperature);
}

void Capture::clearSequenceQueue()
{
    activeJob = nullptr;
    //m_TargetName.clear();
    //stop();
    while (queueTable->rowCount() > 0)
        queueTable->removeRow(0);
    qDeleteAll(jobs);
    jobs.clear();
}

QString Capture::getSequenceQueueStatus()
{
    if (jobs.count() == 0)
        return "Invalid";

    if (isBusy)
        return "Running";

    int idle = 0, error = 0, complete = 0, aborted = 0, running = 0;

    foreach (SequenceJob * job, jobs)
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
        if (m_State == CAPTURE_SUSPENDED)
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

void Capture::processTelescopeNumber(INumberVectorProperty * nvp)
{
    // If it is not ours, return.
    if (nvp->device != currentTelescope->getDeviceName() || strstr(nvp->name, "EQUATORIAL_") == nullptr)
        return;

    switch (meridianFlipStage)
    {
        case MF_NONE:
            break;
        case MF_INITIATED:
        {
            if (nvp->s == IPS_BUSY)
                setMeridianFlipStage(MF_FLIPPING);
        }
        break;

        case MF_FLIPPING:
        {
            if (currentTelescope != nullptr && currentTelescope->isSlewing())
                setMeridianFlipStage(MF_SLEWING);
        }
        break;

        default:
            break;
    }
}

void Capture::processFlipCompleted()
{
    // If dome is syncing, wait until it stops
    if (currentDome && currentDome->isMoving())
        return;

    appendLogText(i18n("Telescope completed the meridian flip."));

    //KNotification::event(QLatin1String("MeridianFlipCompleted"), i18n("Meridian flip is successfully completed"));
    KSNotification::event(QLatin1String("MeridianFlipCompleted"), i18n("Meridian flip is successfully completed"),
                          KSNotification::EVENT_INFO);


    if (m_State == CAPTURE_IDLE || m_State == CAPTURE_ABORTED || m_State == CAPTURE_COMPLETE || m_State == CAPTURE_PAUSED)
    {
        // reset the meridian flip stage and jump directly MF_NONE, since no
        // restart of guiding etc. necessary
        setMeridianFlipStage(MF_NONE);
        return;
    }
}

bool Capture::checkGuidingAfterFlip()
{
    // if no meridian flip has completed, we do not touch guiding
    if (meridianFlipStage < MF_COMPLETED)
        return false;
    // If we're not autoguiding then we're done
    if (resumeGuidingAfterFlip == false)
        return false;

    // if we are waiting for a calibration, start it
    if (m_State < CAPTURE_CALIBRATING)
    {
        appendLogText(i18n("Performing post flip re-calibration and guiding..."));
        secondsLabel->setText(i18n("Calibrating..."));

        m_State = CAPTURE_CALIBRATING;
        emit newStatus(Ekos::CAPTURE_CALIBRATING);

        setMeridianFlipStage(MF_GUIDING);
        emit meridianFlipCompleted();
        return true;
    }
    else if (m_State == CAPTURE_CALIBRATING && (guideState == GUIDE_CALIBRATION_ERROR || guideState == GUIDE_ABORTED))
    {
        // restart guiding after failure
        appendLogText(i18n("Post meridian flip calibration error. Restarting..."));
        emit meridianFlipCompleted();
        return true;
    }
    else
        // in all other cases, do not touch
        return false;
}

bool Capture::checkAlignmentAfterFlip()
{
    // if no meridian flip has completed, we do not touch guiding
    if (meridianFlipStage < MF_COMPLETED)
        return false;
    // If we do not need to align then we're done
    if (resumeAlignmentAfterFlip == false)
        return false;

    // if we are waiting for a calibration, start it
    if (m_State < CAPTURE_ALIGNING)
    {
        appendLogText(i18n("Performing post flip re-alignment..."));
        secondsLabel->setText(i18n("Aligning..."));

        retries = 0;
        m_State   = CAPTURE_ALIGNING;
        emit newStatus(Ekos::CAPTURE_ALIGNING);

        setMeridianFlipStage(MF_ALIGNING);
        //QTimer::singleShot(Options::settlingTime(), [this]() {emit meridialFlipTracked();});
        //emit meridialFlipTracked();
        return true;
    }
    else
        // in all other cases, do not touch
        return false;
}


bool Capture::checkPausing()
{
    if (m_State == CAPTURE_PAUSE_PLANNED)
    {
        appendLogText(i18n("Sequence paused."));
        secondsLabel->setText(i18n("Paused..."));
        m_State = CAPTURE_PAUSED;
        // handle a requested meridian flip
        if (meridianFlipStage != MF_NONE)
            setMeridianFlipStage(MF_READY);
        // pause
        return true;
    }
    // no pause
    return false;
}


bool Capture::checkMeridianFlipReady()
{
    if (currentTelescope == nullptr)
        return false;

    // If active job is taking flat field image at a wall source
    // then do not flip.
    if (activeJob && activeJob->getFrameType() == FRAME_FLAT && activeJob->getFlatFieldSource() == SOURCE_WALL)
        return false;

    if (meridianFlipStage != MF_REQUESTED)
        // if no flip has been requested or is already ongoing
        return false;

    // meridian flip requested or already in action

    // Reset frame if we need to do focusing later on
    if (isInSequenceFocus || (limitRefocusS->isChecked() && getRefocusEveryNTimerElapsedSec() > 0))
        emit resetFocus();

    // signal that meridian flip may take place
    if (meridianFlipStage == MF_REQUESTED)
        setMeridianFlipStage(MF_READY);


    return true;
}

void Capture::checkGuideDeviationTimeout()
{
    if (activeJob && activeJob->getStatus() == SequenceJob::JOB_ABORTED && m_DeviationDetected)
    {
        appendLogText(i18n("Guide module timed out."));
        m_DeviationDetected = false;

        // If capture was suspended, it should be aborted (failed) now.
        if (m_State == CAPTURE_SUSPENDED)
        {
            m_State = CAPTURE_ABORTED;
            emit newStatus(m_State);
        }
    }
}

void Capture::setAlignStatus(AlignState state)
{
    qCDebug(KSTARS_EKOS_CAPTURE) << "setAlignStatus: " << state;
    alignState = state;

    resumeAlignmentAfterFlip = true;

    switch (state)
    {
        case ALIGN_COMPLETE:
            if (meridianFlipStage == MF_ALIGNING)
            {
                appendLogText(i18n("Post flip re-alignment completed successfully."));
                retries = 0;
                // Trigger guiding if necessary.
                if (checkGuidingAfterFlip() == false)
                {
                    // If no guiding is required, the meridian flip is complete
                    setMeridianFlipStage(MF_NONE);
                    m_State = CAPTURE_WAITING;
                }
            }
            break;

        case ALIGN_ABORTED:
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

                    this->m_State = CAPTURE_ALIGNING;
                    emit newStatus(Ekos::CAPTURE_ALIGNING);

                    setMeridianFlipStage(MF_ALIGNING);
                }
            }
            break;

        default:
            break;
    }
}

void Capture::setGuideStatus(GuideState state)
{
    if (state != guideState)
        qCDebug(KSTARS_EKOS_CAPTURE) << "Guiding state changed from" << getGuideStatusString(guideState)
                                     << "to" << getGuideStatusString(state);

    switch (state)
    {
        case GUIDE_IDLE:
            break;

        case GUIDE_GUIDING:
        case GUIDE_CALIBRATION_SUCESS:
            autoGuideReady = true;
            break;

        case GUIDE_ABORTED:
        case GUIDE_CALIBRATION_ERROR:
            processGuidingFailed();
            guideState = state;
            break;

        case GUIDE_DITHERING_SUCCESS:
            qCInfo(KSTARS_EKOS_CAPTURE) << "Dithering succeeded, capture state" << getCaptureStatusString(m_State);
            // do nothing if something happened during dithering
            appendLogText(i18n("Dithering succeeded."));
            if (m_State != CAPTURE_DITHERING)
                break;

            if (Options::guidingSettle() > 0)
            {
                // N.B. Do NOT convert to i18np since guidingRate is DOUBLE value (e.g. 1.36) so we always use plural with that.
                appendLogText(i18n("Dither complete. Resuming in %1 seconds...", Options::guidingSettle()));
                QTimer::singleShot(Options::guidingSettle() * 1000, this, [this]()
                {
                    ditheringState = IPS_OK;
                });
            }
            else
            {
                appendLogText(i18n("Dither complete."));
                ditheringState = IPS_OK;
            }
            break;

        case GUIDE_DITHERING_ERROR:
            qCInfo(KSTARS_EKOS_CAPTURE) << "Dithering failed, capture state" << getCaptureStatusString(m_State);
            if (m_State != CAPTURE_DITHERING)
                break;

            if (Options::guidingSettle() > 0)
            {
                // N.B. Do NOT convert to i18np since guidingRate is DOUBLE value (e.g. 1.36) so we always use plural with that.
                appendLogText(i18n("Warning: Dithering failed. Resuming in %1 seconds...", Options::guidingSettle()));
                // set dithering state to OK after settling time and signal to proceed
                QTimer::singleShot(Options::guidingSettle() * 1000, this, [this]()
                {
                    ditheringState = IPS_OK;
                });
            }
            else
            {
                appendLogText(i18n("Warning: Dithering failed."));
                // signal OK so that capturing may continue although dithering failed
                ditheringState = IPS_OK;
            }

            break;

        default:
            break;
    }

    guideState = state;
}


void Capture::processGuidingFailed()
{
    // If Autoguiding was started before and now stopped, let's abort (unless we're doing a meridian flip)
    if (isGuidingOn() && meridianFlipStage == MF_NONE &&
            ((activeJob && activeJob->getStatus() == SequenceJob::JOB_BUSY) ||
             this->m_State == CAPTURE_SUSPENDED || this->m_State == CAPTURE_PAUSED))
    {
        appendLogText(i18n("Autoguiding stopped. Aborting..."));
        abort();
    }
    else if (meridianFlipStage == MF_GUIDING)
    {
        if (++retries >= 3)
        {
            appendLogText(i18n("Post meridian flip calibration error. Aborting..."));
            abort();
        }
    }
    autoGuideReady = false;
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

    // Check if saturated, then take shorter capture and discard value
    ExpRaw.append(activeJob->getExposure());
    ADURaw.append(value);

    qCDebug(KSTARS_EKOS_CAPTURE) << "Capture: Current ADU = " << value << " targetADU = " << targetADU
                                 << " Exposure Count: " << ExpRaw.count();

    // Most CCDs are quite linear so 1st degree polynomial is quite sufficient
    // But DSLRs can exhibit non-linear response curve and so a 2nd degree polynomial is more appropriate
    if (ExpRaw.count() >= 2)
    {
        if (ExpRaw.count() >= 5)
        {
            double chisq = 0;

            coeff = gsl_polynomial_fit(ADURaw.data(), ExpRaw.data(), ExpRaw.count(), 2, chisq);
            qCDebug(KSTARS_EKOS_CAPTURE) << "Running polynomial fitting. Found " << coeff.size() << " coefficients.";
            if (std::isnan(coeff[0]) || std::isinf(coeff[0]))
            {
                qCDebug(KSTARS_EKOS_CAPTURE) << "Coefficients are invalid.";
                targetADUAlgorithm = ADU_LEAST_SQUARES;
            }
            else
            {
                nextExposure = coeff[0] + (coeff[1] * targetADU) + (coeff[2] * pow(targetADU, 2));
                // If exposure is not valid or does not make sense, then we fall back to least squares
                if (nextExposure < 0 || (nextExposure > ExpRaw.last() || targetADU < ADURaw.last())
                        || (nextExposure < ExpRaw.last() || targetADU > ADURaw.last()))
                {
                    nextExposure = 0;
                    targetADUAlgorithm = ADU_LEAST_SQUARES;
                }
                else
                {
                    targetADUAlgorithm = ADU_POLYNOMIAL;
                    for (size_t i = 0; i < coeff.size(); i++)
                        qCDebug(KSTARS_EKOS_CAPTURE) << "Coeff #" << i << "=" << coeff[i];
                }
            }
        }

        bool looping = false;
        if (ExpRaw.count() >= 10)
        {
            int size = ExpRaw.count();
            looping  = (std::fabs(ExpRaw[size - 1] - ExpRaw[size - 2] < 0.01)) &&
                       (std::fabs(ExpRaw[size - 2] - ExpRaw[size - 3] < 0.01));
            if (looping && targetADUAlgorithm == ADU_POLYNOMIAL)
            {
                qWarning(KSTARS_EKOS_CAPTURE) << "Detected looping in polynomial results. Falling back to llsqr.";
                targetADUAlgorithm = ADU_LEAST_SQUARES;
            }
        }

        // If we get invalid data, let's fall back to llsq
        // Since polyfit can be unreliable at low counts, let's only use it at the 5th exposure
        // if we don't have results already.
        if (targetADUAlgorithm == ADU_LEAST_SQUARES)
        {
            double a = 0, b = 0;
            llsq(ExpRaw, ADURaw, a, b);

            // If we have valid results, let's calculate next exposure
            if (a != 0.0)
            {
                nextExposure = (targetADU - b) / a;
                // If we get invalid value, let's just proceed iteratively
                if (nextExposure < 0)
                    nextExposure = 0;
            }
        }
    }

    if (nextExposure == 0.0)
    {
        if (value < targetADU)
            nextExposure = activeJob->getExposure() * 1.25;
        else
            nextExposure = activeJob->getExposure() * .75;
    }

    qCDebug(KSTARS_EKOS_CAPTURE) << "next flat exposure is" << nextExposure;

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
    xbar = xbar / static_cast<double>(n);
    ybar = ybar / static_cast<double>(n);
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
}

void Capture::setDirty()
{
    m_Dirty = true;
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
    // If HFR limit was set from file, we cannot override it.
    if (fileHFR > 0)
        return;

    limitFocusHFRN->setValue(0);
    //firstAutoFocus = true;
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

    if (currentDome)
    {
        calibrationOptions.parkDomeC->setEnabled(currentDome->canPark());
        calibrationOptions.parkDomeC->setChecked(preDomePark);
    }
    else
        calibrationOptions.parkDomeC->setEnabled(false);

    //connect(calibrationOptions.wallSourceC, SIGNAL(toggled(bool)), calibrationOptions.parkC, &Ekos::Capture::setDisabled(bool)));

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
            calibrationOptions.ADUValue->setValue(static_cast<int>(std::round(targetADU)));
            calibrationOptions.ADUTolerance->setValue(static_cast<int>(std::round(targetADUTolerance)));
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
                KSNotification::error(i18n("Wall coordinates are invalid."));
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

        Options::setCalibrationFlatSourceIndex(flatFieldSource);
        Options::setCalibrationFlatDurationIndex(flatFieldDuration);
        Options::setCalibrationWallAz(wallCoord.az().Degrees());
        Options::setCalibrationWallAlt(wallCoord.alt().Degrees());
        Options::setCalibrationADUValue(static_cast<uint>(std::round(targetADU)));
        Options::setCalibrationADUValueTolerance(static_cast<uint>(std::round(targetADUTolerance)));
    }
}

/**
 * @brief Check all tasks that might be pending before capturing may start.
 *
 * The following checks are executed:
 *  1. Are there any pending jobs that failed? If yes, return with IPS_ALERT.
 *  2. Is the scope cover open (@see checkLightFrameScopeCoverOpen()).
 *  3. Has pausing been initiated (@see checkPausing()).
 *  4. Is a meridian flip already running (@see checkMeridianFlipRunning()) or ready
 *     for execution (@see checkMeridianFlipReady()).
 *  5. Is a post meridian flip alignment running (@see checkAlignmentAfterFlip()).
 *  6. Is post flip guiding required or running (@see checkGuidingAfterFlip().
 *  7. Is the guiding deviation below the expected limit (@see setGuideDeviation(double,double)).
 *  8. Is dithering required or ongoing (@see checkDithering()).
 *  9. Is re-focusing required or ongoing (@see startFocusIfRequired()).
 * 10. Has guiding been resumed and needs to be restarted (@see resumeGuiding())
 *
 * If none of this is true, everything is ready and capturing may be started.
 *
 * @return IPS_OK iff no task is pending, IPS_BUSY otherwise (or IPS_ALERT if a problem occured)
 */
IPState Capture::checkLightFramePendingTasks()
{
    // step 1: did one of the pending jobs fail or has the user aborted the capture?
    if (m_State == CAPTURE_ABORTED)
        return IPS_ALERT;

    // step 2: ensure that the scope cover is open and wait until it's open
    IPState coverState = checkLightFrameScopeCoverOpen();
    if (coverState != IPS_OK)
        return coverState;

    // step 3: check if pausing has been requested
    if (checkPausing() == true)
    {
        pauseFunction = &Capture::startNextExposure;
        return IPS_BUSY;
    }

    // step 4: check if meridian flip is already running or ready for execution
    if (checkMeridianFlipRunning() || checkMeridianFlipReady())
        return IPS_BUSY;

    // step 5: check if post flip alignment is running
    if (m_State == CAPTURE_ALIGNING || checkAlignmentAfterFlip())
        return IPS_BUSY;

    // step 6: check if post flip guiding is running
    // MF_NONE is set as soon as guiding is running and the guide deviation is below the limit
    if ((m_State == CAPTURE_CALIBRATING && guideState != GUIDE_GUIDING) || checkGuidingAfterFlip())
        return IPS_BUSY;

    // step 7: in case that a meridian flip has been completed and a guide deviation limit is set, we wait
    //         until the guide deviation is reported to be below the limit (@see setGuideDeviation(double, double)).
    //         Otherwise the meridian flip is complete
    if (m_State == CAPTURE_CALIBRATING && meridianFlipStage == MF_GUIDING)
    {
        if (limitGuideDeviationS->isChecked() == true)
            return IPS_BUSY;
        else
            setMeridianFlipStage(MF_NONE);
    }

    // step 8: check if dithering is required or running
    if ((m_State == CAPTURE_DITHERING && ditheringState != IPS_OK) || checkDithering())
        return IPS_BUSY;

    // step 9: check if re-focusing is required
    //         Needs to be checked after dithering checks to avoid dithering in parallel
    //         to focusing, since @startFocusIfRequired() might change its value over time
    if ((m_State == CAPTURE_FOCUSING  && focusState != FOCUS_COMPLETE) || startFocusIfRequired())
        return IPS_BUSY;

    // step 10: resume guiding if it was suspended
    if (guideState == GUIDE_SUSPENDED)
    {
        appendLogText(i18n("Autoguiding resumed."));
        emit resumeGuiding();
        // No need to return IPS_BUSY here, we can continue immediately.
        // In the case that the capturing sequence has a guiding limit,
        // capturing will be interrupted by setGuideDeviation().
    }

    // everything is ready for capturing light frames
    calibrationStage = CAL_PRECAPTURE_COMPLETE;

    return IPS_OK;

}

IPState Capture::checkLightFrameScopeCoverOpen()
{
    switch (activeJob->getFlatFieldSource())
    {
        // All these are considered MANUAL when it comes to light frames
        case SOURCE_MANUAL:
        case SOURCE_DAWN_DUSK:
        case SOURCE_WALL:
            // If telescopes were MANUALLY covered before
            // we need to manually uncover them.
            if (m_TelescopeCoveredDarkExposure || m_TelescopeCoveredFlatExposure)
            {
                // Uncover telescope
                // N.B. This operation cannot be autonomous
                //        if (KMessageBox::warningContinueCancel(
                //                    nullptr, i18n("Remove cover from the telescope in order to continue."), i18n("Telescope Covered"),
                //                    KStandardGuiItem::cont(), KStandardGuiItem::cancel(),
                //                    "uncover_scope_dialog_notification", KMessageBox::WindowModal | KMessageBox::Notify) == KMessageBox::Cancel)
                //        {
                //            return IPS_ALERT;
                //        }

                // If we already asked for confirmation and waiting for it
                // let us see if the confirmation is fulfilled
                // otherwise we return.
                if (calibrationCheckType == CAL_CHECK_CONFIRMATION)
                    return IPS_BUSY;

                // Otherwise, we ask user to confirm manually
                calibrationCheckType = CAL_CHECK_CONFIRMATION;

                // Continue
                connect(KSMessageBox::Instance(), &KSMessageBox::accepted, this, [this]()
                {
                    //QObject::disconnect(KSMessageBox::Instance(), &KSMessageBox::accepted, this, nullptr);
                    KSMessageBox::Instance()->disconnect(this);
                    m_TelescopeCoveredDarkExposure = false;
                    m_TelescopeCoveredFlatExposure = false;
                    calibrationCheckType = CAL_CHECK_TASK;
                });

                // Cancel
                connect(KSMessageBox::Instance(), &KSMessageBox::rejected, this, [&]()
                {
                    KSMessageBox::Instance()->disconnect(this);
                    calibrationCheckType = CAL_CHECK_TASK;
                    abort();
                });

                KSMessageBox::Instance()->warningContinueCancel(i18n("Remove cover from the telescope in order to continue."),
                        i18n("Telescope Covered"),
                        Options::manualCoverTimeout());

                return IPS_BUSY;
            }
            break;
        case SOURCE_FLATCAP:
        case SOURCE_DARKCAP:
            if (currentDustCap == nullptr)
            {
                qCWarning(KSTARS_EKOS_CAPTURE) << "Skipping flat/dark cap since it is not connected.";
                break;
            }

            //            {
            //                appendLogText(i18n("Cap device is missing but the job requires flat or dark cap device."));
            //                return IPS_ALERT;
            //            }

            // If dust cap HAS light and light is ON, then turn it off.
            if (currentDustCap->hasLight() && currentDustCap->isLightOn() == true)
            {
                dustCapLightEnabled = false;
                currentDustCap->SetLightEnabled(false);
            }

            // If cap is parked, we need to unpark it
            if (calibrationStage < CAL_DUSTCAP_UNPARKING && currentDustCap->isParked())
            {
                if (currentDustCap->UnPark())
                {
                    calibrationStage = CAL_DUSTCAP_UNPARKING;
                    appendLogText(i18n("Unparking dust cap..."));
                    return IPS_BUSY;
                }
                else
                {
                    appendLogText(i18n("Unparking dust cap failed, aborting..."));
                    abort();
                    return IPS_ALERT;
                }
            }

            // Wait until cap is unparked
            if (calibrationStage == CAL_DUSTCAP_UNPARKING)
            {
                if (currentDustCap->isUnParked() == false)
                    return IPS_BUSY;
                else
                {
                    calibrationStage = CAL_DUSTCAP_UNPARKED;
                    appendLogText(i18n("Dust cap unparked."));
                }
            }
            break;
    }
    // scope cover open (or no scope cover)
    return IPS_OK;
}



IPState Capture::checkDarkFramePendingTasks()
{
    QStringList shutterfulCCDs  = Options::shutterfulCCDs();
    QStringList shutterlessCCDs = Options::shutterlessCCDs();
    QString deviceName = currentCCD->getDeviceName();

    bool hasShutter   = shutterfulCCDs.contains(deviceName);
    bool hasNoShutter = shutterlessCCDs.contains(deviceName) || (captureISOS && captureISOS->count() > 0);

    // If we have no information, we ask before we proceed.
    if (hasShutter == false && hasNoShutter == false)
    {
        // Awaiting user input
        if (calibrationCheckType == CAL_CHECK_CONFIRMATION)
            return IPS_BUSY;

        //        // This action cannot be autonomous
        //        if (KMessageBox::questionYesNo(nullptr, i18n("Does %1 have a shutter?", deviceName),
        //                                       i18n("Dark Exposure")) == KMessageBox::Yes)
        //        {
        //            hasNoShutter = false;
        //            shutterfulCCDs.append(deviceName);
        //            Options::setShutterfulCCDs(shutterfulCCDs);
        //        }
        //        else
        //        {
        //            hasNoShutter = true;
        //            shutterlessCCDs.append(deviceName);
        //            Options::setShutterlessCCDs(shutterlessCCDs);
        //        }

        connect(KSMessageBox::Instance(), &KSMessageBox::accepted, this, [&]()
        {
            //QObject::disconnect(KSMessageBox::Instance(), &KSMessageBox::accepted, this, nullptr);
            KSMessageBox::Instance()->disconnect(this);
            QStringList shutterfulCCDs  = Options::shutterfulCCDs();
            QString deviceName = currentCCD->getDeviceName();
            shutterfulCCDs.append(deviceName);
            Options::setShutterfulCCDs(shutterfulCCDs);
            calibrationCheckType = CAL_CHECK_TASK;
        });
        connect(KSMessageBox::Instance(), &KSMessageBox::rejected, this, [&]()
        {
            //QObject::disconnect(KSMessageBox::Instance(), &KSMessageBox::rejected, this, nullptr);
            KSMessageBox::Instance()->disconnect(this);
            QStringList shutterlessCCDs = Options::shutterlessCCDs();
            QString deviceName = currentCCD->getDeviceName();
            shutterlessCCDs.append(deviceName);
            Options::setShutterlessCCDs(shutterlessCCDs);
            calibrationCheckType = CAL_CHECK_TASK;
        });

        calibrationCheckType = CAL_CHECK_CONFIRMATION;

        KSMessageBox::Instance()->questionYesNo(i18n("Does %1 have a shutter?", deviceName),
                                                i18n("Dark Exposure"));

        return IPS_BUSY;
    }

    switch (activeJob->getFlatFieldSource())
    {
        // All these are manual when it comes to dark frames
        case SOURCE_MANUAL:
        case SOURCE_DAWN_DUSK:
            // For cameras without a shutter, we need to ask the user to cover the telescope
            // if the telescope is not already covered.
            if (hasNoShutter && !m_TelescopeCoveredDarkExposure)
            {
                if (calibrationCheckType == CAL_CHECK_CONFIRMATION)
                    return IPS_BUSY;

                // Continue
                connect(KSMessageBox::Instance(), &KSMessageBox::accepted, this, [&]()
                {
                    //QObject::disconnect(KSMessageBox::Instance(), &KSMessageBox::accepted, this, nullptr);
                    KSMessageBox::Instance()->disconnect(this);
                    m_TelescopeCoveredDarkExposure = true;
                    m_TelescopeCoveredFlatExposure = false;
                    calibrationCheckType = CAL_CHECK_TASK;
                });

                // Cancel
                connect(KSMessageBox::Instance(), &KSMessageBox::rejected, this, [&]()
                {
                    //QObject::disconnect(KSMessageBox::Instance(), &KSMessageBox::rejected, this, nullptr);
                    KSMessageBox::Instance()->disconnect(this);
                    calibrationCheckType = CAL_CHECK_TASK;
                    abort();
                });

                //            if (KMessageBox::warningContinueCancel(
                //                        nullptr, i18n("Cover the telescope in order to take a dark exposure."), i18n("Dark Exposure"),
                //                        KStandardGuiItem::cont(), KStandardGuiItem::cancel(),
                //                        "cover_scope_dialog_notification", KMessageBox::WindowModal | KMessageBox::Notify) == KMessageBox::Cancel)
                //            {
                //                abort();
                //                return IPS_ALERT;
                //            }

                calibrationCheckType = CAL_CHECK_CONFIRMATION;

                KSMessageBox::Instance()->warningContinueCancel(i18n("Cover the telescope in order to take a dark exposure.")
                        , i18n("Dark Exposure"),
                        Options::manualCoverTimeout());

                return IPS_BUSY;
            }
            break;
        case SOURCE_FLATCAP:
        case SOURCE_DARKCAP:
            if (currentDustCap == nullptr)
            {
                qCWarning(KSTARS_EKOS_CAPTURE) << "Skipping flat/dark cap since it is not connected.";
                break;
            }
            // When using a cap, we need to park, if not already parked.
            // Need to turn off light, if light exists and was on.
            //            if (!currentDustCap)
            //            {
            //                appendLogText(i18n("Cap device is missing but the job requires flat or dark cap device."));
            //                abort();
            //                return IPS_ALERT;
            //            }

            // If cap is not park, park it
            if (calibrationStage < CAL_DUSTCAP_PARKING && currentDustCap->isParked() == false)
            {
                if (currentDustCap->Park())
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

            // Wait until cap is parked
            if (calibrationStage == CAL_DUSTCAP_PARKING)
            {
                if (currentDustCap->isParked() == false)
                    return IPS_BUSY;
                else
                {
                    calibrationStage = CAL_DUSTCAP_PARKED;
                    appendLogText(i18n("Dust cap parked."));
                }
            }

            // Turn off light if it exists and was on.
            if (currentDustCap->hasLight() && currentDustCap->isLightOn() == true)
            {
                dustCapLightEnabled = false;
                currentDustCap->SetLightEnabled(false);
            }
            break;

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
                        // Disable mount tracking if supported by the driver.
                        currentTelescope->setTrackEnabled(false);
                        calibrationStage = CAL_SLEWING_COMPLETE;
                        appendLogText(i18n("Slew to wall position complete."));
                    }
                    else
                        return IPS_BUSY;
                }

                if (currentLightBox && currentLightBox->isLightOn() == true)
                {
                    lightBoxLightEnabled = false;
                    currentLightBox->SetLightEnabled(false);
                }
            }
            break;
    }

    calibrationStage = CAL_PRECAPTURE_COMPLETE;

    return IPS_OK;
}

IPState Capture::checkFlatFramePendingTasks()
{
    switch (activeJob->getFlatFieldSource())
    {
        case SOURCE_MANUAL:
            // Manual mode we need to cover mount with evenly illuminated field.
            if (m_TelescopeCoveredFlatExposure == false)
            {
                if (calibrationCheckType == CAL_CHECK_CONFIRMATION)
                    return IPS_BUSY;
                // This action cannot be autonomous
                //            if (KMessageBox::warningContinueCancel(
                //                        nullptr, i18n("Cover telescope with evenly illuminated light source."), i18n("Flat Frame"),
                //                        KStandardGuiItem::cont(), KStandardGuiItem::cancel(),
                //                        "flat_light_cover_dialog_notification", KMessageBox::WindowModal | KMessageBox::Notify) == KMessageBox::Cancel)
                //            {
                //                abort();
                //                return IPS_ALERT;
                //            }
                //            m_TelescopeCoveredFlatExposure = true;
                //            m_TelescopeCoveredDarkExposure = false;

                // Continue
                connect(KSMessageBox::Instance(), &KSMessageBox::accepted, this, [&]()
                {
                    //QObject::disconnect(KSMessageBox::Instance(), &KSMessageBox::accepted, this, nullptr);
                    KSMessageBox::Instance()->disconnect(this);
                    m_TelescopeCoveredFlatExposure = true;
                    m_TelescopeCoveredDarkExposure = false;
                    calibrationCheckType = CAL_CHECK_TASK;
                });

                // Cancel
                connect(KSMessageBox::Instance(), &KSMessageBox::rejected, this, [&]()
                {
                    //QObject::disconnect(KSMessageBox::Instance(), &KSMessageBox::rejected, this, nullptr);
                    KSMessageBox::Instance()->disconnect(this);
                    calibrationCheckType = CAL_CHECK_TASK;
                    abort();
                });

                calibrationCheckType = CAL_CHECK_CONFIRMATION;

                KSMessageBox::Instance()->warningContinueCancel(i18n("Cover telescope with evenly illuminated light source."),
                        i18n("Flat Frame"),
                        Options::manualCoverTimeout());

                return IPS_BUSY;
            }
            break;
        // Not implemented.
        case SOURCE_DAWN_DUSK:
            break;
        case SOURCE_FLATCAP:
            if (currentDustCap == nullptr)
            {
                qCWarning(KSTARS_EKOS_CAPTURE) << "Skipping flat/dark cap since it is not connected.";
                break;
            }

            //            if (!currentDustCap)
            //            {
            //                appendLogText(i18n("Cap device is missing but the job requires flat cap device."));
            //                abort();
            //                return IPS_ALERT;
            //            }

            // If cap is not park, park it
            if (calibrationStage < CAL_DUSTCAP_PARKING && currentDustCap->isParked() == false)
            {
                if (currentDustCap->Park())
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

            // Wait until cap is parked
            if (calibrationStage == CAL_DUSTCAP_PARKING)
            {
                if (currentDustCap->isParked() == false)
                    return IPS_BUSY;
                else
                {
                    calibrationStage = CAL_DUSTCAP_PARKED;
                    appendLogText(i18n("Dust cap parked."));
                }
            }

            // If light is not on, turn it on.
            if (currentDustCap->hasLight() && currentDustCap->isLightOn() == false)
            {
                dustCapLightEnabled = true;
                currentDustCap->SetLightEnabled(true);
            }
            break;
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
                        // Disable mount tracking if supported by the driver.
                        currentTelescope->setTrackEnabled(false);
                        calibrationStage = CAL_SLEWING_COMPLETE;
                        appendLogText(i18n("Slew to wall position complete."));
                    }
                    else
                        return IPS_BUSY;
                }

                if (currentLightBox)
                {
                    // Check if we have a light box to turn on
                    if (activeJob->getFrameType() == FRAME_FLAT && currentLightBox->isLightOn() == false)
                    {
                        lightBoxLightEnabled = true;
                        currentLightBox->SetLightEnabled(true);
                    }
                    else if (activeJob->getFrameType() != FRAME_FLAT && currentLightBox->isLightOn() == true)
                    {
                        lightBoxLightEnabled = false;
                        currentLightBox->SetLightEnabled(false);
                    }
                }
            }
            break;


        case SOURCE_DARKCAP:
            if (currentDustCap == nullptr)
            {
                qCWarning(KSTARS_EKOS_CAPTURE) << "Skipping flat/dark cap since it is not connected.";
                break;
            }

            //            if (!currentDustCap)
            //            {
            //                appendLogText(i18n("Cap device is missing but the job requires dark cap device."));
            //                abort();
            //                return IPS_ALERT;
            //            }
            // If cap is parked, unpark it since dark cap uses external light source.
            if (calibrationStage < CAL_DUSTCAP_UNPARKING && currentDustCap->isParked() == true)
            {
                if (currentDustCap->UnPark())
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

            // Wait until cap is unparked
            if (calibrationStage == CAL_DUSTCAP_UNPARKING)
            {
                if (currentDustCap->isUnParked() == false)
                    return IPS_BUSY;
                else
                {
                    calibrationStage = CAL_DUSTCAP_UNPARKED;
                    appendLogText(i18n("Dust cap unparked."));
                }
            }

            // If light is off, turn it on.
            if (currentDustCap->hasLight() && currentDustCap->isLightOn() == false)
            {
                dustCapLightEnabled = true;
                currentDustCap->SetLightEnabled(true);
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
    if (preDomePark && currentDome)
    {
        if (calibrationStage < CAL_DOME_PARKING && currentDome->isParked() == false)
        {
            if (currentDome->Park())
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
            if (currentDome->isParked() == false)
                return IPS_BUSY;
            else
            {
                calibrationStage = CAL_DOME_PARKED;
                appendLogText(i18n("Dome parked."));
            }
        }
    }

    // If we used AUTOFOCUS before for a specific frame (e.g. Lum)
    // then the absolute focus position for Lum is recorded in the filter manager
    // when we take flats again, we always go back to the same focus position as the light frames to ensure
    // near identical focus for both frames.
    if (activeJob->getFrameType() == FRAME_FLAT &&
            m_AutoFocusReady &&
            currentFilter != nullptr &&
            Options::flatSyncFocus())
    {
        if (filterManager->syncAbsoluteFocusPosition(activeJob->getTargetFilter() - 1) == false)
            return IPS_BUSY;
    }

    calibrationStage = CAL_PRECAPTURE_COMPLETE;

    return IPS_OK;

}

/**
 * @brief Execute the tasks that need to be completed before capturing may start.
 *
 * For light frames, checkLightFramePendingTasks() is called, for bias and dark frames
 * checkDarkFramePendingTasks() and for flat frames checkFlatFramePendingTasks().
 *
 * @return IPS_OK if all necessary tasks have been completed
 */
IPState Capture::processPreCaptureCalibrationStage()
{
    // If we are currently guide and the frame is NOT a light frame, then we shopld suspend.
    // N.B. The guide camera could be on its own scope unaffected but it doesn't hurt to stop
    // guiding since it is no longer used anyway.
    if (activeJob->getFrameType() != FRAME_LIGHT && guideState == GUIDE_GUIDING)
    {
        appendLogText(i18n("Autoguiding suspended."));
        emit suspendGuiding();
    }

    // Run necessary tasks for each frame type
    switch (activeJob->getFrameType())
    {
        case FRAME_LIGHT:
            return checkLightFramePendingTasks();

        case FRAME_BIAS:
        case FRAME_DARK:
            return checkDarkFramePendingTasks();

        case FRAME_FLAT:
            return checkFlatFramePendingTasks();
    }

    return IPS_OK;
}

bool Capture::processPostCaptureCalibrationStage()
{
    // If there are no more images to capture, do not bother calculating next exposure
    if (calibrationStage == CAL_CALIBRATION_COMPLETE)
        if (activeJob && activeJob->getCount() <= activeJob->getCompleted())
            return true;

    // Check if we need to do flat field slope calculation if the user specified a desired ADU value
    if (activeJob->getFrameType() == FRAME_FLAT && activeJob->getFlatFieldDuration() == DURATION_ADU &&
            activeJob->getTargetADU() > 0)
    {
        if (Options::useFITSViewer() == false)
        {
            Options::setUseFITSViewer(true);
            qCInfo(KSTARS_EKOS_CAPTURE) << "Enabling FITS Viewer...";
        }

        FITSData * image_data   = nullptr;
        FITSView * currentImage = targetChip->getImageView(FITS_NORMAL);

        if (currentImage)
        {
            image_data        = currentImage->getImageData();
            double currentADU = image_data->getADU();
            bool outOfRange = false, saturated = false;

            switch (image_data->bpp())
            {
                case 8:
                    if (activeJob->getTargetADU() > UINT8_MAX)
                        outOfRange = true;
                    else if (currentADU / UINT8_MAX > 0.95)
                        saturated = true;
                    break;

                case 16:
                    if (activeJob->getTargetADU() > UINT16_MAX)
                        outOfRange = true;
                    else if (currentADU / UINT16_MAX > 0.95)
                        saturated = true;
                    break;

                case 32:
                    if (activeJob->getTargetADU() > UINT32_MAX)
                        outOfRange = true;
                    else if (currentADU / UINT32_MAX > 0.95)
                        saturated = true;
                    break;

                default:
                    break;
            }

            if (outOfRange)
            {
                appendLogText(i18n("Flat calibration failed. Captured image is only %1-bit while requested ADU is %2.",
                                   QString::number(image_data->bpp())
                                   , QString::number(activeJob->getTargetADU(), 'f', 2)));
                abort();
                return false;
            }
            else if (saturated)
            {
                double nextExposure = activeJob->getExposure() * 0.1;
                nextExposure = qBound(captureExposureN->minimum(), nextExposure, captureExposureN->maximum());

                appendLogText(i18n("Current image is saturated (%1). Next exposure is %2 seconds.",
                                   QString::number(currentADU, 'f', 0), QString("%L1").arg(nextExposure, 0, 'f', 6)));

                calibrationStage = CAL_CALIBRATION;
                activeJob->setExposure(nextExposure);
                activeJob->setPreview(true);
                rememberUploadMode = activeJob->getUploadMode();
                currentCCD->setUploadMode(ISD::CCD::UPLOAD_CLIENT);
                startNextExposure();
                return false;
            }

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
                    captureExposureN->setValue(activeJob->getExposure());
                    QString imagePrefix = activeJob->property("rawPrefix").toString();
                    constructPrefix(imagePrefix);
                    activeJob->setFullPrefix(imagePrefix);
                    seqPrefix = imagePrefix;
                    currentCCD->setSeqPrefix(imagePrefix);

                    currentCCD->updateUploadSettings(activeJob->getRemoteDir() + activeJob->getDirectoryPostfix());

                    calibrationStage = CAL_CALIBRATION_COMPLETE;
                    startNextExposure();
                    return false;
                }

                return true;
            }

            double nextExposure = -1;

            // If value is saturated, try to reduce it to valid range first
            if (std::fabs(image_data->getMax(0) - image_data->getMin(0)) < 10)
                nextExposure = activeJob->getExposure() * 0.5;
            else
                nextExposure = setCurrentADU(currentADU);

            if (nextExposure <= 0 || std::isnan(nextExposure))
            {
                appendLogText(
                    i18n("Unable to calculate optimal exposure settings, please capture the flats manually."));
                //activeJob->setTargetADU(0);
                //targetADU = 0;
                abort();
                return false;
            }

            // Limit to minimum and maximum values
            nextExposure = qBound(captureExposureN->minimum(), nextExposure, captureExposureN->maximum());

            appendLogText(i18n("Current ADU is %1 Next exposure is %2 seconds.", QString::number(currentADU, 'f', 0),
                               QString("%L1").arg(nextExposure, 0, 'f', 6)));

            calibrationStage = CAL_CALIBRATION;
            activeJob->setExposure(nextExposure);
            activeJob->setPreview(true);
            rememberUploadMode = activeJob->getUploadMode();
            currentCCD->setUploadMode(ISD::CCD::UPLOAD_CLIENT);

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
    emit newSequenceImage(file, QString());
}

/*
void Capture::startPostFilterAutoFocus()
{
    if (focusState >= FOCUS_PROGRESS || state == CAPTURE_FOCUSING)
        return;

    secondsLabel->setText(i18n("Focusing..."));

    state = CAPTURE_FOCUSING;
    emit newStatus(Ekos::CAPTURE_FOCUSING);

    appendLogText(i18n("Post filter change Autofocus..."));

    // Force it to always run autofocus routine
    emit checkFocus(0.1);
}
*/

void Capture::postScriptFinished(int exitCode, QProcess::ExitStatus status)
{
    Q_UNUSED(status)

    appendLogText(i18n("Post capture script finished with code %1.", exitCode));

    // If we're done, proceed to completion.
    if (activeJob->getCount() <= activeJob->getCompleted())
    {
        processJobCompletion();
    }
    // Else check if meridian condition is met.
    else if (checkMeridianFlipReady())
    {
        appendLogText(i18n("Processing meridian flip..."));
    }
    // Then if nothing else, just resume sequence.
    else
    {
        appendLogText(i18n("Resuming sequence..."));
        resumeSequence();
    }
}


void Capture::toggleVideo(bool enabled)
{
    if (currentCCD == nullptr)
        return;

    if (currentCCD->isBLOBEnabled() == false)
    {
        if (Options::guiderType() != Ekos::Guide::GUIDE_INTERNAL)
            currentCCD->setBLOBEnabled(true);
        else
        {
            connect(KSMessageBox::Instance(), &KSMessageBox::accepted, this, [this, enabled]()
            {
                //QObject::disconnect(KSMessageBox::Instance(), &KSMessageBox::accepted, this, nullptr);
                KSMessageBox::Instance()->disconnect(this);
                currentCCD->setBLOBEnabled(true);
                currentCCD->setVideoStreamEnabled(enabled);
            });

            KSMessageBox::Instance()->questionYesNo(i18n("Image transfer is disabled for this camera. Would you like to enable it?"),
                                                    i18n("Image Transfer"), 15);

            return;
        }
    }

    currentCCD->setVideoStreamEnabled(enabled);
}

void Capture::setVideoStreamEnabled(bool enabled)
{
    if (enabled)
    {
        liveVideoB->setChecked(true);
        liveVideoB->setIcon(QIcon::fromTheme("camera-on"));
        //liveVideoB->setStyleSheet("color:red;");
    }
    else
    {
        liveVideoB->setChecked(false);
        liveVideoB->setIcon(QIcon::fromTheme("camera-ready"));
        //liveVideoB->setStyleSheet(QString());
    }
}

void Capture::setMountStatus(ISD::Telescope::Status newState)
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
    for (auto &o : m_observerList)
        observers << QString("%1 %2").arg(o->name(), o->surname());

    QDialog observersDialog(this);
    observersDialog.setWindowTitle(i18n("Select Current Observer"));

    QLabel label(i18n("Current Observer:"));

    QComboBox observerCombo(&observersDialog);
    observerCombo.addItems(observers);
    observerCombo.setCurrentText(m_ObserverName);
    observerCombo.setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    QPushButton manageObserver(&observersDialog);
    manageObserver.setFixedSize(QSize(32, 32));
    manageObserver.setIcon(QIcon::fromTheme("document-edit"));
    manageObserver.setAttribute(Qt::WA_LayoutUsesWidgetRect);
    manageObserver.setToolTip(i18n("Manage Observers"));
    connect(&manageObserver, &QPushButton::clicked, this, [&]()
    {
        ObserverAdd add;
        add.exec();

        QList<OAL::Observer *> m_observerList;
        KStars::Instance()->data()->userdb()->GetAllObservers(m_observerList);
        QStringList observers;
        for (auto &o : m_observerList)
            observers << QString("%1 %2").arg(o->name(), o->surname());

        observerCombo.clear();
        observerCombo.addItems(observers);
        observerCombo.setCurrentText(m_ObserverName);

    });

    QHBoxLayout * layout = new QHBoxLayout;
    layout->addWidget(&label);
    layout->addWidget(&observerCombo);
    layout->addWidget(&manageObserver);

    observersDialog.setLayout(layout);

    observersDialog.exec();

    m_ObserverName = observerCombo.currentText();

    Options::setDefaultObserver(m_ObserverName);
}


void Capture::startRefocusTimer(bool forced)
{
    /* If refocus is requested, only restart timer if not already running in order to keep current elapsed time since last refocus */
    if (limitRefocusS->isChecked())
    {
        // How much time passed since we last started the time
        long elapsedSecs = refocusEveryNTimer.elapsed() / 1000;
        // How many seconds do we wait for between focusing (60 mins ==> 3600 secs)
        int totalSecs   = limitRefocusN->value() * 60;

        if (!refocusEveryNTimer.isValid() || forced)
        {
            appendLogText(i18n("Ekos will refocus in %1 seconds.", totalSecs));
            refocusEveryNTimer.restart();
        }
        else if (elapsedSecs < totalSecs)
        {
            //appendLogText(i18n("Ekos will refocus in %1 seconds, last procedure was %2 seconds ago.", refocusEveryNTimer.elapsed()/1000-refocusEveryNTimer.elapsed()*60, refocusEveryNTimer.elapsed()/1000));
            appendLogText(i18n("Ekos will refocus in %1 seconds, last procedure was %2 seconds ago.", totalSecs - elapsedSecs,
                               elapsedSecs));
        }
        else
        {
            appendLogText(i18n("Ekos will refocus as soon as possible, last procedure was %1 seconds ago.", elapsedSecs));
        }
    }
}

int Capture::getRefocusEveryNTimerElapsedSec()
{
    /* If timer isn't valid, consider there is no focus to be done, that is, that focus was just done */
    return refocusEveryNTimer.isValid() ? static_cast<int>(refocusEveryNTimer.elapsed() / 1000) : 0;
}

void Capture::setAlignResults(double orientation, double ra, double de, double pixscale)
{
    Q_UNUSED(orientation)
    Q_UNUSED(ra)
    Q_UNUSED(de)
    Q_UNUSED(pixscale)

    if (currentRotator == nullptr)
        return;

    rotatorSettings->refresh();
}

void Capture::setFilterManager(const QSharedPointer<FilterManager> &manager)
{
    filterManager = manager;
    connect(filterManagerB, &QPushButton::clicked, [this]()
    {
        filterManager->show();
        filterManager->raise();
    });

    connect(filterManager.data(), &FilterManager::ready, [this]()
    {
        m_CurrentFilterPosition = filterManager->getFilterPosition();
        // Due to race condition,
        focusState = FOCUS_IDLE;
        if (activeJob)
            activeJob->setCurrentFilter(m_CurrentFilterPosition);

    }
           );

    connect(filterManager.data(), &FilterManager::failed, [this]()
    {
        if (activeJob)
        {
            appendLogText(i18n("Filter operation failed."));
            abort();
        }
    }
           );

    connect(filterManager.data(), &FilterManager::newStatus, [this](Ekos::FilterState filterState)
    {
        filterManagerState = filterState;
        if (m_State == CAPTURE_CHANGING_FILTER)
        {
            secondsLabel->setText(Ekos::getFilterStatusString(filterState));
            switch (filterState)
            {
                case FILTER_OFFSET:
                    appendLogText(i18n("Changing focus offset by %1 steps...", filterManager->getTargetFilterOffset()));
                    break;

                case FILTER_CHANGE:
                    appendLogText(i18n("Changing filter to %1...", captureFilterS->itemText(filterManager->getTargetFilterPosition() - 1)));
                    break;

                case FILTER_AUTOFOCUS:
                    appendLogText(i18n("Auto focus on filter change..."));
                    clearAutoFocusHFR();
                    break;

                default:
                    break;
            }
        }
    });

    connect(filterManager.data(), &FilterManager::labelsChanged, this, [this]()
    {
        captureFilterS->clear();
        captureFilterS->addItems(filterManager->getFilterLabels());
        m_CurrentFilterPosition = filterManager->getFilterPosition();
        captureFilterS->setCurrentIndex(m_CurrentFilterPosition - 1);
    });
    connect(filterManager.data(), &FilterManager::positionChanged, this, [this]()
    {
        m_CurrentFilterPosition = filterManager->getFilterPosition();
        captureFilterS->setCurrentIndex(m_CurrentFilterPosition - 1);
    });
}

void Capture::addDSLRInfo(const QString &model, uint32_t maxW, uint32_t maxH, double pixelW, double pixelH)
{
    // Check if model already exists
    auto pos = std::find_if(DSLRInfos.begin(), DSLRInfos.end(), [model](QMap<QString, QVariant> &oneDSLRInfo)
    {
        return (oneDSLRInfo["Model"] == model);
    });

    if (pos != DSLRInfos.end())
    {
        KStarsData::Instance()->userdb()->DeleteDSLRInfo(model);
        DSLRInfos.removeOne(*pos);
    }

    QMap<QString, QVariant> oneDSLRInfo;
    oneDSLRInfo["Model"] = model;
    oneDSLRInfo["Width"] = maxW;
    oneDSLRInfo["Height"] = maxH;
    oneDSLRInfo["PixelW"] = pixelW;
    oneDSLRInfo["PixelH"] = pixelH;

    KStarsData::Instance()->userdb()->AddDSLRInfo(oneDSLRInfo);
    KStarsData::Instance()->userdb()->GetAllDSLRInfos(DSLRInfos);

    updateFrameProperties();
    resetFrame();
    syncDSLRToTargetChip(model);

    // In case the dialog was opened, let's close it
    if (dslrInfoDialog)
        dslrInfoDialog.reset();
}

bool Capture::isModelinDSLRInfo(const QString &model)
{
    auto pos = std::find_if(DSLRInfos.begin(), DSLRInfos.end(), [model](QMap<QString, QVariant> &oneDSLRInfo)
    {
        return (oneDSLRInfo["Model"] == model);
    });

    return (pos != DSLRInfos.end());
}

#if 0
void Capture::syncDriverToDSLRLimits()
{
    if (targetChip == nullptr)
        return;

    QString model(currentCCD->getDeviceName());

    // Check if model already exists
    auto pos = std::find_if(DSLRInfos.begin(), DSLRInfos.end(), [model](QMap<QString, QVariant> &oneDSLRInfo)
    {
        return (oneDSLRInfo["Model"] == model);
    });

    if (pos != DSLRInfos.end())
        targetChip->setImageInfo((*pos)["Width"].toInt(), (*pos)["Height"].toInt(), (*pos)["PixelW"].toDouble(),
                                 (*pos)["PixelH"].toDouble(), 8);
}
#endif

void Capture::cullToDSLRLimits()
{
    QString model(currentCCD->getDeviceName());

    // Check if model already exists
    auto pos = std::find_if(DSLRInfos.begin(), DSLRInfos.end(), [model](QMap<QString, QVariant> &oneDSLRInfo)
    {
        return (oneDSLRInfo["Model"] == model);
    });

    if (pos != DSLRInfos.end())
    {
        if (captureFrameWN->maximum() == 0 || captureFrameWN->maximum() > (*pos)["Width"].toInt())
        {
            captureFrameWN->setValue((*pos)["Width"].toInt());
            captureFrameWN->setMaximum((*pos)["Width"].toInt());
        }

        if (captureFrameHN->maximum() == 0 || captureFrameHN->maximum() > (*pos)["Height"].toInt())
        {
            captureFrameHN->setValue((*pos)["Height"].toInt());
            captureFrameHN->setMaximum((*pos)["Height"].toInt());
        }
    }
}

void Capture::setCapturedFramesMap(const QString &signature, int count)
{
    capturedFramesMap[signature] = static_cast<ushort>(count);
    qCDebug(KSTARS_EKOS_CAPTURE) <<
                                 QString("Client module indicates that storage for '%1' has already %2 captures processed.").arg(signature).arg(count);
    // Scheduler's captured frame map overrides the progress option of the Capture module
    ignoreJobProgress = false;
}

void Capture::setPresetSettings(const QJsonObject &settings)
{
    // FIXME: QComboBox signal "activated" does not trigger when setting text programmatically.
    const QString targetCamera = settings["camera"].toString();
    const QString targetFW = settings["fw"].toString();
    const QString targetFilter = settings["filter"].toString();

    if (cameraS->currentText() != targetCamera)
    {
        const int index = cameraS->findText(targetCamera);
        cameraS->setCurrentIndex(index);
        checkCCD(index);
    }

    if (!targetFW.isEmpty() && filterWheelS->currentText() != targetFW)
    {
        const int index = filterWheelS->findText(targetFW);
        filterWheelS->setCurrentIndex(index);
        checkFilter(index);
    }

    if (!targetFilter.isEmpty() && captureFilterS->currentText() != targetFilter)
    {
        captureFilterS->setCurrentIndex(captureFilterS->findText(targetFilter));
    }

    captureExposureN->setValue(settings["exp"].toDouble(1));

    int bin = settings["bin"].toInt(1);
    setBinning(bin, bin);

    double temperature = settings["temperature"].toDouble(100);
    if (temperature < 100 && currentCCD && currentCCD->hasCoolerControl())
    {
        setForceTemperature(true);
        setTargetTemperature(temperature);
    }

    double gain = settings["gain"].toDouble(-1);
    if (gain >= 0 && currentCCD && currentCCD->hasGain())
    {
        setGain(gain);
    }

    double offset = settings["offset"].toDouble(-1);
    if (offset >= 0 && currentCCD && currentCCD->hasOffset())
    {
        setOffset(offset);
    }

    int format = settings["format"].toInt(-1);
    if (format >= 0)
    {
        captureFormatS->setCurrentIndex(format);
    }

    captureTypeS->setCurrentIndex(qMax(0, settings["frameType"].toInt(0)));

    // ISO
    int isoIndex = settings["iso"].toInt(-1);
    if (isoIndex >= 0)
        setISO(isoIndex);
}

void Capture::setFileSettings(const QJsonObject &settings)
{
    const QString prefix = settings["prefix"].toString(filePrefixT->text());
    const QString script = settings["script"].toString(fileScriptT->text());
    const QString directory = settings["directory"].toString(fileDirectoryT->text());
    const bool filter = settings["filter"].toBool(fileFilterS->isChecked());
    const bool duration = settings["duration"].toBool(fileDurationS->isChecked());
    const bool ts = settings["ts"].toBool(fileTimestampS->isChecked());
    const int upload = settings["upload"].toInt(fileUploadModeS->currentIndex());
    const QString remote = settings["remote"].toString(fileRemoteDirT->text());

    filePrefixT->setText(prefix);
    fileScriptT->setText(script);
    fileDirectoryT->setText(directory);
    fileFilterS->setChecked(filter);
    fileDurationS->setChecked(duration);
    fileTimestampS->setChecked(ts);
    fileUploadModeS->setCurrentIndex(upload);
    fileRemoteDirT->setText(remote);
}

QJsonObject Capture::getFileSettings()
{
    QJsonObject settings =
    {
        {"prefix", filePrefixT->text()},
        {"script", fileScriptT->text()},
        {"directory", fileDirectoryT->text()},
        {"filter", fileFilterS->isChecked()},
        {"duration", fileDurationS->isChecked()},
        {"ts", fileTimestampS->isChecked()},
        {"upload", fileUploadModeS->currentIndex()},
        {"remote", fileRemoteDirT->text()}
    };

    return settings;
}

void Capture::setCalibrationSettings(const QJsonObject &settings)
{
    const int source = settings["source"].toInt(flatFieldSource);
    const int duration = settings["duration"].toInt(flatFieldDuration);
    const double az = settings["az"].toDouble(wallCoord.az().Degrees());
    const double al = settings["al"].toDouble(wallCoord.alt().Degrees());
    const int adu = settings["adu"].toInt(static_cast<int>(std::round(targetADU)));
    const int tolerance = settings["tolerance"].toInt(static_cast<int>(std::round(targetADUTolerance)));
    const bool parkMount = settings["parkMount"].toBool(preMountPark);
    const bool parkDome = settings["parkDome"].toBool(preDomePark);

    flatFieldSource = static_cast<FlatFieldSource>(source);
    flatFieldDuration = static_cast<FlatFieldDuration>(duration);
    wallCoord.setAz(az);
    wallCoord.setAlt(al);
    targetADU = adu;
    targetADUTolerance = tolerance;
    preMountPark = parkMount;
    preDomePark = parkDome;
}

QJsonObject Capture::getCalibrationSettings()
{
    QJsonObject settings =
    {
        {"source", flatFieldSource},
        {"duration", flatFieldDuration},
        {"az", wallCoord.az().Degrees()},
        {"al", wallCoord.alt().Degrees()},
        {"adu", targetADU},
        {"tolerance", targetADUTolerance},
        {"parkMount", preMountPark},
        {"parkDome", preDomePark},
    };

    return settings;
}

void Capture::setLimitSettings(const QJsonObject &settings)
{
    const bool deviationCheck = settings["deviationCheck"].toBool(limitGuideDeviationS->isChecked());
    const double deviationValue = settings["deviationValue"].toDouble(limitGuideDeviationN->value());
    const bool focusHFRCheck = settings["focusHFRCheck"].toBool(limitFocusHFRS->isChecked());
    const double focusHFRValue = settings["focusHFRValue"].toDouble(limitFocusHFRN->value());
    const bool focusDeltaTCheck = settings["focusDeltaTCheck"].toBool(limitFocusDeltaTS->isChecked());
    const double focusDeltaTValue = settings["focusDeltaTValue"].toDouble(limitFocusDeltaTN->value());
    const bool refocusNCheck = settings["refocusNCheck"].toBool(limitRefocusS->isChecked());
    const int refocusNValue = settings["refocusNValue"].toInt(limitRefocusN->value());

    if (deviationCheck)
    {
        limitGuideDeviationS->setChecked(true);
        limitGuideDeviationN->setValue(deviationValue);
    }
    else
        limitGuideDeviationS->setChecked(false);

    if (focusHFRCheck)
    {
        limitFocusHFRS->setChecked(true);
        limitFocusHFRN->setValue(focusHFRValue);
    }
    else
        limitFocusHFRS->setChecked(false);

    if (focusDeltaTCheck)
    {
        limitFocusDeltaTS->setChecked(true);
        limitFocusDeltaTN->setValue(focusDeltaTValue);
    }
    else
        limitFocusDeltaTS->setChecked(false);

    if (refocusNCheck)
    {
        limitRefocusS->setChecked(true);
        limitRefocusN->setValue(refocusNValue);
    }
    else
        limitRefocusS->setChecked(false);
}

QJsonObject Capture::getLimitSettings()
{
    QJsonObject settings =
    {
        {"deviationCheck", limitGuideDeviationS->isChecked()},
        {"deviationValue", limitGuideDeviationN->value()},
        {"focusHFRCheck", limitFocusHFRS->isChecked()},
        {"focusHFRValue", limitFocusHFRN->value()},
        {"focusDeltaTCheck", limitFocusDeltaTS->isChecked()},
        {"focusDeltaTValue", limitFocusDeltaTN->value()},
        {"refocusNCheck", limitRefocusS->isChecked()},
        {"refocusNValue", limitRefocusN->value()},
    };

    return settings;
}

void Capture::clearCameraConfiguration()
{
    //if (!Options::autonomousMode() && KMessageBox::questionYesNo(nullptr, i18n("Reset %1 configuration to default?", currentCCD->getDeviceName()), i18n("Confirmation")) == KMessageBox::No)
    //    return;

    connect(KSMessageBox::Instance(), &KSMessageBox::accepted, this, [this]()
    {
        //QObject::disconnect(KSMessageBox::Instance(), &KSMessageBox::accepted, this, nullptr);
        KSMessageBox::Instance()->disconnect(this);
        currentCCD->setConfig(PURGE_CONFIG);
        KStarsData::Instance()->userdb()->DeleteDSLRInfo(currentCCD->getDeviceName());

        QStringList shutterfulCCDs  = Options::shutterfulCCDs();
        QStringList shutterlessCCDs = Options::shutterlessCCDs();

        // Remove camera from shutterful and shutterless CCDs
        if (shutterfulCCDs.contains(currentCCD->getDeviceName()))
        {
            shutterfulCCDs.removeOne(currentCCD->getDeviceName());
            Options::setShutterfulCCDs(shutterfulCCDs);
        }
        if (shutterlessCCDs.contains(currentCCD->getDeviceName()))
        {
            shutterlessCCDs.removeOne(currentCCD->getDeviceName());
            Options::setShutterlessCCDs(shutterlessCCDs);
        }

        // For DSLRs, immediately ask them to enter the values again.
        if (captureISOS && captureISOS->count() > 0)
        {
            createDSLRDialog();
        }
    });

    KSMessageBox::Instance()->questionYesNo( i18n("Reset %1 configuration to default?", currentCCD->getDeviceName()),
            i18n("Confirmation"), 30);
}

void Capture::setCoolerToggled(bool enabled)
{
    coolerOnB->blockSignals(true);
    coolerOnB->setChecked(enabled);
    coolerOnB->blockSignals(false);

    coolerOffB->blockSignals(true);
    coolerOffB->setChecked(!enabled);
    coolerOffB->blockSignals(false);

    appendLogText(enabled ? i18n("Cooler is on") : i18n("Cooler is off"));
}

void Capture::processCaptureTimeout()
{
    auto restartExposure = [&]()
    {
        appendLogText(i18n("Exposure timeout. Restarting exposure..."));
        currentCCD->setTransformFormat(ISD::CCD::FORMAT_FITS);
        ISD::CCDChip *targetChip = currentCCD->getChip(useGuideHead ? ISD::CCDChip::GUIDE_CCD : ISD::CCDChip::PRIMARY_CCD);
        targetChip->abortExposure();
        targetChip->capture(captureExposureN->value());
        captureTimeout.start(static_cast<int>(captureExposureN->value() * 1000 + CAPTURE_TIMEOUT_THRESHOLD));
    };

    m_CaptureTimeoutCounter++;


    if (m_DeviceRestartCounter >= 3)
    {
        m_CaptureTimeoutCounter = 0;
        m_DeviceRestartCounter = 0;
        appendLogText(i18n("Exposure timeout. Aborting..."));
        abort();
        return;
    }

    if (m_CaptureTimeoutCounter > 1)
    {
        QString camera = currentCCD->getDeviceName();
        QString fw = currentFilter ? currentFilter->getDeviceName() : "";
        emit driverTimedout(camera);
        QTimer::singleShot(5000, [ &, camera, fw]()
        {
            m_DeviceRestartCounter++;
            reconnectDriver(camera, fw);
        });
        return;
    }
    else restartExposure();
}

void Capture::setGeneratedPreviewFITS(const QString &previewFITS)
{
    m_GeneratedPreviewFITS = previewFITS;
}

void Capture::createDSLRDialog()
{
    dslrInfoDialog.reset(new DSLRInfo(this, currentCCD));

    connect(dslrInfoDialog.get(), &DSLRInfo::infoChanged, [this]()
    {
        addDSLRInfo(QString(currentCCD->getDeviceName()),
                    static_cast<uint32_t>(dslrInfoDialog->sensorMaxWidth),
                    static_cast<uint32_t>(dslrInfoDialog->sensorMaxHeight),
                    static_cast<uint32_t>(dslrInfoDialog->sensorPixelW),
                    static_cast<uint32_t>(dslrInfoDialog->sensorPixelH));
    });

    dslrInfoDialog->show();

    emit dslrInfoRequested(currentCCD->getDeviceName());
}

void Capture::removeDevice(ISD::GDInterface *device)
{
    device->disconnect(this);
    if (currentTelescope && currentTelescope->getDeviceName() == device->getDeviceName())
    {
        currentTelescope = nullptr;
    }
    else if (currentDome && currentDome->getDeviceName() == device->getDeviceName())
    {
        currentDome = nullptr;
    }
    else if (currentRotator && currentRotator->getDeviceName() == device->getDeviceName())
    {
        currentRotator = nullptr;
        rotatorB->setEnabled(false);
    }

    if (CCDs.contains(static_cast<ISD::CCD *>(device)))
    {
        ISD::CCD *oneCCD = static_cast<ISD::CCD *>(device);
        CCDs.removeAll(oneCCD);
        cameraS->removeItem(cameraS->findText(device->getDeviceName()));
        cameraS->removeItem(cameraS->findText(device->getDeviceName() + QString(" Guider")));

        if (CCDs.empty())
        {
            currentCCD = nullptr;
            cameraS->setCurrentIndex(-1);
        }
        else
            cameraS->setCurrentIndex(0);

        //checkCCD();
        QTimer::singleShot(1000, this, [this]()
        {
            checkCCD();
        });
    }

    if (Filters.contains(static_cast<ISD::Filter *>(device)))
    {
        Filters.removeOne(static_cast<ISD::Filter *>(device));
        filterManager->removeDevice(device);
        filterWheelS->removeItem(filterWheelS->findText(device->getDeviceName()));
        if (Filters.empty())
        {
            currentFilter = nullptr;
            filterWheelS->setCurrentIndex(-1);
        }
        else
            filterWheelS->setCurrentIndex(0);

        //checkFilter();
        QTimer::singleShot(1000, this, [this]()
        {
            checkFilter();
        });
    }
}

void Capture::setGain(double value)
{
    QMap<QString, QMap<QString, double> > customProps = customPropertiesDialog->getCustomProperties();

    // Gain is manifested in two forms
    // Property CCD_GAIN and
    // Part of CCD_CONTROLS properties.
    // Therefore, we have to find what the currently camera supports first.
    if (currentCCD->getProperty("CCD_GAIN"))
    {
        QMap<QString, double> ccdGain;
        ccdGain["GAIN"] = value;
        customProps["CCD_GAIN"] = ccdGain;
    }
    else if (currentCCD->getProperty("CCD_CONTROLS"))
    {
        QMap<QString, double> ccdGain = customProps["CCD_CONTROLS"];
        ccdGain["Gain"] = value;
        customProps["CCD_CONTROLS"] = ccdGain;
    }

    customPropertiesDialog->setCustomProperties(customProps);
}

double Capture::getGain()
{
    QMap<QString, QMap<QString, double> > customProps = customPropertiesDialog->getCustomProperties();

    // Gain is manifested in two forms
    // Property CCD_GAIN and
    // Part of CCD_CONTROLS properties.
    // Therefore, we have to find what the currently camera supports first.
    if (currentCCD->getProperty("CCD_GAIN"))
    {
        return customProps["CCD_GAIN"].value("GAIN", -1);
    }
    else if (currentCCD->getProperty("CCD_CONTROLS"))
    {
        return customProps["CCD_CONTROLS"].value("Gain", -1);
    }

    return -1;
}

void Capture::setOffset(double value)
{
    QMap<QString, QMap<QString, double> > customProps = customPropertiesDialog->getCustomProperties();

    // Offset is manifested in two forms
    // Property CCD_OFFSET and
    // Part of CCD_CONTROLS properties.
    // Therefore, we have to find what the currently camera supports first.
    if (currentCCD->getProperty("CCD_OFFSET"))
    {
        QMap<QString, double> ccdOffset;
        ccdOffset["OFFSET"] = value;
        customProps["CCD_OFFSET"] = ccdOffset;
    }
    else if (currentCCD->getProperty("CCD_CONTROLS"))
    {
        QMap<QString, double> ccdOffset = customProps["CCD_CONTROLS"];
        ccdOffset["Offset"] = value;
        customProps["CCD_CONTROLS"] = ccdOffset;
    }

    customPropertiesDialog->setCustomProperties(customProps);
}

double Capture::getOffset()
{
    QMap<QString, QMap<QString, double> > customProps = customPropertiesDialog->getCustomProperties();

    // Gain is manifested in two forms
    // Property CCD_GAIN and
    // Part of CCD_CONTROLS properties.
    // Therefore, we have to find what the currently camera supports first.
    if (currentCCD->getProperty("CCD_OFFSET"))
    {
        return customProps["CCD_OFFSET"].value("OFFSET", -1);
    }
    else if (currentCCD->getProperty("CCD_CONTROLS"))
    {
        return customProps["CCD_CONTROLS"].value("Offset", -1);
    }

    return -1;
}

double Capture::getEstimatedDownloadTime()
{
    double total = 0;
    foreach(double dlTime, downloadTimes)
        total += dlTime;
    if(downloadTimes.count() == 0)
        return 0;
    else
        return total / downloadTimes.count();
}

void Capture::reconnectDriver(const QString &camera, const QString &filterWheel)
{
    for (auto &oneCamera : CCDs)
    {
        if (oneCamera->getDeviceName() == camera)
        {
            // Set camera again to the one we restarted
            cameraS->setCurrentIndex(cameraS->findText(camera));
            filterWheelS->setCurrentIndex(filterWheelS->findText(filterWheel));
            checkCCD();

            // restart capture
            m_CaptureTimeoutCounter = 0;

            if (activeJob)
            {
                activeJob->setActiveChip(targetChip);
                activeJob->setActiveCCD(currentCCD);
                activeJob->setActiveFilter(currentFilter);
                captureImage();
            }

            return;
        }
    }

    QTimer::singleShot(5000, this, [ &, camera, filterWheel]()
    {
        reconnectDriver(camera, filterWheel);
    });
}

bool Capture::isGuidingOn()
{
    // In case we are doing non guiding dither, then we are not performing autoguiding.
    if (Options::ditherNoGuiding())
        return false;

    return (guideState == GUIDE_GUIDING ||
            guideState == GUIDE_CALIBRATING ||
            guideState == GUIDE_CALIBRATION_SUCESS ||
            guideState == GUIDE_REACQUIRE ||
            guideState == GUIDE_DITHERING ||
            guideState == GUIDE_DITHERING_SUCCESS ||
            guideState == GUIDE_DITHERING_ERROR ||
            guideState == GUIDE_DITHERING_SETTLE ||
            guideState == GUIDE_SUSPENDED
           );
}

QString Capture::MFStageString(MFStage stage)
{
    switch(stage)
    {
        case MF_NONE:
            return "MF_NONE";
        case MF_REQUESTED:
            return "MF_REQUESTED";
        case MF_READY:
            return "MF_READY";
        case MF_INITIATED:
            return "MF_INITIATED";
        case MF_FLIPPING:
            return "MF_FLIPPING";
        case MF_SLEWING:
            return "MF_SLEWING";
        case MF_COMPLETED:
            return "MF_COMPLETED";
        case MF_ALIGNING:
            return "MF_ALIGNING";
        case MF_GUIDING:
            return "MF_GUIDING";
    }
    return "MFStage unknown.";
}

void Capture::syncDSLRToTargetChip(const QString &model)
{
    auto pos = std::find_if(DSLRInfos.begin(), DSLRInfos.end(), [model](QMap<QString, QVariant> &oneDSLRInfo)
    {
        return (oneDSLRInfo["Model"] == model);
    });

    // Sync Pixel Size
    if (pos != DSLRInfos.end())
    {
        auto camera = *pos;
        targetChip->setImageInfo(camera["Width"].toDouble(),
                                 camera["Height"].toDouble(),
                                 camera["PixelW"].toDouble(),
                                 camera["PixelH"].toDouble(),
                                 8);
    }
}

void Capture::editFilterName()
{
    if (!currentFilter || m_CurrentFilterPosition < 1)
        return;

    QStringList labels = filterManager->getFilterLabels();
    QDialog filterDialog;

    QFormLayout *formLayout = new QFormLayout(&filterDialog);
    QVector<QLineEdit *> newLabelEdits;

    for (uint8_t i = 0; i < labels.count(); i++)
    {
        QLabel *existingLabel = new QLabel(QString("%1. <b>%2</b>").arg(i + 1).arg(labels[i]), &filterDialog);
        QLineEdit *newLabel = new QLineEdit(labels[i], &filterDialog);
        newLabelEdits.append(newLabel);
        formLayout->addRow(existingLabel, newLabel);
    }

    filterDialog.setWindowTitle(currentFilter->getDeviceName());
    filterDialog.setLayout(formLayout);
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &filterDialog);
    connect(buttonBox, &QDialogButtonBox::accepted, &filterDialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &filterDialog, &QDialog::reject);
    filterDialog.layout()->addWidget(buttonBox);

    if (filterDialog.exec() == QDialog::Accepted)
    {
        QStringList newLabels;
        for (uint8_t i = 0; i < labels.count(); i++)
            newLabels << newLabelEdits[i]->text();
        filterManager->setFilterNames(newLabels);
    }
}

void Capture::restartCamera(const QString &name)
{
    connect(KSMessageBox::Instance(), &KSMessageBox::accepted, this, [this, name]()
    {
        KSMessageBox::Instance()->disconnect(this);
        abort();
        emit driverTimedout(name);
    });
    connect(KSMessageBox::Instance(), &KSMessageBox::rejected, this, [this]()
    {
        KSMessageBox::Instance()->disconnect(this);
    });

    KSMessageBox::Instance()->questionYesNo(i18n("Are you sure you want to restart %1 camera driver?", name),
                                            i18n("Driver Restart"), 5);
}
}
