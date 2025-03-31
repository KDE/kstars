/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>
    SPDX-FileCopyrightText: 2024 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "camera.h"
#include "capturemodulestate.h"

#include "Options.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "auxiliary/ksmessagebox.h"
#include "ekos/manager.h"
#include "ekos/auxiliary/darklibrary.h"
#include "ekos/auxiliary/filtermanager.h"
#include "ekos/auxiliary/opticaltrainmanager.h"
#include "ekos/auxiliary/opticaltrainsettings.h"
#include "ekos/auxiliary/profilesettings.h"
#include "ekos/auxiliary/rotatorutils.h"
#include "capturedeviceadaptor.h"
#include "cameraprocess.h"
#include "sequencequeue.h"
#include "scriptsmanager.h"
#include "dslrinfodialog.h"
#include "ui_dslrinfo.h"
#include "exposurecalculator/exposurecalculatordialog.h"

#include "indi/driverinfo.h"
#include "indi/indirotator.h"
#include "oal/observeradd.h"

#include <ekos_capture_debug.h>

#include <QFileDialog>

// These strings are used to store information in the optical train
// for later use in the stand-alone esq editor.

#define KEY_FORMATS     "formatsList"
#define KEY_GAIN_KWD    "ccdGainKeyword"
#define KEY_OFFSET_KWD  "ccdOffsetKeyword"
#define KEY_TEMPERATURE "ccdTemperatures"
#define KEY_TIMESTAMP   "timestamp"
#define KEY_FILTERS     "filtersList"
#define KEY_ISOS        "isoList"
#define KEY_INDEX       "isoIndex"
#define KEY_GAIN_KWD    "ccdGainKeyword"
#define KEY_OFFSET_KWD  "ccdOffsetKeyword"

#define QCDEBUG   qCDebug(KSTARS_EKOS_CAPTURE) << QString("[%1]").arg(getCameraName())
#define QCINFO    qCInfo(KSTARS_EKOS_CAPTURE) << QString("[%1]").arg(getCameraName())
#define QCWARNING qCWarning(KSTARS_EKOS_CAPTURE) << QString("[%1]").arg(getCameraName())

namespace
{
const QStringList standAloneKeys = {KEY_FORMATS, KEY_GAIN_KWD, KEY_OFFSET_KWD,
                                    KEY_TEMPERATURE, KEY_TIMESTAMP, KEY_FILTERS,
                                    KEY_ISOS, KEY_INDEX, KEY_GAIN_KWD, KEY_OFFSET_KWD
                                   };
QVariantMap copyStandAloneSettings(const QVariantMap &settings)
{
    QVariantMap foundSettings;
    for (const auto &k : standAloneKeys)
    {
        auto v = settings.find(k);
        if (v != settings.end())
            foundSettings[k] = *v;
    }
    return foundSettings;
}
}  // namespace

namespace Ekos
{

Camera::Camera(int id, bool standAlone, QWidget *parent)
    : QWidget{parent}, m_standAlone(standAlone), m_cameraId(id)
{
    init();
}

Camera::Camera(bool standAlone, QWidget *parent) : QWidget{parent}, m_standAlone(standAlone), m_cameraId(0)
{
    init();
}

void Ekos::Camera::init()
{
    setupUi(this);
    m_cameraState.reset(new CameraState());
    m_DeviceAdaptor.reset(new CaptureDeviceAdaptor());
    m_cameraProcess.reset(new CameraProcess(state(), m_DeviceAdaptor));

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

    // set units for video recording limit
    videoDurationUnitCB->addItem(i18n("sec"));
    videoDurationUnitCB->addItem(i18n("frames"));

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

    // init connections and load settings
    initCamera();
    initOpticalTrain();
}

Camera::~Camera()
{
    foreach (auto job, state()->allJobs())
        job->deleteLater();

    state()->allJobs().clear();
}

void Camera::initCamera()
{
    KStarsData::Instance()->userdb()->GetAllDSLRInfos(state()->DSLRInfos());
    state()->getSequenceQueue()->loadOptions();

    if (state()->DSLRInfos().count() > 0)
    {
        QCDEBUG << "DSLR Cameras Info:";
        QCDEBUG << state()->DSLRInfos();
    }

    setupOpticalTrainManager();

    // Generate Meridian Flip State
    state()->getMeridianFlipState();

    m_dirPath = QUrl::fromLocalFile(QDir::homePath());

    connect(startB, &QPushButton::clicked, this, &Camera::toggleSequence);
    connect(pauseB, &QPushButton::clicked, this, &Camera::pause);
    connect(previewB, &QPushButton::clicked, this, &Camera::capturePreview);
    connect(loopB, &QPushButton::clicked, this, &Camera::startFraming);

    connect(captureBinHN, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), captureBinVN,
            &QSpinBox::setValue);
    connect(liveVideoB, &QPushButton::clicked, this, &Camera::toggleVideo);

    connect(clearConfigurationB, &QPushButton::clicked, this, &Camera::clearCameraConfiguration);
    connect(restartCameraB, &QPushButton::clicked, this, [this]()
    {
        if (activeCamera())
            restartCamera(activeCamera()->getDeviceName());
    });
    connect(queueLoadB, &QPushButton::clicked, this, static_cast<void(Camera::*)()>(&Camera::loadSequenceQueue));
    connect(resetB, &QPushButton::clicked, this, &Camera::resetJobs);
    connect(queueSaveB, &QPushButton::clicked, this,
            static_cast<void(Camera::*)()>(&Camera::saveSequenceQueue));
    connect(queueSaveAsB, &QPushButton::clicked, this, &Camera::saveSequenceQueueAs);
    connect(queueTable->selectionModel(), &QItemSelectionModel::currentRowChanged, this,
            &Camera::selectedJobChanged);
    connect(queueTable, &QAbstractItemView::doubleClicked, this, &Camera::editJob);
    connect(queueTable, &QTableWidget::itemSelectionChanged, this, [&]()
    {
        resetJobEdit(m_JobUnderEdit);
    });
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

    connect(resetFrameB, &QPushButton::clicked, m_cameraProcess.get(), &CameraProcess::resetFrame);
    connect(calibrationB, &QPushButton::clicked, m_CalibrationDialog, &QDialog::show);

    connect(generateDarkFlatsB, &QPushButton::clicked, this, &Camera::generateDarkFlats);

    connect(&m_DebounceTimer, &QTimer::timeout, this, &Camera::settleSettings);

    connect(FilterPosCombo,
            static_cast<void(QComboBox::*)(const QString &)>(&QComboBox::currentTextChanged),
            [ = ]()
    {
        state()->updateHFRThreshold();
        generatePreviewFilename();
    });

    connect(filterEditB, &QPushButton::clicked, this, &Camera::editFilterName);

    connect(exposureCalcB, &QPushButton::clicked, this, &Camera::openExposureCalculatorDialog);

    // avoid combination of CAPTURE_PREACTION_WALL and CAPTURE_PREACTION_PARK_MOUNT
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

    // video duration unit changes
    connect(videoDurationUnitCB, static_cast<void (QComboBox::*)(int) > (&QComboBox::currentIndexChanged), this,
            &Camera::updateVideoDurationUnit);

    // Autofocus HFR Check
    connect(m_LimitsUI->enforceAutofocusHFR, &QCheckBox::toggled, [ = ](bool checked)
    {
        if (checked == false)
            state()->getRefocusState()->setInSequenceFocus(false);
    });

    connect(removeFromQueueB, &QPushButton::clicked, this, &Camera::removeJobFromQueue);
    connect(selectFileDirectoryB, &QPushButton::clicked, this, &Camera::saveFITSDirectory);

    connect(m_cameraState.get(), &CameraState::newLimitFocusHFR, this, [this](double hfr)
    {
        m_LimitsUI->hFRDeviation->setValue(hfr);
    });

    state()->getCaptureDelayTimer().setSingleShot(true);
    connect(&state()->getCaptureDelayTimer(), &QTimer::timeout, m_cameraProcess.get(), &CameraProcess::captureImage);

    // Exposure Timeout
    state()->getCaptureTimeout().setSingleShot(true);
    connect(&state()->getCaptureTimeout(), &QTimer::timeout, m_cameraProcess.get(),
            &CameraProcess::processCaptureTimeout);

    // Local/Remote directory
    connect(fileUploadModeS, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
            &Camera::checkUploadMode);

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

    // display the capture status in the UI
    connect(m_cameraState.data(), &CameraState::newStatus, captureStatusWidget, &LedStatusWidget::setCaptureState);

    ////////////////////////////////////////////////////////////////////////
    /// Settings
    ////////////////////////////////////////////////////////////////////////
    loadGlobalSettings();
    connectSyncSettings();

    connect(m_LimitsUI->hFRThresholdPercentage, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this]()
    {
        updateHFRCheckAlgo();
    });

    updateHFRCheckAlgo();
    connect(m_LimitsUI->hFRCheckAlgorithm, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int)
    {
        updateHFRCheckAlgo();
    });

    connect(observerB, &QPushButton::clicked, this, &Camera::showObserverDialog);

    connect(fileDirectoryT, &QLineEdit::textChanged, this, [&]()
    {
        generatePreviewFilename();
    });

    connect(fileRemoteDirT, &QLineEdit::textChanged, this, [&]()
    {
        generatePreviewFilename();
    });
    connect(formatSuffixN, QOverload<int>::of(&QSpinBox::valueChanged), this, &Camera::generatePreviewFilename);
    connect(captureExposureN, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            &Camera::generatePreviewFilename);
    connect(targetNameT, &QLineEdit::textEdited, this, [ = ]()
    {
        generatePreviewFilename();
        QCDEBUG << "Changed target to" << targetNameT->text() << "because of user edit";
    });
    connect(captureTypeS, &QComboBox::currentTextChanged, this, &Camera::generatePreviewFilename);
    placeholderFormatT->setText(Options::placeholderFormat());
    connect(placeholderFormatT, &QLineEdit::textChanged, this, [this]()
    {
        generatePreviewFilename();
    });

    // state machine connections
    connect(m_cameraState.data(), &CameraState::captureBusy, this, &Camera::setBusy);
    connect(m_cameraState.data(), &CameraState::startCapture, this, &Camera::start);
    connect(m_cameraState.data(), &CameraState::abortCapture, this, &Camera::abort);
    connect(m_cameraState.data(), &CameraState::suspendCapture, this, &Camera::suspend);
    connect(m_cameraState.data(), &CameraState::newLog, this, &Camera::appendLogText);
    connect(m_cameraState.data(), &CameraState::sequenceChanged, this, &Camera::sequenceChanged);
    connect(m_cameraState.data(), &CameraState::updatePrepareState, this, &Camera::updatePrepareState);
    connect(m_cameraState.data(), &CameraState::newFocusStatus, this, &Camera::updateFocusStatus);
    connect(m_cameraState.data(), &CameraState::runAutoFocus, this,
            [&](AutofocusReason autofocusReason, const QString & reasonInfo)
    {
        emit runAutoFocus(autofocusReason, reasonInfo, opticalTrain());
    });
    connect(m_cameraState.data(), &CameraState::resetFocusFrame, this, [&]()
    {
        emit resetFocusFrame(opticalTrain());
    });
    connect(m_cameraState.data(), &CameraState::adaptiveFocus, this, [&]()
    {
        emit adaptiveFocus(opticalTrain());
    });
    connect(m_cameraProcess.data(), &CameraProcess::abortFocus, this, [&]()
    {
        emit abortFocus(opticalTrain());
    });
    connect(m_cameraState.data(), &CameraState::newMeridianFlipStage, this, &Camera::updateMeridianFlipStage);
    connect(m_cameraState.data(), &CameraState::guideAfterMeridianFlip, this, &Camera::guideAfterMeridianFlip);
    connect(m_cameraState.data(), &CameraState::resetNonGuidedDither, this, [&]()
    {
        // only the main camera sends guiding controls
        if (cameraId() == 0)
            emit resetNonGuidedDither();
    });
    connect(m_cameraState.data(), &CameraState::newStatus, this, &Camera::updateCameraStatus);
    connect(m_cameraState.data(), &CameraState::meridianFlipStarted, this, [&]()
    {
        emit meridianFlipStarted(opticalTrain());
    });
    // process engine connections
    connect(m_cameraProcess.data(), &CameraProcess::refreshCamera, this, &Camera::updateCamera);
    connect(m_cameraProcess.data(), &CameraProcess::sequenceChanged, this, &Camera::sequenceChanged);
    connect(m_cameraProcess.data(), &CameraProcess::addJob, this, &Camera::addJob);
    connect(m_cameraProcess.data(), &CameraProcess::newExposureProgress, this, [&](const QSharedPointer<SequenceJob> &job)
    {
        emit newExposureProgress(job, opticalTrain());
    });
    connect(m_cameraProcess.data(), &CameraProcess::newDownloadProgress, this, [&](double downloadTimeLeft)
    {
        emit updateDownloadProgress(downloadTimeLeft, opticalTrain());
    });
    connect(m_cameraProcess.data(), &CameraProcess::newImage, this, [&](const QSharedPointer<SequenceJob> &job,
            const QSharedPointer<FITSData> data)
    {
        emit newImage(job, data, opticalTrain());
    });
    connect(m_cameraProcess.data(), &CameraProcess::captureComplete, this, [&](const QVariantMap & metadata)
    {
        emit captureComplete(metadata, opticalTrain());
    });
    connect(m_cameraProcess.data(), &CameraProcess::updateCaptureCountDown, this, &Camera::updateCaptureCountDown);
    connect(m_cameraProcess.data(), &CameraProcess::processingFITSfinished, this, &Camera::processingFITSfinished);
    connect(m_cameraProcess.data(), &CameraProcess::captureStopped, this, &Camera::captureStopped);
    connect(m_cameraProcess.data(), &CameraProcess::darkFrameCompleted, this, &Camera::imageCapturingCompleted);
    connect(m_cameraProcess.data(), &CameraProcess::updateMeridianFlipStage, this, &Camera::updateMeridianFlipStage);
    connect(m_cameraProcess.data(), &CameraProcess::syncGUIToJob, this, &Camera::syncGUIToJob);
    connect(m_cameraProcess.data(), &CameraProcess::refreshCameraSettings, this, &Camera::refreshCameraSettings);
    connect(m_cameraProcess.data(), &CameraProcess::updateFrameProperties, this, &Camera::updateFrameProperties);
    connect(m_cameraProcess.data(), &CameraProcess::rotatorReverseToggled, this, &Camera::setRotatorReversed);
    connect(m_cameraProcess.data(), &CameraProcess::refreshFilterSettings, this, &Camera::refreshFilterSettings);
    connect(m_cameraProcess.data(), &CameraProcess::suspendGuiding, this, &Camera::suspendGuiding);
    connect(m_cameraProcess.data(), &CameraProcess::resumeGuiding, this, &Camera::resumeGuiding);
    connect(m_cameraProcess.data(), &CameraProcess::driverTimedout, this, &Camera::driverTimedout);
    connect(m_cameraProcess.data(), &CameraProcess::createJob, [this](SequenceJob::SequenceJobType jobType)
    {
        // report the result back to the process
        process()->jobCreated(createJob(jobType));
    });
    connect(m_cameraProcess.data(), &CameraProcess::updateJobTable, this, &Camera::updateJobTable);
    connect(m_cameraProcess.data(), &CameraProcess::newLog, this, &Camera::appendLogText);
    connect(m_cameraProcess.data(), &CameraProcess::stopCapture, this, &Camera::stop);
    connect(m_cameraProcess.data(), &CameraProcess::jobStarting, this, &Camera::jobStarting);
    connect(m_cameraProcess.data(), &CameraProcess::cameraReady, this, &Camera::ready);
    connect(m_cameraProcess.data(), &CameraProcess::captureAborted, this, &Camera::captureAborted);
    connect(m_cameraProcess.data(), &CameraProcess::captureRunning, this, &Camera::captureRunning);
    connect(m_cameraProcess.data(), &CameraProcess::captureImageStarted, this, &Camera::captureImageStarted);
    connect(m_cameraProcess.data(), &CameraProcess::jobExecutionPreparationStarted, this,
            &Camera::jobExecutionPreparationStarted);
    connect(m_cameraProcess.data(), &CameraProcess::jobPrepared, this, &Camera::jobPrepared);
    connect(m_cameraProcess.data(), &CameraProcess::captureTarget, this, &Camera::setTargetName);
    connect(m_cameraProcess.data(), &CameraProcess::downloadingFrame, this, [this]()
    {
        captureStatusWidget->setStatus(i18n("Downloading..."), Qt::yellow);
    });
    connect(m_cameraProcess.data(), &CameraProcess::requestAction, this, [&](CaptureWorkflowActionType action)
    {
        emit requestAction(m_cameraId, action);
    });

    // connections between state machine and process engine
    connect(m_cameraState.data(), &CameraState::executeActiveJob, m_cameraProcess.data(),
            &CameraProcess::executeJob);
    connect(m_cameraState.data(), &CameraState::captureStarted, m_cameraProcess.data(),
            &CameraProcess::captureStarted);
    connect(m_cameraState.data(), &CameraState::requestAction, this, [&](CaptureWorkflowActionType action)
    {
        emit requestAction(m_cameraId, action);
    });
    connect(m_cameraState.data(), &CameraState::checkFocus, this, [&](double hfr)
    {
        emit checkFocus(hfr, opticalTrain());
    });
    connect(m_cameraState.data(), &CameraState::resetNonGuidedDither, this, [&]()
    {
        if (m_cameraId == 0)
            emit resetNonGuidedDither();
    });

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
    connect(m_DeviceAdaptor.data(), &CaptureDeviceAdaptor::newFilterWheel, this, &Camera::setFilterWheel);

    // connections between state machine and device adaptor
    connect(m_cameraState.data(), &CameraState::newFilterPosition,
            m_DeviceAdaptor.data(), &CaptureDeviceAdaptor::setFilterPosition);
    connect(m_cameraState.data(), &CameraState::abortFastExposure,
            m_DeviceAdaptor.data(), &CaptureDeviceAdaptor::abortFastExposure);

    // connections between state machine and device adaptor
    connect(m_DeviceAdaptor.data(), &CaptureDeviceAdaptor::scopeStatusChanged,
            m_cameraState.data(), &CameraState::setScopeState);
    connect(m_DeviceAdaptor.data(), &CaptureDeviceAdaptor::pierSideChanged,
            m_cameraState.data(), &CameraState::setPierSide);
    connect(m_DeviceAdaptor.data(), &CaptureDeviceAdaptor::scopeParkStatusChanged,
            m_cameraState.data(), &CameraState::setScopeParkState);
    connect(m_DeviceAdaptor.data(), &CaptureDeviceAdaptor::domeStatusChanged,
            m_cameraState.data(), &CameraState::setDomeState);
    connect(m_DeviceAdaptor.data(), &CaptureDeviceAdaptor::dustCapStatusChanged,
            m_cameraState.data(), &CameraState::dustCapStateChanged);
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

void Camera::setFocusStatus(FocusState newstate)
{
    // directly forward it to the state machine
    state()->updateFocusState(newstate);
}

void Camera::updateFocusStatus(FocusState newstate)
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

void Camera::updateCamera(bool isValid)
{
    auto isConnected = activeCamera() && activeCamera()->isConnected();
    CaptureSettingsGroup->setEnabled(isConnected);
    fileSettingsGroup->setEnabled(isConnected);
    sequenceBox->setEnabled(isConnected);
    for (auto &oneChild : sequenceControlsButtonGroup->buttons())
        oneChild->setEnabled(isConnected);

    if (isValid)
    {
        auto name = activeCamera()->getDeviceName();
        opticalTrainCombo->setToolTip(QString("%1 @ %2").arg(name, currentScope()["name"].toString()));
        emit settingsUpdated(getAllSettings());
        emit refreshCamera(m_cameraId, true);
    }
    else
        emit refreshCamera(m_cameraId, false);

}

void Camera::refreshCameraSettings()
{
    // Make sure we have a valid chip and valid base device.
    // Make sure we are not in capture process.
    auto camera = activeCamera();
    auto targetChip = devices()->getActiveChip();
    // If camera is restarted, try again in one second
    if (!m_standAlone && (!camera || !targetChip || !targetChip->getCCD() || targetChip->isCapturing()))
    {
        QTimer::singleShot(1000, this, &Camera::refreshCameraSettings);
        return;
    }

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

    customPropertiesDialog()->setCCD(camera);

    liveVideoB->setEnabled(camera->hasVideoStream());
    if (camera->hasVideoStream())
        setVideoStreamEnabled(camera->isStreamingEnabled());
    else
        liveVideoB->setIcon(QIcon::fromTheme("camera-off"));

    connect(camera, &ISD::Camera::propertyUpdated, this, &Camera::processCameraNumber, Qt::UniqueConnection);
    connect(camera, &ISD::Camera::coolerToggled, this, &Camera::setCoolerToggled, Qt::UniqueConnection);
    connect(camera, &ISD::Camera::videoStreamToggled, this, &Camera::setVideoStreamEnabled, Qt::UniqueConnection);
    connect(camera, &ISD::Camera::ready, this, &Camera::ready, Qt::UniqueConnection);
    connect(camera, &ISD::Camera::error, m_cameraProcess.data(), &CameraProcess::processCaptureError,
            Qt::UniqueConnection);

    syncCameraInfo();

    // update values received by the device adaptor
    // connect(activeCamera(), &ISD::Camera::newTemperatureValue, this, &Capture::updateCCDTemperature, Qt::UniqueConnection);

    DarkLibrary::Instance()->checkCamera();
}

void Camera::processCameraNumber(INDI::Property prop)
{
    if (devices()->getActiveCamera() == nullptr)
        return;

    if ((prop.isNameMatch("CCD_FRAME") && state()->useGuideHead() == false) ||
            (prop.isNameMatch("GUIDER_FRAME") && state()->useGuideHead()))
        updateFrameProperties();
    else if ((prop.isNameMatch("CCD_INFO") && state()->useGuideHead() == false) ||
             (prop.isNameMatch("GUIDER_INFO") && state()->useGuideHead()))
        updateFrameProperties(1);
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

void Camera::updateTargetDistance(double targetDiff)
{
    // ensure that the drift is visible
    targetDriftLabel->setVisible(true);
    targetDrift->setVisible(true);
    targetDriftUnit->setVisible(true);
    // update the drift value
    targetDrift->setText(QString("%L1").arg(targetDiff, 0, 'd', 1));
}

namespace
{
QString frameLabel(CCDFrameType type, const QString &filter)
{
    switch(type)
    {
        case FRAME_LIGHT:
        case FRAME_VIDEO:
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

void Camera::captureRunning()
{
    emit captureStarting(activeJob()->getCoreProperty(SequenceJob::SJ_Exposure).toDouble(),
                         activeJob()->getCoreProperty(SequenceJob::SJ_Filter).toString());
    if (isActiveJobPreview())
        frameInfoLabel->setText("Expose (-/-):");
    else if (activeJob()->getFrameType() != FRAME_VIDEO)
        frameInfoLabel->setText(QString("%1 (%L2/%L3):").arg(frameLabel(activeJob()->getFrameType(),
                                activeJob()->getCoreProperty(SequenceJob::SJ_Filter).toString()))
                                .arg(activeJob()->getCompleted()).arg(activeJob()->getCoreProperty(
                                            SequenceJob::SJ_Count).toInt()));
    else
        frameInfoLabel->setText(QString("%1 (%2x%L3s):").arg(frameLabel(activeJob()->getFrameType(),
                                activeJob()->getCoreProperty(SequenceJob::SJ_Filter).toString()))
                                .arg(activeJob()->getCoreProperty(SequenceJob::SJ_Count).toInt())
                                .arg(activeJob()->getCoreProperty(SequenceJob::SJ_Exposure).toDouble(), 0, 'f', 3));

    // ensure that the download time label is visible
    avgDownloadTime->setVisible(true);
    avgDownloadLabel->setVisible(true);
    secLabel->setVisible(true);
    // show estimated download time
    avgDownloadTime->setText(QString("%L1").arg(state()->averageDownloadTime(), 0, 'd', 2));

    // avoid logging that we captured a temporary file
    if (state()->isLooping() == false)
    {
        if (activeJob()->getFrameType() == FRAME_VIDEO)
            appendLogText(i18n("Capturing %1 x %2-second %3 video...",
                               activeJob()->getCoreProperty(SequenceJob::SJ_Count).toInt(),
                               QString("%L1").arg(activeJob()->getCoreProperty(SequenceJob::SJ_Exposure).toDouble(), 0, 'f', 3),
                               activeJob()->getCoreProperty(SequenceJob::SJ_Filter).toString()));

        else if (activeJob()->jobType() != SequenceJob::JOBTYPE_PREVIEW)
            appendLogText(i18n("Capturing %1-second %2 image...",
                               QString("%L1").arg(activeJob()->getCoreProperty(SequenceJob::SJ_Exposure).toDouble(), 0, 'f', 3),
                               activeJob()->getCoreProperty(SequenceJob::SJ_Filter).toString()));
    }
}

void Camera::captureImageStarted()
{
    if (devices()->filterWheel() != nullptr)
    {
        updateCurrentFilterPosition();
    }

    // necessary since the status widget doesn't store the calibration stage
    if (activeJob()->getCalibrationStage() == SequenceJobState::CAL_CALIBRATION)
        captureStatusWidget->setStatus(i18n("Calibrating..."), Qt::yellow);
}

void Camera::jobExecutionPreparationStarted()
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

void Camera::jobPrepared(const QSharedPointer<SequenceJob> &job)
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

void Camera::setTargetName(const QString &newTargetName)
{
    // target is changed only if no job is running
    if (activeJob() == nullptr)
    {
        // set the target name in the currently selected job
        targetNameT->setText(newTargetName);
        auto rows = queueTable->selectionModel()->selectedRows();
        if(rows.count() > 0)
        {
            // take the first one, since we are in single selection mode
            int pos = rows.constFirst().row();

            if (state()->allJobs().size() > pos)
                state()->allJobs().at(pos)->setCoreProperty(SequenceJob::SJ_TargetName, newTargetName);
        }

        emit captureTarget(newTargetName);
    }
}

void Camera::jobStarting()
{
    if (m_LimitsUI->enforceAutofocusHFR->isChecked() && state()->getRefocusState()->isAutoFocusReady() == false)
        appendLogText(i18n("Warning: in-sequence focusing is selected but autofocus process was not started."));
    if (m_LimitsUI->enforceAutofocusOnTemperature->isChecked()
            && state()->getRefocusState()->isAutoFocusReady() == false)
        appendLogText(i18n("Warning: temperature delta check is selected but autofocus process was not started."));

    updateStartButtons(true, false);

}

void Camera::capturePreview()
{
    process()->capturePreview();
}

void Camera::startFraming()
{
    process()->capturePreview(true);
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

void Camera::updateDownloadProgress(double downloadTimeLeft, const QString &devicename)
{
    frameRemainingTime->setText(state()->imageCountDown().toString("hh:mm:ss"));
    emit newDownloadProgress(downloadTimeLeft, devicename);
}

void Camera::updateCaptureCountDown(int deltaMillis)
{
    state()->imageCountDownAddMSecs(deltaMillis);
    state()->sequenceCountDownAddMSecs(deltaMillis);
    frameRemainingTime->setText(state()->imageCountDown().toString("hh:mm:ss"));
    if (!isActiveJobPreview())
        jobRemainingTime->setText(state()->sequenceCountDown().toString("hh:mm:ss"));
    else
        jobRemainingTime->setText("--:--:--");
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

void Camera::imageCapturingCompleted()
{
    if (activeJob().isNull())
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
    if (activeJob()->getCoreProperty(SequenceJob::SJ_Exposure).toDouble() >= 1)
        KSNotification::event(QLatin1String("EkosCaptureImageReceived"), i18n("Captured image received"),
                              KSNotification::Capture);

    // If it was initially set as pure preview job and NOT as preview for calibration
    if (activeJob()->jobType() == SequenceJob::JOBTYPE_PREVIEW)
        return;

    /* The image progress has now one more capture */
    imgProgress->setValue(activeJob()->getCompleted());
}

void Camera::captureStopped()
{
    imgProgress->reset();
    imgProgress->setEnabled(false);

    frameRemainingTime->setText("--:--:--");
    jobRemainingTime->setText("--:--:--");
    frameInfoLabel->setText(i18n("Expose (-/-):"));

    // stopping to CAPTURE_IDLE means that capturing will continue automatically
    auto captureState = state()->getCaptureState();
    if (captureState == CAPTURE_ABORTED || captureState == CAPTURE_COMPLETE)
        updateStartButtons(false, false);
}

void Camera::processingFITSfinished(bool success)
{
    // do nothing in case of failure
    if (success == false)
        return;

    // If this is a preview job, make sure to enable preview button after
    if (devices()->getActiveCamera()
            && devices()->getActiveCamera()->getUploadMode() != ISD::Camera::UPLOAD_REMOTE)
        previewB->setEnabled(true);

    imageCapturingCompleted();
}

void Camera::checkFrameType(int index)
{
    updateCaptureFormats();

    calibrationB->setEnabled(index != FRAME_LIGHT);
    generateDarkFlatsB->setEnabled(index != FRAME_LIGHT);
    const bool isVideo = captureTypeS->currentText() == CAPTURE_TYPE_VIDEO;
    exposureOptions->setCurrentIndex(isVideo ? 1 : 0);
    exposureOptionsLabel->setToolTip(isVideo ? i18n("Duration of the video sequence") : i18n("Number of images to capture"));
    exposureOptionsLabel->setText(isVideo ? i18n("Duration:") : i18n("Count:"));
    exposureLabel->setToolTip(isVideo ? i18n("Exposure time in seconds of a single video frame") :
                              i18n("Exposure time in seconds for individual images"));

    // enforce the upload mode for videos
    if (isVideo)
        selectUploadMode(ISD::Camera::UPLOAD_REMOTE);
    else
        checkUploadMode(fileUploadModeS->currentIndex());
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

void Camera::setBusy(bool enable)
{
    previewB->setEnabled(!enable);
    loopB->setEnabled(!enable);
    opticalTrainCombo->setEnabled(!enable);
    trainB->setEnabled(!enable);

    foreach (QAbstractButton * button, queueEditButtonGroup->buttons())
        button->setEnabled(!enable);

}

void Camera::updatePrepareState(CaptureState prepareState)
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
            appendLogText(i18n("Waiting for guide drift below %1\"...", Options::startGuideDeviation()));
            captureStatusWidget->setStatus(i18n("Wait for Guider < %1\"...", Options::startGuideDeviation()), Qt::yellow);
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

void Camera::saveSequenceQueueAs()
{
    state()->setSequenceURL(QUrl(""));
    saveSequenceQueue();
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

void Camera::selectUploadMode(int index)
{
    fileUploadModeS->setCurrentIndex(index);
    checkUploadMode(index);
}

void Camera::checkUploadMode(int index)
{
    fileRemoteDirT->setEnabled(index != ISD::Camera::UPLOAD_CLIENT);

    const bool isVideo = captureTypeS->currentText() == CAPTURE_TYPE_VIDEO;

    // Access the underlying model of the combo box
    QStandardItemModel *model = qobject_cast<QStandardItemModel *>(fileUploadModeS->model());
    if (model)
    {
        // restrict selection to local (on the INDI server) for videos
        model->item(0)->setFlags(isVideo ? (model->item(0)->flags() & ~Qt::ItemIsEnabled) :
                                 (model->item(0)->flags() | Qt::ItemIsEnabled));
        model->item(2)->setFlags(isVideo ? (model->item(2)->flags() & ~Qt::ItemIsEnabled) :
                                 (model->item(2)->flags() | Qt::ItemIsEnabled));
    }

    generatePreviewFilename();
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

void Camera::syncCameraInfo()
{
    if (!activeCamera())
        return;

    const QString timestamp = KStarsData::Instance()->lt().toString("yyyy-MM-dd hh:mm");
    storeTrainKeyString(KEY_TIMESTAMP, timestamp);

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

            // Save the camera's temperature parameters for the stand-alone editor.
            const QStringList temperatureList =
                QStringList( { QString::number(min),
                               QString::number(max),
                               isChecked ? "1" : "0" } );
            storeTrainKey(KEY_TEMPERATURE, temperatureList);
        }
        else
        {
            setTemperatureB->setEnabled(false);
            cameraTemperatureN->setReadOnly(true);
            cameraTemperatureS->setEnabled(false);
            cameraTemperatureS->setChecked(false);
            temperatureRegulationB->setEnabled(false);

            // Save default camera temperature parameters for the stand-alone editor.
            const QStringList temperatureList = QStringList( { "-50", "50", "0" } );
            storeTrainKey(KEY_TEMPERATURE, temperatureList);
        }

        double temperature = 0;
        if (activeCamera()->getTemperature(&temperature))
        {
            temperatureOUT->setText(QString("%L1º").arg(temperature, 0, 'f', 1));
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
    captureISOS->setEnabled(false);
    captureISOS->clear();

    // No ISO range available
    if (isoList.isEmpty())
    {
        captureISOS->setEnabled(false);
        if (settings().contains(KEY_ISOS))
        {
            settings().remove(KEY_ISOS);
            m_DebounceTimer.start();
        }
        if (settings().contains(KEY_INDEX))
        {
            settings().remove(KEY_INDEX);
            m_DebounceTimer.start();
        }
    }
    else
    {
        captureISOS->setEnabled(true);
        captureISOS->addItems(isoList);
        const int isoIndex = devices()->getActiveChip()->getISOIndex();
        captureISOS->setCurrentIndex(isoIndex);

        // Save ISO List and index in train settings if different
        storeTrainKey(KEY_ISOS, isoList);
        storeTrainKeyString(KEY_INDEX, QString("%1").arg(isoIndex));

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
            else
                setGain(-1);
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
            else
                setOffset(-1);
        });
    }
    else
    {
        captureOffsetN->setEnabled(false);
        currentOffsetLabel->clear();
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

bool Camera::syncControl(const QVariantMap &settings, const QString &key, QWidget * widget)
{
    QSpinBox *pSB = nullptr;
    QDoubleSpinBox *pDSB = nullptr;
    QCheckBox *pCB = nullptr;
    QComboBox *pComboBox = nullptr;
    QSplitter *pSplitter = nullptr;
    QRadioButton *pRadioButton = nullptr;
    QLineEdit *pLineEdit = nullptr;
    bool ok = true;

    if ((pSB = qobject_cast<QSpinBox *>(widget)))
    {
        const int value = settings[key].toInt(&ok);
        if (ok)
        {
            pSB->setValue(value);
            return true;
        }
    }
    else if ((pDSB = qobject_cast<QDoubleSpinBox *>(widget)))
    {
        const double value = settings[key].toDouble(&ok);
        if (ok)
        {
            pDSB->setValue(value);
            // Special case for gain
            if (pDSB == captureGainN)
            {
                if (captureGainN->value() != GainSpinSpecialValue)
                    setGain(captureGainN->value());
                else
                    setGain(-1);
            }
            else if (pDSB == captureOffsetN)
            {
                if (captureOffsetN->value() != OffsetSpinSpecialValue)
                    setOffset(captureOffsetN->value());
                else
                    setOffset(-1);
            }
            return true;
        }
    }
    else if ((pCB = qobject_cast<QCheckBox *>(widget)))
    {
        const bool value = settings[key].toBool();
        if (value != pCB->isChecked())
            pCB->setChecked(value);
        return true;
    }
    else if ((pRadioButton = qobject_cast<QRadioButton *>(widget)))
    {
        const bool value = settings[key].toBool();
        if (value)
            pRadioButton->setChecked(true);
        return true;
    }
    // ONLY FOR STRINGS, not INDEX
    else if ((pComboBox = qobject_cast<QComboBox *>(widget)))
    {
        const QString value = settings[key].toString();
        pComboBox->setCurrentText(value);
        return true;
    }
    else if ((pSplitter = qobject_cast<QSplitter *>(widget)))
    {
        const auto value = QByteArray::fromBase64(settings[key].toString().toUtf8());
        pSplitter->restoreState(value);
        return true;
    }
    else if ((pLineEdit = qobject_cast<QLineEdit *>(widget)))
    {
        const auto value = settings[key].toString();
        pLineEdit->setText(value);
        // Special case
        if (pLineEdit == fileRemoteDirT)
            generatePreviewFilename();
        return true;
    }

    return false;
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

    if (currentRow > queueTable->rowCount())
        queueTable->selectRow(queueTable->rowCount() - 1);
    else
        queueTable->selectRow(currentRow);
}

void Camera::saveFITSDirectory()
{
    QString dir = QFileDialog::getExistingDirectory(Manager::Instance(), i18nc("@title:window", "FITS Save Directory"),
                  m_dirPath.toLocalFile());
    if (dir.isEmpty())
        return;

    fileDirectoryT->setText(QDir::toNativeSeparators(dir));
}

void Camera::updateCaptureFormats()
{
    if (!activeCamera()) return;

    // list of capture types
    QStringList frameTypes = process()->frameTypes();
    // current selection
    QString currentType = captureTypeS->currentText();

    captureTypeS->blockSignals(true);
    captureTypeS->clear();

    if (frameTypes.isEmpty())
        captureTypeS->setEnabled(false);
    else
    {
        captureTypeS->setEnabled(true);
        captureTypeS->addItems(frameTypes);

        if (currentType == "")
        {
            // if no capture type is selected, take the value from the active chip
            captureTypeS->setCurrentIndex(devices()->getActiveChip()->getFrameType());
            currentType = captureTypeS->currentText();
        }
        else
            captureTypeS->setCurrentText(currentType);
    }
    captureTypeS->blockSignals(false);

    const bool isVideo = currentType == CAPTURE_TYPE_VIDEO;

    // Capture Format
    captureFormatS->blockSignals(true);
    captureFormatS->clear();
    const auto list = isVideo ? activeCamera()->getVideoFormats() : activeCamera()->getCaptureFormats();
    captureFormatS->addItems(list);
    storeTrainKey(KEY_FORMATS, list);
    captureFormatS->setCurrentText(activeCamera()->getCaptureFormat());
    captureFormatS->blockSignals(false);

    // Encoding format
    captureEncodingS->blockSignals(true);
    captureEncodingS->clear();
    captureEncodingS->addItems(isVideo ? activeCamera()->getStreamEncodings() : activeCamera()->getEncodingFormats());
    captureEncodingS->setCurrentText(isVideo ? activeCamera()->getStreamEncoding() : activeCamera()->getEncodingFormat());
    captureEncodingS->blockSignals(false);
}

void Camera::updateHFRCheckAlgo()
{
    // Threshold % is not relevant for FIXED HFR do disable the field
    const bool threshold = (m_LimitsUI->hFRCheckAlgorithm->currentIndex() != HFR_CHECK_FIXED);
    m_LimitsUI->hFRThresholdPercentage->setEnabled(threshold);
    m_LimitsUI->limitFocusHFRThresholdLabel->setEnabled(threshold);
    m_LimitsUI->limitFocusHFRPercentLabel->setEnabled(threshold);
    state()->updateHFRThreshold();
}

void Camera::clearAutoFocusHFR()
{
    if (Options::hFRCheckAlgorithm() == HFR_CHECK_FIXED)
        return;

    m_LimitsUI->hFRDeviation->setValue(0);
    //firstAutoFocus = true;
}

void Camera::selectedJobChanged(QModelIndex current, QModelIndex previous)
{
    Q_UNUSED(previous)
    selectJob(current);
}

void Camera::clearCameraConfiguration()
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

void Camera::editFilterName()
{
    if (m_standAlone)
    {
        QStringList labels;
        for (int index = 0; index < FilterPosCombo->count(); index++)
            labels << FilterPosCombo->itemText(index);
        QStringList newLabels;
        if (editFilterNameInternal(labels, newLabels))
        {
            FilterPosCombo->clear();
            FilterPosCombo->addItems(newLabels);
        }
    }
    else
    {
        if (devices()->filterWheel() == nullptr || state()->getCurrentFilterPosition() < 1)
            return;

        QStringList labels = filterManager()->getFilterLabels();
        QStringList newLabels;
        if (editFilterNameInternal(labels, newLabels))
            filterManager()->setFilterNames(newLabels);
    }
}

bool Camera::editFilterNameInternal(const QStringList &labels, QStringList &newLabels)
{
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

    QString title = m_standAlone ?
                    "Edit Filter Names" : devices()->filterWheel()->getDeviceName();
    filterDialog.setWindowTitle(title);
    filterDialog.setLayout(formLayout);
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &filterDialog);
    connect(buttonBox, &QDialogButtonBox::accepted, &filterDialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &filterDialog, &QDialog::reject);
    filterDialog.layout()->addWidget(buttonBox);

    if (filterDialog.exec() == QDialog::Accepted)
    {
        QStringList results;
        for (uint8_t i = 0; i < labels.count(); i++)
            results << newLabelEdits[i]->text();
        newLabels = results;
        return true;
    }
    return false;
}

void Camera::updateVideoDurationUnit()
{
    if (videoDurationUnitCB->currentIndex() == 0)
    {
        // switching from frames to seconds
        videoDurationSB->setValue(videoDurationSB->value() * captureExposureN->value());
        videoDurationSB->setDecimals(2);
    }
    else
    {
        // switching from seconds to frames
        videoDurationSB->setValue(videoDurationSB->value() / captureExposureN->value());
        videoDurationSB->setDecimals(0);
    }
}

void Camera::loadGlobalSettings()
{
    QString key;
    QVariant value;

    QVariantMap settings;
    // All Combo Boxes
    for (auto &oneWidget : findChildren<QComboBox*>())
    {
        if (oneWidget->objectName() == "opticalTrainCombo")
            continue;

        key = oneWidget->objectName();
        value = Options::self()->property(key.toLatin1());
        if (value.isValid() && oneWidget->count() > 0)
        {
            oneWidget->setCurrentText(value.toString());
            settings[key] = value;
        }
        else
            QCDEBUG << "Option" << key << "not found!";
    }

    // All Double Spin Boxes
    for (auto &oneWidget : findChildren<QDoubleSpinBox*>())
    {
        key = oneWidget->objectName();
        value = Options::self()->property(key.toLatin1());
        if (value.isValid())
        {
            oneWidget->setValue(value.toDouble());
            settings[key] = value;
        }
        else
            QCDEBUG << "Option" << key << "not found!";
    }

    // All Spin Boxes
    for (auto &oneWidget : findChildren<QSpinBox*>())
    {
        key = oneWidget->objectName();
        value = Options::self()->property(key.toLatin1());
        if (value.isValid())
        {
            oneWidget->setValue(value.toInt());
            settings[key] = value;
        }
        else
            QCDEBUG << "Option" << key << "not found!";
    }

    // All Checkboxes
    for (auto &oneWidget : findChildren<QCheckBox*>())
    {
        key = oneWidget->objectName();
        value = Options::self()->property(key.toLatin1());
        if (value.isValid())
        {
            oneWidget->setChecked(value.toBool());
            settings[key] = value;
        }
        else
            QCDEBUG << "Option" << key << "not found!";
    }

    // All Checkable Groupboxes
    for (auto &oneWidget : findChildren<QGroupBox*>())
    {
        if (oneWidget->isCheckable())
        {
            key = oneWidget->objectName();
            value = Options::self()->property(key.toLatin1());
            if (value.isValid())
            {
                oneWidget->setChecked(value.toBool());
                settings[key] = value;
            }
            else
                QCDEBUG << "Option" << key << "not found!";
        }
    }

    // All Radio buttons
    for (auto &oneWidget : findChildren<QRadioButton*>())
    {
        key = oneWidget->objectName();
        value = Options::self()->property(key.toLatin1());
        if (value.isValid())
        {
            oneWidget->setChecked(value.toBool());
            settings[key] = value;
        }
    }

    // All Line Edits
    for (auto &oneWidget : findChildren<QLineEdit*>())
    {
        if (oneWidget->objectName() == "qt_spinbox_lineedit" || oneWidget->isReadOnly())
            continue;
        key = oneWidget->objectName();
        value = Options::self()->property(key.toLatin1());
        if (value.isValid())
        {
            oneWidget->setText(value.toString());
            settings[key] = value;
        }
    }
    m_GlobalSettings = settings;
    setSettings(settings);
}

bool Camera::checkUploadPaths(FilenamePreviewType filenamePreview, const QSharedPointer<SequenceJob> &job)
{
    // only relevant if we do not generate file name previews
    if (filenamePreview != FILENAME_NOT_PREVIEW)
        return true;

    if (fileUploadModeS->currentIndex() != ISD::Camera::UPLOAD_CLIENT && job->getRemoteDirectory().toString().isEmpty())
    {
        KSNotification::error(i18n("You must set local or remote directory for Remotely & Both modes."));
        return false;
    }

    if (fileUploadModeS->currentIndex() != ISD::Camera::UPLOAD_REMOTE && fileDirectoryT->text().isEmpty())
    {
        KSNotification::error(i18n("You must set local directory for Locally & Both modes."));
        return false;
    }
    // everything OK
    return true;
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

void Camera::generatePreviewFilename()
{
    if (state()->isCaptureRunning() == false)
    {
        placeholderFormatT->setToolTip(previewFilename( fileUploadModeS->currentIndex() == ISD::Camera::UPLOAD_REMOTE ?
                                       FILENAME_REMOTE_PREVIEW : FILENAME_LOCAL_PREVIEW ));
        emit newLocalPreview(placeholderFormatT->toolTip());

        if (fileUploadModeS->currentIndex() != ISD::Camera::UPLOAD_CLIENT)
            fileRemoteDirT->setToolTip(previewFilename( FILENAME_REMOTE_PREVIEW ));
    }
}

QString Camera::previewFilename(FilenamePreviewType previewType)
{
    QString previewText;
    QString m_format;
    auto separator = QDir::separator();

    if (previewType == FILENAME_LOCAL_PREVIEW)
    {
        if(!fileDirectoryT->text().endsWith(separator) && !placeholderFormatT->text().startsWith(separator))
            placeholderFormatT->setText(separator + placeholderFormatT->text());
        m_format = fileDirectoryT->text() + placeholderFormatT->text() + formatSuffixN->prefix() +
                   formatSuffixN->cleanText();
    }
    else if (previewType == FILENAME_REMOTE_PREVIEW)
        m_format = fileRemoteDirT->text().isEmpty() ? fileDirectoryT->text() : fileRemoteDirT->text();

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
        QSharedPointer<SequenceJob> m_job = createJob(SequenceJob::JOBTYPE_PREVIEW, previewType);
        if (m_job.isNull())
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
        previewText = m_placeholderPath.generateSequenceFilename(*m_job, previewType == FILENAME_LOCAL_PREVIEW, true, 1,
                      extension, "", false);
        previewText = QDir::toNativeSeparators(previewText);
        // we do not use it any more
        m_job->deleteLater();
    }

    // Must change directory separate to UNIX style for remote
    if (previewType == FILENAME_REMOTE_PREVIEW)
        previewText.replace(separator, "/");

    return previewText;
}

void Camera::updateJobTableCountCell(const QSharedPointer<SequenceJob> &job, QTableWidgetItem * countCell)
{
    countCell->setText(QString("%L1/%L2").arg(job->getCompleted()).arg(job->getCoreProperty(SequenceJob::SJ_Count).toInt()));
}

void Camera::addDSLRInfo(const QString &model, uint32_t maxW, uint32_t maxH, double pixelW, double pixelH)
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
    if (m_DSLRInfoDialog)
        m_DSLRInfoDialog.reset();
}

void Camera::start()
{
    process()->startNextPendingJob();
}

void Camera::stop(CaptureState targetState)
{
    process()->stopCapturing(targetState);
}

void Camera::pause()
{
    process()->pauseCapturing();
    updateStartButtons(false, true);
}

void Camera::toggleSequence()
{
    const CaptureState capturestate = state()->getCaptureState();
    if (capturestate == CAPTURE_PAUSE_PLANNED || capturestate == CAPTURE_PAUSED)
        updateStartButtons(true, false);

    process()->toggleSequence();
}

void Camera::toggleVideo(bool enabled)
{
    process()->toggleVideo(enabled);
}

void Camera::restartCamera(const QString &name)
{
    process()->restartCamera(name);
}

void Camera::cullToDSLRLimits()
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

void Camera::resetFrameToZero()
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

void Camera::updateFrameProperties(int reset)
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

    const QString ccdGainKeyword = devices()->getActiveCamera()->getProperty("CCD_GAIN") ? "CCD_GAIN" : "CCD_CONTROLS";
    storeTrainKeyString(KEY_GAIN_KWD, ccdGainKeyword);

    const QString ccdOffsetKeyword = devices()->getActiveCamera()->getProperty("CCD_OFFSET") ? "CCD_OFFSET" : "CCD_CONTROLS";
    storeTrainKeyString(KEY_OFFSET_KWD, ccdOffsetKeyword);

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
        m_RotatorControlPanel->initRotator(opticalTrainCombo->currentText(), devices(), Rotator);
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

void Camera::setMaximumGuidingDeviation(bool enable, double value)
{
    m_LimitsUI->enforceGuideDeviation->setChecked(enable);
    if (enable)
        m_LimitsUI->guideDeviation->setValue(value);
}

void Camera::setInSequenceFocus(bool enable, double HFR)
{
    m_LimitsUI->enforceAutofocusHFR->setChecked(enable);
    if (enable)
        m_LimitsUI->hFRDeviation->setValue(HFR);
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

bool Camera::saveSequenceQueue(const QString &path)
{
    // forward it to the process engine
    return process()->saveSequenceQueue(path);
}

void Camera::updateCameraStatus(CaptureState status)
{
    // forward a status change
    emit newStatus(status, opticalTrain(), cameraId());
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

void Camera::setAllSettings(const QVariantMap &settings, const QVariantMap * standAloneSettings)
{
    // Disconnect settings that we don't end up calling syncSettings while
    // performing the changes.
    disconnectSyncSettings();

    for (auto &name : settings.keys())
    {
        // Combo
        auto comboBox = findChild<QComboBox*>(name);
        if (comboBox)
        {
            syncControl(settings, name, comboBox);
            continue;
        }

        // Double spinbox
        auto doubleSpinBox = findChild<QDoubleSpinBox*>(name);
        if (doubleSpinBox)
        {
            syncControl(settings, name, doubleSpinBox);
            continue;
        }

        // spinbox
        auto spinBox = findChild<QSpinBox*>(name);
        if (spinBox)
        {
            syncControl(settings, name, spinBox);
            continue;
        }

        // checkbox
        auto checkbox = findChild<QCheckBox*>(name);
        if (checkbox)
        {
            syncControl(settings, name, checkbox);
            continue;
        }

        // Checkable Groupboxes
        auto groupbox = findChild<QGroupBox*>(name);
        if (groupbox && groupbox->isCheckable())
        {
            syncControl(settings, name, groupbox);
            continue;
        }

        // Radio button
        auto radioButton = findChild<QRadioButton*>(name);
        if (radioButton)
        {
            syncControl(settings, name, radioButton);
            continue;
        }

        // Line Edit
        auto lineEdit = findChild<QLineEdit*>(name);
        if (lineEdit)
        {
            syncControl(settings, name, lineEdit);
            continue;
        }
    }

    // Sync to options
    for (auto &key : settings.keys())
    {
        auto value = settings[key];
        // Save immediately
        Options::self()->setProperty(key.toLatin1(), value);

        m_settings[key] = value;
        m_GlobalSettings[key] = value;
    }

    if (standAloneSettings && standAloneSettings->size() > 0)
    {
        for (const auto &k : standAloneSettings->keys())
            m_settings[k] = (*standAloneSettings)[k];
    }

    emit settingsUpdated(getAllSettings());

    // Save to optical train specific settings as well
    if (!m_standAlone)
    {
        const int id = OpticalTrainManager::Instance()->id(opticalTrainCombo->currentText());
        OpticalTrainSettings::Instance()->setOpticalTrainID(id);
        OpticalTrainSettings::Instance()->setOneSetting(OpticalTrainSettings::Capture, m_settings);
        Options::setCaptureTrainID(id);
    }
    // Restablish connections
    connectSyncSettings();
}

void Camera::setupOpticalTrainManager()
{
    connect(OpticalTrainManager::Instance(), &OpticalTrainManager::updated, this, [this]()
    {
        const int current = opticalTrainCombo->currentIndex();
        initOpticalTrain();
        if (current >= 0 && current < opticalTrainCombo->count())
            selectOpticalTrain(opticalTrainCombo->itemText(current));
    });
    connect(trainB, &QPushButton::clicked, this, [this]()
    {
        OpticalTrainManager::Instance()->openEditor(opticalTrainCombo->currentText());
    });
    connect(opticalTrainCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index)
    {
        const int id = OpticalTrainManager::Instance()->id(opticalTrainCombo->itemText(index));
        // remember only the train ID from the first Capture tab
        if (!m_standAlone && cameraId() == 0)
        {
            ProfileSettings::Instance()->setOneSetting(ProfileSettings::CaptureOpticalTrain, id);
        }
        refreshOpticalTrain(id);
        emit trainChanged();
    });
}

void Camera::initOpticalTrain()
{
    opticalTrainCombo->blockSignals(true);
    opticalTrainCombo->clear();
    opticalTrainCombo->addItems(OpticalTrainManager::Instance()->getTrainNames());
    trainB->setEnabled(true);
    QVariant trainID = ProfileSettings::Instance()->getOneSetting(ProfileSettings::CaptureOpticalTrain);
    if (m_standAlone || trainID.isValid())
    {
        auto id = m_standAlone ? Options::captureTrainID() : trainID.toUInt();
        Options::setCaptureTrainID(id);

        // If train not found, select the first one available.
        if (OpticalTrainManager::Instance()->exists(id) == false)
        {
            qCWarning(KSTARS_EKOS_CAPTURE) << "Optical train doesn't exist for id" << id;
            id = OpticalTrainManager::Instance()->id(opticalTrainCombo->itemText(0));
        }

        refreshOpticalTrain(id);
    }
    opticalTrainCombo->blockSignals(false);
}

void Camera::refreshOpticalTrain(const int id)
{
    auto name = OpticalTrainManager::Instance()->name(id);

    opticalTrainCombo->setCurrentText(name);
    if (!m_standAlone)
        process()->refreshOpticalTrain(name);

    // Load train settings
    // This needs to be done near the start of this function as methods further down
    // cause settings to be updated, which in turn interferes with the persistence and
    // setup of settings in OpticalTrainSettings
    OpticalTrainSettings::Instance()->setOpticalTrainID(id);
    auto settings = OpticalTrainSettings::Instance()->getOneSetting(OpticalTrainSettings::Capture);
    if (settings.isValid())
    {
        auto map = settings.toJsonObject().toVariantMap();
        if (map != m_settings)
        {
            QVariantMap standAloneSettings = copyStandAloneSettings(m_settings);
            m_settings.clear();
            setAllSettings(map, &standAloneSettings);
        }
    }
    else
    {
        setSettings(m_GlobalSettings);
    }
}

void Camera::selectOpticalTrain(QString name)
{
    opticalTrainCombo->setCurrentText(name);
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

void Camera::syncSettings()
{
    QDoubleSpinBox *dsb = nullptr;
    QSpinBox *sb = nullptr;
    QCheckBox *cb = nullptr;
    QGroupBox *gb = nullptr;
    QRadioButton *rb = nullptr;
    QComboBox *cbox = nullptr;
    QLineEdit *le = nullptr;

    QString key;
    QVariant value;

    if ( (dsb = qobject_cast<QDoubleSpinBox*>(sender())))
    {
        key = dsb->objectName();
        value = dsb->value();

    }
    else if ( (sb = qobject_cast<QSpinBox*>(sender())))
    {
        key = sb->objectName();
        value = sb->value();
    }
    else if ( (cb = qobject_cast<QCheckBox*>(sender())))
    {
        key = cb->objectName();
        value = cb->isChecked();
    }
    else if ( (gb = qobject_cast<QGroupBox*>(sender())))
    {
        key = gb->objectName();
        value = gb->isChecked();
    }
    else if ( (rb = qobject_cast<QRadioButton*>(sender())))
    {
        key = rb->objectName();
        // Discard false requests
        if (rb->isChecked() == false)
        {
            settings().remove(key);
            return;
        }
        value = true;
    }
    else if ( (cbox = qobject_cast<QComboBox*>(sender())))
    {
        key = cbox->objectName();
        value = cbox->currentText();
    }
    else if ( (le = qobject_cast<QLineEdit*>(sender())))
    {
        key = le->objectName();
        value = le->text();
    }


    if (!m_standAlone)
    {
        settings()[key] = value;
        m_GlobalSettings[key] = value;
        // Save immediately
        Options::self()->setProperty(key.toLatin1(), value);
        m_DebounceTimer.start();
    }
}

void Camera::connectSyncSettings()
{
    // All Combo Boxes
    for (auto &oneWidget : findChildren<QComboBox*>())
        connect(oneWidget, QOverload<int>::of(&QComboBox::activated), this, &Camera::syncSettings);

    // All Double Spin Boxes
    for (auto &oneWidget : findChildren<QDoubleSpinBox*>())
        connect(oneWidget, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &Camera::syncSettings);

    // All Spin Boxes
    for (auto &oneWidget : findChildren<QSpinBox*>())
        connect(oneWidget, QOverload<int>::of(&QSpinBox::valueChanged), this, &Camera::syncSettings);

    // All Checkboxes
    for (auto &oneWidget : findChildren<QCheckBox*>())
        connect(oneWidget, &QCheckBox::toggled, this, &Camera::syncSettings);

    // All Checkable Groupboxes
    for (auto &oneWidget : findChildren<QGroupBox*>())
        if (oneWidget->isCheckable())
            connect(oneWidget, &QGroupBox::toggled, this, &Camera::syncSettings);

    // All Radio Buttons
    for (auto &oneWidget : findChildren<QRadioButton*>())
        connect(oneWidget, &QRadioButton::toggled, this, &Camera::syncSettings);

    // All Line Edits
    for (auto &oneWidget : findChildren<QLineEdit*>())
    {
        if (oneWidget->objectName() == "qt_spinbox_lineedit" || oneWidget->isReadOnly())
            continue;
        connect(oneWidget, &QLineEdit::textChanged, this, &Camera::syncSettings);
    }

    // Train combo box should NOT be synced.
    disconnect(opticalTrainCombo, QOverload<int>::of(&QComboBox::activated), this, &Ekos::Camera::syncSettings);
}

void Camera::disconnectSyncSettings()
{
    // All Combo Boxes
    for (auto &oneWidget : findChildren<QComboBox*>())
        disconnect(oneWidget, QOverload<int>::of(&QComboBox::activated), this, &Camera::syncSettings);

    // All Double Spin Boxes
    for (auto &oneWidget : findChildren<QDoubleSpinBox*>())
        disconnect(oneWidget, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &Camera::syncSettings);

    // All Spin Boxes
    for (auto &oneWidget : findChildren<QSpinBox*>())
        disconnect(oneWidget, QOverload<int>::of(&QSpinBox::valueChanged), this, &Camera::syncSettings);

    // All Checkboxes
    for (auto &oneWidget : findChildren<QCheckBox*>())
        disconnect(oneWidget, &QCheckBox::toggled, this, &Camera::syncSettings);

    // All Checkable Groupboxes
    for (auto &oneWidget : findChildren<QGroupBox*>())
        if (oneWidget->isCheckable())
            disconnect(oneWidget, &QGroupBox::toggled, this, &Camera::syncSettings);

    // All Radio Buttons
    for (auto &oneWidget : findChildren<QRadioButton*>())
        disconnect(oneWidget, &QRadioButton::toggled, this, &Camera::syncSettings);

    // All Line Edits
    for (auto &oneWidget : findChildren<QLineEdit*>())
    {
        if (oneWidget->objectName() == "qt_spinbox_lineedit")
            continue;
        disconnect(oneWidget, &QLineEdit::textChanged, this, &Camera::syncSettings);
    }
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

void Camera::setupFilterManager()
{
    // Clear connections if there was an existing filter manager
    if (filterManager())
        filterManager()->disconnect(this);

    // Create new global filter manager or refresh its filter wheel
    Manager::Instance()->createFilterManager(devices()->filterWheel());

    // Set the filter manager for this filter wheel.
    Manager::Instance()->getFilterManager(devices()->filterWheel()->getDeviceName(), m_FilterManager);

    devices()->setFilterManager(m_FilterManager);

    connect(filterManager().get(), &FilterManager::updated, this, [this]()
    {
        emit filterManagerUpdated(devices()->filterWheel());
    });

    // display capture status changes
    connect(filterManager().get(), &FilterManager::newStatus, this, &Camera::newFilterStatus);

    connect(filterManagerB, &QPushButton::clicked, this, [this]()
    {
        filterManager()->refreshFilterModel();
        filterManager()->show();
        filterManager()->raise();
    });

    connect(filterManager().get(), &FilterManager::failed, this, [this]()
    {
        if (activeJob())
        {
            appendLogText(i18n("Filter operation failed."));
            abort();
        }
    });

    // filter changes
    connect(filterManager().get(), &FilterManager::newStatus, this, &Camera::setFilterStatus);

    connect(filterManager().get(), &FilterManager::labelsChanged, this, [this]()
    {
        FilterPosCombo->clear();
        FilterPosCombo->addItems(filterManager()->getFilterLabels());
        FilterPosCombo->setCurrentIndex(filterManager()->getFilterPosition() - 1);
        updateCurrentFilterPosition();
        storeTrainKey(KEY_FILTERS, filterManager()->getFilterLabels());
    });

    connect(filterManager().get(), &FilterManager::positionChanged, this, [this]()
    {
        FilterPosCombo->setCurrentIndex(filterManager()->getFilterPosition() - 1);
        updateCurrentFilterPosition();
    });
}

void Camera::clearFilterManager()
{
    // Clear connections if there was an existing filter manager
    if (m_FilterManager)
        m_FilterManager->disconnect(this);

    // clear the filter manager for this camera
    m_FilterManager.clear();
    devices()->clearFilterManager();
}

void Camera::refreshFilterSettings()
{
    FilterPosCombo->clear();

    if (!devices()->filterWheel())
    {
        FilterPosLabel->setEnabled(false);
        FilterPosCombo->setEnabled(false);
        filterEditB->setEnabled(false);
        filterManagerB->setEnabled(false);

        clearFilterManager();
        return;
    }

    FilterPosLabel->setEnabled(true);
    FilterPosCombo->setEnabled(true);
    filterEditB->setEnabled(true);
    filterManagerB->setEnabled(true);

    setupFilterManager();

    const auto labels = process()->filterLabels();
    FilterPosCombo->addItems(labels);

    // Save ISO List in train settings if different
    storeTrainKey(KEY_FILTERS, labels);

    updateCurrentFilterPosition();

    filterEditB->setEnabled(state()->getCurrentFilterPosition() > 0);
    filterManagerB->setEnabled(state()->getCurrentFilterPosition() > 0);

    FilterPosCombo->setCurrentIndex(state()->getCurrentFilterPosition() - 1);
}

void Camera::updateCurrentFilterPosition()
{
    const QString currentFilterText = FilterPosCombo->itemText(m_FilterManager->getFilterPosition() - 1);
    state()->setCurrentFilterPosition(m_FilterManager->getFilterPosition(),
                                      currentFilterText,
                                      m_FilterManager->getFilterLock(currentFilterText));

}

void Camera::setFilterWheel(QString name)
{
    // Should not happen
    if (m_standAlone)
        return;

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
        emit settingsUpdated(getAllSettings());
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

void Camera::updateMeridianFlipStage(MeridianFlipState::MFStage stage)
{
    // update UI
    if (state()->getMeridianFlipState()->getMeridianFlipStage() != stage)
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
                break;

            default:
                break;
        }
    }
}

void Camera::openExposureCalculatorDialog()
{
    QCINFO << "Instantiating an Exposure Calculator";

    // Learn how to read these from indi
    double preferredSkyQuality = 20.5;

    auto scope = currentScope();
    double focalRatio = scope["focal_ratio"].toDouble(-1);

    auto reducedFocalLength = currentReducer() * scope["focal_length"].toDouble(-1);
    auto aperture = currentAperture();
    auto reducedFocalRatio = (focalRatio > 0 || aperture == 0) ? focalRatio : reducedFocalLength / aperture;

    if (devices()->getActiveCamera() != nullptr)
    {
        QCINFO << "set ExposureCalculator preferred camera to active camera id: "
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

void Camera::createDSLRDialog()
{
    m_DSLRInfoDialog.reset(new DSLRInfo(this, devices()->getActiveCamera()));

    connect(m_DSLRInfoDialog.get(), &DSLRInfo::infoChanged, this, [this]()
    {
        if (devices()->getActiveCamera())
            addDSLRInfo(QString(devices()->getActiveCamera()->getDeviceName()),
                        m_DSLRInfoDialog->sensorMaxWidth,
                        m_DSLRInfoDialog->sensorMaxHeight,
                        m_DSLRInfoDialog->sensorPixelW,
                        m_DSLRInfoDialog->sensorPixelH);
    });

    m_DSLRInfoDialog->show();

    emit dslrInfoRequested(devices()->getActiveCamera()->getDeviceName());
}

void Camera::showObserverDialog()
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

void Camera::onStandAloneShow(QShowEvent * event)
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

void Camera::setFilterStatus(FilterState filterState)
{
    if (filterState != state()->getFilterManagerState())
        QCDEBUG << "Filter state changed from" << Ekos::getFilterStatusString(
                    state()->getFilterManagerState()) << "to" << Ekos::getFilterStatusString(filterState);
    if (state()->getCaptureState() == CAPTURE_CHANGING_FILTER)
    {
        switch (filterState)
        {
            case FILTER_OFFSET:
                appendLogText(i18n("Changing focus offset by %1 steps...",
                                   filterManager()->getTargetFilterOffset()));
                break;

            case FILTER_CHANGE:
                appendLogText(i18n("Changing filter to %1...",
                                   FilterPosCombo->itemText(filterManager()->getTargetFilterPosition() - 1)));
                break;

            case FILTER_AUTOFOCUS:
                appendLogText(i18n("Auto focus on filter change..."));
                clearAutoFocusHFR();
                break;

            case FILTER_IDLE:
                if (state()->getFilterManagerState() == FILTER_CHANGE)
                {
                    appendLogText(i18n("Filter set to %1.",
                                       FilterPosCombo->itemText(filterManager()->getTargetFilterPosition() - 1)));
                }
                break;

            default:
                break;
        }
    }
    state()->setFilterManagerState(filterState);
    // display capture status changes
    if (activeJob())
        captureStatusWidget->setFilterState(filterState);
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

} // namespace Ekos
