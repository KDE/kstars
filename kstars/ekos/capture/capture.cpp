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
#include "placeholderpath.h"
#include "ekos/manager.h"
#include "ekos/auxiliary/darklibrary.h"
#include "ekos/auxiliary/profilesettings.h"

#include "scriptsmanager.h"
#include "fitsviewer/fitsdata.h"
#include "indi/driverinfo.h"
#include "indi/indifilterwheel.h"
#include "indi/indicamera.h"
#include "indi/indirotator.h"
#include "ekos/guide/guide.h"
#include <basedevice.h>

#include <ekos_capture_debug.h>
#include <qlineedit.h>

#define MF_TIMER_TIMEOUT    90000
#define MF_RA_DIFF_LIMIT    4

// Qt version calming
#include <qtendl.h>

#define KEY_FILTERS     "filtersList"

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

    //isAutoGuiding   = false;

    // connect(darkB, &QAbstractButton::toggled, this, [this]()
    // {
    //     Options::setAutoDark(darkB->isChecked());
    // });

    //connect( seqWatcher, SIGNAL(dirty(QString)), this, &Capture::checkSeqFile(QString)));

    // connect(cameraUI->rotatorB, &QPushButton::clicked, cameraUI->m_RotatorControlPanel.get(), &Capture::show);

    // forward signals from the camera
    connect(cameraUI, &Camera::newLog, this, &Capture::appendLogText);
    connect(cameraUI, &Camera::sequenceChanged, this, &Capture::sequenceChanged);
    connect(cameraUI, &Camera::newLocalPreview, this, &Capture::newLocalPreview);
    connect(cameraUI, &Camera::dslrInfoRequested, this, &Capture::dslrInfoRequested);
    connect(cameraUI, &Camera::trainChanged, this, &Capture::trainChanged);
    connect(cameraUI, &Camera::settingsUpdated, this, &Capture::settingsUpdated);
    connect(cameraUI, &Camera::filterManagerUpdated, this, &Capture::filterManagerUpdated);
    connect(cameraUI, &Camera::newFilterStatus, this, &Capture::newFilterStatus);
    connect(cameraUI, &Camera::ready, this, &Capture::ready);
    connect(cameraUI, &Camera::newExposureProgress, this, &Capture::newExposureProgress);
    connect(cameraUI, &Camera::captureComplete, this, &Capture::captureComplete);
    connect(cameraUI, &Camera::captureStarting, this, &Capture::captureStarting);
    connect(cameraUI, &Camera::captureAborted, this, &Capture::captureAborted);
    connect(cameraUI, &Camera::checkFocus, this, &Capture::checkFocus);
    connect(cameraUI, &Camera::newImage, this, &Capture::newImage);
    connect(cameraUI, &Camera::runAutoFocus, this, &Capture::runAutoFocus);
    connect(cameraUI, &Camera::resetFocus, this, &Capture::resetFocus);
    connect(cameraUI, &Camera::abortFocus, this, &Capture::abortFocus);
    connect(cameraUI, &Camera::adaptiveFocus, this, &Capture::adaptiveFocus);
    connect(cameraUI, &Camera::captureTarget, this, &Capture::captureTarget);

    DarkLibrary::Instance()->setCaptureModule(this);

    // display the capture status in the UI
    connect(this, &Capture::newStatus, cameraUI->captureStatusWidget, &LedStatusWidget::setCaptureState);

    // forward signals from capture module state
    connect(m_captureModuleState.data(), &CaptureModuleState::newStatus, this, &Capture::newStatus);
    connect(m_captureModuleState.data(), &CaptureModuleState::guideAfterMeridianFlip, this,
            &Capture::guideAfterMeridianFlip);
    connect(m_captureModuleState.data(), &CaptureModuleState::newMeridianFlipStage, this, &Capture::updateMeridianFlipStage);
    connect(m_captureModuleState.data(), &CaptureModuleState::meridianFlipStarted, this, &Capture::meridianFlipStarted);

    // forward signals from capture process
    connect(m_captureProcess.data(), &CaptureProcess::refreshCamera, this, &Capture::updateCamera);
    connect(m_captureProcess.data(), &CaptureProcess::updateMeridianFlipStage, this, &Capture::updateMeridianFlipStage);
    connect(m_captureProcess.data(), &CaptureProcess::suspendGuiding, this, &Capture::suspendGuiding);
    connect(m_captureProcess.data(), &CaptureProcess::resumeGuiding, this, &Capture::resumeGuiding);
    connect(m_captureProcess.data(), &CaptureProcess::driverTimedout, this, &Capture::driverTimedout);

    // connections between state machine and device adaptor
    connect(m_captureDeviceAdaptor.data(), &CaptureDeviceAdaptor::pierSideChanged,
            m_captureModuleState.data(), &CaptureModuleState::setPierSide);
    connect(m_captureDeviceAdaptor.data(), &CaptureDeviceAdaptor::scopeParkStatusChanged,
            m_captureModuleState.data(), &CaptureModuleState::setScopeParkState);

    // Generate Meridian Flip State
    getMeridianFlipState();

    //Update the filename preview
}

bool Capture::updateCamera()
{
    QVariant trainID = ProfileSettings::Instance()->getOneSetting(ProfileSettings::CaptureOpticalTrain);

    if (cameraUI->updateCamera() && activeCamera() && trainID.isValid())
    {
        auto name = activeCamera()->getDeviceName();
        cameraTabs->setTabText(cameraTabs->currentIndex(), name);
    }
    else
    {
        cameraTabs->setTabText(cameraTabs->currentIndex(), "no camera");
        return false;
    }
    return true;
}


bool Capture::setDome(ISD::Dome *device)
{
    return m_captureProcess->setDome(device);
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

void Capture::ignoreSequenceHistory()
{
    // This function is called independently from the Scheduler or the UI, so honor the change
    state()->setIgnoreJobProgress(true);
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

void Capture::setAlignResults(double solverPA, double ra, double de, double pixscale)
{
    Q_UNUSED(ra)
    Q_UNUSED(de)
    Q_UNUSED(pixscale)
    if (devices()->rotator() && cameraUI->m_RotatorControlPanel)
        cameraUI->m_RotatorControlPanel->refresh(solverPA);
}

void Capture::setMeridianFlipState(QSharedPointer<MeridianFlipState> newstate)
{
    state()->setMeridianFlipState(newstate);
    connect(state()->getMeridianFlipState().get(), &MeridianFlipState::newLog, this, &Capture::appendLogText);
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

QString Capture::getTargetName()
{
    if (activeJob())
        return activeJob()->getCoreProperty(SequenceJob::SJ_TargetName).toString();
    else
        return "";
}

void Capture::setHFR(double newHFR, int, bool inAutofocus)
{
    state()->getRefocusState()->setFocusHFR(newHFR, inAutofocus);
}
}
