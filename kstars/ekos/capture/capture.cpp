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
#include "skymap.h"
#include "ui_calibrationoptions.h"
#include "auxiliary/QProgressIndicator.h"
#include "auxiliary/ksmessagebox.h"
#include "ekos/manager.h"
#include "ekos/auxiliary/darklibrary.h"
#include "scriptsmanager.h"
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
#define SQ_FORMAT_VERSION 2.3
// We accept file formats with version back to:
#define SQ_COMPAT_VERSION 2.0

// Qt version calming
#include <qtendl.h>

namespace Ekos
{
Capture::Capture()
{
    setupUi(this);

    qRegisterMetaType<Ekos::CaptureState>("Ekos::CaptureState");
    qDBusRegisterMetaType<Ekos::CaptureState>();

    new CaptureAdaptor(this);
    m_captureDeviceAdaptor.reset(new CaptureDeviceAdaptor());
    m_captureState.reset(new SequenceJobState::CaptureState());

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

    rotatorSettings.reset(new RotatorSettings(this));

    // hide avg. download time and target drift initially
    targetDriftLabel->setVisible(false);
    targetDrift->setVisible(false);
    targetDriftUnit->setVisible(false);
    avgDownloadTime->setVisible(false);
    avgDownloadLabel->setVisible(false);
    secLabel->setVisible(false);
    connect(rotatorSettings->setAngleB, &QPushButton::clicked, this, [this]()
    {
        double angle = rotatorSettings->angleSpin->value();
        m_captureDeviceAdaptor->setRotatorAngle(&angle);
    });
    connect(rotatorSettings->setPAB, &QPushButton::clicked, this, [this]()
    {
        // 1. PA = (RawAngle * Multiplier) - Offset
        // 2. Offset = (RawAngle * Multiplier) - PA
        // 3. RawAngle = (Offset + PA) / Multiplier
        double rawAngle = (rotatorSettings->PAOffsetSpin->value() + rotatorSettings->PASpin->value()) /
                          rotatorSettings->PAMulSpin->value();
        while (rawAngle < 0)
            rawAngle += 360;
        while (rawAngle > 360)
            rawAngle -= 360;

        rotatorSettings->angleSpin->setValue(rawAngle);
        m_captureDeviceAdaptor->setRotatorAngle(&rawAngle);
    });
    connect(rotatorSettings->ReverseDirectionCheck, &QCheckBox::toggled, this, [this](bool toggled)
    {
        m_captureDeviceAdaptor->reverseRotator(toggled);
    });

    seqFileCount = 0;
    seqDelayTimer = new QTimer(this);
    connect(seqDelayTimer, &QTimer::timeout, this, &Ekos::Capture::captureImage);
    captureDelayTimer = new QTimer(this);
    connect(captureDelayTimer, &QTimer::timeout, this, &Ekos::Capture::start);

    connect(startB, &QPushButton::clicked, this, &Ekos::Capture::toggleSequence);
    connect(pauseB, &QPushButton::clicked, this, &Ekos::Capture::pause);
    connect(darkLibraryB, &QPushButton::clicked, DarkLibrary::Instance(), &QDialog::show);
    connect(limitsB, &QPushButton::clicked, m_LimitsDialog, &QDialog::show);
    connect(temperatureRegulationB, &QPushButton::clicked, this, &Ekos::Capture::showTemperatureRegulation);

    startB->setIcon(QIcon::fromTheme("media-playback-start"));
    startB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    pauseB->setIcon(QIcon::fromTheme("media-playback-pause"));
    pauseB->setAttribute(Qt::WA_LayoutUsesWidgetRect);

    filterManagerB->setIcon(QIcon::fromTheme("view-filter"));
    filterManagerB->setAttribute(Qt::WA_LayoutUsesWidgetRect);

    filterWheelS->addItem("--");

    connect(captureBinHN, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), captureBinVN, &QSpinBox::setValue);

#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
    connect(cameraS, static_cast<void (QComboBox::*)(const QString &)>(&QComboBox::activated), this,
            &Ekos::Capture::setDefaultCCD);
#else
    connect(cameraS, static_cast<void (QComboBox::*)(const QString &)>(&QComboBox::textActivated), this,
            &Ekos::Capture::setDefaultCCD);
#endif
    connect(cameraS, static_cast<void (QComboBox::*)(int)>(&QComboBox::activated), this, &Ekos::Capture::checkCCD);

    connect(liveVideoB, &QPushButton::clicked, this, &Ekos::Capture::toggleVideo);

    guideDeviationTimer.setInterval(GD_TIMER_TIMEOUT);
    connect(&guideDeviationTimer, &QTimer::timeout, this, &Ekos::Capture::checkGuideDeviationTimeout);

    connect(clearConfigurationB, &QPushButton::clicked, this, &Ekos::Capture::clearCameraConfiguration);

    darkB->setChecked(Options::autoDark());
    connect(darkB, &QAbstractButton::toggled, this, [this]()
    {
        Options::setAutoDark(darkB->isChecked());
    });

#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
    connect(filterWheelS, static_cast<void (QComboBox::*)(const QString &)>(&QComboBox::activated), this,
            &Ekos::Capture::setDefaultFilterWheel);
#else
    connect(filterWheelS, static_cast<void (QComboBox::*)(const QString &)>(&QComboBox::textActivated), this,
            &Ekos::Capture::setDefaultFilterWheel);
#endif
    connect(filterWheelS, static_cast<void (QComboBox::*)(int)>(&QComboBox::activated), this,
            &Ekos::Capture::checkFilter);

    connect(restartCameraB, &QPushButton::clicked, [this]()
    {
        restartCamera(cameraS->currentText());
    });

    connect(cameraTemperatureS, &QCheckBox::toggled, [this](bool toggled)
    {
        if (m_captureDeviceAdaptor->getActiveCCD())
        {
            QVariantMap auxInfo = m_captureDeviceAdaptor->getActiveCCD()->getDriverInfo()->getAuxInfo();
            auxInfo[QString("%1_TC").arg(m_captureDeviceAdaptor->getActiveCCD()->getDeviceName())] = toggled;
            m_captureDeviceAdaptor->getActiveCCD()->getDriverInfo()->setAuxInfo(auxInfo);
        }
    });

    connect(filterEditB, &QPushButton::clicked, this, &Ekos::Capture::editFilterName);

    connect(captureFilterS, static_cast<void(QComboBox::*)(const QString &)>(&QComboBox::currentTextChanged),
            [ = ]()
    {
        updateHFRThreshold();
    });
    connect(previewB, &QPushButton::clicked, this, &Ekos::Capture::captureOne);
    connect(loopB, &QPushButton::clicked, this, &Ekos::Capture::startFraming);

    //connect( seqWatcher, SIGNAL(dirty(QString)), this, &Ekos::Capture::checkSeqFile(QString)));

    connect(addToQueueB, &QPushButton::clicked, this, &Ekos::Capture::addSequenceJob);
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
    connect(setTemperatureB, &QPushButton::clicked, this, [&]()
    {
        if (m_captureDeviceAdaptor->getActiveCCD())
            m_captureDeviceAdaptor->getActiveCCD()->setTemperature(cameraTemperatureN->value());
    });
    connect(coolerOnB, &QPushButton::clicked, this, [&]()
    {
        if (m_captureDeviceAdaptor->getActiveCCD())
            m_captureDeviceAdaptor->getActiveCCD()->setCoolerControl(true);
    });
    connect(coolerOffB, &QPushButton::clicked, this, [&]()
    {
        if (m_captureDeviceAdaptor->getActiveCCD())
            m_captureDeviceAdaptor->getActiveCCD()->setCoolerControl(false);
    });
    connect(cameraTemperatureN, &QDoubleSpinBox::editingFinished, setTemperatureB,
            static_cast<void (QPushButton::*)()>(&QPushButton::setFocus));
    connect(captureTypeS, static_cast<void (QComboBox::*)(int)>(&QComboBox::activated), this, &Ekos::Capture::checkFrameType);
    connect(resetFrameB, &QPushButton::clicked, this, &Ekos::Capture::resetFrame);
    connect(calibrationB, &QPushButton::clicked, this, &Ekos::Capture::openCalibrationDialog);
    connect(rotatorB, &QPushButton::clicked, rotatorSettings.get(), &Ekos::Capture::show);

    connect(generateDarkFlatsB, &QPushButton::clicked, this, &Ekos::Capture::generateDarkFlats);
    connect(scriptManagerB, &QPushButton::clicked, this, &Ekos::Capture::handleScriptsManager);

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

    fileDirectoryT->setText(Options::fitsDir());

    ////////////////////////////////////////////////////////////////////////
    /// Device Adaptor
    ////////////////////////////////////////////////////////////////////////
    connect(m_captureDeviceAdaptor.data(), &Ekos::CaptureDeviceAdaptor::newCCDTemperatureValue, this,
            &Ekos::Capture::updateCCDTemperature, Qt::UniqueConnection);
    connect(m_captureDeviceAdaptor.data(), &Ekos::CaptureDeviceAdaptor::newRotatorAngle, this,
            &Ekos::Capture::updateRotatorAngle, Qt::UniqueConnection);

    ////////////////////////////////////////////////////////////////////////
    /// Settings
    ////////////////////////////////////////////////////////////////////////
    // #0 Start Guide Deviation Check
    m_LimitsUI->startGuiderDriftS->setChecked(Options::enforceStartGuiderDrift());
    connect(m_LimitsUI->startGuiderDriftS, &QCheckBox::toggled, [ = ](bool checked)
    {
        Options::setEnforceStartGuiderDrift(checked);
    });

    // #1 Abort Guide Deviation Check
    m_LimitsUI->limitGuideDeviationS->setChecked(Options::enforceGuideDeviation());
    connect(m_LimitsUI->limitGuideDeviationS, &QCheckBox::toggled, [ = ](bool checked)
    {
        Options::setEnforceGuideDeviation(checked);
    });

    // #2 Guide Deviation Value
    m_LimitsUI->limitGuideDeviationN->setValue(Options::guideDeviation());
    connect(m_LimitsUI->limitGuideDeviationN, &QDoubleSpinBox::editingFinished, [ = ]()
    {
        Options::setGuideDeviation(m_LimitsUI->limitGuideDeviationN->value());
    });

    // 3. Autofocus HFR Check
    m_LimitsUI->limitFocusHFRS->setChecked(Options::enforceAutofocus());
    connect(m_LimitsUI->limitFocusHFRS, &QCheckBox::toggled, [ = ](bool checked)
    {
        Options::setEnforceAutofocus(checked);
        if (checked == false)
            isInSequenceFocus = false;
    });

    // 4. Autofocus HFR Deviation
    m_LimitsUI->limitFocusHFRN->setValue(Options::hFRDeviation());
    connect(m_LimitsUI->limitFocusHFRN, &QDoubleSpinBox::editingFinished, [ = ]()
    {
        Options::setHFRDeviation(m_LimitsUI->limitFocusHFRN->value());
    });

    // 5. Autofocus temperature Check
    m_LimitsUI->limitFocusDeltaTS->setChecked(Options::enforceAutofocusOnTemperature());
    connect(m_LimitsUI->limitFocusDeltaTS, &QCheckBox::toggled, [ = ](bool checked)
    {
        Options::setEnforceAutofocusOnTemperature(checked);
        if (checked == false)
            isTemperatureDeltaCheckActive = false;
    });

    // 6. Autofocus temperature Delta
    m_LimitsUI->limitFocusDeltaTN->setValue(Options::maxFocusTemperatureDelta());
    connect(m_LimitsUI->limitFocusDeltaTN, &QDoubleSpinBox::editingFinished, [ = ]()
    {
        Options::setMaxFocusTemperatureDelta(m_LimitsUI->limitFocusDeltaTN->value());
    });

    // 7. Refocus Every Check
    m_LimitsUI->limitRefocusS->setChecked(Options::enforceRefocusEveryN());
    connect(m_LimitsUI->limitRefocusS, &QCheckBox::toggled, [ = ](bool checked)
    {
        Options::setEnforceRefocusEveryN(checked);
    });

    // 8. Refocus Every Value
    m_LimitsUI->limitRefocusN->setValue(static_cast<int>(Options::refocusEveryN()));
    connect(m_LimitsUI->limitRefocusN, &QDoubleSpinBox::editingFinished, [ = ]()
    {
        Options::setRefocusEveryN(static_cast<uint>(m_LimitsUI->limitRefocusN->value()));
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

    // 12. Refocus after meridian flip
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
        connect(control, &QCheckBox::toggled, this, &Ekos::Capture::setDirty);

    QDoubleSpinBox * const dspinBoxes[]
    {
        m_LimitsUI->limitFocusHFRN,
        m_LimitsUI->limitFocusDeltaTN,
        m_LimitsUI->limitGuideDeviationN,
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
    connect(&m_CaptureScript, static_cast<void (QProcess::*)(int exitCode, QProcess::ExitStatus status)>(&QProcess::finished),
            this, &Ekos::Capture::scriptFinished);
    connect(&m_CaptureScript, &QProcess::errorOccurred,
            [this](QProcess::ProcessError error)
    {
        Q_UNUSED(error)
        appendLogText(m_CaptureScript.errorString());
        scriptFinished(-1, QProcess::NormalExit);
    });
    connect(&m_CaptureScript, &QProcess::readyReadStandardError,
            [this]()
    {
        appendLogText(m_CaptureScript.readAllStandardError());
    });
    connect(&m_CaptureScript, &QProcess::readyReadStandardOutput,
            [this]()
    {
        appendLogText(m_CaptureScript.readAllStandardOutput());
    });

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

    DarkLibrary::Instance()->setCaptureModule(this);
    m_DarkProcessor = new DarkProcessor(this);
    connect(m_DarkProcessor, &DarkProcessor::newLog, this, &Ekos::Capture::appendLogText);
    connect(m_DarkProcessor, &DarkProcessor::darkFrameCompleted, this, &Ekos::Capture::setCaptureComplete);

    // display the capture status in the UI
    connect(this, &Ekos::Capture::newStatus, captureStatusWidget, &Ekos::CaptureStatusWidget::setCaptureState);

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

    DarkLibrary::Instance()->addCamera(newCCD);

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

void Capture::setDome(ISD::GDInterface *device)
{
    // forward it to the command processor
    if (! m_captureDeviceAdaptor.isNull())
        m_captureDeviceAdaptor->setDome(dynamic_cast<ISD::Dome *>(device));
}

void Capture::setDustCap(ISD::GDInterface *device)
{
    // forward it to the command processor
    if (! m_captureDeviceAdaptor.isNull())
        m_captureDeviceAdaptor->setDustCap(dynamic_cast<ISD::DustCap *>(device));

    syncFilterInfo();
}

void Capture::setLightBox(ISD::GDInterface *device)
{
    ISD::LightBox *lightBox = dynamic_cast<ISD::LightBox *>(device);
    // forward it to the command processor
    if (! m_captureDeviceAdaptor.isNull())
        m_captureDeviceAdaptor->setLightBox(lightBox);
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
    if (name == "Mount" && mountInterface == nullptr)
    {
        qCDebug(KSTARS_EKOS_CAPTURE) << "Registering new Module (" << name << ")";
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
    //    if (darkSubCheck->isChecked())
    //    {
    //        KSNotification::error(i18n("Auto dark subtract is not supported in batch mode."));
    //        return;
    //    }

    // Reset progress option if there is no captured frame map set at the time of start - fixes the end-user setting the option just before starting
    ignoreJobProgress = !capturedFramesMap.count() && Options::alwaysResetSequenceWhenStarting();

    if (queueTable->rowCount() == 0)
    {
        if (addJob() == false)
            return;
    }

    SequenceJob * first_job = nullptr;

    for (auto &job : jobs)
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
        for (auto &job : jobs)
        {
            if (job->getStatus() != JOB_DONE)
            {
                // If we arrived here with a zero-delay timer, raise the interval before returning to avoid a cpu peak
                if (captureDelayTimer->isActive())
                {
                    if (captureDelayTimer->interval() <= 0)
                        captureDelayTimer->setInterval(1000);
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
        for (auto &job : jobs)
            job->resetStatus();

        first_job = jobs.first();
    }
    // If we need to ignore job progress, systematically reset all jobs and restart
    // Scheduler will never ignore job progress and doesn't use this path
    else if (ignoreJobProgress)
    {
        appendLogText(i18n("Warning: option \"Always Reset Sequence When Starting\" is enabled and resets the sequence counts."));
        for (auto &job : jobs)
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
    m_SpikesDetected = 0;

    m_State = CAPTURE_PROGRESS;
    emit newStatus(m_State);

    startB->setIcon(QIcon::fromTheme("media-playback-stop"));
    startB->setToolTip(i18n("Stop Sequence"));
    pauseB->setEnabled(true);

    setBusy(true);

    if (m_LimitsUI->limitGuideDeviationS->isChecked() && autoGuideReady == false)
        appendLogText(i18n("Warning: Guide deviation is selected but autoguide process was not started."));
    if (m_LimitsUI->limitFocusHFRS->isChecked() && m_AutoFocusReady == false)
        appendLogText(i18n("Warning: in-sequence focusing is selected but autofocus process was not started."));
    if (m_LimitsUI->limitFocusDeltaTS->isChecked() && m_AutoFocusReady == false)
        appendLogText(i18n("Warning: temperature delta check is selected but autofocus process was not started."));

    if (m_captureDeviceAdaptor->getActiveCCD()
            && m_captureDeviceAdaptor->getActiveCCD()->getTelescopeType() != ISD::CCD::TELESCOPE_PRIMARY)
    {
        connect(KSMessageBox::Instance(), &KSMessageBox::accepted, this, [ = ]()
        {
            KSMessageBox::Instance()->disconnect(this);
            m_captureDeviceAdaptor->getActiveCCD()->setTelescopeType(ISD::CCD::TELESCOPE_PRIMARY);
            prepareJob(first_job);
        });
        connect(KSMessageBox::Instance(), &KSMessageBox::rejected, this, [ = ]()
        {
            KSMessageBox::Instance()->disconnect(this);
            prepareJob(first_job);
        });

        KSMessageBox::Instance()->questionYesNo(i18n("Are you imaging with %1 using your primary telescope?",
                                                m_captureDeviceAdaptor->getActiveCCD()->getDeviceName()),
                                                i18n("Telescope Type"), 10, true);
    }
    else
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
    captureDelayTimer->stop();

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
            KSNotification::event(QLatin1String("CaptureFailed"), stopText);
            appendLogText(stopText);
            activeJob->abort();
            if (activeJob->getCoreProperty(SequenceJob::SJ_Preview).toBool() == false)
            {
                int index = jobs.indexOf(activeJob);
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
            if (m_captureDeviceAdaptor->getActiveCCD())
                m_captureDeviceAdaptor->getActiveCCD()->setUploadMode(activeJob->getUploadMode());
        }
        // or regular preview job
        else
        {
            if (m_captureDeviceAdaptor->getActiveCCD())
                m_captureDeviceAdaptor->getActiveCCD()->setUploadMode(activeJob->getUploadMode());
            jobs.removeOne(activeJob);
            // Delete preview job
            activeJob->deleteLater();
            // Clear active job
            setActiveJob(nullptr);

            emit newStatus(targetState);
        }
    }

    // Only emit a new status if there is an active job or if capturing is suspended.
    // The latter is necessary since suspending clears the active job, but the Capture
    // module keeps the control.
    if (activeJob != nullptr || m_State == CAPTURE_SUSPENDED)
        emit newStatus(targetState);

    // stop focusing if capture is aborted
    if (m_State == CAPTURE_FOCUSING && targetState == CAPTURE_ABORTED)
        emit abortFocus();

    m_State = targetState;

    // Turn off any calibration light, IF they were turned on by Capture module
    if (m_captureDeviceAdaptor->getDustCap() && dustCapLightEnabled)
    {
        dustCapLightEnabled = false;
        m_captureDeviceAdaptor->getDustCap()->SetLightEnabled(false);
    }
    if (m_captureDeviceAdaptor->getLightBox() && lightBoxLightEnabled)
    {
        lightBoxLightEnabled = false;
        m_captureDeviceAdaptor->getLightBox()->SetLightEnabled(false);
    }

    disconnect(m_captureDeviceAdaptor->getActiveCCD(), &ISD::CCD::newImage, this, &Ekos::Capture::processData);
    disconnect(m_captureDeviceAdaptor->getActiveCCD(), &ISD::CCD::newExposureValue, this,  &Ekos::Capture::setExposureProgress);
    //    disconnect(currentCCD, &ISD::CCD::previewFITSGenerated, this, &Ekos::Capture::setGeneratedPreviewFITS);
    disconnect(m_captureDeviceAdaptor->getActiveCCD(), &ISD::CCD::ready, this, &Ekos::Capture::ready);

    // In case of exposure looping, let's abort
    if (m_captureDeviceAdaptor->getActiveCCD() &&
            m_captureDeviceAdaptor->getActiveChip() &&
            m_captureDeviceAdaptor->getActiveCCD()->isFastExposureEnabled())
        m_captureDeviceAdaptor->getActiveChip()->abortExposure();

    imgProgress->reset();
    imgProgress->setEnabled(false);

    frameRemainingTime->setText("--:--:--");
    jobRemainingTime->setText("--:--:--");
    frameInfoLabel->setText(i18n("Expose (-/-):"));
    m_isFraming = false;

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

    seqDelayTimer->stop();

    setActiveJob(nullptr);
    // meridian flip may take place if requested
    setMeridianFlipStage(MF_READY);
}

bool Capture::setCamera(const QString &device)
{
    // Do not change camera while in capture
    if (m_State == CAPTURE_CAPTURING)
        return false;

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
    if (m_captureDeviceAdaptor->getActiveCCD())
        return m_captureDeviceAdaptor->getActiveCCD()->getDeviceName();

    return QString();
}

void Capture::checkCCD(int ccdNum)
{
    // Do not update any camera settings while capture is in progress.
    if (m_State == CAPTURE_CAPTURING)
        return;

    if (ccdNum == -1)
    {
        ccdNum = cameraS->currentIndex();

        if (ccdNum == -1)
            return;
    }

    if (ccdNum < CCDs.count())
    {
        // Check whether main camera or guide head only
        ISD::CCD *currentCCD = CCDs.at(ccdNum);
        m_captureDeviceAdaptor->setActiveCCD(currentCCD);

        m_captureDeviceAdaptor->setActiveChip(nullptr);
        if (cameraS->itemText(ccdNum).right(6) == QString("Guider"))
        {
            useGuideHead = true;
            m_captureDeviceAdaptor->setActiveChip(currentCCD->getChip(ISD::CCDChip::GUIDE_CCD));
        }

        if (m_captureDeviceAdaptor->getActiveChip() == nullptr)
        {
            useGuideHead = false;
            m_captureDeviceAdaptor->setActiveChip(currentCCD->getChip(ISD::CCDChip::PRIMARY_CCD));
        }

        // Make sure we have a valid chip and valid base device.
        // Make sure we are not in capture process.
        ISD::CCDChip *targetChip = m_captureDeviceAdaptor->getActiveChip();
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
            disconnect(ccd, &ISD::CCD::error, this, &Ekos::Capture::processCaptureError);
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
        captureFormatS->addItems(currentCCD->getCaptureFormats());
        captureFormatS->setCurrentText(currentCCD->getCaptureFormat());
        captureFormatS->blockSignals(false);

        // Encoding format
        captureEncodingS->blockSignals(true);
        captureEncodingS->clear();
        captureEncodingS->addItems(currentCCD->getEncodingFormats());
        captureEncodingS->setCurrentText(currentCCD->getEncodingFormat());
        captureEncodingS->blockSignals(false);

        customPropertiesDialog->setCCD(currentCCD);

        liveVideoB->setEnabled(currentCCD->hasVideoStream());
        if (currentCCD->hasVideoStream())
            setVideoStreamEnabled(currentCCD->isStreamingEnabled());
        else
            liveVideoB->setIcon(QIcon::fromTheme("camera-off"));

        connect(currentCCD, &ISD::CCD::numberUpdated, this, &Ekos::Capture::processCCDNumber, Qt::UniqueConnection);
        connect(currentCCD, &ISD::CCD::coolerToggled, this, &Ekos::Capture::setCoolerToggled, Qt::UniqueConnection);
        connect(currentCCD, &ISD::CCD::newRemoteFile, this, &Ekos::Capture::setNewRemoteFile);
        connect(currentCCD, &ISD::CCD::videoStreamToggled, this, &Ekos::Capture::setVideoStreamEnabled);
        connect(currentCCD, &ISD::CCD::ready, this, &Ekos::Capture::ready);
        connect(currentCCD, &ISD::CCD::error, this, &Ekos::Capture::processCaptureError);

        syncCCDControls();

        // update values received by the device adaptor
        // connect(currentCCD, &ISD::CCD::newTemperatureValue, this, &Ekos::Capture::updateCCDTemperature, Qt::UniqueConnection);

        DarkLibrary::Instance()->checkCamera();
    }
}

void Capture::syncCCDControls()
{
    auto currentCCD = m_captureDeviceAdaptor->getActiveCCD();
    if (!currentCCD)
        return;

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
    captureISOS->blockSignals(false);

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

        connect(captureGainN, &QDoubleSpinBox::editingFinished, this, [this]()
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

        connect(captureOffsetN, &QDoubleSpinBox::editingFinished, this, [this]()
        {
            if (captureOffsetN->value() != OffsetSpinSpecialValue)
                setOffset(captureOffsetN->value());
        });
    }
    else
        captureOffsetN->setEnabled(false);
}

void Capture::setGuideChip(ISD::CCDChip * guideChip)
{
    // We should suspend guide in two scenarios:
    // 1. If guide chip is within the primary CCD, then we cannot download any data from guide chip while primary CCD is downloading.
    // 2. If we have two CCDs running from ONE driver (Multiple-Devices-Per-Driver mpdp is true). Same issue as above, only one download
    // at a time.
    // After primary CCD download is complete, we resume guiding.
    if (!m_captureDeviceAdaptor->getActiveCCD())
        return;

    suspendGuideOnDownload =
        (m_captureDeviceAdaptor->getActiveCCD()->getChip(ISD::CCDChip::GUIDE_CCD) == guideChip) ||
        (guideChip->getCCD() == m_captureDeviceAdaptor->getActiveCCD() &&
         m_captureDeviceAdaptor->getActiveCCD()->getDriverInfo()->getAuxInfo().value("mdpd", false).toBool());
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
    if (!m_captureDeviceAdaptor->getActiveCCD())
        return;

    int binx = 1, biny = 1;
    double min, max, step;
    int xstep = 0, ystep = 0;

    QString frameProp    = useGuideHead ? QString("GUIDER_FRAME") : QString("CCD_FRAME");
    QString exposureProp = useGuideHead ? QString("GUIDER_EXPOSURE") : QString("CCD_EXPOSURE");
    QString exposureElem = useGuideHead ? QString("GUIDER_EXPOSURE_VALUE") : QString("CCD_EXPOSURE_VALUE");
    m_captureDeviceAdaptor->setActiveChip(useGuideHead ? m_captureDeviceAdaptor->getActiveCCD()->getChip(
            ISD::CCDChip::GUIDE_CCD) :
                                          m_captureDeviceAdaptor->getActiveCCD()->getChip(ISD::CCDChip::PRIMARY_CCD));

    captureFrameWN->setEnabled(m_captureDeviceAdaptor->getActiveChip()->canSubframe());
    captureFrameHN->setEnabled(m_captureDeviceAdaptor->getActiveChip()->canSubframe());
    captureFrameXN->setEnabled(m_captureDeviceAdaptor->getActiveChip()->canSubframe());
    captureFrameYN->setEnabled(m_captureDeviceAdaptor->getActiveChip()->canSubframe());

    captureBinHN->setEnabled(m_captureDeviceAdaptor->getActiveChip()->canBin());
    captureBinVN->setEnabled(m_captureDeviceAdaptor->getActiveChip()->canBin());

    QList<double> exposureValues;
    exposureValues << 0.01 << 0.02 << 0.05 << 0.1 << 0.2 << 0.25 << 0.5 << 1 << 1.5 << 2 << 2.5 << 3 << 5 << 6 << 7 << 8 << 9 <<
                   10 << 20 << 30 << 40 << 50 << 60 << 120 << 180 << 300 << 600 << 900 << 1200 << 1800;

    if (m_captureDeviceAdaptor->getActiveCCD()->getMinMaxStep(exposureProp, exposureElem, &min, &max, &step))
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

    if (m_captureDeviceAdaptor->getActiveCCD()->getMinMaxStep(frameProp, "WIDTH", &min, &max, &step))
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

    if (m_captureDeviceAdaptor->getActiveCCD()->getMinMaxStep(frameProp, "HEIGHT", &min, &max, &step))
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

    if (m_captureDeviceAdaptor->getActiveCCD()->getMinMaxStep(frameProp, "X", &min, &max, &step))
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

    if (m_captureDeviceAdaptor->getActiveCCD()->getMinMaxStep(frameProp, "Y", &min, &max, &step))
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

void Capture::processCCDNumber(INumberVectorProperty * nvp)
{
    if (m_captureDeviceAdaptor->getActiveCCD() == nullptr)
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
    m_captureDeviceAdaptor->setActiveChip(useGuideHead ? m_captureDeviceAdaptor->getActiveCCD()->getChip(
            ISD::CCDChip::GUIDE_CCD) :
                                          m_captureDeviceAdaptor->getActiveCCD()->getChip(ISD::CCDChip::PRIMARY_CCD));
    m_captureDeviceAdaptor->getActiveChip()->resetFrame();
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
        m_captureDeviceAdaptor->setFilterWheel(nullptr);
        m_CurrentFilterPosition = -1;
        filterEditB->setEnabled(false);
        captureFilterS->clear();
        syncFilterInfo();
        return;
    }

    if (filterNum <= Filters.count())
        m_captureDeviceAdaptor->setFilterWheel(Filters.at(filterNum - 1));

    m_captureDeviceAdaptor->getFilterManager()->setCurrentFilterWheel(m_captureDeviceAdaptor->getFilterWheel());

    syncFilterInfo();

    captureFilterS->clear();

    captureFilterS->addItems(m_captureDeviceAdaptor->getFilterManager()->getFilterLabels());

    m_CurrentFilterPosition = m_captureDeviceAdaptor->getFilterManager()->getFilterPosition();

    filterEditB->setEnabled(m_CurrentFilterPosition > 0);

    captureFilterS->setCurrentIndex(m_CurrentFilterPosition - 1);


    /*if (activeJob &&
        (activeJob->getStatus() == JOB_ABORTED || activeJob->getStatus() == JOB_IDLE))
        activeJob->setCurrentFilter(currentFilterPosition);*/
}

void Capture::syncFilterInfo()
{
    QList<ISD::GDInterface *> devices;
    for (const auto &oneCamera : CCDs)
        devices.append(oneCamera);
    if (m_captureDeviceAdaptor->getDustCap())
        devices.append(m_captureDeviceAdaptor->getDustCap());

    for (const auto &oneDevice : devices)
    {
        auto activeDevices = oneDevice->getBaseDevice()->getText("ACTIVE_DEVICES");
        if (activeDevices)
        {
            auto activeFilter = activeDevices->findWidgetByName("ACTIVE_FILTER");
            if (activeFilter)
            {
                if (m_captureDeviceAdaptor->getFilterWheel() &&
                        (activeFilter->getText() != m_captureDeviceAdaptor->getFilterWheel()->getDeviceName()))
                {
                    Options::setDefaultFocusFilterWheel(m_captureDeviceAdaptor->getFilterWheel()->getDeviceName());
                    activeFilter->setText(m_captureDeviceAdaptor->getFilterWheel()->getDeviceName().toLatin1().constData());
                    oneDevice->getDriverInfo()->getClientManager()->sendNewText(activeDevices);
                }
                // Reset filter name in CCD driver
                else if (!m_captureDeviceAdaptor->getFilterWheel() && strlen(activeFilter->getText()) > 0)
                {
                    activeFilter->setText("");
                    oneDevice->getDriverInfo()->getClientManager()->sendNewText(activeDevices);
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
        m_State = CAPTURE_WAITING;
        emit newStatus(Ekos::CAPTURE_WAITING);
    }
    seqDelayTimer->start(seqDelay);

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


void Capture::processData(const QSharedPointer<FITSData> &data)
{
    ISD::CCDChip * tChip = nullptr;

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

    if (meridianFlipStage >= MF_ALIGNING)
    {
        if (data)
            qCWarning(KSTARS_EKOS_CAPTURE) << blobInfo << "Ignoring Received FITS as meridian flip stage is" << meridianFlipStage;
        return;
    }

    // If image is client or both, let's process it.
    if (m_captureDeviceAdaptor->getActiveCCD()
            && m_captureDeviceAdaptor->getActiveCCD()->getUploadMode() != ISD::CCD::UPLOAD_LOCAL)
    {
        //        if (data.isNull())
        //        {
        //            appendLogText(i18n("Failed to save file to %1", activeJob->getSignature()));
        //            abort();
        //            return;
        //        }

        if (m_State == CAPTURE_IDLE || m_State == CAPTURE_ABORTED)
        {
            qCWarning(KSTARS_EKOS_CAPTURE) << blobInfo << "Ignoring Received FITS as current capture state is not active" << m_State;
            return;
        }

        //if (!strcmp(data->name, "CCD2"))
        if (data)
        {
            tChip = m_captureDeviceAdaptor->getActiveCCD()->getChip(static_cast<ISD::CCDChip::ChipType>
                    (data->property("chip").toInt()));
            if (tChip != m_captureDeviceAdaptor->getActiveChip())
            {
                if (m_GuideState == GUIDE_IDLE)
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

        if (data && data->property("device").toString() != m_captureDeviceAdaptor->getActiveCCD()->getDeviceName())
        {
            qCWarning(KSTARS_EKOS_CAPTURE) << blobInfo << "Ignoring Received FITS as the blob device name does not equal active camera"
                                           << m_captureDeviceAdaptor->getActiveCCD()->getDeviceName();
            return;
        }

        // If this is a preview job, make sure to enable preview button after
        // we receive the FITS
        if (activeJob->getCoreProperty(SequenceJob::SJ_Preview).toBool() && previewB->isEnabled() == false)
            previewB->setEnabled(true);

        // If dark is selected, perform dark substraction.
        if (data && darkB->isChecked() && activeJob->getCoreProperty(SequenceJob::SJ_Preview).toBool() && useGuideHead == false)
        {
            m_DarkProcessor->denoise(m_captureDeviceAdaptor->getActiveChip(),
                                     m_ImageData,
                                     activeJob->getCoreProperty(SequenceJob::SJ_Exposure).toDouble(),
                                     activeJob->getCoreProperty(SequenceJob::SJ_ROI).toRect().x(),
                                     activeJob->getCoreProperty(SequenceJob::SJ_ROI).toRect().y());
            return;
        }
    }

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

    if (!activeJob)
        return IPS_BUSY;

    // In case we're framing, let's return quick to continue the process.
    if (m_isFraming)
    {
        emit newImage(activeJob, m_ImageData);
        // If fast exposure is on, do not capture again, it will be captured by the driver.
        if (m_captureDeviceAdaptor->getActiveCCD()->isFastExposureEnabled() == false)
        {
            captureStatusWidget->setStatus(i18n("Framing..."), Qt::darkGreen);
            activeJob->capture(m_AutoFocusReady, FITS_NORMAL);
        }
        return IPS_OK;
    }

    // If fast exposure is off, disconnect exposure progress
    // otherwise, keep it going since it fires off from driver continuous capture process.
    if (m_captureDeviceAdaptor->getActiveCCD()->isFastExposureEnabled() == false)
    {
        disconnect(m_captureDeviceAdaptor->getActiveCCD(), &ISD::CCD::newExposureValue, this, &Ekos::Capture::setExposureProgress);
        DarkLibrary::Instance()->disconnect(this);
    }

    // Do not calculate download time for images stored on server.
    // Only calculate for longer exposures.
    if (m_captureDeviceAdaptor->getActiveCCD()->getUploadMode() != ISD::CCD::UPLOAD_LOCAL && m_DownloadTimer.isValid())
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
                              KSNotification::EVENT_INFO);

    // If it was initially set as pure preview job and NOT as preview for calibration
    if (activeJob->getCoreProperty(SequenceJob::SJ_Preview).toBool())
    {
        //sendNewImage(blobFilename, blobChip);
        emit newImage(activeJob, m_ImageData);
        jobs.removeOne(activeJob);
        // Reset upload mode if it was changed by preview
        m_captureDeviceAdaptor->getActiveCCD()->setUploadMode(activeJob->getUploadMode());
        // Reset active job pointer
        setActiveJob(nullptr);
        abort();
        if (m_GuideState == GUIDE_SUSPENDED && suspendGuideOnDownload)
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

    if (! activeJob->getCoreProperty(SequenceJob::SJ_Preview).toBool()
            && activeJob->getCalibrationStage() != SequenceJobState::CAL_CALIBRATION)
    {
        /* Increase the sequence's current capture count */
        activeJob->setCompleted(activeJob->getCompleted() + 1);
        /* Decrease the counter for in-sequence focusing */
        if (inSequenceFocusCounter > 0)
            inSequenceFocusCounter--;
    }

    /* Decrease the dithering counter except for directly after meridian flip                                           */
    /* Hint: this happens only when a meridian flip happened during a paused sequence when pressing "Start" afterwards. */
    if (meridianFlipStage < MF_FLIPPING)
        ditherCounter--;

    // JM 2020-06-17: Emit newImage for LOCAL images (stored on remote host)
    //if (currentCCD->getUploadMode() == ISD::CCD::UPLOAD_LOCAL)
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

    m_State = CAPTURE_IMAGE_RECEIVED;
    emit newStatus(Ekos::CAPTURE_IMAGE_RECEIVED);

    if (activeJob)
    {
        QVariantMap metadata;
        metadata["filename"] = filename;
        metadata["type"] = activeJob->getFrameType();
        metadata["exposure"] = activeJob->getCoreProperty(SequenceJob::SJ_Exposure).toDouble();
        metadata["filter"] = activeJob->getCoreProperty(SequenceJob::SJ_Filter).toString();
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

/**
 * @brief Stop execution of the current sequence and check whether there exists a next sequence
 *        and start it, if there is a next one to be started (@see resumeSequence()).
 */
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
            int index = jobs.indexOf(activeJob);
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
                              KSNotification::EVENT_INFO);

        abort();

        m_State = CAPTURE_COMPLETE;
        emit newStatus(Ekos::CAPTURE_COMPLETE);

        //Resume guiding if it was suspended before
        //if (isAutoGuiding && currentCCD->getChip(ISD::CCDChip::GUIDE_CCD) == guideChip)
        if (m_GuideState == GUIDE_SUSPENDED && suspendGuideOnDownload)
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
    if (activeJob && activeJob->getCoreProperty(SequenceJob::SJ_Preview).toBool())
        return false;

    if ( (Options::ditherEnabled() || Options::ditherNoGuiding())
            // 2017-09-20 Jasem: No need to dither after post meridian flip guiding
            && meridianFlipStage != MF_GUIDING
            // We must be either in guide mode or if non-guide dither (via pulsing) is enabled
            && (m_GuideState == GUIDE_GUIDING || Options::ditherNoGuiding())
            // Must be only done for light frames
            && (activeJob != nullptr && activeJob->getFrameType() == FRAME_LIGHT)
            // Check dither counter
            && ditherCounter == 0)
    {
        ditherCounter = Options::ditherFrames();

        qCInfo(KSTARS_EKOS_CAPTURE) << "Dithering...";
        appendLogText(i18n("Dithering..."));

        m_State = CAPTURE_DITHERING;
        m_DitheringState = IPS_BUSY;
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

        for (auto &oneJob : jobs)
        {
            if (oneJob->getStatus() == JOB_IDLE || oneJob->getStatus() == JOB_ABORTED)
            {
                next_job = oneJob;
                break;
            }
        }

        if (next_job)
        {
            //check delta also when starting a new job!
            isTemperatureDeltaCheckActive = (m_AutoFocusReady && m_LimitsUI->limitFocusDeltaTS->isChecked());

            prepareJob(next_job);

            //Resume guiding if it was suspended before
            //if (isAutoGuiding && currentCCD->getChip(ISD::CCDChip::GUIDE_CCD) == guideChip)
            if (m_GuideState == GUIDE_SUSPENDED && suspendGuideOnDownload)
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
        isTemperatureDeltaCheckActive = (m_AutoFocusReady && m_LimitsUI->limitFocusDeltaTS->isChecked());

        // If we suspended guiding due to primary chip download, resume guide chip guiding now
        if (m_GuideState == GUIDE_SUSPENDED && suspendGuideOnDownload)
        {
            qCInfo(KSTARS_EKOS_CAPTURE) << "Resuming guiding...";
            emit resumeGuiding();
        }

        // If looping, we just increment the file system image count
        if (m_captureDeviceAdaptor->getActiveCCD()->isFastExposureEnabled())
        {
            if (m_captureDeviceAdaptor->getActiveCCD()->getUploadMode() != ISD::CCD::UPLOAD_LOCAL)
            {
                checkSeqBoundary(activeJob->getSignature());
                m_captureDeviceAdaptor->getActiveCCD()->setNextSequenceID(nextSequenceID);
            }
        }

        const QString preCaptureScript = activeJob->getScript(SCRIPT_PRE_CAPTURE);
        // JM 2020-12-06: Check if we need to execute pre-capture script first.
        if (!preCaptureScript.isEmpty())
        {
            if (m_captureDeviceAdaptor->getActiveCCD()->isFastExposureEnabled())
            {
                m_RememberFastExposure = true;
                m_captureDeviceAdaptor->getActiveCCD()->setFastExposureEnabled(false);
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
            if (m_captureDeviceAdaptor->getActiveCCD()->isFastExposureEnabled())
            {
                if (activeJob &&
                        activeJob->getFrameType() == FRAME_LIGHT &&
                        checkLightFramePendingTasks() == IPS_OK)
                {
                    // Continue capturing seamlessly
                    m_State = CAPTURE_CAPTURING;
                    emit newStatus(Ekos::CAPTURE_CAPTURING);
                    return IPS_OK;
                }

                // Stop fast exposure now.
                m_RememberFastExposure = true;
                m_captureDeviceAdaptor->getActiveCCD()->setFastExposureEnabled(false);
            }

            checkNextExposure();

        }
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
    if (activeJob == nullptr || activeJob->getFrameType() != FRAME_LIGHT
            || activeJob->getCoreProperty(SequenceJob::SJ_Preview).toBool())
        return false;

    isRefocus = false;
    isInSequenceFocus = (m_AutoFocusReady && m_LimitsUI->limitFocusHFRS->isChecked());

    // check if time for forced refocus
    if (m_LimitsUI->limitRefocusS->isChecked())
    {
        qCDebug(KSTARS_EKOS_CAPTURE) << "Focus elapsed time (secs): " << getRefocusEveryNTimerElapsedSec() <<
                                     ". Requested Interval (secs): " << m_LimitsUI->limitRefocusN->value() * 60;

        if (getRefocusEveryNTimerElapsedSec() >= m_LimitsUI->limitRefocusN->value() * 60)
        {
            isRefocus = true;
            appendLogText(i18n("Scheduled refocus starting after %1 seconds...", getRefocusEveryNTimerElapsedSec()));
        }
    }

    if (!isRefocus && isTemperatureDeltaCheckActive)
    {
        qCDebug(KSTARS_EKOS_CAPTURE) << "Focus temperature delta (°C): " << focusTemperatureDelta <<
                                     ". Requested maximum delta (°C): " << m_LimitsUI->limitFocusDeltaTN->value();

        if (focusTemperatureDelta > m_LimitsUI->limitFocusDeltaTN->value())
        {
            isRefocus = true;
            appendLogText(i18n("Refocus starting because of temperature change of %1 °C...", focusTemperatureDelta));
        }
    }

    if (m_LimitsUI->meridianRefocusS->isChecked() && refocusAfterMeridianFlip)
    {
        isRefocus = true;
        refocusAfterMeridianFlip = false;
        appendLogText(i18n("Refocus after meridian flip"));

        // Post meridian flip we need to reset filter _before_ running in-sequence focusing
        // as it could have changed for whatever reason (e.g. alignment used a different filter).
        if (m_captureDeviceAdaptor->getFilterWheel())
        {
            int targetFilterPosition = activeJob->getTargetFilter();
            int currentFilterPosition = m_captureDeviceAdaptor->getFilterManager()->getFilterPosition();
            if (targetFilterPosition > 0 && targetFilterPosition != currentFilterPosition)
                m_captureDeviceAdaptor->getFilterWheel()->runCommand(INDI_SET_FILTER, &targetFilterPosition);
        }
    }

    // Either it is time to force autofocus or temperature has changed
    if (isRefocus)
    {
        if (m_captureDeviceAdaptor->getActiveCCD()->isFastExposureEnabled())
            m_captureDeviceAdaptor->getActiveChip()->abortExposure();

        // If we are over 30 mins since last autofocus, we'll reset frame.
        if (m_LimitsUI->limitRefocusN->value() >= 30)
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
        if (meridianFlipStage != MF_NONE && m_captureDeviceAdaptor->getFilterWheel())
        {
            int targetFilterPosition = activeJob->getTargetFilter();
            int currentFilterPosition = m_captureDeviceAdaptor->getFilterManager()->getFilterPosition();
            if (targetFilterPosition > 0 && targetFilterPosition != currentFilterPosition)
                m_captureDeviceAdaptor->getFilterWheel()->runCommand(INDI_SET_FILTER, &targetFilterPosition);
        }

        if (m_captureDeviceAdaptor->getActiveCCD()->isFastExposureEnabled())
            m_captureDeviceAdaptor->getActiveChip()->abortExposure();

        setFocusStatus(FOCUS_PROGRESS);
        emit checkFocus(m_LimitsUI->limitFocusHFRN->value() == 0.0 ? 0.1 : m_LimitsUI->limitFocusHFRN->value());

        qCDebug(KSTARS_EKOS_CAPTURE) << "In-sequence focusing started...";
        m_State = CAPTURE_FOCUSING;
        emit newStatus(Ekos::CAPTURE_FOCUSING);
        return true;
    }

    return false;
}

void Capture::captureOne()
{
    if (m_FocusState >= FOCUS_PROGRESS)
    {
        appendLogText(i18n("Cannot capture while focus module is busy."));
    }
    //    else if (captureEncodingS->currentIndex() == ISD::CCD::FORMAT_NATIVE && darkSubCheck->isChecked())
    //    {
    //        appendLogText(i18n("Cannot perform auto dark subtraction of native DSLR formats."));
    //    }
    else if (addJob(true))
    {
        m_State = CAPTURE_PROGRESS;
        prepareJob(jobs.last());
    }
}

void Capture::startFraming()
{
    if (m_FocusState >= FOCUS_PROGRESS)
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

    // This test must be placed before the FOCUS_PROGRESS test,
    // as sometimes the FilterManager can cause an auto-focus.
    // If the filterManager is not IDLE, then try again in 1 second.
    switch (m_FilterManagerState)
    {
        case FILTER_IDLE:
            // do nothing
            break;

        case FILTER_AUTOFOCUS:
            QTimer::singleShot(1000, this, &Ekos::Capture::captureImage);
            return;

        case FILTER_CHANGE:
            QTimer::singleShot(1000, this, &Ekos::Capture::captureImage);
            return;

        case FILTER_OFFSET:
            QTimer::singleShot(1000, this, &Ekos::Capture::captureImage);
            return;
    }

    // Do not start nor abort if Focus is busy
    if (m_FocusState >= FOCUS_PROGRESS)
    {
        //appendLogText(i18n("Delaying capture while focus module is busy."));
        QTimer::singleShot(1000, this, &Ekos::Capture::captureImage);
        return;
    }

    // Bail out if we have no CCD anymore
    if (m_captureDeviceAdaptor->getActiveCCD()->isConnected() == false)
    {
        appendLogText(i18n("Error: Lost connection to CCD."));
        abort();
        return;
    }

    captureTimeout.stop();
    seqDelayTimer->stop();
    captureDelayTimer->stop();

    CAPTUREResult rc = CAPTURE_OK;

    if (m_captureDeviceAdaptor->getFilterWheel() != nullptr)
    {
        // JM 2021.08.23 Call filter info to set the active filter wheel in the camera driver
        // so that it may snoop on the active filter
        syncFilterInfo();
        m_CurrentFilterPosition = m_captureDeviceAdaptor->getFilterManager()->getFilterPosition();
        activeJob->setCurrentFilter(m_CurrentFilterPosition);
    }

    if (m_captureDeviceAdaptor->getActiveCCD()->isFastExposureEnabled())
    {
        int remaining = m_isFraming ? 100000 : (activeJob->getCoreProperty(SequenceJob::SJ_Count).toInt() -
                                                activeJob->getCompleted());
        if (remaining > 1)
            m_captureDeviceAdaptor->getActiveCCD()->setFastCount(static_cast<uint>(remaining));
    }

    connect(m_captureDeviceAdaptor->getActiveCCD(), &ISD::CCD::newImage, this, &Ekos::Capture::processData,
            Qt::UniqueConnection);
    //connect(currentCCD, &ISD::CCD::previewFITSGenerated, this, &Ekos::Capture::setGeneratedPreviewFITS, Qt::UniqueConnection);

    if (activeJob->getFrameType() == FRAME_FLAT)
    {
        // If we have to calibrate ADU levels, first capture must be preview and not in batch mode
        if (activeJob->getCoreProperty(SequenceJob::SJ_Preview).toBool() == false
                && activeJob->getFlatFieldDuration() == DURATION_ADU &&
                activeJob->getCalibrationStage() == SequenceJobState::CAL_NONE)
        {
            if (m_captureDeviceAdaptor->getActiveCCD()->getEncodingFormat() != "FITS")
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
        if (m_captureDeviceAdaptor->getActiveCCD()->getUploadMode() != ISD::CCD::UPLOAD_CLIENT)
            m_captureDeviceAdaptor->getActiveCCD()->setUploadMode(ISD::CCD::UPLOAD_CLIENT);
    }
    // If batch mode, ensure upload mode mathces the active job target.
    else
    {
        if (m_captureDeviceAdaptor->getActiveCCD()->getUploadMode() != activeJob->getUploadMode())
            m_captureDeviceAdaptor->getActiveCCD()->setUploadMode(activeJob->getUploadMode());
    }

    if (m_captureDeviceAdaptor->getActiveCCD()->getUploadMode() != ISD::CCD::UPLOAD_LOCAL)
    {
        checkSeqBoundary(activeJob->getSignature());
        m_captureDeviceAdaptor->getActiveCCD()->setNextSequenceID(nextSequenceID);
    }

    m_State = CAPTURE_CAPTURING;

    //if (activeJob->getCoreProperty(SequenceJob::SJ_Preview).toBool() == false)
    // NOTE: Why we didn't emit this before for preview?
    emit newStatus(Ekos::CAPTURE_CAPTURING);

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
        m_captureDeviceAdaptor->getActiveCCD()->setFastExposureEnabled(true);
    }

    // If using DSLR, make sure it is set to correct transfer format
    m_captureDeviceAdaptor->getActiveCCD()->setEncodingFormat(activeJob->getCoreProperty(SequenceJob::SJ_Encoding).toString());

    connect(m_captureDeviceAdaptor->getActiveCCD(), &ISD::CCD::newExposureValue, this, &Ekos::Capture::setExposureProgress,
            Qt::UniqueConnection);

    // necessary since the status widget doesn't store the calibration stage
    if (activeJob->getCalibrationStage() == SequenceJobState::CAL_CALIBRATION)
        captureStatusWidget->setStatus(i18n("Calibrating..."), Qt::yellow);

    rc = activeJob->capture(m_AutoFocusReady,
                            activeJob->getCalibrationStage() == SequenceJobState::CAL_CALIBRATION ? FITS_CALIBRATE : FITS_NORMAL);

    if (rc != CAPTURE_OK)
    {
        disconnect(m_captureDeviceAdaptor->getActiveCCD(), &ISD::CCD::newExposureValue, this, &Ekos::Capture::setExposureProgress);
    }
    switch (rc)
    {
        case CAPTURE_OK:
        {
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
                int index = jobs.indexOf(activeJob);
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

        case CAPTURE_FILTER_BUSY:
            // Try again in 1 second if filter is busy
            QTimer::singleShot(1000, this, &Ekos::Capture::captureImage);
            break;

        case CAPTURE_GUIDER_DRIFT_WAIT:
            // Try again in 1 second if filter is busy
            qCDebug(KSTARS_EKOS_CAPTURE) << "Waiting for the guider to settle.";
            QTimer::singleShot(1000, this, &Ekos::Capture::captureImage);
            break;

        case CAPTURE_FOCUS_ERROR:
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

void Capture::setExposureProgress(ISD::CCDChip * tChip, double value, IPState state)
{
    if (m_captureDeviceAdaptor->getActiveChip() != tChip ||
            m_captureDeviceAdaptor->getActiveChip()->getCaptureMode() != FITS_NORMAL || meridianFlipStage >= MF_ALIGNING)
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

        if (m_captureDeviceAdaptor->getActiveCCD()
                && m_captureDeviceAdaptor->getActiveCCD()->getUploadMode() == ISD::CCD::UPLOAD_LOCAL)
        {
            if (activeJob && activeJob->getStatus() == JOB_BUSY)
            {
                processData(nullptr);
                return;
            }
        }

        //if (isAutoGuiding && Options::useEkosGuider() && currentCCD->getChip(ISD::CCDChip::GUIDE_CCD) == guideChip)
        if (m_GuideState == GUIDE_GUIDING && Options::guiderType() == 0 && suspendGuideOnDownload)
        {
            qCDebug(KSTARS_EKOS_CAPTURE) << "Autoguiding suspended until primary CCD chip completes downloading...";
            emit suspendGuiding();
        }

        captureStatusWidget->setStatus(i18n("Downloading..."), Qt::yellow);

        //This will start the clock to see how long the download takes.
        m_DownloadTimer.start();
        downloadProgressTimer.start();


        //disconnect(currentCCD, &ISD::CCD::newExposureValue(ISD::CCDChip*,double,IPState)), this, &Ekos::Capture::updateCaptureProgress(ISD::CCDChip*,double,IPState)));
    }
}

void Capture::updateCaptureCountDown(int deltaMillis)
{
    imageCountDown = imageCountDown.addMSecs(deltaMillis);
    sequenceCountDown = sequenceCountDown.addMSecs(deltaMillis);
    frameRemainingTime->setText(imageCountDown.toString("hh:mm:ss"));
    jobRemainingTime->setText(sequenceCountDown.toString("hh:mm:ss"));
}

void Capture::processCaptureError(ISD::CCD::ErrorType type)
{
    if (!activeJob)
        return;

    if (type == ISD::CCD::ERROR_CAPTURE)
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
    }

    // set the new value
    activeJob = value;

    // create job connections
    if (activeJob != nullptr)
    {
        // forward signals to the sequence job
        connect(this, &Capture::newGuiderDrift, activeJob, &SequenceJob::updateGuiderDrift);
        // react upon sequence job signals
        connect(activeJob, &SequenceJob::prepareState, this, &Capture::updatePrepareState);
        connect(activeJob, &SequenceJob::prepareComplete, this, &Capture::executeJob);
        connect(activeJob, &SequenceJob::abortCapture, this, &Capture::abort);
        connect(activeJob, &SequenceJob::newLog, this, &Capture::newLog);
        // forward the devices and attributes
        activeJob->setLightBox(m_captureDeviceAdaptor->getLightBox());
        activeJob->setTelescope(m_captureDeviceAdaptor->getTelescope());
        activeJob->setDome(m_captureDeviceAdaptor->getDome());
        activeJob->setDustCap(m_captureDeviceAdaptor->getDustCap());
        activeJob->setFilterManager(m_captureDeviceAdaptor->getFilterManager());
        activeJob->setAutoFocusReady(m_AutoFocusReady);
    }
}

void Capture::updateCCDTemperature(double value)
{
    if (cameraTemperatureS->isEnabled() == false)
    {
        if (m_captureDeviceAdaptor->getActiveCCD()->getBaseDevice()->getPropertyPermission("CCD_TEMPERATURE") != IP_RO)
            checkCCD();
    }

    temperatureOUT->setText(QString("%L1").arg(value, 0, 'f', 2));

    if (cameraTemperatureN->cleanText().isEmpty())
        cameraTemperatureN->setValue(value);
}

void Capture::updateRotatorAngle(double value)
{
    // Update widget rotator position
    rotatorSettings->setCurrentAngle(value);
}

bool Capture::addSequenceJob()
{
    return addJob(false, false);
}

bool Capture::addJob(bool preview, bool isDarkFlat)
{
    if (m_State != CAPTURE_IDLE && m_State != CAPTURE_ABORTED && m_State != CAPTURE_COMPLETE)
        return false;

    SequenceJob * job = nullptr;

    //    if (preview == false && darkSubCheck->isChecked())
    //    {
    //        KSNotification::error(i18n("Auto dark subtract is not supported in batch mode."));
    //        return false;
    //    }

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
        job = new SequenceJob(m_captureDeviceAdaptor, m_captureState);
        job->setFilterManager(m_captureDeviceAdaptor->getFilterManager());
    }

    Q_ASSERT_X(job, __FUNCTION__, "Capture Job is invalid.");

    job->setCoreProperty(SequenceJob::SJ_Format, captureFormatS->currentText());
    job->setCoreProperty(SequenceJob::SJ_Encoding, captureEncodingS->currentText());
    job->setCoreProperty(SequenceJob::SJ_DarkFlat, isDarkFlat);

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
        //        double currentTemperature;
        //        currentCCD->getTemperature(&currentTemperature);
        job->setCoreProperty(SequenceJob::SJ_EnforceTemperature, cameraTemperatureS->isChecked());
        job->setTargetTemperature(cameraTemperatureN->value());
    }

    //job->setCaptureFilter(static_cast<FITSScale>(filterCombo->currentIndex()));

    job->setUploadMode(static_cast<ISD::CCD::UploadMode>(fileUploadModeS->currentIndex()));
    job->setScripts(m_Scripts);
    job->setFlatFieldDuration(flatFieldDuration);
    job->setFlatFieldSource(flatFieldSource);
    job->setPreMountPark(preMountPark);
    job->setPreDomePark(preDomePark);
    job->setWallCoord(wallCoord);
    job->setCoreProperty(SequenceJob::SJ_TargetADU, targetADU);
    job->setCoreProperty(SequenceJob::SJ_TargetADUTolerance, targetADUTolerance);

    // JM 2019-11-26: In case there is no raw prefix set
    // BUT target name is set, we update the prefix to include
    // the target name, which is usually set by the scheduler.
    if (!m_TargetName.isEmpty())
    {
        // Target name as set externally should override Full Target Name that
        // was set by GOTO operation alone.
        m_FullTargetName = m_TargetName;
        m_FullTargetName.replace("_", " ");
        if (filePrefixT->text().isEmpty())
            filePrefixT->setText(m_TargetName);
    }

    job->setCoreProperty(SequenceJob::SJ_RawPrefix, filePrefixT->text());
    job->setCoreProperty(SequenceJob::SJ_FilterPrefixEnabled, fileFilterS->isChecked());
    job->setCoreProperty(SequenceJob::SJ_ExpPrefixEnabled, fileDurationS->isChecked());
    job->setCoreProperty(SequenceJob::SJ_TimeStampPrefixEnabled, fileTimestampS->isChecked());
    job->setFrameType(static_cast<CCDFrameType>(captureTypeS->currentIndex()));

    job->setCoreProperty(SequenceJob::SJ_EnforceStartGuiderDrift, (job->getFrameType() == FRAME_LIGHT
                         && m_LimitsUI->startGuiderDriftS->isChecked()));
    job->setTargetStartGuiderDrift(m_LimitsUI->startGuiderDriftN->value());

    //if (filterSlot != nullptr && currentFilter != nullptr)
    if (captureFilterS->currentIndex() != -1 && m_captureDeviceAdaptor->getFilterWheel() != nullptr)
        job->setTargetFilter(captureFilterS->currentIndex() + 1, captureFilterS->currentText());

    job->setCoreProperty(SequenceJob::SJ_Exposure, captureExposureN->value());

    job->setCoreProperty(SequenceJob::SJ_Count, captureCountN->value());

    job->setCoreProperty(SequenceJob::SJ_Binning, QPoint(captureBinHN->value(), captureBinVN->value()));

    /* in ms */
    job->setCoreProperty(SequenceJob::SJ_Delay, captureDelayN->value() * 1000);

    // Custom Properties
    job->setCustomProperties(customPropertiesDialog->getCustomProperties());

    if (m_captureDeviceAdaptor->getRotator() && rotatorSettings->isRotationEnforced())
    {
        job->setTargetRotation(rotatorSettings->getTargetRotationPA());
    }

    job->setCoreProperty(SequenceJob::SJ_ROI, QRect(captureFrameXN->value(), captureFrameYN->value(), captureFrameWN->value(),
                         captureFrameHN->value()));
    job->setCoreProperty(SequenceJob::SJ_RemoteDirectory, fileRemoteDirT->text());

    // Remove trailing slash, if any.
    QString fileDirectory = fileDirectoryT->text();
    while (fileDirectory.endsWith("/"))
        fileDirectory.chop(1);
    job->setCoreProperty(SequenceJob::SJ_LocalDirectory, fileDirectory);

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

    auto placeholderPath = Ekos::PlaceholderPath();
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
    if (captureFilterS->count() > 0 && (captureTypeS->currentIndex() == FRAME_LIGHT
                                        || captureTypeS->currentIndex() == FRAME_FLAT || isDarkFlat))
    {
        filter->setText(captureFilterS->currentText());
        jsonJob.insert("Filter", captureFilterS->currentText());
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

bool Capture::removeJob(int index)
{
    if (m_State != CAPTURE_IDLE && m_State != CAPTURE_ABORTED && m_State != CAPTURE_COMPLETE)
        return false;

    if (m_JobUnderEdit)
    {
        resetJobEdit();
        return false;
    }

    if (index < 0 || index >= jobs.count())
        return false;


    queueTable->removeRow(index);
    m_SequenceArray.removeAt(index);
    emit sequenceChanged(m_SequenceArray);

    if (jobs.empty())
        return true;

    SequenceJob * job = jobs.at(index);
    jobs.removeOne(job);
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

    int index = jobs.indexOf(job);
    if (index >= 0)
        queueTable->selectRow(index);

    seqDelay = activeJob->getCoreProperty(SequenceJob::SJ_Delay).toInt();

    if (activeJob->getCoreProperty(SequenceJob::SJ_Preview).toBool() == false)
    {
        // set the progress info
        imgProgress->setEnabled(true);
        imgProgress->setMaximum(activeJob->getCoreProperty(SequenceJob::SJ_Count).toInt());
        imgProgress->setValue(activeJob->getCompleted());

        if (m_captureDeviceAdaptor->getActiveCCD()->getUploadMode() != ISD::CCD::UPLOAD_LOCAL)
            updateSequencePrefix(activeJob->getCoreProperty(SequenceJob::SJ_FullPrefix).toString(),
                                 QFileInfo(activeJob->getSignature()).path());

        // We check if the job is already fully or partially complete by checking how many files of its type exist on the file system
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
            for (auto &a_job : jobs)
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

            m_captureDeviceAdaptor->getActiveCCD()->setNextSequenceID(nextSequenceID);
        }
    }

    if (m_captureDeviceAdaptor->getActiveCCD()->isBLOBEnabled() == false)
    {
        // FIXME: Move this warning pop-up elsewhere, it will interfere with automation.
        //        if (Options::guiderType() != Ekos::Guide::GUIDE_INTERNAL || KMessageBox::questionYesNo(nullptr, i18n("Image transfer is disabled for this camera. Would you like to enable it?")) ==
        //                KMessageBox::Yes)
        if (Options::guiderType() != Ekos::Guide::GUIDE_INTERNAL)
        {
            m_captureDeviceAdaptor->getActiveCCD()->setBLOBEnabled(true);
        }
        else
        {
            connect(KSMessageBox::Instance(), &KSMessageBox::accepted, this, [this]()
            {
                //QObject::disconnect(KSMessageBox::Instance(), &KSMessageBox::accepted, this, nullptr);
                KSMessageBox::Instance()->disconnect(this);
                m_captureDeviceAdaptor->getActiveCCD()->setBLOBEnabled(true);
                prepareActiveJobStage1();

            });
            connect(KSMessageBox::Instance(), &KSMessageBox::rejected, this, [this]()
            {
                //QObject::disconnect(KSMessageBox::Instance(), &KSMessageBox::rejected, this, nullptr);
                KSMessageBox::Instance()->disconnect(this);
                m_captureDeviceAdaptor->getActiveCCD()->setBLOBEnabled(true);
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
/**
 * @brief Reset #calibrationStage and continue with preparePreCaptureActions().
 */
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
    if (activeJob == nullptr)
    {
        qWarning(KSTARS_EKOS_CAPTURE) << "preparePreCaptureActions with null activeJob.";
        // Everything below depends on activeJob. Just return.
        return;
    }

    // Update position
    if (m_CurrentFilterPosition > 0)
        activeJob->setCurrentFilter(m_CurrentFilterPosition);

    // update temperature or guider drift not necessary, the state machine will request it if unknown

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

/**
 * @brief Listen to device property changes (temperature, rotator) that are triggered by
 *        SequenceJob.
 */
void Capture::updatePrepareState(Ekos::CaptureState prepareState)
{
    m_State = prepareState;
    emit newStatus(prepareState);

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

/**
 * @brief Start the execution of #activeJob by initiating updatePreCaptureCalibrationStatus().
 */
void Capture::executeJob()
{
    if (activeJob == nullptr)
    {
        qWarning(KSTARS_EKOS_CAPTURE) << "executeJob with null activeJob.";
        return;
    }

    QMap<QString, QString> FITSHeader;
    QString rawPrefix = activeJob->property("rawPrefix").toString();
    if (m_ObserverName.isEmpty() == false)
        FITSHeader["FITS_OBSERVER"] = m_ObserverName;
    if (m_FullTargetName.isEmpty() == false)
        FITSHeader["FITS_OBJECT"] = m_FullTargetName;
    else if (rawPrefix.isEmpty() == false)
    {
        // JM 2021-07-08: Remove "_" from target name.
        FITSHeader["FITS_OBJECT"] = rawPrefix.remove("_");
    }

    if (FITSHeader.count() > 0)
        m_captureDeviceAdaptor->getActiveCCD()->setFITSHeader(FITSHeader);

    // Update button status
    setBusy(true);

    useGuideHead = (m_captureDeviceAdaptor->getActiveChip()->getType() == ISD::CCDChip::PRIMARY_CCD) ? false : true;

    syncGUIToJob(activeJob);

    // If the job is a dark flat, let's find the optimal exposure from prior
    // flat exposures.
    if (activeJob->getCoreProperty(SequenceJob::SJ_DarkFlat).toBool())
    {
        // If we found a prior exposure, and current upload more is not local, then update full prefix
        if (setDarkFlatExposure(activeJob) && m_captureDeviceAdaptor->getActiveCCD()->getUploadMode() != ISD::CCD::UPLOAD_LOCAL)
        {
            auto placeholderPath = Ekos::PlaceholderPath();
            // Make sure to update Full Prefix as exposure value was changed
            placeholderPath.processJobInfo(activeJob, activeJob->getCoreProperty(SequenceJob::SJ_TargetName).toString());
            updateSequencePrefix(activeJob->getCoreProperty(SequenceJob::SJ_FullPrefix).toString(),
                                 QFileInfo(activeJob->getSignature()).path());
        }

    }

    updatePreCaptureCalibrationStatus();
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
        QTimer::singleShot(1000, this, &Ekos::Capture::updatePreCaptureCalibrationStatus);
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
    const double deviation_rms = std::hypot(delta_ra, delta_dec);
    QString deviationText = QString("%1").arg(deviation_rms, 0, 'f', 3);

    // communicate the new guiding deviation
    emit newGuiderDrift(deviation_rms);

    // if guiding deviations occur and no job is active, check if a meridian flip is ready to be executed
    if (activeJob == nullptr && checkMeridianFlipReady())
        return;

    // if the job is in the startup phase, check if the deviation is below the initial guiding limit
    if (m_State == CAPTURE_PROGRESS)
    {
        // initial guiding deviation irrelevant or below limit
        if (m_LimitsUI->startGuiderDriftS->isChecked() == false || deviation_rms < m_LimitsUI->startGuiderDriftN->value())
        {
            m_State = CAPTURE_CALIBRATING;
            if (m_DeviationDetected == true)
                appendLogText(i18n("Initial guiding deviation %1 below limit value of %2 arcsecs",
                                   deviationText, m_LimitsUI->startGuiderDriftN->value()));
            m_DeviationDetected = false;
        }
        else
        {
            // warn only once
            if (m_DeviationDetected == false)
                appendLogText(i18n("Initial guiding deviation %1 exceeded limit value of %2 arcsecs",
                                   deviationText, m_LimitsUI->startGuiderDriftN->value()));

            m_DeviationDetected = true;

            // Check if we need to start meridian flip. If yes, we need to start capturing
            // to ensure that capturing is recovered after the flip
            if (checkMeridianFlipReady())
                start();
        }

        // in any case, do not proceed
        return;
    }

    // If guiding is started after a meridian flip we will start getting guide deviations again
    // if the guide deviations are within our limits, we resume the sequence
    if (meridianFlipStage == MF_GUIDING)
    {
        // If the user didn't select any guiding deviation, we fall through
        // otherwise we can for deviation RMS
        if (m_LimitsUI->limitGuideDeviationS->isChecked() == false || deviation_rms < m_LimitsUI->limitGuideDeviationN->value())
        {
            appendLogText(i18n("Post meridian flip calibration completed successfully."));
            // N.B. Set meridian flip stage AFTER resumeSequence() always
            setMeridianFlipStage(MF_NONE);
            return;
        }
    }

    // We don't enforce limit on previews
    if (m_LimitsUI->limitGuideDeviationS->isChecked() == false || (activeJob
            && (activeJob->getCoreProperty(SequenceJob::SJ_Preview).toBool()
                || activeJob->getExposeLeft() == 0.0)))
        return;

    // If we have an active busy job, let's abort it if guiding deviation is exceeded.
    // And we accounted for the spike
    if (activeJob && activeJob->getStatus() == JOB_BUSY && activeJob->getFrameType() == FRAME_LIGHT)
    {
        if (deviation_rms <= m_LimitsUI->limitGuideDeviationN->value())
            m_SpikesDetected = 0;
        else
        {
            // Require several consecutive spikes to fail.
            constexpr int CONSECUTIVE_SPIKES_TO_FAIL = 3;
            if (++m_SpikesDetected < CONSECUTIVE_SPIKES_TO_FAIL)
                return;

            appendLogText(i18n("Guiding deviation %1 exceeded limit value of %2 arcsecs for %4 consecutive samples, "
                               "suspending exposure and waiting for guider up to %3 seconds.",
                               deviationText, m_LimitsUI->limitGuideDeviationN->value(),
                               QString("%L1").arg(guideDeviationTimer.interval() / 1000.0, 0, 'f', 3),
                               CONSECUTIVE_SPIKES_TO_FAIL));

            suspend();

            m_SpikesDetected    = 0;
            m_DeviationDetected = true;

            // Check if we need to start meridian flip. If yes, we need to start capturing
            // to ensure that capturing is recovered after the flip
            if (checkMeridianFlipReady())
                start();
            else
                guideDeviationTimer.start();
        }
        return;
    }

    // Find the first aborted job
    SequenceJob * abortedJob = nullptr;
    for(SequenceJob * job : jobs)
    {
        if (job->getStatus() == JOB_ABORTED)
        {
            abortedJob = job;
            break;
        }
    }

    if (abortedJob && m_DeviationDetected)
    {
        if (deviation_rms <= m_LimitsUI->limitGuideDeviationN->value())
        {
            guideDeviationTimer.stop();

            // Start with delay if start hasn't been triggered before
            if (! captureDelayTimer->isActive())
            {
                // if capturing has been suspended, restart it
                if (m_State == CAPTURE_SUSPENDED)
                {
                    if (seqDelay == 0)
                        appendLogText(i18n("Guiding deviation %1 is now lower than limit value of %2 arcsecs, "
                                           "resuming exposure.",
                                           deviationText, m_LimitsUI->limitGuideDeviationN->value()));
                    else
                        appendLogText(i18n("Guiding deviation %1 is now lower than limit value of %2 arcsecs, "
                                           "resuming exposure in %3 seconds.",
                                           deviationText, m_LimitsUI->limitGuideDeviationN->value(), seqDelay / 1000.0));

                    captureDelayTimer->start(seqDelay);
                }
            }
            return;
        }
        else
        {
            // stop the delayed capture start if necessary
            if (captureDelayTimer->isActive())
                captureDelayTimer->stop();

            appendLogText(i18n("Guiding deviation %1 is still higher than limit value of %2 arcsecs.",
                               deviationText, m_LimitsUI->limitGuideDeviationN->value()));
        }
    }
}

void Capture::setFocusStatus(FocusState state)
{
    if (state != m_FocusState)
        qCDebug(KSTARS_EKOS_CAPTURE) << "Focus State changed from" << Ekos::getFocusStatusString(
                                         m_FocusState) << "to" << Ekos::getFocusStatusString(state);
    m_FocusState = state;

    // Do not process above aborted or when meridian flip in progress
    if (m_FocusState > FOCUS_ABORTED || meridianFlipStage == MF_FLIPPING ||  meridianFlipStage == MF_SLEWING)
        return;

    if (m_FocusState == FOCUS_COMPLETE)
    {
        // enable option to have a refocus event occur if HFR goes over threshold
        m_AutoFocusReady = true;
        // forward to the active job
        if (activeJob != nullptr)
            activeJob->setAutoFocusReady(m_AutoFocusReady);

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
            m_LimitsUI->limitFocusHFRN->setValue(median + (median * (Options::hFRThresholdPercentage() / 100.0)));
        }

        // successful focus so reset elapsed time
        restartRefocusEveryNTimer();
    }

    if ((isRefocus || isInSequenceFocus) && activeJob && activeJob->getStatus() == JOB_BUSY)
    {
        if (m_FocusState == FOCUS_COMPLETE)
        {
            appendLogText(i18n("Focus complete."));
            captureStatusWidget->setStatus(i18n("Focus complete."), Qt::yellow);
        }
        // Meridian flip will abort focusing. In this case, after the meridian flip has completed capture
        // will restart the re-focus attempt. Therefore we only abort capture if meridian flip is not running.
        else if ((m_FocusState == FOCUS_FAILED || m_FocusState == FOCUS_ABORTED) &&
                 !(meridianFlipStage == MF_INITIATED || meridianFlipStage == MF_SLEWING)
                )
        {
            appendLogText(i18n("Autofocus failed. Aborting exposure..."));
            captureStatusWidget->setStatus(i18n("Autofocus failed."), Qt::darkRed);
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
        QString filterLock = m_captureDeviceAdaptor->getFilterManager().data()->getFilterLock(currentFilterText);
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
        m_LimitsUI->limitFocusHFRN->setValue(Options::hFRDeviation());
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
    m_LimitsUI->limitFocusHFRN->setValue(median + (median * (Options::hFRThresholdPercentage() / 100.0)));
}

void Capture::setMeridianFlipStage(MFStage stage)
{
    qCDebug(KSTARS_EKOS_CAPTURE) << "setMeridianFlipStage: " << MFStageString(stage);
    if (meridianFlipStage != stage)
    {
        switch (stage)
        {
            case MF_NONE:
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
                    captureStatusWidget->setStatus(i18n("Paused..."), Qt::yellow);
                    meridianFlipStage = stage;
                    emit newMeridianFlipStatus(Mount::FLIP_ACCEPTED);
                }
                else if (!(checkMeridianFlipRunning() || meridianFlipStage == MF_COMPLETED))
                {
                    // if neither a MF has been requested (checked above) or is in a post
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
                captureStatusWidget->setStatus(i18n("Meridian Flip..."), Qt::yellow);
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
                captureStatusWidget->setStatus(i18n("Flip complete."), Qt::yellow);
                meridianFlipStage = MF_COMPLETED;

                // Reset HFR pixels to file value after meridian flip
                if (isInSequenceFocus)
                {
                    qCDebug(KSTARS_EKOS_CAPTURE) << "Resetting HFR value to file value of" << fileHFR << "pixels after meridian flip.";
                    //firstAutoFocus = true;
                    inSequenceFocusCounter = 0;
                    m_LimitsUI->limitFocusHFRN->setValue(fileHFR);
                }

                // after a meridian flip we do not need to dither
                if ( Options::ditherEnabled() || Options::ditherNoGuiding())
                    ditherCounter = Options::ditherFrames();

                // if requested set flag so it perform refocus before next frame
                if (m_LimitsUI->meridianRefocusS->isChecked())
                    refocusAfterMeridianFlip = true;

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
                // This should never happen, since a meridian flip seems to be ongoing
                qCritical(KSTARS_EKOS_CAPTURE) << "Accepting meridian flip request while being in stage " << meridianFlipStage;
            }

            // If we are autoguiding, we should resume autoguiding after flip
            resumeGuidingAfterFlip = isGuidingOn();

            if (m_State == CAPTURE_IDLE || m_State == CAPTURE_ABORTED || m_State == CAPTURE_COMPLETE || m_State == CAPTURE_PAUSED)
            {
                setMeridianFlipStage(MF_INITIATED);
                emit newMeridianFlipStatus(Mount::FLIP_ACCEPTED);
            }
            else
                setMeridianFlipStage(MF_REQUESTED);

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
            result += job->getCoreProperty(SequenceJob::SJ_Count).toInt();
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
    m_captureDeviceAdaptor->setRotator(newRotator);
    m_captureDeviceAdaptor->readRotatorAngle();
    connect(m_captureDeviceAdaptor.data(), &Ekos::CaptureDeviceAdaptor::newRotatorReversed, this, &Capture::setRotatorReversed,
            Qt::UniqueConnection);
    rotatorB->setEnabled(true);
}

void Capture::setRotatorReversed(bool toggled)
{
    rotatorSettings->ReverseDirectionCheck->setEnabled(true);

    rotatorSettings->ReverseDirectionCheck->blockSignals(true);
    rotatorSettings->ReverseDirectionCheck->setChecked(toggled);
    rotatorSettings->ReverseDirectionCheck->blockSignals(false);

}

void Capture::setTelescope(ISD::GDInterface * newTelescope)
{
    // forward it to the command processor
    m_captureDeviceAdaptor->setTelescope(static_cast<ISD::Telescope *>(newTelescope));
    m_captureDeviceAdaptor->getTelescope()->disconnect(this);
    connect(m_captureDeviceAdaptor->getTelescope(), &ISD::GDInterface::numberUpdated, this,
            &Ekos::Capture::processTelescopeNumber);
    connect(m_captureDeviceAdaptor->getTelescope(), &ISD::Telescope::newTargetName, this, &Ekos::Capture::processNewTargetName);
    syncTelescopeInfo();
}

void Capture::processNewTargetName(const QString &name)
{
    if (m_State == CAPTURE_IDLE || m_State == CAPTURE_COMPLETE)
    {
        QString sanitized = name;
        if (sanitized != i18n("unnamed"))
        {
            m_FullTargetName = name;
            // Remove illegal characters that can be problematic
            sanitized = sanitized.replace( QRegularExpression("\\s|/|\\(|\\)|:|\\*|~|\"" ), "_" )
                        // Remove any two or more __
                        .replace( QRegularExpression("_{2,}"), "_")
                        // Remove any _ at the end
                        .replace( QRegularExpression("_$"), "");
            filePrefixT->setText(sanitized);
        }
    }
}

void Capture::syncTelescopeInfo()
{
    if (m_captureDeviceAdaptor->getTelescope() && m_captureDeviceAdaptor->getTelescope()->isConnected())
    {
        // Sync ALL CCDs to current telescope
        for (ISD::CCD * oneCCD : CCDs)
        {
            auto activeDevices = oneCCD->getBaseDevice()->getText("ACTIVE_DEVICES");
            if (activeDevices)
            {
                auto activeTelescope = activeDevices->findWidgetByName("ACTIVE_TELESCOPE");
                if (activeTelescope)
                {
                    activeTelescope->setText(m_captureDeviceAdaptor->getTelescope()->getDeviceName().toLatin1().constData());
                    oneCCD->getDriverInfo()->getClientManager()->sendNewText(activeDevices);
                }
            }
        }
    }
}

void Capture::saveFITSDirectory()
{
    QString dir =
        QFileDialog::getExistingDirectory(Ekos::Manager::Instance(), i18nc("@title:window", "FITS Save Directory"),
                                          dirPath.toLocalFile());

    if (dir.isEmpty())
        return;

    fileDirectoryT->setText(dir);
}

void Capture::loadSequenceQueue()
{
    QUrl fileURL = QFileDialog::getOpenFileUrl(Ekos::Manager::Instance(), i18nc("@title:window", "Open Ekos Sequence Queue"),
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
                else if (!strcmp(tagXMLEle(ep), "GuideDeviation"))
                {
                    m_LimitsUI->limitGuideDeviationS->setChecked(!strcmp(findXMLAttValu(ep, "enabled"), "true"));
                    m_LimitsUI->limitGuideDeviationN->setValue(cLocale.toDouble(pcdataXMLEle(ep)));
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
                    fileHFR = HFRValue > 0.0 ? HFRValue : 0.0;
                    m_LimitsUI->limitFocusHFRN->setValue(fileHFR);
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
                    refocusEveryNMinutesValue = minutesValue > 0 ? minutesValue : 0;
                    m_LimitsUI->limitRefocusN->setValue(refocusEveryNMinutesValue);
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

    bool isDarkFlat = false;
    m_Scripts.clear();
    QLocale cLocale = QLocale::c();

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
            rotatorSettings->setRotationEnforced(true);
            rotatorSettings->setTargetRotationPA(cLocale.toDouble(pcdataXMLEle(ep)));
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
        m_SequenceURL = QFileDialog::getSaveFileUrl(Ekos::Manager::Instance(), i18nc("@title:window", "Save Ekos Sequence Queue"),
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
    outstream << "<CCD>" << cameraS->currentText() << "</CCD>" << Qt::endl;
    outstream << "<FilterWheel>" << filterWheelS->currentText() << "</FilterWheel>" << Qt::endl;
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
    for (auto &job : jobs)
    {
        auto rawPrefix = job->getCoreProperty(SequenceJob::SJ_RawPrefix).toString();
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
        //outstream << "<CompletePrefix>" << job->getPrefix() << "</CompletePrefix>" << Qt::endl;
        outstream << "<RawPrefix>" << rawPrefix << "</RawPrefix>" << Qt::endl;
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
    if (job == nullptr)
    {
        qWarning(KSTARS_EKOS_CAPTURE) << "syncGuiToJob with null job.";
        // Everything below depends on job. Just return.
        return;
    }

    auto rawPrefix = job->getCoreProperty(SequenceJob::SJ_RawPrefix).toString();
    auto filterEnabled = job->getCoreProperty(SequenceJob::SJ_FilterPrefixEnabled).toBool();
    auto expEnabled = job->getCoreProperty(SequenceJob::SJ_ExpPrefixEnabled).toBool();
    auto tsEnabled = job->getCoreProperty(SequenceJob::SJ_TimeStampPrefixEnabled).toBool();
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
    captureFilterS->setCurrentIndex(job->getTargetFilter() - 1);
    captureTypeS->setCurrentIndex(job->getFrameType());
    filePrefixT->setText(rawPrefix);
    fileFilterS->setChecked(filterEnabled);
    fileDurationS->setChecked(expEnabled);
    fileTimestampS->setChecked(tsEnabled);
    captureCountN->setValue(job->getCoreProperty(SequenceJob::SJ_Count).toInt());
    captureDelayN->setValue(job->getCoreProperty(SequenceJob::SJ_Delay).toInt() / 1000);
    fileUploadModeS->setCurrentIndex(job->getUploadMode());
    fileRemoteDirT->setEnabled(fileUploadModeS->currentIndex() != 0);
    fileRemoteDirT->setText(job->getCoreProperty(SequenceJob::SJ_RemoteDirectory).toString());
    fileDirectoryT->setText(job->getCoreProperty(SequenceJob::SJ_LocalDirectory).toString());

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
        rotatorSettings->setRotationEnforced(true);
        rotatorSettings->setTargetRotationPA(job->getTargetRotation());
    }
    else
        rotatorSettings->setRotationEnforced(false);

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
    else if (m_captureDeviceAdaptor->getActiveCCD() && m_captureDeviceAdaptor->getActiveCCD()->hasGain())
        m_captureDeviceAdaptor->getActiveCCD()->getGain(&gain);

    double offset = -1;
    if (OffsetSpinSpecialValue > INVALID_VALUE && captureOffsetN->value() > OffsetSpinSpecialValue)
        offset = captureOffsetN->value();
    else if (m_captureDeviceAdaptor->getActiveCCD() && m_captureDeviceAdaptor->getActiveCCD()->hasOffset())
        m_captureDeviceAdaptor->getActiveCCD()->getOffset(&offset);

    int iso = -1;
    if (captureISOS)
        iso = captureISOS->currentIndex();
    else if (m_captureDeviceAdaptor->getActiveCCD())
        iso = m_captureDeviceAdaptor->getActiveCCD()->getChip(ISD::CCDChip::PRIMARY_CCD)->getISOIndex();

    settings.insert("camera", cameraS->currentText());
    settings.insert("fw", filterWheelS->currentText());
    settings.insert("filter", captureFilterS->currentText());
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
    if (i.row() < 0 || (i.row() + 1) > jobs.size())
        return false;

    SequenceJob * job = jobs.at(i.row());

    if (job == nullptr || job->getCoreProperty(SequenceJob::SJ_DarkFlat).toBool())
        return false;

    syncGUIToJob(job);

    if (isBusy)
        return false;

    if (jobs.size() >= 2)
    {
        queueUpB->setEnabled(i.row() > 0);
        queueDownB->setEnabled(i.row() + 1 < jobs.size());
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

    foreach (SequenceJob * job, jobs)
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
        if (job->getStatus() == JOB_DONE)
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

QString Capture::getJobFilterName(int id)
{
    if (id < jobs.count())
    {
        SequenceJob * job = jobs.at(id);
        return job->getCoreProperty(SequenceJob::SJ_Filter).toString();
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
        return job->getCoreProperty(SequenceJob::SJ_Count).toInt();
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
        return job->getCoreProperty(SequenceJob::SJ_Exposure).toDouble();
    }

    return -1;
}

CCDFrameType Capture::getJobFrameType(int id)
{
    if (id < jobs.count())
    {
        SequenceJob * job = jobs.at(id);
        return job->getFrameType();
    }

    return FRAME_NONE;
}

int Capture::getOverallRemainingTime()
{
    int remaining = 0;
    double estimatedDownloadTime = getEstimatedDownloadTime();

    foreach (SequenceJob * job, jobs)
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

void Capture::setTargetTemperature(double temperature)
{
    cameraTemperatureN->setValue(temperature);
}

void Capture::clearSequenceQueue()
{
    setActiveJob(nullptr);
    while (queueTable->rowCount() > 0)
        queueTable->removeRow(0);
    qDeleteAll(jobs);
    jobs.clear();

    while (m_SequenceArray.count())
        m_SequenceArray.pop_back();
    emit sequenceChanged(m_SequenceArray);
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
    if (nvp->device != m_captureDeviceAdaptor->getTelescope()->getDeviceName() || strstr(nvp->name, "EQUATORIAL_") == nullptr)
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
            if (m_captureDeviceAdaptor->getTelescope() != nullptr && m_captureDeviceAdaptor->getTelescope()->isSlewing())
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
    if (m_captureDeviceAdaptor->getDome() && m_captureDeviceAdaptor->getDome()->isMoving())
        return;

    appendLogText(i18n("Telescope completed the meridian flip."));

    KSNotification::event(QLatin1String("MeridianFlipCompleted"), i18n("Meridian flip is successfully completed"),
                          KSNotification::EVENT_INFO);


    if (m_State == CAPTURE_IDLE || m_State == CAPTURE_ABORTED || m_State == CAPTURE_COMPLETE)
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
    {
        setMeridianFlipStage(MF_NONE);
        return false;
    }

    // if we are waiting for a calibration, start it
    if (m_State < CAPTURE_CALIBRATING)
    {
        appendLogText(i18n("Performing post flip re-calibration and guiding..."));

        m_State = CAPTURE_CALIBRATING;
        emit newStatus(Ekos::CAPTURE_CALIBRATING);

        setMeridianFlipStage(MF_GUIDING);
        emit meridianFlipCompleted();
        return true;
    }
    else if (m_State == CAPTURE_CALIBRATING && (m_GuideState == GUIDE_CALIBRATION_ERROR || m_GuideState == GUIDE_ABORTED))
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

        retries = 0;
        m_State   = CAPTURE_ALIGNING;
        emit newStatus(Ekos::CAPTURE_ALIGNING);

        setMeridianFlipStage(MF_ALIGNING);
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
        m_State = CAPTURE_PAUSED;
        emit newStatus(m_State);
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
    if (m_captureDeviceAdaptor->getTelescope() == nullptr)
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
    if (isInSequenceFocus || (m_LimitsUI->limitRefocusS->isChecked() && getRefocusEveryNTimerElapsedSec() > 0))
        emit resetFocus();

    // signal that meridian flip may take place
    if (meridianFlipStage == MF_REQUESTED)
        setMeridianFlipStage(MF_READY);


    return true;
}

void Capture::checkGuideDeviationTimeout()
{
    if (activeJob && activeJob->getStatus() == JOB_ABORTED && m_DeviationDetected)
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
    if (state != m_AlignState)
        qCDebug(KSTARS_EKOS_CAPTURE) << "Align State changed from" << Ekos::getAlignStatusString(
                                         m_AlignState) << "to" << Ekos::getAlignStatusString(state);
    m_AlignState = state;

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
    if (state != m_GuideState)
        qCDebug(KSTARS_EKOS_CAPTURE) << "Guiding state changed from" << Ekos::getGuideStatusString(m_GuideState)
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
            m_GuideState = state;
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
                    m_DitheringState = IPS_OK;
                });
            }
            else
            {
                appendLogText(i18n("Dither complete."));
                m_DitheringState = IPS_OK;
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
                    m_DitheringState = IPS_OK;
                });
            }
            else
            {
                appendLogText(i18n("Warning: Dithering failed."));
                // signal OK so that capturing may continue although dithering failed
                m_DitheringState = IPS_OK;
            }

            break;

        default:
            break;
    }

    m_GuideState = state;
}


void Capture::processGuidingFailed()
{
    if (m_FocusState > FOCUS_PROGRESS)
    {
        appendLogText(i18n("Autoguiding stopped. Waiting for autofocus to finish..."));
    }
    // If Autoguiding was started before and now stopped, let's abort (unless we're doing a meridian flip)
    else if (isGuidingOn() && meridianFlipStage == MF_NONE &&
             ((activeJob && activeJob->getStatus() == JOB_BUSY) ||
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
    if (m_captureDeviceAdaptor->getActiveCCD() && m_captureDeviceAdaptor->getActiveCCD()->hasCoolerControl())
        return true;

    return false;
}

bool Capture::setCoolerControl(bool enable)
{
    if (m_captureDeviceAdaptor->getActiveCCD() && m_captureDeviceAdaptor->getActiveCCD()->hasCoolerControl())
        return m_captureDeviceAdaptor->getActiveCCD()->setCoolerControl(enable);

    return false;
}

void Capture::clearAutoFocusHFR()
{
    // If HFR limit was set from file, we cannot override it.
    if (fileHFR > 0)
        return;

    m_LimitsUI->limitFocusHFRN->setValue(0);
    //firstAutoFocus = true;
}

void Capture::openCalibrationDialog()
{
    QDialog calibrationDialog(this);

    Ui_calibrationOptions calibrationOptions;
    calibrationOptions.setupUi(&calibrationDialog);

    if (m_captureDeviceAdaptor->getTelescope())
    {
        calibrationOptions.parkMountC->setEnabled(m_captureDeviceAdaptor->getTelescope()->canPark());
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

/**
 * @brief Check all tasks that might be pending before capturing may start.
 *
 * The following checks are executed:
 *  1. Are there any pending jobs that failed? If yes, return with IPS_ALERT.
 *  2. Has pausing been initiated (@see checkPausing()).
 *  3. Is a meridian flip already running (@see checkMeridianFlipRunning()) or ready
 *     for execution (@see checkMeridianFlipReady()).
 *  4. Is a post meridian flip alignment running (@see checkAlignmentAfterFlip()).
 *  5. Is post flip guiding required or running (@see checkGuidingAfterFlip().
 *  6. Is the guiding deviation below the expected limit (@see setGuideDeviation(double,double)).
 *  7. Is dithering required or ongoing (@see checkDithering()).
 *  8. Is re-focusing required or ongoing (@see startFocusIfRequired()).
 *  9. Has guiding been resumed and needs to be restarted (@see resumeGuiding())
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

    // step 2: check if pausing has been requested
    if (checkPausing() == true)
    {
        pauseFunction = &Capture::startNextExposure;
        return IPS_BUSY;
    }

    // step 3: check if meridian flip is already running or ready for execution
    if (checkMeridianFlipRunning() || checkMeridianFlipReady())
        return IPS_BUSY;

    // step 4: check if post flip alignment is running
    if (m_State == CAPTURE_ALIGNING || checkAlignmentAfterFlip())
        return IPS_BUSY;

    // step 5: check if post flip guiding is running
    // MF_NONE is set as soon as guiding is running and the guide deviation is below the limit
    if ((m_State == CAPTURE_CALIBRATING && m_GuideState != GUIDE_GUIDING) || checkGuidingAfterFlip())
        return IPS_BUSY;

    // step 6: check guide deviation for non meridian flip stages if the initial guide limit is set.
    //         Wait until the guide deviation is reported to be below the limit (@see setGuideDeviation(double, double)).
    if (m_State == CAPTURE_PROGRESS && m_GuideState == GUIDE_GUIDING && m_LimitsUI->startGuiderDriftS->isChecked())
        return IPS_BUSY;

    // step 7: in case that a meridian flip has been completed and a guide deviation limit is set, we wait
    //         until the guide deviation is reported to be below the limit (@see setGuideDeviation(double, double)).
    //         Otherwise the meridian flip is complete
    if (m_State == CAPTURE_CALIBRATING && meridianFlipStage == MF_GUIDING)
    {
        if (m_LimitsUI->limitGuideDeviationS->isChecked() || m_LimitsUI->startGuiderDriftS->isChecked())
            return IPS_BUSY;
        else
            setMeridianFlipStage(MF_NONE);
    }

    // step 8: check if dithering is required or running
    if ((m_State == CAPTURE_DITHERING && m_DitheringState != IPS_OK) || checkDithering())
        return IPS_BUSY;

    // step 9: check if re-focusing is required
    //         Needs to be checked after dithering checks to avoid dithering in parallel
    //         to focusing, since @startFocusIfRequired() might change its value over time
    if ((m_State == CAPTURE_FOCUSING  && m_FocusState != FOCUS_COMPLETE && m_FocusState != FOCUS_ABORTED)
            || startFocusIfRequired())
        return IPS_BUSY;

    // step 10: resume guiding if it was suspended
    if (m_GuideState == GUIDE_SUSPENDED)
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


/**
 * @brief Execute the tasks that need to be completed before capturing may start.
 *
 * For light frames, checkLightFramePendingTasks() is called.
 *
 * @return IPS_OK if all necessary tasks have been completed
 */
IPState Capture::processPreCaptureCalibrationStage()
{
    // in some rare cases it might happen that activeJob has been cleared by a concurrent thread
    if (activeJob == nullptr)
    {
        qCWarning(KSTARS_EKOS_CAPTURE) << "Processing pre capture calibration without active job, state = " <<
                                       getCaptureStatusString(m_State);
        return IPS_ALERT;
    }

    // If we are currently guide and the frame is NOT a light frame, then we shopld suspend.
    // N.B. The guide camera could be on its own scope unaffected but it doesn't hurt to stop
    // guiding since it is no longer used anyway.
    if (activeJob->getFrameType() != FRAME_LIGHT && m_GuideState == GUIDE_GUIDING)
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
                if (m_captureDeviceAdaptor->getActiveCCD()->getUploadMode() != ISD::CCD::UPLOAD_CLIENT)
                {
                    m_captureDeviceAdaptor->getActiveCCD()->setUploadMode(ISD::CCD::UPLOAD_CLIENT);
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
                    m_captureDeviceAdaptor->getActiveCCD()->setUploadMode(activeJob->getUploadMode());
                    auto placeholderPath = Ekos::PlaceholderPath();
                    // Make sure to update Full Prefix as exposure value was changed
                    placeholderPath.processJobInfo(activeJob, activeJob->getCoreProperty(SequenceJob::SJ_TargetName).toString());
                    // Mark calibration as complete
                    activeJob->setCalibrationStage(SequenceJobState::CAL_CALIBRATION_COMPLETE);

                    // Must update sequence prefix as this step is only done in prepareJob
                    // but since the duration has now been updated, we must take care to update signature
                    // since it may include a placeholder for duration which would affect it.
                    if (m_captureDeviceAdaptor->getActiveCCD()
                            && m_captureDeviceAdaptor->getActiveCCD()->getUploadMode() != ISD::CCD::UPLOAD_LOCAL)
                        updateSequencePrefix(activeJob->getCoreProperty(SequenceJob::SJ_FullPrefix).toString(),
                                             QFileInfo(activeJob->getSignature()).path());

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
            if (m_captureDeviceAdaptor->getActiveCCD()->getUploadMode() != ISD::CCD::UPLOAD_CLIENT)
            {
                m_captureDeviceAdaptor->getActiveCCD()->setUploadMode(ISD::CCD::UPLOAD_CLIENT);
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
    if (m_captureDeviceAdaptor->getActiveCCD() == nullptr)
        return;

    if (m_captureDeviceAdaptor->getActiveCCD()->isBLOBEnabled() == false)
    {
        if (Options::guiderType() != Ekos::Guide::GUIDE_INTERNAL)
            m_captureDeviceAdaptor->getActiveCCD()->setBLOBEnabled(true);
        else
        {
            connect(KSMessageBox::Instance(), &KSMessageBox::accepted, this, [this, enabled]()
            {
                KSMessageBox::Instance()->disconnect(this);
                m_captureDeviceAdaptor->getActiveCCD()->setBLOBEnabled(true);
                m_captureDeviceAdaptor->getActiveCCD()->setVideoStreamEnabled(enabled);
            });

            KSMessageBox::Instance()->questionYesNo(i18n("Image transfer is disabled for this camera. Would you like to enable it?"),
                                                    i18n("Image Transfer"), 15);

            return;
        }
    }

    m_captureDeviceAdaptor->getActiveCCD()->setVideoStreamEnabled(enabled);
}

bool Capture::setVideoLimits(uint16_t maxBufferSize, uint16_t maxPreviewFPS)
{
    if (m_captureDeviceAdaptor->getActiveCCD() == nullptr)
        return false;

    return m_captureDeviceAdaptor->getActiveCCD()->setStreamLimits(maxBufferSize, maxPreviewFPS);
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
            if (isBusy == false)
                startB->setEnabled(false);
            break;

        default:
            if (isBusy == false)
            {
                previewB->setEnabled(true);
                if (m_captureDeviceAdaptor->getActiveCCD())
                    liveVideoB->setEnabled(m_captureDeviceAdaptor->getActiveCCD()->hasVideoStream());
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
    if (m_LimitsUI->limitRefocusS->isChecked())
    {
        // How much time passed since we last started the time
        long elapsedSecs = refocusEveryNTimer.elapsed() / 1000;
        // How many seconds do we wait for between focusing (60 mins ==> 3600 secs)
        int totalSecs   = m_LimitsUI->limitRefocusN->value() * 60;

        if (!refocusEveryNTimer.isValid() || forced)
        {
            appendLogText(i18n("Ekos will refocus in %1 seconds.", totalSecs));
            refocusEveryNTimer.restart();
        }
        else if (elapsedSecs < totalSecs)
        {
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

    if (m_captureDeviceAdaptor->getRotator() == nullptr)
        return;

    rotatorSettings->refresh();
}

void Capture::setFilterManager(const QSharedPointer<FilterManager> &manager)
{
    m_captureDeviceAdaptor->setFilterManager(manager);
    // forward it to the active job
    if (activeJob != nullptr)
        activeJob->setFilterManager(manager);

    connect(filterManagerB, &QPushButton::clicked, [this]()
    {
        m_captureDeviceAdaptor->getFilterManager()->show();
        m_captureDeviceAdaptor->getFilterManager()->raise();
    });

    connect(m_captureDeviceAdaptor->getFilterManager().data(), &FilterManager::ready, [this]()
    {
        m_CurrentFilterPosition = m_captureDeviceAdaptor->getFilterManager()->getFilterPosition();
        // Due to race condition,
        m_FocusState = FOCUS_IDLE;
        if (activeJob)
            activeJob->setCurrentFilter(m_CurrentFilterPosition);

    }
           );

    connect(m_captureDeviceAdaptor->getFilterManager().data(), &FilterManager::failed, [this]()
    {
        if (activeJob)
        {
            appendLogText(i18n("Filter operation failed."));
            abort();
        }
    }
           );

    connect(m_captureDeviceAdaptor->getFilterManager().data(), &FilterManager::newStatus,
            this, [this](Ekos::FilterState filterState)
    {
        if (filterState != m_FilterManagerState)
            qCDebug(KSTARS_EKOS_CAPTURE) << "Focus State changed from" << Ekos::getFilterStatusString(
                                             m_FilterManagerState) << "to" << Ekos::getFilterStatusString(filterState);
        m_FilterManagerState = filterState;
        if (m_State == CAPTURE_CHANGING_FILTER)
        {
            switch (filterState)
            {
                case FILTER_OFFSET:
                    appendLogText(i18n("Changing focus offset by %1 steps...",
                                       m_captureDeviceAdaptor->getFilterManager()->getTargetFilterOffset()));
                    break;

                case FILTER_CHANGE:
                    appendLogText(i18n("Changing filter to %1...",
                                       captureFilterS->itemText(m_captureDeviceAdaptor->getFilterManager()->getTargetFilterPosition() - 1)));
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
    // display capture status changes
    connect(m_captureDeviceAdaptor->getFilterManager().data(), &FilterManager::newStatus,
            captureStatusWidget, &CaptureStatusWidget::setFilterState);

    connect(m_captureDeviceAdaptor->getFilterManager().data(), &FilterManager::labelsChanged, this, [this]()
    {
        captureFilterS->clear();
        captureFilterS->addItems(m_captureDeviceAdaptor->getFilterManager()->getFilterLabels());
        m_CurrentFilterPosition = m_captureDeviceAdaptor->getFilterManager()->getFilterPosition();
        captureFilterS->setCurrentIndex(m_CurrentFilterPosition - 1);
    });
    connect(m_captureDeviceAdaptor->getFilterManager().data(), &FilterManager::positionChanged, this, [this]()
    {
        m_CurrentFilterPosition = m_captureDeviceAdaptor->getFilterManager()->getFilterPosition();
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

void Capture::cullToDSLRLimits()
{
    QString model(m_captureDeviceAdaptor->getActiveCCD()->getDeviceName());

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
    static bool init = false;

    // FIXME: QComboBox signal "activated" does not trigger when setting text programmatically.
    const QString targetCamera = settings["camera"].toString(cameraS->currentText());
    const QString targetFW = settings["fw"].toString(filterWheelS->currentText());
    const QString targetFilter = settings["filter"].toString(captureFilterS->currentText());

    if (cameraS->currentText() != targetCamera || init == false)
    {
        const int index = cameraS->findText(targetCamera);
        cameraS->setCurrentIndex(index);
        checkCCD(index);
    }

    if ((!targetFW.isEmpty() && filterWheelS->currentText() != targetFW) || init == false)
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

    double temperature = settings["temperature"].toDouble(INVALID_VALUE);
    if (temperature > INVALID_VALUE && m_captureDeviceAdaptor->getActiveCCD()
            && m_captureDeviceAdaptor->getActiveCCD()->hasCoolerControl())
    {
        setForceTemperature(true);
        setTargetTemperature(temperature);
    }
    else
        setForceTemperature(false);

    double gain = settings["gain"].toDouble(GainSpinSpecialValue);
    if (m_captureDeviceAdaptor->getActiveCCD() && m_captureDeviceAdaptor->getActiveCCD()->hasGain())
    {
        if (gain == GainSpinSpecialValue)
            captureGainN->setValue(GainSpinSpecialValue);
        else
            setGain(gain);
    }

    double offset = settings["offset"].toDouble(OffsetSpinSpecialValue);
    if (m_captureDeviceAdaptor->getActiveCCD() && m_captureDeviceAdaptor->getActiveCCD()->hasOffset())
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

    init = true;
}

void Capture::setFileSettings(const QJsonObject &settings)
{
    const QString prefix = settings["prefix"].toString(filePrefixT->text());
    //const QString script = settings["script"].toString(fileScriptT->text());
    const QString directory = settings["directory"].toString(fileDirectoryT->text());
    const bool filter = settings["filter"].toBool(fileFilterS->isChecked());
    const bool duration = settings["duration"].toBool(fileDurationS->isChecked());
    const bool ts = settings["ts"].toBool(fileTimestampS->isChecked());
    const int upload = settings["upload"].toInt(fileUploadModeS->currentIndex());
    const QString remote = settings["remote"].toString(fileRemoteDirT->text());

    filePrefixT->setText(prefix);
    //    fileScriptT->setText(script);
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
        //        {"script", fileScriptT->text()},
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
    const bool deviationCheck = settings["deviationCheck"].toBool(m_LimitsUI->limitGuideDeviationS->isChecked());
    const double deviationValue = settings["deviationValue"].toDouble(m_LimitsUI->limitGuideDeviationN->value());
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
}

QJsonObject Capture::getLimitSettings()
{
    QJsonObject settings =
    {
        {"deviationCheck", m_LimitsUI->limitGuideDeviationS->isChecked()},
        {"deviationValue", m_LimitsUI->limitGuideDeviationN->value()},
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
        m_captureDeviceAdaptor->getActiveCCD()->setConfig(PURGE_CONFIG);
        KStarsData::Instance()->userdb()->DeleteDSLRInfo(m_captureDeviceAdaptor->getActiveCCD()->getDeviceName());

        QStringList shutterfulCCDs  = Options::shutterfulCCDs();
        QStringList shutterlessCCDs = Options::shutterlessCCDs();

        // Remove camera from shutterful and shutterless CCDs
        if (shutterfulCCDs.contains(m_captureDeviceAdaptor->getActiveCCD()->getDeviceName()))
        {
            shutterfulCCDs.removeOne(m_captureDeviceAdaptor->getActiveCCD()->getDeviceName());
            Options::setShutterfulCCDs(shutterfulCCDs);
        }
        if (shutterlessCCDs.contains(m_captureDeviceAdaptor->getActiveCCD()->getDeviceName()))
        {
            shutterlessCCDs.removeOne(m_captureDeviceAdaptor->getActiveCCD()->getDeviceName());
            Options::setShutterlessCCDs(shutterlessCCDs);
        }

        // For DSLRs, immediately ask them to enter the values again.
        if (captureISOS && captureISOS->count() > 0)
        {
            createDSLRDialog();
        }
    });

    KSMessageBox::Instance()->questionYesNo( i18n("Reset %1 configuration to default?",
            m_captureDeviceAdaptor->getActiveCCD()->getDeviceName()),
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

    if (m_CaptureTimeoutCounter > 1 && m_captureDeviceAdaptor->getActiveCCD())
    {
        QString camera = m_captureDeviceAdaptor->getActiveCCD()->getDeviceName();
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
        // Double check that currentCCD is valid in case it was reset due to driver restart.
        if (m_captureDeviceAdaptor->getActiveCCD())
        {
            appendLogText(i18n("Exposure timeout. Restarting exposure..."));
            m_captureDeviceAdaptor->getActiveCCD()->setEncodingFormat("FITS");
            ISD::CCDChip *targetChip = m_captureDeviceAdaptor->getActiveCCD()->getChip(useGuideHead ? ISD::CCDChip::GUIDE_CCD :
                                       ISD::CCDChip::PRIMARY_CCD);
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
    dslrInfoDialog.reset(new DSLRInfo(this, m_captureDeviceAdaptor->getActiveCCD()));

    connect(dslrInfoDialog.get(), &DSLRInfo::infoChanged, this, [this]()
    {
        if (m_captureDeviceAdaptor->getActiveCCD())
            addDSLRInfo(QString(m_captureDeviceAdaptor->getActiveCCD()->getDeviceName()),
                        dslrInfoDialog->sensorMaxWidth,
                        dslrInfoDialog->sensorMaxHeight,
                        dslrInfoDialog->sensorPixelW,
                        dslrInfoDialog->sensorPixelH);
    });

    dslrInfoDialog->show();

    emit dslrInfoRequested(m_captureDeviceAdaptor->getActiveCCD()->getDeviceName());
}

void Capture::removeDevice(ISD::GDInterface *device)
{
    auto name = device->getDeviceName();

    device->disconnect(this);
    if (m_captureDeviceAdaptor->getTelescope() && m_captureDeviceAdaptor->getTelescope()->getDeviceName() == name)
    {
        m_captureDeviceAdaptor->setTelescope(nullptr);
        if (activeJob != nullptr)
            activeJob->setTelescope(nullptr);
    }
    else if (m_captureDeviceAdaptor->getDome() && m_captureDeviceAdaptor->getDome()->getDeviceName() == name)
    {
        m_captureDeviceAdaptor->setDome(nullptr);
    }
    else if (m_captureDeviceAdaptor->getRotator() && m_captureDeviceAdaptor->getRotator()->getDeviceName() == name)
    {
        m_captureDeviceAdaptor->setRotator(nullptr);
        rotatorB->setEnabled(false);
    }

    for (auto &oneCCD : CCDs)
    {
        if (oneCCD->getDeviceName() == name)
        {
            CCDs.removeAll(oneCCD);
            cameraS->removeItem(cameraS->findText(name));
            cameraS->removeItem(cameraS->findText(name + " Guider"));

            if (m_captureDeviceAdaptor->getActiveCCD() == oneCCD)
                m_captureDeviceAdaptor->setActiveCCD(nullptr);

            DarkLibrary::Instance()->removeCamera(oneCCD);

            if (CCDs.empty())
            {
                cameraS->setCurrentIndex(-1);
            }
            else
                cameraS->setCurrentIndex(0);

            //checkCCD();
            QTimer::singleShot(1000, this, [this]()
            {
                checkCCD();
            });

            break;
        }
    }

    for (auto &oneFilter : Filters)
    {
        if (oneFilter->getDeviceName() == name)
        {
            Filters.removeAll(oneFilter);
            m_captureDeviceAdaptor->getFilterManager()->removeDevice(device);
            filterWheelS->removeItem(filterWheelS->findText(name));

            if (m_captureDeviceAdaptor->getFilterWheel() == oneFilter)
                m_captureDeviceAdaptor->setFilterWheel(nullptr);

            if (Filters.empty())
            {
                filterWheelS->setCurrentIndex(-1);
            }
            else
                filterWheelS->setCurrentIndex(0);

            //checkFilter();
            QTimer::singleShot(1000, this, [this]()
            {
                checkFilter();
            });

            break;
        }
    }
}

void Capture::setGain(double value)
{
    if (!m_captureDeviceAdaptor->getActiveCCD())
        return;

    QMap<QString, QMap<QString, QVariant> > customProps = customPropertiesDialog->getCustomProperties();

    // Gain is manifested in two forms
    // Property CCD_GAIN and
    // Part of CCD_CONTROLS properties.
    // Therefore, we have to find what the currently camera supports first.
    if (m_captureDeviceAdaptor->getActiveCCD()->getProperty("CCD_GAIN"))
    {
        QMap<QString, QVariant> ccdGain;
        ccdGain["GAIN"] = value;
        customProps["CCD_GAIN"] = ccdGain;
    }
    else if (m_captureDeviceAdaptor->getActiveCCD()->getProperty("CCD_CONTROLS"))
    {
        QMap<QString, QVariant> ccdGain = customProps["CCD_CONTROLS"];
        ccdGain["Gain"] = value;
        customProps["CCD_CONTROLS"] = ccdGain;
    }

    customPropertiesDialog->setCustomProperties(customProps);
}

double Capture::getGain()
{
    if (!m_captureDeviceAdaptor->getActiveCCD())
        return -1;

    QMap<QString, QMap<QString, QVariant> > customProps = customPropertiesDialog->getCustomProperties();

    // Gain is manifested in two forms
    // Property CCD_GAIN and
    // Part of CCD_CONTROLS properties.
    // Therefore, we have to find what the currently camera supports first.
    if (m_captureDeviceAdaptor->getActiveCCD()->getProperty("CCD_GAIN"))
    {
        return customProps["CCD_GAIN"].value("GAIN", -1).toDouble();
    }
    else if (m_captureDeviceAdaptor->getActiveCCD()->getProperty("CCD_CONTROLS"))
    {
        return customProps["CCD_CONTROLS"].value("Gain", -1).toDouble();
    }

    return -1;
}

void Capture::setOffset(double value)
{
    if (!m_captureDeviceAdaptor->getActiveCCD())
        return;

    QMap<QString, QMap<QString, QVariant> > customProps = customPropertiesDialog->getCustomProperties();

    // Offset is manifested in two forms
    // Property CCD_OFFSET and
    // Part of CCD_CONTROLS properties.
    // Therefore, we have to find what the currently camera supports first.
    if (m_captureDeviceAdaptor->getActiveCCD()->getProperty("CCD_OFFSET"))
    {
        QMap<QString, QVariant> ccdOffset;
        ccdOffset["OFFSET"] = value;
        customProps["CCD_OFFSET"] = ccdOffset;
    }
    else if (m_captureDeviceAdaptor->getActiveCCD()->getProperty("CCD_CONTROLS"))
    {
        QMap<QString, QVariant> ccdOffset = customProps["CCD_CONTROLS"];
        ccdOffset["Offset"] = value;
        customProps["CCD_CONTROLS"] = ccdOffset;
    }

    customPropertiesDialog->setCustomProperties(customProps);
}

double Capture::getOffset()
{
    if (!m_captureDeviceAdaptor->getActiveCCD())
        return -1;

    QMap<QString, QMap<QString, QVariant> > customProps = customPropertiesDialog->getCustomProperties();

    // Gain is manifested in two forms
    // Property CCD_GAIN and
    // Part of CCD_CONTROLS properties.
    // Therefore, we have to find what the currently camera supports first.
    if (m_captureDeviceAdaptor->getActiveCCD()->getProperty("CCD_OFFSET"))
    {
        return customProps["CCD_OFFSET"].value("OFFSET", -1).toDouble();
    }
    else if (m_captureDeviceAdaptor->getActiveCCD()->getProperty("CCD_CONTROLS"))
    {
        return customProps["CCD_CONTROLS"].value("Offset", -1).toDouble();
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
            Ekos::CaptureState rememberState = m_State;
            m_State = CAPTURE_IDLE;
            checkCCD();
            m_State = rememberState;

            // restart capture
            m_CaptureTimeoutCounter = 0;

            if (activeJob)
            {
                m_captureDeviceAdaptor->setActiveChip(m_captureDeviceAdaptor->getActiveChip());
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

    return (m_GuideState == GUIDE_GUIDING ||
            m_GuideState == GUIDE_CALIBRATING ||
            m_GuideState == GUIDE_CALIBRATION_SUCCESS ||
            m_GuideState == GUIDE_DARK ||
            m_GuideState == GUIDE_SUBFRAME ||
            m_GuideState == GUIDE_STAR_SELECT ||
            m_GuideState == GUIDE_REACQUIRE ||
            m_GuideState == GUIDE_DITHERING ||
            m_GuideState == GUIDE_DITHERING_SUCCESS ||
            m_GuideState == GUIDE_DITHERING_ERROR ||
            m_GuideState == GUIDE_DITHERING_SETTLE ||
            m_GuideState == GUIDE_SUSPENDED
           );
}

bool Capture::isActivelyGuiding()
{
    return isGuidingOn() && (m_GuideState == GUIDE_GUIDING);
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
    if (m_captureDeviceAdaptor->getFilterWheel() == nullptr || m_CurrentFilterPosition < 1)
        return;

    QStringList labels = m_captureDeviceAdaptor->getFilterManager()->getFilterLabels();
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
        m_captureDeviceAdaptor->getFilterManager()->setFilterNames(newLabels);
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
    if (!m_captureDeviceAdaptor->getActiveCCD())
        return;

    double currentRamp, currentThreshold;
    if (!m_captureDeviceAdaptor->getActiveCCD()->getTemperatureRegulation(currentRamp, currentThreshold))
        return;


    double rMin, rMax, rStep, tMin, tMax, tStep;

    m_captureDeviceAdaptor->getActiveCCD()->getMinMaxStep("CCD_TEMP_RAMP", "RAMP_SLOPE", &rMin, &rMax, &rStep);
    m_captureDeviceAdaptor->getActiveCCD()->getMinMaxStep("CCD_TEMP_RAMP", "RAMP_THRESHOLD", &tMin, &tMax, &tStep);

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
        if (m_captureDeviceAdaptor->getActiveCCD())
            m_captureDeviceAdaptor->getActiveCCD()->setTemperatureRegulation(rampSpin.value(), thresholdSpin.value());
    }
}

void Capture::generateDarkFlats()
{
    const auto existingJobs = jobs.size();
    uint8_t jobsAdded = 0;

    for (int i = 0; i < existingJobs; i++)
    {
        if (jobs[i]->getFrameType() != FRAME_FLAT)
            continue;

        syncGUIToJob(jobs[i]);

        captureTypeS->setCurrentIndex(FRAME_DARK);
        addJob(false, true);
        jobsAdded++;
    }

    if (jobsAdded > 0)
    {
        appendLogText(i18np("One dark flats job was created.", "%1 dark flats jobs were created.", jobsAdded));
    }
}

bool Capture::setDarkFlatExposure(SequenceJob *job)
{
    const auto darkFlatFilter = job->getCoreProperty(SequenceJob::SJ_Filter).toString();
    const auto darkFlatBinning = job->getCoreProperty(SequenceJob::SJ_Binning).toPoint();
    const auto darkFlatADU = job->getCoreProperty(SequenceJob::SJ_TargetADU).toInt();

    for (auto &oneJob : jobs)
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

}
