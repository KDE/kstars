/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "capture.h"

#include "captureprocess.h"
#include "capturemodulestate.h"
#include "capturedeviceadaptor.h"
#include "captureadaptor.h"
#include "refocusstate.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "Options.h"
#include "rotatorsettings.h"
#include "sequencejob.h"
#include "placeholderpath.h"
#include "ui_calibrationoptions.h"
#include "auxiliary/ksmessagebox.h"
#include "ekos/manager.h"
#include "ekos/auxiliary/darklibrary.h"
#include "ekos/auxiliary/profilesettings.h"
#include "ekos/auxiliary/opticaltrainmanager.h"
#include "scriptsmanager.h"
#include "fitsviewer/fitsdata.h"
#include "indi/driverinfo.h"
#include "indi/indifilterwheel.h"
#include "indi/indicamera.h"
#include "indi/indirotator.h"
#include "oal/observeradd.h"
#include "ekos/guide/guide.h"
#include "exposurecalculator/exposurecalculatordialog.h"
#include "dslrinfodialog.h"
#include "ekos/auxiliary/rotatorutils.h"
#include <basedevice.h>

#include <ekos_capture_debug.h>

#define MF_TIMER_TIMEOUT    90000
#define MF_RA_DIFF_LIMIT    4


// Current Sequence File Format:
#define SQ_FORMAT_VERSION 2.6
// We accept file formats with version back to:
#define SQ_COMPAT_VERSION 2.0

// Qt version calming
#include <qtendl.h>

namespace
{

// Columns in the job table
enum JobTableColumnIndex
{
    JOBTABLE_COL_STATUS = 0,
    JOBTABLE_COL_FILTER,
    JOBTABLE_COL_COUNTS,
    JOBTABLE_COL_EXP,
    JOBTABLE_COL_TYPE,
    JOBTABLE_COL_BINNING,
    JOBTABLE_COL_ISO,
    JOBTABLE_COL_OFFSET
};
} // namespace

namespace Ekos
{
Capture::Capture()
{
    setupUi(this);

    qRegisterMetaType<CaptureState>("CaptureState");
    qDBusRegisterMetaType<CaptureState>();

    new CaptureAdaptor(this);
    m_captureModuleState.reset(new CaptureModuleState());
    m_captureDeviceAdaptor.reset(new CaptureDeviceAdaptor());
    m_captureProcess = new CaptureProcess(state(), m_captureDeviceAdaptor);

    QDBusConnection::sessionBus().registerObject("/KStars/Ekos/Capture", this);
    QPointer<QDBusInterface> ekosInterface = new QDBusInterface("org.kde.kstars", "/KStars/Ekos", "org.kde.kstars.Ekos",
            QDBusConnection::sessionBus(), this);

    // Connecting DBus signals
    QDBusConnection::sessionBus().connect("org.kde.kstars", "/KStars/Ekos", "org.kde.kstars.Ekos", "newModule", this,
                                          SLOT(registerNewModule(QString)));

    // ensure that the mount interface is present
    registerNewModule("Mount");

    KStarsData::Instance()->userdb()->GetAllDSLRInfos(state()->DSLRInfos());

    if (state()->DSLRInfos().count() > 0)
    {
        qCDebug(KSTARS_EKOS_CAPTURE) << "DSLR Cameras Info:";
        qCDebug(KSTARS_EKOS_CAPTURE) << state()->DSLRInfos();
    }

    m_LimitsDialog = new QDialog(this);
    m_LimitsUI.reset(new Ui::Limits());
    m_LimitsUI->setupUi(m_LimitsDialog);

    dirPath = QUrl::fromLocalFile(QDir::homePath());

    //isAutoGuiding   = false;

    // hide avg. download time and target drift initially
    targetDriftLabel->setVisible(false);
    targetDrift->setVisible(false);
    targetDriftUnit->setVisible(false);
    avgDownloadTime->setVisible(false);
    avgDownloadLabel->setVisible(false);
    secLabel->setVisible(false);

    connect(&state()->getSeqDelayTimer(), &QTimer::timeout, m_captureProcess, &CaptureProcess::captureImage);
    state()->getCaptureDelayTimer().setSingleShot(true);
    connect(&state()->getCaptureDelayTimer(), &QTimer::timeout, this, &Capture::start, Qt::UniqueConnection);

    connect(startB, &QPushButton::clicked, this, &Capture::toggleSequence);
    connect(pauseB, &QPushButton::clicked, this, &Capture::pause);
    connect(darkLibraryB, &QPushButton::clicked, DarkLibrary::Instance(), &QDialog::show);
    connect(limitsB, &QPushButton::clicked, m_LimitsDialog, &QDialog::show);
    connect(temperatureRegulationB, &QPushButton::clicked, this, &Capture::showTemperatureRegulation);

    startB->setIcon(QIcon::fromTheme("media-playback-start"));
    startB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    pauseB->setIcon(QIcon::fromTheme("media-playback-pause"));
    pauseB->setAttribute(Qt::WA_LayoutUsesWidgetRect);

    filterManagerB->setIcon(QIcon::fromTheme("view-filter"));
    filterManagerB->setAttribute(Qt::WA_LayoutUsesWidgetRect);

    connect(captureBinHN, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), captureBinVN, &QSpinBox::setValue);

    connect(liveVideoB, &QPushButton::clicked, this, &Capture::toggleVideo);

    connect(clearConfigurationB, &QPushButton::clicked, this, &Capture::clearCameraConfiguration);

    darkB->setChecked(Options::autoDark());
    connect(darkB, &QAbstractButton::toggled, this, [this]()
    {
        Options::setAutoDark(darkB->isChecked());
    });

    connect(restartCameraB, &QPushButton::clicked, this, [this]()
    {
        if (activeCamera())
            restartCamera(activeCamera()->getDeviceName());
    });

    connect(cameraTemperatureS, &QCheckBox::toggled, this, [this](bool toggled)
    {
        if (devices()->getActiveCamera())
        {
            QVariantMap auxInfo = devices()->getActiveCamera()->getDriverInfo()->getAuxInfo();
            auxInfo[QString("%1_TC").arg(devices()->getActiveCamera()->getDeviceName())] = toggled;
            devices()->getActiveCamera()->getDriverInfo()->setAuxInfo(auxInfo);
        }
    });

    connect(filterEditB, &QPushButton::clicked, this, &Capture::editFilterName);

    connect(FilterPosCombo, static_cast<void(QComboBox::*)(const QString &)>(&QComboBox::currentTextChanged),
            [ = ]()
    {
        state()->updateHFRThreshold();
        generatePreviewFilename();
    });
    connect(previewB, &QPushButton::clicked, this, &Capture::capturePreview);
    connect(loopB, &QPushButton::clicked, this, &Capture::startFraming);

    //connect( seqWatcher, SIGNAL(dirty(QString)), this, &Capture::checkSeqFile(QString)));

    connect(addToQueueB, &QPushButton::clicked, this, [this]()
    {
        if (m_JobUnderEdit)
            editJobFinished();
        else
            createJob();
    });
    connect(queueUpB, &QPushButton::clicked, [this]()
    {
        moveJob(true);
    });
    connect(queueDownB, &QPushButton::clicked, [this]()
    {
        moveJob(false);
    });
    connect(removeFromQueueB, &QPushButton::clicked, this, &Capture::removeJobFromQueue);
    connect(selectFileDirectoryB, &QPushButton::clicked, this, &Capture::saveFITSDirectory);
    connect(queueSaveB, &QPushButton::clicked, this, static_cast<void(Capture::*)()>(&Capture::saveSequenceQueue));
    connect(queueSaveAsB, &QPushButton::clicked, this, &Capture::saveSequenceQueueAs);
    connect(queueLoadB, &QPushButton::clicked, this, static_cast<void(Capture::*)()>(&Capture::loadSequenceQueue));
    connect(resetB, &QPushButton::clicked, this, &Capture::resetJobs);
    connect(queueTable->selectionModel(), &QItemSelectionModel::currentRowChanged, this, &Capture::selectedJobChanged);
    connect(queueTable, &QAbstractItemView::doubleClicked, this, &Capture::editJob);
    connect(queueTable, &QTableWidget::itemSelectionChanged, this, [&]()
    {
        resetJobEdit(m_JobUnderEdit);
    });
    connect(setTemperatureB, &QPushButton::clicked, this, [&]()
    {
        if (devices()->getActiveCamera())
            devices()->getActiveCamera()->setTemperature(cameraTemperatureN->value());
    });
    connect(coolerOnB, &QPushButton::clicked, this, [&]()
    {
        if (devices()->getActiveCamera())
            devices()->getActiveCamera()->setCoolerControl(true);
    });
    connect(coolerOffB, &QPushButton::clicked, this, [&]()
    {
        if (devices()->getActiveCamera())
            devices()->getActiveCamera()->setCoolerControl(false);
    });
    connect(cameraTemperatureN, &QDoubleSpinBox::editingFinished, setTemperatureB,
            static_cast<void (QPushButton::*)()>(&QPushButton::setFocus));
    connect(captureTypeS, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
            &Capture::checkFrameType);
    connect(resetFrameB, &QPushButton::clicked, m_captureProcess, &CaptureProcess::resetFrame);
    connect(calibrationB, &QPushButton::clicked, this, &Capture::openCalibrationDialog);
    // connect(rotatorB, &QPushButton::clicked, m_RotatorControlPanel.get(), &Capture::show);

    connect(generateDarkFlatsB, &QPushButton::clicked, this, &Capture::generateDarkFlats);
    connect(scriptManagerB, &QPushButton::clicked, this, &Capture::handleScriptsManager);
    connect(resetFormatB, &QPushButton::clicked, this, [this]()
    {
        placeholderFormatT->setText(KSUtils::getDefaultPath("PlaceholderFormat"));
    });

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
    generateDarkFlatsB->setIcon(QIcon::fromTheme("tools-wizard"));
    generateDarkFlatsB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    // rotatorB->setIcon(QIcon::fromTheme("kstars_solarsystem"));
    rotatorB->setAttribute(Qt::WA_LayoutUsesWidgetRect);

    addToQueueB->setToolTip(i18n("Add job to sequence queue"));
    removeFromQueueB->setToolTip(i18n("Remove job from sequence queue"));

    ////////////////////////////////////////////////////////////////////////
    /// Device Adaptor
    ////////////////////////////////////////////////////////////////////////
    connect(m_captureDeviceAdaptor.data(), &CaptureDeviceAdaptor::newCCDTemperatureValue, this,
            &Capture::updateCCDTemperature, Qt::UniqueConnection);
    connect(m_captureDeviceAdaptor.data(), &CaptureDeviceAdaptor::newRotatorAngle, this,
            &Capture::updateRotatorAngle, Qt::UniqueConnection);

    ////////////////////////////////////////////////////////////////////////
    /// Settings
    ////////////////////////////////////////////////////////////////////////
    syncGUIToGeneralSettings();
    // Start Guide Deviation Check
    connect(m_LimitsUI->startGuiderDriftS, &QCheckBox::toggled, [ = ](bool checked)
    {
        Options::setEnforceStartGuiderDrift(checked);
    });

    // Start Guide Deviation Value
    connect(m_LimitsUI->startGuiderDriftN, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this]()
    {
        Options::setStartGuideDeviation(m_LimitsUI->startGuiderDriftN->value());
    });

    // Abort Guide Deviation Check
    connect(m_LimitsUI->limitGuideDeviationS, &QCheckBox::toggled, [ = ](bool checked)
    {
        Options::setEnforceGuideDeviation(checked);
    });

    // Guide Deviation Value
    connect(m_LimitsUI->limitGuideDeviationN, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this]()
    {
        Options::setGuideDeviation(m_LimitsUI->limitGuideDeviationN->value());
    });

    connect(m_LimitsUI->limitGuideDeviationRepsN, QOverload<int>::of(&QSpinBox::valueChanged), this, [this]()
    {
        Options::setGuideDeviationReps(static_cast<uint>(m_LimitsUI->limitGuideDeviationRepsN->value()));
    });

    // Autofocus HFR Check
    connect(m_LimitsUI->limitFocusHFRS, &QCheckBox::toggled, [ = ](bool checked)
    {
        Options::setEnforceAutofocusHFR(checked);
        if (checked == false)
            state()->getRefocusState()->setInSequenceFocus(false);
    });

    // Autofocus HFR Deviation
    m_LimitsUI->limitFocusHFRN->setValue(Options::hFRDeviation());
    connect(m_LimitsUI->limitFocusHFRN, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this]()
    {
        Options::setHFRDeviation(m_LimitsUI->limitFocusHFRN->value());
    });
    connect(m_captureModuleState.get(), &CaptureModuleState::newLimitFocusHFR, this, [this](double hfr)
    {
        m_LimitsUI->limitFocusHFRN->setValue(hfr);
    });

    // Autofocus temperature Check
    connect(m_LimitsUI->limitFocusDeltaTS, &QCheckBox::toggled, this,  [ = ](bool checked)
    {
        Options::setEnforceAutofocusOnTemperature(checked);
    });

    // Autofocus temperature Delta
    connect(m_LimitsUI->limitFocusDeltaTN, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this]()
    {
        Options::setMaxFocusTemperatureDelta(m_LimitsUI->limitFocusDeltaTN->value());
    });

    // Refocus Every Check
    connect(m_LimitsUI->limitRefocusS, &QCheckBox::toggled, this, [](bool checked)
    {
        Options::setEnforceRefocusEveryN(checked);
    });

    // Refocus Every Value
    connect(m_LimitsUI->limitRefocusN, QOverload<int>::of(&QSpinBox::valueChanged), this, [this]()
    {
        Options::setRefocusEveryN(static_cast<uint>(m_LimitsUI->limitRefocusN->value()));
    });

    // Refocus after meridian flip
    m_LimitsUI->meridianRefocusS->setChecked(Options::refocusAfterMeridianFlip());
    connect(m_LimitsUI->meridianRefocusS, &QCheckBox::toggled, [](bool checked)
    {
        Options::setRefocusAfterMeridianFlip(checked);
    });

    QCheckBox * const checkBoxes[] =
    {
        m_LimitsUI->limitGuideDeviationS,
        m_LimitsUI->startGuiderDriftS,
        m_LimitsUI->limitRefocusS,
        m_LimitsUI->limitFocusDeltaTS,
        m_LimitsUI->limitFocusHFRS,
        m_LimitsUI->meridianRefocusS,
    };
    for (const QCheckBox * control : checkBoxes)
        connect(control, &QCheckBox::toggled, this, [&]()
    {
        state()->setDirty(true);
    });

    QDoubleSpinBox * const dspinBoxes[]
    {
        m_LimitsUI->limitFocusHFRN,
        m_LimitsUI->limitFocusDeltaTN,
        m_LimitsUI->limitGuideDeviationN,
        m_LimitsUI->startGuiderDriftN
    };
    for (const QDoubleSpinBox * control : dspinBoxes)
        connect(control, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), this, [&]()
    {
        state()->setDirty(true);
    });

    connect(fileUploadModeS, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, [&]()
    {
        state()->setDirty(true);
    });
    connect(fileRemoteDirT, &QLineEdit::editingFinished, this, [&]()
    {
        state()->setDirty(true);
    });

    observerB->setIcon(QIcon::fromTheme("im-user"));
    observerB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    connect(observerB, &QPushButton::clicked, this, &Capture::showObserverDialog);

    // Exposure Timeout
    state()->getCaptureTimeout().setSingleShot(true);
    connect(&state()->getCaptureTimeout(), &QTimer::timeout, m_captureProcess,
            &CaptureProcess::processCaptureTimeout);

    // Remote directory
    connect(fileUploadModeS, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
            [&](int index)
    {
        fileRemoteDirT->setEnabled(index != 0);
    });

    customPropertiesDialog.reset(new CustomProperties());
    connect(customValuesB, &QPushButton::clicked, this, [&]()
    {
        customPropertiesDialog.get()->show();
        customPropertiesDialog.get()->raise();
    });
    connect(customPropertiesDialog.get(), &CustomProperties::valueChanged, this, [&]()
    {
        const double newGain = getGain();
        if (captureGainN && newGain >= 0)
            captureGainN->setValue(newGain);
        const int newOffset = getOffset();
        if (newOffset >= 0)
            captureOffsetN->setValue(newOffset);
    });

    if(!Options::captureDirectory().isEmpty())
        fileDirectoryT->setText(Options::captureDirectory());
    else
    {
        fileDirectoryT->setText(QDir::homePath() + QDir::separator() + "Pictures");
        Options::setCaptureDirectory(fileDirectoryT->text());
    }

    connect(fileDirectoryT, &QLineEdit::textChanged, this, [&]()
    {
        Options::setCaptureDirectory(fileDirectoryT->text());
        generatePreviewFilename();
    });

    if (Options::remoteCaptureDirectory().isEmpty() == false)
    {
        fileRemoteDirT->setText(Options::remoteCaptureDirectory());
    }
    connect(fileRemoteDirT, &QLineEdit::editingFinished, this, [&]()
    {
        Options::setRemoteCaptureDirectory(fileRemoteDirT->text());
        generatePreviewFilename();
    });

    //Note:  This is to prevent a button from being called the default button
    //and then executing when the user hits the enter key such as when on a Text Box
    QList<QPushButton *> qButtons = findChildren<QPushButton *>();
    for (auto &button : qButtons)
        button->setAutoDefault(false);

    DarkLibrary::Instance()->setCaptureModule(this);

    // display the capture status in the UI
    connect(this, &Capture::newStatus, captureStatusWidget, &LedStatusWidget::setCaptureState);

    // react upon state changes
    connect(m_captureModuleState.data(), &CaptureModuleState::captureBusy, this, &Capture::setBusy);
    connect(m_captureModuleState.data(), &CaptureModuleState::startCapture, this, &Capture::start);
    connect(m_captureModuleState.data(), &CaptureModuleState::abortCapture, this, &Capture::abort);
    connect(m_captureModuleState.data(), &CaptureModuleState::suspendCapture, this, &Capture::suspend);
    connect(m_captureModuleState.data(), &CaptureModuleState::executeActiveJob, m_captureProcess.data(),
            &CaptureProcess::executeJob);
    connect(m_captureModuleState.data(), &CaptureModuleState::updatePrepareState, this, &Capture::updatePrepareState);
    // forward signals from capture module state
    connect(m_captureModuleState.data(), &CaptureModuleState::captureStarted, m_captureProcess.data(),
            &CaptureProcess::captureStarted);
    connect(m_captureModuleState.data(), &CaptureModuleState::newLog, this, &Capture::appendLogText);
    connect(m_captureModuleState.data(), &CaptureModuleState::newStatus, this, &Capture::newStatus);
    connect(m_captureModuleState.data(), &CaptureModuleState::sequenceChanged, this, &Capture::sequenceChanged);
    connect(m_captureModuleState.data(), &CaptureModuleState::checkFocus, this, &Capture::checkFocus);
    connect(m_captureModuleState.data(), &CaptureModuleState::runAutoFocus, this, &Capture::runAutoFocus);
    connect(m_captureModuleState.data(), &CaptureModuleState::resetFocus, this, &Capture::resetFocus);
    connect(m_captureModuleState.data(), &CaptureModuleState::adaptiveFocus, this, &Capture::adaptiveFocus);
    connect(m_captureModuleState.data(), &CaptureModuleState::guideAfterMeridianFlip, this,
            &Capture::guideAfterMeridianFlip);
    connect(m_captureModuleState.data(), &CaptureModuleState::newFocusStatus, this, &Capture::updateFocusStatus);
    connect(m_captureModuleState.data(), &CaptureModuleState::newMeridianFlipStage, this, &Capture::updateMeridianFlipStage);
    connect(m_captureModuleState.data(), &CaptureModuleState::meridianFlipStarted, this, &Capture::meridianFlipStarted);
    // forward signals from capture process
    connect(m_captureProcess.data(), &CaptureProcess::cameraReady, this, &Capture::ready);
    connect(m_captureProcess.data(), &CaptureProcess::refreshCamera, this, &Capture::updateCamera);
    connect(m_captureProcess.data(), &CaptureProcess::refreshCameraSettings, this, &Capture::refreshCameraSettings);
    connect(m_captureProcess.data(), &CaptureProcess::refreshFilterSettings, this, &Capture::refreshFilterSettings);
    connect(m_captureProcess.data(), &CaptureProcess::newExposureProgress, this, &Capture::newExposureProgress);
    connect(m_captureProcess.data(), &CaptureProcess::newDownloadProgress, this, &Capture::updateDownloadProgress);
    connect(m_captureProcess.data(), &CaptureProcess::updateCaptureCountDown, this, &Capture::updateCaptureCountDown);
    connect(m_captureProcess.data(), &CaptureProcess::processingFITSfinished, this, &Capture::processingFITSfinished);
    connect(m_captureProcess.data(), &CaptureProcess::newImage, this, &Capture::newImage);
    connect(m_captureProcess.data(), &CaptureProcess::syncGUIToJob, this, &Capture::syncGUIToJob);
    connect(m_captureProcess.data(), &CaptureProcess::captureComplete, this, &Capture::captureComplete);
    connect(m_captureProcess.data(), &CaptureProcess::updateFrameProperties, this, &Capture::updateFrameProperties);
    connect(m_captureProcess.data(), &CaptureProcess::jobExecutionPreparationStarted, this,
            &Capture::jobExecutionPreparationStarted);
    connect(m_captureProcess.data(), &CaptureProcess::sequenceChanged, this, &Capture::sequenceChanged);
    connect(m_captureProcess.data(), &CaptureProcess::addJob, this, &Capture::addJob);
    connect(m_captureProcess.data(), &CaptureProcess::createJob, [this](SequenceJob::SequenceJobType jobType)
    {
        // report the result back to the process
        process()->jobCreated(createJob(jobType));
    });
    connect(m_captureProcess.data(), &CaptureProcess::jobPrepared, this, &Capture::jobPrepared);
    connect(m_captureProcess.data(), &CaptureProcess::captureImageStarted, this, &Capture::captureImageStarted);
    connect(m_captureProcess.data(), &CaptureProcess::captureTarget, this, &Capture::captureTarget);
    connect(m_captureProcess.data(), &CaptureProcess::downloadingFrame, this, [this]()
    {
        captureStatusWidget->setStatus(i18n("Downloading..."), Qt::yellow);
    });
    connect(m_captureProcess.data(), &CaptureProcess::captureAborted, this, &Capture::captureAborted);
    connect(m_captureProcess.data(), &CaptureProcess::captureStopped, this, &Capture::captureStopped);
    connect(m_captureProcess.data(), &CaptureProcess::updateJobTable, this, &Capture::updateJobTable);
    connect(m_captureProcess.data(), &CaptureProcess::abortFocus, this, &Capture::abortFocus);
    connect(m_captureProcess.data(), &CaptureProcess::updateMeridianFlipStage, this, &Capture::updateMeridianFlipStage);
    connect(m_captureProcess.data(), &CaptureProcess::darkFrameCompleted, this, &Capture::imageCapturingCompleted);
    connect(m_captureProcess.data(), &CaptureProcess::newLog, this, &Capture::appendLogText);
    connect(m_captureProcess.data(), &CaptureProcess::jobStarting, this, &Capture::jobStarting);
    connect(m_captureProcess.data(), &CaptureProcess::captureRunning, this, &Capture::captureRunning);
    connect(m_captureProcess.data(), &CaptureProcess::stopCapture, this, &Capture::stop);
    connect(m_captureProcess.data(), &CaptureProcess::suspendGuiding, this, &Capture::suspendGuiding);
    connect(m_captureProcess.data(), &CaptureProcess::resumeGuiding, this, &Capture::resumeGuiding);
    connect(m_captureProcess.data(), &CaptureProcess::driverTimedout, this, &Capture::driverTimedout);
    connect(m_captureProcess.data(), &CaptureProcess::rotatorReverseToggled, this, &Capture::setRotatorReversed);
    // connections between state machine and device adaptor
    connect(m_captureModuleState.data(), &CaptureModuleState::newFilterPosition,
            m_captureDeviceAdaptor.data(), &CaptureDeviceAdaptor::setFilterPosition);
    connect(m_captureModuleState.data(), &CaptureModuleState::abortFastExposure,
            m_captureDeviceAdaptor.data(), &CaptureDeviceAdaptor::abortFastExposure);
    connect(m_captureDeviceAdaptor.data(), &CaptureDeviceAdaptor::pierSideChanged,
            m_captureModuleState.data(), &CaptureModuleState::setPierSide);
    connect(m_captureDeviceAdaptor.data(), &CaptureDeviceAdaptor::newFilterWheel, this, &Capture::setFilterWheel);
    connect(m_captureDeviceAdaptor.data(), &CaptureDeviceAdaptor::CameraConnected, this, [this](bool connected)
    {
        CCDFWGroup->setEnabled(connected);
        sequenceBox->setEnabled(connected);
        for (auto &oneChild : sequenceControlsButtonGroup->buttons())
            oneChild->setEnabled(connected);

        if (! connected)
        {
            opticalTrainCombo->setEnabled(true);
            trainLabel->setEnabled(true);
        }
    });
    connect(m_captureDeviceAdaptor.data(), &CaptureDeviceAdaptor::FilterWheelConnected, this, [this](bool connected)
    {
        FilterPosLabel->setEnabled(connected);
        FilterPosCombo->setEnabled(connected);
        filterManagerB->setEnabled(connected);
    });
    connect(m_captureDeviceAdaptor.data(), &CaptureDeviceAdaptor::newRotator, this, &Capture::setRotator);

    setupOpticalTrainManager();

    // Generate Meridian Flip State
    getMeridianFlipState();

    //Update the filename preview
    placeholderFormatT->setText(Options::placeholderFormat());
    connect(placeholderFormatT, &QLineEdit::textChanged, this, [this]()
    {
        Options::setPlaceholderFormat(placeholderFormatT->text());
        generatePreviewFilename();
    });
    connect(formatSuffixN, QOverload<int>::of(&QSpinBox::valueChanged), this, &Capture::generatePreviewFilename);
    connect(captureExposureN, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            &Capture::generatePreviewFilename);
    connect(targetNameT, &QLineEdit::textEdited, this, [ = ]()
    {
        generatePreviewFilename();
        qCDebug(KSTARS_EKOS_CAPTURE) << "Changed target to" << targetNameT->text() << "because of user edit";
    });
    connect(captureTypeS, &QComboBox::currentTextChanged, this, &Capture::generatePreviewFilename);

    connect(exposureCalcB, &QPushButton::clicked, this, &Capture::openExposureCalculatorDialog);

}

Capture::~Capture()
{
    qDeleteAll(state()->allJobs());
    state()->allJobs().clear();
}

bool Capture::updateCamera()
{
    auto isConnected = activeCamera() && activeCamera()->isConnected();
    CCDFWGroup->setEnabled(isConnected);
    sequenceBox->setEnabled(isConnected);
    for (auto &oneChild : sequenceControlsButtonGroup->buttons())
        oneChild->setEnabled(isConnected);

    QVariant trainID = ProfileSettings::Instance()->getOneSetting(ProfileSettings::CaptureOpticalTrain);

    if (activeCamera() && trainID.isValid())
    {
        opticalTrainCombo->setToolTip(QString("%1 @ %2").arg(activeCamera()->getDeviceName(), currentScope()["name"].toString()));

        cameraLabel->setText(activeCamera()->getDeviceName());
    }
    else
    {
        cameraLabel->clear();
        return false;
    }

    if (devices()->filterWheel())
        process()->updateFilterInfo();

    process()->checkCamera();

    emit settingsUpdated(getPresetSettings());

    return true;
}



void Capture::setFilterWheel(QString name)
{
    if (devices()->filterWheel() && devices()->filterWheel()->getDeviceName() == name)
    {
        refreshFilterSettings();
        return;
    }

    auto isConnected = devices()->filterWheel() && devices()->filterWheel()->isConnected();
    FilterPosLabel->setEnabled(isConnected);
    FilterPosCombo->setEnabled(isConnected);
    filterManagerB->setEnabled(isConnected);

    refreshFilterSettings();

    if (devices()->filterWheel())
        emit settingsUpdated(getPresetSettings());
}

bool Capture::setDome(ISD::Dome *device)
{
    return m_captureProcess->setDome(device);
}

void Capture::setRotator(QString name)
{
    ISD::Rotator *Rotator = devices()->rotator();
    // clear old rotator
    rotatorB->setEnabled(false);
    if (Rotator && !m_RotatorControlPanel.isNull())
        m_RotatorControlPanel->close();

    // set new rotator
    if (!name.isEmpty())  // start real rotator
    {
        Manager::Instance()->getRotatorController(name, m_RotatorControlPanel);
        m_RotatorControlPanel->initRotator(opticalTrainCombo->currentText(), m_captureDeviceAdaptor.data(), Rotator);
        connect(rotatorB, &QPushButton::clicked, this, [this]()
        {
            m_RotatorControlPanel->show();
            m_RotatorControlPanel->raise();
        });
        rotatorB->setEnabled(true);
    }
    else if (Options::astrometryUseRotator()) // start at least rotatorutils for "manual rotator"
    {
        RotatorUtils::Instance()->initRotatorUtils(opticalTrainCombo->currentText());
    }
}

void Capture::pause()
{
    process()->pauseCapturing();
    updateStartButtons(false, true);
}

void Capture::toggleSequence()
{
    const CaptureState capturestate = state()->getCaptureState();
    if (capturestate == CAPTURE_PAUSE_PLANNED || capturestate == CAPTURE_PAUSED)
        updateStartButtons(true, false);

    process()->toggleSequence();
}

void Capture::jobStarting()
{
    if (m_LimitsUI->limitFocusHFRS->isChecked() && state()->getRefocusState()->isAutoFocusReady() == false)
        appendLogText(i18n("Warning: in-sequence focusing is selected but autofocus process was not started."));
    if (m_LimitsUI->limitFocusDeltaTS->isChecked() && state()->getRefocusState()->isAutoFocusReady() == false)
        appendLogText(i18n("Warning: temperature delta check is selected but autofocus process was not started."));

    updateStartButtons(true, false);
}

void Capture::registerNewModule(const QString &name)
{
    if (name == "Mount" && mountInterface == nullptr)
    {
        qCDebug(KSTARS_EKOS_CAPTURE) << "Registering new Module (" << name << ")";
        mountInterface = new QDBusInterface("org.kde.kstars", "/KStars/Ekos/Mount",
                                            "org.kde.kstars.Ekos.Mount", QDBusConnection::sessionBus(), this);

    }
}

QString Capture::camera()
{
    if (devices()->getActiveCamera())
        return devices()->getActiveCamera()->getDeviceName();

    return QString();
}

void Capture::refreshCameraSettings()
{
    // Make sure we have a valid chip and valid base device.
    // Make sure we are not in capture process.
    auto camera = activeCamera();

    if (!camera)
        return;

    ISD::CameraChip *targetChip = devices()->getActiveChip();
    if (!targetChip || !targetChip->getCCD() || targetChip->isCapturing())
        return;

    if (camera->hasCoolerControl())
    {
        coolerOnB->setEnabled(true);
        coolerOffB->setEnabled(true);
        coolerOnB->setChecked(camera->isCoolerOn());
        coolerOffB->setChecked(!camera->isCoolerOn());
    }
    else
    {
        coolerOnB->setEnabled(false);
        coolerOnB->setChecked(false);
        coolerOffB->setEnabled(false);
        coolerOffB->setChecked(false);
    }

    updateFrameProperties();

    updateCaptureFormats();

    customPropertiesDialog->setCCD(camera);

    liveVideoB->setEnabled(camera->hasVideoStream());
    if (camera->hasVideoStream())
        setVideoStreamEnabled(camera->isStreamingEnabled());
    else
        liveVideoB->setIcon(QIcon::fromTheme("camera-off"));

    connect(camera, &ISD::Camera::propertyUpdated, this, &Capture::processCameraNumber, Qt::UniqueConnection);
    connect(camera, &ISD::Camera::coolerToggled, this, &Capture::setCoolerToggled, Qt::UniqueConnection);
    connect(camera, &ISD::Camera::videoStreamToggled, this, &Capture::setVideoStreamEnabled, Qt::UniqueConnection);
    connect(camera, &ISD::Camera::ready, this, &Capture::ready, Qt::UniqueConnection);
    connect(camera, &ISD::Camera::error, m_captureProcess.data(), &CaptureProcess::processCaptureError,
            Qt::UniqueConnection);

    syncCameraInfo();

    // update values received by the device adaptor
    // connect(activeCamera(), &ISD::Camera::newTemperatureValue, this, &Capture::updateCCDTemperature, Qt::UniqueConnection);

    DarkLibrary::Instance()->checkCamera();
}

void Capture::updateCaptureFormats()
{
    QStringList frameTypes = process()->frameTypes();

    captureTypeS->clear();

    if (frameTypes.isEmpty())
        captureTypeS->setEnabled(false);
    else
    {
        captureTypeS->setEnabled(true);
        captureTypeS->addItems(frameTypes);
        captureTypeS->setCurrentIndex(devices()->getActiveChip()->getFrameType());
    }

    // Capture Format
    captureFormatS->blockSignals(true);
    captureFormatS->clear();
    captureFormatS->addItems(activeCamera()->getCaptureFormats());
    captureFormatS->setCurrentText(activeCamera()->getCaptureFormat());
    captureFormatS->blockSignals(false);

    // Encoding format
    captureEncodingS->blockSignals(true);
    captureEncodingS->clear();
    captureEncodingS->addItems(activeCamera()->getEncodingFormats());
    captureEncodingS->setCurrentText(activeCamera()->getEncodingFormat());
    captureEncodingS->blockSignals(false);
}

void Capture::syncCameraInfo()
{
    if (!activeCamera())
        return;

    if (activeCamera()->hasCooler())
    {
        cameraTemperatureS->setEnabled(true);
        cameraTemperatureN->setEnabled(true);

        if (activeCamera()->getPermission("CCD_TEMPERATURE") != IP_RO)
        {
            double min, max, step;
            setTemperatureB->setEnabled(true);
            cameraTemperatureN->setReadOnly(false);
            cameraTemperatureS->setEnabled(true);
            temperatureRegulationB->setEnabled(true);
            activeCamera()->getMinMaxStep("CCD_TEMPERATURE", "CCD_TEMPERATURE_VALUE", &min, &max, &step);
            cameraTemperatureN->setMinimum(min);
            cameraTemperatureN->setMaximum(max);
            cameraTemperatureN->setSingleStep(1);
            bool isChecked = activeCamera()->getDriverInfo()->getAuxInfo().value(QString("%1_TC").arg(activeCamera()->getDeviceName()),
                             false).toBool();
            cameraTemperatureS->setChecked(isChecked);
        }
        else
        {
            setTemperatureB->setEnabled(false);
            cameraTemperatureN->setReadOnly(true);
            cameraTemperatureS->setEnabled(false);
            cameraTemperatureS->setChecked(false);
            temperatureRegulationB->setEnabled(false);
        }

        double temperature = 0;
        if (activeCamera()->getTemperature(&temperature))
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
        temperatureRegulationB->setEnabled(false);
        cameraTemperatureN->clear();
        temperatureOUT->clear();
        setTemperatureB->setEnabled(false);
    }

    auto isoList = devices()->getActiveChip()->getISOList();
    captureISOS->blockSignals(true);
    captureISOS->clear();

    // No ISO range available
    if (isoList.isEmpty())
    {
        captureISOS->setEnabled(false);
    }
    else
    {
        captureISOS->setEnabled(true);
        captureISOS->addItems(isoList);
        captureISOS->setCurrentIndex(devices()->getActiveChip()->getISOIndex());

        uint16_t w, h;
        uint8_t bbp {8};
        double pixelX = 0, pixelY = 0;
        bool rc = devices()->getActiveChip()->getImageInfo(w, h, pixelX, pixelY, bbp);
        bool isModelInDB = state()->isModelinDSLRInfo(QString(activeCamera()->getDeviceName()));
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
                QString model = QString(activeCamera()->getDeviceName());
                process()->syncDSLRToTargetChip(model);
            }
        }
    }
    captureISOS->blockSignals(false);

    // Gain Check
    if (activeCamera()->hasGain())
    {
        double min, max, step, value, targetCustomGain;
        activeCamera()->getGainMinMaxStep(&min, &max, &step);

        // Allow the possibility of no gain value at all.
        GainSpinSpecialValue = min - step;
        captureGainN->setRange(GainSpinSpecialValue, max);
        captureGainN->setSpecialValueText(i18n("--"));
        captureGainN->setEnabled(true);
        captureGainN->setSingleStep(step);
        activeCamera()->getGain(&value);
        currentGainLabel->setText(QString::number(value, 'f', 0));

        targetCustomGain = getGain();

        // Set the custom gain if we have one
        // otherwise it will not have an effect.
        if (targetCustomGain > 0)
            captureGainN->setValue(targetCustomGain);
        else
            captureGainN->setValue(GainSpinSpecialValue);

        captureGainN->setReadOnly(activeCamera()->getGainPermission() == IP_RO);

        connect(captureGainN, &QDoubleSpinBox::editingFinished, this, [this]()
        {
            if (captureGainN->value() != GainSpinSpecialValue)
                setGain(captureGainN->value());
        });
    }
    else
    {
        captureGainN->setEnabled(false);
        currentGainLabel->clear();
    }

    // Offset checks
    if (activeCamera()->hasOffset())
    {
        double min, max, step, value, targetCustomOffset;
        activeCamera()->getOffsetMinMaxStep(&min, &max, &step);

        // Allow the possibility of no Offset value at all.
        OffsetSpinSpecialValue = min - step;
        captureOffsetN->setRange(OffsetSpinSpecialValue, max);
        captureOffsetN->setSpecialValueText(i18n("--"));
        captureOffsetN->setEnabled(true);
        captureOffsetN->setSingleStep(step);
        activeCamera()->getOffset(&value);
        currentOffsetLabel->setText(QString::number(value, 'f', 0));

        targetCustomOffset = getOffset();

        // Set the custom Offset if we have one
        // otherwise it will not have an effect.
        if (targetCustomOffset > 0)
            captureOffsetN->setValue(targetCustomOffset);
        else
            captureOffsetN->setValue(OffsetSpinSpecialValue);

        captureOffsetN->setReadOnly(activeCamera()->getOffsetPermission() == IP_RO);

        connect(captureOffsetN, &QDoubleSpinBox::editingFinished, this, [this]()
        {
            if (captureOffsetN->value() != OffsetSpinSpecialValue)
                setOffset(captureOffsetN->value());
        });
    }
    else
    {
        captureOffsetN->setEnabled(false);
        currentOffsetLabel->clear();
    }
}

void Capture::setGuideChip(ISD::CameraChip * guideChip)
{
    // We should suspend guide in two scenarios:
    // 1. If guide chip is within the primary CCD, then we cannot download any data from guide chip while primary CCD is downloading.
    // 2. If we have two CCDs running from ONE driver (Multiple-Devices-Per-Driver mpdp is true). Same issue as above, only one download
    // at a time.
    // After primary CCD download is complete, we resume guiding.
    if (!devices()->getActiveCamera())
        return;

    state()->setSuspendGuidingOnDownload((devices()->getActiveCamera()->getChip(
            ISD::CameraChip::GUIDE_CCD) == guideChip) ||
                                         (guideChip->getCCD() == devices()->getActiveCamera() &&
                                          devices()->getActiveCamera()->getDriverInfo()->getAuxInfo().value("mdpd", false).toBool()));
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
    if (!devices()->getActiveCamera())
        return;

    int binx = 1, biny = 1;
    double min, max, step;
    int xstep = 0, ystep = 0;

    QString frameProp    = state()->useGuideHead() ? QString("GUIDER_FRAME") : QString("CCD_FRAME");
    QString exposureProp = state()->useGuideHead() ? QString("GUIDER_EXPOSURE") : QString("CCD_EXPOSURE");
    QString exposureElem = state()->useGuideHead() ? QString("GUIDER_EXPOSURE_VALUE") :
                           QString("CCD_EXPOSURE_VALUE");
    devices()->setActiveChip(state()->useGuideHead() ?
                             devices()->getActiveCamera()->getChip(
                                 ISD::CameraChip::GUIDE_CCD) :
                             devices()->getActiveCamera()->getChip(ISD::CameraChip::PRIMARY_CCD));

    captureFrameWN->setEnabled(devices()->getActiveChip()->canSubframe());
    captureFrameHN->setEnabled(devices()->getActiveChip()->canSubframe());
    captureFrameXN->setEnabled(devices()->getActiveChip()->canSubframe());
    captureFrameYN->setEnabled(devices()->getActiveChip()->canSubframe());

    captureBinHN->setEnabled(devices()->getActiveChip()->canBin());
    captureBinVN->setEnabled(devices()->getActiveChip()->canBin());

    QList<double> exposureValues;
    exposureValues << 0.01 << 0.02 << 0.05 << 0.1 << 0.2 << 0.25 << 0.5 << 1 << 1.5 << 2 << 2.5 << 3 << 5 << 6 << 7 << 8 << 9 <<
                   10 << 20 << 30 << 40 << 50 << 60 << 120 << 180 << 300 << 600 << 900 << 1200 << 1800;

    if (devices()->getActiveCamera()->getMinMaxStep(exposureProp, exposureElem, &min, &max, &step))
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
    state()->setExposureRange(exposureValues.first(), exposureValues.last());

    if (devices()->getActiveCamera()->getMinMaxStep(frameProp, "WIDTH", &min, &max, &step))
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

    if (devices()->getActiveCamera()->getMinMaxStep(frameProp, "HEIGHT", &min, &max, &step))
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

    if (devices()->getActiveCamera()->getMinMaxStep(frameProp, "X", &min, &max, &step))
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

    if (devices()->getActiveCamera()->getMinMaxStep(frameProp, "Y", &min, &max, &step))
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
    if (state()->useGuideHead() == false)
        cullToDSLRLimits();

    if (reset == 1 || state()->frameSettings().contains(devices()->getActiveChip()) == false)
    {
        QVariantMap settings;

        settings["x"]    = 0;
        settings["y"]    = 0;
        settings["w"]    = captureFrameWN->maximum();
        settings["h"]    = captureFrameHN->maximum();
        settings["binx"] = captureBinHN->value();
        settings["biny"] = captureBinVN->value();

        state()->frameSettings()[devices()->getActiveChip()] = settings;
    }
    else if (reset == 2 && state()->frameSettings().contains(devices()->getActiveChip()))
    {
        QVariantMap settings = state()->frameSettings()[devices()->getActiveChip()];
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
        settings["binx"] = captureBinHN->value();
        settings["biny"] = captureBinVN->value();

        state()->frameSettings()[devices()->getActiveChip()] = settings;
    }

    if (state()->frameSettings().contains(devices()->getActiveChip()))
    {
        QVariantMap settings = state()->frameSettings()[devices()->getActiveChip()];
        int x = settings["x"].toInt();
        int y = settings["y"].toInt();
        int w = settings["w"].toInt();
        int h = settings["h"].toInt();

        if (devices()->getActiveChip()->canBin())
        {
            devices()->getActiveChip()->getMaxBin(&binx, &biny);
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

void Capture::processCameraNumber(INDI::Property prop)
{
    if (devices()->getActiveCamera() == nullptr)
        return;

    if ((prop.isNameMatch("CCD_FRAME") && state()->useGuideHead() == false) ||
            (prop.isNameMatch("GUIDER_FRAME") && state()->useGuideHead()))
        updateFrameProperties();
    else if ((prop.isNameMatch("CCD_INFO") && state()->useGuideHead() == false) ||
             (prop.isNameMatch("GUIDER_INFO") && state()->useGuideHead()))
        updateFrameProperties(2);
    else if (prop.isNameMatch("CCD_TRANSFER_FORMAT") || prop.isNameMatch("CCD_CAPTURE_FORMAT"))
        updateCaptureFormats();
    else if (prop.isNameMatch("CCD_CONTROLS"))
    {
        auto nvp = prop.getNumber();
        auto gain = nvp->findWidgetByName("Gain");
        if (gain)
            currentGainLabel->setText(QString::number(gain->value, 'f', 0));
        auto offset = nvp->findWidgetByName("Offset");
        if (offset)
            currentOffsetLabel->setText(QString::number(offset->value, 'f', 0));
    }
    else if (prop.isNameMatch("CCD_GAIN"))
    {
        auto nvp = prop.getNumber();
        currentGainLabel->setText(QString::number(nvp->at(0)->getValue(), 'f', 0));
    }
    else if (prop.isNameMatch("CCD_OFFSET"))
    {
        auto nvp = prop.getNumber();
        currentOffsetLabel->setText(QString::number(nvp->at(0)->getValue(), 'f', 0));
    }
}

void Capture::syncFrameType(const QString &name)
{
    if (!activeCamera() || name != activeCamera()->getDeviceName())
        return;

    QStringList frameTypes = process()->frameTypes();

    captureTypeS->clear();

    if (frameTypes.isEmpty())
        captureTypeS->setEnabled(false);
    else
    {
        captureTypeS->setEnabled(true);
        captureTypeS->addItems(frameTypes);
        ISD::CameraChip *tChip = devices()->getActiveCamera()->getChip(ISD::CameraChip::PRIMARY_CCD);
        captureTypeS->setCurrentIndex(tChip->getFrameType());
    }
}

QString Capture::filterWheel()
{
    if (devices()->filterWheel())
        return devices()->filterWheel()->getDeviceName();

    return QString();
}

bool Capture::setFilter(const QString &filter)
{
    if (devices()->filterWheel())
    {
        FilterPosCombo->setCurrentText(filter);
        return true;
    }

    return false;
}

QString Capture::filter()
{
    return FilterPosCombo->currentText();
}

void Capture::updateCurrentFilterPosition()
{
    const QString currentFilterText = FilterPosCombo->itemText(m_FilterManager->getFilterPosition() - 1);
    state()->setCurrentFilterPosition(m_FilterManager->getFilterPosition(),
                                      currentFilterText,
                                      m_FilterManager->getFilterLock(currentFilterText));
}

void Capture::refreshFilterSettings()
{
    FilterPosCombo->clear();

    if (!devices()->filterWheel())
    {
        FilterPosLabel->setEnabled(false);
        FilterPosCombo->setEnabled(false);
        filterEditB->setEnabled(false);

        devices()->setFilterManager(m_FilterManager);
        return;
    }

    FilterPosLabel->setEnabled(true);
    FilterPosCombo->setEnabled(true);
    filterEditB->setEnabled(true);

    setupFilterManager();

    process()->updateFilterInfo();

    FilterPosCombo->addItems(process()->filterLabels());

    updateCurrentFilterPosition();

    filterEditB->setEnabled(state()->getCurrentFilterPosition() > 0);

    FilterPosCombo->setCurrentIndex(state()->getCurrentFilterPosition() - 1);
}

void Capture::processingFITSfinished(bool success)
{
    // do nothing in case of failure
    if (success == false)
        return;

    // If this is a preview job, make sure to enable preview button after
    if (devices()->getActiveCamera()
            && devices()->getActiveCamera()->getUploadMode() != ISD::Camera::UPLOAD_LOCAL)
        previewB->setEnabled(true);

    imageCapturingCompleted();
}

void Capture::imageCapturingCompleted()
{
    SequenceJob *thejob = activeJob();

    if (!thejob)
        return;

    // In case we're framing, let's return quickly to continue the process.
    if (state()->isLooping())
    {
        captureStatusWidget->setStatus(i18n("Framing..."), Qt::darkGreen);
        return;
    }

    // If fast exposure is off, disconnect exposure progress
    // otherwise, keep it going since it fires off from driver continuous capture process.
    if (devices()->getActiveCamera()->isFastExposureEnabled() == false)
        DarkLibrary::Instance()->disconnect(this);

    // Do not display notifications for very short captures
    if (thejob->getCoreProperty(SequenceJob::SJ_Exposure).toDouble() >= 1)
        KSNotification::event(QLatin1String("EkosCaptureImageReceived"), i18n("Captured image received"),
                              KSNotification::Capture);

    // If it was initially set as pure preview job and NOT as preview for calibration
    if (thejob->jobType() == SequenceJob::JOBTYPE_PREVIEW)
        return;

    /* The image progress has now one more capture */
    imgProgress->setValue(thejob->getCompleted());
}

void Capture::captureStopped()
{
    imgProgress->reset();
    imgProgress->setEnabled(false);

    frameRemainingTime->setText("--:--:--");
    jobRemainingTime->setText("--:--:--");
    frameInfoLabel->setText(i18n("Expose (-/-):"));

    // stopping to CAPTURE_IDLE means that capturing will continue automatically
    auto captureState = state()->getCaptureState();
    if (captureState == CAPTURE_ABORTED || captureState == CAPTURE_SUSPENDED || captureState == CAPTURE_COMPLETE)
        updateStartButtons(false, false);
}

void Capture::updateTargetDistance(double targetDiff)
{
    // ensure that the drift is visible
    targetDriftLabel->setVisible(true);
    targetDrift->setVisible(true);
    targetDriftUnit->setVisible(true);
    // update the drift value
    targetDrift->setText(QString("%L1").arg(targetDiff, 0, 'd', 1));
}

void Capture::captureImageStarted()
{
    if (devices()->filterWheel() != nullptr)
    {
        // JM 2021.08.23 Call filter info to set the active filter wheel in the camera driver
        // so that it may snoop on the active filter
        process()->updateFilterInfo();
        updateCurrentFilterPosition();
    }

    // necessary since the status widget doesn't store the calibration stage
    if (activeJob()->getCalibrationStage() == SequenceJobState::CAL_CALIBRATION)
        captureStatusWidget->setStatus(i18n("Calibrating..."), Qt::yellow);
}

namespace
{
QString frameLabel(CCDFrameType type, const QString &filter)
{
    switch(type)
    {
        case FRAME_LIGHT:
            if (filter.size() == 0)
                return CCDFrameTypeNames[type];
            else
                return filter;
            break;
        case FRAME_FLAT:
            if (filter.size() == 0)
                return CCDFrameTypeNames[type];
            else
                return QString("%1 %2").arg(filter).arg(CCDFrameTypeNames[type]);
            break;
        case FRAME_BIAS:
        case FRAME_DARK:
        case FRAME_NONE:
        default:
            return CCDFrameTypeNames[type];
    }
}
}

void Capture::captureRunning()
{
    emit captureStarting(activeJob()->getCoreProperty(SequenceJob::SJ_Exposure).toDouble(),
                         activeJob()->getCoreProperty(SequenceJob::SJ_Filter).toString());
    appendLogText(i18n("Capturing %1-second %2 image...",
                       QString("%L1").arg(activeJob()->getCoreProperty(SequenceJob::SJ_Exposure).toDouble(), 0, 'f', 3),
                       activeJob()->getCoreProperty(SequenceJob::SJ_Filter).toString()));
    frameInfoLabel->setText(QString("%1 (%L3/%L4):").arg(frameLabel(activeJob()->getFrameType(),
                            activeJob()->getCoreProperty(SequenceJob::SJ_Filter).toString()))
                            .arg(activeJob()->getCompleted()).arg(activeJob()->getCoreProperty(
                                        SequenceJob::SJ_Count).toInt()));
    // ensure that the download time label is visible
    avgDownloadTime->setVisible(true);
    avgDownloadLabel->setVisible(true);
    secLabel->setVisible(true);
    // show estimated download time
    avgDownloadTime->setText(QString("%L1").arg(state()->averageDownloadTime(), 0, 'd', 2));
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

void Capture::updateDownloadProgress(double downloadTimeLeft)
{
    frameRemainingTime->setText(state()->imageCountDown().toString("hh:mm:ss"));
    emit newDownloadProgress(downloadTimeLeft);
}

void Capture::updateCaptureCountDown(int deltaMillis)
{
    state()->imageCountDownAddMSecs(deltaMillis);
    state()->sequenceCountDownAddMSecs(deltaMillis);
    frameRemainingTime->setText(state()->imageCountDown().toString("hh:mm:ss"));
    jobRemainingTime->setText(state()->sequenceCountDown().toString("hh:mm:ss"));
}

void Capture::updateCCDTemperature(double value)
{
    if (cameraTemperatureS->isEnabled() == false && devices()->getActiveCamera())
    {
        if (devices()->getActiveCamera()->getPermission("CCD_TEMPERATURE") != IP_RO)
            process()->checkCamera();
    }

    temperatureOUT->setText(QString("%L1").arg(value, 0, 'f', 2));

    if (cameraTemperatureN->cleanText().isEmpty())
        cameraTemperatureN->setValue(value);
}

void Capture::updateRotatorAngle(double value)
{
    IPState RState = devices()->rotator()->absoluteAngleState();
    if (RState == IPS_OK)
        m_RotatorControlPanel->updateRotator(value);
    else
        m_RotatorControlPanel->updateGauge(value);
}

void Capture::addJob(SequenceJob *job)
{
    // add the job to the job list
    state()->allJobs().append(job);

    // create a new row
    createNewJobTableRow(job);
}

SequenceJob *Capture::createJob(SequenceJob::SequenceJobType jobtype, FilenamePreviewType filenamePreview)
{
    SequenceJob *job = new SequenceJob(devices(), state(), jobtype);

    updateJobFromUI(job, filenamePreview);

    // Nothing more to do if preview or for placeholder calculations
    if (jobtype == SequenceJob::JOBTYPE_PREVIEW || filenamePreview != NOT_PREVIEW)
        return job;

    // check if the upload paths are correct
    if (checkUploadPaths(filenamePreview) == false)
        return nullptr;

    // all other jobs will be added to the job list
    state()->allJobs().append(job);

    // create a new row
    createNewJobTableRow(job);

    return job;
}

void Ekos::Capture::createNewJobTableRow(SequenceJob *job)
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


void Capture::editJobFinished()
{
    if (queueTable->currentRow() < 0)
        qCWarning(KSTARS_EKOS_CAPTURE()) << "Editing finished, but no row selected!";

    int currentRow = queueTable->currentRow();
    SequenceJob *job = state()->allJobs().at(currentRow);
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

bool Capture::removeJob(int index)
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

    SequenceJob * job = state()->allJobs().at(index);
    // remove completed frame counts from frame count map
    state()->removeCapturedFrameCount(job->getSignature(), job->getCompleted());
    // remove the job
    state()->allJobs().removeOne(job);
    if (job == activeJob())
        state()->setActiveJob(nullptr);

    delete job;

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

void Capture::moveJob(bool up)
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

    SequenceJob * job = state()->allJobs().takeAt(currentRow);

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

void Capture::newTargetName(const QString &name)
{
    targetNameT->setText(name);
    generatePreviewFilename();
}

void Capture::setBusy(bool enable)
{
    previewB->setEnabled(!enable);
    loopB->setEnabled(!enable);
    opticalTrainCombo->setEnabled(!enable);
    trainB->setEnabled(!enable);

    foreach (QAbstractButton * button, queueEditButtonGroup->buttons())
        button->setEnabled(!enable);
}

void Capture::jobPrepared(SequenceJob * job)
{

    int index = state()->allJobs().indexOf(job);
    if (index >= 0)
        queueTable->selectRow(index);

    if (activeJob()->jobType() != SequenceJob::JOBTYPE_PREVIEW)
    {
        // set the progress info
        imgProgress->setEnabled(true);
        imgProgress->setMaximum(activeJob()->getCoreProperty(SequenceJob::SJ_Count).toInt());
        imgProgress->setValue(activeJob()->getCompleted());
    }
}

void Capture::jobExecutionPreparationStarted()
{
    if (activeJob() == nullptr)
    {
        // this should never happen
        qWarning(KSTARS_EKOS_CAPTURE) << "jobExecutionPreparationStarted with null state()->getActiveJob().";
        return;
    }
    if (activeJob()->jobType() == SequenceJob::JOBTYPE_PREVIEW)
        updateStartButtons(true, false);
}

void Capture::updatePrepareState(CaptureState prepareState)
{
    state()->setCaptureState(prepareState);

    if (activeJob() == nullptr)
    {
        qWarning(KSTARS_EKOS_CAPTURE) << "updatePrepareState with null activeJob().";
        // Everything below depends on activeJob(). Just return.
        return;
    }

    switch (prepareState)
    {
        case CAPTURE_SETTING_TEMPERATURE:
            appendLogText(i18n("Setting temperature to %1 °C...", activeJob()->getTargetTemperature()));
            captureStatusWidget->setStatus(i18n("Set Temp to %1 °C...", activeJob()->getTargetTemperature()),
                                           Qt::yellow);
            break;
        case CAPTURE_GUIDER_DRIFT:
            appendLogText(i18n("Waiting for guide drift below %1\"...",
                               activeJob()->getTargetStartGuiderDrift()));
            captureStatusWidget->setStatus(i18n("Wait for Guider < %1\"...",
                                                activeJob()->getTargetStartGuiderDrift()), Qt::yellow);
            break;

        case CAPTURE_SETTING_ROTATOR:
            appendLogText(i18n("Setting camera to %1 degrees E of N...", activeJob()->getTargetRotation()));
            captureStatusWidget->setStatus(i18n("Set Camera to %1 deg...", activeJob()->getTargetRotation()),
                                           Qt::yellow);
            break;

        default:
            break;

    }
}

void Capture::setFocusTemperatureDelta(double focusTemperatureDelta, double absTemperture)
{
    Q_UNUSED(absTemperture);
    // This produces too much log spam
    // Maybe add a threshold to report later?
    //qCDebug(KSTARS_EKOS_CAPTURE) << "setFocusTemperatureDelta: " << focusTemperatureDelta;
    state()->getRefocusState()->setFocusTemperatureDelta(focusTemperatureDelta);
}

void Capture::setGuideDeviation(double delta_ra, double delta_dec)
{
    const double deviation_rms = std::hypot(delta_ra, delta_dec);

    // forward it to the state machine
    state()->setGuideDeviation(deviation_rms);

}

void Capture::setFocusStatus(FocusState newstate)
{
    // directly forward it to the state machine
    state()->updateFocusState(newstate);
}

void Capture::updateFocusStatus(FocusState newstate)
{
    if ((state()->getRefocusState()->isRefocusing()
            || state()->getRefocusState()->isInSequenceFocus()) && activeJob()
            && activeJob()->getStatus() == JOB_BUSY)
    {
        switch (newstate)
        {
            case FOCUS_COMPLETE:
                appendLogText(i18n("Focus complete."));
                captureStatusWidget->setStatus(i18n("Focus complete."), Qt::yellow);
                break;
            case FOCUS_FAILED:
            case FOCUS_ABORTED:
                captureStatusWidget->setStatus(i18n("Autofocus failed."), Qt::darkRed);
                break;
            default:
                // otherwise do nothing
                break;
        }
    }
}



void Capture::updateMeridianFlipStage(MeridianFlipState::MFStage stage)
{
    // update UI
    if (getMeridianFlipState()->getMeridianFlipStage() != stage)
    {
        switch (stage)
        {
            case MeridianFlipState::MF_READY:
                if (state()->getCaptureState() == CAPTURE_PAUSED)
                {
                    // paused after meridian flip requested
                    captureStatusWidget->setStatus(i18n("Paused..."), Qt::yellow);
                }
                break;

            case MeridianFlipState::MF_INITIATED:
                captureStatusWidget->setStatus(i18n("Meridian Flip..."), Qt::yellow);
                KSNotification::event(QLatin1String("MeridianFlipStarted"), i18n("Meridian flip started"), KSNotification::Capture);
                break;

            case MeridianFlipState::MF_COMPLETED:
                captureStatusWidget->setStatus(i18n("Flip complete."), Qt::yellow);

                // Reset HFR pixels to file value after meridian flip
                if (state()->getRefocusState()->isInSequenceFocus())
                    m_LimitsUI->limitFocusHFRN->setValue(state()->getFileHFR());
                break;

            default:
                break;
        }
    }
}

void Capture::setRotatorReversed(bool toggled)
{
    m_RotatorControlPanel->reverseDirection->setEnabled(true);

    m_RotatorControlPanel->reverseDirection->blockSignals(true);
    m_RotatorControlPanel->reverseDirection->setChecked(toggled);
    m_RotatorControlPanel->reverseDirection->blockSignals(false);
}

void Capture::saveFITSDirectory()
{
    QString dir =
        QFileDialog::getExistingDirectory(Manager::Instance(), i18nc("@title:window", "FITS Save Directory"),
                                          dirPath.toLocalFile());
    if (dir.isEmpty())
        return;

    fileDirectoryT->setText(QDir::toNativeSeparators(dir));
}

void Capture::loadSequenceQueue()
{
    QUrl fileURL = QFileDialog::getOpenFileUrl(Manager::Instance(), i18nc("@title:window", "Open Ekos Sequence Queue"),
                   dirPath,
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

bool Capture::loadSequenceQueue(const QString &fileURL, QString targetName)
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

    const bool result = process()->loadSequenceQueue(fileURL, targetName);
    // cancel if loading fails
    if (result == false)
        return result;

    // update general settings
    setObserverName(state()->observerName());
    syncGUIToGeneralSettings();

    // select the first one of the loaded jobs
    if (state()->allJobs().size() > 0)
        syncGUIToJob(state()->allJobs().first());

    // update save button tool tip
    queueSaveB->setToolTip("Save to " + sFile.fileName());

    return true;
}

void Capture::saveSequenceQueue()
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
                                dirPath,
                                "Ekos Sequence Queue (*.esq)"));
        // if user presses cancel
        if (state()->sequenceURL().isEmpty())
        {
            state()->setSequenceURL(backupCurrent);
            return;
        }

        dirPath = QUrl(state()->sequenceURL().url(QUrl::RemoveFilename));

        if (state()->sequenceURL().toLocalFile().endsWith(QLatin1String(".esq")) == false)
            state()->setSequenceURL(QUrl("file:" + state()->sequenceURL().toLocalFile() + ".esq"));

    }

    if (state()->sequenceURL().isValid())
    {
        if ((process()->saveSequenceQueue(state()->sequenceURL().toLocalFile())) == false)
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

void Capture::saveSequenceQueueAs()
{
    state()->setSequenceURL(QUrl(""));
    saveSequenceQueue();
}

bool Capture::saveSequenceQueue(const QString &path)
{
    // forward it to the process engine
    return process()->saveSequenceQueue(path);
}

void Capture::resetJobs()
{
    // Stop any running capture
    stop();

    // If a job is selected for edit, reset only that job
    if (m_JobUnderEdit == true)
    {
        SequenceJob * job = state()->allJobs().at(queueTable->currentRow());
        if (nullptr != job)
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

        foreach (SequenceJob * job, state()->allJobs())
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

void Capture::ignoreSequenceHistory()
{
    // This function is called independently from the Scheduler or the UI, so honor the change
    state()->setIgnoreJobProgress(true);
}

void Capture::syncGUIToJob(SequenceJob * job)
{
    if (job == nullptr)
    {
        qWarning(KSTARS_EKOS_CAPTURE) << "syncGuiToJob with null job.";
        // Everything below depends on job. Just return.
        return;
    }

    const auto roi = job->getCoreProperty(SequenceJob::SJ_ROI).toRect();

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
    captureTypeS->setCurrentIndex(job->getFrameType());
    captureCountN->setValue(job->getCoreProperty(SequenceJob::SJ_Count).toInt());
    captureDelayN->setValue(job->getCoreProperty(SequenceJob::SJ_Delay).toInt() / 1000);
    targetNameT->setText(job->getCoreProperty(SequenceJob::SJ_TargetName).toString());
    fileDirectoryT->setText(job->getCoreProperty(SequenceJob::SJ_LocalDirectory).toString());
    fileUploadModeS->setCurrentIndex(job->getUploadMode());
    fileRemoteDirT->setEnabled(fileUploadModeS->currentIndex() != 0);
    fileRemoteDirT->setText(job->getCoreProperty(SequenceJob::SJ_RemoteDirectory).toString());
    placeholderFormatT->setText(job->getCoreProperty(SequenceJob::SJ_PlaceholderFormat).toString());
    formatSuffixN->setValue(job->getCoreProperty(SequenceJob::SJ_PlaceholderSuffix).toUInt());

    // Temperature Options
    cameraTemperatureS->setChecked(job->getCoreProperty(SequenceJob::SJ_EnforceTemperature).toBool());
    if (job->getCoreProperty(SequenceJob::SJ_EnforceTemperature).toBool())
        cameraTemperatureN->setValue(job->getTargetTemperature());

    // Start guider drift options
    m_LimitsUI->startGuiderDriftS->setChecked(job->getCoreProperty(SequenceJob::SJ_EnforceStartGuiderDrift).toBool());
    if (job->getCoreProperty(SequenceJob::SJ_EnforceStartGuiderDrift).toBool())
        m_LimitsUI->startGuiderDriftN->setValue(job->getTargetStartGuiderDrift());

    // Flat field options
    calibrationB->setEnabled(job->getFrameType() != FRAME_LIGHT);
    generateDarkFlatsB->setEnabled(job->getFrameType() != FRAME_LIGHT);
    state()->setFlatFieldDuration(job->getFlatFieldDuration());
    state()->setCalibrationPreAction(job->getCalibrationPreAction());
    state()->setTargetADU(job->getCoreProperty(SequenceJob::SJ_TargetADU).toDouble());
    state()->setTargetADUTolerance(job->getCoreProperty(SequenceJob::SJ_TargetADUTolerance).toDouble());
    state()->setWallCoord(job->getWallCoord());

    // Script options
    state()->setScripts(job->getScripts());

    // Custom Properties
    customPropertiesDialog->setCustomProperties(job->getCustomProperties());

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

    emit settingsUpdated(getPresetSettings());
}

void Capture::syncGUIToGeneralSettings()
{
    m_LimitsUI->startGuiderDriftS->setChecked(Options::enforceStartGuiderDrift());
    m_LimitsUI->startGuiderDriftN->setValue(Options::startGuideDeviation());
    m_LimitsUI->limitGuideDeviationS->setChecked(Options::enforceGuideDeviation());
    m_LimitsUI->limitGuideDeviationN->setValue(Options::guideDeviation());
    m_LimitsUI->limitGuideDeviationRepsN->setValue(static_cast<int>(Options::guideDeviationReps()));
    m_LimitsUI->limitFocusHFRS->setChecked(Options::enforceAutofocusHFR());
    m_LimitsUI->limitFocusHFRN->setValue(Options::hFRDeviation());
    m_LimitsUI->limitFocusDeltaTS->setChecked(Options::enforceAutofocusOnTemperature());
    m_LimitsUI->limitFocusDeltaTN->setValue(Options::maxFocusTemperatureDelta());
    m_LimitsUI->limitRefocusS->setChecked(Options::enforceRefocusEveryN());
    m_LimitsUI->limitRefocusN->setValue(static_cast<int>(Options::refocusEveryN()));
    m_LimitsUI->meridianRefocusS->setChecked(Options::refocusAfterMeridianFlip());
}

QJsonObject Capture::getPresetSettings()
{
    QJsonObject settings;

    // Try to get settings value
    // if not found, fallback to camera value
    double gain = -1;
    if (GainSpinSpecialValue > INVALID_VALUE && captureGainN->value() > GainSpinSpecialValue)
        gain = captureGainN->value();
    else if (devices()->getActiveCamera() && devices()->getActiveCamera()->hasGain())
        devices()->getActiveCamera()->getGain(&gain);

    double offset = -1;
    if (OffsetSpinSpecialValue > INVALID_VALUE && captureOffsetN->value() > OffsetSpinSpecialValue)
        offset = captureOffsetN->value();
    else if (devices()->getActiveCamera() && devices()->getActiveCamera()->hasOffset())
        devices()->getActiveCamera()->getOffset(&offset);

    int iso = -1;
    if (captureISOS)
        iso = captureISOS->currentIndex();
    else if (devices()->getActiveCamera())
        iso = devices()->getActiveCamera()->getChip(ISD::CameraChip::PRIMARY_CCD)->getISOIndex();

    settings.insert("optical_train", opticalTrainCombo->currentText());
    settings.insert("filter", FilterPosCombo->currentText());
    settings.insert("dark", darkB->isChecked());
    settings.insert("exp", captureExposureN->value());
    settings.insert("bin", captureBinHN->value());
    settings.insert("iso", iso);
    settings.insert("frameType", captureTypeS->currentIndex());
    settings.insert("captureFormat", captureFormatS->currentIndex());
    settings.insert("transferFormat", captureEncodingS->currentIndex());
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

bool Capture::selectJob(QModelIndex i)
{
    if (i.row() < 0 || (i.row() + 1) > state()->allJobs().size())
        return false;

    SequenceJob * job = state()->allJobs().at(i.row());

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

void Capture::editJob(QModelIndex i)
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

void Capture::resetJobEdit(bool cancelled)
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

void Capture::setMaximumGuidingDeviation(bool enable, double value)
{
    m_LimitsUI->limitGuideDeviationS->setChecked(enable);
    if (enable)
        m_LimitsUI->limitGuideDeviationN->setValue(value);
}

void Capture::setInSequenceFocus(bool enable, double HFR)
{
    m_LimitsUI->limitFocusHFRS->setChecked(enable);
    if (enable)
        m_LimitsUI->limitFocusHFRN->setValue(HFR);
}

void Capture::clearSequenceQueue()
{
    state()->setActiveJob(nullptr);
    while (queueTable->rowCount() > 0)
        queueTable->removeRow(0);
    qDeleteAll(state()->allJobs());
    state()->allJobs().clear();

    while (state()->getSequence().count())
        state()->getSequence().pop_back();
    emit sequenceChanged(state()->getSequence());
}

void Capture::setAlignStatus(AlignState newstate)
{
    // forward it directly to the state machine
    state()->setAlignState(newstate);
}

void Capture::setGuideStatus(GuideState newstate)
{
    // forward it directly to the state machine
    state()->setGuideState(newstate);
}

void Capture::checkFrameType(int index)
{
    calibrationB->setEnabled(index != FRAME_LIGHT);
    generateDarkFlatsB->setEnabled(index != FRAME_LIGHT);
}

void Capture::clearAutoFocusHFR()
{
    // If HFR limit was set from file, we cannot override it.
    if (state()->getFileHFR() > 0)
        return;

    m_LimitsUI->limitFocusHFRN->setValue(0);
    //firstAutoFocus = true;
}

void Capture::openCalibrationDialog()
{
    QDialog calibrationDialog(this);

    Ui_calibrationOptions calibrationOptions;
    calibrationOptions.setupUi(&calibrationDialog);

    calibrationOptions.parkMountC->setEnabled(devices()->mount() && devices()->mount()->canPark());
    calibrationOptions.parkDomeC->setEnabled(devices()->dome() && devices()->dome()->canPark());

    calibrationOptions.parkMountC->setChecked(false);
    calibrationOptions.parkDomeC->setChecked(false);
    calibrationOptions.gotoWallC->setChecked(false);

    calibrationOptions.parkMountC->setChecked(state()->calibrationPreAction() & ACTION_PARK_MOUNT);
    calibrationOptions.parkDomeC->setChecked(state()->calibrationPreAction() & ACTION_PARK_DOME);
    if (state()->calibrationPreAction() & ACTION_WALL)
    {
        calibrationOptions.gotoWallC->setChecked(true);
        calibrationOptions.azBox->setText(state()->wallCoord().az().toDMSString());
        calibrationOptions.altBox->setText(state()->wallCoord().alt().toDMSString());
    }

    switch (state()->flatFieldDuration())
    {
        case DURATION_MANUAL:
            calibrationOptions.manualDurationC->setChecked(true);
            break;

        case DURATION_ADU:
            calibrationOptions.ADUC->setChecked(true);
            calibrationOptions.ADUValue->setValue(static_cast<int>(std::round(state()->targetADU())));
            calibrationOptions.ADUTolerance->setValue(static_cast<int>(std::round(state()->targetADUTolerance())));
            break;
    }

    // avoid combination of ACTION_WALL and ACTION_PARK_MOUNT
    connect(calibrationOptions.gotoWallC, &QCheckBox::clicked, [&](bool checked)
    {
        if (checked)
            calibrationOptions.parkMountC->setChecked(false);
    });
    connect(calibrationOptions.parkMountC, &QCheckBox::clicked, [&](bool checked)
    {
        if (checked)
            calibrationOptions.gotoWallC->setChecked(false);
    });

    if (calibrationDialog.exec() == QDialog::Accepted)
    {
        state()->setCalibrationPreAction(ACTION_NONE);
        if (calibrationOptions.parkMountC->isChecked())
            state()->setCalibrationPreAction(state()->calibrationPreAction() | ACTION_PARK_MOUNT);
        if (calibrationOptions.parkDomeC->isChecked())
            state()->setCalibrationPreAction(state()->calibrationPreAction() | ACTION_PARK_DOME);
        if (calibrationOptions.gotoWallC->isChecked())
        {
            dms wallAz, wallAlt;
            bool azOk = false, altOk = false;

            wallAz  = calibrationOptions.azBox->createDms(&azOk);
            wallAlt = calibrationOptions.altBox->createDms(&altOk);

            if (azOk && altOk)
            {
                state()->setCalibrationPreAction((state()->calibrationPreAction() & ~ACTION_PARK_MOUNT) | ACTION_WALL);
                state()->wallCoord().setAz(wallAz);
                state()->wallCoord().setAlt(wallAlt);
            }
            else
            {
                calibrationOptions.gotoWallC->setChecked(false);
                KSNotification::error(i18n("Wall coordinates are invalid."));
            }
        }

        if (calibrationOptions.manualDurationC->isChecked())
            state()->setFlatFieldDuration(DURATION_MANUAL);
        else
        {
            state()->setFlatFieldDuration(DURATION_ADU);
            state()->setTargetADU(calibrationOptions.ADUValue->value());
            state()->setTargetADUTolerance(calibrationOptions.ADUTolerance->value());
        }

        state()->setDirty(true);

        Options::setCalibrationPreActionIndex(state()->calibrationPreAction());
        Options::setCalibrationFlatDurationIndex(state()->flatFieldDuration());
        Options::setCalibrationWallAz(state()->wallCoord().az().Degrees());
        Options::setCalibrationWallAlt(state()->wallCoord().alt().Degrees());
        Options::setCalibrationADUValue(static_cast<uint>(std::round(state()->targetADU())));
        Options::setCalibrationADUValueTolerance(static_cast<uint>(std::round(state()->targetADUTolerance())));
    }
}

bool Capture::setVideoLimits(uint16_t maxBufferSize, uint16_t maxPreviewFPS)
{
    if (devices()->getActiveCamera() == nullptr)
        return false;

    return devices()->getActiveCamera()->setStreamLimits(maxBufferSize, maxPreviewFPS);
}

void Capture::setVideoStreamEnabled(bool enabled)
{
    if (enabled)
    {
        liveVideoB->setChecked(true);
        liveVideoB->setIcon(QIcon::fromTheme("camera-on"));
    }
    else
    {
        liveVideoB->setChecked(false);
        liveVideoB->setIcon(QIcon::fromTheme("camera-ready"));
    }
}

void Capture::setMountStatus(ISD::Mount::Status newState)
{
    switch (newState)
    {
        case ISD::Mount::MOUNT_PARKING:
        case ISD::Mount::MOUNT_SLEWING:
        case ISD::Mount::MOUNT_MOVING:
            previewB->setEnabled(false);
            liveVideoB->setEnabled(false);
            // Only disable when button is "Start", and not "Stopped"
            // If mount is in motion, Stopped button should always be enabled to terminate
            // the sequence
            if (state()->isBusy() == false)
                startB->setEnabled(false);
            break;

        default:
            if (state()->isBusy() == false)
            {
                previewB->setEnabled(true);
                if (devices()->getActiveCamera())
                    liveVideoB->setEnabled(devices()->getActiveCamera()->hasVideoStream());
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
    observersDialog.setWindowTitle(i18nc("@title:window", "Select Current Observer"));

    QLabel label(i18n("Current Observer:"));

    QComboBox observerCombo(&observersDialog);
    observerCombo.addItems(observers);
    observerCombo.setCurrentText(getObserverName());
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
        observerCombo.setCurrentText(getObserverName());

    });

    QHBoxLayout * layout = new QHBoxLayout;
    layout->addWidget(&label);
    layout->addWidget(&observerCombo);
    layout->addWidget(&manageObserver);

    observersDialog.setLayout(layout);

    observersDialog.exec();
    setObserverName(observerCombo.currentText());
}

void Capture::setAlignResults(double solverPA, double ra, double de, double pixscale)
{
    Q_UNUSED(ra)
    Q_UNUSED(de)
    Q_UNUSED(pixscale)
    if (devices()->rotator() && m_RotatorControlPanel)
        m_RotatorControlPanel->refresh(solverPA);
}

void Capture::setFilterStatus(FilterState filterState)
{
    if (filterState != state()->getFilterManagerState())
        qCDebug(KSTARS_EKOS_CAPTURE) << "Filter state changed from" << Ekos::getFilterStatusString(
                                         state()->getFilterManagerState()) << "to" << Ekos::getFilterStatusString(filterState);
    if (state()->getCaptureState() == CAPTURE_CHANGING_FILTER)
    {
        switch (filterState)
        {
            case FILTER_OFFSET:
                appendLogText(i18n("Changing focus offset by %1 steps...",
                                   m_FilterManager->getTargetFilterOffset()));
                break;

            case FILTER_CHANGE:
                appendLogText(i18n("Changing filter to %1...",
                                   FilterPosCombo->itemText(m_FilterManager->getTargetFilterPosition() - 1)));
                break;

            case FILTER_AUTOFOCUS:
                appendLogText(i18n("Auto focus on filter change..."));
                clearAutoFocusHFR();
                break;

            case FILTER_IDLE:
                if (state()->getFilterManagerState() == FILTER_CHANGE)
                {
                    appendLogText(i18n("Filter set to %1.",
                                       FilterPosCombo->itemText(m_FilterManager->getTargetFilterPosition() - 1)));
                }
                break;

            default:
                break;
        }
    }
    state()->setFilterManagerState(filterState);
}

void Capture::setupFilterManager()
{
    // Do we have an existing filter manager?
    if (m_FilterManager)
        m_FilterManager->disconnect(this);

    // Create new or refresh device
    Manager::Instance()->createFilterManager(devices()->filterWheel());

    // Return global filter manager for this filter wheel.
    Manager::Instance()->getFilterManager(devices()->filterWheel()->getDeviceName(), m_FilterManager);

    devices()->setFilterManager(m_FilterManager);

    connect(m_FilterManager.get(), &FilterManager::updated, this, [this]()
    {
        emit filterManagerUpdated(devices()->filterWheel());
    });

    // display capture status changes
    connect(m_FilterManager.get(), &FilterManager::newStatus, this, &Capture::newFilterStatus);

    connect(filterManagerB, &QPushButton::clicked, this, [this]()
    {
        m_FilterManager->refreshFilterModel();
        m_FilterManager->show();
        m_FilterManager->raise();
    });

    connect(m_FilterManager.get(), &FilterManager::ready, this, &Capture::updateCurrentFilterPosition);

    connect(m_FilterManager.get(), &FilterManager::failed, this, [this]()
    {
        if (activeJob())
        {
            appendLogText(i18n("Filter operation failed."));
            abort();
        }
    });

    // filter changes
    connect(m_FilterManager.get(), &FilterManager::newStatus, this, &Capture::setFilterStatus);

    // display capture status changes
    connect(m_FilterManager.get(), &FilterManager::newStatus, captureStatusWidget, &LedStatusWidget::setFilterState);

    connect(m_FilterManager.get(), &FilterManager::labelsChanged, this, [this]()
    {
        FilterPosCombo->clear();
        FilterPosCombo->addItems(m_FilterManager->getFilterLabels());
        FilterPosCombo->setCurrentIndex(m_FilterManager->getFilterPosition() - 1);
        updateCurrentFilterPosition();
    });

    connect(m_FilterManager.get(), &FilterManager::positionChanged, this, [this]()
    {
        FilterPosCombo->setCurrentIndex(m_FilterManager->getFilterPosition() - 1);
        updateCurrentFilterPosition();
    });
}

void Capture::addDSLRInfo(const QString &model, uint32_t maxW, uint32_t maxH, double pixelW, double pixelH)
{
    // Check if model already exists
    auto pos = std::find_if(state()->DSLRInfos().begin(), state()->DSLRInfos().end(), [model](const auto & oneDSLRInfo)
    {
        return (oneDSLRInfo["Model"] == model);
    });

    if (pos != state()->DSLRInfos().end())
    {
        KStarsData::Instance()->userdb()->DeleteDSLRInfo(model);
        state()->DSLRInfos().removeOne(*pos);
    }

    QMap<QString, QVariant> oneDSLRInfo;
    oneDSLRInfo["Model"] = model;
    oneDSLRInfo["Width"] = maxW;
    oneDSLRInfo["Height"] = maxH;
    oneDSLRInfo["PixelW"] = pixelW;
    oneDSLRInfo["PixelH"] = pixelH;

    KStarsData::Instance()->userdb()->AddDSLRInfo(oneDSLRInfo);
    KStarsData::Instance()->userdb()->GetAllDSLRInfos(state()->DSLRInfos());

    updateFrameProperties();
    process()->resetFrame();
    process()->syncDSLRToTargetChip(model);

    // In case the dialog was opened, let's close it
    if (dslrInfoDialog)
        dslrInfoDialog.reset();
}

void Capture::cullToDSLRLimits()
{
    QString model(devices()->getActiveCamera()->getDeviceName());

    // Check if model already exists
    auto pos = std::find_if(state()->DSLRInfos().begin(),
                            state()->DSLRInfos().end(), [model](QMap<QString, QVariant> &oneDSLRInfo)
    {
        return (oneDSLRInfo["Model"] == model);
    });

    if (pos != state()->DSLRInfos().end())
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

void Capture::setPresetSettings(const QJsonObject &settings)
{
    auto opticalTrain = settings["optical_train"].toString(opticalTrainCombo->currentText());
    auto targetFilter = settings["filter"].toString(FilterPosCombo->currentText());

    opticalTrainCombo->setCurrentText(opticalTrain);
    FilterPosCombo->setCurrentText(targetFilter);

    captureExposureN->setValue(settings["exp"].toDouble(1));

    int bin = settings["bin"].toInt(1);
    setBinning(bin, bin);

    double temperature = settings["temperature"].toDouble(INVALID_VALUE);
    if (temperature > INVALID_VALUE && devices()->getActiveCamera()
            && devices()->getActiveCamera()->hasCoolerControl())
    {
        setForceTemperature(true);
        setTargetTemperature(temperature);
    }
    else
        setForceTemperature(false);

    double gain = settings["gain"].toDouble(GainSpinSpecialValue);
    if (devices()->getActiveCamera() && devices()->getActiveCamera()->hasGain())
    {
        if (gain == GainSpinSpecialValue)
            captureGainN->setValue(GainSpinSpecialValue);
        else
            setGain(gain);
    }

    double offset = settings["offset"].toDouble(OffsetSpinSpecialValue);
    if (devices()->getActiveCamera() && devices()->getActiveCamera()->hasOffset())
    {
        if (offset == OffsetSpinSpecialValue)
            captureOffsetN->setValue(OffsetSpinSpecialValue);
        else
            setOffset(offset);
    }

    int transferFormat = settings["transferFormat"].toInt(-1);
    if (transferFormat >= 0)
    {
        captureEncodingS->setCurrentIndex(transferFormat);
    }

    QString captureFormat = settings["captureFormat"].toString(captureFormatS->currentText());
    if (captureFormat != captureFormatS->currentText())
        captureFormatS->setCurrentText(captureFormat);

    captureTypeS->setCurrentIndex(qMax(0, settings["frameType"].toInt(0)));

    // ISO
    int isoIndex = settings["iso"].toInt(-1);
    if (isoIndex >= 0)
        setISO(isoIndex);

    bool dark = settings["dark"].toBool(darkB->isChecked());
    if (dark != darkB->isChecked())
        darkB->setChecked(dark);
}

void Capture::setFileSettings(const QJsonObject &settings)
{
    const auto prefix = settings["prefix"].toString(targetNameT->text());
    const auto directory = settings["directory"].toString(fileDirectoryT->text());
    const auto upload = settings["upload"].toInt(fileUploadModeS->currentIndex());
    const auto remote = settings["remote"].toString(fileRemoteDirT->text());
    const auto format = settings["format"].toString(placeholderFormatT->text());
    const auto suffix = settings["suffix"].toInt(formatSuffixN->value());

    targetNameT->setText(prefix);
    fileDirectoryT->setText(directory);
    fileUploadModeS->setCurrentIndex(upload);
    fileRemoteDirT->setText(remote);
    placeholderFormatT->setText(format);
    formatSuffixN->setValue(suffix);
}

QJsonObject Capture::getFileSettings()
{
    QJsonObject settings =
    {
        {"prefix", targetNameT->text()},
        {"directory", fileDirectoryT->text()},
        {"format", placeholderFormatT->text()},
        {"suffix", formatSuffixN->value()},
        {"upload", fileUploadModeS->currentIndex()},
        {"remote", fileRemoteDirT->text()}
    };

    return settings;
}

void Capture::setLimitSettings(const QJsonObject &settings)
{
    const bool deviationCheck = settings["deviationCheck"].toBool(Options::enforceGuideDeviation());
    const double deviationValue = settings["deviationValue"].toDouble(Options::guideDeviation());
    const bool focusHFRCheck = settings["focusHFRCheck"].toBool(m_LimitsUI->limitFocusHFRS->isChecked());
    const double focusHFRValue = settings["focusHFRValue"].toDouble(m_LimitsUI->limitFocusHFRN->value());
    const bool focusDeltaTCheck = settings["focusDeltaTCheck"].toBool(m_LimitsUI->limitFocusDeltaTS->isChecked());
    const double focusDeltaTValue = settings["focusDeltaTValue"].toDouble(m_LimitsUI->limitFocusDeltaTN->value());
    const bool refocusNCheck = settings["refocusNCheck"].toBool(m_LimitsUI->limitRefocusS->isChecked());
    const int refocusNValue = settings["refocusNValue"].toInt(m_LimitsUI->limitRefocusN->value());

    if (deviationCheck)
    {
        m_LimitsUI->limitGuideDeviationS->setChecked(true);
        m_LimitsUI->limitGuideDeviationN->setValue(deviationValue);
    }
    else
        m_LimitsUI->limitGuideDeviationS->setChecked(false);

    if (focusHFRCheck)
    {
        m_LimitsUI->limitFocusHFRS->setChecked(true);
        m_LimitsUI->limitFocusHFRN->setValue(focusHFRValue);
    }
    else
        m_LimitsUI->limitFocusHFRS->setChecked(false);

    if (focusDeltaTCheck)
    {
        m_LimitsUI->limitFocusDeltaTS->setChecked(true);
        m_LimitsUI->limitFocusDeltaTN->setValue(focusDeltaTValue);
    }
    else
        m_LimitsUI->limitFocusDeltaTS->setChecked(false);

    if (refocusNCheck)
    {
        m_LimitsUI->limitRefocusS->setChecked(true);
        m_LimitsUI->limitRefocusN->setValue(refocusNValue);
    }
    else
        m_LimitsUI->limitRefocusS->setChecked(false);

    syncRefocusOptionsFromGUI();
}

QJsonObject Capture::getLimitSettings()
{
    QJsonObject settings =
    {
        {"deviationCheck", Options::enforceGuideDeviation()},
        {"deviationValue", Options::guideDeviation()},
        {"focusHFRCheck", m_LimitsUI->limitFocusHFRS->isChecked()},
        {"focusHFRValue", m_LimitsUI->limitFocusHFRN->value()},
        {"focusDeltaTCheck", m_LimitsUI->limitFocusDeltaTS->isChecked()},
        {"focusDeltaTValue", m_LimitsUI->limitFocusDeltaTN->value()},
        {"refocusNCheck", m_LimitsUI->limitRefocusS->isChecked()},
        {"refocusNValue", m_LimitsUI->limitRefocusN->value()},
    };

    return settings;
}

void Capture::clearCameraConfiguration()
{
    connect(KSMessageBox::Instance(), &KSMessageBox::accepted, this, [this]()
    {
        //QObject::disconnect(KSMessageBox::Instance(), &KSMessageBox::accepted, this, nullptr);
        KSMessageBox::Instance()->disconnect(this);
        devices()->getActiveCamera()->setConfig(PURGE_CONFIG);
        KStarsData::Instance()->userdb()->DeleteDSLRInfo(devices()->getActiveCamera()->getDeviceName());

        QStringList shutterfulCCDs  = Options::shutterfulCCDs();
        QStringList shutterlessCCDs = Options::shutterlessCCDs();

        // Remove camera from shutterful and shutterless CCDs
        if (shutterfulCCDs.contains(devices()->getActiveCamera()->getDeviceName()))
        {
            shutterfulCCDs.removeOne(devices()->getActiveCamera()->getDeviceName());
            Options::setShutterfulCCDs(shutterfulCCDs);
        }
        if (shutterlessCCDs.contains(devices()->getActiveCamera()->getDeviceName()))
        {
            shutterlessCCDs.removeOne(devices()->getActiveCamera()->getDeviceName());
            Options::setShutterlessCCDs(shutterlessCCDs);
        }

        // For DSLRs, immediately ask them to enter the values again.
        if (captureISOS && captureISOS->count() > 0)
        {
            createDSLRDialog();
        }
    });

    KSMessageBox::Instance()->questionYesNo( i18n("Reset %1 configuration to default?",
            devices()->getActiveCamera()->getDeviceName()),
            i18n("Confirmation"), 30);
}

void Capture::updateJobTable(SequenceJob *job, bool full)
{
    if (job == nullptr)
    {
        QListIterator<SequenceJob *> iter(state()->allJobs());
        while (iter.hasNext())
            updateJobTable(iter.next(), full);
    }
    else
    {
        // find the job's row
        int row = state()->allJobs().indexOf(job);
        if (row >= 0 && row < queueTable->rowCount())
        {
            QTableWidgetItem *status = queueTable->item(row, JOBTABLE_COL_STATUS);
            QTableWidgetItem *count  = queueTable->item(row, JOBTABLE_COL_COUNTS);
            status->setText(job->getStatusString());
            updateJobTableCountCell(job, count);

            if (full)
            {
                bool isDarkFlat = job->jobType() == SequenceJob::JOBTYPE_DARKFLAT;

                QTableWidgetItem *filter = queueTable->item(row, JOBTABLE_COL_FILTER);
                if (FilterPosCombo->findText(job->getCoreProperty(SequenceJob::SJ_Filter).toString()) >= 0 &&
                        (captureTypeS->currentIndex() == FRAME_LIGHT || captureTypeS->currentIndex() == FRAME_FLAT || isDarkFlat) )
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

void Capture::updateJobTableCountCell(SequenceJob *job, QTableWidgetItem *countCell)
{
    countCell->setText(QString("%L1/%L2").arg(job->getCompleted()).arg(job->getCoreProperty(SequenceJob::SJ_Count).toInt()));
}

bool Capture::checkUploadPaths(FilenamePreviewType filenamePreview)
{
    // only relevant if we do not generate file name previews
    if (filenamePreview != NOT_PREVIEW)
        return true;

    if (fileUploadModeS->currentIndex() != ISD::Camera::UPLOAD_CLIENT && fileRemoteDirT->text().isEmpty())
    {
        KSNotification::error(i18n("You must set remote directory for Local & Both modes."));
        return false;
    }

    if (fileUploadModeS->currentIndex() != ISD::Camera::UPLOAD_LOCAL && fileDirectoryT->text().isEmpty())
    {
        KSNotification::error(i18n("You must set local directory for Client & Both modes."));
        return false;
    }
    // everything OK
    return true;
}

QJsonObject Capture::createJsonJob(SequenceJob *job, int currentRow)
{
    if (job == nullptr)
        return QJsonObject();

    QJsonObject jsonJob = {{"Status", "Idle"}};
    bool isDarkFlat = job->jobType() == SequenceJob::JOBTYPE_DARKFLAT;
    jsonJob.insert("Filter", FilterPosCombo->currentText());
    jsonJob.insert("Count", queueTable->item(currentRow, JOBTABLE_COL_COUNTS)->text());
    jsonJob.insert("Exp", queueTable->item(currentRow, JOBTABLE_COL_EXP)->text());
    jsonJob.insert("Type", isDarkFlat ? i18n("Dark Flat") : queueTable->item(currentRow, JOBTABLE_COL_TYPE)->text());
    jsonJob.insert("Bin", queueTable->item(currentRow, JOBTABLE_COL_BINNING)->text());
    jsonJob.insert("ISO/Gain", queueTable->item(currentRow, JOBTABLE_COL_ISO)->text());
    jsonJob.insert("Offset", queueTable->item(currentRow, JOBTABLE_COL_OFFSET)->text());

    return jsonJob;
}

void Capture::setCoolerToggled(bool enabled)
{
    auto isToggled = (!enabled && coolerOnB->isChecked()) || (enabled && coolerOffB->isChecked());

    coolerOnB->blockSignals(true);
    coolerOnB->setChecked(enabled);
    coolerOnB->blockSignals(false);

    coolerOffB->blockSignals(true);
    coolerOffB->setChecked(!enabled);
    coolerOffB->blockSignals(false);

    if (isToggled)
        appendLogText(enabled ? i18n("Cooler is on") : i18n("Cooler is off"));
}

void Capture::createDSLRDialog()
{
    dslrInfoDialog.reset(new DSLRInfo(this, devices()->getActiveCamera()));

    connect(dslrInfoDialog.get(), &DSLRInfo::infoChanged, this, [this]()
    {
        if (devices()->getActiveCamera())
            addDSLRInfo(QString(devices()->getActiveCamera()->getDeviceName()),
                        dslrInfoDialog->sensorMaxWidth,
                        dslrInfoDialog->sensorMaxHeight,
                        dslrInfoDialog->sensorPixelW,
                        dslrInfoDialog->sensorPixelH);
    });

    dslrInfoDialog->show();

    emit dslrInfoRequested(devices()->getActiveCamera()->getDeviceName());
}

void Capture::setGain(double value)
{
    if (!devices()->getActiveCamera())
        return;

    QMap<QString, QMap<QString, QVariant> > customProps = customPropertiesDialog->getCustomProperties();
    process()->updateGain(value, customProps);
    customPropertiesDialog->setCustomProperties(customProps);
}

void Capture::setOffset(double value)
{
    if (!devices()->getActiveCamera())
        return;

    QMap<QString, QMap<QString, QVariant> > customProps = customPropertiesDialog->getCustomProperties();

    process()->updateOffset(value, customProps);
    customPropertiesDialog->setCustomProperties(customProps);
}



void Capture::editFilterName()
{
    if (devices()->filterWheel() == nullptr || state()->getCurrentFilterPosition() < 1)
        return;

    QStringList labels = m_FilterManager->getFilterLabels();
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

    filterDialog.setWindowTitle(devices()->filterWheel()->getDeviceName());
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
        m_FilterManager->setFilterNames(newLabels);
    }
}

void Capture::handleScriptsManager()
{
    QScopedPointer<ScriptsManager> manager(new ScriptsManager(this));

    manager->setScripts(state()->scripts());

    if (manager->exec() == QDialog::Accepted)
    {
        state()->setScripts(manager->getScripts());
    }
}

void Capture::showTemperatureRegulation()
{
    if (!devices()->getActiveCamera())
        return;

    double currentRamp, currentThreshold;
    if (!devices()->getActiveCamera()->getTemperatureRegulation(currentRamp, currentThreshold))
        return;


    double rMin, rMax, rStep, tMin, tMax, tStep;

    devices()->getActiveCamera()->getMinMaxStep("CCD_TEMP_RAMP", "RAMP_SLOPE", &rMin, &rMax, &rStep);
    devices()->getActiveCamera()->getMinMaxStep("CCD_TEMP_RAMP", "RAMP_THRESHOLD", &tMin, &tMax, &tStep);

    QLabel rampLabel(i18nc("Maximum temperature variation over time when regulating.", "Ramp (°C/min):"));
    QDoubleSpinBox rampSpin;
    rampSpin.setMinimum(rMin);
    rampSpin.setMaximum(rMax);
    rampSpin.setSingleStep(rStep);
    rampSpin.setValue(currentRamp);
    rampSpin.setToolTip(i18n("<html><body>"
                             "<p>Maximum temperature change per minute when cooling or warming the camera. Set zero to disable."
                             "<p>This setting is read from and stored in the INDI camera driver configuration."
                             "</body></html>"));

    QLabel thresholdLabel(i18nc("Temperature threshold above which regulation triggers.", "Threshold (°C):"));
    QDoubleSpinBox thresholdSpin;
    thresholdSpin.setMinimum(tMin);
    thresholdSpin.setMaximum(tMax);
    thresholdSpin.setSingleStep(tStep);
    thresholdSpin.setValue(currentThreshold);
    thresholdSpin.setToolTip(i18n("<html><body>"
                                  "<p>Maximum difference between camera and target temperatures triggering regulation."
                                  "<p>This setting is read from and stored in the INDI camera driver configuration."
                                  "</body></html>"));

    QFormLayout layout;
    layout.addRow(&rampLabel, &rampSpin);
    layout.addRow(&thresholdLabel, &thresholdSpin);

    QPointer<QDialog> dialog = new QDialog(this);
    QDialogButtonBox buttonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dialog);
    connect(&buttonBox, &QDialogButtonBox::accepted, dialog, &QDialog::accept);
    connect(&buttonBox, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
    dialog->setWindowTitle(i18nc("@title:window", "Set Temperature Regulation"));
    layout.addWidget(&buttonBox);
    dialog->setLayout(&layout);
    dialog->setMinimumWidth(300);

    if (dialog->exec() == QDialog::Accepted)
    {
        if (devices()->getActiveCamera())
            devices()->getActiveCamera()->setTemperatureRegulation(rampSpin.value(), thresholdSpin.value());
    }
}

void Capture::updateStartButtons(bool start, bool pause)
{
    if (start)
    {
        // start capturing, therefore next possible action is stopping
        startB->setIcon(QIcon::fromTheme("media-playback-stop"));
        startB->setToolTip(i18n("Stop Sequence"));
    }
    else
    {
        // stop capturing, therefore next possible action is starting
        startB->setIcon(QIcon::fromTheme("media-playback-start"));
        startB->setToolTip(i18n(pause ? "Resume Sequence" : "Start Sequence"));
    }
    pauseB->setEnabled(start && !pause);

}

void Capture::generateDarkFlats()
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

void Capture::updateJobFromUI(SequenceJob *job, FilenamePreviewType filenamePreview)
{
    job->setCoreProperty(SequenceJob::SJ_Format, captureFormatS->currentText());
    job->setCoreProperty(SequenceJob::SJ_Encoding, captureEncodingS->currentText());

    if (captureISOS)
        job->setISO(captureISOS->currentIndex());

    if (getGain() >= 0)
        job->setCoreProperty(SequenceJob::SJ_Gain, getGain());

    if (getOffset() >= 0)
        job->setCoreProperty(SequenceJob::SJ_Offset, getOffset());

    if (cameraTemperatureN->isEnabled())
    {
        job->setCoreProperty(SequenceJob::SJ_EnforceTemperature, cameraTemperatureS->isChecked());
        job->setTargetTemperature(cameraTemperatureN->value());
    }

    job->setUploadMode(static_cast<ISD::Camera::UploadMode>(fileUploadModeS->currentIndex()));
    job->setScripts(state()->scripts());
    job->setFlatFieldDuration(state()->flatFieldDuration());
    job->setCalibrationPreAction(state()->calibrationPreAction());
    job->setWallCoord(state()->wallCoord());
    job->setCoreProperty(SequenceJob::SJ_TargetADU, state()->targetADU());
    job->setCoreProperty(SequenceJob::SJ_TargetADUTolerance, state()->targetADUTolerance());
    job->setFrameType(static_cast<CCDFrameType>(qMax(0, captureTypeS->currentIndex())));

    job->setCoreProperty(SequenceJob::SJ_EnforceStartGuiderDrift, (job->getFrameType() == FRAME_LIGHT
                         && Options::enforceStartGuiderDrift()));
    job->setTargetStartGuiderDrift(Options::startGuideDeviation());

    if (FilterPosCombo->currentIndex() != -1 && devices()->filterWheel() != nullptr)
        job->setTargetFilter(FilterPosCombo->currentIndex() + 1, FilterPosCombo->currentText());

    job->setCoreProperty(SequenceJob::SJ_Exposure, captureExposureN->value());

    job->setCoreProperty(SequenceJob::SJ_Count, captureCountN->value());

    job->setCoreProperty(SequenceJob::SJ_Binning, QPoint(captureBinHN->value(), captureBinVN->value()));

    /* in ms */
    job->setCoreProperty(SequenceJob::SJ_Delay, captureDelayN->value() * 1000);

    // Custom Properties
    job->setCustomProperties(customPropertiesDialog->getCustomProperties());

    job->setCoreProperty(SequenceJob::SJ_ROI, QRect(captureFrameXN->value(), captureFrameYN->value(), captureFrameWN->value(),
                         captureFrameHN->value()));
    job->setCoreProperty(SequenceJob::SJ_RemoteDirectory, fileRemoteDirT->text());
    job->setCoreProperty(SequenceJob::SJ_LocalDirectory, fileDirectoryT->text());
    job->setCoreProperty(SequenceJob::SJ_TargetName, targetNameT->text());
    job->setCoreProperty(SequenceJob::SJ_PlaceholderFormat, placeholderFormatT->text());
    job->setCoreProperty(SequenceJob::SJ_PlaceholderSuffix, formatSuffixN->value());

    auto placeholderPath = PlaceholderPath();
    placeholderPath.addJob(job, placeholderFormatT->text());

    QString signature = placeholderPath.generateSequenceFilename(*job,
                        filenamePreview != REMOTE_PREVIEW, true, 1,
                        ".fits", "", false, true);
    job->setCoreProperty(SequenceJob::SJ_Signature, signature);

    auto remoteUpload = placeholderPath.generateSequenceFilename(*job,
                        false,
                        true,
                        1,
                        ".fits",
                        "",
                        false,
                        true);

    auto lastSeparator = remoteUpload.lastIndexOf(QDir::separator());
    auto remoteDirectory = remoteUpload.mid(0, lastSeparator);
    auto remoteFilename = QString("%1_XXX").arg(remoteUpload.mid(lastSeparator + 1));
    job->setCoreProperty(SequenceJob::SJ_RemoteFormatDirectory, remoteDirectory);
    job->setCoreProperty(SequenceJob::SJ_RemoteFormatFilename, remoteFilename);
}

void Capture::setMeridianFlipState(QSharedPointer<MeridianFlipState> newstate)
{
    state()->setMeridianFlipState(newstate);
    connect(state()->getMeridianFlipState().get(), &MeridianFlipState::newLog, this, &Capture::appendLogText);
}

void Capture::syncRefocusOptionsFromGUI()
{
    Options::setEnforceAutofocusHFR(m_LimitsUI->limitFocusHFRS->isChecked());
    Options::setHFRDeviation(m_LimitsUI->limitFocusHFRN->value());
    Options::setEnforceAutofocusOnTemperature(m_LimitsUI->limitFocusDeltaTS->isChecked());
    Options::setMaxFocusTemperatureDelta(m_LimitsUI->limitFocusDeltaTN->value());
    Options::setEnforceRefocusEveryN(m_LimitsUI->limitRefocusS->isChecked());
    Options::setRefocusEveryN(static_cast<uint>(m_LimitsUI->limitRefocusN->value()));
    Options::setRefocusAfterMeridianFlip(m_LimitsUI->meridianRefocusS->isChecked());
}

QJsonObject Capture::currentScope()
{
    QVariant trainID = ProfileSettings::Instance()->getOneSetting(ProfileSettings::CaptureOpticalTrain);
    if (activeCamera() && trainID.isValid())
    {
        auto id = trainID.toUInt();
        auto name = OpticalTrainManager::Instance()->name(id);
        return OpticalTrainManager::Instance()->getScope(name);
    }
    // return empty JSON object
    return QJsonObject();
}

double Capture::currentReducer()
{
    QVariant trainID = ProfileSettings::Instance()->getOneSetting(ProfileSettings::CaptureOpticalTrain);
    if (activeCamera() && trainID.isValid())
    {
        auto id = trainID.toUInt();
        auto name = OpticalTrainManager::Instance()->name(id);
        return OpticalTrainManager::Instance()->getReducer(name);
    }
    // no reducer available
    return 1.0;
}

double Capture::currentAperture()
{
    auto scope = currentScope();

    double focalLength = scope["focal_length"].toDouble(-1);
    double aperture = scope["aperture"].toDouble(-1);
    double focalRatio = scope["focal_ratio"].toDouble(-1);

    // DSLR Lens Aperture
    if (aperture < 0 && focalRatio > 0)
        aperture = focalLength * focalRatio;

    return aperture;
}

void Capture::setupOpticalTrainManager()
{
    connect(OpticalTrainManager::Instance(), &OpticalTrainManager::updated, this, &Capture::refreshOpticalTrain);
    connect(trainB, &QPushButton::clicked, this, [this]()
    {
        OpticalTrainManager::Instance()->openEditor(opticalTrainCombo->currentText());
    });
    connect(opticalTrainCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index)
    {
        ProfileSettings::Instance()->setOneSetting(ProfileSettings::CaptureOpticalTrain,
                OpticalTrainManager::Instance()->id(opticalTrainCombo->itemText(index)));
        refreshOpticalTrain();
        emit trainChanged();
    });
}

void Capture::refreshOpticalTrain()
{
    opticalTrainCombo->blockSignals(true);
    opticalTrainCombo->clear();
    opticalTrainCombo->addItems(OpticalTrainManager::Instance()->getTrainNames());
    trainB->setEnabled(true);

    QVariant trainID = ProfileSettings::Instance()->getOneSetting(ProfileSettings::CaptureOpticalTrain);

    if (trainID.isValid())
    {
        auto id = trainID.toUInt();

        // If train not found, select the first one available.
        if (OpticalTrainManager::Instance()->exists(id) == false)
        {
            qCWarning(KSTARS_EKOS_CAPTURE) << "Optical train doesn't exist for id" << id;
            id = OpticalTrainManager::Instance()->id(opticalTrainCombo->itemText(0));
        }

        auto name = OpticalTrainManager::Instance()->name(id);

        opticalTrainCombo->setCurrentText(name);
        process()->refreshOpticalTrain(name);
    }

    opticalTrainCombo->blockSignals(false);
}

void Capture::generatePreviewFilename()
{
    if (state()->isCaptureRunning() == false)
    {
        placeholderFormatT->setToolTip(previewFilename( LOCAL_PREVIEW ));
        emit newLocalPreview(placeholderFormatT->toolTip());

        if (fileUploadModeS->currentIndex() != 0)
            fileRemoteDirT->setToolTip(previewFilename( REMOTE_PREVIEW ));
    }
}

QString Capture::previewFilename(FilenamePreviewType previewType)
{
    QString previewText;
    QString m_format;
    auto separator = QDir::separator();

    if (previewType == LOCAL_PREVIEW)
    {
        if(!fileDirectoryT->text().endsWith(separator) && !placeholderFormatT->text().startsWith(separator))
            placeholderFormatT->setText(separator + placeholderFormatT->text());
        m_format = fileDirectoryT->text() + placeholderFormatT->text() + formatSuffixN->prefix() + formatSuffixN->cleanText();
    }
    else if (previewType == REMOTE_PREVIEW)
        m_format = fileRemoteDirT->text();

    //Guard against an empty format to avoid the empty directory warning pop-up in addjob
    if (m_format.isEmpty())
        return previewText;
    // Tags %d & %p disable for now for simplicity
    //    else if (state()->sequenceURL().toLocalFile().isEmpty() && (m_format.contains("%d") || m_format.contains("%p")
    //             || m_format.contains("%f")))
    else if (state()->sequenceURL().toLocalFile().isEmpty() && m_format.contains("%f"))
        previewText = ("Save the sequence file to show filename preview");
    else
    {
        // create temporarily a sequence job
        SequenceJob *m_job = createJob(SequenceJob::JOBTYPE_PREVIEW, previewType);
        if (m_job == nullptr)
            return previewText;

        QString previewSeq;
        if (state()->sequenceURL().toLocalFile().isEmpty())
        {
            if (m_format.startsWith(separator))
                previewSeq = m_format.left(m_format.lastIndexOf(separator));
        }
        else
            previewSeq = state()->sequenceURL().toLocalFile();
        auto m_placeholderPath = PlaceholderPath(previewSeq);

        QString extension;
        if (captureEncodingS->currentText() == "FITS")
            extension = ".fits";
        else if (captureEncodingS->currentText() == "XISF")
            extension = ".xisf";
        else
            extension = ".[NATIVE]";
        previewText = m_placeholderPath.generateSequenceFilename(*m_job, previewType == LOCAL_PREVIEW, true, 1,
                      extension, "", false);
        previewText = QDir::toNativeSeparators(previewText);
        // we do not use it any more
        m_job->deleteLater();
    }

    // Must change directory separate to UNIX style for remote
    if (previewType == REMOTE_PREVIEW)
        previewText.replace(separator, "/");

    return previewText;
}

void Capture::openExposureCalculatorDialog()
{
    qCInfo(KSTARS_EKOS_CAPTURE) << "Instantiating an Exposure Calculator";

    // Learn how to read these from indi
    double preferredSkyQuality = 20.5;

    auto scope = currentScope();
    double focalRatio = scope["focal_ratio"].toDouble(-1);

    auto reducedFocalLength = currentReducer() * scope["focal_length"].toDouble(-1);
    auto aperture = currentAperture();
    auto reducedFocalRatio = (focalRatio > 0 || aperture == 0) ? focalRatio : reducedFocalLength / aperture;

    if (devices()->getActiveCamera() != nullptr)
    {
        qCInfo(KSTARS_EKOS_CAPTURE) << "set ExposureCalculator preferred camera to active camera id: "
                                    << devices()->getActiveCamera()->getDeviceName();
    }

    QPointer<ExposureCalculatorDialog> anExposureCalculatorDialog(new ExposureCalculatorDialog(KStars::Instance(),
            preferredSkyQuality,
            reducedFocalRatio,
            devices()->getActiveCamera()->getDeviceName()));
    anExposureCalculatorDialog->setAttribute(Qt::WA_DeleteOnClose);
    anExposureCalculatorDialog->show();
}

bool Capture::hasCoolerControl()
{
    return process()->hasCoolerControl();
}

bool Capture::setCoolerControl(bool enable)
{
    return process()->setCoolerControl(enable);
}

void Capture::removeDevice(const QSharedPointer<ISD::GenericDevice> &device)
{
    process()->removeDevice(device);
}

void Capture::start()
{
    process()->startNextPendingJob();
}

void Capture::stop(CaptureState targetState)
{
    process()->stopCapturing(targetState);
}

void Capture::toggleVideo(bool enabled)
{
    process()->toggleVideo(enabled);
}

void Capture::setTargetName(const QString &newTargetName)
{
    if (activeJob() != nullptr)
    {
        activeJob()->setCoreProperty(SequenceJob::SJ_TargetName, newTargetName);
        targetNameT->setText(newTargetName);
    }
}

QString Capture::getTargetName()
{
    if (activeJob())
        return activeJob()->getCoreProperty(SequenceJob::SJ_TargetName).toString();
    else
        return "";
}

void Capture::restartCamera(const QString &name)
{
    process()->restartCamera(name);
}

void Capture::capturePreview()
{
    process()->capturePreview();
}

void Capture::startFraming()
{
    process()->capturePreview(true);
}

double Capture::getGain()
{
    return devices()->cameraGain(customPropertiesDialog->getCustomProperties());
}

double Capture::getOffset()
{
    return devices()->cameraOffset(customPropertiesDialog->getCustomProperties());
}

void Capture::setHFR(double newHFR, int)
{
    state()->getRefocusState()->setFocusHFR(newHFR);
}

ISD::Camera *Capture::activeCamera()
{
    return m_captureDeviceAdaptor->getActiveCamera();
}
}
