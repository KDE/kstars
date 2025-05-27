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

// Include the split source files here
#include "camera_actions.cpp"
#include "camera_config.cpp"
#include "camera_device.cpp"
#include "camera_jobs.cpp"

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

void Camera::updateCameraStatus(CaptureState status)
{
    // forward a status change
    emit newStatus(status, opticalTrain(), cameraId());
}

ISD::Camera *Camera::activeCamera()
{
    // Ensure m_DeviceAdaptor is valid before dereferencing
    return m_DeviceAdaptor ? m_DeviceAdaptor->getActiveCamera() : nullptr;
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
    connect(darkLibraryB, &QPushButton::clicked, this, [this]
    {
        DarkLibrary::Instance()->setOpticalTrain(opticalTrain(), false);
        DarkLibrary::Instance()->show();
    });
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

void Camera::setMaximumGuidingDeviation(bool enable, double value)
{
    m_LimitsUI->enforceGuideDeviation->setChecked(enable);
    if (enable)
        m_LimitsUI->guideDeviation->setValue(value);
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

void Camera::setInSequenceFocus(bool enable, double HFR)
{
    m_LimitsUI->enforceAutofocusHFR->setChecked(enable);
    if (enable)
        m_LimitsUI->hFRDeviation->setValue(HFR);
}

}
