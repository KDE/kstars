/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "capture.h"

#include "captureadaptor.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "Options.h"
//#include "ekos/capture/rotatorsettings.h"
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
#include "indi/indilistener.h"
#include "oal/observeradd.h"
#include "ekos/guide/guide.h"
#include "exposurecalculator/exposurecalculatordialog.h"
#include <basedevice.h>

#include <ekos_capture_debug.h>

#define MF_TIMER_TIMEOUT    90000
#define GD_TIMER_TIMEOUT    60000
#define MF_RA_DIFF_LIMIT    4

// Current Sequence File Format:
#define SQ_FORMAT_VERSION 2.5
// We accept file formats with version back to:
#define SQ_COMPAT_VERSION 2.0

// Qt version calming
#include <qtendl.h>

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
    m_captureProcess = new CaptureProcess(m_captureModuleState, m_captureDeviceAdaptor);

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

    connect(&m_captureModuleState->getSeqDelayTimer(), &QTimer::timeout, m_captureProcess, &CaptureProcess::captureImage);
    m_captureModuleState->getCaptureDelayTimer().setSingleShot(true);
    connect(&m_captureModuleState->getCaptureDelayTimer(), &QTimer::timeout, this, &Capture::start, Qt::UniqueConnection);

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

    m_captureModuleState->getGuideDeviationTimer().setInterval(GD_TIMER_TIMEOUT);
    connect(&m_captureModuleState->getGuideDeviationTimer(), &QTimer::timeout, this, &Capture::checkGuideDeviationTimeout);

    connect(clearConfigurationB, &QPushButton::clicked, this, &Capture::clearCameraConfiguration);

    darkB->setChecked(Options::autoDark());
    connect(darkB, &QAbstractButton::toggled, this, [this]()
    {
        Options::setAutoDark(darkB->isChecked());
    });


    connect(restartCameraB, &QPushButton::clicked, this, [this]()
    {
        if (m_Camera)
            restartCamera(m_Camera->getDeviceName());
    });

    connect(cameraTemperatureS, &QCheckBox::toggled, this, [this](bool toggled)
    {
        if (m_captureDeviceAdaptor->getActiveCamera())
        {
            QVariantMap auxInfo = m_captureDeviceAdaptor->getActiveCamera()->getDriverInfo()->getAuxInfo();
            auxInfo[QString("%1_TC").arg(m_captureDeviceAdaptor->getActiveCamera()->getDeviceName())] = toggled;
            m_captureDeviceAdaptor->getActiveCamera()->getDriverInfo()->setAuxInfo(auxInfo);
        }
    });

    connect(filterEditB, &QPushButton::clicked, this, &Capture::editFilterName);

    connect(FilterPosCombo, static_cast<void(QComboBox::*)(const QString &)>(&QComboBox::currentTextChanged),
            [ = ]()
    {
        m_captureModuleState->updateHFRThreshold();
        generatePreviewFilename();
    });
    connect(previewB, &QPushButton::clicked, this, &Capture::captureOne);
    connect(loopB, &QPushButton::clicked, this, &Capture::startFraming);

    //connect( seqWatcher, SIGNAL(dirty(QString)), this, &Capture::checkSeqFile(QString)));

    connect(addToQueueB, &QPushButton::clicked, this, &Capture::addSequenceJob);
    connect(removeFromQueueB, &QPushButton::clicked, this, &Capture::removeJobFromQueue);
    connect(queueUpB, &QPushButton::clicked, this, &Capture::moveJobUp);
    connect(queueDownB, &QPushButton::clicked, this, &Capture::moveJobDown);
    connect(selectFileDirectoryB, &QPushButton::clicked, this, &Capture::saveFITSDirectory);
    connect(queueSaveB, &QPushButton::clicked, this, static_cast<void(Capture::*)()>(&Capture::saveSequenceQueue));
    connect(queueSaveAsB, &QPushButton::clicked, this, &Capture::saveSequenceQueueAs);
    connect(queueLoadB, &QPushButton::clicked, this, static_cast<void(Capture::*)()>(&Capture::loadSequenceQueue));
    connect(resetB, &QPushButton::clicked, this, &Capture::resetJobs);
    connect(queueTable->selectionModel(), &QItemSelectionModel::currentRowChanged, this, &Capture::selectedJobChanged);
    connect(queueTable, &QAbstractItemView::doubleClicked, this, &Capture::editJob);
    connect(queueTable, &QTableWidget::itemSelectionChanged, this, &Capture::resetJobEdit);
    connect(setTemperatureB, &QPushButton::clicked, this, [&]()
    {
        if (m_captureDeviceAdaptor->getActiveCamera())
            m_captureDeviceAdaptor->getActiveCamera()->setTemperature(cameraTemperatureN->value());
    });
    connect(coolerOnB, &QPushButton::clicked, this, [&]()
    {
        if (m_captureDeviceAdaptor->getActiveCamera())
            m_captureDeviceAdaptor->getActiveCamera()->setCoolerControl(true);
    });
    connect(coolerOffB, &QPushButton::clicked, this, [&]()
    {
        if (m_captureDeviceAdaptor->getActiveCamera())
            m_captureDeviceAdaptor->getActiveCamera()->setCoolerControl(false);
    });
    connect(cameraTemperatureN, &QDoubleSpinBox::editingFinished, setTemperatureB,
            static_cast<void (QPushButton::*)()>(&QPushButton::setFocus));
    connect(captureTypeS, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
            &Capture::checkFrameType);
    connect(resetFrameB, &QPushButton::clicked, this, &Capture::resetFrame);
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
    // Start Guide Deviation Check
    m_LimitsUI->startGuiderDriftS->setChecked(Options::enforceStartGuiderDrift());
    connect(m_LimitsUI->startGuiderDriftS, &QCheckBox::toggled, [ = ](bool checked)
    {
        Options::setEnforceStartGuiderDrift(checked);
    });

    // Start Guide Deviation Value
    m_LimitsUI->startGuiderDriftN->setValue(Options::startGuideDeviation());
    connect(m_LimitsUI->startGuiderDriftN, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this]()
    {
        Options::setStartGuideDeviation(m_LimitsUI->startGuiderDriftN->value());
    });

    // Abort Guide Deviation Check
    m_LimitsUI->limitGuideDeviationS->setChecked(Options::enforceGuideDeviation());
    connect(m_LimitsUI->limitGuideDeviationS, &QCheckBox::toggled, [ = ](bool checked)
    {
        Options::setEnforceGuideDeviation(checked);
    });

    // Guide Deviation Value
    m_LimitsUI->limitGuideDeviationN->setValue(Options::guideDeviation());
    connect(m_LimitsUI->limitGuideDeviationN, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this]()
    {
        Options::setGuideDeviation(m_LimitsUI->limitGuideDeviationN->value());
    });

    // Autofocus HFR Check
    m_LimitsUI->limitFocusHFRS->setChecked(Options::enforceAutofocusHFR());
    connect(m_LimitsUI->limitFocusHFRS, &QCheckBox::toggled, [ = ](bool checked)
    {
        Options::setEnforceAutofocusHFR(checked);
        if (checked == false)
            m_captureModuleState->getRefocusState()->setInSequenceFocus(false);
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
    m_LimitsUI->limitFocusDeltaTS->setChecked(Options::enforceAutofocusOnTemperature());
    connect(m_LimitsUI->limitFocusDeltaTS, &QCheckBox::toggled, this,  [ = ](bool checked)
    {
        Options::setEnforceAutofocusOnTemperature(checked);
    });

    // Autofocus temperature Delta
    m_LimitsUI->limitFocusDeltaTN->setValue(Options::maxFocusTemperatureDelta());
    connect(m_LimitsUI->limitFocusDeltaTN, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this]()
    {
        Options::setMaxFocusTemperatureDelta(m_LimitsUI->limitFocusDeltaTN->value());
    });

    // Refocus Every Check
    m_LimitsUI->limitRefocusS->setChecked(Options::enforceRefocusEveryN());
    connect(m_LimitsUI->limitRefocusS, &QCheckBox::toggled, this, [](bool checked)
    {
        Options::setEnforceRefocusEveryN(checked);
    });

    // Refocus Every Value
    m_LimitsUI->limitRefocusN->setValue(static_cast<int>(Options::refocusEveryN()));
    connect(m_LimitsUI->limitRefocusN, QOverload<int>::of(&QSpinBox::valueChanged), this, [this]()
    {
        Options::setRefocusEveryN(static_cast<uint>(m_LimitsUI->limitRefocusN->value()));
    });

    // File settings: filter name
    FilterEnabled = Options::fileSettingsUseFilter();

    // File settings: duration
    ExpEnabled = Options::fileSettingsUseDuration();

    // File settings: timestamp
    TimeStampEnabled = Options::fileSettingsUseTimestamp();

    // Refocus after meridian flip
    m_LimitsUI->meridianRefocusS->setChecked(Options::refocusAfterMeridianFlip());
    connect(m_LimitsUI->meridianRefocusS, &QCheckBox::toggled, [](bool checked)
    {
        Options::setRefocusAfterMeridianFlip(checked);
    });

    QCheckBox * const checkBoxes[] =
    {
        m_LimitsUI->limitGuideDeviationS,
        m_LimitsUI->limitRefocusS,
        m_LimitsUI->limitGuideDeviationS,
    };
    for (const QCheckBox * control : checkBoxes)
        connect(control, &QCheckBox::toggled, this, [&]()
    {
        m_captureModuleState->setDirty(true);
    });

    QDoubleSpinBox * const dspinBoxes[]
    {
        m_LimitsUI->limitFocusHFRN,
        m_LimitsUI->limitFocusDeltaTN,
        m_LimitsUI->limitGuideDeviationN,
    };
    for (const QDoubleSpinBox * control : dspinBoxes)
        connect(control, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), this, [&]()
    {
        m_captureModuleState->setDirty(true);
    });

    connect(fileUploadModeS, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, [&]()
    {
        m_captureModuleState->setDirty(true);
    });
    connect(fileRemoteDirT, &QLineEdit::editingFinished, this, [&]()
    {
        m_captureModuleState->setDirty(true);
    });

    observerB->setIcon(QIcon::fromTheme("im-user"));
    observerB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    connect(observerB, &QPushButton::clicked, this, &Capture::showObserverDialog);

    // Exposure Timeout
    m_captureModuleState->getCaptureTimeout().setSingleShot(true);
    connect(&m_captureModuleState->getCaptureTimeout(), &QTimer::timeout, m_captureProcess,
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

    flatFieldSource = static_cast<FlatFieldSource>(Options::calibrationFlatSourceIndex());
    flatFieldDuration = static_cast<FlatFieldDuration>(Options::calibrationFlatDurationIndex());
    wallCoord.setAz(Options::calibrationWallAz());
    wallCoord.setAlt(Options::calibrationWallAlt());
    targetADU = Options::calibrationADUValue();

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

    //This Timer will update the Exposure time in the capture module to display the estimated download time left
    //It will also update the Exposure time left in the Summary Screen.
    //It fires every 100 ms while images are downloading.
    m_captureModuleState->downloadProgressTimer().setInterval(100);
    connect(&m_captureModuleState->downloadProgressTimer(), &QTimer::timeout, this, &Capture::setDownloadProgress);

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
    connect(m_captureModuleState.data(), &CaptureModuleState::captureStarted, this, &Capture::captureStarted);
    connect(m_captureModuleState.data(), &CaptureModuleState::newLog, this, &Capture::appendLogText);
    connect(m_captureModuleState.data(), &CaptureModuleState::newStatus, this, &Capture::newStatus);
    connect(m_captureModuleState.data(), &CaptureModuleState::checkFocus, this, &Capture::checkFocus);
    connect(m_captureModuleState.data(), &CaptureModuleState::resetFocus, this, &Capture::resetFocus);
    connect(m_captureModuleState.data(), &CaptureModuleState::adaptiveFocus, this, &Capture::adaptiveFocus);
    connect(m_captureModuleState.data(), &CaptureModuleState::guideAfterMeridianFlip, this,
            &Capture::guideAfterMeridianFlip);
    connect(m_captureModuleState.data(), &CaptureModuleState::newFocusStatus, this, &Capture::updateFocusStatus);
    connect(m_captureModuleState.data(), &CaptureModuleState::newMeridianFlipStage, this, &Capture::updateMeridianFlipStage);
    connect(m_captureModuleState.data(), &CaptureModuleState::meridianFlipStarted, this, &Capture::meridianFlipStarted);
    // forward signals from capture process
    connect(m_captureProcess.data(), &CaptureProcess::cameraReady, this, &Capture::ready);
    connect(m_captureProcess.data(), &CaptureProcess::newExposureValue, this, &Capture::setExposureProgress);
    connect(m_captureProcess.data(), &CaptureProcess::processingFITSfinished, this, &Capture::processingFITSfinished);
    connect(m_captureProcess.data(), &CaptureProcess::newImage, this, &Capture::newImage);
    connect(m_captureProcess.data(), &CaptureProcess::syncGUIToJob, this, &Capture::syncGUIToJob);
    connect(m_captureProcess.data(), &CaptureProcess::captureComplete, this, &Capture::captureComplete);
    connect(m_captureProcess.data(), &CaptureProcess::jobExecutionPreparationStarted, this,
            &Capture::jobExecutionPreparationStarted);
    connect(m_captureProcess.data(), &CaptureProcess::sequenceChanged, this, &Capture::sequenceChanged);
    connect(m_captureProcess.data(), &CaptureProcess::jobPrepared, this, &Capture::jobPrepared);
    connect(m_captureProcess.data(), &CaptureProcess::captureImageStarted, this, &Capture::captureImageStarted);
    connect(m_captureProcess.data(), &CaptureProcess::updateMeridianFlipStage, this, &Capture::updateMeridianFlipStage);
    connect(m_captureProcess.data(), &CaptureProcess::darkFrameCompleted, this, &Capture::imageCapturingCompleted);
    connect(m_captureProcess.data(), &CaptureProcess::newLog, this, &Capture::appendLogText);
    connect(m_captureProcess.data(), &CaptureProcess::stopCapture, this, &Capture::stop);
    connect(m_captureProcess.data(), &CaptureProcess::suspendGuiding, this, &Capture::suspendGuiding);
    connect(m_captureProcess.data(), &CaptureProcess::resumeGuiding, this, &Capture::resumeGuiding);
    connect(m_captureProcess.data(), &CaptureProcess::driverTimedout, this, &Capture::driverTimedout);
    connect(m_captureProcess.data(), &CaptureProcess::reconnectDriver, this, &Capture::reconnectDriver);
    // connections between state machine and device adaptor
    connect(m_captureModuleState.data(), &CaptureModuleState::newFilterPosition,
            m_captureDeviceAdaptor.data(), &CaptureDeviceAdaptor::setFilterPosition);
    connect(m_captureModuleState.data(), &CaptureModuleState::abortFastExposure,
            m_captureDeviceAdaptor.data(), &CaptureDeviceAdaptor::abortFastExposure);

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
        m_captureModuleState->setTargetName(targetNameT->text());
        generatePreviewFilename();
        qCDebug(KSTARS_EKOS_CAPTURE) << "Changed target to" << targetNameT->text() << "because of user edit";
    });
    connect(captureTypeS, &QComboBox::currentTextChanged, this, &Capture::generatePreviewFilename);

    connect(exposureCalcB, &QPushButton::clicked, this, &Capture::openExposureCalculatorDialog);

}

Capture::~Capture()
{
    qDeleteAll(m_captureModuleState->allJobs());
    m_captureModuleState->allJobs().clear();
}

bool Capture::setCamera(ISD::Camera *device)
{
    if (m_Camera && m_Camera == device)
    {
        checkCamera();
        return false;
    }

    if (m_Camera)
        m_Camera->disconnect(this);

    m_Camera = device;

    if (m_Camera)
    {
        connect(m_Camera, &ISD::ConcreteDevice::Connected, this, [this]()
        {
            CCDFWGroup->setEnabled(true);
            sequenceBox->setEnabled(true);
            for (auto &oneChild : sequenceControlsButtonGroup->buttons())
                oneChild->setEnabled(true);
        });
        connect(m_Camera, &ISD::ConcreteDevice::Disconnected, this, [this]()
        {
            CCDFWGroup->setEnabled(false);
            sequenceBox->setEnabled(false);
            for (auto &oneChild : sequenceControlsButtonGroup->buttons())
                oneChild->setEnabled(false);

            opticalTrainCombo->setEnabled(true);
            trainLabel->setEnabled(true);
        });
    }

    auto isConnected = m_Camera && m_Camera->isConnected();
    CCDFWGroup->setEnabled(isConnected);
    sequenceBox->setEnabled(isConnected);
    for (auto &oneChild : sequenceControlsButtonGroup->buttons())
        oneChild->setEnabled(isConnected);

    if (!m_Camera)
    {
        cameraLabel->clear();
        return false;
    }
    else
        cameraLabel->setText(m_Camera->getDeviceName());

    if (m_FilterWheel)
        syncFilterInfo();

    checkCamera();

    emit settingsUpdated(getPresetSettings());

    if (device->hasGuideHead())
        addGuideHead(device);

    return true;
}

void Capture::addGuideHead(ISD::Camera * device)
{
    Q_UNUSED(device)
    //QString guiderName = device->getDeviceName() + QString(" Guider");

    // FIXME add support for guide head
    //    if (cameraS->findText(guiderName) == -1)
    //    {
    //        cameraS->addItem(guiderName);
    //        m_Cameras.append(device);
    //    }
}

bool Capture::setFilterWheel(ISD::FilterWheel * device)
{
    if (m_FilterWheel && m_FilterWheel == device)
    {
        checkFilter();
        return false;
    }

    if (m_FilterWheel)
        m_FilterWheel->disconnect(this);

    m_FilterWheel = device;
    m_captureDeviceAdaptor->setFilterWheel(m_FilterWheel);

    if (m_FilterWheel)
    {
        connect(m_FilterWheel, &ISD::ConcreteDevice::Connected, this, [this]()
        {
            FilterPosLabel->setEnabled(true);
            FilterPosCombo->setEnabled(true);
            filterManagerB->setEnabled(true);
        });
        connect(m_FilterWheel, &ISD::ConcreteDevice::Disconnected, this, [this]()
        {
            FilterPosLabel->setEnabled(false);
            FilterPosCombo->setEnabled(false);
            filterManagerB->setEnabled(false);
        });
    }

    auto isConnected = m_FilterWheel && m_FilterWheel->isConnected();
    FilterPosLabel->setEnabled(isConnected);
    FilterPosCombo->setEnabled(isConnected);
    filterManagerB->setEnabled(isConnected);

    checkFilter();

    if (m_FilterWheel)
        emit settingsUpdated(getPresetSettings());

    return true;
}

bool Capture::setDome(ISD::Dome *device)
{
    if (m_Dome && m_Dome == device)
        return false;

    if (m_Dome)
        m_Dome->disconnect(this);

    m_Dome = device;

    // forward it to the command processor
    m_captureDeviceAdaptor->setDome(m_Dome);

    return true;
}

bool Capture::setDustCap(ISD::DustCap *device)
{
    if (m_DustCap && m_DustCap == device)
        return false;

    if (m_DustCap)
        m_DustCap->disconnect(this);

    m_DustCap = device;

    // forward it to the command processor
    m_captureDeviceAdaptor->setDustCap(m_DustCap);
    m_captureModuleState->setDustCapState(CaptureModuleState::CAP_UNKNOWN);

    syncFilterInfo();
    return true;
}

bool Capture::setMount(ISD::Mount *device)
{
    if (m_Mount && m_Mount == device)
    {
        syncTelescopeInfo();
        return false;
    }

    if (m_Mount)
        m_Mount->disconnect(this);

    m_Mount = device;

    // forward it to the command processor
    m_captureDeviceAdaptor->setMount(m_Mount);

    if (!m_Mount)
        return false;

    m_captureDeviceAdaptor->getMount()->disconnect(this);
    connect(m_captureDeviceAdaptor->getMount(), &ISD::Mount::newTargetName, this, &Capture::setTargetName);

    syncTelescopeInfo();
    return true;
}

void Capture::setRotator(ISD::Rotator * device)
{
    // rotator initializing depends on present mount process
    if (m_Mount)
    {
        if (!m_Rotator || !(m_Rotator == device))
        {
            rotatorB->setEnabled(false);
            if (m_Rotator)
            {
                m_Rotator->disconnect(this);
                m_RotatorControlPanel->close();
            }
            m_Rotator = device;
            if (m_Rotator)
            {
                Manager::Instance()->createRotatorController(m_Rotator->getDeviceName());
                Manager::Instance()->getRotatorController(m_Rotator->getDeviceName(), m_RotatorControlPanel);
                m_RotatorControlPanel->initRotator(opticalTrainCombo->currentText(), m_captureDeviceAdaptor.get(),
                                                   m_Rotator->getDeviceName());
                m_captureDeviceAdaptor->setRotator(device);
                connect(m_captureDeviceAdaptor.data(), &CaptureDeviceAdaptor::rotatorReverseToggled,
                        this,
                        &Capture::setRotatorReversed,
                        Qt::UniqueConnection);
                connect(rotatorB, &QPushButton::clicked, this, [this]()
                {
                    m_RotatorControlPanel->show();
                    m_RotatorControlPanel->raise();
                });
                rotatorB->setEnabled(true);
            }
        }
    }
}

bool Capture::setLightBox(ISD::LightBox *device)
{
    if (m_LightBox && m_LightBox == device)
        return false;

    if (m_LightBox)
        m_LightBox->disconnect(this);

    m_LightBox = device;

    // forward it to the command processor
    m_captureDeviceAdaptor->setLightBox(m_LightBox);
    m_captureModuleState->setLightBoxLightState(CaptureModuleState::CAP_LIGHT_UNKNOWN);

    return true;
}

void Capture::pause()
{
    if (m_captureModuleState->getCaptureState() != CAPTURE_CAPTURING)
    {
        // Ensure that the pause function is only called during frame capturing
        // Handling it this way is by far easier than trying to enable/disable the pause button
        // Fixme: make pausing possible at all stages. This makes it necessary to separate the pausing states from CaptureState.
        appendLogText(i18n("Pausing only possible while frame capture is running."));
        qCInfo(KSTARS_EKOS_CAPTURE) << "Pause button pressed while not capturing.";
        return;
    }
    // we do not decide at this stage how to resume, since pause is only planned here
    m_captureModuleState->setContinueAction(CaptureModuleState::CONTINUE_ACTION_NONE);
    m_captureModuleState->setCaptureState(CAPTURE_PAUSE_PLANNED);
    appendLogText(i18n("Sequence shall be paused after current exposure is complete."));
    pauseB->setEnabled(false);

    startB->setIcon(QIcon::fromTheme("media-playback-start"));
    startB->setToolTip(i18n("Resume Sequence"));
}

void Capture::toggleSequence()
{
    if (m_captureModuleState->getCaptureState() == CAPTURE_PAUSE_PLANNED
            || m_captureModuleState->getCaptureState() == CAPTURE_PAUSED)
    {
        startB->setIcon(
            QIcon::fromTheme("media-playback-stop"));
        startB->setToolTip(i18n("Stop Sequence"));
        pauseB->setEnabled(true);

        // change the state back to capturing only if planned pause is cleared
        if (m_captureModuleState->getCaptureState() == CAPTURE_PAUSE_PLANNED)
            m_captureModuleState->setCaptureState(CAPTURE_CAPTURING);

        appendLogText(i18n("Sequence resumed."));

        // Call from where ever we have left of when we paused
        switch (m_captureModuleState->getContinueAction())
        {
            case CaptureModuleState::CONTINUE_ACTION_CAPTURE_COMPLETE:
                m_captureProcess->resumeSequence();
                break;
            case CaptureModuleState::CONTINUE_ACTION_NEXT_EXPOSURE:
                m_captureProcess->startNextExposure();
                break;
            default:
                break;
        }
    }
    else if (m_captureModuleState->getCaptureState() == CAPTURE_IDLE
             || m_captureModuleState->getCaptureState() == CAPTURE_ABORTED
             || m_captureModuleState->getCaptureState() == CAPTURE_COMPLETE)
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
    if (name == "Mount" && mountInterface == nullptr)
    {
        qCDebug(KSTARS_EKOS_CAPTURE) << "Registering new Module (" << name << ")";
        mountInterface = new QDBusInterface("org.kde.kstars", "/KStars/Ekos/Mount",
                                            "org.kde.kstars.Ekos.Mount", QDBusConnection::sessionBus(), this);

    }
}

void Capture::start()
{
    //    if (darkSubCheck->isChecked())
    //    {
    //        KSNotification::error(i18n("Auto dark subtract is not supported in batch mode."));
    //        return;
    //    }

    m_captureModuleState->setStartingCapture(false);

    // Reset progress option if there is no captured frame map set at the time of start - fixes the end-user setting the option just before starting
    m_captureModuleState->setIgnoreJobProgress(!m_captureModuleState->hasCapturedFramesMap()
            && Options::alwaysResetSequenceWhenStarting());

    if (queueTable->rowCount() == 0)
    {
        if (addJob() == false)
            return;
    }

    SequenceJob * first_job = nullptr;

    for (auto &job : m_captureModuleState->allJobs())
    {
        if (job->getStatus() == JOB_IDLE || job->getStatus() == JOB_ABORTED)
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
        for (auto &job : m_captureModuleState->allJobs())
        {
            if (job->getStatus() != JOB_DONE)
            {
                // If we arrived here with a zero-delay timer, raise the interval before returning to avoid a cpu peak
                if (m_captureModuleState->getCaptureDelayTimer().isActive())
                {
                    if (m_captureModuleState->getCaptureDelayTimer().interval() <= 0)
                        m_captureModuleState->getCaptureDelayTimer().setInterval(1000);
                }
                else appendLogText(i18n("No pending jobs found. Please add a job to the sequence queue."));
                return;
            }
        }

        // If we only have completed jobs and we don't ignore job progress, ask the end-user what to do
        if (!m_captureModuleState->ignoreJobProgress())
            if(KMessageBox::warningContinueCancel(
                        nullptr,
                        i18n("All jobs are complete. Do you want to reset the status of all jobs and restart capturing?"),
                        i18n("Reset job status"), KStandardGuiItem::cont(), KStandardGuiItem::cancel(),
                        "reset_job_complete_status_warning") != KMessageBox::Continue)
                return;

        // If the end-user accepted to reset, reset all jobs and restart
        for (auto &job : m_captureModuleState->allJobs())
            job->resetStatus();

        first_job = m_captureModuleState->allJobs().first();
    }
    // If we need to ignore job progress, systematically reset all jobs and restart
    // Scheduler will never ignore job progress and doesn't use this path
    else if (m_captureModuleState->ignoreJobProgress())
    {
        appendLogText(i18n("Warning: option \"Always Reset Sequence When Starting\" is enabled and resets the sequence counts."));
        for (auto &job : m_captureModuleState->allJobs())
            job->resetStatus();
    }

    // Refocus timer should not be reset on deviation error
    if (m_captureModuleState->isGuidingDeviationDetected() == false
            && m_captureModuleState->getCaptureState() != CAPTURE_SUSPENDED)
    {
        // start timer to measure time until next forced refocus
        m_captureModuleState->getRefocusState()->startRefocusTimer();
    }

    // Only reset these counters if we are NOT restarting from deviation errors
    // So when starting a new job or fresh then we reset them.
    if (m_captureModuleState->isGuidingDeviationDetected() == false)
    {
        m_captureModuleState->resetDitherCounter();
        m_captureModuleState->getRefocusState()->resetInSequenceFocusCounter();
        m_captureModuleState->getRefocusState()->setAdaptiveFocusDone(false);
    }

    m_captureModuleState->setGuidingDeviationDetected(false);
    m_captureModuleState->resetSpikesDetected();

    m_captureModuleState->setCaptureState(CAPTURE_PROGRESS);

    startB->setIcon(QIcon::fromTheme("media-playback-stop"));
    startB->setToolTip(i18n("Stop Sequence"));
    pauseB->setEnabled(true);

    m_captureModuleState->setBusy(true);

    if (Options::enforceGuideDeviation() && m_captureModuleState->isGuidingOn() == false)
        appendLogText(i18n("Warning: Guide deviation is selected but autoguide process was not started."));
    if (m_LimitsUI->limitFocusHFRS->isChecked() && m_captureModuleState->getRefocusState()->isAutoFocusReady() == false)
        appendLogText(i18n("Warning: in-sequence focusing is selected but autofocus process was not started."));
    if (m_LimitsUI->limitFocusDeltaTS->isChecked() && m_captureModuleState->getRefocusState()->isAutoFocusReady() == false)
        appendLogText(i18n("Warning: temperature delta check is selected but autofocus process was not started."));

    m_captureProcess->prepareJob(first_job);
}

void Ekos::Capture::changeSequenceValue(int index, QString key, QString value)
{
    QJsonArray seqArray = m_captureModuleState->getSequence();
    QJsonObject oneSequence = seqArray[index].toObject();
    oneSequence[key] = value;
    seqArray.replace(index, oneSequence);
    m_captureModuleState->setSequence(seqArray);
    emit sequenceChanged(seqArray);
}

/**
 * @brief Stop, suspend or abort the currently active job.
 * @param targetState
 */

void Capture::stop(CaptureState targetState)
{
    m_captureModuleState->resetAlignmentRetries();
    //seqTotalCount   = 0;
    //seqCurrentCount = 0;

    m_captureModuleState->getCaptureTimeout().stop();
    m_captureModuleState->getCaptureDelayTimer().stop();

    m_captureProcess->clearFlatCache();

    if (m_captureModuleState->getActiveJob() != nullptr)
    {
        if (m_captureModuleState->getActiveJob()->getStatus() == JOB_BUSY)
        {
            QString stopText;
            switch (targetState)
            {
                case CAPTURE_SUSPENDED:
                    stopText = i18n("CCD capture suspended");
                    m_captureModuleState->getActiveJob()->resetStatus(JOB_BUSY);
                    break;

                case CAPTURE_COMPLETE:
                    stopText = i18n("CCD capture complete");
                    m_captureModuleState->getActiveJob()->resetStatus(JOB_DONE);
                    break;

                case CAPTURE_ABORTED:
                    stopText = i18n("CCD capture aborted");
                    m_captureModuleState->getActiveJob()->resetStatus(JOB_ABORTED);
                    break;

                default:
                    stopText = i18n("CCD capture stopped");
                    m_captureModuleState->getActiveJob()->resetStatus(JOB_IDLE);
                    break;
            }
            emit captureAborted(m_captureModuleState->getActiveJob()->getCoreProperty(SequenceJob::SJ_Exposure).toDouble());
            KSNotification::event(QLatin1String("CaptureFailed"), stopText, KSNotification::Capture, KSNotification::Alert);
            appendLogText(stopText);
            m_captureModuleState->getActiveJob()->abort();
            if (m_captureModuleState->getActiveJob()->getCoreProperty(SequenceJob::SJ_Preview).toBool() == false)
            {
                int index = m_captureModuleState->allJobs().indexOf(m_captureModuleState->getActiveJob());
                changeSequenceValue(index, "Status", "Aborted");
            }
        }

        // In case of batch job
        if (m_captureModuleState->getActiveJob()->getCoreProperty(SequenceJob::SJ_Preview).toBool() == false)
        {
            m_captureModuleState->getActiveJob()->disconnect(this);
        }
        // or preview job in calibration stage
        else if (m_captureModuleState->getActiveJob()->getCalibrationStage() == SequenceJobState::CAL_CALIBRATION)
        {
            m_captureModuleState->getActiveJob()->disconnect(this);
            m_captureModuleState->getActiveJob()->setCoreProperty(SequenceJob::SJ_Preview, false);
            if (m_captureDeviceAdaptor->getActiveCamera())
                m_captureDeviceAdaptor->getActiveCamera()->setUploadMode(m_captureModuleState->getActiveJob()->getUploadMode());
        }
        // or regular preview job
        else
        {
            if (m_captureDeviceAdaptor->getActiveCamera())
                m_captureDeviceAdaptor->getActiveCamera()->setUploadMode(m_captureModuleState->getActiveJob()->getUploadMode());
            m_captureModuleState->allJobs().removeOne(m_captureModuleState->getActiveJob());
            // Delete preview job
            m_captureModuleState->getActiveJob()->deleteLater();
            // Clear active job
            m_captureModuleState->setActiveJob(nullptr);
        }
    }

    // stop focusing if capture is aborted
    if (m_captureModuleState->getCaptureState() == CAPTURE_FOCUSING && targetState == CAPTURE_ABORTED)
        emit abortFocus();

    m_captureModuleState->setCaptureState(targetState);

    // Turn off any calibration light, IF they were turned on by Capture module
    if (m_captureDeviceAdaptor->getLightBox() && lightBoxLightEnabled)
    {
        lightBoxLightEnabled = false;
        m_captureDeviceAdaptor->getLightBox()->setLightEnabled(false);
    }

    // disconnect camera device
    m_captureProcess->connectCamera(false);

    // In case of exposure looping, let's abort
    if (m_captureDeviceAdaptor->getActiveCamera() &&
            m_captureDeviceAdaptor->getActiveChip() &&
            m_captureDeviceAdaptor->getActiveCamera()->isFastExposureEnabled())
        m_captureDeviceAdaptor->getActiveChip()->abortExposure();

    imgProgress->reset();
    imgProgress->setEnabled(false);

    frameRemainingTime->setText("--:--:--");
    jobRemainingTime->setText("--:--:--");
    frameInfoLabel->setText(i18n("Expose (-/-):"));
    m_captureModuleState->setLooping(false);

    m_captureModuleState->setBusy(false);

    // stopping to CAPTURE_IDLE means that capturing will continue automatically
    auto captureState = m_captureModuleState->getCaptureState();
    if (captureState == CAPTURE_ABORTED || captureState == CAPTURE_SUSPENDED || captureState == CAPTURE_COMPLETE)
    {
        startB->setIcon(
            QIcon::fromTheme("media-playback-start"));
        startB->setToolTip(i18n("Start Sequence"));
        pauseB->setEnabled(false);
    }

    m_captureModuleState->getSeqDelayTimer().stop();

    m_captureModuleState->setActiveJob(nullptr);
}

QString Capture::camera()
{
    if (m_captureDeviceAdaptor->getActiveCamera())
        return m_captureDeviceAdaptor->getActiveCamera()->getDeviceName();

    return QString();
}

void Capture::checkCamera()
{
    // Do not update any camera settings while capture is in progress.
    if (m_captureModuleState->getCaptureState() == CAPTURE_CAPTURING || !m_Camera)
        return;

    m_captureDeviceAdaptor->setActiveCamera(m_Camera);

    m_captureDeviceAdaptor->setActiveChip(nullptr);

    // FIXME TODO fix guide head detection
    if (m_Camera->getDeviceName().contains("Guider"))
    {
        m_captureModuleState->setUseGuideHead(true);
        m_captureDeviceAdaptor->setActiveChip(m_Camera->getChip(ISD::CameraChip::GUIDE_CCD));
    }

    if (m_captureDeviceAdaptor->getActiveChip() == nullptr)
    {
        m_captureModuleState->setUseGuideHead(false);
        m_captureDeviceAdaptor->setActiveChip(m_Camera->getChip(ISD::CameraChip::PRIMARY_CCD));
    }

    // Make sure we have a valid chip and valid base device.
    // Make sure we are not in capture process.
    ISD::CameraChip *targetChip = m_captureDeviceAdaptor->getActiveChip();
    if (!targetChip || !targetChip->getCCD() || targetChip->isCapturing())
        return;

    if (m_Camera->hasCoolerControl())
    {
        coolerOnB->setEnabled(true);
        coolerOffB->setEnabled(true);
        coolerOnB->setChecked(m_Camera->isCoolerOn());
        coolerOffB->setChecked(!m_Camera->isCoolerOn());
    }
    else
    {
        coolerOnB->setEnabled(false);
        coolerOnB->setChecked(false);
        coolerOffB->setEnabled(false);
        coolerOffB->setChecked(false);
    }

    updateFrameProperties();

    QStringList frameTypes = m_captureDeviceAdaptor->getActiveChip()->getFrameTypes();

    captureTypeS->clear();

    if (frameTypes.isEmpty())
        captureTypeS->setEnabled(false);
    else
    {
        captureTypeS->setEnabled(true);
        captureTypeS->addItems(frameTypes);
        captureTypeS->setCurrentIndex(m_captureDeviceAdaptor->getActiveChip()->getFrameType());
    }

    // Capture Format
    captureFormatS->blockSignals(true);
    captureFormatS->clear();
    captureFormatS->addItems(m_Camera->getCaptureFormats());
    captureFormatS->setCurrentText(m_Camera->getCaptureFormat());
    captureFormatS->blockSignals(false);

    // Encoding format
    captureEncodingS->blockSignals(true);
    captureEncodingS->clear();
    captureEncodingS->addItems(m_Camera->getEncodingFormats());
    captureEncodingS->setCurrentText(m_Camera->getEncodingFormat());
    captureEncodingS->blockSignals(false);

    customPropertiesDialog->setCCD(m_Camera);

    liveVideoB->setEnabled(m_Camera->hasVideoStream());
    if (m_Camera->hasVideoStream())
        setVideoStreamEnabled(m_Camera->isStreamingEnabled());
    else
        liveVideoB->setIcon(QIcon::fromTheme("camera-off"));

    connect(m_Camera, &ISD::Camera::propertyUpdated, this, &Capture::processCameraNumber, Qt::UniqueConnection);
    connect(m_Camera, &ISD::Camera::coolerToggled, this, &Capture::setCoolerToggled, Qt::UniqueConnection);
    connect(m_Camera, &ISD::Camera::newRemoteFile, this, &Capture::setNewRemoteFile, Qt::UniqueConnection);
    connect(m_Camera, &ISD::Camera::videoStreamToggled, this, &Capture::setVideoStreamEnabled, Qt::UniqueConnection);
    connect(m_Camera, &ISD::Camera::ready, this, &Capture::ready, Qt::UniqueConnection);
    connect(m_Camera, &ISD::Camera::error, m_captureProcess.data(), &CaptureProcess::processCaptureError, Qt::UniqueConnection);

    syncCameraInfo();

    // update values received by the device adaptor
    // connect(m_Camera, &ISD::Camera::newTemperatureValue, this, &Capture::updateCCDTemperature, Qt::UniqueConnection);

    DarkLibrary::Instance()->checkCamera();
}

void Capture::syncCameraInfo()
{
    auto m_Camera = m_captureDeviceAdaptor->getActiveCamera();
    if (!m_Camera)
        return;

    if (m_Camera->hasCooler())
    {
        cameraTemperatureS->setEnabled(true);
        cameraTemperatureN->setEnabled(true);

        if (m_Camera->getPermission("CCD_TEMPERATURE") != IP_RO)
        {
            double min, max, step;
            setTemperatureB->setEnabled(true);
            cameraTemperatureN->setReadOnly(false);
            cameraTemperatureS->setEnabled(true);
            temperatureRegulationB->setEnabled(true);
            m_Camera->getMinMaxStep("CCD_TEMPERATURE", "CCD_TEMPERATURE_VALUE", &min, &max, &step);
            cameraTemperatureN->setMinimum(min);
            cameraTemperatureN->setMaximum(max);
            cameraTemperatureN->setSingleStep(1);
            bool isChecked = m_Camera->getDriverInfo()->getAuxInfo().value(QString("%1_TC").arg(m_Camera->getDeviceName()),
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
        if (m_Camera->getTemperature(&temperature))
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

    auto isoList = m_captureDeviceAdaptor->getActiveChip()->getISOList();
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
        captureISOS->setCurrentIndex(m_captureDeviceAdaptor->getActiveChip()->getISOIndex());

        uint16_t w, h;
        uint8_t bbp {8};
        double pixelX = 0, pixelY = 0;
        bool rc = m_captureDeviceAdaptor->getActiveChip()->getImageInfo(w, h, pixelX, pixelY, bbp);
        bool isModelInDB = isModelinDSLRInfo(QString(m_Camera->getDeviceName()));
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
                QString model = QString(m_Camera->getDeviceName());
                syncDSLRToTargetChip(model);
            }
        }
    }
    captureISOS->blockSignals(false);

    // Gain Check
    if (m_Camera->hasGain())
    {
        double min, max, step, value, targetCustomGain;
        m_Camera->getGainMinMaxStep(&min, &max, &step);

        // Allow the possibility of no gain value at all.
        GainSpinSpecialValue = min - step;
        captureGainN->setRange(GainSpinSpecialValue, max);
        captureGainN->setSpecialValueText(i18n("--"));
        captureGainN->setEnabled(true);
        captureGainN->setSingleStep(step);
        m_Camera->getGain(&value);
        currentGainLabel->setText(QString::number(value, 'f', 0));

        targetCustomGain = getGain();

        // Set the custom gain if we have one
        // otherwise it will not have an effect.
        if (targetCustomGain > 0)
            captureGainN->setValue(targetCustomGain);
        else
            captureGainN->setValue(GainSpinSpecialValue);

        captureGainN->setReadOnly(m_Camera->getGainPermission() == IP_RO);

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
    if (m_Camera->hasOffset())
    {
        double min, max, step, value, targetCustomOffset;
        m_Camera->getOffsetMinMaxStep(&min, &max, &step);

        // Allow the possibility of no Offset value at all.
        OffsetSpinSpecialValue = min - step;
        captureOffsetN->setRange(OffsetSpinSpecialValue, max);
        captureOffsetN->setSpecialValueText(i18n("--"));
        captureOffsetN->setEnabled(true);
        captureOffsetN->setSingleStep(step);
        m_Camera->getOffset(&value);
        currentOffsetLabel->setText(QString::number(value, 'f', 0));

        targetCustomOffset = getOffset();

        // Set the custom Offset if we have one
        // otherwise it will not have an effect.
        if (targetCustomOffset > 0)
            captureOffsetN->setValue(targetCustomOffset);
        else
            captureOffsetN->setValue(OffsetSpinSpecialValue);

        captureOffsetN->setReadOnly(m_Camera->getOffsetPermission() == IP_RO);

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
    if (!m_captureDeviceAdaptor->getActiveCamera())
        return;

    m_captureModuleState->setSuspendGuidingOnDownload((m_captureDeviceAdaptor->getActiveCamera()->getChip(
                ISD::CameraChip::GUIDE_CCD) == guideChip) ||
            (guideChip->getCCD() == m_captureDeviceAdaptor->getActiveCamera() &&
             m_captureDeviceAdaptor->getActiveCamera()->getDriverInfo()->getAuxInfo().value("mdpd", false).toBool()));
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
    if (!m_captureDeviceAdaptor->getActiveCamera())
        return;

    int binx = 1, biny = 1;
    double min, max, step;
    int xstep = 0, ystep = 0;

    QString frameProp    = m_captureModuleState->useGuideHead() ? QString("GUIDER_FRAME") : QString("CCD_FRAME");
    QString exposureProp = m_captureModuleState->useGuideHead() ? QString("GUIDER_EXPOSURE") : QString("CCD_EXPOSURE");
    QString exposureElem = m_captureModuleState->useGuideHead() ? QString("GUIDER_EXPOSURE_VALUE") :
                           QString("CCD_EXPOSURE_VALUE");
    m_captureDeviceAdaptor->setActiveChip(m_captureModuleState->useGuideHead() ?
                                          m_captureDeviceAdaptor->getActiveCamera()->getChip(
                                                  ISD::CameraChip::GUIDE_CCD) :
                                          m_captureDeviceAdaptor->getActiveCamera()->getChip(ISD::CameraChip::PRIMARY_CCD));

    captureFrameWN->setEnabled(m_captureDeviceAdaptor->getActiveChip()->canSubframe());
    captureFrameHN->setEnabled(m_captureDeviceAdaptor->getActiveChip()->canSubframe());
    captureFrameXN->setEnabled(m_captureDeviceAdaptor->getActiveChip()->canSubframe());
    captureFrameYN->setEnabled(m_captureDeviceAdaptor->getActiveChip()->canSubframe());

    captureBinHN->setEnabled(m_captureDeviceAdaptor->getActiveChip()->canBin());
    captureBinVN->setEnabled(m_captureDeviceAdaptor->getActiveChip()->canBin());

    QList<double> exposureValues;
    exposureValues << 0.01 << 0.02 << 0.05 << 0.1 << 0.2 << 0.25 << 0.5 << 1 << 1.5 << 2 << 2.5 << 3 << 5 << 6 << 7 << 8 << 9 <<
                   10 << 20 << 30 << 40 << 50 << 60 << 120 << 180 << 300 << 600 << 900 << 1200 << 1800;

    if (m_captureDeviceAdaptor->getActiveCamera()->getMinMaxStep(exposureProp, exposureElem, &min, &max, &step))
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
    m_captureModuleState->setExposureRange(exposureValues.first(), exposureValues.last());

    if (m_captureDeviceAdaptor->getActiveCamera()->getMinMaxStep(frameProp, "WIDTH", &min, &max, &step))
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

    if (m_captureDeviceAdaptor->getActiveCamera()->getMinMaxStep(frameProp, "HEIGHT", &min, &max, &step))
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

    if (m_captureDeviceAdaptor->getActiveCamera()->getMinMaxStep(frameProp, "X", &min, &max, &step))
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

    if (m_captureDeviceAdaptor->getActiveCamera()->getMinMaxStep(frameProp, "Y", &min, &max, &step))
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
    if (m_captureModuleState->useGuideHead() == false)
        cullToDSLRLimits();

    if (reset == 1 || m_captureModuleState->frameSettings().contains(m_captureDeviceAdaptor->getActiveChip()) == false)
    {
        QVariantMap settings;

        settings["x"]    = 0;
        settings["y"]    = 0;
        settings["w"]    = captureFrameWN->maximum();
        settings["h"]    = captureFrameHN->maximum();
        settings["binx"] = 1;
        settings["biny"] = 1;

        m_captureModuleState->frameSettings()[m_captureDeviceAdaptor->getActiveChip()] = settings;
    }
    else if (reset == 2 && m_captureModuleState->frameSettings().contains(m_captureDeviceAdaptor->getActiveChip()))
    {
        QVariantMap settings = m_captureModuleState->frameSettings()[m_captureDeviceAdaptor->getActiveChip()];
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

        m_captureModuleState->frameSettings()[m_captureDeviceAdaptor->getActiveChip()] = settings;
    }

    if (m_captureModuleState->frameSettings().contains(m_captureDeviceAdaptor->getActiveChip()))
    {
        QVariantMap settings = m_captureModuleState->frameSettings()[m_captureDeviceAdaptor->getActiveChip()];
        int x = settings["x"].toInt();
        int y = settings["y"].toInt();
        int w = settings["w"].toInt();
        int h = settings["h"].toInt();

        if (m_captureDeviceAdaptor->getActiveChip()->canBin())
        {
            m_captureDeviceAdaptor->getActiveChip()->getMaxBin(&binx, &biny);
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
    if (m_captureDeviceAdaptor->getActiveCamera() == nullptr)
        return;

    if ((prop.isNameMatch("CCD_FRAME") && m_captureModuleState->useGuideHead() == false) ||
            (prop.isNameMatch("GUIDER_FRAME") && m_captureModuleState->useGuideHead()))
        updateFrameProperties();
    else if ((prop.isNameMatch("CCD_INFO") && m_captureModuleState->useGuideHead() == false) ||
             (prop.isNameMatch("GUIDER_INFO") && m_captureModuleState->useGuideHead()))
        updateFrameProperties(2);
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

void Capture::resetFrame()
{
    m_captureDeviceAdaptor->setActiveChip(m_captureModuleState->useGuideHead() ?
                                          m_captureDeviceAdaptor->getActiveCamera()->getChip(
                                                  ISD::CameraChip::GUIDE_CCD) :
                                          m_captureDeviceAdaptor->getActiveCamera()->getChip(ISD::CameraChip::PRIMARY_CCD));
    m_captureDeviceAdaptor->getActiveChip()->resetFrame();
    updateFrameProperties(1);
}

void Capture::syncFrameType(const QString &name)
{
    if (!m_Camera || name != m_Camera->getDeviceName())
        return;

    ISD::CameraChip * tChip = m_captureDeviceAdaptor->getActiveCamera()->getChip(ISD::CameraChip::PRIMARY_CCD);

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

QString Capture::filterWheel()
{
    if (m_FilterWheel)
        return m_FilterWheel->getDeviceName();

    return QString();
}

bool Capture::setFilter(const QString &filter)
{
    if (m_FilterWheel)
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
    m_captureModuleState->setCurrentFilterPosition(m_FilterManager->getFilterPosition(),
            currentFilterText,
            m_FilterManager->getFilterLock(currentFilterText));
}

void Capture::checkFilter()
{
    FilterPosCombo->clear();

    if (!m_FilterWheel)
    {
        FilterPosLabel->setEnabled(false);
        FilterPosCombo->setEnabled(false);
        filterEditB->setEnabled(false);

        m_captureDeviceAdaptor->setFilterManager(m_FilterManager);
        return;
    }

    FilterPosLabel->setEnabled(true);
    FilterPosCombo->setEnabled(true);
    filterEditB->setEnabled(true);

    setupFilterManager();

    syncFilterInfo();

    FilterPosCombo->addItems(m_FilterManager->getFilterLabels());

    updateCurrentFilterPosition();

    filterEditB->setEnabled(m_captureModuleState->getCurrentFilterPosition() > 0);

    FilterPosCombo->setCurrentIndex(m_captureModuleState->getCurrentFilterPosition() - 1);
}

void Capture::syncFilterInfo()
{
    QList<ISD::ConcreteDevice *> devices;
    if (m_Camera)
        devices.append(m_Camera);
    if (m_DustCap)
        devices.append(m_DustCap);

    for (auto &oneDevice : devices)
    {
        auto activeDevices = oneDevice->getText("ACTIVE_DEVICES");
        if (activeDevices)
        {
            auto activeFilter = activeDevices->findWidgetByName("ACTIVE_FILTER");
            if (activeFilter)
            {
                if (m_FilterWheel)
                {
                    if (activeFilter->getText() != m_FilterWheel->getDeviceName())
                    {
                        activeFilter->setText(m_FilterWheel->getDeviceName().toLatin1().constData());
                        oneDevice->sendNewProperty(activeDevices);
                    }
                }
                // Reset filter name in CCD driver
                else if (QString(activeFilter->getText()).isEmpty())
                {
                    // Add debug info since this issue is reported by users. Need to know when it happens.
                    qCDebug(KSTARS_EKOS_CAPTURE) << "No active filter wheel. " << oneDevice->getDeviceName() << " ACTIVE_FILTER is reset.";
                    activeFilter->setText("");
                    oneDevice->sendNewProperty(activeDevices);
                }
            }
        }
    }
}

void Capture::processingFITSfinished(bool success)
{
    // do nothing in case of failure
    if (success == false)
        return;

    // If this is a preview job, make sure to enable preview button after
    if (m_captureDeviceAdaptor->getActiveCamera()
            && m_captureDeviceAdaptor->getActiveCamera()->getUploadMode() != ISD::Camera::UPLOAD_LOCAL)
        previewB->setEnabled(true);

    imageCapturingCompleted();
}

void Capture::imageCapturingCompleted()
{
    SequenceJob *thejob = m_captureModuleState->getActiveJob();

    if (!thejob)
        return;

    // In case we're framing, let's return quickly to continue the process.
    if (m_captureModuleState->isLooping())
    {
        captureStatusWidget->setStatus(i18n("Framing..."), Qt::darkGreen);
        return;
    }

    // If fast exposure is off, disconnect exposure progress
    // otherwise, keep it going since it fires off from driver continuous capture process.
    if (m_captureDeviceAdaptor->getActiveCamera()->isFastExposureEnabled() == false)
        DarkLibrary::Instance()->disconnect(this);

    // Do not display notifications for very short captures
    if (thejob->getCoreProperty(SequenceJob::SJ_Exposure).toDouble() >= 1)
        KSNotification::event(QLatin1String("EkosCaptureImageReceived"), i18n("Captured image received"),
                              KSNotification::Capture);

    // If it was initially set as pure preview job and NOT as preview for calibration
    if (thejob->getCoreProperty(SequenceJob::SJ_Preview).toBool())
        return;

    /* The image progress has now one more capture */
    imgProgress->setValue(thejob->getCompleted());
}

void Capture::captureOne()
{
    if (m_captureModuleState->getFocusState() >= FOCUS_PROGRESS)
    {
        appendLogText(i18n("Cannot capture while focus module is busy."));
    }
    //    else if (captureEncodingS->currentIndex() == ISD::Camera::FORMAT_NATIVE && darkSubCheck->isChecked())
    //    {
    //        appendLogText(i18n("Cannot perform auto dark subtraction of native DSLR formats."));
    //    }
    else if (addJob(true))
    {
        m_captureModuleState->setCaptureState(CAPTURE_PROGRESS);
        m_captureProcess->prepareJob(m_captureModuleState->allJobs().last());
    }
}

void Capture::startFraming()
{
    if (m_captureModuleState->getFocusState() >= FOCUS_PROGRESS)
    {
        appendLogText(i18n("Cannot start framing while focus module is busy."));
    }
    else if (!m_captureModuleState->isLooping())
    {
        m_captureModuleState->setLooping(true);
        appendLogText(i18n("Starting framing..."));
        captureOne();
    }
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
    if (m_captureDeviceAdaptor->getFilterWheel() != nullptr)
    {
        // JM 2021.08.23 Call filter info to set the active filter wheel in the camera driver
        // so that it may snoop on the active filter
        syncFilterInfo();
        updateCurrentFilterPosition();
    }

    // necessary since the status widget doesn't store the calibration stage
    if (m_captureModuleState->getActiveJob()->getCalibrationStage() == SequenceJobState::CAL_CALIBRATION)
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

void Capture::captureStarted(CaptureModuleState::CAPTUREResult rc)
{
    if (rc != CaptureModuleState::CAPTURE_OK)
    {
        disconnect(m_captureDeviceAdaptor->getActiveCamera(), &ISD::Camera::newExposureValue, this,
                   &Capture::setExposureProgress);
    }
    switch (rc)
    {
        case CaptureModuleState::CAPTURE_OK:
        {
            m_captureModuleState->setCaptureState(CAPTURE_CAPTURING);
            emit captureStarting(m_captureModuleState->getActiveJob()->getCoreProperty(SequenceJob::SJ_Exposure).toDouble(),
                                 m_captureModuleState->getActiveJob()->getCoreProperty(SequenceJob::SJ_Filter).toString());
            appendLogText(i18n("Capturing %1-second %2 image...",
                               QString("%L1").arg(m_captureModuleState->getActiveJob()->getCoreProperty(SequenceJob::SJ_Exposure).toDouble(), 0, 'f', 3),
                               m_captureModuleState->getActiveJob()->getCoreProperty(SequenceJob::SJ_Filter).toString()));
            m_captureModuleState->getCaptureTimeout().start(static_cast<int>(m_captureModuleState->getActiveJob()->getCoreProperty(
                        SequenceJob::SJ_Exposure).toDouble()) * 1000 +
                    CAPTURE_TIMEOUT_THRESHOLD);
            // calculate remaining capture time for the current job
            imageCountDown.setHMS(0, 0, 0);
            double ms_left = std::ceil(m_captureModuleState->getActiveJob()->getExposeLeft() * 1000.0);
            imageCountDown = imageCountDown.addMSecs(int(ms_left));
            lastRemainingFrameTimeMS = ms_left;
            sequenceCountDown.setHMS(0, 0, 0);
            sequenceCountDown = sequenceCountDown.addSecs(getActiveJobRemainingTime());
            frameInfoLabel->setText(QString("%1 (%L3/%L4):").arg(frameLabel(m_captureModuleState->getActiveJob()->getFrameType(),
                                    m_captureModuleState->getActiveJob()->getCoreProperty(SequenceJob::SJ_Filter).toString()))
                                    .arg(m_captureModuleState->getActiveJob()->getCompleted()).arg(m_captureModuleState->getActiveJob()->getCoreProperty(
                                                SequenceJob::SJ_Count).toInt()));
            // ensure that the download time label is visible
            avgDownloadTime->setVisible(true);
            avgDownloadLabel->setVisible(true);
            secLabel->setVisible(true);
            // show estimated download time
            avgDownloadTime->setText(QString("%L1").arg(m_captureModuleState->averageDownloadTime(), 0, 'd', 2));

            if (m_captureModuleState->getActiveJob()->getCoreProperty(SequenceJob::SJ_Preview).toBool() == false)
            {
                auto index = m_captureModuleState->allJobs().indexOf(m_captureModuleState->getActiveJob());
                if (index >= 0 && index < m_captureModuleState->getSequence().count())
                    changeSequenceValue(index, "Status", "In Progress");
            }
        }
        break;

        case CaptureModuleState::CAPTURE_FRAME_ERROR:
            appendLogText(i18n("Failed to set sub frame."));
            abort();
            break;

        case CaptureModuleState::CAPTURE_BIN_ERROR:
            appendLogText(i18n("Failed to set binning."));
            abort();
            break;

        case CaptureModuleState::CAPTURE_FOCUS_ERROR:
            appendLogText(i18n("Cannot capture while focus module is busy."));
            abort();
            break;
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

void Capture::setDownloadProgress()
{
    if (m_captureModuleState->getActiveJob())
    {
        double downloadTimeLeft = m_captureModuleState->averageDownloadTime() - m_captureModuleState->downloadTimer().elapsed() /
                                  1000.0;
        if(downloadTimeLeft > 0)
        {
            imageCountDown.setHMS(0, 0, 0);
            imageCountDown = imageCountDown.addSecs(int(std::ceil(downloadTimeLeft)));
            frameRemainingTime->setText(imageCountDown.toString("hh:mm:ss"));
            emit newDownloadProgress(downloadTimeLeft);
        }
    }
}

void Capture::setExposureProgress(ISD::CameraChip * tChip, double value, IPState state)
{
    if (m_captureDeviceAdaptor->getActiveChip() != tChip ||
            m_captureDeviceAdaptor->getActiveChip()->getCaptureMode() != FITS_NORMAL
            || getMeridianFlipState()->getMeridianFlipStage() >= MeridianFlipState::MF_ALIGNING)
        return;

    double deltaMS = std::ceil(1000.0 * value - lastRemainingFrameTimeMS);
    updateCaptureCountDown(int(deltaMS));
    lastRemainingFrameTimeMS += deltaMS;

    if (m_captureModuleState->getActiveJob())
    {
        m_captureModuleState->getActiveJob()->setExposeLeft(value);

        emit newExposureProgress(m_captureModuleState->getActiveJob());
    }

    if (m_captureModuleState->getActiveJob() && state == IPS_ALERT)
    {
        int retries = m_captureModuleState->getActiveJob()->getCaptureRetires() + 1;

        m_captureModuleState->getActiveJob()->setCaptureRetires(retries);

        appendLogText(i18n("Capture failed. Check INDI Control Panel for details."));

        if (retries == 3)
        {
            abort();
            return;
        }

        appendLogText(i18n("Restarting capture attempt #%1", retries));

        m_captureModuleState->setNextSequenceID(1);

        m_captureProcess->captureImage();
        return;
    }

    if (m_captureModuleState->getActiveJob() != nullptr && state == IPS_OK)
    {
        m_captureModuleState->getActiveJob()->setCaptureRetires(0);
        m_captureModuleState->getActiveJob()->setExposeLeft(0);

        if (m_captureDeviceAdaptor->getActiveCamera()
                && m_captureDeviceAdaptor->getActiveCamera()->getUploadMode() == ISD::Camera::UPLOAD_LOCAL)
        {
            if (m_captureModuleState->getActiveJob()->getStatus() == JOB_BUSY)
            {
                processingFITSfinished(false);
                return;
            }
        }

        //if (isAutoGuiding && Options::useEkosGuider() && currentCCD->getChip(ISD::CameraChip::GUIDE_CCD) == guideChip)
        if (m_captureModuleState->getGuideState() == GUIDE_GUIDING && Options::guiderType() == 0
                && m_captureModuleState->suspendGuidingOnDownload())
        {
            qCDebug(KSTARS_EKOS_CAPTURE) << "Autoguiding suspended until primary CCD chip completes downloading...";
            emit suspendGuiding();
        }

        captureStatusWidget->setStatus(i18n("Downloading..."), Qt::yellow);

        //This will start the clock to see how long the download takes.
        m_captureModuleState->downloadTimer().start();
        m_captureModuleState->downloadProgressTimer().start();


        //disconnect(m_Camera, &ISD::Camera::newExposureValue(ISD::CameraChip*,double,IPState)), this, &Capture::updateCaptureProgress(ISD::CameraChip*,double,IPState)));
    }
}

void Capture::updateCaptureCountDown(int deltaMillis)
{
    imageCountDown = imageCountDown.addMSecs(deltaMillis);
    sequenceCountDown = sequenceCountDown.addMSecs(deltaMillis);
    frameRemainingTime->setText(imageCountDown.toString("hh:mm:ss"));
    jobRemainingTime->setText(sequenceCountDown.toString("hh:mm:ss"));
}

void Capture::updateCCDTemperature(double value)
{
    if (cameraTemperatureS->isEnabled() == false && m_captureDeviceAdaptor->getActiveCamera())
    {
        if (m_captureDeviceAdaptor->getActiveCamera()->getPermission("CCD_TEMPERATURE") != IP_RO)
            checkCamera();
    }

    temperatureOUT->setText(QString("%L1").arg(value, 0, 'f', 2));

    if (cameraTemperatureN->cleanText().isEmpty())
        cameraTemperatureN->setValue(value);
}

void Capture::updateRotatorAngle(double value)
{
    IPState RState = m_captureDeviceAdaptor->getRotator()->absoluteAngleState();
    if (RState == IPS_OK)
        m_RotatorControlPanel->updateRotator(value);
    else
        m_RotatorControlPanel->updateGauge(value);
}

bool Capture::addSequenceJob()
{
    return addJob(false, false);
}

bool Capture::addJob(bool preview, bool isDarkFlat, FilenamePreviewType filenamePreview)
{
    if (m_captureModuleState->getCaptureState() != CAPTURE_IDLE && m_captureModuleState->getCaptureState() != CAPTURE_ABORTED
            && m_captureModuleState->getCaptureState() != CAPTURE_COMPLETE)
        return false;

    SequenceJob * job = nullptr;

    if (filenamePreview == NOT_PREVIEW)
    {
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
    }

    if (m_JobUnderEdit && filenamePreview == NOT_PREVIEW)
        job = m_captureModuleState->allJobs().at(queueTable->currentRow());
    else
    {
        job = new SequenceJob(m_captureDeviceAdaptor, m_captureModuleState);
    }

    Q_ASSERT_X(job, __FUNCTION__, "Capture Job is invalid.");

    job->setCoreProperty(SequenceJob::SJ_Format, captureFormatS->currentText());
    job->setCoreProperty(SequenceJob::SJ_Encoding, captureEncodingS->currentText());
    job->setCoreProperty(SequenceJob::SJ_DarkFlat, isDarkFlat);
    job->setCoreProperty(SequenceJob::SJ_UsingPlaceholders, true);

    if (captureISOS)
        job->setCoreProperty(SequenceJob::SJ_ISOIndex, captureISOS->currentIndex());

    if (getGain() >= 0)
        job->setCoreProperty(SequenceJob::SJ_Gain, getGain());

    if (getOffset() >= 0)
        job->setCoreProperty(SequenceJob::SJ_Offset, getOffset());

    job->setCoreProperty(SequenceJob::SJ_Encoding, captureEncodingS->currentText());
    job->setCoreProperty(SequenceJob::SJ_Preview, preview);

    if (cameraTemperatureN->isEnabled())
    {
        job->setCoreProperty(SequenceJob::SJ_EnforceTemperature, cameraTemperatureS->isChecked());
        job->setTargetTemperature(cameraTemperatureN->value());
    }

    job->setUploadMode(static_cast<ISD::Camera::UploadMode>(fileUploadModeS->currentIndex()));
    job->setScripts(m_Scripts);
    job->setFlatFieldDuration(flatFieldDuration);
    job->setFlatFieldSource(flatFieldSource);
    job->setPreMountPark(preMountPark);
    job->setPreDomePark(preDomePark);
    job->setWallCoord(wallCoord);
    job->setCoreProperty(SequenceJob::SJ_TargetADU, targetADU);
    job->setCoreProperty(SequenceJob::SJ_TargetADUTolerance, m_captureModuleState->targetADUTolerance());
    job->setCoreProperty(SequenceJob::SJ_FilterPrefixEnabled, FilterEnabled);
    job->setCoreProperty(SequenceJob::SJ_ExpPrefixEnabled, ExpEnabled);
    job->setCoreProperty(SequenceJob::SJ_TimeStampPrefixEnabled, TimeStampEnabled);
    job->setFrameType(static_cast<CCDFrameType>(qMax(0, captureTypeS->currentIndex())));

    job->setCoreProperty(SequenceJob::SJ_EnforceStartGuiderDrift, (job->getFrameType() == FRAME_LIGHT
                         && Options::enforceStartGuiderDrift()));
    job->setTargetStartGuiderDrift(Options::startGuideDeviation());

    //if (filterSlot != nullptr && currentFilter != nullptr)
    if (FilterPosCombo->currentIndex() != -1 && m_captureDeviceAdaptor->getFilterWheel() != nullptr)
        job->setTargetFilter(FilterPosCombo->currentIndex() + 1, FilterPosCombo->currentText());

    job->setCoreProperty(SequenceJob::SJ_Exposure, captureExposureN->value());

    job->setCoreProperty(SequenceJob::SJ_Count, captureCountN->value());

    job->setCoreProperty(SequenceJob::SJ_Binning, QPoint(captureBinHN->value(), captureBinVN->value()));

    /* in ms */
    job->setCoreProperty(SequenceJob::SJ_Delay, captureDelayN->value() * 1000);

    // Custom Properties
    job->setCustomProperties(customPropertiesDialog->getCustomProperties());

    if (m_captureDeviceAdaptor->getRotator() && m_RotatorControlPanel && m_RotatorControlPanel->isRotationEnforced())
    {
        job->setTargetRotation(m_RotatorControlPanel->getCameraPA());
    }

    job->setCoreProperty(SequenceJob::SJ_ROI, QRect(captureFrameXN->value(), captureFrameYN->value(), captureFrameWN->value(),
                         captureFrameHN->value()));
    job->setCoreProperty(SequenceJob::SJ_RemoteDirectory, fileRemoteDirT->text());
    job->setCoreProperty(SequenceJob::SJ_LocalDirectory, fileDirectoryT->text());
    job->setCoreProperty(SequenceJob::SJ_PlaceholderFormat, placeholderFormatT->text());
    job->setCoreProperty(SequenceJob::SJ_PlaceholderSuffix, formatSuffixN->value());

    if (m_JobUnderEdit == false || filenamePreview != NOT_PREVIEW)
    {
        // JM 2018-09-24: If this is the first job added
        // We always ignore job progress by default.
        if (m_captureModuleState->allJobs().isEmpty() && preview == false)
            m_captureModuleState->setIgnoreJobProgress(true);

        m_captureModuleState->allJobs().append(job);

        // Nothing more to do if preview
        if (preview)
            return true;
    }

    QJsonObject jsonJob = {{"Status", "Idle"}};

    auto placeholderPath = PlaceholderPath();
    placeholderPath.addJob(job, m_captureModuleState->targetName());

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
    if (FilterPosCombo->count() > 0 && (captureTypeS->currentIndex() == FRAME_LIGHT
                                        || captureTypeS->currentIndex() == FRAME_FLAT || isDarkFlat))
    {
        filter->setText(FilterPosCombo->currentText());
        jsonJob.insert("Filter", FilterPosCombo->currentText());
    }

    filter->setTextAlignment(Qt::AlignHCenter);
    filter->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

    QTableWidgetItem * count = m_JobUnderEdit ? queueTable->item(currentRow, 2) : new QTableWidgetItem();
    job->setCountCell(count);
    jsonJob.insert("Count", count->text());

    QTableWidgetItem * exp = m_JobUnderEdit ? queueTable->item(currentRow, 3) : new QTableWidgetItem();
    exp->setText(QString("%L1").arg(captureExposureN->value(), 0, 'f', captureExposureN->decimals()));
    exp->setTextAlignment(Qt::AlignHCenter);
    exp->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    jsonJob.insert("Exp", exp->text());

    QTableWidgetItem * type = m_JobUnderEdit ? queueTable->item(currentRow, 4) : new QTableWidgetItem();
    type->setText(isDarkFlat ? i18n("Dark Flat") : captureTypeS->currentText());
    type->setTextAlignment(Qt::AlignHCenter);
    type->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    jsonJob.insert("Type", isDarkFlat ? i18n("Dark Flat") : type->text());

    QTableWidgetItem * bin = m_JobUnderEdit ? queueTable->item(currentRow, 5) : new QTableWidgetItem();
    bin->setText(QString("%1x%2").arg(captureBinHN->value()).arg(captureBinVN->value()));
    bin->setTextAlignment(Qt::AlignHCenter);
    bin->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    jsonJob.insert("Bin", bin->text());

    QTableWidgetItem * iso = m_JobUnderEdit ? queueTable->item(currentRow, 6) : new QTableWidgetItem();
    if (captureISOS && captureISOS->currentIndex() != -1)
    {
        iso->setText(captureISOS->currentText());
        jsonJob.insert("ISO/Gain", iso->text());
    }
    else if (job->getCoreProperty(SequenceJob::SJ_Gain).toDouble() >= 0)
    {
        iso->setText(QString::number(job->getCoreProperty(SequenceJob::SJ_Gain).toDouble(), 'f', 1));
        jsonJob.insert("ISO/Gain", iso->text());
    }
    else
    {
        iso->setText("--");
        jsonJob.insert("ISO/Gain", "--");
    }
    iso->setTextAlignment(Qt::AlignHCenter);
    iso->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

    QTableWidgetItem * offset = m_JobUnderEdit ? queueTable->item(currentRow, 7) : new QTableWidgetItem();
    if (job->getCoreProperty(SequenceJob::SJ_Offset).toDouble() >= 0)
    {
        offset->setText(QString::number(job->getCoreProperty(SequenceJob::SJ_Offset).toDouble(), 'f', 1));
        jsonJob.insert("Offset", offset->text());
    }
    else
    {
        offset->setText("--");
        jsonJob.insert("Offset", "--");
    }
    offset->setTextAlignment(Qt::AlignHCenter);
    offset->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

    if (m_JobUnderEdit == false)
    {
        queueTable->setItem(currentRow, 0, status);
        queueTable->setItem(currentRow, 1, filter);
        queueTable->setItem(currentRow, 2, count);
        queueTable->setItem(currentRow, 3, exp);
        queueTable->setItem(currentRow, 4, type);
        queueTable->setItem(currentRow, 5, bin);
        queueTable->setItem(currentRow, 6, iso);
        queueTable->setItem(currentRow, 7, offset);

        m_captureModuleState->getSequence().append(jsonJob);
        emit sequenceChanged(m_captureModuleState->getSequence());
    }

    removeFromQueueB->setEnabled(true);

    if (queueTable->rowCount() > 0)
    {
        queueSaveAsB->setEnabled(true);
        queueSaveB->setEnabled(true);
        resetB->setEnabled(true);
        m_captureModuleState->setDirty(true);
    }

    if (queueTable->rowCount() > 1)
    {
        queueUpB->setEnabled(true);
        queueDownB->setEnabled(true);
    }

    if (m_JobUnderEdit && filenamePreview == NOT_PREVIEW)
    {
        m_JobUnderEdit = false;
        resetJobEdit();
        appendLogText(i18n("Job #%1 changes applied.", currentRow + 1));

        m_captureModuleState->getSequence().replace(currentRow, jsonJob);
        emit sequenceChanged(m_captureModuleState->getSequence());
    }

    QString signature = placeholderPath.generateFilename(*job, m_captureModuleState->targetName(),
                        filenamePreview != REMOTE_PREVIEW, true, 1,
                        ".fits", "", false, true);
    job->setCoreProperty(SequenceJob::SJ_Signature, signature);

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

bool Capture::removeJob(int index)
{
    if (m_captureModuleState->getCaptureState() != CAPTURE_IDLE && m_captureModuleState->getCaptureState() != CAPTURE_ABORTED
            && m_captureModuleState->getCaptureState() != CAPTURE_COMPLETE)
        return false;

    if (m_JobUnderEdit)
    {
        resetJobEdit();
        return false;
    }

    if (index < 0 || index >= m_captureModuleState->allJobs().count())
        return false;

    queueTable->removeRow(index);
    QJsonArray seqArray = m_captureModuleState->getSequence();
    seqArray.removeAt(index);
    m_captureModuleState->setSequence(seqArray);
    emit sequenceChanged(seqArray);

    if (m_captureModuleState->allJobs().empty())
        return true;

    SequenceJob * job = m_captureModuleState->allJobs().at(index);
    // remove completed frame counts from frame count map
    m_captureModuleState->removeCapturedFrameCount(job->getSignature(), job->getCompleted());
    // remove the job
    m_captureModuleState->allJobs().removeOne(job);
    if (job == m_captureModuleState->getActiveJob())
        m_captureModuleState->setActiveJob(nullptr);

    delete job;

    if (queueTable->rowCount() == 0)
        removeFromQueueB->setEnabled(false);

    if (queueTable->rowCount() == 1)
    {
        queueUpB->setEnabled(false);
        queueDownB->setEnabled(false);
    }

    for (int i = 0; i < m_captureModuleState->allJobs().count(); i++)
        m_captureModuleState->allJobs().at(i)->setStatusCell(queueTable->item(i, 0));

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

    m_captureModuleState->setDirty(true);

    return true;
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

    SequenceJob * job = m_captureModuleState->allJobs().takeAt(currentRow);

    m_captureModuleState->allJobs().removeOne(job);
    m_captureModuleState->allJobs().insert(destinationRow, job);

    QJsonArray seqArray = m_captureModuleState->getSequence();
    QJsonObject currentJob = seqArray[currentRow].toObject();
    seqArray.replace(currentRow, seqArray[destinationRow]);
    seqArray.replace(destinationRow, currentJob);
    emit sequenceChanged(seqArray);

    queueTable->selectRow(destinationRow);

    for (int i = 0; i < m_captureModuleState->allJobs().count(); i++)
        m_captureModuleState->allJobs().at(i)->setStatusCell(queueTable->item(i, 0));

    m_captureModuleState->setDirty(true);
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

    SequenceJob * job = m_captureModuleState->allJobs().takeAt(currentRow);

    m_captureModuleState->allJobs().removeOne(job);
    m_captureModuleState->allJobs().insert(destinationRow, job);

    QJsonArray seqArray = m_captureModuleState->getSequence();
    QJsonObject currentJob = seqArray[currentRow].toObject();
    seqArray.replace(currentRow, seqArray[destinationRow]);
    seqArray.replace(destinationRow, currentJob);
    emit sequenceChanged(seqArray);

    queueTable->selectRow(destinationRow);

    for (int i = 0; i < m_captureModuleState->allJobs().count(); i++)
        m_captureModuleState->allJobs().at(i)->setStatusCell(queueTable->item(i, 0));

    m_captureModuleState->setDirty(true);
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

    int index = m_captureModuleState->allJobs().indexOf(job);
    if (index >= 0)
        queueTable->selectRow(index);

    if (m_captureModuleState->getActiveJob()->getCoreProperty(SequenceJob::SJ_Preview).toBool() == false)
    {
        // set the progress info
        imgProgress->setEnabled(true);
        imgProgress->setMaximum(m_captureModuleState->getActiveJob()->getCoreProperty(SequenceJob::SJ_Count).toInt());
        imgProgress->setValue(m_captureModuleState->getActiveJob()->getCompleted());
    }
}

void Capture::jobExecutionPreparationStarted()
{
    if (m_captureModuleState->getActiveJob() == nullptr)
    {
        // this should never happen
        qWarning(KSTARS_EKOS_CAPTURE) << "jobExecutionPreparationStarted with null m_captureModuleState->getActiveJob().";
        return;
    }
    if (m_captureModuleState->getActiveJob()->getCoreProperty(SequenceJob::SJ_Preview).toBool())
    {
        startB->setIcon(
            QIcon::fromTheme("media-playback-stop"));
        startB->setToolTip(i18n("Stop"));
    }
}

void Capture::updatePrepareState(CaptureState prepareState)
{
    m_captureModuleState->setCaptureState(prepareState);

    if (m_captureModuleState->getActiveJob() == nullptr)
    {
        qWarning(KSTARS_EKOS_CAPTURE) << "updatePrepareState with null m_captureModuleState->getActiveJob().";
        // Everything below depends on m_captureModuleState->getActiveJob(). Just return.
        return;
    }

    switch (prepareState)
    {
        case CAPTURE_SETTING_TEMPERATURE:
            appendLogText(i18n("Setting temperature to %1 °C...", m_captureModuleState->getActiveJob()->getTargetTemperature()));
            captureStatusWidget->setStatus(i18n("Set Temp to %1 °C...", m_captureModuleState->getActiveJob()->getTargetTemperature()),
                                           Qt::yellow);
            break;
        case CAPTURE_GUIDER_DRIFT:
            appendLogText(i18n("Waiting for guide drift below %1\"...",
                               m_captureModuleState->getActiveJob()->getTargetStartGuiderDrift()));
            captureStatusWidget->setStatus(i18n("Wait for Guider < %1\"...",
                                                m_captureModuleState->getActiveJob()->getTargetStartGuiderDrift()), Qt::yellow);
            break;

        case CAPTURE_SETTING_ROTATOR:
            appendLogText(i18n("Setting camera to %1 degrees E of N...", m_captureModuleState->getActiveJob()->getTargetRotation()));
            captureStatusWidget->setStatus(i18n("Set Camera to %1 deg...", m_captureModuleState->getActiveJob()->getTargetRotation()),
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
    m_captureModuleState->getRefocusState()->setFocusTemperatureDelta(focusTemperatureDelta);
}

void Capture::setGuideDeviation(double delta_ra, double delta_dec)
{
    const double deviation_rms = std::hypot(delta_ra, delta_dec);

    // forward it to the state machine
    m_captureModuleState->setGuideDeviation(deviation_rms);

}

void Capture::setFocusStatus(FocusState state)
{
    // directly forward it to the state machine
    m_captureModuleState->updateFocusState(state);
}

void Capture::updateFocusStatus(FocusState state)
{
    if ((m_captureModuleState->getRefocusState()->isRefocusing()
            || m_captureModuleState->getRefocusState()->isInSequenceFocus()) && m_captureModuleState->getActiveJob()
            && m_captureModuleState->getActiveJob()->getStatus() == JOB_BUSY)
    {
        switch (state)
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

void Capture::focusAdaptiveComplete(bool success)
{
    // directly forward it to the state machine
    m_captureModuleState->updateAdaptiveFocusState(success);
    if (success)
        appendLogText(i18n("Adaptive focus complete."));
    else
        appendLogText(i18n("Adaptive focus failed. Continuing..."));
}

void Capture::updateMeridianFlipStage(MeridianFlipState::MFStage stage)
{
    // update UI
    if (getMeridianFlipState()->getMeridianFlipStage() != stage)
    {
        switch (stage)
        {
            case MeridianFlipState::MF_READY:
                if (m_captureModuleState->getCaptureState() == CAPTURE_PAUSED)
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
                if (m_captureModuleState->getRefocusState()->isInSequenceFocus())
                    m_LimitsUI->limitFocusHFRN->setValue(m_captureModuleState->getFileHFR());
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

void Capture::setTargetName(const QString &name)
{
    if (m_captureModuleState->isCaptureRunning() == false)
    {
        m_captureModuleState->setTargetName(name);
        targetNameT->setText(name);
        generatePreviewFilename();
    }
}

void Capture::setObserverName(const QString &value)
{
    m_captureModuleState->setObserverName(value);
    Options::setDefaultObserver(value);
}

void Capture::syncTelescopeInfo()
{
    if (m_Mount && m_Camera && m_Mount->isConnected())
    {
        // Camera to current telescope
        auto activeDevices = m_Camera->getText("ACTIVE_DEVICES");
        if (activeDevices)
        {
            auto activeTelescope = activeDevices->findWidgetByName("ACTIVE_TELESCOPE");
            if (activeTelescope)
            {
                activeTelescope->setText(m_captureDeviceAdaptor->getMount()->getDeviceName().toLatin1().constData());
                m_Camera->sendNewProperty(activeDevices);
            }
        }
    }
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

bool Capture::loadSequenceQueue(const QString &fileURL, bool ignoreTarget)
{
    QFile sFile(fileURL);
    if (!sFile.open(QIODevice::ReadOnly))
    {
        QString message = i18n("Unable to open file %1", fileURL);
        KSNotification::sorry(message, i18n("Could Not Open File"));
        return false;
    }

    m_captureModuleState->clearCapturedFramesMap();
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
                    setObserverName(QString(pcdataXMLEle(ep)));
                }
                else if (!strcmp(tagXMLEle(ep), "GuideDeviation"))
                {
                    m_LimitsUI->limitGuideDeviationS->setChecked(!strcmp(findXMLAttValu(ep, "enabled"), "true"));
                    m_LimitsUI->limitGuideDeviationN->setValue(cLocale.toDouble(pcdataXMLEle(ep)));
                }
                else if (!strcmp(tagXMLEle(ep), "CCD"))
                {
                    // Old field in some files. Without this empty test, it would fall through to the else condition and create a job.
                }
                else if (!strcmp(tagXMLEle(ep), "FilterWheel"))
                {
                    // Old field in some files. Without this empty test, it would fall through to the else condition and create a job.
                }
                else if (!strcmp(tagXMLEle(ep), "GuideStartDeviation"))
                {
                    m_LimitsUI->startGuiderDriftS->setChecked(!strcmp(findXMLAttValu(ep, "enabled"), "true"));
                    m_LimitsUI->startGuiderDriftN->setValue(cLocale.toDouble(pcdataXMLEle(ep)));
                }
                else if (!strcmp(tagXMLEle(ep), "Autofocus"))
                {
                    m_LimitsUI->limitFocusHFRS->setChecked(!strcmp(findXMLAttValu(ep, "enabled"), "true"));
                    double const HFRValue = cLocale.toDouble(pcdataXMLEle(ep));
                    // Set the HFR value from XML, or reset it to zero, don't let another unrelated older HFR be used
                    // Note that HFR value will only be serialized to XML when option "Save Sequence HFR to File" is enabled
                    m_captureModuleState->setFileHFR(HFRValue > 0.0 ? HFRValue : 0.0);
                    m_LimitsUI->limitFocusHFRN->setValue(m_captureModuleState->getFileHFR());
                }
                else if (!strcmp(tagXMLEle(ep), "RefocusOnTemperatureDelta"))
                {
                    m_LimitsUI->limitFocusDeltaTS->setChecked(!strcmp(findXMLAttValu(ep, "enabled"), "true"));
                    double const deltaValue = cLocale.toDouble(pcdataXMLEle(ep));
                    m_LimitsUI->limitFocusDeltaTN->setValue(deltaValue);
                }
                else if (!strcmp(tagXMLEle(ep), "RefocusEveryN"))
                {
                    m_LimitsUI->limitRefocusS->setChecked(!strcmp(findXMLAttValu(ep, "enabled"), "true"));
                    int const minutesValue = cLocale.toInt(pcdataXMLEle(ep));
                    // Set the refocus period from XML, or reset it to zero, don't let another unrelated older refocus period be used.
                    m_LimitsUI->limitRefocusN->setValue(minutesValue > 0 ? minutesValue : 0);
                }
                else if (!strcmp(tagXMLEle(ep), "RefocusOnMeridianFlip"))
                {
                    m_LimitsUI->meridianRefocusS->setChecked(!strcmp(findXMLAttValu(ep, "enabled"), "true"));
                }
                else if (!strcmp(tagXMLEle(ep), "MeridianFlip"))
                {
                    // meridian flip is managed by the mount only
                    // older files might nevertheless contain MF settings
                    if (! strcmp(findXMLAttValu(ep, "enabled"), "true"))
                        appendLogText(
                            i18n("Meridian flip configuration has been shifted to the mount module. Please configure the meridian flip there."));
                }
                else
                {
                    processJobInfo(ep, ignoreTarget);
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

    m_captureModuleState->setSequenceURL(QUrl::fromLocalFile(fileURL));
    m_captureModuleState->setDirty(false);
    delLilXML(xmlParser);
    // update save button tool tip
    queueSaveB->setToolTip("Save to " + sFile.fileName());

    syncRefocusOptionsFromGUI();
    return true;
}

bool Capture::processJobInfo(XMLEle * root, bool ignoreTarget)
{
    XMLEle * ep;
    XMLEle * subEP;
    if (m_RotatorControlPanel)
        m_RotatorControlPanel->setRotationEnforced(false);

    bool isDarkFlat = false;
    m_Scripts.clear();
    QLocale cLocale = QLocale::c();
    bool foundPlaceholderFormat = false;

    for (ep = nextXMLEle(root, 1); ep != nullptr; ep = nextXMLEle(root, 0))
    {
        if (!strcmp(tagXMLEle(ep), "Exposure"))
            captureExposureN->setValue(cLocale.toDouble(pcdataXMLEle(ep)));
        else if (!strcmp(tagXMLEle(ep), "Format"))
            captureFormatS->setCurrentText(pcdataXMLEle(ep));
        else if (!strcmp(tagXMLEle(ep), "Encoding"))
        {
            captureEncodingS->setCurrentText(pcdataXMLEle(ep));
        }
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
            //FilterPosCombo->setCurrentIndex(atoi(pcdataXMLEle(ep))-1);
            if (FilterPosCombo->findText(pcdataXMLEle(ep)) == -1)
            {
                appendLogText(i18n("Warning: Filter %1 not found in filter wheel.", pcdataXMLEle(ep)));
                qWarning(KSTARS_EKOS_CAPTURE) << QString("Filter  %1 not found in filter wheel.").arg(pcdataXMLEle(ep));
            }
            FilterPosCombo->setCurrentText(pcdataXMLEle(ep));
        }
        else if (!strcmp(tagXMLEle(ep), "Type"))
        {
            captureTypeS->setCurrentText(pcdataXMLEle(ep));
        }
        else if (!strcmp(tagXMLEle(ep), "Prefix"))
        {
            // RawPrefix is outdated and will be ignored
            subEP = findXMLEle(ep, "RawPrefix");
            if (subEP && ignoreTarget == false)
            {
                if (strcmp(pcdataXMLEle(subEP), "") != 0)
                    qWarning(KSTARS_EKOS_CAPTURE) << QString("Sequence job raw prefix %1 ignored.").arg(pcdataXMLEle(subEP));
            }
            subEP = findXMLEle(ep, "FilterEnabled");
            if (subEP)
                FilterEnabled = !strcmp("1", pcdataXMLEle(subEP));
            subEP = findXMLEle(ep, "ExpEnabled");
            if (subEP)
                ExpEnabled = !strcmp("1", pcdataXMLEle(subEP));
            subEP = findXMLEle(ep, "TimeStampEnabled");
            if (subEP)
                TimeStampEnabled = !strcmp("1", pcdataXMLEle(subEP));
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
            m_Scripts[SCRIPT_POST_CAPTURE] = pcdataXMLEle(ep);
        }
        else if (!strcmp(tagXMLEle(ep), "PreCaptureScript"))
        {
            m_Scripts[SCRIPT_PRE_CAPTURE] = pcdataXMLEle(ep);
        }
        else if (!strcmp(tagXMLEle(ep), "PostJobScript"))
        {
            m_Scripts[SCRIPT_POST_JOB] = pcdataXMLEle(ep);
        }
        else if (!strcmp(tagXMLEle(ep), "PreJobScript"))
        {
            m_Scripts[SCRIPT_PRE_JOB] = pcdataXMLEle(ep);
        }
        else if (!strcmp(tagXMLEle(ep), "FITSDirectory"))
        {
            fileDirectoryT->setText(pcdataXMLEle(ep));
        }
        else if (!strcmp(tagXMLEle(ep), "PlaceholderFormat"))
        {
            placeholderFormatT->setText(pcdataXMLEle(ep));
            foundPlaceholderFormat = true;
        }
        else if (!strcmp(tagXMLEle(ep), "PlaceholderSuffix"))
        {
            formatSuffixN->setValue(cLocale.toUInt(pcdataXMLEle(ep)));
            foundPlaceholderFormat = true;
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
        else if (!strcmp(tagXMLEle(ep), "Rotation") && m_RotatorControlPanel)
        {
            m_RotatorControlPanel->setRotationEnforced(true);
            m_RotatorControlPanel->setCameraPA(cLocale.toDouble(pcdataXMLEle(ep)));
        }
        else if (!strcmp(tagXMLEle(ep), "Properties"))
        {
            QMap<QString, QMap<QString, QVariant>> propertyMap;

            for (subEP = nextXMLEle(ep, 1); subEP != nullptr; subEP = nextXMLEle(ep, 0))
            {
                QMap<QString, QVariant> elements;
                XMLEle * oneElement = nullptr;
                for (oneElement = nextXMLEle(subEP, 1); oneElement != nullptr; oneElement = nextXMLEle(subEP, 0))
                {
                    const char * name = findXMLAttValu(oneElement, "name");
                    bool ok = false;
                    // String
                    auto xmlValue = pcdataXMLEle(oneElement);
                    // Try to load it as double
                    auto value = cLocale.toDouble(xmlValue, &ok);
                    if (ok)
                        elements[name] = value;
                    else
                        elements[name] = xmlValue;
                }

                const char * name = findXMLAttValu(subEP, "name");
                propertyMap[name] = elements;
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
                const char * dark = findXMLAttValu(subEP, "dark");
                isDarkFlat = !strcmp(dark, "true");

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
                    m_captureModuleState->setTargetADUTolerance(cLocale.toDouble(pcdataXMLEle(aduEP)));
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

    if (!foundPlaceholderFormat)
        placeholderFormatT->setText(PlaceholderPath::defaultFormat(FilterEnabled, ExpEnabled, TimeStampEnabled));

    addJob(false, isDarkFlat);

    return true;
}

void Capture::saveSequenceQueue()
{
    QUrl backupCurrent = m_captureModuleState->sequenceURL();

    if (m_captureModuleState->sequenceURL().toLocalFile().startsWith(QLatin1String("/tmp/"))
            || m_captureModuleState->sequenceURL().toLocalFile().contains("/Temp"))
        m_captureModuleState->setSequenceURL(QUrl(""));

    // If no changes made, return.
    if (m_captureModuleState->dirty() == false && !m_captureModuleState->sequenceURL().isEmpty())
        return;

    if (m_captureModuleState->sequenceURL().isEmpty())
    {
        m_captureModuleState->setSequenceURL(QFileDialog::getSaveFileUrl(Manager::Instance(), i18nc("@title:window",
                                             "Save Ekos Sequence Queue"),
                                             dirPath,
                                             "Ekos Sequence Queue (*.esq)"));
        // if user presses cancel
        if (m_captureModuleState->sequenceURL().isEmpty())
        {
            m_captureModuleState->setSequenceURL(backupCurrent);
            return;
        }

        dirPath = QUrl(m_captureModuleState->sequenceURL().url(QUrl::RemoveFilename));

        if (m_captureModuleState->sequenceURL().toLocalFile().endsWith(QLatin1String(".esq")) == false)
            m_captureModuleState->setSequenceURL(QUrl("file:" + m_captureModuleState->sequenceURL().toLocalFile() + ".esq"));

    }

    if (m_captureModuleState->sequenceURL().isValid())
    {
        if ((saveSequenceQueue(m_captureModuleState->sequenceURL().toLocalFile())) == false)
        {
            KSNotification::error(i18n("Failed to save sequence queue"), i18n("Save"));
            return;
        }

        m_captureModuleState->setDirty(false);
    }
    else
    {
        QString message = i18n("Invalid URL: %1", m_captureModuleState->sequenceURL().url());
        KSNotification::sorry(message, i18n("Invalid URL"));
    }
}

void Capture::saveSequenceQueueAs()
{
    m_captureModuleState->setSequenceURL(QUrl(""));
    saveSequenceQueue();
}

bool Capture::saveSequenceQueue(const QString &path)
{
    QFile file;
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

    outstream << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << Qt::endl;
    outstream << "<SequenceQueue version='" << SQ_FORMAT_VERSION << "'>" << Qt::endl;
    if (getObserverName().isEmpty() == false)
        outstream << "<Observer>" << getObserverName() << "</Observer>" << Qt::endl;
    outstream << "<GuideDeviation enabled='" << (m_LimitsUI->limitGuideDeviationS->isChecked() ? "true" : "false") << "'>"
              << cLocale.toString(m_LimitsUI->limitGuideDeviationN->value()) << "</GuideDeviation>" << Qt::endl;
    outstream << "<GuideStartDeviation enabled='" << (m_LimitsUI->startGuiderDriftS->isChecked() ? "true" : "false") << "'>"
              << cLocale.toString(m_LimitsUI->startGuiderDriftN->value()) << "</GuideStartDeviation>" << Qt::endl;
    // Issue a warning when autofocus is enabled but Ekos options prevent HFR value from being written
    if (m_LimitsUI->limitFocusHFRS->isChecked() && !Options::saveHFRToFile())
        appendLogText(i18n(
                          "Warning: HFR-based autofocus is set but option \"Save Sequence HFR Value to File\" is not enabled. "
                          "Current HFR value will not be written to sequence file."));
    outstream << "<Autofocus enabled='" << (m_LimitsUI->limitFocusHFRS->isChecked() ? "true" : "false") << "'>"
              << cLocale.toString(Options::saveHFRToFile() ? m_LimitsUI->limitFocusHFRN->value() : 0) << "</Autofocus>" << Qt::endl;
    outstream << "<RefocusOnTemperatureDelta enabled='" << (m_LimitsUI->limitFocusDeltaTS->isChecked() ? "true" : "false") <<
              "'>"
              << cLocale.toString(m_LimitsUI->limitFocusDeltaTN->value()) << "</RefocusOnTemperatureDelta>" << Qt::endl;
    outstream << "<RefocusEveryN enabled='" << (m_LimitsUI->limitRefocusS->isChecked() ? "true" : "false") << "'>"
              << cLocale.toString(m_LimitsUI->limitRefocusN->value()) << "</RefocusEveryN>" << Qt::endl;
    outstream << "<RefocusOnMeridianFlip enabled='" << (m_LimitsUI->meridianRefocusS->isChecked() ? "true" : "false") << "'/>"
              << Qt::endl;
    for (auto &job : m_captureModuleState->allJobs())
    {
        auto filterEnabled = job->getCoreProperty(SequenceJob::SJ_FilterPrefixEnabled).toBool();
        auto expEnabled = job->getCoreProperty(SequenceJob::SJ_ExpPrefixEnabled).toBool();
        auto tsEnabled = job->getCoreProperty(SequenceJob::SJ_TimeStampPrefixEnabled).toBool();
        auto roi = job->getCoreProperty(SequenceJob::SJ_ROI).toRect();

        outstream << "<Job>" << Qt::endl;

        outstream << "<Exposure>" << cLocale.toString(job->getCoreProperty(SequenceJob::SJ_Exposure).toDouble()) << "</Exposure>" <<
                  Qt::endl;
        outstream << "<Format>" << job->getCoreProperty(SequenceJob::SJ_Format).toString() << "</Format>" << Qt::endl;
        outstream << "<Encoding>" << job->getCoreProperty(SequenceJob::SJ_Encoding).toString() << "</Encoding>" << Qt::endl;
        outstream << "<Binning>" << Qt::endl;
        outstream << "<X>" << cLocale.toString(job->getCoreProperty(SequenceJob::SJ_Binning).toPoint().x()) << "</X>" << Qt::endl;
        outstream << "<Y>" << cLocale.toString(job->getCoreProperty(SequenceJob::SJ_Binning).toPoint().x()) << "</Y>" << Qt::endl;
        outstream << "</Binning>" << Qt::endl;
        outstream << "<Frame>" << Qt::endl;
        outstream << "<X>" << cLocale.toString(roi.x()) << "</X>" << Qt::endl;
        outstream << "<Y>" << cLocale.toString(roi.y()) << "</Y>" << Qt::endl;
        outstream << "<W>" << cLocale.toString(roi.width()) << "</W>" << Qt::endl;
        outstream << "<H>" << cLocale.toString(roi.height()) << "</H>" << Qt::endl;
        outstream << "</Frame>" << Qt::endl;
        if (job->getTargetTemperature() != Ekos::INVALID_VALUE)
            outstream << "<Temperature force='" << (job->getCoreProperty(SequenceJob::SJ_EnforceTemperature).toBool() ? "true" :
                                                    "false") << "'>"
                      << cLocale.toString(job->getTargetTemperature()) << "</Temperature>" << Qt::endl;
        if (job->getTargetFilter() >= 0)
            outstream << "<Filter>" << job->getCoreProperty(SequenceJob::SJ_Filter).toString() << "</Filter>" << Qt::endl;
        outstream << "<Type>" << frameTypes.key(job->getFrameType()) << "</Type>" << Qt::endl;
        outstream << "<Prefix>" << Qt::endl;
        outstream << "<FilterEnabled>" << (filterEnabled ? 1 : 0) << "</FilterEnabled>" << Qt::endl;
        outstream << "<ExpEnabled>" << (expEnabled ? 1 : 0) << "</ExpEnabled>" << Qt::endl;
        outstream << "<TimeStampEnabled>" << (tsEnabled ? 1 : 0) << "</TimeStampEnabled>" << Qt::endl;
        outstream << "</Prefix>" << Qt::endl;
        outstream << "<Count>" << cLocale.toString(job->getCoreProperty(SequenceJob::SJ_Count).toInt()) << "</Count>" << Qt::endl;
        // ms to seconds
        outstream << "<Delay>" << cLocale.toString(job->getCoreProperty(SequenceJob::SJ_Delay).toInt() / 1000.0) << "</Delay>" <<
                  Qt::endl;
        if (job->getScript(SCRIPT_PRE_CAPTURE).isEmpty() == false)
            outstream << "<PreCaptureScript>" << job->getScript(SCRIPT_PRE_CAPTURE) << "</PreCaptureScript>" << Qt::endl;
        if (job->getScript(SCRIPT_POST_CAPTURE).isEmpty() == false)
            outstream << "<PostCaptureScript>" << job->getScript(SCRIPT_POST_CAPTURE) << "</PostCaptureScript>" << Qt::endl;
        if (job->getScript(SCRIPT_PRE_JOB).isEmpty() == false)
            outstream << "<PreJobScript>" << job->getScript(SCRIPT_PRE_JOB) << "</PreJobScript>" << Qt::endl;
        if (job->getScript(SCRIPT_POST_JOB).isEmpty() == false)
            outstream << "<PostJobScript>" << job->getScript(SCRIPT_POST_JOB) << "</PostJobScript>" << Qt::endl;
        outstream << "<FITSDirectory>" << job->getCoreProperty(SequenceJob::SJ_LocalDirectory).toString() << "</FITSDirectory>" <<
                  Qt::endl;
        outstream << "<PlaceholderFormat>" << job->getCoreProperty(SequenceJob::SJ_PlaceholderFormat).toString() <<
                  "</PlaceholderFormat>" <<
                  Qt::endl;
        outstream << "<PlaceholderSuffix>" << job->getCoreProperty(SequenceJob::SJ_PlaceholderSuffix).toUInt() <<
                  "</PlaceholderSuffix>" <<
                  Qt::endl;
        outstream << "<UploadMode>" << job->getUploadMode() << "</UploadMode>" << Qt::endl;
        if (job->getCoreProperty(SequenceJob::SJ_RemoteDirectory).toString().isEmpty() == false)
            outstream << "<RemoteDirectory>" << job->getCoreProperty(SequenceJob::SJ_RemoteDirectory).toString() << "</RemoteDirectory>"
                      << Qt::endl;
        if (job->getCoreProperty(SequenceJob::SJ_ISOIndex).toInt() != -1)
            outstream << "<ISOIndex>" << (job->getCoreProperty(SequenceJob::SJ_ISOIndex).toInt()) << "</ISOIndex>" << Qt::endl;
        if (job->getTargetRotation() != Ekos::INVALID_VALUE)
            outstream << "<Rotation>" << (job->getTargetRotation()) << "</Rotation>" << Qt::endl;
        QMapIterator<QString, QMap<QString, QVariant>> customIter(job->getCustomProperties());
        outstream << "<Properties>" << Qt::endl;
        while (customIter.hasNext())
        {
            customIter.next();
            outstream << "<PropertyVector name='" << customIter.key() << "'>" << Qt::endl;
            QMap<QString, QVariant> elements = customIter.value();
            QMapIterator<QString, QVariant> iter(elements);
            while (iter.hasNext())
            {
                iter.next();
                if (iter.value().type() == QVariant::String)
                {
                    outstream << "<OneElement name='" << iter.key()
                              << "'>" << iter.value().toString() << "</OneElement>" << Qt::endl;
                }
                else
                {
                    outstream << "<OneElement name='" << iter.key()
                              << "'>" << iter.value().toDouble() << "</OneElement>" << Qt::endl;
                }
            }
            outstream << "</PropertyVector>" << Qt::endl;
        }
        outstream << "</Properties>" << Qt::endl;

        outstream << "<Calibration>" << Qt::endl;
        outstream << "<FlatSource>" << Qt::endl;
        if (job->getFlatFieldSource() == SOURCE_MANUAL)
            outstream << "<Type>Manual</Type>" << Qt::endl;
        else if (job->getFlatFieldSource() == SOURCE_FLATCAP)
            outstream << "<Type>FlatCap</Type>" << Qt::endl;
        else if (job->getFlatFieldSource() == SOURCE_DARKCAP)
            outstream << "<Type>DarkCap</Type>" << Qt::endl;
        else if (job->getFlatFieldSource() == SOURCE_WALL)
        {
            outstream << "<Type>Wall</Type>" << Qt::endl;
            outstream << "<Az>" << cLocale.toString(job->getWallCoord().az().Degrees()) << "</Az>" << Qt::endl;
            outstream << "<Alt>" << cLocale.toString(job->getWallCoord().alt().Degrees()) << "</Alt>" << Qt::endl;
        }
        else
            outstream << "<Type>DawnDust</Type>" << Qt::endl;
        outstream << "</FlatSource>" << Qt::endl;

        outstream << "<FlatDuration dark='" << (job->getCoreProperty(SequenceJob::SJ_DarkFlat).toBool() ? "true" : "false")
                  << "'>" << Qt::endl;
        if (job->getFlatFieldDuration() == DURATION_MANUAL)
            outstream << "<Type>Manual</Type>" << Qt::endl;
        else
        {
            outstream << "<Type>ADU</Type>" << Qt::endl;
            outstream << "<Value>" << cLocale.toString(job->getCoreProperty(SequenceJob::SJ_TargetADU).toDouble()) << "</Value>" <<
                      Qt::endl;
            outstream << "<Tolerance>" << cLocale.toString(job->getCoreProperty(SequenceJob::SJ_TargetADUTolerance).toDouble()) <<
                      "</Tolerance>" << Qt::endl;
        }
        outstream << "</FlatDuration>" << Qt::endl;

        outstream << "<PreMountPark>" << (job->getPreMountPark() ? "True" : "False") <<
                  "</PreMountPark>" << Qt::endl;
        outstream << "<PreDomePark>" << (job->getPreDomePark() ? "True" : "False") <<
                  "</PreDomePark>" << Qt::endl;
        outstream << "</Calibration>" << Qt::endl;

        outstream << "</Job>" << Qt::endl;
    }

    outstream << "</SequenceQueue>" << Qt::endl;

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
        SequenceJob * job = m_captureModuleState->allJobs().at(queueTable->currentRow());
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

        foreach (SequenceJob * job, m_captureModuleState->allJobs())
            job->resetStatus();
    }

    // Also reset the storage count for all jobs
    m_captureModuleState->clearCapturedFramesMap();

    // We're not controlled by the Scheduler, restore progress option
    m_captureModuleState->setIgnoreJobProgress(Options::alwaysResetSequenceWhenStarting());
}

void Capture::ignoreSequenceHistory()
{
    // This function is called independently from the Scheduler or the UI, so honor the change
    m_captureModuleState->setIgnoreJobProgress(true);
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
    flatFieldDuration  = job->getFlatFieldDuration();
    flatFieldSource    = job->getFlatFieldSource();
    targetADU          = job->getCoreProperty(SequenceJob::SJ_TargetADU).toDouble();
    m_captureModuleState->setTargetADUTolerance(job->getCoreProperty(SequenceJob::SJ_TargetADUTolerance).toDouble());
    wallCoord          = job->getWallCoord();
    preMountPark       = job->getPreMountPark();
    preDomePark        = job->getPreDomePark();

    // Script options
    m_Scripts          = job->getScripts();

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

    if (m_RotatorControlPanel) // only if rotator is registered
    {
        if (job->getTargetRotation() != Ekos::INVALID_VALUE)
        {
            m_RotatorControlPanel->setRotationEnforced(true);
            m_RotatorControlPanel->setCameraPA(job->getTargetRotation());
        }
        else
            m_RotatorControlPanel->setRotationEnforced(false);
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

QJsonObject Capture::getPresetSettings()
{
    QJsonObject settings;

    // Try to get settings value
    // if not found, fallback to camera value
    double gain = -1;
    if (GainSpinSpecialValue > INVALID_VALUE && captureGainN->value() > GainSpinSpecialValue)
        gain = captureGainN->value();
    else if (m_captureDeviceAdaptor->getActiveCamera() && m_captureDeviceAdaptor->getActiveCamera()->hasGain())
        m_captureDeviceAdaptor->getActiveCamera()->getGain(&gain);

    double offset = -1;
    if (OffsetSpinSpecialValue > INVALID_VALUE && captureOffsetN->value() > OffsetSpinSpecialValue)
        offset = captureOffsetN->value();
    else if (m_captureDeviceAdaptor->getActiveCamera() && m_captureDeviceAdaptor->getActiveCamera()->hasOffset())
        m_captureDeviceAdaptor->getActiveCamera()->getOffset(&offset);

    int iso = -1;
    if (captureISOS)
        iso = captureISOS->currentIndex();
    else if (m_captureDeviceAdaptor->getActiveCamera())
        iso = m_captureDeviceAdaptor->getActiveCamera()->getChip(ISD::CameraChip::PRIMARY_CCD)->getISOIndex();

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
    if (i.row() < 0 || (i.row() + 1) > m_captureModuleState->allJobs().size())
        return false;

    SequenceJob * job = m_captureModuleState->allJobs().at(i.row());

    if (job == nullptr || job->getCoreProperty(SequenceJob::SJ_DarkFlat).toBool())
        return false;

    syncGUIToJob(job);

    if (m_captureModuleState->isBusy())
        return false;

    if (m_captureModuleState->allJobs().size() >= 2)
    {
        queueUpB->setEnabled(i.row() > 0);
        queueDownB->setEnabled(i.row() + 1 < m_captureModuleState->allJobs().size());
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

void Capture::resetJobEdit()
{
    if (m_JobUnderEdit)
        appendLogText(i18n("Editing job canceled."));

    m_JobUnderEdit = false;
    addToQueueB->setIcon(QIcon::fromTheme("list-add"));

    addToQueueB->setToolTip(i18n("Add job to sequence queue"));
    removeFromQueueB->setToolTip(i18n("Remove job from sequence queue"));

    addToQueueB->setDefault(false);
    previewB->setDefault(true);
}

double Capture::getProgressPercentage()
{
    int totalImageCount     = 0;
    int totalImageCompleted = 0;

    foreach (SequenceJob * job, m_captureModuleState->allJobs())
    {
        totalImageCount += job->getCoreProperty(SequenceJob::SJ_Count).toInt();
        totalImageCompleted += job->getCompleted();
    }

    if (totalImageCount != 0)
        return ((static_cast<double>(totalImageCompleted) / totalImageCount) * 100.0);
    else
        return -1;
}

int Capture::getActiveJobID()
{
    if (m_captureModuleState->getActiveJob() == nullptr)
        return -1;

    for (int i = 0; i < m_captureModuleState->allJobs().count(); i++)
    {
        if (m_captureModuleState->getActiveJob() == m_captureModuleState->allJobs().at(i))
            return i;
    }

    return -1;
}

int Capture::getPendingJobCount()
{
    int completedJobs = 0;

    foreach (SequenceJob * job, m_captureModuleState->allJobs())
    {
        if (job->getStatus() == JOB_DONE)
            completedJobs++;
    }

    return (m_captureModuleState->allJobs().count() - completedJobs);
}

QString Capture::getJobState(int id)
{
    if (id < m_captureModuleState->allJobs().count())
    {
        SequenceJob * job = m_captureModuleState->allJobs().at(id);
        return job->getStatusString();
    }

    return QString();
}

QString Capture::getJobFilterName(int id)
{
    if (id < m_captureModuleState->allJobs().count())
    {
        SequenceJob * job = m_captureModuleState->allJobs().at(id);
        return job->getCoreProperty(SequenceJob::SJ_Filter).toString();
    }

    return QString();
}

int Capture::getJobImageProgress(int id)
{
    if (id < m_captureModuleState->allJobs().count())
    {
        SequenceJob * job = m_captureModuleState->allJobs().at(id);
        return job->getCompleted();
    }

    return -1;
}

int Capture::getJobImageCount(int id)
{
    if (id < m_captureModuleState->allJobs().count())
    {
        SequenceJob * job = m_captureModuleState->allJobs().at(id);
        return job->getCoreProperty(SequenceJob::SJ_Count).toInt();
    }

    return -1;
}

double Capture::getJobExposureProgress(int id)
{
    if (id < m_captureModuleState->allJobs().count())
    {
        SequenceJob * job = m_captureModuleState->allJobs().at(id);
        return job->getExposeLeft();
    }

    return -1;
}

double Capture::getJobExposureDuration(int id)
{
    if (id < m_captureModuleState->allJobs().count())
    {
        SequenceJob * job = m_captureModuleState->allJobs().at(id);
        return job->getCoreProperty(SequenceJob::SJ_Exposure).toDouble();
    }

    return -1;
}

CCDFrameType Capture::getJobFrameType(int id)
{
    if (id < m_captureModuleState->allJobs().count())
    {
        SequenceJob * job = m_captureModuleState->allJobs().at(id);
        return job->getFrameType();
    }

    return FRAME_NONE;
}

int Capture::getOverallRemainingTime()
{
    int remaining = 0;
    double estimatedDownloadTime = m_captureModuleState->averageDownloadTime();

    foreach (SequenceJob * job, m_captureModuleState->allJobs())
        remaining += job->getJobRemainingTime(estimatedDownloadTime);

    return remaining;
}

int Capture::getActiveJobRemainingTime()
{
    if (m_captureModuleState->getActiveJob() == nullptr)
        return -1;

    return m_captureModuleState->getActiveJob()->getJobRemainingTime(m_captureModuleState->averageDownloadTime());
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
    m_captureModuleState->setActiveJob(nullptr);
    while (queueTable->rowCount() > 0)
        queueTable->removeRow(0);
    qDeleteAll(m_captureModuleState->allJobs());
    m_captureModuleState->allJobs().clear();

    while (m_captureModuleState->getSequence().count())
        m_captureModuleState->getSequence().pop_back();
    emit sequenceChanged(m_captureModuleState->getSequence());
}

QString Capture::getSequenceQueueStatus()
{
    if (m_captureModuleState->allJobs().count() == 0)
        return "Invalid";

    if (m_captureModuleState->isBusy())
        return "Running";

    int idle = 0, error = 0, complete = 0, aborted = 0, running = 0;

    foreach (SequenceJob * job, m_captureModuleState->allJobs())
    {
        switch (job->getStatus())
        {
            case JOB_ABORTED:
                aborted++;
                break;
            case JOB_BUSY:
                running++;
                break;
            case JOB_DONE:
                complete++;
                break;
            case JOB_ERROR:
                error++;
                break;
            case JOB_IDLE:
                idle++;
                break;
        }
    }

    if (error > 0)
        return "Error";

    if (aborted > 0)
    {
        if (m_captureModuleState->getCaptureState() == CAPTURE_SUSPENDED)
            return "Suspended";
        else
            return "Aborted";
    }

    if (running > 0)
        return "Running";

    if (idle == m_captureModuleState->allJobs().count())
        return "Idle";

    if (complete == m_captureModuleState->allJobs().count())
        return "Complete";

    return "Invalid";
}

void Capture::checkGuideDeviationTimeout()
{
    if (m_captureModuleState->getActiveJob() && m_captureModuleState->getActiveJob()->getStatus() == JOB_ABORTED
            && m_captureModuleState->isGuidingDeviationDetected())
    {
        appendLogText(i18n("Guide module timed out."));
        m_captureModuleState->setGuidingDeviationDetected(false);

        // If capture was suspended, it should be aborted (failed) now.
        if (m_captureModuleState->getCaptureState() == CAPTURE_SUSPENDED)
        {
            m_captureModuleState->setCaptureState(CAPTURE_ABORTED);
        }
    }
}

void Capture::setAlignStatus(AlignState state)
{
    // forward it directly to the state machine
    m_captureModuleState->setAlignState(state);
}

void Capture::setGuideStatus(GuideState state)
{
    // forward it directly to the state machine
    m_captureModuleState->setGuideState(state);
}


void Capture::checkFrameType(int index)
{
    calibrationB->setEnabled(index != FRAME_LIGHT);
    generateDarkFlatsB->setEnabled(index != FRAME_LIGHT);
}


bool Capture::hasCoolerControl()
{
    if (m_captureDeviceAdaptor->getActiveCamera() && m_captureDeviceAdaptor->getActiveCamera()->hasCoolerControl())
        return true;

    return false;
}

bool Capture::setCoolerControl(bool enable)
{
    if (m_captureDeviceAdaptor->getActiveCamera() && m_captureDeviceAdaptor->getActiveCamera()->hasCoolerControl())
        return m_captureDeviceAdaptor->getActiveCamera()->setCoolerControl(enable);

    return false;
}

void Capture::clearAutoFocusHFR()
{
    // If HFR limit was set from file, we cannot override it.
    if (m_captureModuleState->getFileHFR() > 0)
        return;

    m_LimitsUI->limitFocusHFRN->setValue(0);
    //firstAutoFocus = true;
}

void Capture::openCalibrationDialog()
{
    QDialog calibrationDialog(this);

    Ui_calibrationOptions calibrationOptions;
    calibrationOptions.setupUi(&calibrationDialog);

    if (m_captureDeviceAdaptor->getMount())
    {
        calibrationOptions.parkMountC->setEnabled(m_captureDeviceAdaptor->getMount()->canPark());
        calibrationOptions.parkMountC->setChecked(preMountPark);
    }
    else
        calibrationOptions.parkMountC->setEnabled(false);

    if (m_captureDeviceAdaptor->getDome())
    {
        calibrationOptions.parkDomeC->setEnabled(m_captureDeviceAdaptor->getDome()->canPark());
        calibrationOptions.parkDomeC->setChecked(preDomePark);
    }
    else
        calibrationOptions.parkDomeC->setEnabled(false);

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
            calibrationOptions.ADUTolerance->setValue(static_cast<int>(std::round(m_captureModuleState->targetADUTolerance())));
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

            wallAz  = calibrationOptions.azBox->createDms(&azOk);
            wallAlt = calibrationOptions.altBox->createDms(&altOk);

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
            m_captureModuleState->setTargetADUTolerance(calibrationOptions.ADUTolerance->value());
        }

        preMountPark = calibrationOptions.parkMountC->isChecked();
        preDomePark  = calibrationOptions.parkDomeC->isChecked();

        m_captureModuleState->setDirty(true);

        Options::setCalibrationFlatSourceIndex(flatFieldSource);
        Options::setCalibrationFlatDurationIndex(flatFieldDuration);
        Options::setCalibrationWallAz(wallCoord.az().Degrees());
        Options::setCalibrationWallAlt(wallCoord.alt().Degrees());
        Options::setCalibrationADUValue(static_cast<uint>(std::round(targetADU)));
        Options::setCalibrationADUValueTolerance(static_cast<uint>(std::round(m_captureModuleState->targetADUTolerance())));
    }
}

void Capture::setNewRemoteFile(QString file)
{
    appendLogText(i18n("Remote image saved to %1", file));
}

void Capture::toggleVideo(bool enabled)
{
    if (m_captureDeviceAdaptor->getActiveCamera() == nullptr)
        return;

    if (m_captureDeviceAdaptor->getActiveCamera()->isBLOBEnabled() == false)
    {
        if (Options::guiderType() != Guide::GUIDE_INTERNAL)
            m_captureDeviceAdaptor->getActiveCamera()->setBLOBEnabled(true);
        else
        {
            connect(KSMessageBox::Instance(), &KSMessageBox::accepted, this, [this, enabled]()
            {
                KSMessageBox::Instance()->disconnect(this);
                m_captureDeviceAdaptor->getActiveCamera()->setBLOBEnabled(true);
                m_captureDeviceAdaptor->getActiveCamera()->setVideoStreamEnabled(enabled);
            });

            KSMessageBox::Instance()->questionYesNo(i18n("Image transfer is disabled for this camera. Would you like to enable it?"),
                                                    i18n("Image Transfer"), 15);

            return;
        }
    }

    m_captureDeviceAdaptor->getActiveCamera()->setVideoStreamEnabled(enabled);
}

bool Capture::setVideoLimits(uint16_t maxBufferSize, uint16_t maxPreviewFPS)
{
    if (m_captureDeviceAdaptor->getActiveCamera() == nullptr)
        return false;

    return m_captureDeviceAdaptor->getActiveCamera()->setStreamLimits(maxBufferSize, maxPreviewFPS);
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
            if (m_captureModuleState->isBusy() == false)
                startB->setEnabled(false);
            break;

        default:
            if (m_captureModuleState->isBusy() == false)
            {
                previewB->setEnabled(true);
                if (m_captureDeviceAdaptor->getActiveCamera())
                    liveVideoB->setEnabled(m_captureDeviceAdaptor->getActiveCamera()->hasVideoStream());
                startB->setEnabled(true);
            }

            break;
    }
}

void Capture::updateMFMountState(MeridianFlipState::MeridianFlipMountState status)
{
    // forward the new state to the state machine
    m_captureModuleState->updateMFMountState(status);
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
    if (m_captureDeviceAdaptor->getRotator() && m_RotatorControlPanel)
        m_RotatorControlPanel->refresh(solverPA);
}

void Capture::setFilterStatus(FilterState filterState)
{
    if (filterState != m_captureModuleState->getFilterManagerState())
        qCDebug(KSTARS_EKOS_CAPTURE) << "Focus State changed from" << Ekos::getFilterStatusString(
                                         m_captureModuleState->getFilterManagerState()) << "to" << Ekos::getFilterStatusString(filterState);
    if (m_captureModuleState->getCaptureState() == CAPTURE_CHANGING_FILTER)
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
                if (m_captureModuleState->getFilterManagerState() == FILTER_CHANGE)
                {
                    appendLogText(i18n("Filter set to %1.",
                                       FilterPosCombo->itemText(m_FilterManager->getTargetFilterPosition() - 1)));
                }
                break;

            default:
                break;
        }
    }
    m_captureModuleState->setFilterManagerState(filterState);
}

void Capture::setupFilterManager()
{
    // Do we have an existing filter manager?
    if (m_FilterManager)
        m_FilterManager->disconnect(this);

    // Create new or refresh device
    Manager::Instance()->createFilterManager(m_FilterWheel);

    // Return global filter manager for this filter wheel.
    Manager::Instance()->getFilterManager(m_FilterWheel->getDeviceName(), m_FilterManager);

    m_captureDeviceAdaptor->setFilterManager(m_FilterManager);

    connect(m_FilterManager.get(), &FilterManager::updated, this, [this]()
    {
        emit filterManagerUpdated(m_FilterWheel);
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
        if (m_captureModuleState->getActiveJob())
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
    auto pos = std::find_if(DSLRInfos.begin(), DSLRInfos.end(), [model](const auto & oneDSLRInfo)
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

void Capture::cullToDSLRLimits()
{
    QString model(m_captureDeviceAdaptor->getActiveCamera()->getDeviceName());

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
    m_captureModuleState->setCapturedFramesCount(signature, static_cast<ushort>(count));
    qCDebug(KSTARS_EKOS_CAPTURE) <<
                                 QString("Client module indicates that storage for '%1' has already %2 captures processed.").arg(signature).arg(count);
    // Scheduler's captured frame map overrides the progress option of the Capture module
    m_captureModuleState->setIgnoreJobProgress(false);
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
    if (temperature > INVALID_VALUE && m_captureDeviceAdaptor->getActiveCamera()
            && m_captureDeviceAdaptor->getActiveCamera()->hasCoolerControl())
    {
        setForceTemperature(true);
        setTargetTemperature(temperature);
    }
    else
        setForceTemperature(false);

    double gain = settings["gain"].toDouble(GainSpinSpecialValue);
    if (m_captureDeviceAdaptor->getActiveCamera() && m_captureDeviceAdaptor->getActiveCamera()->hasGain())
    {
        if (gain == GainSpinSpecialValue)
            captureGainN->setValue(GainSpinSpecialValue);
        else
            setGain(gain);
    }

    double offset = settings["offset"].toDouble(OffsetSpinSpecialValue);
    if (m_captureDeviceAdaptor->getActiveCamera() && m_captureDeviceAdaptor->getActiveCamera()->hasOffset())
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

void Capture::setCalibrationSettings(const QJsonObject &settings)
{
    const int source = settings["source"].toInt(flatFieldSource);
    const int duration = settings["duration"].toInt(flatFieldDuration);
    const double az = settings["az"].toDouble(wallCoord.az().Degrees());
    const double al = settings["al"].toDouble(wallCoord.alt().Degrees());
    const int adu = settings["adu"].toInt(static_cast<int>(std::round(targetADU)));
    const int tolerance = settings["tolerance"].toInt(static_cast<int>(std::round(m_captureModuleState->targetADUTolerance())));
    const bool parkMount = settings["parkMount"].toBool(preMountPark);
    const bool parkDome = settings["parkDome"].toBool(preDomePark);

    flatFieldSource = static_cast<FlatFieldSource>(source);
    flatFieldDuration = static_cast<FlatFieldDuration>(duration);
    wallCoord.setAz(az);
    wallCoord.setAlt(al);
    targetADU = adu;
    m_captureModuleState->setTargetADUTolerance(tolerance);
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
        {"tolerance", m_captureModuleState->targetADUTolerance()},
        {"parkMount", preMountPark},
        {"parkDome", preDomePark},
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
        m_captureDeviceAdaptor->getActiveCamera()->setConfig(PURGE_CONFIG);
        KStarsData::Instance()->userdb()->DeleteDSLRInfo(m_captureDeviceAdaptor->getActiveCamera()->getDeviceName());

        QStringList shutterfulCCDs  = Options::shutterfulCCDs();
        QStringList shutterlessCCDs = Options::shutterlessCCDs();

        // Remove camera from shutterful and shutterless CCDs
        if (shutterfulCCDs.contains(m_captureDeviceAdaptor->getActiveCamera()->getDeviceName()))
        {
            shutterfulCCDs.removeOne(m_captureDeviceAdaptor->getActiveCamera()->getDeviceName());
            Options::setShutterfulCCDs(shutterfulCCDs);
        }
        if (shutterlessCCDs.contains(m_captureDeviceAdaptor->getActiveCamera()->getDeviceName()))
        {
            shutterlessCCDs.removeOne(m_captureDeviceAdaptor->getActiveCamera()->getDeviceName());
            Options::setShutterlessCCDs(shutterlessCCDs);
        }

        // For DSLRs, immediately ask them to enter the values again.
        if (captureISOS && captureISOS->count() > 0)
        {
            createDSLRDialog();
        }
    });

    KSMessageBox::Instance()->questionYesNo( i18n("Reset %1 configuration to default?",
            m_captureDeviceAdaptor->getActiveCamera()->getDeviceName()),
            i18n("Confirmation"), 30);
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
    dslrInfoDialog.reset(new DSLRInfo(this, m_captureDeviceAdaptor->getActiveCamera()));

    connect(dslrInfoDialog.get(), &DSLRInfo::infoChanged, this, [this]()
    {
        if (m_captureDeviceAdaptor->getActiveCamera())
            addDSLRInfo(QString(m_captureDeviceAdaptor->getActiveCamera()->getDeviceName()),
                        dslrInfoDialog->sensorMaxWidth,
                        dslrInfoDialog->sensorMaxHeight,
                        dslrInfoDialog->sensorPixelW,
                        dslrInfoDialog->sensorPixelH);
    });

    dslrInfoDialog->show();

    emit dslrInfoRequested(m_captureDeviceAdaptor->getActiveCamera()->getDeviceName());
}

void Capture::removeDevice(const QSharedPointer<ISD::GenericDevice> &device)
{
    auto name = device->getDeviceName();
    device->disconnect(this);

    // Mounts
    if (m_Mount && m_Mount->getDeviceName() == device->getDeviceName())
    {
        m_Mount->disconnect(this);
        m_Mount = nullptr;
        m_captureDeviceAdaptor->setMount(nullptr);
        if (m_captureModuleState->getActiveJob() != nullptr)
            m_captureModuleState->getActiveJob()->addMount(nullptr);
    }

    // Domes
    if (m_Dome && m_Dome->getDeviceName() == device->getDeviceName())
    {
        m_Dome->disconnect(this);
        m_Dome = nullptr;
        m_captureDeviceAdaptor->setDome(nullptr);
    }

    // Rotators
    if (m_Rotator && m_Rotator->getDeviceName() == device->getDeviceName())
    {
        m_Rotator->disconnect(this);
        m_Rotator = nullptr;
        m_captureDeviceAdaptor->setRotator(nullptr);
    }

    // Dust Caps
    if (m_DustCap && m_DustCap->getDeviceName() == device->getDeviceName())
    {
        m_DustCap->disconnect(this);
        m_DustCap = nullptr;
        m_captureDeviceAdaptor->setDustCap(nullptr);
        m_captureModuleState->hasDustCap = false;
        m_captureModuleState->setDustCapState(CaptureModuleState::CAP_UNKNOWN);
    }

    // Light Boxes
    if (m_LightBox && m_LightBox->getDeviceName() == device->getDeviceName())
    {
        m_LightBox->disconnect(this);
        m_LightBox = nullptr;
        m_captureDeviceAdaptor->setLightBox(nullptr);
        m_captureModuleState->hasLightBox = false;
        m_captureModuleState->setLightBoxLightState(CaptureModuleState::CAP_LIGHT_UNKNOWN);
    }

    // Cameras
    if (m_Camera && m_Camera->getDeviceName() == name)
    {
        m_Camera->disconnect(this);
        m_Camera = nullptr;
        m_captureDeviceAdaptor->setActiveCamera(nullptr);

        QSharedPointer<ISD::GenericDevice> generic;
        if (INDIListener::findDevice(name, generic))
            DarkLibrary::Instance()->removeDevice(generic);

        QTimer::singleShot(1000, this, &Capture::checkCamera);
    }

    // Filter Wheels
    if (m_FilterWheel && m_FilterWheel->getDeviceName() == name)
    {
        m_FilterWheel->disconnect(this);
        m_FilterWheel = nullptr;
        m_captureDeviceAdaptor->setFilterWheel(nullptr);

        QTimer::singleShot(1000, this, &Capture::checkFilter);
    }
}

void Capture::setGain(double value)
{
    if (!m_captureDeviceAdaptor->getActiveCamera())
        return;

    QMap<QString, QMap<QString, QVariant> > customProps = customPropertiesDialog->getCustomProperties();

    // Gain is manifested in two forms
    // Property CCD_GAIN and
    // Part of CCD_CONTROLS properties.
    // Therefore, we have to find what the currently camera supports first.
    if (m_captureDeviceAdaptor->getActiveCamera()->getProperty("CCD_GAIN"))
    {
        QMap<QString, QVariant> ccdGain;
        ccdGain["GAIN"] = value;
        customProps["CCD_GAIN"] = ccdGain;
    }
    else if (m_captureDeviceAdaptor->getActiveCamera()->getProperty("CCD_CONTROLS"))
    {
        QMap<QString, QVariant> ccdGain = customProps["CCD_CONTROLS"];
        ccdGain["Gain"] = value;
        customProps["CCD_CONTROLS"] = ccdGain;
    }

    customPropertiesDialog->setCustomProperties(customProps);
}

double Capture::getGain()
{
    if (!m_captureDeviceAdaptor->getActiveCamera())
        return -1;

    QMap<QString, QMap<QString, QVariant> > customProps = customPropertiesDialog->getCustomProperties();

    // Gain is manifested in two forms
    // Property CCD_GAIN and
    // Part of CCD_CONTROLS properties.
    // Therefore, we have to find what the currently camera supports first.
    if (m_captureDeviceAdaptor->getActiveCamera()->getProperty("CCD_GAIN"))
    {
        return customProps["CCD_GAIN"].value("GAIN", -1).toDouble();
    }
    else if (m_captureDeviceAdaptor->getActiveCamera()->getProperty("CCD_CONTROLS"))
    {
        return customProps["CCD_CONTROLS"].value("Gain", -1).toDouble();
    }

    return -1;
}

void Capture::setOffset(double value)
{
    if (!m_captureDeviceAdaptor->getActiveCamera())
        return;

    QMap<QString, QMap<QString, QVariant> > customProps = customPropertiesDialog->getCustomProperties();

    // Offset is manifested in two forms
    // Property CCD_OFFSET and
    // Part of CCD_CONTROLS properties.
    // Therefore, we have to find what the currently camera supports first.
    if (m_captureDeviceAdaptor->getActiveCamera()->getProperty("CCD_OFFSET"))
    {
        QMap<QString, QVariant> ccdOffset;
        ccdOffset["OFFSET"] = value;
        customProps["CCD_OFFSET"] = ccdOffset;
    }
    else if (m_captureDeviceAdaptor->getActiveCamera()->getProperty("CCD_CONTROLS"))
    {
        QMap<QString, QVariant> ccdOffset = customProps["CCD_CONTROLS"];
        ccdOffset["Offset"] = value;
        customProps["CCD_CONTROLS"] = ccdOffset;
    }

    customPropertiesDialog->setCustomProperties(customProps);
}

double Capture::getOffset()
{
    if (!m_captureDeviceAdaptor->getActiveCamera())
        return -1;

    QMap<QString, QMap<QString, QVariant> > customProps = customPropertiesDialog->getCustomProperties();

    // Gain is manifested in two forms
    // Property CCD_GAIN and
    // Part of CCD_CONTROLS properties.
    // Therefore, we have to find what the currently camera supports first.
    if (m_captureDeviceAdaptor->getActiveCamera()->getProperty("CCD_OFFSET"))
    {
        return customProps["CCD_OFFSET"].value("OFFSET", -1).toDouble();
    }
    else if (m_captureDeviceAdaptor->getActiveCamera()->getProperty("CCD_CONTROLS"))
    {
        return customProps["CCD_CONTROLS"].value("Offset", -1).toDouble();
    }

    return -1;
}

void Capture::reconnectDriver(const QString &camera, const QString &filterWheel)
{
    if (m_Camera && m_Camera->getDeviceName() == camera)
    {
        // Set camera again to the one we restarted
        CaptureState rememberState = m_captureModuleState->getCaptureState();
        m_captureModuleState->setCaptureState(CAPTURE_IDLE);
        checkCamera();
        m_captureModuleState->setCaptureState(rememberState);

        // restart capture
        m_captureModuleState->setCaptureTimeoutCounter(0);

        if (m_captureModuleState->getActiveJob())
        {
            m_captureDeviceAdaptor->setActiveChip(m_captureDeviceAdaptor->getActiveChip());
            m_captureProcess->captureImage();
        }

        return;
    }

    QTimer::singleShot(5000, this, [ &, camera, filterWheel]()
    {
        reconnectDriver(camera, filterWheel);
    });
}

void Capture::syncDSLRToTargetChip(const QString &model)
{
    auto pos = std::find_if(DSLRInfos.begin(), DSLRInfos.end(), [model](const QMap<QString, QVariant> &oneDSLRInfo)
    {
        return (oneDSLRInfo["Model"] == model);
    });

    // Sync Pixel Size
    if (pos != DSLRInfos.end())
    {
        auto camera = *pos;
        m_captureDeviceAdaptor->getActiveChip()->setImageInfo(camera["Width"].toInt(),
                camera["Height"].toInt(),
                camera["PixelW"].toDouble(),
                camera["PixelH"].toDouble(),
                8);
    }
}

void Capture::editFilterName()
{
    if (m_captureDeviceAdaptor->getFilterWheel() == nullptr || m_captureModuleState->getCurrentFilterPosition() < 1)
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

    filterDialog.setWindowTitle(m_captureDeviceAdaptor->getFilterWheel()->getDeviceName());
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

void Capture::handleScriptsManager()
{
    QScopedPointer<ScriptsManager> manager(new ScriptsManager(this));

    manager->setScripts(m_Scripts);

    if (manager->exec() == QDialog::Accepted)
    {
        m_Scripts = manager->getScripts();
    }
}

void Capture::showTemperatureRegulation()
{
    if (!m_captureDeviceAdaptor->getActiveCamera())
        return;

    double currentRamp, currentThreshold;
    if (!m_captureDeviceAdaptor->getActiveCamera()->getTemperatureRegulation(currentRamp, currentThreshold))
        return;


    double rMin, rMax, rStep, tMin, tMax, tStep;

    m_captureDeviceAdaptor->getActiveCamera()->getMinMaxStep("CCD_TEMP_RAMP", "RAMP_SLOPE", &rMin, &rMax, &rStep);
    m_captureDeviceAdaptor->getActiveCamera()->getMinMaxStep("CCD_TEMP_RAMP", "RAMP_THRESHOLD", &tMin, &tMax, &tStep);

    QLabel rampLabel(i18nc("Temperature ramp celcius per minute", "Ramp (C/min):"));
    QDoubleSpinBox rampSpin;
    rampSpin.setMinimum(rMin);
    rampSpin.setMaximum(rMax);
    rampSpin.setSingleStep(rStep);
    rampSpin.setValue(currentRamp);
    rampSpin.setToolTip(i18n("Maximum temperature change per minute when cooling or warming the camera. Set zero to disable."));

    QLabel thresholdLabel(i18n("Threshold:"));
    QDoubleSpinBox thresholdSpin;
    thresholdSpin.setMinimum(tMin);
    thresholdSpin.setMaximum(tMax);
    thresholdSpin.setSingleStep(tStep);
    thresholdSpin.setValue(currentThreshold);
    thresholdSpin.setToolTip(i18n("Maximum difference between camera and target temperatures"));

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
        if (m_captureDeviceAdaptor->getActiveCamera())
            m_captureDeviceAdaptor->getActiveCamera()->setTemperatureRegulation(rampSpin.value(), thresholdSpin.value());
    }
}

void Capture::generateDarkFlats()
{
    const auto existingJobs = m_captureModuleState->allJobs().size();
    uint8_t jobsAdded = 0;

    for (int i = 0; i < existingJobs; i++)
    {
        if (m_captureModuleState->allJobs().at(i)->getFrameType() != FRAME_FLAT)
            continue;

        syncGUIToJob(m_captureModuleState->allJobs().at(i));

        captureTypeS->setCurrentIndex(FRAME_DARK);
        addJob(false, true);
        jobsAdded++;
    }

    if (jobsAdded > 0)
    {
        appendLogText(i18np("One dark flats job was created.", "%1 dark flats jobs were created.", jobsAdded));
    }
}

QSharedPointer<MeridianFlipState> Capture::getMeridianFlipState()
{
    return m_captureModuleState->getMeridianFlipState();
}

void Capture::setMeridianFlipState(QSharedPointer<MeridianFlipState> state)
{
    m_captureModuleState->setMeridianFlipState(state);
    connect(m_captureModuleState->getMeridianFlipState().get(), &MeridianFlipState::newLog, this, &Capture::appendLogText);
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

        auto mount = OpticalTrainManager::Instance()->getMount(name);
        setMount(mount);

        auto camera = OpticalTrainManager::Instance()->getCamera(name);
        if (camera)
        {
            auto scope = OpticalTrainManager::Instance()->getScope(name);
            opticalTrainCombo->setToolTip(QString("%1 @ %2").arg(camera->getDeviceName(), scope["name"].toString()));

            m_FocalLength = scope["focal_length"].toDouble(-1);
            m_Aperture = scope["aperture"].toDouble(-1);
            m_FocalRatio = scope["focal_ratio"].toDouble(-1);
            m_Reducer = OpticalTrainManager::Instance()->getReducer(name);

            // DSLR Lens Aperture
            if (m_Aperture < 0 && m_FocalRatio > 0)
                m_Aperture = m_FocalLength * m_FocalRatio;
        }
        setCamera(camera);

        auto filterWheel = OpticalTrainManager::Instance()->getFilterWheel(name);
        setFilterWheel(filterWheel);

        auto rotator = OpticalTrainManager::Instance()->getRotator(name);
        setRotator(rotator);

        auto dustcap = OpticalTrainManager::Instance()->getDustCap(name);
        setDustCap(dustcap);

        auto lightbox = OpticalTrainManager::Instance()->getLightBox(name);
        setLightBox(lightbox);

    }

    opticalTrainCombo->blockSignals(false);
}

void Capture::generatePreviewFilename()
{
    if (m_captureModuleState->isCaptureRunning() == false)
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
    //    else if (m_captureModuleState->sequenceURL().toLocalFile().isEmpty() && (m_format.contains("%d") || m_format.contains("%p")
    //             || m_format.contains("%f")))
    else if (m_captureModuleState->sequenceURL().toLocalFile().isEmpty() && m_format.contains("%f"))
        previewText = ("Save the sequence file to show filename preview");
    else if (addJob(true, false, previewType) == true)
    {
        QString previewSeq;
        if (m_captureModuleState->sequenceURL().toLocalFile().isEmpty())
        {
            if (m_format.startsWith(separator))
                previewSeq = m_format.left(m_format.lastIndexOf(separator));
        }
        else
            previewSeq = m_captureModuleState->sequenceURL().toLocalFile();
        auto m_placeholderPath = PlaceholderPath(previewSeq);
        auto m_job = m_captureModuleState->allJobs().last();

        QString extension;
        if (captureEncodingS->currentText() == "FITS")
            extension = ".fits";
        else if (captureEncodingS->currentText() == "XISF")
            extension = ".xisf";
        else
            extension = ".[NATIVE]";
        previewText = m_placeholderPath.generateFilename(*m_job, targetNameT->text(), previewType == LOCAL_PREVIEW, true, 1,
                      extension, "", false);
        m_captureModuleState->allJobs().removeLast();
        previewText = QDir::toNativeSeparators(previewText);
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

    auto reducedFocalLength = m_Reducer * m_FocalLength;
    auto reducedFocalRatio = m_FocalRatio > 0 ? m_FocalRatio : reducedFocalLength / m_Aperture;

    if (m_captureDeviceAdaptor->getActiveCamera() != nullptr)
    {
        qCInfo(KSTARS_EKOS_CAPTURE) << "set ExposureCalculator preferred camera to active camera id: "
                                    << m_captureDeviceAdaptor->getActiveCamera()->getDeviceName();
    }

    QPointer<ExposureCalculatorDialog> anExposureCalculatorDialog(new ExposureCalculatorDialog(KStars::Instance(),
            preferredSkyQuality,
            reducedFocalRatio,
            m_captureDeviceAdaptor->getActiveCamera()->getDeviceName()));
    anExposureCalculatorDialog->setAttribute(Qt::WA_DeleteOnClose);
    anExposureCalculatorDialog->show();
}

}
