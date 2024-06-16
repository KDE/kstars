/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "capture.h"

#include "cameraprocess.h"
#include "camerastate.h"
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

    if (!standAlone)
    {
        qRegisterMetaType<CaptureState>("CaptureState");
        qDBusRegisterMetaType<CaptureState>();
    }
    new CaptureAdaptor(this);

    // Add a single camera
    setMainCamera(addCamera());
    mainCamera()->m_standAlone = standAlone;

    if (!mainCamera()->m_standAlone)
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

    DarkLibrary::Instance()->setCaptureModule(this);

    // display the capture status in the UI
    connect(this, &Capture::newStatus, mainCamera()->captureStatusWidget, &LedStatusWidget::setCaptureState);
}


Camera* Capture::addCamera()
{
    Camera *camera = new Camera();
    cameraTabs->addTab(camera, "new Camera");

    // forward signals from the camera
    connect(camera, &Camera::newLog, this, &Capture::appendLogText);
    connect(camera, &Camera::refreshCamera, this, &Capture::updateCamera);
    connect(camera, &Camera::sequenceChanged, this, &Capture::sequenceChanged);
    connect(camera, &Camera::newLocalPreview, this, &Capture::newLocalPreview);
    connect(camera, &Camera::dslrInfoRequested, this, &Capture::dslrInfoRequested);
    connect(camera, &Camera::trainChanged, this, &Capture::trainChanged);
    connect(camera, &Camera::settingsUpdated, this, &Capture::settingsUpdated);
    connect(camera, &Camera::filterManagerUpdated, this, &Capture::filterManagerUpdated);
    connect(camera, &Camera::newFilterStatus, this, &Capture::newFilterStatus);
    connect(camera, &Camera::ready, this, &Capture::ready);
    connect(camera, &Camera::newExposureProgress, this, &Capture::newExposureProgress);
    connect(camera, &Camera::captureComplete, this, &Capture::captureComplete);
    connect(camera, &Camera::captureStarting, this, &Capture::captureStarting);
    connect(camera, &Camera::captureAborted, this, &Capture::captureAborted);
    connect(camera, &Camera::checkFocus, this, &Capture::checkFocus);
    connect(camera, &Camera::newImage, this, &Capture::newImage);
    connect(camera, &Camera::runAutoFocus, this, &Capture::runAutoFocus);
    connect(camera, &Camera::resetFocus, this, &Capture::resetFocus);
    connect(camera, &Camera::abortFocus, this, &Capture::abortFocus);
    connect(camera, &Camera::adaptiveFocus, this, &Capture::adaptiveFocus);
    connect(camera, &Camera::captureTarget, this, &Capture::captureTarget);
    connect(camera, &Camera::guideAfterMeridianFlip, this, &Capture::guideAfterMeridianFlip);
    connect(camera, &Camera::newStatus, this, &Capture::newStatus);
    connect(camera, &Camera::suspendGuiding, this, &Capture::suspendGuiding);
    connect(camera, &Camera::resumeGuiding, this, &Capture::resumeGuiding);
    connect(camera, &Camera::driverTimedout, this, &Capture::driverTimedout);

    return camera;
}

void Capture::updateCamera(bool isValid)
{
    QVariant trainID = ProfileSettings::Instance()->getOneSetting(ProfileSettings::CaptureOpticalTrain);

    if (isValid)
    {
        auto name = mainCamera()->activeCamera()->getDeviceName();
        cameraTabs->setTabText(cameraTabs->currentIndex(), name);
    }
    else
        cameraTabs->setTabText(cameraTabs->currentIndex(), "no camera");
}


bool Capture::setDome(ISD::Dome *device)
{
    return process()->setDome(device);
}

void Capture::registerNewModule(const QString &name)
{
    if (mainCamera()->m_standAlone)
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
        mainCamera()->FilterPosCombo->setCurrentText(filter);
        return true;
    }

    return false;
}

QString Capture::filter()
{
    return mainCamera()->FilterPosCombo->currentText();
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
            mainCamera()->previewB->setEnabled(false);
            mainCamera()->liveVideoB->setEnabled(false);
            // Only disable when button is "Start", and not "Stopped"
            // If mount is in motion, Stopped button should always be enabled to terminate
            // the sequence
            if (state()->isBusy() == false)
                mainCamera()->startB->setEnabled(false);
            break;

        default:
            if (state()->isBusy() == false)
            {
                mainCamera()->previewB->setEnabled(true);
                if (devices()->getActiveCamera())
                    mainCamera()->liveVideoB->setEnabled(devices()->getActiveCamera()->hasVideoStream());
                mainCamera()->startB->setEnabled(true);
            }

            break;
    }
}

void Capture::setAlignResults(double solverPA, double ra, double de, double pixscale)
{
    Q_UNUSED(ra)
    Q_UNUSED(de)
    Q_UNUSED(pixscale)
    if (devices()->rotator() && mainCamera()->m_RotatorControlPanel)
        mainCamera()->m_RotatorControlPanel->refresh(solverPA);
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
