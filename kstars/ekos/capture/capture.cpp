/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "capture.h"

#include "captureadaptor.h"
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
#include "indi/indilistener.h"
#include "oal/observeradd.h"
#include "ekos/guide/guide.h"

#include <basedevice.h>

#include <ekos_capture_debug.h>

#define MF_TIMER_TIMEOUT    90000
#define GD_TIMER_TIMEOUT    60000
#define MF_RA_DIFF_LIMIT    4

// Wait 3-minutes as maximum beyond exposure
// value.
#define CAPTURE_TIMEOUT_THRESHOLD  180000

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
    m_captureDeviceAdaptor.reset(new CaptureDeviceAdaptor(m_captureModuleState));

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

    m_RotatorControlPanel.reset(new RotatorSettings(Manager::Instance()));

    // hide avg. download time and target drift initially
    targetDriftLabel->setVisible(false);
    targetDrift->setVisible(false);
    targetDriftUnit->setVisible(false);
    avgDownloadTime->setVisible(false);
    avgDownloadLabel->setVisible(false);
    secLabel->setVisible(false);
    connect(m_RotatorControlPanel->setRawAngleB, &QPushButton::clicked, this, [this]()
    {
        double angle = m_RotatorControlPanel->targetRawAngle();
        m_captureDeviceAdaptor->setRotatorAngle(&angle);
    });
    connect(m_RotatorControlPanel->setPositionAngleB, &QPushButton::clicked, this, [this]()
    {
        // 1. PA = (RawAngle * Multiplier) - Offset
        // 2. Offset = (RawAngle * Multiplier) - PA
        // 3. RawAngle = (Offset + PA) / Multiplier
        double rawAngle = (m_RotatorControlPanel->adjustedOffset() + m_RotatorControlPanel->targetPositionAngle()) /
                          Options::pAMultiplier();
        while (rawAngle < 0)
            rawAngle += 360;
        while (rawAngle > 360)
            rawAngle -= 360;

        m_RotatorControlPanel->setTargetRawAngle(rawAngle);
        m_captureDeviceAdaptor->setRotatorAngle(&rawAngle);
    });
    connect(m_RotatorControlPanel->ReverseDirectionCheck, &QCheckBox::toggled, this, [this](bool toggled)
    {
        m_captureDeviceAdaptor->reverseRotator(toggled);
    });

    seqFileCount = 0;
    seqDelayTimer = new QTimer(this);
    connect(seqDelayTimer, &QTimer::timeout, this, &Capture::captureImage);
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
    connect(rotatorB, &QPushButton::clicked, m_RotatorControlPanel.get(), &Capture::show);

    connect(generateDarkFlatsB, &QPushButton::clicked, this, &Capture::generateDarkFlats);
    connect(scriptManagerB, &QPushButton::clicked, this, &Capture::handleScriptsManager);

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
    rotatorB->setIcon(QIcon::fromTheme("kstars_solarsystem"));
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
    connect(m_LimitsUI->startGuiderDriftN, &QDoubleSpinBox::editingFinished, this, [this]()
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
    connect(m_LimitsUI->limitGuideDeviationN, &QDoubleSpinBox::editingFinished, this, [this]()
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
    connect(m_LimitsUI->limitFocusHFRN, &QDoubleSpinBox::editingFinished, this, [this]()
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
    connect(m_LimitsUI->limitFocusDeltaTN, &QDoubleSpinBox::editingFinished, this, [this]()
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
    connect(m_LimitsUI->limitRefocusN, &QDoubleSpinBox::editingFinished, this, [this]()
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
        connect(control, &QCheckBox::toggled, this, &Capture::setDirty);

    QDoubleSpinBox * const dspinBoxes[]
    {
        m_LimitsUI->limitFocusHFRN,
        m_LimitsUI->limitFocusDeltaTN,
        m_LimitsUI->limitGuideDeviationN,
    };
    for (const QDoubleSpinBox * control : dspinBoxes)
        connect(control, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), this,
                &Capture::setDirty);

    connect(fileUploadModeS, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
            &Capture::setDirty);
    connect(fileRemoteDirT, &QLineEdit::editingFinished, this, &Capture::setDirty);

    m_ObserverName = Options::defaultObserver();
    observerB->setIcon(QIcon::fromTheme("im-user"));
    observerB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    connect(observerB, &QPushButton::clicked, this, &Capture::showObserverDialog);

    // Exposure Timeout
    captureTimeout.setSingleShot(true);
    connect(&captureTimeout, &QTimer::timeout, this, &Capture::processCaptureTimeout);

    // Post capture script
    connect(&m_CaptureScript, static_cast<void (QProcess::*)(int exitCode, QProcess::ExitStatus status)>(&QProcess::finished),
            this, &Capture::scriptFinished);
    connect(&m_CaptureScript, &QProcess::errorOccurred, this,
            [this](QProcess::ProcessError error)
    {
        Q_UNUSED(error)
        appendLogText(m_CaptureScript.errorString());
        scriptFinished(-1, QProcess::NormalExit);
    });
    connect(&m_CaptureScript, &QProcess::readyReadStandardError, this,
            [this]()
    {
        appendLogText(m_CaptureScript.readAllStandardError());
    });
    connect(&m_CaptureScript, &QProcess::readyReadStandardOutput, this,
            [this]()
    {
        appendLogText(m_CaptureScript.readAllStandardOutput());
    });

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
    targetADUTolerance = Options::calibrationADUValueTolerance();

    if(!Options::captureDirectory().isEmpty())
        fileDirectoryT->setText(Options::captureDirectory());
    else
    {
        fileDirectoryT->setText(QDir::toNativeSeparators(QDir::homePath() + "/Pictures"));
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
    downloadProgressTimer.setInterval(100);
    connect(&downloadProgressTimer, &QTimer::timeout, this, &Capture::setDownloadProgress);

    DarkLibrary::Instance()->setCaptureModule(this);
    m_DarkProcessor = new DarkProcessor(this);
    connect(m_DarkProcessor, &DarkProcessor::newLog, this, &Capture::appendLogText);
    connect(m_DarkProcessor, &DarkProcessor::darkFrameCompleted, this, &Capture::setCaptureComplete);

    // display the capture status in the UI
    connect(this, &Capture::newStatus, captureStatusWidget, &LedStatusWidget::setCaptureState);

    // react upon state changes
    connect(m_captureModuleState.data(), &CaptureModuleState::startCapture, this, &Capture::start);
    connect(m_captureModuleState.data(), &CaptureModuleState::abortCapture, this, &Capture::abort);
    connect(m_captureModuleState.data(), &CaptureModuleState::suspendCapture, this, &Capture::suspend);
    // forward signals from capture module state
    connect(m_captureModuleState.data(), &CaptureModuleState::newLog, this, &Capture::appendLogText);
    connect(m_captureModuleState.data(), &CaptureModuleState::newStatus, this, &Capture::newStatus);
    connect(m_captureModuleState.data(), &CaptureModuleState::checkFocus, this, &Capture::checkFocus);
    connect(m_captureModuleState.data(), &CaptureModuleState::resetFocus, this, &Capture::resetFocus);
    connect(m_captureModuleState.data(), &CaptureModuleState::guideAfterMeridianFlip, this,
            &Capture::guideAfterMeridianFlip);
    connect(m_captureModuleState.data(), &CaptureModuleState::newFocusStatus, this, &Capture::updateFocusStatus);
    connect(m_captureModuleState.data(), &CaptureModuleState::newMeridianFlipStage, this, &Capture::updateMeridianFlipStage);
    connect(m_captureModuleState.data(), &CaptureModuleState::meridianFlipStarted, this, &Capture::meridianFlipStarted);
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
        m_TargetName = targetNameT->text();
        generatePreviewFilename();
        qCDebug(KSTARS_EKOS_CAPTURE) << "Changed target to" << m_TargetName << "because of user edit";
    });
    connect(captureTypeS, &QComboBox::currentTextChanged, this, &Capture::generatePreviewFilename);

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

    m_RotatorControlPanel->setCurrentPierSide(device->pierSide());
    connect(m_captureDeviceAdaptor->getMount(), &ISD::Mount::pierSideChanged, m_RotatorControlPanel.get(),
            &RotatorSettings::setCurrentPierSide);
    syncTelescopeInfo();
    return true;
}

bool Capture::setRotator(ISD::Rotator * device)
{
    if (m_Rotator && m_Rotator == device)
    {
        rotatorB->setEnabled(true);
        return false;
    }

    if (m_Rotator)
        m_Rotator->disconnect(this);

    m_Rotator = device;

    if (!m_Rotator)
    {
        rotatorB->setEnabled(false);
        return false;
    }

    connect(m_captureDeviceAdaptor.data(), &CaptureDeviceAdaptor::rotatorReverseToggled, this,
            &Capture::setRotatorReversed,
            Qt::UniqueConnection);

    m_captureDeviceAdaptor->setRotator(device);
    rotatorB->setEnabled(true);
    return true;
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
                setCaptureComplete();
                break;
            case CaptureModuleState::CONTINUE_ACTION_NEXT_EXPOSURE:
                startNextExposure();
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
    ignoreJobProgress = !capturedFramesMap.count() && Options::alwaysResetSequenceWhenStarting();

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
        if (!ignoreJobProgress)
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
    else if (ignoreJobProgress)
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
    }

    m_captureModuleState->setGuidingDeviationDetected(false);
    m_captureModuleState->resetSpikesDetected();

    m_captureModuleState->setCaptureState(CAPTURE_PROGRESS);

    startB->setIcon(QIcon::fromTheme("media-playback-stop"));
    startB->setToolTip(i18n("Stop Sequence"));
    pauseB->setEnabled(true);

    setBusy(true);

    if (Options::enforceGuideDeviation() && autoGuideReady == false)
        appendLogText(i18n("Warning: Guide deviation is selected but autoguide process was not started."));
    if (m_LimitsUI->limitFocusHFRS->isChecked() && m_captureModuleState->getRefocusState()->isAutoFocusReady() == false)
        appendLogText(i18n("Warning: in-sequence focusing is selected but autofocus process was not started."));
    if (m_LimitsUI->limitFocusDeltaTS->isChecked() && m_captureModuleState->getRefocusState()->isAutoFocusReady() == false)
        appendLogText(i18n("Warning: temperature delta check is selected but autofocus process was not started."));

    prepareJob(first_job);
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

    captureTimeout.stop();
    m_captureModuleState->getCaptureDelayTimer().stop();

    ADURaw.clear();
    ExpRaw.clear();

    if (activeJob != nullptr)
    {
        if (activeJob->getStatus() == JOB_BUSY)
        {
            QString stopText;
            switch (targetState)
            {
                case CAPTURE_SUSPENDED:
                    stopText = i18n("CCD capture suspended");
                    activeJob->resetStatus(JOB_BUSY);
                    break;

                case CAPTURE_COMPLETE:
                    stopText = i18n("CCD capture complete");
                    activeJob->resetStatus(JOB_DONE);
                    break;

                case CAPTURE_ABORTED:
                    stopText = i18n("CCD capture aborted");
                    activeJob->resetStatus(JOB_ABORTED);
                    break;

                default:
                    stopText = i18n("CCD capture stopped");
                    activeJob->resetStatus(JOB_IDLE);
                    break;
            }
            emit captureAborted(activeJob->getCoreProperty(SequenceJob::SJ_Exposure).toDouble());
            KSNotification::event(QLatin1String("CaptureFailed"), stopText, KSNotification::Capture, KSNotification::Alert);
            appendLogText(stopText);
            activeJob->abort();
            if (activeJob->getCoreProperty(SequenceJob::SJ_Preview).toBool() == false)
            {
                int index = m_captureModuleState->allJobs().indexOf(activeJob);
                QJsonObject oneSequence = m_SequenceArray[index].toObject();
                oneSequence["Status"] = "Aborted";
                m_SequenceArray.replace(index, oneSequence);
                emit sequenceChanged(m_SequenceArray);
            }
        }

        // In case of batch job
        if (activeJob->getCoreProperty(SequenceJob::SJ_Preview).toBool() == false)
        {
            activeJob->disconnect(this);
        }
        // or preview job in calibration stage
        else if (activeJob->getCalibrationStage() == SequenceJobState::CAL_CALIBRATION)
        {
            activeJob->disconnect(this);
            activeJob->setCoreProperty(SequenceJob::SJ_Preview, false);
            if (m_captureDeviceAdaptor->getActiveCamera())
                m_captureDeviceAdaptor->getActiveCamera()->setUploadMode(activeJob->getUploadMode());
        }
        // or regular preview job
        else
        {
            if (m_captureDeviceAdaptor->getActiveCamera())
                m_captureDeviceAdaptor->getActiveCamera()->setUploadMode(activeJob->getUploadMode());
            m_captureModuleState->allJobs().removeOne(activeJob);
            // Delete preview job
            activeJob->deleteLater();
            // Clear active job
            setActiveJob(nullptr);
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
    connectCamera(false);

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
    m_isFraming = false;

    setBusy(false);

    // stopping to CAPTURE_IDLE means that capturing will continue automatically
    auto captureState = m_captureModuleState->getCaptureState();
    if (captureState == CAPTURE_ABORTED || captureState == CAPTURE_SUSPENDED || captureState == CAPTURE_COMPLETE)
    {
        startB->setIcon(
            QIcon::fromTheme("media-playback-start"));
        startB->setToolTip(i18n("Start Sequence"));
        pauseB->setEnabled(false);
    }

    seqDelayTimer->stop();

    setActiveJob(nullptr);
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
        useGuideHead = true;
        m_captureDeviceAdaptor->setActiveChip(m_Camera->getChip(ISD::CameraChip::GUIDE_CCD));
    }

    if (m_captureDeviceAdaptor->getActiveChip() == nullptr)
    {
        useGuideHead = false;
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
    connect(m_Camera, &ISD::Camera::error, this, &Capture::processCaptureError, Qt::UniqueConnection);

    syncCameraInfo();

    // update values received by the device adaptor
    // connect(m_Camera, &ISD::Camera::newTemperatureValue, this, &Capture::updateCCDTemperature, Qt::UniqueConnection);

    DarkLibrary::Instance()->checkCamera();
}

void Capture::connectCamera(bool connection)
{
    if (connection)
    {
        connect(m_captureDeviceAdaptor->getActiveCamera(), &ISD::Camera::newExposureValue, this,
                &Capture::setExposureProgress, Qt::UniqueConnection);
        connect(m_captureDeviceAdaptor->getActiveCamera(), &ISD::Camera::newImage, this, &Capture::processData,
                Qt::UniqueConnection);
        //connect(m_Camera, &ISD::Camera::previewFITSGenerated, this, &Capture::setGeneratedPreviewFITS, Qt::UniqueConnection);
        connect(m_Camera, &ISD::Camera::ready, this, &Capture::ready);
    }
    else
    {
        disconnect(m_captureDeviceAdaptor->getActiveCamera(), &ISD::Camera::newImage, this, &Capture::processData);
        disconnect(m_captureDeviceAdaptor->getActiveCamera(), &ISD::Camera::newExposureValue, this,
                   &Capture::setExposureProgress);
        //    disconnect(m_Camera, &ISD::Camera::previewFITSGenerated, this, &Capture::setGeneratedPreviewFITS);
        disconnect(m_captureDeviceAdaptor->getActiveCamera(), &ISD::Camera::ready, this, &Capture::ready);
    }
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

    suspendGuideOnDownload =
        (m_captureDeviceAdaptor->getActiveCamera()->getChip(ISD::CameraChip::GUIDE_CCD) == guideChip) ||
        (guideChip->getCCD() == m_captureDeviceAdaptor->getActiveCamera() &&
         m_captureDeviceAdaptor->getActiveCamera()->getDriverInfo()->getAuxInfo().value("mdpd", false).toBool());
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

    QString frameProp    = useGuideHead ? QString("GUIDER_FRAME") : QString("CCD_FRAME");
    QString exposureProp = useGuideHead ? QString("GUIDER_EXPOSURE") : QString("CCD_EXPOSURE");
    QString exposureElem = useGuideHead ? QString("GUIDER_EXPOSURE_VALUE") : QString("CCD_EXPOSURE_VALUE");
    m_captureDeviceAdaptor->setActiveChip(useGuideHead ? m_captureDeviceAdaptor->getActiveCamera()->getChip(
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
    if (useGuideHead == false)
        cullToDSLRLimits();

    if (reset == 1 || frameSettings.contains(m_captureDeviceAdaptor->getActiveChip()) == false)
    {
        QVariantMap settings;

        settings["x"]    = 0;
        settings["y"]    = 0;
        settings["w"]    = captureFrameWN->maximum();
        settings["h"]    = captureFrameHN->maximum();
        settings["binx"] = 1;
        settings["biny"] = 1;

        frameSettings[m_captureDeviceAdaptor->getActiveChip()] = settings;
    }
    else if (reset == 2 && frameSettings.contains(m_captureDeviceAdaptor->getActiveChip()))
    {
        QVariantMap settings = frameSettings[m_captureDeviceAdaptor->getActiveChip()];
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

        frameSettings[m_captureDeviceAdaptor->getActiveChip()] = settings;
    }

    if (frameSettings.contains(m_captureDeviceAdaptor->getActiveChip()))
    {
        QVariantMap settings = frameSettings[m_captureDeviceAdaptor->getActiveChip()];
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

    if ((prop.isNameMatch("CCD_FRAME") && useGuideHead == false) ||
            (prop.isNameMatch("GUIDER_FRAME") && useGuideHead))
        updateFrameProperties();
    else if ((prop.isNameMatch("CCD_INFO") && useGuideHead == false) ||
             (prop.isNameMatch("GUIDER_INFO") && useGuideHead))
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
    m_captureDeviceAdaptor->setActiveChip(useGuideHead ? m_captureDeviceAdaptor->getActiveCamera()->getChip(
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

    const int seqDelay = activeJob->getCoreProperty(SequenceJob::SJ_Delay).toInt();
    // nothing pending, let's start the next exposure
    if (seqDelay > 0)
    {
        m_captureModuleState->setCaptureState(CAPTURE_WAITING);
    }
    seqDelayTimer->start(seqDelay);

    return IPS_OK;
}

void Capture::checkNextExposure()
{
    IPState started = startNextExposure();
    // if starting the next exposure did not succeed due to pending jobs running,
    // we retry after 1 second
    if (started == IPS_BUSY)
        QTimer::singleShot(1000, this, &Capture::checkNextExposure);
}


void Capture::processData(const QSharedPointer<FITSData> &data)
{
    ISD::CameraChip * tChip = nullptr;

    QString blobInfo;
    if (data)
    {
        m_ImageData = data;
        blobInfo = QString("{Device: %1 Property: %2 Element: %3 Chip: %4}").arg(data->property("device").toString())
                   .arg(data->property("blobVector").toString())
                   .arg(data->property("blobElement").toString())
                   .arg(data->property("chip").toInt());
    }
    else
        m_ImageData.reset();

    // If there is no active job, ignore
    if (activeJob == nullptr)
    {
        if (data)
            qCWarning(KSTARS_EKOS_CAPTURE) << blobInfo << "Ignoring received FITS as active job is null.";
        return;
    }

    if (getMeridianFlipState()->getMeridianFlipStage() >= MeridianFlipState::MF_ALIGNING)
    {
        if (data)
            qCWarning(KSTARS_EKOS_CAPTURE) << blobInfo << "Ignoring Received FITS as meridian flip stage is" <<
                                           getMeridianFlipState()->getMeridianFlipStage();
        return;
    }

    // If image is client or both, let's process it.
    if (m_captureDeviceAdaptor->getActiveCamera()
            && m_captureDeviceAdaptor->getActiveCamera()->getUploadMode() != ISD::Camera::UPLOAD_LOCAL)
    {
        //        if (data.isNull())
        //        {
        //            appendLogText(i18n("Failed to save file to %1", activeJob->getSignature()));
        //            abort();
        //            return;
        //        }

        if (m_captureModuleState->getCaptureState() == CAPTURE_IDLE || m_captureModuleState->getCaptureState() == CAPTURE_ABORTED)
        {
            qCWarning(KSTARS_EKOS_CAPTURE) << blobInfo << "Ignoring Received FITS as current capture state is not active" <<
                                           m_captureModuleState->getCaptureState();
            return;
        }

        //if (!strcmp(data->name, "CCD2"))
        if (data)
        {
            tChip = m_captureDeviceAdaptor->getActiveCamera()->getChip(static_cast<ISD::CameraChip::ChipType>
                    (data->property("chip").toInt()));
            if (tChip != m_captureDeviceAdaptor->getActiveChip())
            {
                if (m_captureModuleState->getGuideState() == GUIDE_IDLE)
                    qCWarning(KSTARS_EKOS_CAPTURE) << blobInfo << "Ignoring Received FITS as it does not correspond to the target chip"
                                                   << m_captureDeviceAdaptor->getActiveChip()->getType();
                return;
            }
        }

        if (m_captureDeviceAdaptor->getActiveChip()->getCaptureMode() == FITS_FOCUS ||
                m_captureDeviceAdaptor->getActiveChip()->getCaptureMode() == FITS_GUIDE)
        {
            qCWarning(KSTARS_EKOS_CAPTURE) << blobInfo << "Ignoring Received FITS as it has the wrong capture mode" <<
                                           m_captureDeviceAdaptor->getActiveChip()->getCaptureMode();
            return;
        }

        // If the FITS is not for our device, simply ignore

        if (data && data->property("device").toString() != m_captureDeviceAdaptor->getActiveCamera()->getDeviceName())
        {
            qCWarning(KSTARS_EKOS_CAPTURE) << blobInfo << "Ignoring Received FITS as the blob device name does not equal active camera"
                                           << m_captureDeviceAdaptor->getActiveCamera()->getDeviceName();
            return;
        }

        // If this is a preview job, make sure to enable preview button after
        // we receive the FITS
        if (activeJob->getCoreProperty(SequenceJob::SJ_Preview).toBool() && previewB->isEnabled() == false)
            previewB->setEnabled(true);

        // If dark is selected, perform dark substraction.
        if (data && darkB->isChecked() && activeJob->getCoreProperty(SequenceJob::SJ_Preview).toBool() && useGuideHead == false)
        {
            m_DarkProcessor->denoise(OpticalTrainManager::Instance()->id(opticalTrainCombo->currentText()),
                                     m_captureDeviceAdaptor->getActiveChip(),
                                     m_ImageData,
                                     activeJob->getCoreProperty(SequenceJob::SJ_Exposure).toDouble(),
                                     activeJob->getCoreProperty(SequenceJob::SJ_ROI).toRect().x(),
                                     activeJob->getCoreProperty(SequenceJob::SJ_ROI).toRect().y());
            return;
        }
    }

    setCaptureComplete();
}

IPState Capture::setCaptureComplete()
{
    captureTimeout.stop();
    m_CaptureTimeoutCounter = 0;

    downloadProgressTimer.stop();

    if (!activeJob)
        return IPS_BUSY;

    // In case we're framing, let's return quick to continue the process.
    if (m_isFraming)
    {
        emit newImage(activeJob, m_ImageData);
        // If fast exposure is on, do not capture again, it will be captured by the driver.
        if (m_captureDeviceAdaptor->getActiveCamera()->isFastExposureEnabled() == false)
        {
            captureStatusWidget->setStatus(i18n("Framing..."), Qt::darkGreen);
            const int seqDelay = activeJob->getCoreProperty(SequenceJob::SJ_Delay).toInt();

            if (seqDelay > 0)
            {
                QTimer::singleShot(seqDelay, this, [this]()
                {
                    activeJob->startCapturing(m_captureModuleState->getRefocusState()->isAutoFocusReady(), FITS_NORMAL);
                });
            }
            else
                activeJob->startCapturing(m_captureModuleState->getRefocusState()->isAutoFocusReady(), FITS_NORMAL);
        }
        return IPS_OK;
    }

    // If fast exposure is off, disconnect exposure progress
    // otherwise, keep it going since it fires off from driver continuous capture process.
    if (m_captureDeviceAdaptor->getActiveCamera()->isFastExposureEnabled() == false)
    {
        disconnect(m_captureDeviceAdaptor->getActiveCamera(), &ISD::Camera::newExposureValue, this,
                   &Capture::setExposureProgress);
        DarkLibrary::Instance()->disconnect(this);
    }

    // Do not calculate download time for images stored on server.
    // Only calculate for longer exposures.
    if (m_captureDeviceAdaptor->getActiveCamera()->getUploadMode() != ISD::Camera::UPLOAD_LOCAL && m_DownloadTimer.isValid())
    {
        //This determines the time since the image started downloading
        //Then it gets the estimated time left and displays it in the log.
        double currentDownloadTime = m_DownloadTimer.elapsed() / 1000.0;
        downloadTimes << currentDownloadTime;
        QString dLTimeString = QString::number(currentDownloadTime, 'd', 2);
        QString estimatedTimeString = QString::number(getEstimatedDownloadTime(), 'd', 2);
        appendLogText(i18n("Download Time: %1 s, New Download Time Estimate: %2 s.", dLTimeString, estimatedTimeString));

        // Always invalidate timer as it must be explicitly started.
        m_DownloadTimer.invalidate();
    }

    // Do not display notifications for very short captures
    if (activeJob->getCoreProperty(SequenceJob::SJ_Exposure).toDouble() >= 1)
        KSNotification::event(QLatin1String("EkosCaptureImageReceived"), i18n("Captured image received"),
                              KSNotification::Capture);

    // If it was initially set as pure preview job and NOT as preview for calibration
    if (activeJob->getCoreProperty(SequenceJob::SJ_Preview).toBool())
    {
        //sendNewImage(blobFilename, blobChip);
        emit newImage(activeJob, m_ImageData);
        m_captureModuleState->allJobs().removeOne(activeJob);
        // Reset upload mode if it was changed by preview
        m_captureDeviceAdaptor->getActiveCamera()->setUploadMode(activeJob->getUploadMode());
        // Reset active job pointer
        setActiveJob(nullptr);
        stop(CAPTURE_COMPLETE);
        if (m_captureModuleState->getGuideState() == GUIDE_SUSPENDED && suspendGuideOnDownload)
            emit resumeGuiding();
        return IPS_OK;
    }

    // check if pausing has been requested
    if (checkPausing() == true)
    {
        // resume with capture complete
        m_captureModuleState->setContinueAction(CaptureModuleState::CONTINUE_ACTION_CAPTURE_COMPLETE);
        return IPS_BUSY;
    }

    if (! activeJob->getCoreProperty(SequenceJob::SJ_Preview).toBool()
            && activeJob->getCalibrationStage() != SequenceJobState::CAL_CALIBRATION)
    {
        /* Increase the sequence's current capture count */
        activeJob->setCompleted(activeJob->getCompleted() + 1);
        /* Decrease the counter for in-sequence focusing */
        m_captureModuleState->getRefocusState()->decreaseInSequenceFocusCounter();
    }

    /* Decrease the dithering counter except for directly after meridian flip                                              */
    /* Hint: this isonly relevant when a meridian flip happened during a paused sequence when pressing "Start" afterwards. */
    if (getMeridianFlipState()->getMeridianFlipStage() < MeridianFlipState::MF_FLIPPING)
        m_captureModuleState->decreaseDitherCounter();

    // JM 2020-06-17: Emit newImage for LOCAL images (stored on remote host)
    //if (m_Camera->getUploadMode() == ISD::Camera::UPLOAD_LOCAL)
    emit newImage(activeJob, m_ImageData);
    // For Client/Both images, send file name.
    //else
    //    sendNewImage(blobFilename, blobChip);


    /* If we were assigned a captured frame map, also increase the relevant counter for prepareJob */
    SchedulerJob::CapturedFramesMap::iterator frame_item = capturedFramesMap.find(activeJob->getSignature());
    if (capturedFramesMap.end() != frame_item)
        frame_item.value()++;

    if (activeJob->getFrameType() != FRAME_LIGHT)
    {
        if (processPostCaptureCalibrationStage() == false)
            return IPS_OK;

        if (activeJob->getCalibrationStage() == SequenceJobState::CAL_CALIBRATION_COMPLETE)
            activeJob->setCalibrationStage(SequenceJobState::CAL_CAPTURING);
    }

    /* The image progress has now one more capture */
    imgProgress->setValue(activeJob->getCompleted());

    appendLogText(i18n("Received image %1 out of %2.", activeJob->getCompleted(),
                       activeJob->getCoreProperty(SequenceJob::SJ_Count).toInt()));

    double hfr = -1, eccentricity = -1;
    int numStars = -1, median = -1;
    QString filename;
    if (m_ImageData)
    {
        QVariant frameType;
        if (Options::autoHFR() && m_ImageData && !m_ImageData->areStarsSearched() && m_ImageData->getRecordValue("FRAME", frameType)
                && frameType.toString() == "Light")
        {
            QFuture<bool> result = m_ImageData->findStars(ALGORITHM_SEP);
            result.waitForFinished();
        }
        hfr = m_ImageData->getHFR(HFR_AVERAGE);
        numStars = m_ImageData->getSkyBackground().starsDetected;
        median = m_ImageData->getMedian();
        eccentricity = m_ImageData->getEccentricity();
        filename = m_ImageData->filename();
        appendLogText(i18n("Captured %1", filename));
        auto remainingPlaceholders = PlaceholderPath::remainingPlaceholders(filename);
        if (remainingPlaceholders.size() > 0)
        {
            appendLogText(
                i18n("WARNING: remaining and potentially unknown placeholders %1 in %2",
                     remainingPlaceholders.join(", "), filename));
        }
    }

    m_captureModuleState->setCaptureState(CAPTURE_IMAGE_RECEIVED);

    if (activeJob)
    {
        QVariantMap metadata;
        metadata["filename"] = filename;
        metadata["type"] = activeJob->getFrameType();
        metadata["exposure"] = activeJob->getCoreProperty(SequenceJob::SJ_Exposure).toDouble();
        metadata["filter"] = activeJob->getCoreProperty(SequenceJob::SJ_Filter).toString();
        metadata["width"] = activeJob->getCoreProperty(SequenceJob::SJ_ROI).toRect().width();
        metadata["height"] = activeJob->getCoreProperty(SequenceJob::SJ_ROI).toRect().height();
        metadata["hfr"] = hfr;
        metadata["starCount"] = numStars;
        metadata["median"] = median;
        metadata["eccentricity"] = eccentricity;
        emit captureComplete(metadata);

        // Check if we need to execute post capture script first

        const QString postCaptureScript = activeJob->getScript(SCRIPT_POST_CAPTURE);
        if (postCaptureScript.isEmpty() == false)
        {
            m_CaptureScriptType = SCRIPT_POST_CAPTURE;
            m_CaptureScript.start(postCaptureScript, generateScriptArguments());
            appendLogText(i18n("Executing post capture script %1", postCaptureScript));
            return IPS_OK;
        }

        // if we're done
        if (activeJob->getCoreProperty(SequenceJob::SJ_Count).toInt() <= activeJob->getCompleted())
        {
            processJobCompletionStage1();
            return IPS_OK;
        }
    }

    return resumeSequence();
}

void Capture::processJobCompletionStage1()
{
    if (activeJob == nullptr)
    {
        qWarning(KSTARS_EKOS_CAPTURE) << "procesJobCompletionStage1 with null activeJob.";
    }
    else
    {
        // JM 2020-12-06: Check if we need to execute post-job script first.
        const QString postJobScript = activeJob->getScript(SCRIPT_POST_JOB);
        if (!postJobScript.isEmpty())
        {
            m_CaptureScriptType = SCRIPT_POST_JOB;
            m_CaptureScript.start(postJobScript, generateScriptArguments());
            appendLogText(i18n("Executing post job script %1", postJobScript));
            return;
        }
    }

    processJobCompletionStage2();
}

void Capture::processJobCompletionStage2()
{
    if (activeJob == nullptr)
    {
        qWarning(KSTARS_EKOS_CAPTURE) << "procesJobCompletionStage2 with null activeJob.";
    }
    else
    {
        activeJob->done();

        if (activeJob->getCoreProperty(SequenceJob::SJ_Preview).toBool() == false)
        {
            int index = m_captureModuleState->allJobs().indexOf(activeJob);
            QJsonObject oneSequence = m_SequenceArray[index].toObject();
            oneSequence["Status"] = "Complete";
            m_SequenceArray.replace(index, oneSequence);
            emit sequenceChanged(m_SequenceArray);
        }
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
                              KSNotification::Capture);

        stop(CAPTURE_COMPLETE);

        //Resume guiding if it was suspended before
        //if (isAutoGuiding && currentCCD->getChip(ISD::CameraChip::GUIDE_CCD) == guideChip)
        if (m_captureModuleState->getGuideState() == GUIDE_SUSPENDED && suspendGuideOnDownload)
            emit resumeGuiding();
    }
}


IPState Capture::resumeSequence()
{
    // If no job is active, we have to find if there are more pending jobs in the queue
    if (!activeJob)
    {
        SequenceJob * next_job = nullptr;

        for (auto &oneJob : m_captureModuleState->allJobs())
        {
            if (oneJob->getStatus() == JOB_IDLE || oneJob->getStatus() == JOB_ABORTED)
            {
                next_job = oneJob;
                break;
            }
        }

        if (next_job)
        {

            prepareJob(next_job);

            //Resume guiding if it was suspended before, except for an active meridian flip is running.
            //if (isAutoGuiding && currentCCD->getChip(ISD::CameraChip::GUIDE_CCD) == guideChip)
            if (m_captureModuleState->getGuideState() == GUIDE_SUSPENDED && suspendGuideOnDownload &&
                    getMeridianFlipState()->checkMeridianFlipActive() == false)
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
        // If we suspended guiding due to primary chip download, resume guide chip guiding now - unless
        // a meridian flip is ongoing
        if (m_captureModuleState->getGuideState() == GUIDE_SUSPENDED && suspendGuideOnDownload &&
                getMeridianFlipState()->checkMeridianFlipActive() == false)
        {
            qCInfo(KSTARS_EKOS_CAPTURE) << "Resuming guiding...";
            emit resumeGuiding();
        }

        // If looping, we just increment the file system image count
        if (m_captureDeviceAdaptor->getActiveCamera()->isFastExposureEnabled())
        {
            if (m_captureDeviceAdaptor->getActiveCamera()->getUploadMode() != ISD::Camera::UPLOAD_LOCAL)
            {
                checkSeqBoundary();
                m_captureDeviceAdaptor->getActiveCamera()->setNextSequenceID(nextSequenceID);
            }
        }

        // set state to capture preparation
        m_captureModuleState->setCaptureState(CAPTURE_PROGRESS);

        const QString preCaptureScript = activeJob->getScript(SCRIPT_PRE_CAPTURE);
        // JM 2020-12-06: Check if we need to execute pre-capture script first.
        if (!preCaptureScript.isEmpty())
        {
            if (m_captureDeviceAdaptor->getActiveCamera()->isFastExposureEnabled())
            {
                m_RememberFastExposure = true;
                m_captureDeviceAdaptor->getActiveCamera()->setFastExposureEnabled(false);
            }

            m_CaptureScriptType = SCRIPT_PRE_CAPTURE;
            m_CaptureScript.start(preCaptureScript, generateScriptArguments());
            appendLogText(i18n("Executing pre capture script %1", preCaptureScript));
            return IPS_BUSY;
        }
        else
        {
            // Check if we need to stop fast exposure to perform any
            // pending tasks. If not continue as is.
            if (m_captureDeviceAdaptor->getActiveCamera()->isFastExposureEnabled())
            {
                if (activeJob &&
                        activeJob->getFrameType() == FRAME_LIGHT &&
                        checkLightFramePendingTasks() == IPS_OK)
                {
                    // Continue capturing seamlessly
                    m_captureModuleState->setCaptureState(CAPTURE_CAPTURING);
                    return IPS_OK;
                }

                // Stop fast exposure now.
                m_RememberFastExposure = true;
                m_captureDeviceAdaptor->getActiveCamera()->setFastExposureEnabled(false);
            }

            checkNextExposure();

        }
    }

    return IPS_OK;
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
        prepareJob(m_captureModuleState->allJobs().last());
    }
}

void Capture::startFraming()
{
    if (m_captureModuleState->getFocusState() >= FOCUS_PROGRESS)
    {
        appendLogText(i18n("Cannot start framing while focus module is busy."));
    }
    else if (!m_isFraming)
    {
        m_isFraming = true;
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

void Capture::captureImage()
{
    if (activeJob == nullptr)
        return;

    // Bail out if we have no CCD anymore
    if (m_captureDeviceAdaptor->getActiveCamera()->isConnected() == false)
    {
        appendLogText(i18n("Error: Lost connection to CCD."));
        abort();
        return;
    }

    captureTimeout.stop();
    seqDelayTimer->stop();
    m_captureModuleState->getCaptureDelayTimer().stop();

    if (m_captureDeviceAdaptor->getFilterWheel() != nullptr)
    {
        // JM 2021.08.23 Call filter info to set the active filter wheel in the camera driver
        // so that it may snoop on the active filter
        syncFilterInfo();
        updateCurrentFilterPosition();
    }

    if (m_captureDeviceAdaptor->getActiveCamera()->isFastExposureEnabled())
    {
        int remaining = m_isFraming ? 100000 : (activeJob->getCoreProperty(SequenceJob::SJ_Count).toInt() -
                                                activeJob->getCompleted());
        if (remaining > 1)
            m_captureDeviceAdaptor->getActiveCamera()->setFastCount(static_cast<uint>(remaining));
    }

    connectCamera(true);

    if (activeJob->getFrameType() == FRAME_FLAT)
    {
        // If we have to calibrate ADU levels, first capture must be preview and not in batch mode
        if (activeJob->getCoreProperty(SequenceJob::SJ_Preview).toBool() == false
                && activeJob->getFlatFieldDuration() == DURATION_ADU &&
                activeJob->getCalibrationStage() == SequenceJobState::CAL_NONE)
        {
            if (m_captureDeviceAdaptor->getActiveCamera()->getEncodingFormat() != "FITS" &&
                    m_captureDeviceAdaptor->getActiveCamera()->getEncodingFormat() != "XISF")
            {
                appendLogText(i18n("Cannot calculate ADU levels in non-FITS images."));
                abort();
                return;
            }

            activeJob->setCalibrationStage(SequenceJobState::CAL_CALIBRATION);
        }
    }

    // If preview, always set to UPLOAD_CLIENT if not already set.
    if (activeJob->getCoreProperty(SequenceJob::SJ_Preview).toBool())
    {
        if (m_captureDeviceAdaptor->getActiveCamera()->getUploadMode() != ISD::Camera::UPLOAD_CLIENT)
            m_captureDeviceAdaptor->getActiveCamera()->setUploadMode(ISD::Camera::UPLOAD_CLIENT);
    }
    // If batch mode, ensure upload mode mathces the active job target.
    else
    {
        if (m_captureDeviceAdaptor->getActiveCamera()->getUploadMode() != activeJob->getUploadMode())
            m_captureDeviceAdaptor->getActiveCamera()->setUploadMode(activeJob->getUploadMode());
    }

    if (m_captureDeviceAdaptor->getActiveCamera()->getUploadMode() != ISD::Camera::UPLOAD_LOCAL)
    {
        checkSeqBoundary();
        m_captureDeviceAdaptor->getActiveCamera()->setNextSequenceID(nextSequenceID);
    }

    if (frameSettings.contains(m_captureDeviceAdaptor->getActiveChip()))
    {
        const auto roi = activeJob->getCoreProperty(SequenceJob::SJ_ROI).toRect();
        QVariantMap settings;
        settings["x"]    = roi.x();
        settings["y"]    = roi.y();
        settings["w"]    = roi.width();
        settings["h"]    = roi.height();
        settings["binx"] = activeJob->getCoreProperty(SequenceJob::SJ_Binning).toPoint().x();
        settings["biny"] = activeJob->getCoreProperty(SequenceJob::SJ_Binning).toPoint().y();

        frameSettings[m_captureDeviceAdaptor->getActiveChip()] = settings;
    }

    // Re-enable fast exposure if it was disabled before due to pending tasks
    if (m_RememberFastExposure)
    {
        m_RememberFastExposure = false;
        m_captureDeviceAdaptor->getActiveCamera()->setFastExposureEnabled(true);
    }

    // If using DSLR, make sure it is set to correct transfer format
    m_captureDeviceAdaptor->getActiveCamera()->setEncodingFormat(activeJob->getCoreProperty(
                SequenceJob::SJ_Encoding).toString());

    // necessary since the status widget doesn't store the calibration stage
    if (activeJob->getCalibrationStage() == SequenceJobState::CAL_CALIBRATION)
        captureStatusWidget->setStatus(i18n("Calibrating..."), Qt::yellow);

    m_captureModuleState->setStartingCapture(true);
    auto placeholderPath = PlaceholderPath(m_SequenceURL.toLocalFile());
    placeholderPath.setGenerateFilenameSettings(*activeJob, m_TargetName);
    m_captureDeviceAdaptor->getActiveCamera()->setPlaceholderPath(placeholderPath);
    // now hand over the control of capturing to the sequence job. As soon as capturing
    // has started, the sequence job will report the result with the captureStarted() event
    // that will trigger Capture::captureStarted()
    activeJob->startCapturing(m_captureModuleState->getRefocusState()->isAutoFocusReady(),
                              activeJob->getCalibrationStage() == SequenceJobState::CAL_CALIBRATION ? FITS_CALIBRATE : FITS_NORMAL);

}

void Capture::captureStarted(CAPTUREResult rc)
{
    if (rc != CAPTURE_OK)
    {
        disconnect(m_captureDeviceAdaptor->getActiveCamera(), &ISD::Camera::newExposureValue, this,
                   &Capture::setExposureProgress);
    }
    switch (rc)
    {
        case CAPTURE_OK:
        {
            m_captureModuleState->setCaptureState(CAPTURE_CAPTURING);
            emit captureStarting(activeJob->getCoreProperty(SequenceJob::SJ_Exposure).toDouble(),
                                 activeJob->getCoreProperty(SequenceJob::SJ_Filter).toString());
            appendLogText(i18n("Capturing %1-second %2 image...",
                               QString("%L1").arg(activeJob->getCoreProperty(SequenceJob::SJ_Exposure).toDouble(), 0, 'f', 3),
                               activeJob->getCoreProperty(SequenceJob::SJ_Filter).toString()));
            captureTimeout.start(static_cast<int>(activeJob->getCoreProperty(SequenceJob::SJ_Exposure).toDouble()) * 1000 +
                                 CAPTURE_TIMEOUT_THRESHOLD);
            // calculate remaining capture time for the current job
            imageCountDown.setHMS(0, 0, 0);
            double ms_left = std::ceil(activeJob->getExposeLeft() * 1000.0);
            imageCountDown = imageCountDown.addMSecs(int(ms_left));
            lastRemainingFrameTimeMS = ms_left;
            sequenceCountDown.setHMS(0, 0, 0);
            sequenceCountDown = sequenceCountDown.addSecs(getActiveJobRemainingTime());
            frameInfoLabel->setText(QString("%1 %2 (%L3/%L4):").arg(CCDFrameTypeNames[activeJob->getFrameType()])
                                    .arg(activeJob->getCoreProperty(SequenceJob::SJ_Filter).toString())
                                    .arg(activeJob->getCompleted()).arg(activeJob->getCoreProperty(SequenceJob::SJ_Count).toInt()));
            // ensure that the download time label is visible
            avgDownloadTime->setVisible(true);
            avgDownloadLabel->setVisible(true);
            secLabel->setVisible(true);
            // show estimated download time
            avgDownloadTime->setText(QString("%L1").arg(getEstimatedDownloadTime(), 0, 'd', 2));

            if (activeJob->getCoreProperty(SequenceJob::SJ_Preview).toBool() == false)
            {
                int index = m_captureModuleState->allJobs().indexOf(activeJob);
                QJsonObject oneSequence = m_SequenceArray[index].toObject();
                oneSequence["Status"] = "In Progress";
                m_SequenceArray.replace(index, oneSequence);
                emit sequenceChanged(m_SequenceArray);
            }
        }
        break;

        case CAPTURE_FRAME_ERROR:
            appendLogText(i18n("Failed to set sub frame."));
            abort();
            break;

        case CAPTURE_BIN_ERROR:
            appendLogText(i18n("Failed to set binning."));
            abort();
            break;

        case CAPTURE_FOCUS_ERROR:
            appendLogText(i18n("Cannot capture while focus module is busy."));
            abort();
            break;
    }
}

void Capture::updateSequencePrefix(const QString &newPrefix)
{
    seqPrefix = newPrefix;
    nextSequenceID = 1;
}

void Capture::checkSeqBoundary()
{
    // No updates during meridian flip
    if (getMeridianFlipState()->getMeridianFlipStage() >= MeridianFlipState::MF_ALIGNING)
        return;

    auto placeholderPath = PlaceholderPath(m_SequenceURL.toLocalFile());


    nextSequenceID = placeholderPath.checkSeqBoundary(*activeJob, m_TargetName);
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
    if (activeJob)
    {
        double downloadTimeLeft = getEstimatedDownloadTime() - m_DownloadTimer.elapsed() / 1000.0;
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

    if (activeJob != nullptr && state == IPS_OK)
    {
        activeJob->setCaptureRetires(0);
        activeJob->setExposeLeft(0);

        if (m_captureDeviceAdaptor->getActiveCamera()
                && m_captureDeviceAdaptor->getActiveCamera()->getUploadMode() == ISD::Camera::UPLOAD_LOCAL)
        {
            if (activeJob && activeJob->getStatus() == JOB_BUSY)
            {
                processData(nullptr);
                return;
            }
        }

        //if (isAutoGuiding && Options::useEkosGuider() && currentCCD->getChip(ISD::CameraChip::GUIDE_CCD) == guideChip)
        if (m_captureModuleState->getGuideState() == GUIDE_GUIDING && Options::guiderType() == 0 && suspendGuideOnDownload)
        {
            qCDebug(KSTARS_EKOS_CAPTURE) << "Autoguiding suspended until primary CCD chip completes downloading...";
            emit suspendGuiding();
        }

        captureStatusWidget->setStatus(i18n("Downloading..."), Qt::yellow);

        //This will start the clock to see how long the download takes.
        m_DownloadTimer.start();
        downloadProgressTimer.start();


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

void Capture::processCaptureError(ISD::Camera::ErrorType type)
{
    if (!activeJob)
        return;

    if (type == ISD::Camera::ERROR_CAPTURE)
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
    else
    {
        abort();
    }
}

void Capture::setActiveJob(SequenceJob *value)
{
    // do nothing if active job is not changed
    if (activeJob == value)
        return;

    // clear existing job connections
    if (activeJob != nullptr)
    {
        disconnect(this, nullptr, activeJob, nullptr);
        disconnect(activeJob, nullptr, this, nullptr);
        // ensure that the device adaptor does not send any new events
        activeJob->disconnectDeviceAdaptor();
    }

    // set the new value
    activeJob = value;
    // forward it to the module state
    m_captureModuleState->setActiveJob(value);

    // create job connections
    if (activeJob != nullptr)
    {
        // connect job with device adaptor events
        activeJob->connectDeviceAdaptor();
        // forward signals to the sequence job
        connect(this, &Capture::newGuiderDrift, activeJob, &SequenceJob::updateGuiderDrift);
        // react upon sequence job signals
        connect(activeJob, &SequenceJob::prepareState, this, &Capture::updatePrepareState);
        connect(activeJob, &SequenceJob::prepareComplete, this, [this](bool success)
        {
            if (success)
            {
                m_captureModuleState->setCaptureState(CAPTURE_PROGRESS);
                executeJob();
            }
            else
            {
                qWarning(KSTARS_EKOS_CAPTURE) << "Capture preparation failed, aborting.";
                m_captureModuleState->setCaptureState(CAPTURE_ABORTED);
                abort();
            }
        }, Qt::UniqueConnection);
        connect(activeJob, &SequenceJob::abortCapture, this, &Capture::abort);
        connect(activeJob, &SequenceJob::captureStarted, this, &Capture::captureStarted);
        connect(activeJob, &SequenceJob::newLog, this, &Capture::newLog);
        // forward the devices and attributes
        activeJob->setLightBox(m_captureDeviceAdaptor->getLightBox());
        activeJob->addMount(m_captureDeviceAdaptor->getMount());
        activeJob->setDome(m_captureDeviceAdaptor->getDome());
        activeJob->setDustCap(m_captureDeviceAdaptor->getDustCap());
        activeJob->setAutoFocusReady(m_captureModuleState->getRefocusState()->isAutoFocusReady());
    }
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
    // Update widget rotator position
    m_RotatorControlPanel->setCurrentRawAngle(value);
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
    job->setCoreProperty(SequenceJob::SJ_TargetADUTolerance, targetADUTolerance);
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

    if (m_captureDeviceAdaptor->getRotator() && m_RotatorControlPanel->isRotationEnforced())
    {
        job->setTargetRotation(m_RotatorControlPanel->targetPositionAngle());
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
            ignoreJobProgress = true;

        m_captureModuleState->allJobs().append(job);

        // Nothing more to do if preview
        if (preview)
            return true;
    }

    QJsonObject jsonJob = {{"Status", "Idle"}};

    auto placeholderPath = PlaceholderPath();
    placeholderPath.addJob(job, m_TargetName);

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

    if (m_JobUnderEdit && filenamePreview == NOT_PREVIEW)
    {
        m_JobUnderEdit = false;
        resetJobEdit();
        appendLogText(i18n("Job #%1 changes applied.", currentRow + 1));

        m_SequenceArray.replace(currentRow, jsonJob);
        emit sequenceChanged(m_SequenceArray);
    }

    QString signature = placeholderPath.generateFilename(*job, m_TargetName, filenamePreview != REMOTE_PREVIEW, true, 1,
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
    m_SequenceArray.removeAt(index);
    emit sequenceChanged(m_SequenceArray);

    if (m_captureModuleState->allJobs().empty())
        return true;

    SequenceJob * job = m_captureModuleState->allJobs().at(index);
    m_captureModuleState->allJobs().removeOne(job);
    if (job == activeJob)
        setActiveJob(nullptr);

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

    m_Dirty = true;

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

    QJsonObject currentJob = m_SequenceArray[currentRow].toObject();
    m_SequenceArray.replace(currentRow, m_SequenceArray[destinationRow]);
    m_SequenceArray.replace(destinationRow, currentJob);
    emit sequenceChanged(m_SequenceArray);

    queueTable->selectRow(destinationRow);

    for (int i = 0; i < m_captureModuleState->allJobs().count(); i++)
        m_captureModuleState->allJobs().at(i)->setStatusCell(queueTable->item(i, 0));

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

    SequenceJob * job = m_captureModuleState->allJobs().takeAt(currentRow);

    m_captureModuleState->allJobs().removeOne(job);
    m_captureModuleState->allJobs().insert(destinationRow, job);

    QJsonObject currentJob = m_SequenceArray[currentRow].toObject();
    m_SequenceArray.replace(currentRow, m_SequenceArray[destinationRow]);
    m_SequenceArray.replace(destinationRow, currentJob);
    emit sequenceChanged(m_SequenceArray);

    queueTable->selectRow(destinationRow);

    for (int i = 0; i < m_captureModuleState->allJobs().count(); i++)
        m_captureModuleState->allJobs().at(i)->setStatusCell(queueTable->item(i, 0));

    m_Dirty = true;
}

void Capture::setBusy(bool enable)
{
    isBusy = enable;

    previewB->setEnabled(!enable);
    loopB->setEnabled(!enable);
    opticalTrainCombo->setEnabled(!enable);
    trainB->setEnabled(!enable);

    foreach (QAbstractButton * button, queueEditButtonGroup->buttons())
        button->setEnabled(!enable);
}

void Capture::prepareJob(SequenceJob * job)
{
    setActiveJob(job);

    // If job is Preview and NO view is available, ask to enable it.
    // if job is batch job, then NO VIEW IS REQUIRED at all. It's optional.
    if (job->getCoreProperty(SequenceJob::SJ_Preview).toBool() && Options::useFITSViewer() == false
            && Options::useSummaryPreview() == false)
    {
        // ask if FITS viewer usage should be enabled
        connect(KSMessageBox::Instance(), &KSMessageBox::accepted, this, [ = ]()
        {
            KSMessageBox::Instance()->disconnect(this);
            Options::setUseFITSViewer(true);
            // restart
            prepareJob(job);
        });
        connect(KSMessageBox::Instance(), &KSMessageBox::rejected, this, [&]()
        {
            KSMessageBox::Instance()->disconnect(this);
            abort();
        });
        KSMessageBox::Instance()->questionYesNo(i18n("No view available for previews. Enable FITS viewer?"),
                                                i18n("Display preview"), 15);
        // do nothing because currently none of the previews is active.
        return;
    }

    if (m_isFraming == false)
        qCDebug(KSTARS_EKOS_CAPTURE) << "Preparing capture job" << job->getSignature() << "for execution.";

    int index = m_captureModuleState->allJobs().indexOf(job);
    if (index >= 0)
        queueTable->selectRow(index);

    if (activeJob->getCoreProperty(SequenceJob::SJ_Preview).toBool() == false)
    {
        // set the progress info
        imgProgress->setEnabled(true);
        imgProgress->setMaximum(activeJob->getCoreProperty(SequenceJob::SJ_Count).toInt());
        imgProgress->setValue(activeJob->getCompleted());

        if (m_captureDeviceAdaptor->getActiveCamera()->getUploadMode() != ISD::Camera::UPLOAD_LOCAL)
            updateSequencePrefix(activeJob->getCoreProperty(SequenceJob::SJ_FullPrefix).toString());

        // We check if the job is already fully or partially complete by checking how many files of its type exist on the file system
        // The signature is the unique identification path in the system for a particular job. Format is "<storage path>/<target>/<frame type>/<filter name>".
        // If the Scheduler is requesting the Capture tab to process a sequence job, a target name will be inserted after the sequence file storage field (e.g. /path/to/storage/target/Light/...)
        // If the end-user is requesting the Capture tab to process a sequence job, the sequence file storage will be used as is (e.g. /path/to/storage/Light/...)
        QString signature = activeJob->getSignature();

        // Now check on the file system ALL the files that exist with the above signature
        // If 29 files exist for example, then nextSequenceID would be the NEXT file number (30)
        // Therefore, we know how to number the next file.
        // However, we do not deduce the number of captures to process from this function.
        checkSeqBoundary();

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
            for (auto &a_job : m_captureModuleState->allJobs())
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

        // Check whether active job is complete by comparing required captures to what is already available
        if (activeJob->getCoreProperty(SequenceJob::SJ_Count).toInt() <= activeJob->getCompleted())
        {
            activeJob->setCompleted(activeJob->getCoreProperty(SequenceJob::SJ_Count).toInt());
            appendLogText(i18n("Job requires %1-second %2 images, has already %3/%4 captures and does not need to run.",
                               QString("%L1").arg(job->getCoreProperty(SequenceJob::SJ_Exposure).toDouble(), 0, 'f', 3),
                               job->getCoreProperty(SequenceJob::SJ_Filter).toString(),
                               activeJob->getCompleted(), activeJob->getCoreProperty(SequenceJob::SJ_Count).toInt()));
            processJobCompletionStage2();

            /* FIXME: find a clearer way to exit here */
            return;
        }
        else
        {
            // There are captures to process
            appendLogText(i18n("Job requires %1-second %2 images, has %3/%4 frames captured and will be processed.",
                               QString("%L1").arg(job->getCoreProperty(SequenceJob::SJ_Exposure).toDouble(), 0, 'f', 3),
                               job->getCoreProperty(SequenceJob::SJ_Filter).toString(),
                               activeJob->getCompleted(), activeJob->getCoreProperty(SequenceJob::SJ_Count).toInt()));

            // Emit progress update - done a few lines below
            // emit newImage(nullptr, activeJob);

            m_captureDeviceAdaptor->getActiveCamera()->setNextSequenceID(nextSequenceID);
        }
    }

    if (m_captureDeviceAdaptor->getActiveCamera()->isBLOBEnabled() == false)
    {
        // FIXME: Move this warning pop-up elsewhere, it will interfere with automation.
        //        if (Options::guiderType() != Ekos::Guide::GUIDE_INTERNAL || KMessageBox::questionYesNo(nullptr, i18n("Image transfer is disabled for this camera. Would you like to enable it?")) ==
        //                KMessageBox::Yes)
        if (Options::guiderType() != Guide::GUIDE_INTERNAL)
        {
            m_captureDeviceAdaptor->getActiveCamera()->setBLOBEnabled(true);
        }
        else
        {
            connect(KSMessageBox::Instance(), &KSMessageBox::accepted, this, [this]()
            {
                //QObject::disconnect(KSMessageBox::Instance(), &KSMessageBox::accepted, this, nullptr);
                KSMessageBox::Instance()->disconnect(this);
                m_captureDeviceAdaptor->getActiveCamera()->setBLOBEnabled(true);
                prepareActiveJobStage1();

            });
            connect(KSMessageBox::Instance(), &KSMessageBox::rejected, this, [this]()
            {
                //QObject::disconnect(KSMessageBox::Instance(), &KSMessageBox::rejected, this, nullptr);
                KSMessageBox::Instance()->disconnect(this);
                m_captureDeviceAdaptor->getActiveCamera()->setBLOBEnabled(true);
                setBusy(false);
            });

            KSMessageBox::Instance()->questionYesNo(i18n("Image transfer is disabled for this camera. Would you like to enable it?"),
                                                    i18n("Image Transfer"), 15);

            return;
        }
    }

    prepareActiveJobStage1();

}

void Capture::prepareActiveJobStage1()
{
    if (activeJob == nullptr)
    {
        qWarning(KSTARS_EKOS_CAPTURE) << "prepareActiveJobStage1 with null activeJob.";
    }
    else
    {
        // JM 2020-12-06: Check if we need to execute pre-job script first.
        const QString preJobScript = activeJob->getScript(SCRIPT_PRE_JOB);
        // Only run pre-job script for the first time and not after some images were captured but then stopped due to abort.
        if (!preJobScript.isEmpty() && activeJob->getCompleted() == 0)
        {
            m_CaptureScriptType = SCRIPT_PRE_JOB;
            m_CaptureScript.start(preJobScript, generateScriptArguments());
            appendLogText(i18n("Executing pre job script %1", preJobScript));
            return;
        }
    }
    prepareActiveJobStage2();
}

void Capture::prepareActiveJobStage2()
{
    // Just notification of active job stating up
    if (activeJob == nullptr)
    {
        qWarning(KSTARS_EKOS_CAPTURE) << "prepareActiveJobStage2 with null activeJob.";
    }
    else
        emit newImage(activeJob, m_ImageData);


    /* Disable this restriction, let the sequence run even if focus did not run prior to the capture.
     * Besides, this locks up the Scheduler when the Capture module starts a sequence without any prior focus procedure done.
     * This is quite an old code block. The message "Manual scheduled" seems to even refer to some manual intervention?
     * With the new HFR threshold, it might be interesting to prevent the execution because we actually need an HFR value to
     * begin capturing, but even there, on one hand it makes sense for the end-user to know what HFR to put in the edit box,
     * and on the other hand the focus procedure will deduce the next HFR automatically.
     * But in the end, it's not entirely clear what the intent was. Note there is still a warning that a preliminary autofocus
     * procedure is important to avoid any surprise that could make the whole schedule ineffective.
     */
    if (activeJob != nullptr)
    {
        const QString preCaptureScript = activeJob->getScript(SCRIPT_PRE_CAPTURE);
        // JM 2020-12-06: Check if we need to execute pre-capture script first.
        if (!preCaptureScript.isEmpty())
        {
            m_CaptureScriptType = SCRIPT_PRE_CAPTURE;
            m_CaptureScript.start(preCaptureScript, generateScriptArguments());
            appendLogText(i18n("Executing pre capture script %1", preCaptureScript));
            return;
        }
    }

    preparePreCaptureActions();
}

void Capture::preparePreCaptureActions()
{
    if (activeJob == nullptr)
    {
        qWarning(KSTARS_EKOS_CAPTURE) << "preparePreCaptureActions with null activeJob.";
        // Everything below depends on activeJob. Just return.
        return;
    }

    setBusy(true);

    if (activeJob->getCoreProperty(SequenceJob::SJ_Preview).toBool())
    {
        startB->setIcon(
            QIcon::fromTheme("media-playback-stop"));
        startB->setToolTip(i18n("Stop"));
    }

    // Update guiderActive before prepareCapture.
    activeJob->setCoreProperty(SequenceJob::SJ_GuiderActive, isActivelyGuiding());

    // signal that capture preparation steps should be executed
    activeJob->prepareCapture();
}

void Capture::updatePrepareState(CaptureState prepareState)
{
    m_captureModuleState->setCaptureState(prepareState);

    if (activeJob == nullptr)
    {
        qWarning(KSTARS_EKOS_CAPTURE) << "updatePrepareState with null activeJob.";
        // Everything below depends on activeJob. Just return.
        return;
    }

    switch (prepareState)
    {
        case CAPTURE_SETTING_TEMPERATURE:
            appendLogText(i18n("Setting temperature to %1 °C...", activeJob->getTargetTemperature()));
            captureStatusWidget->setStatus(i18n("Set Temp to %1 °C...", activeJob->getTargetTemperature()), Qt::yellow);
            break;
        case CAPTURE_GUIDER_DRIFT:
            appendLogText(i18n("Waiting for guide drift below %1\"...", activeJob->getTargetStartGuiderDrift()));
            captureStatusWidget->setStatus(i18n("Wait for Guider < %1\"...", activeJob->getTargetStartGuiderDrift()), Qt::yellow);
            break;

        case CAPTURE_SETTING_ROTATOR:
            appendLogText(i18n("Setting rotation to %1 degrees E of N...", activeJob->getTargetRotation()));
            captureStatusWidget->setStatus(i18n("Set Rotator to %1 deg...", activeJob->getTargetRotation()), Qt::yellow);
            break;

        default:
            break;

    }
}

void Capture::executeJob()
{
    if (activeJob == nullptr)
    {
        qWarning(KSTARS_EKOS_CAPTURE) << "executeJob with null activeJob.";
        return;
    }

    QList<FITSData::Record> FITSHeaders;
    if (m_ObserverName.isEmpty() == false)
        FITSHeaders.append(FITSData::Record("Observer", m_ObserverName, "Observer"));
    if (m_TargetName.isEmpty() == false)
        FITSHeaders.append(FITSData::Record("Object", m_TargetName, "Object"));

    if (!FITSHeaders.isEmpty())
        m_captureDeviceAdaptor->getActiveCamera()->setFITSHeaders(FITSHeaders);

    // Update button status
    setBusy(true);

    useGuideHead = (m_captureDeviceAdaptor->getActiveChip()->getType() == ISD::CameraChip::PRIMARY_CCD) ? false : true;

    syncGUIToJob(activeJob);

    // If the job is a dark flat, let's find the optimal exposure from prior
    // flat exposures.
    if (activeJob->getCoreProperty(SequenceJob::SJ_DarkFlat).toBool())
    {
        // If we found a prior exposure, and current upload more is not local, then update full prefix
        if (setDarkFlatExposure(activeJob)
                && m_captureDeviceAdaptor->getActiveCamera()->getUploadMode() != ISD::Camera::UPLOAD_LOCAL)
        {
            auto placeholderPath = PlaceholderPath();
            // Make sure to update Full Prefix as exposure value was changed
            placeholderPath.processJobInfo(activeJob, m_TargetName);
            updateSequencePrefix(activeJob->getCoreProperty(SequenceJob::SJ_FullPrefix).toString());
        }

    }

    updatePreCaptureCalibrationStatus();
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
        QTimer::singleShot(1000, this, &Capture::updatePreCaptureCalibrationStatus);
        return;
    }

    captureImage();
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

    // communicate the new guiding deviation
    emit newGuiderDrift(deviation_rms);
    // forward it to the state machine
    m_captureModuleState->setGuideDeviation(deviation_rms);

}

void Capture::setFocusStatus(FocusState state)
{
    // directly forward it to the state machine
    m_captureModuleState->updateFocusState(state);
    if (activeJob != nullptr)
        activeJob->setFocusStatus(state);
}

void Capture::updateFocusStatus(FocusState state)
{
    if ((m_captureModuleState->getRefocusState()->isRefocusing()
            || m_captureModuleState->getRefocusState()->isInSequenceFocus()) && activeJob && activeJob->getStatus() == JOB_BUSY)
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


int Capture::getTotalFramesCount(QString signature)
{
    int  result = 0;
    bool found  = false;

    foreach (SequenceJob * job, m_captureModuleState->allJobs())
    {
        // FIXME: this should be part of SequenceJob
        QString sig = job->getSignature();
        if (sig == signature)
        {
            result += job->getCoreProperty(SequenceJob::SJ_Count).toInt();
            found = true;
        }
    }

    if (found)
        return result;
    else
        return -1;
}

void Capture::setRotatorReversed(bool toggled)
{
    m_RotatorControlPanel->ReverseDirectionCheck->setEnabled(true);

    m_RotatorControlPanel->ReverseDirectionCheck->blockSignals(true);
    m_RotatorControlPanel->ReverseDirectionCheck->setChecked(toggled);
    m_RotatorControlPanel->ReverseDirectionCheck->blockSignals(false);

}

void Capture::setTargetName(const QString &name)
{
    if (m_captureModuleState->isCaptureRunning() == false)
    {
        m_TargetName = name;
        targetNameT->setText(m_TargetName);
        generatePreviewFilename();
    }
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

    m_SequenceURL = QUrl::fromLocalFile(fileURL);
    m_Dirty      = false;
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
        else if (!strcmp(tagXMLEle(ep), "Rotation"))
        {
            m_RotatorControlPanel->setRotationEnforced(true);
            m_RotatorControlPanel->setTargetPositionAngle(cLocale.toDouble(pcdataXMLEle(ep)));
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

    if (!foundPlaceholderFormat)
        placeholderFormatT->setText(PlaceholderPath::defaultFormat(FilterEnabled, ExpEnabled, TimeStampEnabled));

    addJob(false, isDarkFlat);

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
        m_SequenceURL = QFileDialog::getSaveFileUrl(Manager::Instance(), i18nc("@title:window", "Save Ekos Sequence Queue"),
                        dirPath,
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
    if (m_ObserverName.isEmpty() == false)
        outstream << "<Observer>" << m_ObserverName << "</Observer>" << Qt::endl;
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
    targetADUTolerance = job->getCoreProperty(SequenceJob::SJ_TargetADUTolerance).toDouble();
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

    if (job->getTargetRotation() != Ekos::INVALID_VALUE)
    {
        m_RotatorControlPanel->setRotationEnforced(true);
        m_RotatorControlPanel->setTargetPositionAngle(job->getTargetRotation());
    }
    else
        m_RotatorControlPanel->setRotationEnforced(false);

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

    if (isBusy)
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
    if (activeJob == nullptr)
        return -1;

    for (int i = 0; i < m_captureModuleState->allJobs().count(); i++)
    {
        if (activeJob == m_captureModuleState->allJobs().at(i))
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
    double estimatedDownloadTime = getEstimatedDownloadTime();

    foreach (SequenceJob * job, m_captureModuleState->allJobs())
        remaining += job->getJobRemainingTime(estimatedDownloadTime);

    return remaining;
}

int Capture::getActiveJobRemainingTime()
{
    if (activeJob == nullptr)
        return -1;

    return activeJob->getJobRemainingTime(getEstimatedDownloadTime());
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
    setActiveJob(nullptr);
    while (queueTable->rowCount() > 0)
        queueTable->removeRow(0);
    qDeleteAll(m_captureModuleState->allJobs());
    m_captureModuleState->allJobs().clear();

    while (m_SequenceArray.count())
        m_SequenceArray.pop_back();
    emit sequenceChanged(m_SequenceArray);
}

QString Capture::getSequenceQueueStatus()
{
    if (m_captureModuleState->allJobs().count() == 0)
        return "Invalid";

    if (isBusy)
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

bool Capture::checkPausing()
{
    if (m_captureModuleState->getCaptureState() == CAPTURE_PAUSE_PLANNED)
    {
        appendLogText(i18n("Sequence paused."));
        m_captureModuleState->setCaptureState(CAPTURE_PAUSED);
        // disconnect camera device
        connectCamera(false);
        // handle a requested meridian flip
        if (getMeridianFlipState()->getMeridianFlipStage() != MeridianFlipState::MF_NONE)
            updateMeridianFlipStage(MeridianFlipState::MF_READY);
        // pause
        return true;
    }
    // no pause
    return false;
}




void Capture::checkGuideDeviationTimeout()
{
    if (activeJob && activeJob->getStatus() == JOB_ABORTED && m_captureModuleState->isGuidingDeviationDetected())
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
    if (state != m_captureModuleState->getAlignState())
        qCDebug(KSTARS_EKOS_CAPTURE) << "Align State changed from" << Ekos::getAlignStatusString(
                                         m_captureModuleState->getAlignState()) << "to" << Ekos::getAlignStatusString(state);
    m_captureModuleState->setAlignState(state);

    getMeridianFlipState()->setResumeAlignmentAfterFlip(true);

    switch (state)
    {
        case ALIGN_COMPLETE:
            if (getMeridianFlipState()->getMeridianFlipStage() == MeridianFlipState::MF_ALIGNING)
            {
                appendLogText(i18n("Post flip re-alignment completed successfully."));
                m_captureModuleState->resetAlignmentRetries();
                // Trigger guiding if necessary.
                if (m_captureModuleState->checkGuidingAfterFlip() == false)
                {
                    // If no guiding is required, the meridian flip is complete
                    updateMeridianFlipStage(MeridianFlipState::MF_NONE);
                    m_captureModuleState->setCaptureState(CAPTURE_WAITING);
                }
            }
            break;

        case ALIGN_ABORTED:
        case ALIGN_FAILED:
            // TODO run it 3 times before giving up
            if (getMeridianFlipState()->getMeridianFlipStage() == MeridianFlipState::MF_ALIGNING)
            {
                if (m_captureModuleState->increaseAlignmentRetries() >= 3)
                {
                    appendLogText(i18n("Post-flip alignment failed."));
                    abort();
                }
                else
                {
                    appendLogText(i18n("Post-flip alignment failed. Retrying..."));

                    m_captureModuleState->setCaptureState(CAPTURE_ALIGNING);

                    updateMeridianFlipStage(MeridianFlipState::MF_ALIGNING);
                }
            }
            break;

        default:
            break;
    }
}

void Capture::setGuideStatus(GuideState state)
{
    if (state != m_captureModuleState->getGuideState())
        qCDebug(KSTARS_EKOS_CAPTURE) << "Guiding state changed from" << Ekos::getGuideStatusString(
                                         m_captureModuleState->getGuideState())
                                     << "to" << Ekos::getGuideStatusString(state);
    switch (state)
    {
        case GUIDE_IDLE:
            break;

        case GUIDE_GUIDING:
        case GUIDE_CALIBRATION_SUCCESS:
            autoGuideReady = true;
            break;

        case GUIDE_ABORTED:
        case GUIDE_CALIBRATION_ERROR:
            processGuidingFailed();
            m_captureModuleState->setGuideState(state);
            break;

        case GUIDE_DITHERING_SUCCESS:
            qCInfo(KSTARS_EKOS_CAPTURE) << "Dithering succeeded, capture state" << getCaptureStatusString(
                                            m_captureModuleState->getCaptureState());
            // do nothing if something happened during dithering
            appendLogText(i18n("Dithering succeeded."));
            if (m_captureModuleState->getCaptureState() != CAPTURE_DITHERING)
                break;

            if (Options::guidingSettle() > 0)
            {
                // N.B. Do NOT convert to i18np since guidingRate is DOUBLE value (e.g. 1.36) so we always use plural with that.
                appendLogText(i18n("Dither complete. Resuming in %1 seconds...", Options::guidingSettle()));
                QTimer::singleShot(Options::guidingSettle() * 1000, this, [this]()
                {
                    m_captureModuleState->setDitheringState(IPS_OK);
                });
            }
            else
            {
                appendLogText(i18n("Dither complete."));
                m_captureModuleState->setDitheringState(IPS_OK);
            }
            break;

        case GUIDE_DITHERING_ERROR:
            qCInfo(KSTARS_EKOS_CAPTURE) << "Dithering failed, capture state" << getCaptureStatusString(
                                            m_captureModuleState->getCaptureState());
            if (m_captureModuleState->getCaptureState() != CAPTURE_DITHERING)
                break;

            if (Options::guidingSettle() > 0)
            {
                // N.B. Do NOT convert to i18np since guidingRate is DOUBLE value (e.g. 1.36) so we always use plural with that.
                appendLogText(i18n("Warning: Dithering failed. Resuming in %1 seconds...", Options::guidingSettle()));
                // set dithering state to OK after settling time and signal to proceed
                QTimer::singleShot(Options::guidingSettle() * 1000, this, [this]()
                {
                    m_captureModuleState->setDitheringState(IPS_OK);
                });
            }
            else
            {
                appendLogText(i18n("Warning: Dithering failed."));
                // signal OK so that capturing may continue although dithering failed
                m_captureModuleState->setDitheringState(IPS_OK);
            }

            break;

        default:
            break;
    }

    m_captureModuleState->setGuideState(state);
    // forward it to the currently active sequence job
    if (activeJob != nullptr)
        activeJob->setCoreProperty(SequenceJob::SJ_GuiderActive, isActivelyGuiding());
}


void Capture::processGuidingFailed()
{
    if (m_captureModuleState->getFocusState() > FOCUS_PROGRESS)
    {
        appendLogText(i18n("Autoguiding stopped. Waiting for autofocus to finish..."));
    }
    // If Autoguiding was started before and now stopped, let's abort (unless we're doing a meridian flip)
    else if (m_captureModuleState->isGuidingOn()
             && getMeridianFlipState()->getMeridianFlipStage() == MeridianFlipState::MF_NONE &&
             // JM 2022.08.03: Only abort if the current job is LIGHT. For calibration frames, we can ignore guide failures.
             ((activeJob && activeJob->getStatus() == JOB_BUSY && activeJob->getFrameType() == FRAME_LIGHT) ||
              this->m_captureModuleState->getCaptureState() == CAPTURE_SUSPENDED
              || this->m_captureModuleState->getCaptureState() == CAPTURE_PAUSED))
    {
        appendLogText(i18n("Autoguiding stopped. Aborting..."));
        abort();
    }
    else if (getMeridianFlipState()->getMeridianFlipStage() == MeridianFlipState::MF_GUIDING)
    {
        if (m_captureModuleState->increaseAlignmentRetries() >= 3)
        {
            appendLogText(i18n("Post meridian flip calibration error. Aborting..."));
            abort();
        }
    }
    autoGuideReady = false;
}

void Capture::checkFrameType(int index)
{
    calibrationB->setEnabled(index != FRAME_LIGHT);
    generateDarkFlatsB->setEnabled(index != FRAME_LIGHT);
}

double Capture::setCurrentADU(double value)
{
    if (activeJob == nullptr)
    {
        qWarning(KSTARS_EKOS_CAPTURE) << "setCurrentADU with null activeJob.";
        // Nothing good to do here. Just don't crash.
        return value;
    }

    double nextExposure = 0;
    double targetADU    = activeJob->getCoreProperty(SequenceJob::SJ_TargetADU).toDouble();
    std::vector<double> coeff;

    // Check if saturated, then take shorter capture and discard value
    ExpRaw.append(activeJob->getCoreProperty(SequenceJob::SJ_Exposure).toDouble());
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

    // 2022.01.12 Put a hard limit to 180 seconds.
    // If it goes over this limit, the flat source is probably off.
    if (nextExposure == 0.0 || nextExposure > 180)
    {
        if (value < targetADU)
            nextExposure = activeJob->getCoreProperty(SequenceJob::SJ_Exposure).toDouble() * 1.25;
        else
            nextExposure = activeJob->getCoreProperty(SequenceJob::SJ_Exposure).toDouble() * .75;
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


IPState Capture::checkLightFramePendingTasks()
{
    // step 1: did one of the pending jobs fail or has the user aborted the capture?
    if (m_captureModuleState->getCaptureState() == CAPTURE_ABORTED)
        return IPS_ALERT;

    // step 2: check if pausing has been requested
    if (checkPausing() == true)
    {
        // resume with starting next exposure
        m_captureModuleState->setContinueAction(CaptureModuleState::CONTINUE_ACTION_NEXT_EXPOSURE);
        return IPS_BUSY;
    }

    // step 3: check if meridian flip is already running or ready for execution
    if (getMeridianFlipState()->checkMeridianFlipRunning() || m_captureModuleState->checkMeridianFlipReady())
        return IPS_BUSY;

    // step 4: check if post flip alignment is running
    if (m_captureModuleState->getCaptureState() == CAPTURE_ALIGNING || m_captureModuleState->checkAlignmentAfterFlip())
        return IPS_BUSY;

    // step 5: check if post flip guiding is running
    // MF_NONE is set as soon as guiding is running and the guide deviation is below the limit
    if (getMeridianFlipState()->getMeridianFlipStage() >= MeridianFlipState::MF_COMPLETED
            && m_captureModuleState->getGuideState() != GUIDE_GUIDING && m_captureModuleState->checkGuidingAfterFlip())
        return IPS_BUSY;

    // step 6: in case that a meridian flip has been completed and a guide deviation limit is set, we wait
    //         until the guide deviation is reported to be below the limit (@see setGuideDeviation(double, double)).
    //         Otherwise the meridian flip is complete
    if (m_captureModuleState->getCaptureState() == CAPTURE_CALIBRATING
            && getMeridianFlipState()->getMeridianFlipStage() == MeridianFlipState::MF_GUIDING)
    {
        if (Options::enforceGuideDeviation() || Options::enforceStartGuiderDrift())
            return IPS_BUSY;
        else
            updateMeridianFlipStage(MeridianFlipState::MF_NONE);
    }

    // step 7: check guide deviation for non meridian flip stages if the initial guide limit is set.
    //         Wait until the guide deviation is reported to be below the limit (@see setGuideDeviation(double, double)).
    if (m_captureModuleState->getCaptureState() == CAPTURE_PROGRESS &&
            m_captureModuleState->getGuideState() == GUIDE_GUIDING &&
            Options::enforceStartGuiderDrift())
        return IPS_BUSY;

    // step 8: check if dithering is required or running
    if ((m_captureModuleState->getCaptureState() == CAPTURE_DITHERING && m_captureModuleState->getDitheringState() != IPS_OK)
            || m_captureModuleState->checkDithering())
        return IPS_BUSY;

    // step 9: check if re-focusing is required
    //         Needs to be checked after dithering checks to avoid dithering in parallel
    //         to focusing, since @startFocusIfRequired() might change its value over time
    if ((m_captureModuleState->getCaptureState() == CAPTURE_FOCUSING && m_captureModuleState->checkFocusRunning())
            || m_captureModuleState->startFocusIfRequired())
        return IPS_BUSY;

    // step 10: resume guiding if it was suspended
    if (m_captureModuleState->getGuideState() == GUIDE_SUSPENDED)
    {
        appendLogText(i18n("Autoguiding resumed."));
        emit resumeGuiding();
        // No need to return IPS_BUSY here, we can continue immediately.
        // In the case that the capturing sequence has a guiding limit,
        // capturing will be interrupted by setGuideDeviation().
    }

    // everything is ready for capturing light frames
    return IPS_OK;

}


IPState Capture::processPreCaptureCalibrationStage()
{
    // in some rare cases it might happen that activeJob has been cleared by a concurrent thread
    if (activeJob == nullptr)
    {
        qCWarning(KSTARS_EKOS_CAPTURE) << "Processing pre capture calibration without active job, state = " <<
                                       getCaptureStatusString(m_captureModuleState->getCaptureState());
        return IPS_ALERT;
    }

    // If we are currently guide and the frame is NOT a light frame, then we shopld suspend.
    // N.B. The guide camera could be on its own scope unaffected but it doesn't hurt to stop
    // guiding since it is no longer used anyway.
    if (activeJob->getFrameType() != FRAME_LIGHT && m_captureModuleState->getGuideState() == GUIDE_GUIDING)
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
        case FRAME_FLAT:
        case FRAME_NONE:
            // no actions necessary
            break;
    }

    return IPS_OK;
}

bool Capture::processPostCaptureCalibrationStage()
{
    if (activeJob == nullptr)
    {
        qWarning(KSTARS_EKOS_CAPTURE) << "processPostCaptureCalibrationStage with null activeJob.";
        abort();
        return false;
    }

    // If there are no more images to capture, do not bother calculating next exposure
    if (activeJob->getCalibrationStage() == SequenceJobState::CAL_CALIBRATION_COMPLETE)
        if (activeJob && activeJob->getCoreProperty(SequenceJob::SJ_Count).toInt() <= activeJob->getCompleted())
            return true;

    // Check if we need to do flat field slope calculation if the user specified a desired ADU value
    if (activeJob->getFrameType() == FRAME_FLAT && activeJob->getFlatFieldDuration() == DURATION_ADU &&
            activeJob->getCoreProperty(SequenceJob::SJ_TargetADU).toDouble() > 0)
    {
        if (!m_ImageData.isNull())
        {
            double currentADU = m_ImageData->getADU();
            bool outOfRange = false, saturated = false;

            switch (m_ImageData->bpp())
            {
                case 8:
                    if (activeJob->getCoreProperty(SequenceJob::SJ_TargetADU).toDouble() > UINT8_MAX)
                        outOfRange = true;
                    else if (currentADU / UINT8_MAX > 0.95)
                        saturated = true;
                    break;

                case 16:
                    if (activeJob->getCoreProperty(SequenceJob::SJ_TargetADU).toDouble() > UINT16_MAX)
                        outOfRange = true;
                    else if (currentADU / UINT16_MAX > 0.95)
                        saturated = true;
                    break;

                case 32:
                    if (activeJob->getCoreProperty(SequenceJob::SJ_TargetADU).toDouble() > UINT32_MAX)
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
                                   QString::number(m_ImageData->bpp())
                                   , QString::number(activeJob->getCoreProperty(SequenceJob::SJ_TargetADU).toDouble(), 'f', 2)));
                abort();
                return false;
            }
            else if (saturated)
            {
                double nextExposure = activeJob->getCoreProperty(SequenceJob::SJ_Exposure).toDouble() * 0.1;
                nextExposure = qBound(captureExposureN->minimum(), nextExposure, captureExposureN->maximum());

                appendLogText(i18n("Current image is saturated (%1). Next exposure is %2 seconds.",
                                   QString::number(currentADU, 'f', 0), QString("%L1").arg(nextExposure, 0, 'f', 6)));

                activeJob->setCalibrationStage(SequenceJobState::CAL_CALIBRATION);
                activeJob->setCoreProperty(SequenceJob::SJ_Exposure, nextExposure);
                if (m_captureDeviceAdaptor->getActiveCamera()->getUploadMode() != ISD::Camera::UPLOAD_CLIENT)
                {
                    m_captureDeviceAdaptor->getActiveCamera()->setUploadMode(ISD::Camera::UPLOAD_CLIENT);
                }
                startNextExposure();
                return false;
            }

            double ADUDiff = fabs(currentADU - activeJob->getCoreProperty(SequenceJob::SJ_TargetADU).toDouble());

            // If it is within tolerance range of target ADU
            if (ADUDiff <= targetADUTolerance)
            {
                if (activeJob->getCalibrationStage() == SequenceJobState::CAL_CALIBRATION)
                {
                    appendLogText(
                        i18n("Current ADU %1 within target ADU tolerance range.", QString::number(currentADU, 'f', 0)));
                    m_captureDeviceAdaptor->getActiveCamera()->setUploadMode(activeJob->getUploadMode());
                    auto placeholderPath = PlaceholderPath();
                    // Make sure to update Full Prefix as exposure value was changed
                    placeholderPath.processJobInfo(activeJob, m_TargetName);
                    // Mark calibration as complete
                    activeJob->setCalibrationStage(SequenceJobState::CAL_CALIBRATION_COMPLETE);

                    // Must update sequence prefix as this step is only done in prepareJob
                    // but since the duration has now been updated, we must take care to update signature
                    // since it may include a placeholder for duration which would affect it.
                    if (m_captureDeviceAdaptor->getActiveCamera()
                            && m_captureDeviceAdaptor->getActiveCamera()->getUploadMode() != ISD::Camera::UPLOAD_LOCAL)
                        updateSequencePrefix(activeJob->getCoreProperty(SequenceJob::SJ_FullPrefix).toString());

                    startNextExposure();
                    return false;
                }

                return true;
            }

            double nextExposure = -1;

            // If value is saturated, try to reduce it to valid range first
            if (std::fabs(m_ImageData->getMax(0) - m_ImageData->getMin(0)) < 10)
                nextExposure = activeJob->getCoreProperty(SequenceJob::SJ_Exposure).toDouble() * 0.5;
            else
                nextExposure = setCurrentADU(currentADU);

            if (nextExposure <= 0 || std::isnan(nextExposure))
            {
                appendLogText(
                    i18n("Unable to calculate optimal exposure settings, please capture the flats manually."));
                abort();
                return false;
            }

            // Limit to minimum and maximum values
            nextExposure = qBound(captureExposureN->minimum(), nextExposure, captureExposureN->maximum());

            appendLogText(i18n("Current ADU is %1 Next exposure is %2 seconds.", QString::number(currentADU, 'f', 0),
                               QString("%L1").arg(nextExposure, 0, 'f', 6)));

            activeJob->setCalibrationStage(SequenceJobState::CAL_CALIBRATION);
            activeJob->setCoreProperty(SequenceJob::SJ_Exposure, nextExposure);
            //activeJob->setPreview(true);
            if (m_captureDeviceAdaptor->getActiveCamera()->getUploadMode() != ISD::Camera::UPLOAD_CLIENT)
            {
                m_captureDeviceAdaptor->getActiveCamera()->setUploadMode(ISD::Camera::UPLOAD_CLIENT);
            }

            startNextExposure();
            return false;
        }
    }

    activeJob->setCalibrationStage(SequenceJobState::CAL_CALIBRATION_COMPLETE);
    return true;
}

void Capture::setNewRemoteFile(QString file)
{
    appendLogText(i18n("Remote image saved to %1", file));
}

void Capture::scriptFinished(int exitCode, QProcess::ExitStatus status)
{
    Q_UNUSED(status)

    switch (m_CaptureScriptType)
    {
        case SCRIPT_PRE_CAPTURE:
            appendLogText(i18n("Pre capture script finished with code %1.", exitCode));
            if (activeJob && activeJob->getStatus() == JOB_IDLE)
                preparePreCaptureActions();
            else
                checkNextExposure();
            break;

        case SCRIPT_POST_CAPTURE:
            appendLogText(i18n("Post capture script finished with code %1.", exitCode));

            // If we're done, proceed to completion.
            if (activeJob == nullptr || activeJob->getCoreProperty(SequenceJob::SJ_Count).toInt() <= activeJob->getCompleted())
            {
                processJobCompletionStage1();
            }
            // Else check if meridian condition is met.
            else if (m_captureModuleState->checkMeridianFlipReady())
            {
                appendLogText(i18n("Processing meridian flip..."));
            }
            // Then if nothing else, just resume sequence.
            else
            {
                appendLogText(i18n("Resuming sequence..."));
                resumeSequence();
            }
            break;

        case SCRIPT_PRE_JOB:
            appendLogText(i18n("Pre job script finished with code %1.", exitCode));
            prepareActiveJobStage2();
            break;

        case SCRIPT_POST_JOB:
            appendLogText(i18n("Post job script finished with code %1.", exitCode));
            processJobCompletionStage2();
            break;
    }
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
            if (isBusy == false)
                startB->setEnabled(false);
            break;

        default:
            if (isBusy == false)
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


void Capture::setAlignResults(double orientation, double ra, double de, double pixscale)
{
    Q_UNUSED(orientation)
    Q_UNUSED(ra)
    Q_UNUSED(de)
    Q_UNUSED(pixscale)

    if (m_captureDeviceAdaptor->getRotator() == nullptr)
        return;

    m_RotatorControlPanel->refresh();
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
        if (activeJob)
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
    capturedFramesMap[signature] = static_cast<ushort>(count);
    qCDebug(KSTARS_EKOS_CAPTURE) <<
                                 QString("Client module indicates that storage for '%1' has already %2 captures processed.").arg(signature).arg(count);
    // Scheduler's captured frame map overrides the progress option of the Capture module
    ignoreJobProgress = false;
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

void Capture::processCaptureTimeout()
{
    m_CaptureTimeoutCounter++;

    if (m_DeviceRestartCounter >= 3)
    {
        m_CaptureTimeoutCounter = 0;
        m_DeviceRestartCounter = 0;
        appendLogText(i18n("Exposure timeout. Aborting..."));
        abort();
        return;
    }

    if (m_CaptureTimeoutCounter > 1 && m_captureDeviceAdaptor->getActiveCamera())
    {
        QString camera = m_captureDeviceAdaptor->getActiveCamera()->getDeviceName();
        QString fw = (m_captureDeviceAdaptor->getFilterWheel() != nullptr) ?
                     m_captureDeviceAdaptor->getFilterWheel()->getDeviceName() : "";
        emit driverTimedout(camera);
        QTimer::singleShot(5000, this, [ &, camera, fw]()
        {
            m_DeviceRestartCounter++;
            reconnectDriver(camera, fw);
        });
        return;
    }
    else
    {
        // Double check that m_Camera is valid in case it was reset due to driver restart.
        if (m_captureDeviceAdaptor->getActiveCamera())
        {
            appendLogText(i18n("Exposure timeout. Restarting exposure..."));
            m_captureDeviceAdaptor->getActiveCamera()->setEncodingFormat("FITS");
            ISD::CameraChip *targetChip = m_captureDeviceAdaptor->getActiveCamera()->getChip(useGuideHead ? ISD::CameraChip::GUIDE_CCD :
                                          ISD::CameraChip::PRIMARY_CCD);
            targetChip->abortExposure();
            targetChip->capture(captureExposureN->value());
            captureTimeout.start(static_cast<int>(captureExposureN->value() * 1000 + CAPTURE_TIMEOUT_THRESHOLD));
        }
        else
        {
            qCDebug(KSTARS_EKOS_CAPTURE) << "Unable to restart exposure as camera is missing, trying again in 5 seconds...";
            QTimer::singleShot(5000, this, &Capture::processCaptureTimeout);
        }
    }
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
        if (activeJob != nullptr)
            activeJob->addMount(nullptr);
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
    }

    // Light Boxes
    if (m_LightBox && m_LightBox->getDeviceName() == device->getDeviceName())
    {
        m_LightBox->disconnect(this);
        m_LightBox = nullptr;
        m_captureDeviceAdaptor->setLightBox(nullptr);
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

double Capture::getEstimatedDownloadTime()
{
    double total = std::accumulate(downloadTimes.begin(), downloadTimes.end(), 0.0);
    if(downloadTimes.empty())
        return 0;
    else
        return total / downloadTimes.count();
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
        m_CaptureTimeoutCounter = 0;

        if (activeJob)
        {
            m_captureDeviceAdaptor->setActiveChip(m_captureDeviceAdaptor->getActiveChip());
            captureImage();
        }

        return;
    }

    QTimer::singleShot(5000, this, [ &, camera, filterWheel]()
    {
        reconnectDriver(camera, filterWheel);
    });
}

bool Capture::isActivelyGuiding()
{
    return m_captureModuleState->isGuidingOn() && (m_captureModuleState->getGuideState() == GUIDE_GUIDING);
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

QStringList Capture::generateScriptArguments() const
{
    // TODO based on user feedback on what paramters are most useful to pass
    return QStringList();
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

bool Capture::setDarkFlatExposure(SequenceJob *job)
{
    const auto darkFlatFilter = job->getCoreProperty(SequenceJob::SJ_Filter).toString();
    const auto darkFlatBinning = job->getCoreProperty(SequenceJob::SJ_Binning).toPoint();
    const auto darkFlatADU = job->getCoreProperty(SequenceJob::SJ_TargetADU).toInt();

    for (auto &oneJob : m_captureModuleState->allJobs())
    {
        if (oneJob->getFrameType() != FRAME_FLAT)
            continue;

        const auto filter = oneJob->getCoreProperty(SequenceJob::SJ_Filter).toString();

        // Match filter, if one exists.
        if (!darkFlatFilter.isEmpty() && darkFlatFilter != filter)
            continue;

        // Match binning
        const auto binning = oneJob->getCoreProperty(SequenceJob::SJ_Binning).toPoint();
        if (darkFlatBinning != binning)
            continue;

        // Match ADU, if used.
        const auto adu = oneJob->getCoreProperty(SequenceJob::SJ_TargetADU).toInt();
        if (job->getFlatFieldDuration() == DURATION_ADU)
        {
            if (darkFlatADU != adu)
                continue;
        }

        // Now get the exposure
        job->setCoreProperty(SequenceJob::SJ_Exposure, oneJob->getCoreProperty(SequenceJob::SJ_Exposure).toDouble());

        return true;
    }

    return false;
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
    refreshOpticalTrain();
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
            id = OpticalTrainManager::Instance()->id(opticalTrainCombo->itemText(0));
            ProfileSettings::Instance()->setOneSetting(ProfileSettings::CaptureOpticalTrain, id);
        }

        auto name = OpticalTrainManager::Instance()->name(id);

        opticalTrainCombo->setCurrentText(name);

        auto camera = OpticalTrainManager::Instance()->getCamera(name);
        if (camera)
        {
            auto scope = OpticalTrainManager::Instance()->getScope(name);
            opticalTrainCombo->setToolTip(QString("%1 @ %2").arg(camera->getDeviceName(), scope["name"].toString()));
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

        auto mount = OpticalTrainManager::Instance()->getMount(name);
        setMount(mount);
    }

    opticalTrainCombo->blockSignals(false);
}

void Capture::generatePreviewFilename()
{
    if (m_captureModuleState->getCaptureState() == CAPTURE_IDLE || m_captureModuleState->getCaptureState() == CAPTURE_ABORTED
            || m_captureModuleState->getCaptureState() == CAPTURE_COMPLETE)
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
    //    else if (m_SequenceURL.toLocalFile().isEmpty() && (m_format.contains("%d") || m_format.contains("%p")
    //             || m_format.contains("%f")))
    else if (m_SequenceURL.toLocalFile().isEmpty() && m_format.contains("%f"))
        previewText = ("Save the sequence file to show filename preview");
    else if (addJob(true, false, previewType) == true)
    {
        QString previewSeq;
        if (m_SequenceURL.toLocalFile().isEmpty())
        {
            if (m_format.startsWith(separator))
                previewSeq = m_format.left(m_format.lastIndexOf(separator));
        }
        else
            previewSeq = m_SequenceURL.toLocalFile();
        auto m_placeholderPath = PlaceholderPath(previewSeq);
        auto m_job = m_captureModuleState->allJobs().last();

        QString extension = ".fits";
        if (captureEncodingS->currentText() == "XISF")
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

}
