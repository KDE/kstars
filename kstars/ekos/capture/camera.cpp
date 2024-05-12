/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>
    SPDX-FileCopyrightText: 2024 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "camera.h"

#include "Options.h"
#include "kstars.h"
#include "ekos/manager.h"
#include "ekos/auxiliary/darklibrary.h"
#include "ekos/auxiliary/filtermanager.h"
#include "ekos/auxiliary/opticaltrainmanager.h"
#include "ekos/auxiliary/opticaltrainsettings.h"
#include "ekos/auxiliary/profilesettings.h"
#include "ekos/auxiliary/rotatorutils.h"
#include "capturedeviceadaptor.h"
#include "captureprocess.h"
#include "scriptsmanager.h"
#include "exposurecalculator/exposurecalculatordialog.h"

#include "indi/driverinfo.h"
#include "indi/indirotator.h"

#include <ekos_capture_debug.h>

#define KEY_GAIN_KWD    "ccdGainKeyword"
#define KEY_OFFSET_KWD  "ccdOffsetKeyword"

namespace Ekos
{

Camera::Camera(bool standAlone, QWidget *parent)
    : QWidget{parent}, m_standAlone(standAlone)
{
    setupUi(this);
    m_customPropertiesDialog.reset(new CustomProperties());
    m_LimitsDialog = new QDialog(this);
    m_LimitsUI.reset(new Ui::Limits());
    m_LimitsUI->setupUi(m_LimitsDialog);

    m_CalibrationDialog = new QDialog(this);
    m_CalibrationUI.reset(new Ui::Calibration());
    m_CalibrationUI->setupUi(m_CalibrationDialog);

    m_scriptsManager = new ScriptsManager(this);
    if (m_standAlone)
    {
        // Prepend "Capture Sequence Editor" to the two pop-up window titles, to differentiate them
        // from similar windows in the Capture tab.
        auto title = i18n("Capture Sequence Editor: %1", m_LimitsDialog->windowTitle());
        m_LimitsDialog->setWindowTitle(title);
        title = i18n("Capture Sequence Editor: %1", m_scriptsManager->windowTitle());
        m_scriptsManager->setWindowTitle(title);
    }

    // Setup Debounce timer to limit over-activation of settings changes
    m_DebounceTimer.setInterval(500);
    m_DebounceTimer.setSingleShot(true);

    // hide avg. download time and target drift initially
    targetDriftLabel->setVisible(false);
    targetDrift->setVisible(false);
    targetDriftUnit->setVisible(false);
    avgDownloadTime->setVisible(false);
    avgDownloadLabel->setVisible(false);
    secLabel->setVisible(false);
    darkB->setChecked(Options::autoDark());

    startB->setIcon(QIcon::fromTheme("media-playback-start"));
    startB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    pauseB->setIcon(QIcon::fromTheme("media-playback-pause"));
    pauseB->setAttribute(Qt::WA_LayoutUsesWidgetRect);

    filterManagerB->setIcon(QIcon::fromTheme("view-filter"));
    filterManagerB->setAttribute(Qt::WA_LayoutUsesWidgetRect);

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

    observerB->setIcon(QIcon::fromTheme("im-user"));
    observerB->setAttribute(Qt::WA_LayoutUsesWidgetRect);

    addToQueueB->setToolTip(i18n("Add job to sequence queue"));
    removeFromQueueB->setToolTip(i18n("Remove job from sequence queue"));

    if (Options::remoteCaptureDirectory().isEmpty() == false)
    {
        fileRemoteDirT->setText(Options::remoteCaptureDirectory());
    }

    if(!Options::captureDirectory().isEmpty())
        fileDirectoryT->setText(Options::captureDirectory());
    else
    {
        fileDirectoryT->setText(QDir::homePath() + QDir::separator() + "Pictures");
    }

    //Note:  This is to prevent a button from being called the default button
    //and then executing when the user hits the enter key such as when on a Text Box
    QList<QPushButton *> qButtons = findChildren<QPushButton *>();
    for (auto &button : qButtons)
        button->setAutoDefault(false);
}

void Camera::initCamera()
{

    connect(&m_DebounceTimer, &QTimer::timeout, this, &Camera::settleSettings);

    connect(m_FilterManager.get(), &FilterManager::ready, this, &Camera::updateCurrentFilterPosition);

    connect(exposureCalcB, &QPushButton::clicked, this, &Camera::openExposureCalculatorDialog);

    // avoid combination of ACTION_WALL and ACTION_PARK_MOUNT
    connect(m_CalibrationUI->captureCalibrationWall, &QCheckBox::clicked, [&](bool checked)
    {
        if (checked)
            m_CalibrationUI->captureCalibrationParkMount->setChecked(false);
    });
    connect(m_CalibrationUI->captureCalibrationParkMount, &QCheckBox::clicked, [&](bool checked)
    {
        if (checked)
            m_CalibrationUI->captureCalibrationWall->setChecked(false);
    });

    connect(limitsB, &QPushButton::clicked, m_LimitsDialog, &QDialog::show);
    connect(darkLibraryB, &QPushButton::clicked, DarkLibrary::Instance(), &QDialog::show);
    connect(scriptManagerB, &QPushButton::clicked, this, &Camera::handleScriptsManager);

    connect(temperatureRegulationB, &QPushButton::clicked, this, &Camera::showTemperatureRegulation);
    connect(cameraTemperatureS, &QCheckBox::toggled, this, [this](bool toggled)
    {
        if (devices()->getActiveCamera())
        {
            QVariantMap auxInfo = devices()->getActiveCamera()->getDriverInfo()->getAuxInfo();
            auxInfo[QString("%1_TC").arg(devices()->getActiveCamera()->getDeviceName())] = toggled;
            devices()->getActiveCamera()->getDriverInfo()->setAuxInfo(auxInfo);
        }
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
    connect(resetFormatB, &QPushButton::clicked, this, [this]()
    {
        placeholderFormatT->setText(KSUtils::getDefaultPath("PlaceholderFormat"));
    });

    connect(captureTypeS, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
            &Camera::checkFrameType);

    // Autofocus HFR Check
    connect(m_LimitsUI->enforceAutofocusHFR, &QCheckBox::toggled, [ = ](bool checked)
    {
        if (checked == false)
            state()->getRefocusState()->setInSequenceFocus(false);
    });

    connect(m_captureModuleState.get(), &CaptureModuleState::newLimitFocusHFR, this, [this](double hfr)
    {
        m_LimitsUI->hFRDeviation->setValue(hfr);
    });

    state()->getCaptureDelayTimer().setSingleShot(true);
    connect(&state()->getCaptureDelayTimer(), &QTimer::timeout, m_captureProcess.get(), &CaptureProcess::captureImage);

    // Exposure Timeout
    state()->getCaptureTimeout().setSingleShot(true);
    connect(&state()->getCaptureTimeout(), &QTimer::timeout, m_captureProcess.get(),
            &CaptureProcess::processCaptureTimeout);

    // Remote directory
    connect(fileUploadModeS, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
            [&](int index)
    {
        fileRemoteDirT->setEnabled(index != 0);
    });

    connect(customValuesB, &QPushButton::clicked, this, [&]()
    {
        customPropertiesDialog()->show();
        customPropertiesDialog()->raise();
    });

    connect(customPropertiesDialog(), &CustomProperties::valueChanged, this, [&]()
    {
        const double newGain = getGain();
        if (captureGainN && newGain >= 0)
            captureGainN->setValue(newGain);
        const int newOffset = getOffset();
        if (newOffset >= 0)
            captureOffsetN->setValue(newOffset);
    });

    // process engine connections
    connect(m_captureProcess.data(), &CaptureProcess::rotatorReverseToggled, this, &Camera::setRotatorReversed);
    // device adaptor connections
    connect(m_DeviceAdaptor.data(), &CaptureDeviceAdaptor::newCCDTemperatureValue, this,
            &Camera::updateCCDTemperature, Qt::UniqueConnection);
    connect(m_DeviceAdaptor.data(), &CaptureDeviceAdaptor::CameraConnected, this, [this](bool connected)
    {
        CaptureSettingsGroup->setEnabled(connected);
        fileSettingsGroup->setEnabled(connected);
        sequenceBox->setEnabled(connected);
        for (auto &oneChild : sequenceControlsButtonGroup->buttons())
            oneChild->setEnabled(connected);

        if (! connected)
            trainLayout->setEnabled(true);
    });
    connect(m_DeviceAdaptor.data(), &CaptureDeviceAdaptor::FilterWheelConnected, this, [this](bool connected)
    {
        FilterPosLabel->setEnabled(connected);
        FilterPosCombo->setEnabled(connected);
        filterManagerB->setEnabled(connected);
    });
    connect(m_DeviceAdaptor.data(), &CaptureDeviceAdaptor::newRotator, this, &Camera::setRotator);
    connect(m_DeviceAdaptor.data(), &CaptureDeviceAdaptor::newRotatorAngle, this, &Camera::updateRotatorAngle,
            Qt::UniqueConnection);
}

void Camera::updateRotatorAngle(double value)
{
    IPState RState = devices()->rotator()->absoluteAngleState();
    if (RState == IPS_OK)
        m_RotatorControlPanel->updateRotator(value);
    else
        m_RotatorControlPanel->updateGauge(value);
}

void Camera::setRotatorReversed(bool toggled)
{
    m_RotatorControlPanel->reverseDirection->setEnabled(true);
    m_RotatorControlPanel->reverseDirection->blockSignals(true);
    m_RotatorControlPanel->reverseDirection->setChecked(toggled);
    m_RotatorControlPanel->reverseDirection->blockSignals(false);
}

void Camera::updateCCDTemperature(double value)
{
    if (cameraTemperatureS->isEnabled() == false && devices()->getActiveCamera())
    {
        if (devices()->getActiveCamera()->getPermission("CCD_TEMPERATURE") != IP_RO)
            process()->checkCamera();
    }

    temperatureOUT->setText(QString("%L1º").arg(value, 0, 'f', 1));

    if (cameraTemperatureN->cleanText().isEmpty())
        cameraTemperatureN->setValue(value);
}

void Camera::checkFrameType(int index)
{
    calibrationB->setEnabled(index != FRAME_LIGHT);
    generateDarkFlatsB->setEnabled(index != FRAME_LIGHT);
}

void Camera::updateStartButtons(bool start, bool pause)
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



void Camera::setGain(double value)
{
    if (m_standAlone)
    {
        setStandAloneGain(value);
        return;
    }
    if (!devices()->getActiveCamera())
        return;

    QMap<QString, QMap<QString, QVariant> > customProps = customPropertiesDialog()->getCustomProperties();
    process()->updateGain(value, customProps);
    customPropertiesDialog()->setCustomProperties(customProps);

}

double Camera::getGain()
{
    return devices()->cameraGain(customPropertiesDialog()->getCustomProperties());
}

void Camera::setOffset(double value)
{
    if (m_standAlone)
    {
        setStandAloneOffset(value);
        return;
    }
    if (!devices()->getActiveCamera())
        return;

    QMap<QString, QMap<QString, QVariant> > customProps = customPropertiesDialog()->getCustomProperties();

    process()->updateOffset(value, customProps);
    customPropertiesDialog()->setCustomProperties(customProps);
}

double Camera::getOffset()
{
    return devices()->cameraOffset(customPropertiesDialog()->getCustomProperties());
}

void Camera::setRotator(QString name)
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
        m_RotatorControlPanel->initRotator(opticalTrainCombo->currentText(), devices().data(),
                                           Rotator);
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

QVariantMap Camera::getAllSettings() const
{
    QVariantMap settings;

    // All QLineEdits
    // N.B. This must be always first since other Widgets can be casted to QLineEdit like QSpinBox but not vice-versa.
    for (auto &oneWidget : findChildren<QLineEdit*>())
    {
        auto name = oneWidget->objectName();
        if (name == "qt_spinbox_lineedit")
            continue;
        settings.insert(name, oneWidget->text());
    }

    // All Combo Boxes
    for (auto &oneWidget : findChildren<QComboBox*>())
        settings.insert(oneWidget->objectName(), oneWidget->currentText());

    // All Double Spin Boxes
    for (auto &oneWidget : findChildren<QDoubleSpinBox*>())
        settings.insert(oneWidget->objectName(), oneWidget->value());

    // All Spin Boxes
    for (auto &oneWidget : findChildren<QSpinBox*>())
        settings.insert(oneWidget->objectName(), oneWidget->value());

    // All Checkboxes
    for (auto &oneWidget : findChildren<QCheckBox*>())
        settings.insert(oneWidget->objectName(), oneWidget->isChecked());

    // All Checkable Groupboxes
    for (auto &oneWidget : findChildren<QGroupBox*>())
        if (oneWidget->isCheckable())
            settings.insert(oneWidget->objectName(), oneWidget->isChecked());

    // All Radio Buttons
    for (auto &oneWidget : findChildren<QRadioButton*>())
        settings.insert(oneWidget->objectName(), oneWidget->isChecked());

    return settings;
}

void Camera::syncLimitSettings()
{
    m_LimitsUI->enforceStartGuiderDrift->setChecked(Options::enforceStartGuiderDrift());
    m_LimitsUI->startGuideDeviation->setValue(Options::startGuideDeviation());
    m_LimitsUI->enforceGuideDeviation->setChecked(Options::enforceGuideDeviation());
    m_LimitsUI->guideDeviation->setValue(Options::guideDeviation());
    m_LimitsUI->guideDeviationReps->setValue(static_cast<int>(Options::guideDeviationReps()));
    m_LimitsUI->enforceAutofocusHFR->setChecked(Options::enforceAutofocusHFR());
    m_LimitsUI->hFRThresholdPercentage->setValue(Options::hFRThresholdPercentage());
    m_LimitsUI->hFRDeviation->setValue(Options::hFRDeviation());
    m_LimitsUI->inSequenceCheckFrames->setValue(Options::inSequenceCheckFrames());
    m_LimitsUI->hFRCheckAlgorithm->setCurrentIndex(Options::hFRCheckAlgorithm());
    m_LimitsUI->enforceAutofocusOnTemperature->setChecked(Options::enforceAutofocusOnTemperature());
    m_LimitsUI->maxFocusTemperatureDelta->setValue(Options::maxFocusTemperatureDelta());
    m_LimitsUI->enforceRefocusEveryN->setChecked(Options::enforceRefocusEveryN());
    m_LimitsUI->refocusEveryN->setValue(static_cast<int>(Options::refocusEveryN()));
    m_LimitsUI->refocusAfterMeridianFlip->setChecked(Options::refocusAfterMeridianFlip());
}

void Camera::settleSettings()
{
    state()->setDirty(true);
    emit settingsUpdated(getAllSettings());
    // Save to optical train specific settings as well
    const int id = OpticalTrainManager::Instance()->id(opticalTrainCombo->currentText());
    OpticalTrainSettings::Instance()->setOpticalTrainID(id);
    OpticalTrainSettings::Instance()->setOneSetting(OpticalTrainSettings::Capture, m_settings);
    Options::setCaptureTrainID(id);
}

void Camera::storeTrainKey(const QString &key, const QStringList &list)
{
    if (!m_settings.contains(key) || m_settings[key].toStringList() != list)
    {
        m_settings[key] = list;
        m_DebounceTimer.start();
    }
}

void Camera::storeTrainKeyString(const QString &key, const QString &str)
{
    if (!m_settings.contains(key) || m_settings[key].toString() != str)
    {
        m_settings[key] = str;
        m_DebounceTimer.start();
    }
}

void Camera::updateCurrentFilterPosition()
{
    const QString currentFilterText = FilterPosCombo->itemText(m_FilterManager->getFilterPosition() - 1);
    state()->setCurrentFilterPosition(m_FilterManager->getFilterPosition(),
                                      currentFilterText,
                                      m_FilterManager->getFilterLock(currentFilterText));

}

ISD::Camera *Camera::activeCamera()
{
    return m_DeviceAdaptor->getActiveCamera();
}

QJsonObject Camera::currentScope()
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

double Camera::currentReducer()
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

double Camera::currentAperture()
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

void Camera::openExposureCalculatorDialog()
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

void Camera::handleScriptsManager()
{
    QMap<ScriptTypes, QString> old_scripts = m_scriptsManager->getScripts();

    if (m_scriptsManager->exec() != QDialog::Accepted)
        // reset to old value
        m_scriptsManager->setScripts(old_scripts);
}

void Camera::showTemperatureRegulation()
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

void Camera::onStandAloneShow(QShowEvent *event)
{
    OpticalTrainSettings::Instance()->setOpticalTrainID(Options::captureTrainID());
    auto oneSetting = OpticalTrainSettings::Instance()->getOneSetting(OpticalTrainSettings::Capture);
    setSettings(oneSetting.toJsonObject().toVariantMap());

    Q_UNUSED(event);
    QSharedPointer<FilterManager> fm;

    captureGainN->setValue(GainSpinSpecialValue);
    captureOffsetN->setValue(OffsetSpinSpecialValue);

    m_standAloneUseCcdGain = true;
    m_standAloneUseCcdOffset = true;
    if (m_settings.contains(KEY_GAIN_KWD) && m_settings[KEY_GAIN_KWD].toString() == "CCD_CONTROLS")
        m_standAloneUseCcdGain = false;
    if (m_settings.contains(KEY_OFFSET_KWD) && m_settings[KEY_OFFSET_KWD].toString() == "CCD_CONTROLS")
        m_standAloneUseCcdOffset = false;


    // Capture Gain
    connect(captureGainN, &QDoubleSpinBox::editingFinished, this, [this]()
    {
        if (captureGainN->value() != GainSpinSpecialValue)
            setGain(captureGainN->value());
        else
            setGain(-1);
    });

    // Capture Offset
    connect(captureOffsetN, &QDoubleSpinBox::editingFinished, this, [this]()
    {
        if (captureOffsetN->value() != OffsetSpinSpecialValue)
            setOffset(captureOffsetN->value());
        else
            setOffset(-1);
    });
}

void Camera::setStandAloneGain(double value)
{
    QMap<QString, QMap<QString, QVariant> > propertyMap = m_customPropertiesDialog->getCustomProperties();

    if (m_standAloneUseCcdGain)
    {
        if (value >= 0)
        {
            QMap<QString, QVariant> ccdGain;
            ccdGain["GAIN"] = value;
            propertyMap["CCD_GAIN"] = ccdGain;
        }
        else
        {
            propertyMap["CCD_GAIN"].remove("GAIN");
            if (propertyMap["CCD_GAIN"].size() == 0)
                propertyMap.remove("CCD_GAIN");
        }
    }
    else
    {
        if (value >= 0)
        {
            QMap<QString, QVariant> ccdGain = propertyMap["CCD_CONTROLS"];
            ccdGain["Gain"] = value;
            propertyMap["CCD_CONTROLS"] = ccdGain;
        }
        else
        {
            propertyMap["CCD_CONTROLS"].remove("Gain");
            if (propertyMap["CCD_CONTROLS"].size() == 0)
                propertyMap.remove("CCD_CONTROLS");
        }
    }

    m_customPropertiesDialog->setCustomProperties(propertyMap);
}

void Camera::setStandAloneOffset(double value)
{
    QMap<QString, QMap<QString, QVariant> > propertyMap = m_customPropertiesDialog->getCustomProperties();

    if (m_standAloneUseCcdOffset)
    {
        if (value >= 0)
        {
            QMap<QString, QVariant> ccdOffset;
            ccdOffset["OFFSET"] = value;
            propertyMap["CCD_OFFSET"] = ccdOffset;
        }
        else
        {
            propertyMap["CCD_OFFSET"].remove("OFFSET");
            if (propertyMap["CCD_OFFSET"].size() == 0)
                propertyMap.remove("CCD_OFFSET");
        }
    }
    else
    {
        if (value >= 0)
        {
            QMap<QString, QVariant> ccdOffset = propertyMap["CCD_CONTROLS"];
            ccdOffset["Offset"] = value;
            propertyMap["CCD_CONTROLS"] = ccdOffset;
        }
        else
        {
            propertyMap["CCD_CONTROLS"].remove("Offset");
            if (propertyMap["CCD_CONTROLS"].size() == 0)
                propertyMap.remove("CCD_CONTROLS");
        }
    }

    m_customPropertiesDialog->setCustomProperties(propertyMap);
}

void Camera::setVideoStreamEnabled(bool enabled)
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

void Camera::setCoolerToggled(bool enabled)
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

} // namespace Ekos
