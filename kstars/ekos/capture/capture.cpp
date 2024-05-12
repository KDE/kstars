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
#include "sequencejob.h"
#include "sequencequeue.h"
#include "placeholderpath.h"
#include "auxiliary/ksmessagebox.h"
#include "ekos/manager.h"
#include "ekos/auxiliary/darklibrary.h"
#include "ekos/auxiliary/profilesettings.h"

// Optical Trains
#include "ekos/auxiliary/opticaltrainmanager.h"

#include "scriptsmanager.h"
#include "fitsviewer/fitsdata.h"
#include "indi/driverinfo.h"
#include "indi/indifilterwheel.h"
#include "indi/indicamera.h"
#include "indi/indirotator.h"
#include "oal/observeradd.h"
#include "ekos/guide/guide.h"
#include "dslrinfodialog.h"
#include <basedevice.h>

#include <ekos_capture_debug.h>
#include <qlineedit.h>

#define MF_TIMER_TIMEOUT    90000
#define MF_RA_DIFF_LIMIT    4

// Qt version calming
#include <qtendl.h>

// These strings are used to store information in the optical train
// for later use in the stand-alone esq editor.
#define KEY_FILTERS     "filtersList"
#define KEY_FORMATS     "formatsList"
#define KEY_ISOS        "isoList"
#define KEY_INDEX       "isoIndex"
#define KEY_GAIN_KWD    "ccdGainKeyword"
#define KEY_OFFSET_KWD  "ccdOffsetKeyword"
#define KEY_TEMPERATURE "ccdTemperatures"
#define KEY_TIMESTAMP   "timestamp"

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

Capture::Capture(bool standAlone)
{
    setupUi(this);
    cameraUI->m_standAlone = standAlone;

    if (!cameraUI->m_standAlone)
    {
        qRegisterMetaType<CaptureState>("CaptureState");
        qDBusRegisterMetaType<CaptureState>();
    }
    new CaptureAdaptor(this);
    m_captureModuleState.reset(new CaptureModuleState());
    m_captureDeviceAdaptor.reset(new CaptureDeviceAdaptor());
    m_captureProcess.reset(new CaptureProcess(state(), m_captureDeviceAdaptor));

    cameraUI->setDeviceAdaptor(m_captureDeviceAdaptor);
    cameraUI->setCaptureProcess(m_captureProcess);
    cameraUI->setCaptureModuleState(m_captureModuleState);
    cameraUI->initCamera();

    state()->getSequenceQueue()->loadOptions();

    if (!cameraUI->m_standAlone)
    {
        QDBusConnection::sessionBus().registerObject("/KStars/Ekos/Capture", this);
        QPointer<QDBusInterface> ekosInterface = new QDBusInterface("org.kde.kstars", "/KStars/Ekos", "org.kde.kstars.Ekos",
                QDBusConnection::sessionBus(), this);

        // Connecting DBus signals
        QDBusConnection::sessionBus().connect("org.kde.kstars", "/KStars/Ekos", "org.kde.kstars.Ekos", "newModule", this,
                                              SLOT(registerNewModule(QString)));

        // ensure that the mount interface is present
        registerNewModule("Mount");
    }
    KStarsData::Instance()->userdb()->GetAllDSLRInfos(state()->DSLRInfos());

    if (state()->DSLRInfos().count() > 0)
    {
        qCDebug(KSTARS_EKOS_CAPTURE) << "DSLR Cameras Info:";
        qCDebug(KSTARS_EKOS_CAPTURE) << state()->DSLRInfos();
    }
    dirPath = QUrl::fromLocalFile(QDir::homePath());

    //isAutoGuiding   = false;


    connect(cameraUI->startB, &QPushButton::clicked, this, &Capture::toggleSequence);
    connect(cameraUI->pauseB, &QPushButton::clicked, this, &Capture::pause);
    connect(cameraUI, &Camera::settingsUpdated, this, &Capture::settingsUpdated);

    connect(cameraUI->captureBinHN, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), cameraUI->captureBinVN,
            &QSpinBox::setValue);
    connect(cameraUI->liveVideoB, &QPushButton::clicked, this, &Capture::toggleVideo);
    connect(cameraUI->clearConfigurationB, &QPushButton::clicked, this, &Capture::clearCameraConfiguration);

    // connect(darkB, &QAbstractButton::toggled, this, [this]()
    // {
    //     Options::setAutoDark(darkB->isChecked());
    // });

    connect(cameraUI->restartCameraB, &QPushButton::clicked, this, [this]()
    {
        if (activeCamera())
            restartCamera(activeCamera()->getDeviceName());
    });

    connect(cameraUI->filterEditB, &QPushButton::clicked, this, &Capture::editFilterName);

    connect(cameraUI->FilterPosCombo,
            static_cast<void(QComboBox::*)(const QString &)>(&QComboBox::currentTextChanged),
            [ = ]()
    {
        state()->updateHFRThreshold();
        generatePreviewFilename();
    });
    connect(cameraUI->previewB, &QPushButton::clicked, this, &Capture::capturePreview);
    connect(cameraUI->loopB, &QPushButton::clicked, this, &Capture::startFraming);

    //connect( seqWatcher, SIGNAL(dirty(QString)), this, &Capture::checkSeqFile(QString)));

    connect(cameraUI->addToQueueB, &QPushButton::clicked, this, [this]()
    {
        if (m_JobUnderEdit)
            editJobFinished();
        else
            createJob();
    });
    connect(cameraUI->queueUpB, &QPushButton::clicked, [this]()
    {
        moveJob(true);
    });
    connect(cameraUI->queueDownB, &QPushButton::clicked, [this]()
    {
        moveJob(false);
    });
    connect(cameraUI->removeFromQueueB, &QPushButton::clicked, this, &Capture::removeJobFromQueue);
    connect(cameraUI->selectFileDirectoryB, &QPushButton::clicked, this, &Capture::saveFITSDirectory);
    connect(cameraUI->queueSaveB, &QPushButton::clicked, this,
            static_cast<void(Capture::*)()>(&Capture::saveSequenceQueue));
    connect(cameraUI->queueSaveAsB, &QPushButton::clicked, this, &Capture::saveSequenceQueueAs);
    connect(cameraUI->queueLoadB, &QPushButton::clicked, this, static_cast<void(Capture::*)()>(&Capture::loadSequenceQueue));
    connect(cameraUI->resetB, &QPushButton::clicked, this, &Capture::resetJobs);
    connect(cameraUI->queueTable->selectionModel(), &QItemSelectionModel::currentRowChanged, this,
            &Capture::selectedJobChanged);
    connect(cameraUI->queueTable, &QAbstractItemView::doubleClicked, this, &Capture::editJob);
    connect(cameraUI->queueTable, &QTableWidget::itemSelectionChanged, this, [&]()
    {
        resetJobEdit(m_JobUnderEdit);
    });
    connect(cameraUI->resetFrameB, &QPushButton::clicked, m_captureProcess.get(), &CaptureProcess::resetFrame);
    connect(cameraUI->calibrationB, &QPushButton::clicked, cameraUI->m_CalibrationDialog, &QDialog::show);
    // connect(cameraUI->rotatorB, &QPushButton::clicked, cameraUI->m_RotatorControlPanel.get(), &Capture::show);

    connect(cameraUI->generateDarkFlatsB, &QPushButton::clicked, this, &Capture::generateDarkFlats);

    ////////////////////////////////////////////////////////////////////////
    /// Camera
    ////////////////////////////////////////////////////////////////////////
    connect(cameraUI, &Camera::newLog, this, &Capture::appendLogText);

    ////////////////////////////////////////////////////////////////////////
    /// Settings
    ////////////////////////////////////////////////////////////////////////
    loadGlobalSettings();
    connectSyncSettings();

    connect(cameraUI->m_LimitsUI->hFRThresholdPercentage, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this]()
    {
        Capture::updateHFRCheckAlgo();
    });

    updateHFRCheckAlgo();
    connect(cameraUI->m_LimitsUI->hFRCheckAlgorithm, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int)
    {
        Capture::updateHFRCheckAlgo();
    });

    connect(cameraUI->observerB, &QPushButton::clicked, this, &Capture::showObserverDialog);

    connect(cameraUI->fileDirectoryT, &QLineEdit::textChanged, this, [&]()
    {
        generatePreviewFilename();
    });

    connect(cameraUI->fileRemoteDirT, &QLineEdit::editingFinished, this, [&]()
    {
        generatePreviewFilename();
    });

    DarkLibrary::Instance()->setCaptureModule(this);

    // display the capture status in the UI
    connect(this, &Capture::newStatus, cameraUI->captureStatusWidget, &LedStatusWidget::setCaptureState);

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
    connect(m_captureProcess.data(), &CaptureProcess::captureTarget, this, &Capture::setTargetName);
    connect(m_captureProcess.data(), &CaptureProcess::downloadingFrame, this, [this]()
    {
        cameraUI->captureStatusWidget->setStatus(i18n("Downloading..."), Qt::yellow);
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
    // connections between state machine and device adaptor
    connect(m_captureModuleState.data(), &CaptureModuleState::newFilterPosition,
            m_captureDeviceAdaptor.data(), &CaptureDeviceAdaptor::setFilterPosition);
    connect(m_captureModuleState.data(), &CaptureModuleState::abortFastExposure,
            m_captureDeviceAdaptor.data(), &CaptureDeviceAdaptor::abortFastExposure);
    connect(m_captureDeviceAdaptor.data(), &CaptureDeviceAdaptor::pierSideChanged,
            m_captureModuleState.data(), &CaptureModuleState::setPierSide);
    connect(m_captureDeviceAdaptor.data(), &CaptureDeviceAdaptor::newFilterWheel, this, &Capture::setFilterWheel);

    setupOpticalTrainManager();

    // Generate Meridian Flip State
    getMeridianFlipState();

    //Update the filename preview
    cameraUI->placeholderFormatT->setText(Options::placeholderFormat());
    connect(cameraUI->placeholderFormatT, &QLineEdit::textChanged, this, [this]()
    {
        generatePreviewFilename();
    });
    connect(cameraUI->formatSuffixN, QOverload<int>::of(&QSpinBox::valueChanged), this, &Capture::generatePreviewFilename);
    connect(cameraUI->captureExposureN, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            &Capture::generatePreviewFilename);
    connect(cameraUI->targetNameT, &QLineEdit::textEdited, this, [ = ]()
    {
        generatePreviewFilename();
        qCDebug(KSTARS_EKOS_CAPTURE) << "Changed target to" << cameraUI->targetNameT->text() << "because of user edit";
    });
    connect(cameraUI->captureTypeS, &QComboBox::currentTextChanged, this, &Capture::generatePreviewFilename);

}

Capture::~Capture()
{
    qDeleteAll(state()->allJobs());
    state()->allJobs().clear();
}

void Capture::updateHFRCheckAlgo()
{
    // Threshold % is not relevant for FIXED HFR do disable the field
    const bool threshold = (cameraUI->m_LimitsUI->hFRCheckAlgorithm->currentIndex() != HFR_CHECK_FIXED);
    cameraUI->m_LimitsUI->hFRThresholdPercentage->setEnabled(threshold);
    cameraUI->m_LimitsUI->limitFocusHFRThresholdLabel->setEnabled(threshold);
    cameraUI->m_LimitsUI->limitFocusHFRPercentLabel->setEnabled(threshold);
    state()->updateHFRThreshold();
}

bool Capture::updateCamera()
{
    auto isConnected = activeCamera() && activeCamera()->isConnected();
    cameraUI->CaptureSettingsGroup->setEnabled(isConnected);
    cameraUI->fileSettingsGroup->setEnabled(isConnected);
    cameraUI->sequenceBox->setEnabled(isConnected);
    for (auto &oneChild : cameraUI->sequenceControlsButtonGroup->buttons())
        oneChild->setEnabled(isConnected);

    QVariant trainID = ProfileSettings::Instance()->getOneSetting(ProfileSettings::CaptureOpticalTrain);

    if (activeCamera() && trainID.isValid())
    {
        auto name = activeCamera()->getDeviceName();
        cameraUI->opticalTrainCombo->setToolTip(QString("%1 @ %2").arg(name, cameraUI->currentScope()["name"].toString()));
        cameraTabs->setTabText(cameraTabs->currentIndex(), name);
    }
    else
    {
        cameraTabs->setTabText(cameraTabs->currentIndex(), "no camera");
        return false;
    }

    if (devices()->filterWheel())
        process()->updateFilterInfo();

    process()->checkCamera();

    emit settingsUpdated(cameraUI->getAllSettings());

    return true;
}



void Capture::setFilterWheel(QString name)
{
    // Should not happen
    if (cameraUI->m_standAlone)
        return;

    if (devices()->filterWheel() && devices()->filterWheel()->getDeviceName() == name)
    {
        refreshFilterSettings();
        return;
    }

    auto isConnected = devices()->filterWheel() && devices()->filterWheel()->isConnected();
    cameraUI->FilterPosLabel->setEnabled(isConnected);
    cameraUI->FilterPosCombo->setEnabled(isConnected);
    cameraUI->filterManagerB->setEnabled(isConnected);

    refreshFilterSettings();

    if (devices()->filterWheel())
        emit settingsUpdated(cameraUI->getAllSettings());
}

bool Capture::setDome(ISD::Dome *device)
{
    return m_captureProcess->setDome(device);
}

void Capture::pause()
{
    process()->pauseCapturing();
    cameraUI->updateStartButtons(false, true);
}

void Capture::toggleSequence()
{
    const CaptureState capturestate = state()->getCaptureState();
    if (capturestate == CAPTURE_PAUSE_PLANNED || capturestate == CAPTURE_PAUSED)
        cameraUI->updateStartButtons(true, false);

    process()->toggleSequence();
}

void Capture::jobStarting()
{
    if (cameraUI->m_LimitsUI->enforceAutofocusHFR->isChecked() && state()->getRefocusState()->isAutoFocusReady() == false)
        appendLogText(i18n("Warning: in-sequence focusing is selected but autofocus process was not started."));
    if (cameraUI->m_LimitsUI->enforceAutofocusOnTemperature->isChecked()
            && state()->getRefocusState()->isAutoFocusReady() == false)
        appendLogText(i18n("Warning: temperature delta check is selected but autofocus process was not started."));

    cameraUI->updateStartButtons(true, false);
}

void Capture::registerNewModule(const QString &name)
{
    if (cameraUI->m_standAlone)
        return;
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
    auto targetChip = devices()->getActiveChip();
    // If camera is restarted, try again in one second
    if (!cameraUI->m_standAlone && (!camera || !targetChip || !targetChip->getCCD() || targetChip->isCapturing()))
    {
        QTimer::singleShot(1000, this, &Capture::refreshCameraSettings);
        return;
    }

    if (camera->hasCoolerControl())
    {
        cameraUI->coolerOnB->setEnabled(true);
        cameraUI->coolerOffB->setEnabled(true);
        cameraUI->coolerOnB->setChecked(camera->isCoolerOn());
        cameraUI->coolerOffB->setChecked(!camera->isCoolerOn());
    }
    else
    {
        cameraUI->coolerOnB->setEnabled(false);
        cameraUI->coolerOnB->setChecked(false);
        cameraUI->coolerOffB->setEnabled(false);
        cameraUI->coolerOffB->setChecked(false);
    }

    updateFrameProperties();

    updateCaptureFormats();

    cameraUI->customPropertiesDialog()->setCCD(camera);

    cameraUI->liveVideoB->setEnabled(camera->hasVideoStream());
    if (camera->hasVideoStream())
        cameraUI->setVideoStreamEnabled(camera->isStreamingEnabled());
    else
        cameraUI->liveVideoB->setIcon(QIcon::fromTheme("camera-off"));

    connect(camera, &ISD::Camera::propertyUpdated, this, &Capture::processCameraNumber, Qt::UniqueConnection);
    connect(camera, &ISD::Camera::coolerToggled, cameraUI, &Camera::setCoolerToggled, Qt::UniqueConnection);
    connect(camera, &ISD::Camera::videoStreamToggled, cameraUI, &Camera::setVideoStreamEnabled, Qt::UniqueConnection);
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

    cameraUI->captureTypeS->clear();

    if (frameTypes.isEmpty())
        cameraUI->captureTypeS->setEnabled(false);
    else
    {
        cameraUI->captureTypeS->setEnabled(true);
        cameraUI->captureTypeS->addItems(frameTypes);
        cameraUI->captureTypeS->setCurrentIndex(devices()->getActiveChip()->getFrameType());
    }

    // Capture Format
    cameraUI->captureFormatS->blockSignals(true);
    cameraUI->captureFormatS->clear();
    const auto list = activeCamera()->getCaptureFormats();
    cameraUI->captureFormatS->addItems(list);
    cameraUI->storeTrainKey(KEY_FORMATS, list);

    cameraUI->captureFormatS->setCurrentText(activeCamera()->getCaptureFormat());
    cameraUI->captureFormatS->blockSignals(false);

    // Encoding format
    cameraUI->captureEncodingS->blockSignals(true);
    cameraUI->captureEncodingS->clear();
    cameraUI->captureEncodingS->addItems(activeCamera()->getEncodingFormats());
    cameraUI->captureEncodingS->setCurrentText(activeCamera()->getEncodingFormat());
    cameraUI->captureEncodingS->blockSignals(false);
}

void Capture::syncCameraInfo()
{
    if (!activeCamera())
        return;

    const QString timestamp = KStarsData::Instance()->lt().toString("yyyy-MM-dd hh:mm");
    cameraUI->storeTrainKeyString(KEY_TIMESTAMP, timestamp);

    if (activeCamera()->hasCooler())
    {
        cameraUI->cameraTemperatureS->setEnabled(true);
        cameraUI->cameraTemperatureN->setEnabled(true);

        if (activeCamera()->getPermission("CCD_TEMPERATURE") != IP_RO)
        {
            double min, max, step;
            cameraUI->setTemperatureB->setEnabled(true);
            cameraUI->cameraTemperatureN->setReadOnly(false);
            cameraUI->cameraTemperatureS->setEnabled(true);
            cameraUI->temperatureRegulationB->setEnabled(true);
            activeCamera()->getMinMaxStep("CCD_TEMPERATURE", "CCD_TEMPERATURE_VALUE", &min, &max, &step);
            cameraUI->cameraTemperatureN->setMinimum(min);
            cameraUI->cameraTemperatureN->setMaximum(max);
            cameraUI->cameraTemperatureN->setSingleStep(1);
            bool isChecked = activeCamera()->getDriverInfo()->getAuxInfo().value(QString("%1_TC").arg(activeCamera()->getDeviceName()),
                             false).toBool();
            cameraUI->cameraTemperatureS->setChecked(isChecked);

            // Save the camera's temperature parameters for the stand-alone editor.
            const QStringList temperatureList =
                QStringList( { QString::number(min),
                               QString::number(max),
                               isChecked ? "1" : "0" } );
            cameraUI->storeTrainKey(KEY_TEMPERATURE, temperatureList);
        }
        else
        {
            cameraUI->setTemperatureB->setEnabled(false);
            cameraUI->cameraTemperatureN->setReadOnly(true);
            cameraUI->cameraTemperatureS->setEnabled(false);
            cameraUI->cameraTemperatureS->setChecked(false);
            cameraUI->temperatureRegulationB->setEnabled(false);

            // Save default camera temperature parameters for the stand-alone editor.
            const QStringList temperatureList = QStringList( { "-50", "50", "0" } );
            cameraUI->storeTrainKey(KEY_TEMPERATURE, temperatureList);
        }

        double temperature = 0;
        if (activeCamera()->getTemperature(&temperature))
        {
            cameraUI->temperatureOUT->setText(QString("%L1º").arg(temperature, 0, 'f', 1));
            if (cameraUI->cameraTemperatureN->cleanText().isEmpty())
                cameraUI->cameraTemperatureN->setValue(temperature);
        }
    }
    else
    {
        cameraUI->cameraTemperatureS->setEnabled(false);
        cameraUI->cameraTemperatureN->setEnabled(false);
        cameraUI->temperatureRegulationB->setEnabled(false);
        cameraUI->cameraTemperatureN->clear();
        cameraUI->temperatureOUT->clear();
        cameraUI->setTemperatureB->setEnabled(false);
    }

    auto isoList = devices()->getActiveChip()->getISOList();
    cameraUI->captureISOS->blockSignals(true);
    cameraUI->captureISOS->setEnabled(false);
    cameraUI->captureISOS->clear();

    // No ISO range available
    if (isoList.isEmpty())
    {
        cameraUI->captureISOS->setEnabled(false);
        if (cameraUI->settings().contains(KEY_ISOS))
        {
            cameraUI->settings().remove(KEY_ISOS);
            cameraUI->m_DebounceTimer.start();
        }
        if (cameraUI->settings().contains(KEY_INDEX))
        {
            cameraUI->settings().remove(KEY_INDEX);
            cameraUI->m_DebounceTimer.start();
        }
    }
    else
    {
        cameraUI->captureISOS->setEnabled(true);
        cameraUI->captureISOS->addItems(isoList);
        const int isoIndex = devices()->getActiveChip()->getISOIndex();
        cameraUI->captureISOS->setCurrentIndex(isoIndex);

        // Save ISO List and index in train settings if different
        cameraUI->storeTrainKey(KEY_ISOS, isoList);
        cameraUI->storeTrainKeyString(KEY_INDEX, QString("%1").arg(isoIndex));

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
    cameraUI->captureISOS->blockSignals(false);

    // Gain Check
    if (activeCamera()->hasGain())
    {
        double min, max, step, value, targetCustomGain;
        activeCamera()->getGainMinMaxStep(&min, &max, &step);

        // Allow the possibility of no gain value at all.
        GainSpinSpecialValue = min - step;
        cameraUI->captureGainN->setRange(GainSpinSpecialValue, max);
        cameraUI->captureGainN->setSpecialValueText(i18n("--"));
        cameraUI->captureGainN->setEnabled(true);
        cameraUI->captureGainN->setSingleStep(step);
        activeCamera()->getGain(&value);
        cameraUI->currentGainLabel->setText(QString::number(value, 'f', 0));

        targetCustomGain = cameraUI->getGain();

        // Set the custom gain if we have one
        // otherwise it will not have an effect.
        if (targetCustomGain > 0)
            cameraUI->captureGainN->setValue(targetCustomGain);
        else
            cameraUI->captureGainN->setValue(GainSpinSpecialValue);

        cameraUI->captureGainN->setReadOnly(activeCamera()->getGainPermission() == IP_RO);

        connect(cameraUI->captureGainN, &QDoubleSpinBox::editingFinished, this, [this]()
        {
            if (cameraUI->captureGainN->value() != GainSpinSpecialValue)
                cameraUI->setGain(cameraUI->captureGainN->value());
            else
                cameraUI->setGain(-1);
        });
    }
    else
    {
        cameraUI->captureGainN->setEnabled(false);
        cameraUI->currentGainLabel->clear();
    }

    // Offset checks
    if (activeCamera()->hasOffset())
    {
        double min, max, step, value, targetCustomOffset;
        activeCamera()->getOffsetMinMaxStep(&min, &max, &step);

        // Allow the possibility of no Offset value at all.
        OffsetSpinSpecialValue = min - step;
        cameraUI->captureOffsetN->setRange(OffsetSpinSpecialValue, max);
        cameraUI->captureOffsetN->setSpecialValueText(i18n("--"));
        cameraUI->captureOffsetN->setEnabled(true);
        cameraUI->captureOffsetN->setSingleStep(step);
        activeCamera()->getOffset(&value);
        cameraUI->currentOffsetLabel->setText(QString::number(value, 'f', 0));

        targetCustomOffset = cameraUI->getOffset();

        // Set the custom Offset if we have one
        // otherwise it will not have an effect.
        if (targetCustomOffset > 0)
            cameraUI->captureOffsetN->setValue(targetCustomOffset);
        else
            cameraUI->captureOffsetN->setValue(OffsetSpinSpecialValue);

        cameraUI->captureOffsetN->setReadOnly(activeCamera()->getOffsetPermission() == IP_RO);

        connect(cameraUI->captureOffsetN, &QDoubleSpinBox::editingFinished, this, [this]()
        {
            if (cameraUI->captureOffsetN->value() != OffsetSpinSpecialValue)
                cameraUI->setOffset(cameraUI->captureOffsetN->value());
            else
                cameraUI->setOffset(-1);
        });
    }
    else
    {
        cameraUI->captureOffsetN->setEnabled(false);
        cameraUI->currentOffsetLabel->clear();
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
    cameraUI->captureFrameXN->setMinimum(0);
    cameraUI->captureFrameXN->setMaximum(0);
    cameraUI->captureFrameXN->setValue(0);

    cameraUI->captureFrameYN->setMinimum(0);
    cameraUI->captureFrameYN->setMaximum(0);
    cameraUI->captureFrameYN->setValue(0);

    cameraUI->captureFrameWN->setMinimum(0);
    cameraUI->captureFrameWN->setMaximum(0);
    cameraUI->captureFrameWN->setValue(0);

    cameraUI->captureFrameHN->setMinimum(0);
    cameraUI->captureFrameHN->setMaximum(0);
    cameraUI->captureFrameHN->setValue(0);
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

    cameraUI->captureFrameWN->setEnabled(devices()->getActiveChip()->canSubframe());
    cameraUI->captureFrameHN->setEnabled(devices()->getActiveChip()->canSubframe());
    cameraUI->captureFrameXN->setEnabled(devices()->getActiveChip()->canSubframe());
    cameraUI->captureFrameYN->setEnabled(devices()->getActiveChip()->canSubframe());

    cameraUI->captureBinHN->setEnabled(devices()->getActiveChip()->canBin());
    cameraUI->captureBinVN->setEnabled(devices()->getActiveChip()->canBin());

    QList<double> exposureValues;
    exposureValues << 0.01 << 0.02 << 0.05 << 0.1 << 0.2 << 0.25 << 0.5 << 1 << 1.5 << 2 << 2.5 << 3 << 5 << 6 << 7 << 8 << 9 <<
                   10 << 20 << 30 << 40 << 50 << 60 << 120 << 180 << 300 << 600 << 900 << 1200 << 1800;

    if (devices()->getActiveCamera()->getMinMaxStep(exposureProp, exposureElem, &min, &max, &step))
    {
        if (min < 0.001)
            cameraUI->captureExposureN->setDecimals(6);
        else
            cameraUI->captureExposureN->setDecimals(3);
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

    cameraUI->captureExposureN->setRecommendedValues(exposureValues);
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
            cameraUI->captureFrameWN->setMinimum(static_cast<int>(min));
            cameraUI->captureFrameWN->setMaximum(static_cast<int>(max));
            cameraUI->captureFrameWN->setSingleStep(xstep);
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
            cameraUI->captureFrameHN->setMinimum(static_cast<int>(min));
            cameraUI->captureFrameHN->setMaximum(static_cast<int>(max));
            cameraUI->captureFrameHN->setSingleStep(ystep);
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
            cameraUI->captureFrameXN->setMinimum(static_cast<int>(min));
            cameraUI->captureFrameXN->setMaximum(static_cast<int>(max));
            cameraUI->captureFrameXN->setSingleStep(static_cast<int>(step));
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
            cameraUI->captureFrameYN->setMinimum(static_cast<int>(min));
            cameraUI->captureFrameYN->setMaximum(static_cast<int>(max));
            cameraUI->captureFrameYN->setSingleStep(static_cast<int>(step));
        }
    }
    else
        return;

    // cull to camera limits, if there are any
    if (state()->useGuideHead() == false)
        cullToDSLRLimits();

    const QString ccdGainKeyword = devices()->getActiveCamera()->getProperty("CCD_GAIN") ? "CCD_GAIN" : "CCD_CONTROLS";
    cameraUI->storeTrainKeyString(KEY_GAIN_KWD, ccdGainKeyword);

    const QString ccdOffsetKeyword = devices()->getActiveCamera()->getProperty("CCD_OFFSET") ? "CCD_OFFSET" : "CCD_CONTROLS";
    cameraUI->storeTrainKeyString(KEY_OFFSET_KWD, ccdOffsetKeyword);

    if (reset == 1 || state()->frameSettings().contains(devices()->getActiveChip()) == false)
    {
        QVariantMap settings;

        settings["x"]    = 0;
        settings["y"]    = 0;
        settings["w"]    = cameraUI->captureFrameWN->maximum();
        settings["h"]    = cameraUI->captureFrameHN->maximum();
        settings["binx"] = cameraUI->captureBinHN->value();
        settings["biny"] = cameraUI->captureBinVN->value();

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
        x = qBound(cameraUI->captureFrameXN->minimum(), x, cameraUI->captureFrameXN->maximum() - 1);
        y = qBound(cameraUI->captureFrameYN->minimum(), y, cameraUI->captureFrameYN->maximum() - 1);
        w = qBound(cameraUI->captureFrameWN->minimum(), w, cameraUI->captureFrameWN->maximum());
        h = qBound(cameraUI->captureFrameHN->minimum(), h, cameraUI->captureFrameHN->maximum());

        settings["x"] = x;
        settings["y"] = y;
        settings["w"] = w;
        settings["h"] = h;
        settings["binx"] = cameraUI->captureBinHN->value();
        settings["biny"] = cameraUI->captureBinVN->value();

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
            cameraUI->captureBinHN->setMaximum(binx);
            cameraUI->captureBinVN->setMaximum(biny);

            cameraUI->captureBinHN->setValue(settings["binx"].toInt());
            cameraUI->captureBinVN->setValue(settings["biny"].toInt());
        }
        else
        {
            cameraUI->captureBinHN->setValue(1);
            cameraUI->captureBinVN->setValue(1);
        }

        if (x >= 0)
            cameraUI->captureFrameXN->setValue(x);
        if (y >= 0)
            cameraUI->captureFrameYN->setValue(y);
        if (w > 0)
            cameraUI->captureFrameWN->setValue(w);
        if (h > 0)
            cameraUI->captureFrameHN->setValue(h);
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
        updateFrameProperties(1);
    else if (prop.isNameMatch("CCD_TRANSFER_FORMAT") || prop.isNameMatch("CCD_CAPTURE_FORMAT"))
        updateCaptureFormats();
    else if (prop.isNameMatch("CCD_CONTROLS"))
    {
        auto nvp = prop.getNumber();
        auto gain = nvp->findWidgetByName("Gain");
        if (gain)
            cameraUI->currentGainLabel->setText(QString::number(gain->value, 'f', 0));
        auto offset = nvp->findWidgetByName("Offset");
        if (offset)
            cameraUI->currentOffsetLabel->setText(QString::number(offset->value, 'f', 0));
    }
    else if (prop.isNameMatch("CCD_GAIN"))
    {
        auto nvp = prop.getNumber();
        cameraUI->currentGainLabel->setText(QString::number(nvp->at(0)->getValue(), 'f', 0));
    }
    else if (prop.isNameMatch("CCD_OFFSET"))
    {
        auto nvp = prop.getNumber();
        cameraUI->currentOffsetLabel->setText(QString::number(nvp->at(0)->getValue(), 'f', 0));
    }
}

void Capture::syncFrameType(const QString &name)
{
    if (!activeCamera() || name != activeCamera()->getDeviceName())
        return;

    QStringList frameTypes = process()->frameTypes();

    cameraUI->captureTypeS->clear();

    if (frameTypes.isEmpty())
        cameraUI->captureTypeS->setEnabled(false);
    else
    {
        cameraUI->captureTypeS->setEnabled(true);
        cameraUI->captureTypeS->addItems(frameTypes);
        ISD::CameraChip *tChip = devices()->getActiveCamera()->getChip(ISD::CameraChip::PRIMARY_CCD);
        cameraUI->captureTypeS->setCurrentIndex(tChip->getFrameType());
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
        cameraUI->FilterPosCombo->setCurrentText(filter);
        return true;
    }

    return false;
}

QString Capture::filter()
{
    return cameraUI->FilterPosCombo->currentText();
}

void Capture::refreshFilterSettings()
{
    cameraUI->FilterPosCombo->clear();

    if (!devices()->filterWheel())
    {
        cameraUI->FilterPosLabel->setEnabled(false);
        cameraUI->FilterPosCombo->setEnabled(false);
        cameraUI->filterEditB->setEnabled(false);
        cameraUI->filterManagerB->setEnabled(false);

        devices()->setFilterManager(cameraUI->filterManager());
        return;
    }

    cameraUI->FilterPosLabel->setEnabled(true);
    cameraUI->FilterPosCombo->setEnabled(true);
    cameraUI->filterEditB->setEnabled(true);
    cameraUI->filterManagerB->setEnabled(true);

    setupFilterManager();

    process()->updateFilterInfo();

    const auto labels = process()->filterLabels();
    cameraUI->FilterPosCombo->addItems(labels);

    // Save ISO List in train settings if different
    cameraUI->storeTrainKey(KEY_FILTERS, labels);

    cameraUI->updateCurrentFilterPosition();

    cameraUI->filterEditB->setEnabled(state()->getCurrentFilterPosition() > 0);
    cameraUI->filterManagerB->setEnabled(state()->getCurrentFilterPosition() > 0);

    cameraUI->FilterPosCombo->setCurrentIndex(state()->getCurrentFilterPosition() - 1);
}

void Capture::processingFITSfinished(bool success)
{
    // do nothing in case of failure
    if (success == false)
        return;

    // If this is a preview job, make sure to enable preview button after
    if (devices()->getActiveCamera()
            && devices()->getActiveCamera()->getUploadMode() != ISD::Camera::UPLOAD_LOCAL)
        cameraUI->previewB->setEnabled(true);

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
        cameraUI->captureStatusWidget->setStatus(i18n("Framing..."), Qt::darkGreen);
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
    cameraUI->imgProgress->setValue(thejob->getCompleted());
}

void Capture::captureStopped()
{
    cameraUI->imgProgress->reset();
    cameraUI->imgProgress->setEnabled(false);

    cameraUI->frameRemainingTime->setText("--:--:--");
    cameraUI->jobRemainingTime->setText("--:--:--");
    cameraUI->frameInfoLabel->setText(i18n("Expose (-/-):"));

    // stopping to CAPTURE_IDLE means that capturing will continue automatically
    auto captureState = state()->getCaptureState();
    if (captureState == CAPTURE_ABORTED || captureState == CAPTURE_SUSPENDED || captureState == CAPTURE_COMPLETE)
        cameraUI->updateStartButtons(false, false);
}

void Capture::updateTargetDistance(double targetDiff)
{
    // ensure that the drift is visible
    cameraUI->targetDriftLabel->setVisible(true);
    cameraUI->targetDrift->setVisible(true);
    cameraUI->targetDriftUnit->setVisible(true);
    // update the drift value
    cameraUI->targetDrift->setText(QString("%L1").arg(targetDiff, 0, 'd', 1));
}

void Capture::captureImageStarted()
{
    if (devices()->filterWheel() != nullptr)
    {
        // JM 2021.08.23 Call filter info to set the active filter wheel in the camera driver
        // so that it may snoop on the active filter
        process()->updateFilterInfo();
        cameraUI->updateCurrentFilterPosition();
    }

    // necessary since the status widget doesn't store the calibration stage
    if (activeJob()->getCalibrationStage() == SequenceJobState::CAL_CALIBRATION)
        cameraUI->captureStatusWidget->setStatus(i18n("Calibrating..."), Qt::yellow);
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
    if (isActiveJobPreview())
        cameraUI->frameInfoLabel->setText("Expose (-/-):");
    else
        cameraUI->frameInfoLabel->setText(QString("%1 (%L3/%L4):").arg(frameLabel(activeJob()->getFrameType(),
                                          activeJob()->getCoreProperty(SequenceJob::SJ_Filter).toString()))
                                          .arg(activeJob()->getCompleted()).arg(activeJob()->getCoreProperty(
                                                  SequenceJob::SJ_Count).toInt()));

    // ensure that the download time label is visible
    cameraUI->avgDownloadTime->setVisible(true);
    cameraUI->avgDownloadLabel->setVisible(true);
    cameraUI->secLabel->setVisible(true);
    // show estimated download time
    cameraUI->avgDownloadTime->setText(QString("%L1").arg(state()->averageDownloadTime(), 0, 'd', 2));

    // avoid logging that we captured a temporary file
    if (state()->isLooping() == false && activeJob()->jobType() != SequenceJob::JOBTYPE_PREVIEW)
        appendLogText(i18n("Capturing %1-second %2 image...",
                           QString("%L1").arg(activeJob()->getCoreProperty(SequenceJob::SJ_Exposure).toDouble(), 0, 'f', 3),
                           activeJob()->getCoreProperty(SequenceJob::SJ_Filter).toString()));
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
    cameraUI->frameRemainingTime->setText(state()->imageCountDown().toString("hh:mm:ss"));
    emit newDownloadProgress(downloadTimeLeft);
}

void Capture::updateCaptureCountDown(int deltaMillis)
{
    state()->imageCountDownAddMSecs(deltaMillis);
    state()->sequenceCountDownAddMSecs(deltaMillis);
    cameraUI->frameRemainingTime->setText(state()->imageCountDown().toString("hh:mm:ss"));
    if (!isActiveJobPreview())
        cameraUI->jobRemainingTime->setText(state()->sequenceCountDown().toString("hh:mm:ss"));
    else
        cameraUI->jobRemainingTime->setText("--:--:--");
}

void Capture::addJob(SequenceJob *job)
{
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
    int currentRow = cameraUI->queueTable->rowCount();
    cameraUI->queueTable->insertRow(currentRow);

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
    cameraUI->queueTable->setItem(currentRow, JOBTABLE_COL_STATUS, status);
    cameraUI->queueTable->setItem(currentRow, JOBTABLE_COL_FILTER, filter);
    cameraUI->queueTable->setItem(currentRow, JOBTABLE_COL_COUNTS, count);
    cameraUI->queueTable->setItem(currentRow, JOBTABLE_COL_EXP, exp);
    cameraUI->queueTable->setItem(currentRow, JOBTABLE_COL_TYPE, type);
    cameraUI->queueTable->setItem(currentRow, JOBTABLE_COL_BINNING, bin);
    cameraUI->queueTable->setItem(currentRow, JOBTABLE_COL_ISO, iso);
    cameraUI->queueTable->setItem(currentRow, JOBTABLE_COL_OFFSET, offset);

    // full update to the job table row
    updateJobTable(job, true);

    // Create a new JSON object. Needs to be called after the new row has been filled
    QJsonObject jsonJob = createJsonJob(job, currentRow);
    state()->getSequence().append(jsonJob);
    emit sequenceChanged(state()->getSequence());

    cameraUI->removeFromQueueB->setEnabled(true);
}


void Capture::editJobFinished()
{
    if (cameraUI->queueTable->currentRow() < 0)
        qCWarning(KSTARS_EKOS_CAPTURE()) << "Editing finished, but no row selected!";

    int currentRow = cameraUI->queueTable->currentRow();
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
    int currentRow = cameraUI->queueTable->currentRow();

    if (currentRow < 0)
        currentRow = cameraUI->queueTable->rowCount() - 1;

    removeJob(currentRow);

    // update selection
    if (cameraUI->queueTable->rowCount() == 0)
        return;

    if (currentRow > cameraUI->queueTable->rowCount())
        cameraUI->queueTable->selectRow(cameraUI->queueTable->rowCount() - 1);
    else
        cameraUI->queueTable->selectRow(currentRow);
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

    cameraUI->queueTable->removeRow(index);
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

    if (cameraUI->queueTable->rowCount() == 0)
        cameraUI->removeFromQueueB->setEnabled(false);

    if (cameraUI->queueTable->rowCount() == 1)
    {
        cameraUI->queueUpB->setEnabled(false);
        cameraUI->queueDownB->setEnabled(false);
    }

    if (index < cameraUI->queueTable->rowCount())
        cameraUI->queueTable->selectRow(index);
    else if (cameraUI->queueTable->rowCount() > 0)
        cameraUI->queueTable->selectRow(cameraUI->queueTable->rowCount() - 1);

    if (cameraUI->queueTable->rowCount() == 0)
    {
        cameraUI->queueSaveAsB->setEnabled(false);
        cameraUI->queueSaveB->setEnabled(false);
        cameraUI->resetB->setEnabled(false);
    }

    state()->setDirty(true);

    return true;
}

void Capture::moveJob(bool up)
{
    int currentRow = cameraUI->queueTable->currentRow();
    int destinationRow = up ? currentRow - 1 : currentRow + 1;

    int columnCount = cameraUI->queueTable->columnCount();

    if (currentRow < 0 || destinationRow < 0 || destinationRow >= cameraUI->queueTable->rowCount())
        return;

    for (int i = 0; i < columnCount; i++)
    {
        QTableWidgetItem * selectedLine = cameraUI->queueTable->takeItem(currentRow, i);
        QTableWidgetItem * counterpart  = cameraUI->queueTable->takeItem(destinationRow, i);

        cameraUI->queueTable->setItem(destinationRow, i, selectedLine);
        cameraUI->queueTable->setItem(currentRow, i, counterpart);
    }

    SequenceJob * job = state()->allJobs().takeAt(currentRow);

    state()->allJobs().removeOne(job);
    state()->allJobs().insert(destinationRow, job);

    QJsonArray seqArray = state()->getSequence();
    QJsonObject currentJob = seqArray[currentRow].toObject();
    seqArray.replace(currentRow, seqArray[destinationRow]);
    seqArray.replace(destinationRow, currentJob);
    emit sequenceChanged(seqArray);

    cameraUI->queueTable->selectRow(destinationRow);

    state()->setDirty(true);
}

void Capture::newTargetName(const QString &name)
{
    cameraUI->targetNameT->setText(name);
    generatePreviewFilename();
}

void Capture::setBusy(bool enable)
{
    cameraUI->previewB->setEnabled(!enable);
    cameraUI->loopB->setEnabled(!enable);
    cameraUI->opticalTrainCombo->setEnabled(!enable);
    cameraUI->trainB->setEnabled(!enable);

    foreach (QAbstractButton * button, cameraUI->queueEditButtonGroup->buttons())
        button->setEnabled(!enable);
}

void Capture::jobPrepared(SequenceJob * job)
{

    int index = state()->allJobs().indexOf(job);
    if (index >= 0)
        cameraUI->queueTable->selectRow(index);

    if (activeJob()->jobType() != SequenceJob::JOBTYPE_PREVIEW)
    {
        // set the progress info
        cameraUI->imgProgress->setEnabled(true);
        cameraUI->imgProgress->setMaximum(activeJob()->getCoreProperty(SequenceJob::SJ_Count).toInt());
        cameraUI->imgProgress->setValue(activeJob()->getCompleted());
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
        cameraUI->updateStartButtons(true, false);
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
            cameraUI->captureStatusWidget->setStatus(i18n("Set Temp to %1 °C...", activeJob()->getTargetTemperature()),
                    Qt::yellow);
            break;
        case CAPTURE_GUIDER_DRIFT:
            appendLogText(i18n("Waiting for guide drift below %1\"...", Options::startGuideDeviation()));
            cameraUI->captureStatusWidget->setStatus(i18n("Wait for Guider < %1\"...", Options::startGuideDeviation()), Qt::yellow);
            break;

        case CAPTURE_SETTING_ROTATOR:
            appendLogText(i18n("Setting camera to %1 degrees E of N...", activeJob()->getTargetRotation()));
            cameraUI->captureStatusWidget->setStatus(i18n("Set Camera to %1 deg...", activeJob()->getTargetRotation()),
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
                cameraUI->captureStatusWidget->setStatus(i18n("Focus complete."), Qt::yellow);
                break;
            case FOCUS_FAILED:
            case FOCUS_ABORTED:
                cameraUI->captureStatusWidget->setStatus(i18n("Autofocus failed."), Qt::darkRed);
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
                    cameraUI->captureStatusWidget->setStatus(i18n("Paused..."), Qt::yellow);
                }
                break;

            case MeridianFlipState::MF_INITIATED:
                cameraUI->captureStatusWidget->setStatus(i18n("Meridian Flip..."), Qt::yellow);
                KSNotification::event(QLatin1String("MeridianFlipStarted"), i18n("Meridian flip started"), KSNotification::Capture);
                break;

            case MeridianFlipState::MF_COMPLETED:
                cameraUI->captureStatusWidget->setStatus(i18n("Flip complete."), Qt::yellow);
                break;

            default:
                break;
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

    cameraUI->fileDirectoryT->setText(QDir::toNativeSeparators(dir));
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

    // !m_standAlone so the stand-alone editor doesn't influence a live capture sesion.
    const bool result = process()->loadSequenceQueue(fileURL, targetName, !cameraUI->m_standAlone);
    // cancel if loading fails
    if (result == false)
        return result;

    // update general settings
    setObserverName(state()->observerName());

    // select the first one of the loaded jobs
    if (state()->allJobs().size() > 0)
        syncGUIToJob(state()->allJobs().first());

    // update save button tool tip
    cameraUI->queueSaveB->setToolTip("Save to " + sFile.fileName());

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
        // !m_standAlone so the stand-alone editor doesn't influence a live capture sesion.
        if ((process()->saveSequenceQueue(state()->sequenceURL().toLocalFile(), !cameraUI->m_standAlone)) == false)
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
        SequenceJob * job = state()->allJobs().at(cameraUI->queueTable->currentRow());
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
    cameraUI->startB->setEnabled(true);
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

    cameraUI->captureFormatS->setCurrentText(job->getCoreProperty(SequenceJob::SJ_Format).toString());
    cameraUI->captureEncodingS->setCurrentText(job->getCoreProperty(SequenceJob::SJ_Encoding).toString());
    cameraUI->captureExposureN->setValue(job->getCoreProperty(SequenceJob::SJ_Exposure).toDouble());
    cameraUI->captureBinHN->setValue(job->getCoreProperty(SequenceJob::SJ_Binning).toPoint().x());
    cameraUI->captureBinVN->setValue(job->getCoreProperty(SequenceJob::SJ_Binning).toPoint().y());
    cameraUI->captureFrameXN->setValue(roi.x());
    cameraUI->captureFrameYN->setValue(roi.y());
    cameraUI->captureFrameWN->setValue(roi.width());
    cameraUI->captureFrameHN->setValue(roi.height());
    cameraUI->FilterPosCombo->setCurrentIndex(job->getTargetFilter() - 1);
    cameraUI->captureTypeS->setCurrentIndex(job->getFrameType());
    cameraUI->captureCountN->setValue(job->getCoreProperty(SequenceJob::SJ_Count).toInt());
    cameraUI->captureDelayN->setValue(job->getCoreProperty(SequenceJob::SJ_Delay).toInt() / 1000);
    cameraUI->targetNameT->setText(job->getCoreProperty(SequenceJob::SJ_TargetName).toString());
    cameraUI->fileDirectoryT->setText(job->getCoreProperty(SequenceJob::SJ_LocalDirectory).toString());
    cameraUI->fileUploadModeS->setCurrentIndex(job->getUploadMode());
    cameraUI->fileRemoteDirT->setEnabled(cameraUI->fileUploadModeS->currentIndex() != 0);
    cameraUI->fileRemoteDirT->setText(job->getCoreProperty(SequenceJob::SJ_RemoteDirectory).toString());
    cameraUI->placeholderFormatT->setText(job->getCoreProperty(SequenceJob::SJ_PlaceholderFormat).toString());
    cameraUI->formatSuffixN->setValue(job->getCoreProperty(SequenceJob::SJ_PlaceholderSuffix).toUInt());

    // Temperature Options
    cameraUI->cameraTemperatureS->setChecked(job->getCoreProperty(SequenceJob::SJ_EnforceTemperature).toBool());
    if (job->getCoreProperty(SequenceJob::SJ_EnforceTemperature).toBool())
        cameraUI->cameraTemperatureN->setValue(job->getTargetTemperature());

    // Start guider drift options
    cameraUI->m_LimitsUI->guideDitherPerJobFrequency->setValue(job->getCoreProperty(
                SequenceJob::SJ_DitherPerJobFrequency).toInt());
    cameraUI->syncLimitSettings();

    // Flat field options
    cameraUI->calibrationB->setEnabled(job->getFrameType() != FRAME_LIGHT);
    cameraUI->generateDarkFlatsB->setEnabled(job->getFrameType() != FRAME_LIGHT);

    if (job->getFlatFieldDuration() == DURATION_MANUAL)
        cameraUI->m_CalibrationUI->captureCalibrationDurationManual->setChecked(true);
    else
        cameraUI->m_CalibrationUI->captureCalibrationUseADU->setChecked(true);

    // Calibration Pre-Action
    const auto action = job->getCalibrationPreAction();
    if (action & ACTION_WALL)
    {
        cameraUI->m_CalibrationUI->azBox->setText(job->getWallCoord().az().toDMSString());
        cameraUI->m_CalibrationUI->altBox->setText(job->getWallCoord().alt().toDMSString());
    }
    cameraUI->m_CalibrationUI->captureCalibrationWall->setChecked(action & ACTION_WALL);
    cameraUI->m_CalibrationUI->captureCalibrationParkMount->setChecked(action & ACTION_PARK_MOUNT);
    cameraUI->m_CalibrationUI->captureCalibrationParkDome->setChecked(action & ACTION_PARK_DOME);

    // Calibration Flat Duration
    switch (job->getFlatFieldDuration())
    {
        case DURATION_MANUAL:
            cameraUI->m_CalibrationUI->captureCalibrationDurationManual->setChecked(true);
            break;

        case DURATION_ADU:
            cameraUI->m_CalibrationUI->captureCalibrationUseADU->setChecked(true);
            cameraUI->m_CalibrationUI->captureCalibrationADUValue->setValue(job->getCoreProperty(SequenceJob::SJ_TargetADU).toUInt());
            cameraUI->m_CalibrationUI->captureCalibrationADUTolerance->setValue(job->getCoreProperty(
                        SequenceJob::SJ_TargetADUTolerance).toUInt());
            cameraUI->m_CalibrationUI->captureCalibrationSkyFlats->setChecked(job->getCoreProperty(SequenceJob::SJ_SkyFlat).toBool());
            break;
    }

    cameraUI->m_scriptsManager->setScripts(job->getScripts());

    // Custom Properties
    cameraUI->customPropertiesDialog()->setCustomProperties(job->getCustomProperties());

    if (cameraUI->captureISOS)
        cameraUI->captureISOS->setCurrentIndex(job->getCoreProperty(SequenceJob::SJ_ISOIndex).toInt());

    double gain = cameraUI->getGain();
    if (gain >= 0)
        cameraUI->captureGainN->setValue(gain);
    else
        cameraUI->captureGainN->setValue(GainSpinSpecialValue);

    double offset = cameraUI->getOffset();
    if (offset >= 0)
        cameraUI->captureOffsetN->setValue(offset);
    else
        cameraUI->captureOffsetN->setValue(OffsetSpinSpecialValue);

    // update place holder typ
    generatePreviewFilename();

    if (cameraUI->m_RotatorControlPanel) // only if rotator is registered
    {
        if (job->getTargetRotation() != Ekos::INVALID_VALUE)
        {
            // remove enforceJobPA cameraUI->m_RotatorControlPanel->setRotationEnforced(true);
            cameraUI->m_RotatorControlPanel->setCameraPA(job->getTargetRotation());
        }
        // remove enforceJobPA
        // else
        //    cameraUI->m_RotatorControlPanel->setRotationEnforced(false);
    }

    // hide target drift if align check frequency is == 0
    if (Options::alignCheckFrequency() == 0)
    {
        cameraUI->targetDriftLabel->setVisible(false);
        cameraUI->targetDrift->setVisible(false);
        cameraUI->targetDriftUnit->setVisible(false);
    }
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
        cameraUI->queueUpB->setEnabled(i.row() > 0);
        cameraUI->queueDownB->setEnabled(i.row() + 1 < state()->allJobs().size());
    }

    return true;
}

void Capture::editJob(QModelIndex i)
{
    // Try to select a job. If job not found or not editable return.
    if (selectJob(i) == false)
        return;

    appendLogText(i18n("Editing job #%1...", i.row() + 1));

    cameraUI->addToQueueB->setIcon(QIcon::fromTheme("dialog-ok-apply"));
    cameraUI->addToQueueB->setToolTip(i18n("Apply job changes."));
    cameraUI->removeFromQueueB->setToolTip(i18n("Cancel job changes."));

    // Make it sure if user presses enter, the job is validated.
    cameraUI->previewB->setDefault(false);
    cameraUI->addToQueueB->setDefault(true);

    m_JobUnderEdit = true;
}

void Capture::resetJobEdit(bool cancelled)
{
    if (cancelled == true)
        appendLogText(i18n("Editing job canceled."));

    m_JobUnderEdit = false;
    cameraUI->addToQueueB->setIcon(QIcon::fromTheme("list-add"));

    cameraUI->addToQueueB->setToolTip(i18n("Add job to sequence queue"));
    cameraUI->removeFromQueueB->setToolTip(i18n("Remove job from sequence queue"));

    cameraUI->addToQueueB->setDefault(false);
    cameraUI->previewB->setDefault(true);
}

void Capture::setMaximumGuidingDeviation(bool enable, double value)
{
    cameraUI->m_LimitsUI->enforceGuideDeviation->setChecked(enable);
    if (enable)
        cameraUI->m_LimitsUI->guideDeviation->setValue(value);
}

void Capture::setInSequenceFocus(bool enable, double HFR)
{
    cameraUI->m_LimitsUI->enforceAutofocusHFR->setChecked(enable);
    if (enable)
        cameraUI->m_LimitsUI->hFRDeviation->setValue(HFR);
}

void Capture::clearSequenceQueue()
{
    state()->setActiveJob(nullptr);
    while (cameraUI->queueTable->rowCount() > 0)
        cameraUI->queueTable->removeRow(0);
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

void Capture::clearAutoFocusHFR()
{
    if (Options::hFRCheckAlgorithm() == HFR_CHECK_FIXED)
        return;

    cameraUI->m_LimitsUI->hFRDeviation->setValue(0);
    //firstAutoFocus = true;
}

bool Capture::setVideoLimits(uint16_t maxBufferSize, uint16_t maxPreviewFPS)
{
    if (devices()->getActiveCamera() == nullptr)
        return false;

    return devices()->getActiveCamera()->setStreamLimits(maxBufferSize, maxPreviewFPS);
}

void Capture::setMountStatus(ISD::Mount::Status newState)
{
    switch (newState)
    {
        case ISD::Mount::MOUNT_PARKING:
        case ISD::Mount::MOUNT_SLEWING:
        case ISD::Mount::MOUNT_MOVING:
            cameraUI->previewB->setEnabled(false);
            cameraUI->liveVideoB->setEnabled(false);
            // Only disable when button is "Start", and not "Stopped"
            // If mount is in motion, Stopped button should always be enabled to terminate
            // the sequence
            if (state()->isBusy() == false)
                cameraUI->startB->setEnabled(false);
            break;

        default:
            if (state()->isBusy() == false)
            {
                cameraUI->previewB->setEnabled(true);
                if (devices()->getActiveCamera())
                    cameraUI->liveVideoB->setEnabled(devices()->getActiveCamera()->hasVideoStream());
                cameraUI->startB->setEnabled(true);
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
    if (devices()->rotator() && cameraUI->m_RotatorControlPanel)
        cameraUI->m_RotatorControlPanel->refresh(solverPA);
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
                                   cameraUI->filterManager()->getTargetFilterOffset()));
                break;

            case FILTER_CHANGE:
                appendLogText(i18n("Changing filter to %1...",
                                   cameraUI->FilterPosCombo->itemText(cameraUI->filterManager()->getTargetFilterPosition() - 1)));
                break;

            case FILTER_AUTOFOCUS:
                appendLogText(i18n("Auto focus on filter change..."));
                clearAutoFocusHFR();
                break;

            case FILTER_IDLE:
                if (state()->getFilterManagerState() == FILTER_CHANGE)
                {
                    appendLogText(i18n("Filter set to %1.",
                                       cameraUI->FilterPosCombo->itemText(cameraUI->filterManager()->getTargetFilterPosition() - 1)));
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
    if (cameraUI->filterManager())
        cameraUI->filterManager()->disconnect(this);

    // Create new or refresh device
    Manager::Instance()->createFilterManager(devices()->filterWheel());

    // Return global filter manager for this filter wheel.
    Manager::Instance()->getFilterManager(devices()->filterWheel()->getDeviceName(), cameraUI->filterManager());

    devices()->setFilterManager(cameraUI->filterManager());

    connect(cameraUI->filterManager().get(), &FilterManager::updated, this, [this]()
    {
        emit filterManagerUpdated(devices()->filterWheel());
    });

    // display capture status changes
    connect(cameraUI->filterManager().get(), &FilterManager::newStatus, this, &Capture::newFilterStatus);

    connect(cameraUI->filterManagerB, &QPushButton::clicked, this, [this]()
    {
        cameraUI->filterManager()->refreshFilterModel();
        cameraUI->filterManager()->show();
        cameraUI->filterManager()->raise();
    });

    connect(cameraUI->filterManager().get(), &FilterManager::failed, this, [this]()
    {
        if (activeJob())
        {
            appendLogText(i18n("Filter operation failed."));
            abort();
        }
    });

    // filter changes
    connect(cameraUI->filterManager().get(), &FilterManager::newStatus, this, &Capture::setFilterStatus);

    // display capture status changes
    connect(cameraUI->filterManager().get(), &FilterManager::newStatus, cameraUI->captureStatusWidget,
            &LedStatusWidget::setFilterState);

    connect(cameraUI->filterManager().get(), &FilterManager::labelsChanged, this, [this]()
    {
        cameraUI->FilterPosCombo->clear();
        cameraUI->FilterPosCombo->addItems(cameraUI->filterManager()->getFilterLabels());
        cameraUI->FilterPosCombo->setCurrentIndex(cameraUI->filterManager()->getFilterPosition() - 1);
        cameraUI->updateCurrentFilterPosition();
    });

    connect(cameraUI->filterManager().get(), &FilterManager::positionChanged, this, [this]()
    {
        cameraUI->FilterPosCombo->setCurrentIndex(cameraUI->filterManager()->getFilterPosition() - 1);
        cameraUI->updateCurrentFilterPosition();
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
        if (cameraUI->captureFrameWN->maximum() == 0 || cameraUI->captureFrameWN->maximum() > (*pos)["Width"].toInt())
        {
            cameraUI->captureFrameWN->setValue((*pos)["Width"].toInt());
            cameraUI->captureFrameWN->setMaximum((*pos)["Width"].toInt());
        }

        if (cameraUI->captureFrameHN->maximum() == 0 || cameraUI->captureFrameHN->maximum() > (*pos)["Height"].toInt())
        {
            cameraUI->captureFrameHN->setValue((*pos)["Height"].toInt());
            cameraUI->captureFrameHN->setMaximum((*pos)["Height"].toInt());
        }
    }
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
        if (cameraUI->captureISOS && cameraUI->captureISOS->count() > 0)
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
        if (row >= 0 && row < cameraUI->queueTable->rowCount())
        {
            updateRowStyle(job);
            QTableWidgetItem *status = cameraUI->queueTable->item(row, JOBTABLE_COL_STATUS);
            QTableWidgetItem *count  = cameraUI->queueTable->item(row, JOBTABLE_COL_COUNTS);
            status->setText(job->getStatusString());
            updateJobTableCountCell(job, count);

            if (full)
            {
                bool isDarkFlat = job->jobType() == SequenceJob::JOBTYPE_DARKFLAT;

                QTableWidgetItem *filter = cameraUI->queueTable->item(row, JOBTABLE_COL_FILTER);
                if (cameraUI->FilterPosCombo->findText(job->getCoreProperty(SequenceJob::SJ_Filter).toString()) >= 0 &&
                        (cameraUI->captureTypeS->currentIndex() == FRAME_LIGHT || cameraUI->captureTypeS->currentIndex() == FRAME_FLAT
                         || isDarkFlat) )
                    filter->setText(job->getCoreProperty(SequenceJob::SJ_Filter).toString());
                else
                    filter->setText("--");

                QTableWidgetItem *exp = cameraUI->queueTable->item(row, JOBTABLE_COL_EXP);
                exp->setText(QString("%L1").arg(job->getCoreProperty(SequenceJob::SJ_Exposure).toDouble(), 0, 'f',
                                                cameraUI->captureExposureN->decimals()));

                QTableWidgetItem *type = cameraUI->queueTable->item(row, JOBTABLE_COL_TYPE);
                type->setText(isDarkFlat ? i18n("Dark Flat") : CCDFrameTypeNames[job->getFrameType()]);

                QTableWidgetItem *bin = cameraUI->queueTable->item(row, JOBTABLE_COL_BINNING);
                QPoint binning = job->getCoreProperty(SequenceJob::SJ_Binning).toPoint();
                bin->setText(QString("%1x%2").arg(binning.x()).arg(binning.y()));

                QTableWidgetItem *iso = cameraUI->queueTable->item(row, JOBTABLE_COL_ISO);
                if (job->getCoreProperty(SequenceJob::SJ_ISOIndex).toInt() != -1)
                    iso->setText(cameraUI->captureISOS->itemText(job->getCoreProperty(SequenceJob::SJ_ISOIndex).toInt()));
                else if (job->getCoreProperty(SequenceJob::SJ_Gain).toDouble() >= 0)
                    iso->setText(QString::number(job->getCoreProperty(SequenceJob::SJ_Gain).toDouble(), 'f', 1));
                else
                    iso->setText("--");

                QTableWidgetItem *offset = cameraUI->queueTable->item(row, JOBTABLE_COL_OFFSET);
                if (job->getCoreProperty(SequenceJob::SJ_Offset).toDouble() >= 0)
                    offset->setText(QString::number(job->getCoreProperty(SequenceJob::SJ_Offset).toDouble(), 'f', 1));
                else
                    offset->setText("--");
            }

            // update button enablement
            if (cameraUI->queueTable->rowCount() > 0)
            {
                cameraUI->queueSaveAsB->setEnabled(true);
                cameraUI->queueSaveB->setEnabled(true);
                cameraUI->resetB->setEnabled(true);
                state()->setDirty(true);
            }

            if (cameraUI->queueTable->rowCount() > 1)
            {
                cameraUI->queueUpB->setEnabled(true);
                cameraUI->queueDownB->setEnabled(true);
            }
        }
    }
}

void Capture::updateRowStyle(SequenceJob *job)
{
    if (job == nullptr)
        return;

    // find the job's row
    int row = state()->allJobs().indexOf(job);
    if (row >= 0 && row < cameraUI->queueTable->rowCount())
    {
        updateCellStyle(cameraUI->queueTable->item(row, JOBTABLE_COL_STATUS), job->getStatus() == JOB_BUSY);
        updateCellStyle(cameraUI->queueTable->item(row, JOBTABLE_COL_FILTER), job->getStatus() == JOB_BUSY);
        updateCellStyle(cameraUI->queueTable->item(row, JOBTABLE_COL_COUNTS), job->getStatus() == JOB_BUSY);
        updateCellStyle(cameraUI->queueTable->item(row, JOBTABLE_COL_EXP), job->getStatus() == JOB_BUSY);
        updateCellStyle(cameraUI->queueTable->item(row, JOBTABLE_COL_TYPE), job->getStatus() == JOB_BUSY);
        updateCellStyle(cameraUI->queueTable->item(row, JOBTABLE_COL_BINNING), job->getStatus() == JOB_BUSY);
        updateCellStyle(cameraUI->queueTable->item(row, JOBTABLE_COL_ISO), job->getStatus() == JOB_BUSY);
        updateCellStyle(cameraUI->queueTable->item(row, JOBTABLE_COL_OFFSET), job->getStatus() == JOB_BUSY);
    }
}

void Capture::updateCellStyle(QTableWidgetItem *cell, bool active)
{
    if (cell == nullptr)
        return;

    QFont font(cell->font());
    font.setBold(active);
    font.setItalic(active);
    cell->setFont(font);
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

    if (cameraUI->fileUploadModeS->currentIndex() != ISD::Camera::UPLOAD_CLIENT && cameraUI->fileRemoteDirT->text().isEmpty())
    {
        KSNotification::error(i18n("You must set remote directory for Local & Both modes."));
        return false;
    }

    if (cameraUI->fileUploadModeS->currentIndex() != ISD::Camera::UPLOAD_LOCAL && cameraUI->fileDirectoryT->text().isEmpty())
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
    jsonJob.insert("Filter", cameraUI->FilterPosCombo->currentText());
    jsonJob.insert("Count", cameraUI->queueTable->item(currentRow, JOBTABLE_COL_COUNTS)->text());
    jsonJob.insert("Exp", cameraUI->queueTable->item(currentRow, JOBTABLE_COL_EXP)->text());
    jsonJob.insert("Type", isDarkFlat ? i18n("Dark Flat") : cameraUI->queueTable->item(currentRow, JOBTABLE_COL_TYPE)->text());
    jsonJob.insert("Bin", cameraUI->queueTable->item(currentRow, JOBTABLE_COL_BINNING)->text());
    jsonJob.insert("ISO/Gain", cameraUI->queueTable->item(currentRow, JOBTABLE_COL_ISO)->text());
    jsonJob.insert("Offset", cameraUI->queueTable->item(currentRow, JOBTABLE_COL_OFFSET)->text());

    return jsonJob;
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

void Capture::editFilterName()
{
    if (cameraUI->m_standAlone)
    {
        QStringList labels;
        for (int index = 0; index < cameraUI->FilterPosCombo->count(); index++)
            labels << cameraUI->FilterPosCombo->itemText(index);
        QStringList newLabels;
        if (editFilterNameInternal(labels, newLabels))
        {
            cameraUI->FilterPosCombo->clear();
            cameraUI->FilterPosCombo->addItems(newLabels);
        }
    }
    else
    {
        if (devices()->filterWheel() == nullptr || state()->getCurrentFilterPosition() < 1)
            return;

        QStringList labels = cameraUI->filterManager()->getFilterLabels();
        QStringList newLabels;
        if (editFilterNameInternal(labels, newLabels))
            cameraUI->filterManager()->setFilterNames(newLabels);
    }
}

bool Capture::editFilterNameInternal(const QStringList &labels, QStringList &newLabels)
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

    QString title = cameraUI->m_standAlone ?
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

void Capture::generateDarkFlats()
{
    const auto existingJobs = state()->allJobs().size();
    uint8_t jobsAdded = 0;

    for (int i = 0; i < existingJobs; i++)
    {
        if (state()->allJobs().at(i)->getFrameType() != FRAME_FLAT)
            continue;

        syncGUIToJob(state()->allJobs().at(i));

        cameraUI->captureTypeS->setCurrentIndex(FRAME_DARK);
        createJob(SequenceJob::JOBTYPE_DARKFLAT);
        jobsAdded++;
    }

    if (jobsAdded > 0)
    {
        appendLogText(i18np("One dark flats job was created.", "%1 dark flats jobs were created.", jobsAdded));
    }
}

void Capture::updateJobFromUI(SequenceJob * job, FilenamePreviewType filenamePreview)
{
    job->setCoreProperty(SequenceJob::SJ_Format, cameraUI->captureFormatS->currentText());
    job->setCoreProperty(SequenceJob::SJ_Encoding, cameraUI->captureEncodingS->currentText());

    if (cameraUI->captureISOS)
        job->setISO(cameraUI->captureISOS->currentIndex());

    job->setCoreProperty(SequenceJob::SJ_Gain, cameraUI->getGain());
    job->setCoreProperty(SequenceJob::SJ_Offset, cameraUI->getOffset());

    if (cameraUI->cameraTemperatureN->isEnabled())
    {
        job->setCoreProperty(SequenceJob::SJ_EnforceTemperature, cameraUI->cameraTemperatureS->isChecked());
        job->setTargetTemperature(cameraUI->cameraTemperatureN->value());
    }

    job->setScripts(cameraUI->m_scriptsManager->getScripts());
    job->setUploadMode(static_cast<ISD::Camera::UploadMode>(cameraUI->fileUploadModeS->currentIndex()));


    job->setFlatFieldDuration(cameraUI->m_CalibrationUI->captureCalibrationDurationManual->isChecked() ? DURATION_MANUAL :
                              DURATION_ADU);

    int action = ACTION_NONE;
    if (cameraUI->m_CalibrationUI->captureCalibrationParkMount->isChecked())
        action |= ACTION_PARK_MOUNT;
    if (cameraUI->m_CalibrationUI->captureCalibrationParkDome->isChecked())
        action |= ACTION_PARK_DOME;
    if (cameraUI->m_CalibrationUI->captureCalibrationWall->isChecked())
    {
        bool azOk = false, altOk = false;
        auto wallAz  = cameraUI->m_CalibrationUI->azBox->createDms(&azOk);
        auto wallAlt = cameraUI->m_CalibrationUI->altBox->createDms(&altOk);

        if (azOk && altOk)
        {
            action = (action & ~ACTION_PARK_MOUNT) | ACTION_WALL;
            SkyPoint wallSkyPoint;
            wallSkyPoint.setAz(wallAz);
            wallSkyPoint.setAlt(wallAlt);
            job->setWallCoord(wallSkyPoint);
        }
    }

    if (cameraUI->m_CalibrationUI->captureCalibrationUseADU->isChecked())
    {
        job->setCoreProperty(SequenceJob::SJ_TargetADU, cameraUI->m_CalibrationUI->captureCalibrationADUValue->value());
        job->setCoreProperty(SequenceJob::SJ_TargetADUTolerance,
                             cameraUI->m_CalibrationUI->captureCalibrationADUTolerance->value());
        job->setCoreProperty(SequenceJob::SJ_SkyFlat, cameraUI->m_CalibrationUI->captureCalibrationSkyFlats->isChecked());
    }

    job->setCalibrationPreAction(action);

    job->setFrameType(static_cast<CCDFrameType>(qMax(0, cameraUI->captureTypeS->currentIndex())));

    if (cameraUI->FilterPosCombo->currentIndex() != -1 && (cameraUI->m_standAlone || devices()->filterWheel() != nullptr))
        job->setTargetFilter(cameraUI->FilterPosCombo->currentIndex() + 1, cameraUI->FilterPosCombo->currentText());

    job->setCoreProperty(SequenceJob::SJ_Exposure, cameraUI->captureExposureN->value());

    job->setCoreProperty(SequenceJob::SJ_Count, cameraUI->captureCountN->value());

    job->setCoreProperty(SequenceJob::SJ_Binning, QPoint(cameraUI->captureBinHN->value(), cameraUI->captureBinVN->value()));

    /* in ms */
    job->setCoreProperty(SequenceJob::SJ_Delay, cameraUI->captureDelayN->value() * 1000);

    // Custom Properties
    job->setCustomProperties(cameraUI->customPropertiesDialog()->getCustomProperties());

    job->setCoreProperty(SequenceJob::SJ_ROI, QRect(cameraUI->captureFrameXN->value(), cameraUI->captureFrameYN->value(),
                         cameraUI->captureFrameWN->value(),
                         cameraUI->captureFrameHN->value()));
    job->setCoreProperty(SequenceJob::SJ_RemoteDirectory, cameraUI->fileRemoteDirT->text());
    job->setCoreProperty(SequenceJob::SJ_LocalDirectory, cameraUI->fileDirectoryT->text());
    job->setCoreProperty(SequenceJob::SJ_TargetName, cameraUI->targetNameT->text());
    job->setCoreProperty(SequenceJob::SJ_PlaceholderFormat, cameraUI->placeholderFormatT->text());
    job->setCoreProperty(SequenceJob::SJ_PlaceholderSuffix, cameraUI->formatSuffixN->value());

    job->setCoreProperty(SequenceJob::SJ_DitherPerJobFrequency, cameraUI->m_LimitsUI->guideDitherPerJobFrequency->value());

    auto placeholderPath = PlaceholderPath();
    placeholderPath.updateFullPrefix(job, cameraUI->placeholderFormatT->text());

    QString signature = placeholderPath.generateSequenceFilename(*job,
                        filenamePreview != REMOTE_PREVIEW, true, 1,
                        ".fits", "", false, true);
    job->setCoreProperty(SequenceJob::SJ_Signature, signature);
}

void Capture::setMeridianFlipState(QSharedPointer<MeridianFlipState> newstate)
{
    state()->setMeridianFlipState(newstate);
    connect(state()->getMeridianFlipState().get(), &MeridianFlipState::newLog, this, &Capture::appendLogText);
}

void Capture::setupOpticalTrainManager()
{
    connect(OpticalTrainManager::Instance(), &OpticalTrainManager::updated, this, &Capture::refreshOpticalTrain);
    connect(cameraUI->trainB, &QPushButton::clicked, this, [this]()
    {
        OpticalTrainManager::Instance()->openEditor(cameraUI->opticalTrainCombo->currentText());
    });
    connect(cameraUI->opticalTrainCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index)
    {
        ProfileSettings::Instance()->setOneSetting(ProfileSettings::CaptureOpticalTrain,
                OpticalTrainManager::Instance()->id(cameraUI->opticalTrainCombo->itemText(index)));
        refreshOpticalTrain();
        emit trainChanged();
    });
}

void Capture::refreshOpticalTrain()
{
    cameraUI->opticalTrainCombo->blockSignals(true);
    cameraUI->opticalTrainCombo->clear();
    cameraUI->opticalTrainCombo->addItems(OpticalTrainManager::Instance()->getTrainNames());
    cameraUI->trainB->setEnabled(true);

    QVariant trainID = ProfileSettings::Instance()->getOneSetting(ProfileSettings::CaptureOpticalTrain);
    if (cameraUI->m_standAlone || trainID.isValid())
    {
        auto id = cameraUI->m_standAlone ? Options::captureTrainID() : trainID.toUInt();
        Options::setCaptureTrainID(id);

        // If train not found, select the first one available.
        if (OpticalTrainManager::Instance()->exists(id) == false)
        {
            qCWarning(KSTARS_EKOS_CAPTURE) << "Optical train doesn't exist for id" << id;
            id = OpticalTrainManager::Instance()->id(cameraUI->opticalTrainCombo->itemText(0));
        }

        auto name = OpticalTrainManager::Instance()->name(id);

        cameraUI->opticalTrainCombo->setCurrentText(name);
        if (!cameraUI->m_standAlone)
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
            if (map != cameraUI->settings())
            {
                cameraUI->settings().clear();
                setAllSettings(map);
            }
        }
        else
            cameraUI->setSettings(m_GlobalSettings);
    }

    cameraUI->opticalTrainCombo->blockSignals(false);
}

void Capture::generatePreviewFilename()
{
    if (state()->isCaptureRunning() == false)
    {
        cameraUI->placeholderFormatT->setToolTip(previewFilename( LOCAL_PREVIEW ));
        emit newLocalPreview(cameraUI->placeholderFormatT->toolTip());

        if (cameraUI->fileUploadModeS->currentIndex() != 0)
            cameraUI->fileRemoteDirT->setToolTip(previewFilename( REMOTE_PREVIEW ));
    }
}

QString Capture::previewFilename(FilenamePreviewType previewType)
{
    QString previewText;
    QString m_format;
    auto separator = QDir::separator();

    if (previewType == LOCAL_PREVIEW)
    {
        if(!cameraUI->fileDirectoryT->text().endsWith(separator) && !cameraUI->placeholderFormatT->text().startsWith(separator))
            cameraUI->placeholderFormatT->setText(separator + cameraUI->placeholderFormatT->text());
        m_format = cameraUI->fileDirectoryT->text() + cameraUI->placeholderFormatT->text() + cameraUI->formatSuffixN->prefix() +
                   cameraUI->formatSuffixN->cleanText();
    }
    else if (previewType == REMOTE_PREVIEW)
        m_format = cameraUI->fileRemoteDirT->text();

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
        if (cameraUI->captureEncodingS->currentText() == "FITS")
            extension = ".fits";
        else if (cameraUI->captureEncodingS->currentText() == "XISF")
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
    // target is changed only if no job is running
    if (activeJob() == nullptr)
    {
        // set the target name in the currently selected job
        cameraUI->targetNameT->setText(newTargetName);
        auto rows = cameraUI->queueTable->selectionModel()->selectedRows();
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

void Capture::setHFR(double newHFR, int, bool inAutofocus)
{
    state()->getRefocusState()->setFocusHFR(newHFR, inAutofocus);
}

//////////////////////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////////////////////
void Capture::setAllSettings(const QVariantMap &settings)
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

        cameraUI->settings()[key] = value;
        m_GlobalSettings[key] = value;
    }

    emit settingsUpdated(cameraUI->getAllSettings());

    // Save to optical train specific settings as well
    if (!cameraUI->m_standAlone)
    {
        const int id = OpticalTrainManager::Instance()->id(cameraUI->opticalTrainCombo->currentText());
        OpticalTrainSettings::Instance()->setOpticalTrainID(id);
        OpticalTrainSettings::Instance()->setOneSetting(OpticalTrainSettings::Capture, cameraUI->settings());
        Options::setCaptureTrainID(id);
    }
    // Restablish connections
    connectSyncSettings();
}

//////////////////////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////////////////////
bool Capture::syncControl(const QVariantMap &settings, const QString &key, QWidget * widget)
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
            if (pDSB == cameraUI->captureGainN)
            {
                if (cameraUI->captureGainN->value() != GainSpinSpecialValue)
                    cameraUI->setGain(cameraUI->captureGainN->value());
                else
                    cameraUI->setGain(-1);
            }
            else if (pDSB == cameraUI->captureOffsetN)
            {
                if (cameraUI->captureOffsetN->value() != OffsetSpinSpecialValue)
                    cameraUI->setOffset(cameraUI->captureOffsetN->value());
                else
                    cameraUI->setOffset(-1);
            }
            return true;
        }
    }
    else if ((pCB = qobject_cast<QCheckBox *>(widget)))
    {
        const bool value = settings[key].toBool();
        if (value != pCB->isChecked())
            pCB->click();
        return true;
    }
    else if ((pRadioButton = qobject_cast<QRadioButton *>(widget)))
    {
        const bool value = settings[key].toBool();
        if (value != pRadioButton->isChecked())
            pRadioButton->click();
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
    else if ((pRadioButton = qobject_cast<QRadioButton *>(widget)))
    {
        const bool value = settings[key].toBool();
        if (value)
            pRadioButton->click();
        return true;
    }
    else if ((pLineEdit = qobject_cast<QLineEdit *>(widget)))
    {
        const auto value = settings[key].toString();
        pLineEdit->setText(value);
        // Special case
        if (pLineEdit == cameraUI->fileRemoteDirT)
            generatePreviewFilename();
        return true;
    }

    return false;
};

//////////////////////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////////////////////
void Capture::syncSettings()
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
            cameraUI->settings().remove(key);
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


    if (!cameraUI->m_standAlone)
    {
        cameraUI->settings()[key] = value;
        m_GlobalSettings[key] = value;
        // Save immediately
        Options::self()->setProperty(key.toLatin1(), value);
        cameraUI->m_DebounceTimer.start();
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////////////////////
void Capture::loadGlobalSettings()
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
        if (value.isValid())
        {
            oneWidget->setCurrentText(value.toString());
            settings[key] = value;
        }
        else
            qCDebug(KSTARS_EKOS_CAPTURE) << "Option" << key << "not found!";
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
            qCDebug(KSTARS_EKOS_CAPTURE) << "Option" << key << "not found!";
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
            qCDebug(KSTARS_EKOS_CAPTURE) << "Option" << key << "not found!";
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
            qCDebug(KSTARS_EKOS_CAPTURE) << "Option" << key << "not found!";
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
                qCDebug(KSTARS_EKOS_CAPTURE) << "Option" << key << "not found!";
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
    cameraUI->setSettings(settings);
}

//////////////////////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////////////////////
void Capture::connectSyncSettings()
{
    // All Combo Boxes
    for (auto &oneWidget : findChildren<QComboBox*>())
        // Don't sync Optical Train combo
        if (oneWidget != cameraUI->opticalTrainCombo)
            connect(oneWidget, QOverload<int>::of(&QComboBox::activated), this, &Ekos::Capture::syncSettings);

    // All Double Spin Boxes
    for (auto &oneWidget : findChildren<QDoubleSpinBox*>())
        connect(oneWidget, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &Ekos::Capture::syncSettings);

    // All Spin Boxes
    for (auto &oneWidget : findChildren<QSpinBox*>())
        connect(oneWidget, QOverload<int>::of(&QSpinBox::valueChanged), this, &Ekos::Capture::syncSettings);

    // All Checkboxes
    for (auto &oneWidget : findChildren<QCheckBox*>())
        connect(oneWidget, &QCheckBox::toggled, this, &Ekos::Capture::syncSettings);

    // All Checkable Groupboxes
    for (auto &oneWidget : findChildren<QGroupBox*>())
        if (oneWidget->isCheckable())
            connect(oneWidget, &QGroupBox::toggled, this, &Ekos::Capture::syncSettings);

    // All Radio Buttons
    for (auto &oneWidget : findChildren<QRadioButton*>())
        connect(oneWidget, &QRadioButton::toggled, this, &Ekos::Capture::syncSettings);

    // All Line Edits
    for (auto &oneWidget : findChildren<QLineEdit*>())
    {
        if (oneWidget->objectName() == "qt_spinbox_lineedit" || oneWidget->isReadOnly())
            continue;
        connect(oneWidget, &QLineEdit::textChanged, this, &Ekos::Capture::syncSettings);
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////////////////////
void Capture::disconnectSyncSettings()
{
    // All Combo Boxes
    for (auto &oneWidget : findChildren<QComboBox*>())
        disconnect(oneWidget, QOverload<int>::of(&QComboBox::activated), this, &Ekos::Capture::syncSettings);

    // All Double Spin Boxes
    for (auto &oneWidget : findChildren<QDoubleSpinBox*>())
        disconnect(oneWidget, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &Ekos::Capture::syncSettings);

    // All Spin Boxes
    for (auto &oneWidget : findChildren<QSpinBox*>())
        disconnect(oneWidget, QOverload<int>::of(&QSpinBox::valueChanged), this, &Ekos::Capture::syncSettings);

    // All Checkboxes
    for (auto &oneWidget : findChildren<QCheckBox*>())
        disconnect(oneWidget, &QCheckBox::toggled, this, &Ekos::Capture::syncSettings);

    // All Checkable Groupboxes
    for (auto &oneWidget : findChildren<QGroupBox*>())
        if (oneWidget->isCheckable())
            disconnect(oneWidget, &QGroupBox::toggled, this, &Ekos::Capture::syncSettings);

    // All Radio Buttons
    for (auto &oneWidget : findChildren<QRadioButton*>())
        disconnect(oneWidget, &QRadioButton::toggled, this, &Ekos::Capture::syncSettings);

    // All Line Edits
    for (auto &oneWidget : findChildren<QLineEdit*>())
    {
        if (oneWidget->objectName() == "qt_spinbox_lineedit")
            continue;
        disconnect(oneWidget, &QLineEdit::textChanged, this, &Ekos::Capture::syncSettings);
    }
}

}
