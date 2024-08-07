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
#include "auxiliary/ksmessagebox.h"

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

#define KEY_FILTERS "filtersList"
#define TAB_BUTTON_SIZE 20

namespace Ekos
{

Capture::Capture()
{
    setupUi(this);

    qRegisterMetaType<CaptureState>("CaptureState");
    qDBusRegisterMetaType<CaptureState>();
    new CaptureAdaptor(this);

    // Adding the "New Tab" tab
    QWidget *newTab = new QWidget;
    QPushButton *addButton = new QPushButton;
    addButton->setIcon(QIcon::fromTheme("list-add"));
    addButton->setFixedSize(TAB_BUTTON_SIZE, TAB_BUTTON_SIZE);
    addButton->setToolTip(i18n("<p>Add additional camera</p><p><b>WARNING</b>: This feature is experimental!</p>"));
    connect(addButton, &QPushButton::clicked, this, &Capture::addCamera);

    cameraTabs->addTab(newTab, "");
    cameraTabs->tabBar()->setTabButton(0, QTabBar::RightSide, addButton);

    // Create main camera
    addCamera();

    QDBusConnection::sessionBus().registerObject("/KStars/Ekos/Capture", this);
    QPointer<QDBusInterface> ekosInterface = new QDBusInterface("org.kde.kstars", "/KStars/Ekos", "org.kde.kstars.Ekos",
            QDBusConnection::sessionBus(), this);

    // Connecting DBus signals
    QDBusConnection::sessionBus().connect("org.kde.kstars", "/KStars/Ekos", "org.kde.kstars.Ekos", "newModule", this,
                                          SLOT(registerNewModule(QString)));

    // ensure that the mount interface is present
    registerNewModule("Mount");

    DarkLibrary::Instance()->setCaptureModule(this);
}


QSharedPointer<Camera> Capture::addCamera()
{
    QSharedPointer<Camera> newCamera;
    newCamera.reset(new Camera(m_Cameras.count()));

    // create the new tab and bring it to front
    const int tabIndex = cameraTabs->insertTab(std::max(0, cameraTabs->count() - 1), newCamera.get(), "new Camera");
    cameraTabs->setCurrentIndex(tabIndex);
    // make the tab closeable if it's not the first one
    if (tabIndex > 0)
    {
        QPushButton *closeButton = new QPushButton();
        closeButton->setIcon(QIcon::fromTheme("window-close"));
        closeButton->setFixedSize(TAB_BUTTON_SIZE, TAB_BUTTON_SIZE);
        cameraTabs->tabBar()->setTabButton(tabIndex, QTabBar::RightSide, closeButton);
        connect(closeButton, &QPushButton::clicked, this, [this, tabIndex]()
        {
            checkCloseCameraTab(tabIndex);
        });
    }

    // forward signals from the camera
    connect(newCamera.get(), &Camera::newLog, this, &Capture::appendLogText);
    connect(newCamera.get(), &Camera::refreshCamera, this, &Capture::updateCamera);
    connect(newCamera.get(), &Camera::sequenceChanged, this, &Capture::sequenceChanged);
    connect(newCamera.get(), &Camera::newLocalPreview, this, &Capture::newLocalPreview);
    connect(newCamera.get(), &Camera::dslrInfoRequested, this, &Capture::dslrInfoRequested);
    connect(newCamera.get(), &Camera::trainChanged, this, &Capture::trainChanged);
    connect(newCamera.get(), &Camera::settingsUpdated, this, &Capture::settingsUpdated);
    connect(newCamera.get(), &Camera::filterManagerUpdated, this, &Capture::filterManagerUpdated);
    connect(newCamera.get(), &Camera::newFilterStatus, this, &Capture::newFilterStatus);
    connect(newCamera.get(), &Camera::ready, this, &Capture::ready);
    connect(newCamera.get(), &Camera::newExposureProgress, this, &Capture::newExposureProgress);
    connect(newCamera.get(), &Camera::captureComplete, this, &Capture::captureComplete);
    connect(newCamera.get(), &Camera::captureStarting, this, &Capture::captureStarting);
    connect(newCamera.get(), &Camera::captureAborted, this, &Capture::captureAborted);
    connect(newCamera.get(), &Camera::checkFocus, this, &Capture::checkFocus);
    connect(newCamera.get(), &Camera::newImage, this, &Capture::newImage);
    connect(newCamera.get(), &Camera::runAutoFocus, this, &Capture::runAutoFocus);
    connect(newCamera.get(), &Camera::resetFocus, this, &Capture::resetFocus);
    connect(newCamera.get(), &Camera::abortFocus, this, &Capture::abortFocus);
    connect(newCamera.get(), &Camera::adaptiveFocus, this, &Capture::adaptiveFocus);
    connect(newCamera.get(), &Camera::captureTarget, this, &Capture::captureTarget);
    connect(newCamera.get(), &Camera::guideAfterMeridianFlip, this, &Capture::guideAfterMeridianFlip);
    connect(newCamera.get(), &Camera::newStatus, this, &Capture::newStatus);
    connect(newCamera.get(), &Camera::suspendGuiding, this, &Capture::suspendGuiding);
    connect(newCamera.get(), &Camera::resumeGuiding, this, &Capture::resumeGuiding);
    connect(newCamera.get(), &Camera::driverTimedout, this, &Capture::driverTimedout);

    m_Cameras.append(newCamera);
    // update the tab text
    updateCamera(tabIndex, true);
    return newCamera;
}

void Capture::updateCamera(int tabID, bool isValid)
{
    if (isValid)
    {
        if (tabID < cameraTabs->count() && tabID < m_Cameras.count() && m_Cameras[tabID]->activeCamera() != nullptr)
        {
            auto name = m_Cameras[tabID]->activeCamera()->getDeviceName();
            cameraTabs->setTabText(tabID, name);
        }
        else
            qCWarning(KSTARS_EKOS_CAPTURE) << "Unknown camera ID:" << tabID;
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

bool Capture::loadSequenceQueue(const QString &fileURL, QString train, bool isLead, QString targetName)
{
    QSharedPointer<Camera> cam;
    if (train == "")
        cam = mainCamera();
    else if (isLead)
    {
        // take the main camera, and select the train if necessary
        if (mainCamera()->opticalTrainCombo->currentText() != train)
            mainCamera()->selectOpticalTrain(train);

        cam = mainCamera();
    }
    else
    {
        // find the camera, create a new one if necessary
        int pos = findCamera(train, true);
        if (pos < 0)
            return false;
        else
            cam = camera(pos);
    }
    // camera found, load the sequence queue
    return cam->loadSequenceQueue(fileURL, targetName);
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

void Capture::setCapturedFramesMap(const QString &signature, int count, QString train)
{
    QSharedPointer<Camera> cam;
    if (train == "")
        cam = mainCamera();
    else
    {
        // find the camera, create a new one if necessary
        int pos = findCamera(train, true);
        if (pos < 0)
            qCWarning(KSTARS_EKOS_CAPTURE) << "Cannot set the captured frames map for train" << train;
        else
            cam = camera(pos);
    }
    // camera found, set the captured frames map
    cam->state()->setCapturedFramesCount(signature, count);
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

QString Capture::start(QString train)
{
    QSharedPointer<Camera> cam;
    if (train == "")
        cam = mainCamera();
    else
    {
        // find the camera, create a new one if necessary
        int pos = findCamera(train, true);
        if (pos < 0)
        {
            qCWarning(KSTARS_EKOS_CAPTURE) << "Cannot start capturing for train" << train;
            return "";
        }
        else
            cam = camera(pos);
    }
    // camera found, start capturing
    cam->start();
    return cam->getCameraName();
}

void Capture::abort(QString train)
{
    QSharedPointer<Camera> cam;
    if (train == "")
        for (auto cam : cameras())
            cam->abort();
    else
    {
        int pos = findCamera(train, false);
        if (pos < 0)
        {
            qCWarning(KSTARS_EKOS_CAPTURE) << "Cannot abort capturing for train" << train;
            return;
        }
        else
            cam = camera(pos);

        // camera found, abort capturing
        cam->abort();
    }
}

QSharedPointer<Camera> &Capture::camera(int i)
{
    if (i < m_Cameras.count())
        return m_Cameras[i];
    else
    {
        qCWarning(KSTARS_EKOS_CAPTURE) << "Unknown camera ID:" << i;
        return m_Cameras[0];
    }
}

void Ekos::Capture::closeCameraTab(int tabIndex)
{
    cameraTabs->removeTab(tabIndex);
    camera(tabIndex).clear();
    m_Cameras.removeAt(tabIndex);
    // select the next one on the left
    cameraTabs->setCurrentIndex(std::max(0, tabIndex - 1));
}

void Capture::checkCloseCameraTab(int tabIndex)
{
    if (m_Cameras[tabIndex]->state()->isBusy())
    {
        // if accept has been clicked, abort and close the tab
        connect(KSMessageBox::Instance(), &KSMessageBox::accepted, this, [this, &tabIndex]()
        {
            KSMessageBox::Instance()->disconnect(this);
            m_Cameras[tabIndex]->abort();
            closeCameraTab(tabIndex);
        });
        // if cancel has been clicked, do not close the tab
        connect(KSMessageBox::Instance(), &KSMessageBox::rejected, this, [this]()
        {
            KSMessageBox::Instance()->disconnect(this);
        });

        KSMessageBox::Instance()->warningContinueCancel(i18n("Camera %1 is busy. Abort to close?",
                m_Cameras[tabIndex]->activeCamera()->getDeviceName()), i18n("Stop capturing"), 30, false, i18n("Abort"));
    }
    else
    {
        closeCameraTab(tabIndex);
    }
}

QSharedPointer<Camera> Capture::mainCamera() const
{
    if (m_Cameras.size() > 0)
        return m_Cameras[0];
    else
        return QSharedPointer<Camera>(new Camera());
}

int Capture::findCamera(QString train, bool addIfNecessary)
{
    for (auto &cam : m_Cameras)
    {
        if (cam->opticalTrainCombo->currentText() == train)
            return cam->m_cameraId;
    }

    // none found
    if (addIfNecessary)
    {
        QSharedPointer<Camera> cam = addCamera();
        cam->selectOpticalTrain(train);
        return cam->m_cameraId;
    }
    // nothing found
    return -1;
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
