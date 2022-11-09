/*
    SPDX-FileCopyrightText: 2015 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "mount.h"

#include <QQuickView>
#include <QQuickItem>
#include <indicom.h>

#include <KNotifications/KNotification>
#include <KLocalizedContext>
#include <KActionCollection>

#include "Options.h"

#include "ksmessagebox.h"
#include "indi/driverinfo.h"
#include "indi/indicommon.h"
#include "indi/clientmanager.h"
#include "indi/indigps.h"


#include "mountadaptor.h"

#include "ekos/manager.h"
#include "ekos/auxiliary/opticaltrainmanager.h"
#include "ekos/auxiliary/profilesettings.h"
#include "ekos/auxiliary/opticaltrainsettings.h"

#include "kstars.h"
#include "skymapcomposite.h"
#include "dialogs/finddialog.h"
#include "kstarsdata.h"

#include <basedevice.h>

#include <ekos_mount_debug.h>

extern const char *libindi_strings_context;

#define ABORT_DISPATCH_LIMIT 3

namespace Ekos
{

Mount::Mount()
{
    setupUi(this);

    new MountAdaptor(this);
    QDBusConnection::sessionBus().registerObject("/KStars/Ekos/Mount", this);
    // Set up DBus interfaces
    QPointer<QDBusInterface> ekosInterface = new QDBusInterface("org.kde.kstars", "/KStars/Ekos", "org.kde.kstars.Ekos",
            QDBusConnection::sessionBus(), this);
    qDBusRegisterMetaType<SkyPoint>();

    // Connecting DBus signals
    QDBusConnection::sessionBus().connect("org.kde.kstars", "/KStars/Ekos", "org.kde.kstars.Ekos", "newModule", this,
                                          SLOT(registerNewModule(QString)));

    m_Mount = nullptr;

    m_AbortDispatch = -1;

    // initialize the state machine
    m_MeridianFlipState.reset(new MeridianFlipState());
    // set the status message in the mount tab and write it to the log
    connect(m_MeridianFlipState.get(), &MeridianFlipState::newMeridianFlipMountStatusText, [&](const QString & text)
    {
        meridianFlipStatusWidget->setStatus(text);
        if (m_MeridianFlipState->getMeridianFlipMountState() != MeridianFlipState::MOUNT_FLIP_NONE &&
                m_MeridianFlipState->getMeridianFlipMountState() != MeridianFlipState::MOUNT_FLIP_PLANNED)
            appendLogText(text);
    });
    connect(mountToolBoxB, &QPushButton::clicked, this, &Mount::toggleMountToolBox);

    connect(clearAlignmentModelB, &QPushButton::clicked, this, &Mount::resetModel);

    connect(clearParkingB, &QPushButton::clicked, this, [this]()
    {
        if (m_Mount)
            m_Mount->clearParking();

    });

    connect(purgeConfigB, &QPushButton::clicked, this, [this]()
    {
        if (m_Mount)
        {
            if (KMessageBox::questionYesNo(KStars::Instance(),
                                           i18n("Are you sure you want to clear all mount configurations?"),
                                           i18n("Mount Configuration"), KStandardGuiItem::yes(), KStandardGuiItem::no(),
                                           "purge_mount_settings_dialog") == KMessageBox::Yes)
            {
                resetModel();
                m_Mount->clearParking();
                m_Mount->setConfig(PURGE_CONFIG);
            }
        }
    });

    connect(enableAltitudeLimits, &QCheckBox::toggled, this, &Mount::setAltitudeLimitsEnabled);
    m_AltitudeLimitEnabled = enableAltitudeLimits->isChecked();
    connect(enableHaLimit, &QCheckBox::toggled, this, &Mount::enableHourAngleLimits);

    // meridian flip
    connect(m_MeridianFlipState.get(), &MeridianFlipState::newMeridianFlipMountStatusText, meridianFlipStatusWidget,
            &MeridianFlipStatusWidget::setStatus);

    // This is always in degrees
    connect(executeMeridianFlip, &QCheckBox::toggled, this, &Ekos::Mount::meridianFlipSetupChanged);
    connect(meridianFlipOffsetDegrees, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), this,
            &Ekos::Mount::meridianFlipSetupChanged);

    //    ParkTime->setTime(QTime::fromString(Options::parkTime()));
    //    connect(ParkTime, &QTimeEdit::editingFinished, this, [this]()
    //    {
    //        Options::setParkTime(ParkTime->time().toString());
    //    });

    connect(&autoParkTimer, &QTimer::timeout, this, &Mount::startAutoPark);
    connect(startTimerB, &QPushButton::clicked, this, &Mount::startParkTimer);
    connect(stopTimerB, &QPushButton::clicked, this, &Mount::stopParkTimer);

    stopTimerB->setEnabled(false);

    if (parkEveryDay->isChecked())
        startTimerB->animateClick();

    // QML Stuff
    m_BaseView = new QQuickView();

    m_BaseView->setSource(QUrl("qrc:/qml/mount/mountbox.qml"));

    m_BaseView->setTitle(i18n("Mount Control"));
#ifdef Q_OS_OSX
    m_BaseView->setFlags(Qt::Tool | Qt::WindowStaysOnTopHint);
#else
    m_BaseView->setFlags(Qt::WindowStaysOnTopHint | Qt::WindowCloseButtonHint);
#endif

    // Theming?
    m_BaseView->setColor(Qt::black);

    m_BaseObj = m_BaseView->rootObject();

    m_Ctxt = m_BaseView->rootContext();
    ///Use instead of KDeclarative
    m_Ctxt->setContextObject(new KLocalizedContext(m_BaseView));

    m_Ctxt->setContextProperty("mount", this);

    m_BaseView->setResizeMode(QQuickView::SizeViewToRootObject);

    m_SpeedSlider  = m_BaseObj->findChild<QQuickItem *>("speedSliderObject");
    m_SpeedLabel   = m_BaseObj->findChild<QQuickItem *>("speedLabelObject");
    m_raValue      = m_BaseObj->findChild<QQuickItem *>("raValueObject");
    m_deValue      = m_BaseObj->findChild<QQuickItem *>("deValueObject");
    m_azValue      = m_BaseObj->findChild<QQuickItem *>("azValueObject");
    m_altValue     = m_BaseObj->findChild<QQuickItem *>("altValueObject");
    m_haValue      = m_BaseObj->findChild<QQuickItem *>("haValueObject");
    m_zaValue      = m_BaseObj->findChild<QQuickItem *>("zaValueObject");
    m_targetText   = m_BaseObj->findChild<QQuickItem *>("targetTextObject");
    m_targetRAText = m_BaseObj->findChild<QQuickItem *>("targetRATextObject");
    m_targetDEText = m_BaseObj->findChild<QQuickItem *>("targetDETextObject");
    m_J2000Check   = m_BaseObj->findChild<QQuickItem *>("j2000CheckObject");
    m_JNowCheck    = m_BaseObj->findChild<QQuickItem *>("jnowCheckObject");
    m_Park         = m_BaseObj->findChild<QQuickItem *>("parkButtonObject");
    m_Unpark       = m_BaseObj->findChild<QQuickItem *>("unparkButtonObject");
    m_statusText   = m_BaseObj->findChild<QQuickItem *>("statusTextObject");
    m_equatorialCheck = m_BaseObj->findChild<QQuickItem *>("equatorialCheckObject");
    m_horizontalCheck = m_BaseObj->findChild<QQuickItem *>("horizontalCheckObject");
    m_haEquatorialCheck = m_BaseObj->findChild<QQuickItem *>("haEquatorialCheckObject");
    m_leftRightCheck = m_BaseObj->findChild<QQuickItem *>("leftRightCheckObject");
    m_upDownCheck = m_BaseObj->findChild<QQuickItem *>("upDownCheckObject");

    m_leftRightCheck->setProperty("checked", Options::leftRightReversed());
    m_upDownCheck->setProperty("checked", Options::upDownReversed());

    //Note:  This is to prevent a button from being called the default button
    //and then executing when the user hits the enter key such as when on a Text Box
    QList<QPushButton *> qButtons = findChildren<QPushButton *>();
    for (auto &button : qButtons)
        button->setAutoDefault(false);

    loadGlobalSettings();
    connectSettings();

    setupOpticalTrainManager();
}

Mount::~Mount()
{
    autoParkTimer.stop();
    delete(m_Ctxt);
    delete(m_BaseObj);
    delete(currentTargetPosition);
}

void Mount::setupParkUI()
{
    if (m_Mount == nullptr)
        return;

    if (m_Mount->canPark())
    {
        switch(m_Mount->parkStatus())
        {
            case ISD::PARK_PARKED:
                parkingTitle->setTitle("Parked");
                break;
            case ISD::PARK_PARKING:
                parkingTitle->setTitle("Parking");
                break;
            case ISD::PARK_UNPARKING:
                parkingTitle->setTitle("Unparking");
                break;
            case ISD::PARK_UNPARKED:
                parkingTitle->setTitle("Unparked");
                break;
            case ISD::PARK_ERROR:
                parkingTitle->setTitle("Park Error");
                break;
            case ISD::PARK_UNKNOWN:
                parkingTitle->setTitle("Park Status Unknown");
                break;
        }
        parkB->setEnabled(m_Mount->parkStatus() == ISD::PARK_UNPARKED);
        unparkB->setEnabled(m_Mount->parkStatus() == ISD::PARK_PARKED);
    }
    else
    {
        parkB->setEnabled(false);
        unparkB->setEnabled(false);
        parkingTitle->setTitle("");
    }
}

bool Mount::setMount(ISD::Mount *device)
{
    if (device && device == m_Mount)
    {
        syncTelescopeInfo();
        return false;
    }

    if (m_Mount)
        m_Mount->disconnect(this);

    m_Mount = device;

    if (m_Mount)
    {
        connect(m_Mount, &ISD::ConcreteDevice::Connected, this, [this]()
        {
            setEnabled(true);
        });
        connect(m_Mount, &ISD::ConcreteDevice::Disconnected, this, [this]()
        {
            setEnabled(false);
            opticalTrainCombo->setEnabled(true);
            trainLabel->setEnabled(true);
        });
    }
    else
        return false;

    mainLayout->setEnabled(true);

    // forward the new mount to the meridian flip state machine
    m_MeridianFlipState->setMount(device);

    if (m_GPS != nullptr)
        syncGPS();

    connect(m_Mount, &ISD::Mount::numberUpdated, this, &Mount::updateNumber);
    connect(m_Mount, &ISD::Mount::switchUpdated, this, &Mount::updateSwitch);
    connect(m_Mount, &ISD::Mount::newTarget, this, &Mount::newTarget);
    connect(m_Mount, &ISD::Mount::newTargetName, this, &Mount::newTargetName);
    connect(m_Mount, &ISD::Mount::newCoords, this, &Mount::newCoords);
    connect(m_Mount, &ISD::Mount::newCoords, this, &Mount::updateTelescopeCoords);
    connect(m_Mount, &ISD::Mount::slewRateChanged, this, &Mount::slewRateChanged);
    connect(m_Mount, &ISD::Mount::pierSideChanged, this, &Mount::pierSideChanged);
    connect(m_Mount, &ISD::Mount::axisReversed, this, &Mount::syncAxisReversed);
    connect(m_Mount, &ISD::Mount::Disconnected, this, [this]()
    {
        m_BaseView->hide();
    });
    connect(m_Mount, &ISD::Mount::newParkStatus, this, [&](ISD::ParkStatus status)
    {
        m_ParkStatus = status;
        emit newParkStatus(status);

        setupParkUI();

        // If mount is unparked AND every day auto-paro check is ON
        // AND auto park timer is not yet started, we try to initiate it.
        if (status == ISD::PARK_UNPARKED && parkEveryDay->isChecked() && autoParkTimer.isActive() == false)
            startTimerB->animateClick();
    });

    // If mount is ready then let's set it up.
    if (m_Mount->isReady())
    {
        if (enableAltitudeLimits->isChecked())
            m_Mount->setAltLimits(minimumAltLimit->value(), maximumAltLimit->value());

        syncTelescopeInfo();

        // Send initial status
        m_Status = m_Mount->status();
        emit newStatus(m_Status);

        m_ParkStatus = m_Mount->parkStatus();
        emit newParkStatus(m_ParkStatus);
        emit ready();
    }
    // Otherwise, let's wait for mount to be ready
    else
    {
        connect(m_Mount, &ISD::Mount::ready, this, [this]()
        {
            if (enableAltitudeLimits->isChecked())
                m_Mount->setAltLimits(minimumAltLimit->value(), maximumAltLimit->value());

            syncTelescopeInfo();

            // Send initial status
            m_Status = m_Mount->status();
            emit newStatus(m_Status);

            m_ParkStatus = m_Mount->parkStatus();
            emit newParkStatus(m_ParkStatus);
            emit ready();
        });
    }

    return true;
}

bool Mount::addGPS(ISD::GPS * device)
{
    // No duplicates
    for (auto &oneGPS : m_GPSes)
    {
        if (oneGPS->getDeviceName() == device->getDeviceName())
            return false;
    }

    for (auto &oneGPS : m_GPSes)
        oneGPS->disconnect(this);

    m_GPSes.append(device);

    auto executeSetGPS = [this, device]()
    {
        m_GPS = device;
        connect(m_GPS, &ISD::GPS::numberUpdated, this, &Ekos::Mount::updateNumber, Qt::UniqueConnection);
        appendLogText(i18n("GPS driver detected. KStars and mount time and location settings are now synced to the GPS driver."));
        syncGPS();
    };

    if (Options::useGPSSource() == false)
    {
        connect(KSMessageBox::Instance(), &KSMessageBox::accepted, this, [this, executeSetGPS]()
        {
            KSMessageBox::Instance()->disconnect(this);
            Options::setUseKStarsSource(false);
            Options::setUseMountSource(false);
            Options::setUseGPSSource(true);
            executeSetGPS();
        });

        KSMessageBox::Instance()->questionYesNo(i18n("GPS is detected. Do you want to switch time and location source to GPS?"),
                                                i18n("GPS Settings"), 10);
    }
    else
        executeSetGPS();

    return true;
}

void Mount::syncGPS()
{
    // We only update when location is OK
    auto location = m_GPS->getNumber("GEOGRAPHIC_COORD");
    if (!location || location->getState() != IPS_OK)
        return;

    // Sync name
    if (m_Mount)
    {
        auto activeDevices = m_Mount->getText("ACTIVE_DEVICES");
        if (activeDevices)
        {
            auto activeGPS = activeDevices->findWidgetByName("ACTIVE_GPS");
            if (activeGPS)
            {
                if (activeGPS->getText() != m_GPS->getDeviceName())
                {
                    activeGPS->setText(m_GPS->getDeviceName().toLatin1().constData());
                    m_Mount->getDriverInfo()->getClientManager()->sendNewText(activeDevices);
                }
            }
        }
    }

    // GPS Refresh should only be called once automatically.
    if (GPSInitialized == false)
    {
        auto refreshGPS = m_GPS->getSwitch("GPS_REFRESH");
        if (refreshGPS)
        {
            refreshGPS->at(0)->setState(ISS_ON);
            m_GPS->getDriverInfo()->getClientManager()->sendNewSwitch(refreshGPS);
            GPSInitialized = true;
        }
    }
}

void Mount::removeDevice(const QSharedPointer<ISD::GenericDevice> &device)
{
    if (m_Mount && m_Mount->getDeviceName() == device->getDeviceName())
    {
        m_Mount->disconnect(this);
        m_BaseView->hide();
        qCDebug(KSTARS_EKOS_MOUNT) << "Removing mount driver" << m_Mount->getDeviceName();
        m_Mount = nullptr;
    }

    for (auto &oneGPS : m_GPSes)
    {
        if (oneGPS->getDeviceName() == device->getDeviceName())
        {
            oneGPS->disconnect(this);
            m_GPSes.removeOne(oneGPS);
            m_GPS = nullptr;
            break;
        }
    }
}

void Mount::syncTelescopeInfo()
{
    if (!m_Mount || m_Mount->isConnected() == false)
        return;

    auto svp = m_Mount->getSwitch("TELESCOPE_SLEW_RATE");

    if (svp)
    {
        int index = svp->findOnSwitchIndex();

        // QtQuick
        m_SpeedSlider->setEnabled(true);
        m_SpeedSlider->setProperty("maximumValue", svp->count() - 1);
        m_SpeedSlider->setProperty("value", index);

        m_SpeedLabel->setProperty("text", i18nc(libindi_strings_context, svp->at(index)->getLabel()));
        m_SpeedLabel->setEnabled(true);
    }
    else
    {
        // QtQuick
        m_SpeedSlider->setEnabled(false);
        m_SpeedLabel->setEnabled(false);
    }

    if (m_Mount->canPark())
    {
        connect(parkB, &QPushButton::clicked, m_Mount, &ISD::Mount::park, Qt::UniqueConnection);
        connect(unparkB, &QPushButton::clicked, m_Mount, &ISD::Mount::unpark, Qt::UniqueConnection);

        // QtQuick
        m_Park->setEnabled(!m_Mount->isParked());
        m_Unpark->setEnabled(m_Mount->isParked());
    }
    else
    {
        disconnect(parkB, &QPushButton::clicked, m_Mount, &ISD::Mount::park);
        disconnect(unparkB, &QPushButton::clicked, m_Mount, &ISD::Mount::unpark);

        // QtQuick
        m_Park->setEnabled(false);
        m_Unpark->setEnabled(false);
    }
    setupParkUI();

    // Tracking State
    svp = m_Mount->getSwitch("TELESCOPE_TRACK_STATE");
    if (svp)
    {
        trackingGroup->setEnabled(true);
        trackOnB->disconnect();
        trackOffB->disconnect();
        connect(trackOnB, &QPushButton::clicked, this, [&]()
        {
            m_Mount->setTrackEnabled(true);
        });
        connect(trackOffB, &QPushButton::clicked, this, [&]()
        {
            if (KMessageBox::questionYesNo(KStars::Instance(),
                                           i18n("Are you sure you want to turn off mount tracking?"),
                                           i18n("Mount Tracking"), KStandardGuiItem::yes(), KStandardGuiItem::no(),
                                           "turn_off_mount_tracking_dialog") == KMessageBox::Yes)
                m_Mount->setTrackEnabled(false);
        });
    }
    else
    {
        trackOnB->setChecked(false);
        trackOffB->setChecked(false);
        trackingGroup->setEnabled(false);
    }

    m_leftRightCheck->setProperty("checked", m_Mount->isReversed(AXIS_RA));
    m_upDownCheck->setProperty("checked", m_Mount->isReversed(AXIS_DE));
}

void Mount::registerNewModule(const QString &name)
{
    if (name == "Capture" && captureInterface == nullptr)
    {
        captureInterface = new QDBusInterface("org.kde.kstars", "/KStars/Ekos/Capture", "org.kde.kstars.Ekos.Capture",
                                              QDBusConnection::sessionBus(), this);
    }

}

void Mount::updateTelescopeCoords(const SkyPoint &position, ISD::Mount::PierSide pierSide, const dms &ha)
{
    if (m_Mount == nullptr || !m_Mount->isConnected())
        return;

    telescopeCoord = position;

    // No need to update coords if we are still parked.
    if (m_Status == ISD::Mount::MOUNT_PARKED && m_Status == m_Mount->status())
        return;

    // Ekos Mount Tab coords are always in JNow
    raOUT->setText(telescopeCoord.ra().toHMSString());
    decOUT->setText(telescopeCoord.dec().toDMSString());

    // Mount Control Panel coords depend on the switch
    if (m_JNowCheck->property("checked").toBool())
    {
        m_raValue->setProperty("text", telescopeCoord.ra().toHMSString());
        m_deValue->setProperty("text", telescopeCoord.dec().toDMSString());
    }
    else
    {
        m_raValue->setProperty("text", telescopeCoord.ra0().toHMSString());
        m_deValue->setProperty("text", telescopeCoord.dec0().toDMSString());
    }

    // Get horizontal coords
    azOUT->setText(telescopeCoord.az().toDMSString());
    m_azValue->setProperty("text", telescopeCoord.az().toDMSString());
    altOUT->setText(telescopeCoord.alt().toDMSString());
    m_altValue->setProperty("text", telescopeCoord.alt().toDMSString());

    dms lst = KStarsData::Instance()->geo()->GSTtoLST(KStarsData::Instance()->clock()->utc().gst());
    dms haSigned(ha);
    QChar sgn('+');

    if (haSigned.Hours() > 12.0)
    {
        haSigned.setH(24.0 - haSigned.Hours());
        sgn = '-';
    }

    haOUT->setText(QString("%1%2").arg(sgn).arg(haSigned.toHMSString()));

    m_haValue->setProperty("text", haOUT->text());
    lstOUT->setText(lst.toHMSString());

    double currentAlt = telescopeCoord.altRefracted().Degrees();

    m_zaValue->setProperty("text", dms(90 - currentAlt).toDMSString());

    if (minimumAltLimit->isEnabled() && (currentAlt < minimumAltLimit->value() || currentAlt > maximumAltLimit->value()))
    {
        if (currentAlt < minimumAltLimit->value())
        {
            // Only stop if current altitude is less than last altitude indicate worse situation
            if (currentAlt < m_LastAltitude &&
                    (m_AbortDispatch == -1 ||
                     (m_Mount->isInMotion() /* && ++abortDispatch > ABORT_DISPATCH_LIMIT*/)))
            {
                appendLogText(i18n("Telescope altitude is below minimum altitude limit of %1. Aborting motion...",
                                   QString::number(minimumAltLimit->value(), 'g', 3)));
                m_Mount->abort();
                m_Mount->setTrackEnabled(false);
                //KNotification::event( QLatin1String( "OperationFailed" ));
                KNotification::beep();
                m_AbortDispatch++;
            }
        }
        else
        {
            // Only stop if current altitude is higher than last altitude indicate worse situation
            if (currentAlt > m_LastAltitude &&
                    (m_AbortDispatch == -1 ||
                     (m_Mount->isInMotion() /* && ++abortDispatch > ABORT_DISPATCH_LIMIT*/)))
            {
                appendLogText(i18n("Telescope altitude is above maximum altitude limit of %1. Aborting motion...",
                                   QString::number(maximumAltLimit->value(), 'g', 3)));
                m_Mount->abort();
                m_Mount->setTrackEnabled(false);
                //KNotification::event( QLatin1String( "OperationFailed" ));
                KNotification::beep();
                m_AbortDispatch++;
            }
        }
    }
    else
        m_AbortDispatch = -1;

    //qCDebug(KSTARS_EKOS_MOUNT) << "MaximumHaLimit " << MaximumHaLimit->isEnabled() << " value " << MaximumHaLimit->value();

    double haHours = rangeHA(ha.Hours());
    // handle Ha limit:
    // Telescope must report Pier Side
    // MaximumHaLimit must be enabled
    // for PierSide West -> East if Ha > MaximumHaLimit stop tracking
    // for PierSide East -> West if Ha > MaximumHaLimit - 12 stop Tracking
    if (maximumHaLimit->isEnabled())
    {
        // get hour angle limit
        double haLimit = maximumHaLimit->value();
        bool haLimitReached = false;
        switch(pierSide)
        {
            case ISD::Mount::PierSide::PIER_WEST:
                haLimitReached = haHours > haLimit;
                break;
            case ISD::Mount::PierSide::PIER_EAST:
                haLimitReached = rangeHA(haHours + 12.0) > haLimit;
                break;
            default:
                // can't tell so always false
                haLimitReached = false;
                break;
        }

        qCDebug(KSTARS_EKOS_MOUNT) << "Ha: " << haHours <<
                                   " haLimit " << haLimit <<
                                   " " << pierSideStateString() <<
                                   " haLimitReached " << (haLimitReached ? "true" : "false") <<
                                   " lastHa " << m_LastHourAngle;

        // compare with last ha to avoid multiple calls
        if (haLimitReached && (rangeHA(haHours - m_LastHourAngle) >= 0 ) &&
                (m_AbortDispatch == -1 ||
                 m_Mount->isInMotion()))
        {
            // moved past the limit, so stop
            appendLogText(i18n("Telescope hour angle is more than the maximum hour angle of %1. Aborting motion...",
                               QString::number(maximumHaLimit->value(), 'g', 3)));
            m_Mount->abort();
            m_Mount->setTrackEnabled(false);
            //KNotification::event( QLatin1String( "OperationFailed" ));
            KNotification::beep();
            m_AbortDispatch++;
            // ideally we pause and wait until we have passed the pier flip limit,
            // then do a pier flip and try to resume
            // this will need changing to use a target position because the current HA has stopped.
        }
    }
    else
        m_AbortDispatch = -1;

    m_LastAltitude = currentAlt;
    m_LastHourAngle = haHours;

    ISD::Mount::Status currentStatus = m_Mount->status();
    if (m_Status != currentStatus)
    {
        qCDebug(KSTARS_EKOS_MOUNT) << "Mount status changed from " << m_Mount->statusString(m_Status)
                                   << " to " << m_Mount->statusString(currentStatus);
        // If we just finished a slew, let's update initialHA and the current target's position,
        // but only if the meridian flip is not deactived
        if (currentStatus == ISD::Mount::MOUNT_TRACKING && m_Status == ISD::Mount::MOUNT_SLEWING
                && m_MeridianFlipState->getMeridianFlipMountState() != MeridianFlipState::MOUNT_FLIP_INACTIVE)
        {
            if (m_MeridianFlipState->getMeridianFlipMountState() == MeridianFlipState::MOUNT_FLIP_NONE)
            {
                m_MeridianFlipState->setFlipDelayHrs(0);
            }
            setInitialHA((sgn == '-' ? -1 : 1) * ha.Hours());
            delete currentTargetPosition;
            currentTargetPosition = new SkyPoint(telescopeCoord.ra(), telescopeCoord.dec());
            qCDebug(KSTARS_EKOS_MOUNT) << "Slew finished, MFStatus " << MeridianFlipState::meridianFlipStatusString(
                                           m_MeridianFlipState->getMeridianFlipMountState());
        }

        //setScopeStatus(currentStatus);

        m_statusText->setProperty("text", m_Mount->statusString(currentStatus));
        m_Status = currentStatus;
        // forward
        emit newStatus(m_Status);

        setupParkUI();
        m_Park->setEnabled(!m_Mount->isParked());
        m_Unpark->setEnabled(m_Mount->isParked());

        QAction *a = KStars::Instance()->actionCollection()->action("telescope_track");
        if (a != nullptr)
            a->setChecked(currentStatus == ISD::Mount::MOUNT_TRACKING);
    }

    bool isTracking = (currentStatus == ISD::Mount::MOUNT_TRACKING);
    if (trackingGroup->isEnabled())
    {
        trackOnB->setChecked(isTracking);
        trackOffB->setChecked(!isTracking);
    }

    // handle pier side display
    pierSideLabel->setText(pierSideStateString());

    // Auto Park Timer
    if (autoParkTimer.isActive())
    {
        QTime remainingTime(0, 0, 0);
        remainingTime = remainingTime.addMSecs(autoParkTimer.remainingTime());
        countdownLabel->setText(remainingTime.toString("hh:mm:ss"));
    }

    // don't check the meridian flip while in motion
    bool inMotion = (currentStatus == ISD::Mount::MOUNT_SLEWING || currentStatus == ISD::Mount::MOUNT_MOVING
                     || currentStatus == ISD::Mount::MOUNT_PARKING);
    if ((inMotion == false) && checkMeridianFlip(lst))
        startMeridianFlip();
    else
    {
        const QString message(i18n("Meridian flip inactive (parked)"));
        if (m_Mount->isParked() && meridianFlipStatusWidget->getStatus() != message)
        {
            m_MeridianFlipState->publishMFMountStatusText(message);
        }
    }

}

void Mount::updateNumber(INumberVectorProperty * nvp)
{
    if (m_GPS != nullptr && (nvp->device == m_GPS->getDeviceName()) && !strcmp(nvp->name, "GEOGRAPHIC_COORD")
            && nvp->s == IPS_OK)
        syncGPS();
}

bool Mount::setSlewRate(int index)
{
    if (m_Mount)
        return m_Mount->setSlewRate(index);

    return false;
}

void Mount::setUpDownReversed(bool enabled)
{
    Options::setUpDownReversed(enabled);
    if (m_Mount)
        m_Mount->setReversedEnabled(AXIS_DE, enabled);
}

void Mount::setLeftRightReversed(bool enabled)
{
    Options::setLeftRightReversed(enabled);
    if (m_Mount)
        m_Mount->setReversedEnabled(AXIS_RA, enabled);
}

void Mount::setMeridianFlipValues(bool activate, double degrees)
{
    executeMeridianFlip->setChecked(activate);
    meridianFlipOffsetDegrees->setValue(degrees);

    meridianFlipSetupChanged();
}

void Mount::paaStageChanged(int stage)
{
    // Clear the current target position is necessary due to a bug in some mount drivers
    // which report a mount slew instead of a mount motion. For these mounts, ending a slew
    // leads to setting the current target position, which is necessary for meridian flips
    // Since we want to avoid meridian flips during and after finishing PAA, it needs to
    // be set to nullptr.

    if (stage != PolarAlignmentAssistant::PAH_IDLE)
        currentTargetPosition = nullptr;

    switch (stage)
    {
        // deactivate the meridian flip when the first capture is taken
        case PolarAlignmentAssistant::PAH_FIRST_CAPTURE:
        case PolarAlignmentAssistant::PAH_FIRST_SOLVE:
            if (m_MeridianFlipState->getMeridianFlipMountState() != MeridianFlipState::MOUNT_FLIP_INACTIVE)
            {
                appendLogText(i18n("Meridian flip set inactive during polar alignment."));
                m_MeridianFlipState->updateMFMountState(MeridianFlipState::MOUNT_FLIP_INACTIVE);
            }
            break;
        // activate it when the last rotation is finished or stopped
        // for safety reasons, we add all stages after the last rotation
        case PolarAlignmentAssistant::PAH_THIRD_CAPTURE:
        case PolarAlignmentAssistant::PAH_THIRD_SOLVE:
        case PolarAlignmentAssistant::PAH_STAR_SELECT:
        case PolarAlignmentAssistant::PAH_REFRESH:
        case PolarAlignmentAssistant::PAH_POST_REFRESH:
        case PolarAlignmentAssistant::PAH_IDLE:
            if (m_MeridianFlipState->getMeridianFlipMountState() == MeridianFlipState::MOUNT_FLIP_INACTIVE)
            {
                appendLogText(i18n("Polar alignment motions finished, meridian flip activated."));
                m_MeridianFlipState->updateMFMountState(MeridianFlipState::MOUNT_FLIP_NONE);
            }
            break;
    }
}

void Mount::meridianFlipSetupChanged()
{
    if (executeMeridianFlip->isChecked() == false)
        // reset meridian flip
        m_MeridianFlipState->updateMFMountState(MeridianFlipState::MOUNT_FLIP_NONE);
}

void Mount::updateSwitch(ISwitchVectorProperty * svp)
{
    if (!strcmp(svp->name, "TELESCOPE_SLEW_RATE"))
    {
        int index = IUFindOnSwitchIndex(svp);

        m_SpeedSlider->setProperty("value", index);
        m_SpeedLabel->setProperty("text", i18nc(libindi_strings_context, svp->sp[index].label));
    }
}

void Mount::appendLogText(const QString &text)
{
    m_LogText.insert(0, i18nc("log entry; %1 is the date, %2 is the text", "%1 %2",
                              KStarsData::Instance()->lt().toString("yyyy-MM-ddThh:mm:ss"), text));

    qCInfo(KSTARS_EKOS_MOUNT) << text;

    emit newLog(text);
}

void Mount::updateLog(int messageID)
{
    if (m_Mount == nullptr)
        return;

    auto message = m_Mount->getMessage(messageID);
    m_LogText.insert(0, i18nc("Message shown in Ekos Mount module", "%1", message));

    emit newLog(message);
}

void Mount::clearLog()
{
    m_LogText.clear();
    emit newLog(QString());
}

void Mount::motionCommand(int command, int NS, int WE)
{
    if (m_Mount == nullptr || !m_Mount->isConnected())
        return;

    if (NS != -1)
    {
        m_Mount->MoveNS(static_cast<ISD::Mount::VerticalMotion>(NS),
                        static_cast<ISD::Mount::MotionCommand>(command));
    }

    if (WE != -1)
    {
        m_Mount->MoveWE(static_cast<ISD::Mount::HorizontalMotion>(WE),
                        static_cast<ISD::Mount::MotionCommand>(command));
    }
}


void Mount::doPulse(GuideDirection ra_dir, int ra_msecs, GuideDirection dec_dir, int dec_msecs)
{
    if (m_Mount == nullptr || !m_Mount->isConnected())
        return;

    m_Mount->doPulse(ra_dir, ra_msecs, dec_dir, dec_msecs);
}

void Mount::saveLimits()
{
    if (m_Mount == nullptr)
        return;

    m_Mount->setAltLimits(minimumAltLimit->value(), maximumAltLimit->value());
}

void Mount::setAltitudeLimits(bool enable)
{
    if (enable)
    {
        minAltLabel->setEnabled(true);
        maxAltLabel->setEnabled(true);

        minimumAltLimit->setEnabled(true);
        maximumAltLimit->setEnabled(true);

        if (m_Mount)
            m_Mount->setAltLimits(minimumAltLimit->value(), maximumAltLimit->value());
    }
    else
    {
        minAltLabel->setEnabled(false);
        maxAltLabel->setEnabled(false);

        minimumAltLimit->setEnabled(false);
        maximumAltLimit->setEnabled(false);

        if (m_Mount)
            m_Mount->setAltLimits(-1, -1);
    }
}

void Mount::enableAltLimits()
{
    //Only enable if it was already enabled before and the MinimumAltLimit is currently disabled.
    if (m_AltitudeLimitEnabled && minimumAltLimit->isEnabled() == false)
        setAltitudeLimits(true);
}

void Mount::disableAltLimits()
{
    m_AltitudeLimitEnabled = enableAltitudeLimits->isChecked();

    setAltitudeLimits(false);
}

void Mount::enableHourAngleLimits(bool enable)
{
    maxHaLabel->setEnabled(enable);
    maximumHaLimit->setEnabled(enable);
}

void Mount::enableHaLimits()
{
    //Only enable if it was already enabled before and the minHaLimit is currently disabled.
    if (m_HourAngleLimitEnabled && maximumHaLimit->isEnabled() == false)
        enableHourAngleLimits(true);
}

void Mount::disableHaLimits()
{
    m_HourAngleLimitEnabled = enableHaLimit->isChecked();

    enableHourAngleLimits(false);
}

QList<double> Mount::altitudeLimits()
{
    QList<double> limits;

    limits.append(minimumAltLimit->value());
    limits.append(maximumAltLimit->value());

    return limits;
}

void Mount::setAltitudeLimits(QList<double> limits)
{
    minimumAltLimit->setValue(limits[0]);
    maximumAltLimit->setValue(limits[1]);
}

void Mount::setAltitudeLimitsEnabled(bool enable)
{
    enableAltitudeLimits->setChecked(enable);
}

bool Mount::altitudeLimitsEnabled()
{
    return enableAltitudeLimits->isChecked();
}

double Mount::hourAngleLimit()
{
    return maximumHaLimit->value();
}

void Mount::setHourAngleLimit(double limit)
{
    maximumHaLimit->setValue(limit);
}

void Mount::setHourAngleLimitEnabled(bool enable)
{
    enableHaLimit->setChecked(enable);
}

bool Mount::hourAngleLimitEnabled()
{
    return enableHaLimit->isChecked();
}

void Mount::setJ2000Enabled(bool enabled)
{
    m_J2000Check->setProperty("checked", enabled);
}

bool Mount::gotoTarget(const QString &target)
{
    SkyObject *object = KStarsData::Instance()->skyComposite()->findByName(target, false);

    if (object != nullptr)
    {
        object->updateCoordsNow(KStarsData::Instance()->updateNum());
        return slew(object->ra().Hours(), object->dec().Degrees());
    }

    return false;
}

bool Mount::gotoTarget(const SkyPoint &target)
{
    return slew(target.ra().Hours(), target.dec().Degrees());
}

bool Mount::syncTarget(const QString &target)
{
    SkyObject *object = KStarsData::Instance()->skyComposite()->findByName(target, false);

    if (object != nullptr)
    {
        object->updateCoordsNow(KStarsData::Instance()->updateNum());
        return sync(object->ra().Hours(), object->dec().Degrees());
    }

    return false;
}

bool Mount::slew(const QString &RA, const QString &DEC)
{
    dms ra, de;

    if (m_equatorialCheck->property("checked").toBool())
    {
        ra = dms::fromString(RA, false);
        de = dms::fromString(DEC, true);
    }

    if (m_horizontalCheck->property("checked").toBool())
    {
        dms az = dms::fromString(RA, true);
        dms at = dms::fromString(DEC, true);
        SkyPoint target;
        target.setAz(az);
        target.setAlt(at);
        target.HorizontalToEquatorial(KStars::Instance()->data()->lst(), KStars::Instance()->data()->geo()->lat());
        ra = target.ra();
        de = target.dec();
    }

    if (m_haEquatorialCheck->property("checked").toBool())
    {
        dms ha = dms::fromString(RA, false);
        de = dms::fromString(DEC, true);
        dms lst = KStarsData::Instance()->geo()->GSTtoLST(KStarsData::Instance()->clock()->utc().gst());
        ra = (lst - ha + dms(360.0)).reduce();
    }

    // If J2000 was checked and the Mount is _not_ already using native J2000 coordinates
    // then we need to convert J2000 to JNow. Otherwise, we send J2000 as is.
    if (m_J2000Check->property("checked").toBool() && m_Mount && m_Mount->isJ2000() == false)
    {
        // J2000 ---> JNow
        SkyPoint J2000Coord(ra, de);
        J2000Coord.setRA0(ra);
        J2000Coord.setDec0(de);
        J2000Coord.apparentCoord(static_cast<long double>(J2000), KStars::Instance()->data()->ut().djd());

        ra = J2000Coord.ra();
        de = J2000Coord.dec();
    }

    return slew(ra.Hours(), de.Degrees());
}

bool Mount::slew(double RA, double DEC)
{
    if (m_Mount == nullptr || m_Mount->isConnected() == false)
        return false;

    // save the current hour angle
    m_MeridianFlipState->setInitialPositionHA(RA);

    // reset the meridian flip status if the slew is not the meridian flip itself
    if (m_MeridianFlipState->getMeridianFlipMountState() != MeridianFlipState::MOUNT_FLIP_RUNNING)
    {
        m_MeridianFlipState->updateMFMountState(MeridianFlipState::MOUNT_FLIP_NONE);
        m_MeridianFlipState->setFlipDelayHrs(0);
        qCDebug(KSTARS_EKOS_MOUNT) << "flipDelayHrs set to zero in slew, m_MFStatus=" <<
                                   MeridianFlipState::meridianFlipStatusString(m_MeridianFlipState->getMeridianFlipMountState());
    }

    delete currentTargetPosition;
    currentTargetPosition = new SkyPoint(RA, DEC);
    SkyPoint J2000Coord(currentTargetPosition->ra(), currentTargetPosition->dec());
    J2000Coord.catalogueCoord(KStars::Instance()->data()->ut().djd());
    currentTargetPosition->setRA0(J2000Coord.ra());
    currentTargetPosition->setDec0(J2000Coord.dec());


    qCDebug(KSTARS_EKOS_MOUNT) << "Slewing to RA=" <<
                               currentTargetPosition->ra().toHMSString() <<
                               "DEC=" << currentTargetPosition->dec().toDMSString();
    qCDebug(KSTARS_EKOS_MOUNT) << "Initial HA " << initialHA() << ", flipDelayHrs " << m_MeridianFlipState->getFlipDelayHrs() <<
                               "MFStatus " << MeridianFlipState::meridianFlipStatusString(m_MeridianFlipState->getMeridianFlipMountState());

    // start the slew
    return(m_Mount->Slew(currentTargetPosition));
}

///
/// \brief Mount::checkMeridianFlip  This updates the Meridian Flip Status state machine using the LST supplied,
/// the mount target position and the pier side if available.
/// \param lst
/// \return true if a flip slew can be started, false otherwise
///
bool Mount::checkMeridianFlip(dms lst)
{
    // checks if a flip is possible
    if (m_Mount == nullptr || m_Mount->isConnected() == false)
    {
        m_MeridianFlipState->publishMFMountStatusText(i18n("Meridian flip inactive (no scope connected)"));
        m_MeridianFlipState->updateMFMountState(MeridianFlipState::MOUNT_FLIP_NONE);
        return false;
    }

    if (executeMeridianFlip->isChecked() == false)
    {
        m_MeridianFlipState->publishMFMountStatusText(i18n("Meridian flip inactive (flip not requested)"));
        return false;
    }

    // Will never get called when parked!
    if (m_Mount->isParked())
    {
        m_MeridianFlipState->publishMFMountStatusText(i18n("Meridian flip inactive (parked)"));
        return false;
    }

    if (currentTargetPosition == nullptr
            || m_MeridianFlipState->getMeridianFlipMountState() == MeridianFlipState::MOUNT_FLIP_INACTIVE)
    {
        m_MeridianFlipState->publishMFMountStatusText(i18n("Meridian flip inactive (no target set)"));
        return false;
    }

    // get the time after the meridian that the flip is called for (Degrees --> Hours)
    double offset = rangeHA(meridianFlipOffsetDegrees->value() / 15.0);

    double hrsToFlip = 0;       // time to go to the next flip - hours  -ve means a flip is required

    double ha = rangeHA(lst.Hours() - telescopeCoord.ra().Hours());     // -12 to 0 to +12

    // calculate time to next flip attempt.  This uses the current hour angle, the pier side if available
    // and the meridian flip offset to get the time to the flip
    //
    // *** should it use the target position so it will continue to track the target even if the mount is not tracking?
    //
    // Note: the PierSide code relies on the mount reporting the pier side correctly
    // It is possible that a mount can flip before the meridian and this has caused problems so hrsToFlip is calculated
    // assuming the the mount can flip up to three hours early.

    static ISD::Mount::PierSide
    initialPierSide;    // used when the flip has completed to determine if the flip was successful

    // adjust ha according to the pier side.
    switch (m_Mount->pierSide())
    {
        case ISD::Mount::PierSide::PIER_WEST:
            // this is the normal case, tracking from East to West, flip is near Ha 0.
            break;
        case ISD::Mount::PierSide::PIER_EAST:
            // this is the below the pole case, tracking West to East, flip is near Ha 12.
            // shift ha by 12h
            ha = rangeHA(ha + 12);
            break;
        default:
            // This is the case where the PierSide is not available, make one attempt only
            m_MeridianFlipState->setFlipDelayHrs(0);
            // we can only attempt a flip if the mount started before the meridian, assumed in the unflipped state
            if (initialHA() >= 0)
            {
                m_MeridianFlipState->publishMFMountStatusText(i18n("Meridian flip inactive (slew after meridian)"));
                if (m_MeridianFlipState->getMeridianFlipMountState() == MeridianFlipState::MOUNT_FLIP_NONE)
                    return false;
            }
            break;
    }
    // get the time to the next flip, allowing for the pier side and
    // the possibility of an early flip
    // adjust ha so an early flip is allowed for
    if (ha >= 9.0)
        ha -= 24.0;
    hrsToFlip = offset + m_MeridianFlipState->getFlipDelayHrs() - ha;

    int hh = static_cast<int> (hrsToFlip);
    int mm = static_cast<int> ((hrsToFlip - hh) * 60);
    int ss = static_cast<int> ((hrsToFlip - hh - mm / 60.0) * 3600);
    QString message = i18n("Meridian flip in %1", QTime(hh, mm, ss).toString(Qt::TextDate));

    // handle the meridian flip state machine
    switch (m_MeridianFlipState->getMeridianFlipMountState())
    {
        case MeridianFlipState::MOUNT_FLIP_NONE:
            m_MeridianFlipState->publishMFMountStatusText(message);

            if (hrsToFlip <= 0)
            {
                // signal that a flip can be done
                qCDebug(KSTARS_EKOS_MOUNT) << "Meridian flip planned with LST=" <<
                                           lst.toHMSString() <<
                                           " scope RA=" << telescopeCoord.ra().toHMSString() <<
                                           " ha=" << ha <<
                                           ", meridian diff=" << offset <<
                                           ", hrstoFlip=" << hrsToFlip <<
                                           ", flipDelayHrs=" << m_MeridianFlipState->getFlipDelayHrs() <<
                                           ", " << pierSideStateString();

                initialPierSide = m_Mount->pierSide();
                m_MeridianFlipState->updateMFMountState(MeridianFlipState::MOUNT_FLIP_PLANNED);
            }
            break;

        case MeridianFlipState::MOUNT_FLIP_PLANNED:
            // handle the case where there is no Capture module
            if (captureInterface == nullptr)
            {
                qCDebug(KSTARS_EKOS_MOUNT) << "no capture interface, starting flip slew.";
                m_MeridianFlipState->updateMFMountState(MeridianFlipState::MOUNT_FLIP_ACCEPTED);
                return true;
            }
            return false;

        case MeridianFlipState::MOUNT_FLIP_ACCEPTED:
            // set by the Capture module when it's ready
            return true;

        case MeridianFlipState::MOUNT_FLIP_RUNNING:
            if (m_Mount->isTracking())
            {
                // meridian flip slew completed, did it work?
                bool flipFailed = false;

                // pointing state change check only for mounts that report pier side
                if (m_Mount->pierSide() == ISD::Mount::PIER_UNKNOWN)
                {
                    // check how long it took
                    if (m_MeridianFlipState->getMinMeridianFlipEndTime() > QDateTime::currentDateTimeUtc())
                    {
                        // don't fail, we have tried but we don't know where the mount was when it started
                        appendLogText(i18n("Meridian flip failed - time too short, pier side unknown."));
                        // signal that capture can resume
                        m_MeridianFlipState->updateMFMountState(MeridianFlipState::MOUNT_FLIP_COMPLETED);
                        return false;
                    }
                }
                else if (m_Mount->pierSide() == initialPierSide)
                {
                    flipFailed = true;
                    qCWarning(KSTARS_EKOS_MOUNT) << "Meridian flip failed, pier side not changed";
                }

                if (flipFailed)
                {
                    if (m_MeridianFlipState->getFlipDelayHrs() <= 1.0)
                    {
                        // Set next flip attempt to be 4 minutes in the future.
                        // These depend on the assignment to flipDelayHrs above.
                        constexpr double delayHours = 4.0 / 60.0;
                        if (m_Mount->pierSide() == ISD::Mount::PierSide::PIER_EAST)
                            m_MeridianFlipState->setFlipDelayHrs(rangeHA(ha + 12 + delayHours) - offset);
                        else
                            m_MeridianFlipState->setFlipDelayHrs(ha + delayHours - offset);

                        // check to stop an infinite loop, 1.0 hrs for now but should use the Ha limit
                        appendLogText(i18n("meridian flip failed, retrying in 4 minutes"));
                    }
                    else
                    {
                        appendLogText(i18n("No successful Meridian Flip done, delay too long"));
                    }
                    m_MeridianFlipState->updateMFMountState(
                        MeridianFlipState::MOUNT_FLIP_COMPLETED);   // this will resume imaging and try again after the extra delay
                }
                else
                {
                    m_MeridianFlipState->setFlipDelayHrs(0);
                    appendLogText(i18n("Meridian flip completed OK."));
                    // signal that capture can resume
                    m_MeridianFlipState->updateMFMountState(MeridianFlipState::MOUNT_FLIP_COMPLETED);
                }
            }
            break;

        case MeridianFlipState::MOUNT_FLIP_COMPLETED:
            m_MeridianFlipState->updateMFMountState(MeridianFlipState::MOUNT_FLIP_NONE);
            break;

        default:
            break;
    }
    return false;
}

bool Mount::startMeridianFlip()
{
    if (/*initialHA() > 0 || */ currentTargetPosition == nullptr)
    {
        // no meridian flip necessary
        qCDebug(KSTARS_EKOS_MOUNT) << "No meridian flip: currentTargetPosition is null";
        return false;
    }

    if (m_Mount->status() != ISD::Mount::MOUNT_TRACKING)
    {
        // no meridian flip necessary
        qCDebug(KSTARS_EKOS_MOUNT) << "No meridian flip: mount not tracking";
        return false;
    }

    dms lst = KStarsData::Instance()->geo()->GSTtoLST(KStarsData::Instance()->clock()->utc().gst());
    double HA = rangeHA(lst.Hours() - currentTargetPosition->ra().Hours());

    // execute meridian flip
    qCInfo(KSTARS_EKOS_MOUNT) << "Meridian flip: slewing to RA=" <<
                              currentTargetPosition->ra().toHMSString() <<
                              "DEC=" << currentTargetPosition->dec().toDMSString() <<
                              " Hour Angle " << dms(HA).toHMSString();
    m_MeridianFlipState->updateMFMountState(MeridianFlipState::MOUNT_FLIP_RUNNING);

    m_MeridianFlipState->updateMinMeridianFlipEndTime();

    if (slew(currentTargetPosition->ra().Hours(), currentTargetPosition->dec().Degrees()))
    {
        appendLogText(i18n("Meridian flip slew started..."));
        return true;
    }
    else
    {
        qCWarning(KSTARS_EKOS_MOUNT) << "Meridian flip FAILED: slewing to RA=" <<
                                     currentTargetPosition->ra().toHMSString() <<
                                     "DEC=" << currentTargetPosition->dec().toDMSString();
        return false;
    }
}


SkyPoint Mount::currentTarget()
{
    return *currentTargetPosition;
}


bool Mount::sync(const QString &RA, const QString &DEC)
{
    dms ra, de;

    if (m_equatorialCheck->property("checked").toBool())
    {
        ra = dms::fromString(RA, false);
        de = dms::fromString(DEC, true);
    }

    if (m_horizontalCheck->property("checked").toBool())
    {
        dms az = dms::fromString(RA, true);
        dms at = dms::fromString(DEC, true);
        SkyPoint target;
        target.setAz(az);
        target.setAlt(at);
        target.HorizontalToEquatorial(KStars::Instance()->data()->lst(), KStars::Instance()->data()->geo()->lat());
        ra = target.ra();
        de = target.dec();
    }

    if (m_haEquatorialCheck->property("checked").toBool())
    {
        dms ha = dms::fromString(RA, false);
        de = dms::fromString(DEC, true);
        dms lst = KStarsData::Instance()->geo()->GSTtoLST(KStarsData::Instance()->clock()->utc().gst());
        ra = (lst - ha + dms(360.0)).reduce();
    }

    if (m_J2000Check->property("checked").toBool())
    {
        // J2000 ---> JNow
        SkyPoint J2000Coord(ra, de);
        J2000Coord.setRA0(ra);
        J2000Coord.setDec0(de);
        J2000Coord.updateCoordsNow(KStarsData::Instance()->updateNum());

        ra = J2000Coord.ra();
        de = J2000Coord.dec();
    }

    return sync(ra.Hours(), de.Degrees());
}

bool Mount::sync(double RA, double DEC)
{
    if (m_Mount == nullptr || m_Mount->isConnected() == false)
        return false;

    return m_Mount->Sync(RA, DEC);
}

bool Mount::abort()
{
    if (m_Mount == nullptr)
        return false;

    return m_Mount->abort();
}

IPState Mount::slewStatus()
{
    if (m_Mount == nullptr)
        return IPS_ALERT;

    return m_Mount->getState("EQUATORIAL_EOD_COORD");
}

QList<double> Mount::equatorialCoords()
{
    double ra {0}, dec {0};
    QList<double> coords;

    if (m_Mount)
        m_Mount->getEqCoords(&ra, &dec);
    coords.append(ra);
    coords.append(dec);

    return coords;
}

QList<double> Mount::horizontalCoords()
{
    QList<double> coords;

    coords.append(telescopeCoord.az().Degrees());
    coords.append(telescopeCoord.alt().Degrees());

    return coords;
}

///
/// \brief Mount::hourAngle
/// \return returns the current mount hour angle in hours in the range -12 to +12
///
double Mount::hourAngle()
{
    dms lst = KStarsData::Instance()->geo()->GSTtoLST(KStarsData::Instance()->clock()->utc().gst());
    dms ha(lst.Degrees() - telescopeCoord.ra().Degrees());
    return rangeHA(ha.Hours());
}

bool Mount::canPark()
{
    if (m_Mount == nullptr)
        return false;

    return m_Mount->canPark();
}

bool Mount::park()
{
    if (m_Mount == nullptr || m_Mount->canPark() == false)
        return false;

    return m_Mount->park();
}

bool Mount::unpark()
{
    if (m_Mount == nullptr || m_Mount->canPark() == false)
        return false;

    return m_Mount->unpark();
}


void Mount::toggleMountToolBox()
{
    if (m_BaseView->isVisible())
    {
        m_BaseView->hide();
        QAction *a = KStars::Instance()->actionCollection()->action("show_mount_box");
        if (a)
            a->setChecked(false);
    }
    else
    {
        m_BaseView->show();
        QAction *a = KStars::Instance()->actionCollection()->action("show_mount_box");
        if (a)
            a->setChecked(true);
    }
}

void Mount::findTarget()
{
    if (FindDialog::Instance()->execWithParent(Ekos::Manager::Instance()) == QDialog::Accepted)
    {
        SkyObject *object = FindDialog::Instance()->targetObject();
        if (object != nullptr)
        {
            KStarsData * const data = KStarsData::Instance();

            SkyObject *o = object->clone();
            o->updateCoords(data->updateNum(), true, data->geo()->lat(), data->lst(), false);

            m_equatorialCheck->setProperty("checked", true);

            m_targetText->setProperty("text", o->name());

            if (m_JNowCheck->property("checked").toBool())
            {
                m_targetRAText->setProperty("text", o->ra().toHMSString());
                m_targetDEText->setProperty("text", o->dec().toDMSString());
            }
            else
            {
                m_targetRAText->setProperty("text", o->ra0().toHMSString());
                m_targetDEText->setProperty("text", o->dec0().toDMSString());
            }
        }
    }
}

//++++ converters for target coordinate display in Mount Control box

bool Mount::raDecToAzAlt(QString qsRA, QString qsDec)
{
    dms RA, Dec;

    if (!RA.setFromString(qsRA, false) || !Dec.setFromString(qsDec, true))
        return false;

    SkyPoint targetCoord(RA, Dec);

    targetCoord.EquatorialToHorizontal(KStarsData::Instance()->lst(),
                                       KStarsData::Instance()->geo()->lat());

    m_targetRAText->setProperty("text", targetCoord.az().toDMSString());
    m_targetDEText->setProperty("text", targetCoord.alt().toDMSString());

    return true;
}

bool  Mount::raDecToHaDec(QString qsRA)
{
    dms RA;

    if (!RA.setFromString(qsRA, false))
        return false;

    dms lst = KStarsData::Instance()->geo()->GSTtoLST(KStarsData::Instance()->clock()->utc().gst());

    dms HA = (lst - RA + dms(360.0)).reduce();

    QChar sgn('+');
    if (HA.Hours() > 12.0)
    {
        HA.setH(24.0 - HA.Hours());
        sgn = '-';
    }

    m_targetRAText->setProperty("text", QString("%1%2").arg(sgn).arg(HA.toHMSString()));

    return true;
}

bool  Mount::azAltToRaDec(QString qsAz, QString qsAlt)
{
    dms Az, Alt;

    if (!Az.setFromString(qsAz, true) || !Alt.setFromString(qsAlt, true))
        return false;

    SkyPoint targetCoord;
    targetCoord.setAz(Az);
    targetCoord.setAlt(Alt);

    targetCoord.HorizontalToEquatorial(KStars::Instance()->data()->lst(),
                                       KStars::Instance()->data()->geo()->lat());

    m_targetRAText->setProperty("text", targetCoord.ra().toHMSString());
    m_targetDEText->setProperty("text", targetCoord.dec().toDMSString());

    return true;
}

bool  Mount::azAltToHaDec(QString qsAz, QString qsAlt)
{
    dms Az, Alt;

    if (!Az.setFromString(qsAz, true) || !Alt.setFromString(qsAlt, true))
        return false;

    SkyPoint targetCoord;
    targetCoord.setAz(Az);
    targetCoord.setAlt(Alt);

    dms lst = KStarsData::Instance()->geo()->GSTtoLST(KStarsData::Instance()->clock()->utc().gst());

    targetCoord.HorizontalToEquatorial(&lst, KStars::Instance()->data()->geo()->lat());

    dms HA = (lst - targetCoord.ra() + dms(360.0)).reduce();

    QChar sgn('+');
    if (HA.Hours() > 12.0)
    {
        HA.setH(24.0 - HA.Hours());
        sgn = '-';
    }

    m_targetRAText->setProperty("text", QString("%1%2").arg(sgn).arg(HA.toHMSString()));
    m_targetDEText->setProperty("text", targetCoord.dec().toDMSString());


    return true;
}

bool  Mount::haDecToRaDec(QString qsHA)
{
    dms HA;

    if (!HA.setFromString(qsHA, false))
        return false;

    dms lst = KStarsData::Instance()->geo()->GSTtoLST(KStarsData::Instance()->clock()->utc().gst());
    dms RA = (lst - HA + dms(360.0)).reduce();

    m_targetRAText->setProperty("text", RA.toHMSString());

    return true;
}

bool  Mount::haDecToAzAlt(QString qsHA, QString qsDec)
{
    dms HA, Dec;

    if (!HA.setFromString(qsHA, false) || !Dec.setFromString(qsDec, true))
        return false;

    dms lst = KStarsData::Instance()->geo()->GSTtoLST(KStarsData::Instance()->clock()->utc().gst());
    dms RA = (lst - HA + dms(360.0)).reduce();

    SkyPoint targetCoord;
    targetCoord.setRA(RA);
    targetCoord.setDec(Dec);

    targetCoord.EquatorialToHorizontal(&lst, KStars::Instance()->data()->geo()->lat());

    m_targetRAText->setProperty("text", targetCoord.az().toDMSString());
    m_targetDEText->setProperty("text", targetCoord.alt().toDMSString());

    return true;
}

//---- end: converters for target coordinate display in Mount Control box

void Mount::centerMount()
{
    if (m_Mount)
        m_Mount->find();
}

bool Mount::resetModel()
{
    if (m_Mount == nullptr)
        return false;

    if (m_Mount->hasAlignmentModel() == false)
        return false;

    if (m_Mount->clearAlignmentModel())
    {
        appendLogText(i18n("Alignment Model cleared."));
        return true;
    }

    appendLogText(i18n("Failed to clear Alignment Model."));
    return false;
}


void Mount::setScopeStatus(ISD::Mount::Status status)
{
    if (m_Status != status)
    {
        m_statusText->setProperty("text", m_Mount->statusString(status));
        m_Status = status;
        // forward
        emit newStatus(status);
    }
}



void Mount::setTrackEnabled(bool enabled)
{
    if (enabled)
        trackOnB->click();
    else
        trackOffB->click();
}

int Mount::slewRate()
{
    if (m_Mount == nullptr)
        return -1;

    return m_Mount->getSlewRate();
}

//QJsonArray Mount::getScopes() const
//{
//    QJsonArray scopes;
//    if (currentTelescope == nullptr)
//        return scopes;

//    QJsonObject primary =
//    {
//        {"name", "Primary"},
//        {"mount", currentTelescope->getDeviceName()},
//        {"aperture", primaryScopeApertureIN->value()},
//        {"focalLength", primaryScopeFocalIN->value()},
//    };

//    scopes.append(primary);

//    QJsonObject guide =
//    {
//        {"name", "Guide"},
//        {"mount", currentTelescope->getDeviceName()},
//        {"aperture", primaryScopeApertureIN->value()},
//        {"focalLength", primaryScopeFocalIN->value()},
//    };

//    scopes.append(guide);

//    return scopes;
//}

bool Mount::autoParkEnabled()
{
    return autoParkTimer.isActive();
}

void Mount::setAutoParkEnabled(bool enable)
{
    if (enable)
        startParkTimer();
    else
        stopParkTimer();
}

void Mount::setAutoParkDailyEnabled(bool enabled)
{
    parkEveryDay->setChecked(enabled);
}

void Mount::setAutoParkStartup(QTime startup)
{
    ParkTime->setTime(startup);
}

bool Mount::meridianFlipEnabled()
{
    return executeMeridianFlip->isChecked();
}

double Mount::meridianFlipValue()
{
    return meridianFlipOffsetDegrees->value();
}

void Mount::stopTimers()
{
    autoParkTimer.stop();
    if (m_Mount)
        m_Mount->stopTimers();
}

void Mount::startParkTimer()
{
    if (m_Mount == nullptr || m_ParkStatus == ISD::PARK_UNKNOWN)
        return;

    if (m_Mount->isParked())
    {
        appendLogText(i18n("Mount already parked."));
        return;
    }

    QTime parkTime = ParkTime->time();

    qCDebug(KSTARS_EKOS_MOUNT) << "Parking time is" << parkTime.toString();
    QDateTime currentDateTime = KStarsData::Instance()->lt();
    QDateTime parkDateTime(currentDateTime);

    parkDateTime.setTime(parkTime);
    qint64 parkMilliSeconds = parkDateTime.msecsTo(currentDateTime);
    qCDebug(KSTARS_EKOS_MOUNT) << "Until parking time:" << parkMilliSeconds << "ms or" << parkMilliSeconds / (60 * 60 * 1000)
                               << "hours";
    if (parkMilliSeconds > 0)
    {
        qCDebug(KSTARS_EKOS_MOUNT) << "Added a day to parking time...";
        parkDateTime = parkDateTime.addDays(1);
        parkMilliSeconds = parkDateTime.msecsTo(currentDateTime);

        int hours = static_cast<int>(parkMilliSeconds / (1000 * 60 * 60));
        if (hours > 0)
        {
            // No need to display warning for every day check
            if (parkEveryDay->isChecked() == false)
                appendLogText(i18n("Parking time cannot be in the past."));
            return;
        }
    }

    parkMilliSeconds = std::abs(parkMilliSeconds);

    if (parkMilliSeconds > 24 * 60 * 60 * 1000)
    {
        appendLogText(i18n("Parking time must be within 24 hours of current time."));
        return;
    }

    if (parkMilliSeconds > 12 * 60 * 60 * 1000)
        appendLogText(i18n("Warning! Parking time is more than 12 hours away."));

    appendLogText(i18n("Caution: do not use Auto Park while scheduler is active."));

    autoParkTimer.setInterval(static_cast<int>(parkMilliSeconds));
    autoParkTimer.start();

    startTimerB->setEnabled(false);
    stopTimerB->setEnabled(true);
}

void Mount::stopParkTimer()
{
    autoParkTimer.stop();
    countdownLabel->setText("00:00:00");
    stopTimerB->setEnabled(false);
    startTimerB->setEnabled(true);
}

void Mount::startAutoPark()
{
    appendLogText(i18n("Parking timer is up."));
    autoParkTimer.stop();
    startTimerB->setEnabled(true);
    stopTimerB->setEnabled(false);
    countdownLabel->setText("00:00:00");
    if (m_Mount)
    {
        if (m_Mount->isParked() == false)
        {
            appendLogText(i18n("Starting auto park..."));
            park();
        }
    }
}

QString Mount::pierSideStateString()
{
    switch (m_Mount->pierSide())
    {
        case ISD::Mount::PierSide::PIER_EAST:
            return "Pier Side: East (pointing West)";
        case ISD::Mount::PierSide::PIER_WEST:
            return "Pier Side: West (pointing East)";
        default:
            return "Pier Side: Unknown";
    }
}

void Mount::syncAxisReversed(INDI_EQ_AXIS axis, bool reversed)
{
    if (axis == AXIS_RA)
        m_leftRightCheck->setProperty("checked", reversed);
    else
        m_upDownCheck->setProperty("checked", reversed);
}

void Mount::setupOpticalTrainManager()
{
    connect(OpticalTrainManager::Instance(), &OpticalTrainManager::updated, this, &Mount::refreshOpticalTrain);
    connect(trainB, &QPushButton::clicked, OpticalTrainManager::Instance(), &OpticalTrainManager::show);
    connect(opticalTrainCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index)
    {
        ProfileSettings::Instance()->setOneSetting(ProfileSettings::MountOpticalTrain,
                OpticalTrainManager::Instance()->id(opticalTrainCombo->itemText(index)));
        refreshOpticalTrain();
        emit trainChanged();
    });
    refreshOpticalTrain();


}

void Mount::refreshOpticalTrain()
{
    opticalTrainCombo->blockSignals(true);
    opticalTrainCombo->clear();
    opticalTrainCombo->addItems(OpticalTrainManager::Instance()->getTrainNames());
    trainB->setEnabled(opticalTrainCombo->count() > 0);

    QVariant trainID = ProfileSettings::Instance()->getOneSetting(ProfileSettings::MountOpticalTrain);

    if (trainID.isValid())
    {
        auto id = trainID.toUInt();
        auto name = OpticalTrainManager::Instance()->name(id);

        opticalTrainCombo->setCurrentText(name);

        auto mount = OpticalTrainManager::Instance()->getMount(name);
        setMount(mount);

        auto scope = OpticalTrainManager::Instance()->getScope(name);
        opticalTrainCombo->setToolTip(scope["name"].toString());

        // Load train settings
        OpticalTrainSettings::Instance()->setOpticalTrainID(id);
        auto settings = OpticalTrainSettings::Instance()->getOneSetting(OpticalTrainSettings::Mount);
        if (settings.isValid())
            setAllSettings(settings.toJsonObject().toVariantMap());
        else
            m_Settings = m_GlobalSettings;
    }

    opticalTrainCombo->blockSignals(false);
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
QVariantMap Mount::getAllSettings() const
{
    QVariantMap settings;

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

    // All Time
    for (auto &oneWidget : findChildren<QTimeEdit*>())
        settings.insert(oneWidget->objectName(), oneWidget->time().toString());

    return settings;
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Mount::setAllSettings(const QVariantMap &settings)
{
    // Disconnect settings that we don't end up calling syncSettings while
    // performing the changes.
    disconnectSettings();

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

        // timeEdit
        auto timeEdit = findChild<QTimeEdit*>(name);
        if (timeEdit)
        {
            syncControl(settings, name, timeEdit);
            continue;
        }
    }

    m_Settings = settings;

    // Restablish connections
    connectSettings();
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
bool Mount::syncControl(const QVariantMap &settings, const QString &key, QWidget * widget)
{
    QSpinBox *pSB = nullptr;
    QDoubleSpinBox *pDSB = nullptr;
    QCheckBox *pCB = nullptr;
    QComboBox *pComboBox = nullptr;
    QTimeEdit *pTimeEdit = nullptr;
    bool ok = false;

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
            return true;
        }
    }
    else if ((pCB = qobject_cast<QCheckBox *>(widget)))
    {
        const bool value = settings[key].toBool();
        pCB->setChecked(value);
        return true;
    }
    // ONLY FOR STRINGS, not INDEX
    else if ((pComboBox = qobject_cast<QComboBox *>(widget)))
    {
        const QString value = settings[key].toString();
        pComboBox->setCurrentText(value);
        return true;
    }
    else if ((pTimeEdit = qobject_cast<QTimeEdit *>(widget)))
    {
        const QString value = settings[key].toString();
        pTimeEdit->setTime(QTime::fromString(value));
        return true;
    }

    return false;
};

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Mount::syncSettings()
{
    QDoubleSpinBox *dsb = nullptr;
    QSpinBox *sb = nullptr;
    QCheckBox *cb = nullptr;
    QComboBox *cbox = nullptr;
    QTimeEdit *timeEdit = nullptr;

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
    else if ( (cbox = qobject_cast<QComboBox*>(sender())))
    {
        key = cbox->objectName();
        value = cbox->currentText();
    }
    else if ( (timeEdit = qobject_cast<QTimeEdit*>(sender())))
    {
        key = timeEdit->objectName();
        value = timeEdit->time().toString();
    }

    // Save immediately
    Options::self()->setProperty(key.toLatin1(), value);
    Options::self()->save();

    m_Settings[key] = value;
    m_GlobalSettings[key] = value;

    emit settingsUpdated(getAllSettings());

    // Save to optical train specific settings as well
    OpticalTrainSettings::Instance()->setOpticalTrainID(OpticalTrainManager::Instance()->id(opticalTrainCombo->currentText()));
    OpticalTrainSettings::Instance()->setOneSetting(OpticalTrainSettings::Mount, m_Settings);
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Mount::loadGlobalSettings()
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
            qCDebug(KSTARS_EKOS_MOUNT) << "Option" << key << "not found!";
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
            qCDebug(KSTARS_EKOS_MOUNT) << "Option" << key << "not found!";
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
            qCDebug(KSTARS_EKOS_MOUNT) << "Option" << key << "not found!";
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
            qCDebug(KSTARS_EKOS_MOUNT) << "Option" << key << "not found!";
    }

    m_GlobalSettings = m_Settings = settings;
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Mount::connectSettings()
{
    // All Combo Boxes
    for (auto &oneWidget : findChildren<QComboBox*>())
        connect(oneWidget, QOverload<int>::of(&QComboBox::activated), this, &Ekos::Mount::syncSettings);

    // All Double Spin Boxes
    for (auto &oneWidget : findChildren<QDoubleSpinBox*>())
        connect(oneWidget, &QDoubleSpinBox::editingFinished, this, &Ekos::Mount::syncSettings);

    // All Spin Boxes
    for (auto &oneWidget : findChildren<QSpinBox*>())
        connect(oneWidget, &QSpinBox::editingFinished, this, &Ekos::Mount::syncSettings);

    // All Checkboxes
    for (auto &oneWidget : findChildren<QCheckBox*>())
        connect(oneWidget, &QCheckBox::toggled, this, &Ekos::Mount::syncSettings);

    // All QDateTimeEdit
    for (auto &oneWidget : findChildren<QDateTimeEdit*>())
        connect(oneWidget, &QDateTimeEdit::editingFinished, this, &Ekos::Mount::syncSettings);

    // Train combo box should NOT be synced.
    disconnect(opticalTrainCombo, QOverload<int>::of(&QComboBox::activated), this, &Ekos::Mount::syncSettings);
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Mount::disconnectSettings()
{
    // All Combo Boxes
    for (auto &oneWidget : findChildren<QComboBox*>())
        disconnect(oneWidget, QOverload<int>::of(&QComboBox::activated), this, &Ekos::Mount::syncSettings);

    // All Double Spin Boxes
    for (auto &oneWidget : findChildren<QDoubleSpinBox*>())
        disconnect(oneWidget, &QDoubleSpinBox::editingFinished, this, &Ekos::Mount::syncSettings);

    // All Spin Boxes
    for (auto &oneWidget : findChildren<QSpinBox*>())
        disconnect(oneWidget, &QSpinBox::editingFinished, this, &Ekos::Mount::syncSettings);

    // All Checkboxes
    for (auto &oneWidget : findChildren<QCheckBox*>())
        disconnect(oneWidget, &QCheckBox::toggled, this, &Ekos::Mount::syncSettings);

    for (auto &oneWidget : findChildren<QDateTimeEdit*>())
        disconnect(oneWidget, &QDateTimeEdit::editingFinished, this, &Ekos::Mount::syncSettings);

}

}
