/*
    SPDX-FileCopyrightText: 2015 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "mount.h"

#include <QQuickView>
#include <QQuickItem>
#include <indicom.h>

#include <knotification.h>
#include <KLocalizedContext>
#include <KActionCollection>

#include <kio_version.h>

#include "Options.h"

#include "ksmessagebox.h"
#include "indi/driverinfo.h"
#include "indi/indicommon.h"
#include "indi/clientmanager.h"
#include "indi/indigps.h"


#include "mountadaptor.h"
#include "mountcontrolpanel.h"

#include "ekos/manager.h"
#include "ekos/auxiliary/opticaltrainmanager.h"
#include "ekos/auxiliary/profilesettings.h"
#include "ekos/auxiliary/opticaltrainsettings.h"
#include "ekos/manager/meridianflipstate.h"
#include "ekos/align/polaralignmentassistant.h"

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

    // initialize the state machine
    mf_state.reset(new MeridianFlipState());
    // connect to the MF state maichine
    getMeridianFlipState()->connectMount(this);

    // set the status message in the mount tab and write it to the log
    connect(mf_state.get(), &MeridianFlipState::newMeridianFlipMountStatusText, [&](const QString & text)
    {
        meridianFlipStatusWidget->setStatus(text);
        if (mf_state->getMeridianFlipMountState() != MeridianFlipState::MOUNT_FLIP_NONE &&
                mf_state->getMeridianFlipMountState() != MeridianFlipState::MOUNT_FLIP_PLANNED)
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
            if (KMessageBox::warningContinueCancel(KStars::Instance(),
                                           i18n("Are you sure you want to clear all mount configurations?"),
                                           i18n("Mount Configuration")) == KMessageBox::Continue)
            {
                resetModel();
                m_Mount->clearParking();
                m_Mount->setConfig(PURGE_CONFIG);
            }
        }
    });

    // If time source changes, sync time source
    connect(Options::self(), &Options::timeSourceChanged, this, &Mount::syncTimeSource);
    connect(Options::self(), &Options::locationSourceChanged, this, &Mount::syncLocationSource);

    connect(enableAltitudeLimits, &QCheckBox::toggled, this, [this](bool toggled)
    {
        m_AltitudeLimitEnabled = toggled;
        setAltitudeLimits(toggled);

    });
    connect(enableAltitudeLimitsTrackingOnly, &QCheckBox::toggled, this, [this]()
    {
        setAltitudeLimits(enableAltitudeLimits->isChecked());
    });

    m_AltitudeLimitEnabled = enableAltitudeLimits->isChecked();
    connect(enableHaLimit, &QCheckBox::toggled, this, &Mount::enableHourAngleLimits);

    // meridian flip
    connect(mf_state.get(), &MeridianFlipState::newMeridianFlipMountStatusText, meridianFlipStatusWidget,
            &MeridianFlipStatusWidget::setStatus);

    connect(&autoParkTimer, &QTimer::timeout, this, &Mount::startAutoPark);
    connect(startTimerB, &QPushButton::clicked, this, &Mount::startParkTimer);
    connect(stopTimerB, &QPushButton::clicked, this, &Mount::stopParkTimer);

    // Setup Debounce timer to limit over-activation of settings changes
    m_DebounceTimer.setInterval(500);
    m_DebounceTimer.setSingleShot(true);
    connect(&m_DebounceTimer, &QTimer::timeout, this, &Mount::settleSettings);

    stopTimerB->setEnabled(false);

    if (parkEveryDay->isChecked())
        startTimerB->animateClick();

    m_ControlPanel.reset(new MountControlPanel());
    connect(m_ControlPanel.get(), &MountControlPanel::newMotionCommand, this, &Mount::motionCommand);
    connect(m_ControlPanel.get(), &MountControlPanel::aborted, this, &Mount::abort);
    connect(m_ControlPanel.get(), &MountControlPanel::newSlewRate, this, &Mount::setSlewRate);
    connect(m_ControlPanel.get(), &MountControlPanel::slew, this, &Mount::slew);
    connect(m_ControlPanel.get(), &MountControlPanel::sync, this, &Mount::sync);
    connect(m_ControlPanel.get(), &MountControlPanel::park, this, &Mount::park);
    connect(m_ControlPanel.get(), &MountControlPanel::unpark, this, &Mount::unpark);
    connect(m_ControlPanel.get(), &MountControlPanel::updownReversed, this, &Mount::setUpDownReversed);
    connect(m_ControlPanel.get(), &MountControlPanel::leftrightReversed, this, &Mount::setLeftRightReversed);
    connect(m_ControlPanel.get(), &MountControlPanel::center, this, &Mount::centerMount);

    connect(mountMotion, &MountMotionWidget::newMotionCommand, this, &Mount::motionCommand);
    connect(mountMotion, &MountMotionWidget::newSlewRate, this, &Mount::setSlewRate);
    connect(mountMotion, &MountMotionWidget::aborted, this, &Mount::abort);
    connect(mountMotion, &MountMotionWidget::updownReversed, this, &Mount::setUpDownReversed);
    connect(mountMotion, &MountMotionWidget::leftrightReversed, this, &Mount::setLeftRightReversed);

    // forward J2000 selection to the target widget, which does not have its own selection
    connect(mountPosition, &MountPositionWidget::J2000Enabled, mountTarget, &MountTargetWidget::setJ2000Enabled);

    // forward target commands
    connect(mountTarget, &MountTargetWidget::slew, this, &Mount::slew);
    connect(mountTarget, &MountTargetWidget::sync, this, &Mount::sync);

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
                parkingLabel->setText("Parked");
                break;
            case ISD::PARK_PARKING:
                parkingLabel->setText("Parking");
                break;
            case ISD::PARK_UNPARKING:
                parkingLabel->setText("Unparking");
                break;
            case ISD::PARK_UNPARKED:
                parkingLabel->setText("Unparked");
                break;
            case ISD::PARK_ERROR:
                parkingLabel->setText("Park Error");
                break;
            case ISD::PARK_UNKNOWN:
                parkingLabel->setText("Park Status Unknown");
                break;
        }
        parkB->setEnabled(m_Mount->parkStatus() == ISD::PARK_UNPARKED);
        unparkB->setEnabled(m_Mount->parkStatus() == ISD::PARK_PARKED);
    }
    else
    {
        parkB->setEnabled(false);
        unparkB->setEnabled(false);
        parkingLabel->setText("");
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
        m_Mount->disconnect(m_Mount, nullptr, this, nullptr);

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
    mf_state->setMountConnected(device != nullptr);

    connect(m_Mount, &ISD::Mount::propertyUpdated, this, &Mount::updateProperty);
    connect(m_Mount, &ISD::Mount::newTarget, this, &Mount::newTarget);
    connect(m_Mount, &ISD::Mount::newTargetName, this, &Mount::newTargetName);
    connect(m_Mount, &ISD::Mount::newCoords, this, &Mount::newCoords);
    connect(m_Mount, &ISD::Mount::newCoords, this, &Mount::updateTelescopeCoords);
    connect(m_Mount, &ISD::Mount::slewRateChanged, this, &Mount::slewRateChanged);
    connect(m_Mount, &ISD::Mount::pierSideChanged, this, &Mount::pierSideChanged);
    connect(m_Mount, &ISD::Mount::axisReversed, this, &Mount::syncAxisReversed);
    connect(m_Mount, &ISD::Mount::Disconnected, this, [this]()
    {
        m_ControlPanel->hide();
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
            m_Mount->setAltLimits(minimumAltLimit->value(), maximumAltLimit->value(), enableAltitudeLimitsTrackingOnly->isChecked());
        else
            m_Mount->setAltLimits(-91, +91, false);

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
                m_Mount->setAltLimits(minimumAltLimit->value(), maximumAltLimit->value(), enableAltitudeLimitsTrackingOnly->isChecked());
            else
                m_Mount->setAltLimits(-91, +91, false);

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

bool Mount::addTimeSource(const QSharedPointer<ISD::GenericDevice> &device)
{
    // No duplicates
    for (auto &oneSource : m_TimeSources)
    {
        if (oneSource->getDeviceName() == device->getDeviceName())
            return false;
    }

    m_TimeSources.append(device);

    timeSource->blockSignals(true);
    timeSource->clear();
    timeSource->addItem("KStars");

    m_TimeSourcesList.clear();
    m_TimeSourcesList.append("KStars");

    for (auto &oneSource : m_TimeSources)
    {
        auto name = oneSource->getDeviceName();

        m_TimeSourcesList.append(name);
        timeSource->addItem(name);

        if (name == Options::timeSource())
        {
            // If GPS, then refresh
            auto refreshGPS = oneSource->getProperty("GPS_REFRESH");
            if (refreshGPS)
            {
                auto sw = refreshGPS.getSwitch();
                sw->at(0)->setState(ISS_ON);
                oneSource->sendNewProperty(refreshGPS);
            }
        }
    }

    timeSource->setCurrentText(Options::timeSource());
    timeSource->blockSignals(false);
    return true;
}

bool Mount::addLocationSource(const QSharedPointer<ISD::GenericDevice> &device)
{
    // No duplicates
    for (auto &oneSource : m_LocationSources)
    {
        if (oneSource->getDeviceName() == device->getDeviceName())
            return false;
    }

    m_LocationSources.append(device);
    locationSource->blockSignals(true);
    locationSource->clear();
    locationSource->addItem("KStars");

    m_LocationSourcesList.clear();
    m_LocationSourcesList.append("KStars");

    for (auto &oneSource : m_LocationSources)
    {
        auto name = oneSource->getDeviceName();
        locationSource->addItem(name);
        m_LocationSourcesList.append(name);

        if (name == Options::locationSource())
        {
            auto refreshGPS = oneSource->getProperty("GPS_REFRESH");
            if (refreshGPS)
            {
                auto sw = refreshGPS.getSwitch();
                sw->at(0)->setState(ISS_ON);
                oneSource->sendNewProperty(refreshGPS);
            }
        }
    }

    locationSource->setCurrentText(Options::locationSource());
    locationSource->blockSignals(false);
    return true;
}

void Mount::removeDevice(const QSharedPointer<ISD::GenericDevice> &device)
{
    if (m_Mount && m_Mount->getDeviceName() == device->getDeviceName())
    {
        m_Mount->disconnect(this);
        m_ControlPanel->hide();
        qCDebug(KSTARS_EKOS_MOUNT) << "Removing mount driver" << m_Mount->getDeviceName();
        m_Mount = nullptr;
    }

    m_TimeSources.erase(std::remove_if(m_TimeSources.begin(), m_TimeSources.end(), [device](const auto & oneSource)
    {
        return device->getDeviceName() == oneSource->getDeviceName();
    }), m_TimeSources.end());

    m_LocationSources.erase(std::remove_if(m_LocationSources.begin(), m_LocationSources.end(), [device](const auto & oneSource)
    {
        return device->getDeviceName() == oneSource->getDeviceName();
    }), m_LocationSources.end());
}

void Mount::syncTelescopeInfo()
{
    if (!m_Mount || m_Mount->isConnected() == false)
        return;

    auto svp = m_Mount->getSwitch("TELESCOPE_SLEW_RATE");
    // sync speed info on the UI and tht separate mount control
    mountMotion->syncSpeedInfo(svp);
    m_ControlPanel->mountMotion->syncSpeedInfo(svp);

    if (m_Mount->canPark())
    {
        connect(parkB, &QPushButton::clicked, m_Mount, &ISD::Mount::park, Qt::UniqueConnection);
        connect(unparkB, &QPushButton::clicked, m_Mount, &ISD::Mount::unpark, Qt::UniqueConnection);

        m_ControlPanel->parkButtonObject->setEnabled(!m_Mount->isParked());
        m_ControlPanel->unparkButtonObject->setEnabled(m_Mount->isParked());
    }
    else
    {
        disconnect(parkB, &QPushButton::clicked, m_Mount, &ISD::Mount::park);
        disconnect(unparkB, &QPushButton::clicked, m_Mount, &ISD::Mount::unpark);

        m_ControlPanel->parkButtonObject->setEnabled(false);
        m_ControlPanel->unparkButtonObject->setEnabled(false);
    }

    m_ControlPanel->setProperty("isJ2000", m_Mount->isJ2000());
    setupParkUI();

    // Tracking State
    svp = m_Mount->getSwitch("TELESCOPE_TRACK_STATE");
    if (svp)
    {
        trackingLabel->setEnabled(true);
        trackOnB->setEnabled(true);
        trackOffB->setEnabled(true);
        trackOnB->disconnect();
        trackOffB->disconnect();
        connect(trackOnB, &QPushButton::clicked, this, [&]()
        {
            m_Mount->setTrackEnabled(true);
        });
        connect(trackOffB, &QPushButton::clicked, this, [&]()
        {
#if KIO_VERSION >= QT_VERSION_CHECK(5, 100, 0)
            if (KMessageBox::questionTwoActions(KStars::Instance(),
                                                i18n("Are you sure you want to turn off mount tracking?"),
                                                i18n("Mount Tracking"),
                                                KGuiItem(i18nc("@action:button", "Yes")),
                                                KGuiItem(i18nc("@action:button", "No")),
                                                "turn_off_mount_tracking_dialog") == KMessageBox::ButtonCode::PrimaryAction)
                m_Mount->setTrackEnabled(false);
#else
            if (KMessageBox::questionYesNo(KStars::Instance(),
                                           i18n("Are you sure you want to turn off mount tracking?"),
                                           i18n("Mount Tracking"),
                                           KStandardGuiItem::yes(),
                                           KStandardGuiItem::no(),
                                           "turn_off_mount_tracking_dialog") == KMessageBox::Yes)
                m_Mount->setTrackEnabled(false);
#endif
        });
    }
    else
    {
        trackingLabel->setEnabled(false);
        trackOnB->setChecked(false);
        trackOnB->setEnabled(false);
        trackOffB->setChecked(false);
        trackOffB->setEnabled(false);
    }

    m_ControlPanel->mountMotion->leftRightCheckObject->setProperty("checked", m_Mount->isReversed(AXIS_RA));
    m_ControlPanel->mountMotion->upDownCheckObject->setProperty("checked", m_Mount->isReversed(AXIS_DE));
}

void Mount::registerNewModule(const QString &name)
{
    if (name == "Capture")
    {
        hasCaptureInterface = true;
        mf_state->setHasCaptureInterface(true);
    }
}

void Mount::updateTelescopeCoords(const SkyPoint &position, ISD::Mount::PierSide pierSide, const dms &ha)
{
    if (m_Mount == nullptr || !m_Mount->isConnected())
        return;

    telescopeCoord = position;

    // forward the position to the position widget and control panel
    mountPosition->updateTelescopeCoords(position, ha);
    m_ControlPanel->mountPosition->updateTelescopeCoords(position, ha);

    // No need to update coords if we are still parked.
    if (m_Status == ISD::Mount::MOUNT_PARKED && m_Status == m_Mount->status())
        return;

    double currentAlt = telescopeCoord.altRefracted().Degrees();

    if (minimumAltLimit->isEnabled() && (currentAlt < minimumAltLimit->value() || currentAlt > maximumAltLimit->value()) &&
            (m_Mount->isTracking() || (!enableAltitudeLimitsTrackingOnly->isChecked() && m_Mount->isInMotion())))
    {
        if (currentAlt < minimumAltLimit->value())
        {
            // Only stop if current altitude is less than last altitude indicate worse situation
            if (currentAlt < m_LastAltitude && m_AbortAltDispatch == -1)
            {
                appendLogText(i18n("Telescope altitude is below minimum altitude limit of %1. Aborting motion...",
                                   QString::number(minimumAltLimit->value(), 'g', 3)));
                m_Mount->abort();
                m_Mount->setTrackEnabled(false);
                //KNotification::event( QLatin1String( "OperationFailed" ));
                KNotification::beep();
                m_AbortAltDispatch++;
            }
        }
        else if (currentAlt > m_LastAltitude && m_AbortAltDispatch == -1)
        {
            // Only stop if current altitude is higher than last altitude indicate worse situation
            appendLogText(i18n("Telescope altitude is above maximum altitude limit of %1. Aborting motion...",
                               QString::number(maximumAltLimit->value(), 'g', 3)));
            m_Mount->abort();
            m_Mount->setTrackEnabled(false);
            //KNotification::event( QLatin1String( "OperationFailed" ));
            KNotification::beep();
            m_AbortAltDispatch++;
        }
    }
    else
        m_AbortAltDispatch = -1;

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
                                   " " << ISD::Mount::pierSideStateString(m_Mount->pierSide()) <<
                                   " haLimitReached " << (haLimitReached ? "true" : "false") <<
                                   " lastHa " << m_LastHourAngle;

        // compare with last ha to avoid multiple calls
        if (haLimitReached && (rangeHA(haHours - m_LastHourAngle) >= 0 ) &&
                (m_AbortHADispatch == -1 ||
                 m_Mount->isInMotion()))
        {
            // moved past the limit, so stop
            appendLogText(i18n("Telescope hour angle is more than the maximum hour angle of %1. Aborting motion...",
                               QString::number(maximumHaLimit->value(), 'g', 3)));
            m_Mount->abort();
            m_Mount->setTrackEnabled(false);
            //KNotification::event( QLatin1String( "OperationFailed" ));
            KNotification::beep();
            m_AbortHADispatch++;
            // ideally we pause and wait until we have passed the pier flip limit,
            // then do a pier flip and try to resume
            // this will need changing to use a target position because the current HA has stopped.
        }
    }
    else
        m_AbortHADispatch = -1;

    m_LastAltitude = currentAlt;
    m_LastHourAngle = haHours;

    ISD::Mount::Status currentStatus = m_Mount->status();
    if (m_Status != currentStatus)
    {
        qCDebug(KSTARS_EKOS_MOUNT) << "Mount status changed from " << m_Mount->statusString(m_Status)
                                   << " to " << m_Mount->statusString(currentStatus);

        //setScopeStatus(currentStatus);

        m_ControlPanel->statusTextObject->setProperty("text", m_Mount->statusString(currentStatus));
        m_Status = currentStatus;
        // forward
        emit newStatus(m_Status);

        setupParkUI();
        m_ControlPanel->parkButtonObject->setEnabled(!m_Mount->isParked());
        m_ControlPanel->unparkButtonObject->setEnabled(m_Mount->isParked());

        QAction *a = KStars::Instance()->actionCollection()->action("telescope_track");
        if (a != nullptr)
            a->setChecked(currentStatus == ISD::Mount::MOUNT_TRACKING);
    }

    bool isTracking = (currentStatus == ISD::Mount::MOUNT_TRACKING);
    if (trackOnB->isEnabled())
    {
        trackOnB->setChecked(isTracking);
        trackOffB->setChecked(!isTracking);
    }

    // handle pier side display
    pierSideLabel->setText(ISD::Mount::pierSideStateString(m_Mount->pierSide()));

    // Auto Park Timer
    if (autoParkTimer.isActive())
    {
        QTime remainingTime(0, 0, 0);
        remainingTime = remainingTime.addMSecs(autoParkTimer.remainingTime());
        countdownLabel->setText(remainingTime.toString("hh:mm:ss"));
        emit autoParkCountdownUpdated(countdownLabel->text());
    }
}

void Mount::updateProperty(INDI::Property prop)
{
    if (prop.isNameMatch("EQUATORIAL_EOD_COORD") || prop.isNameMatch("EQUATORIAL_COORD"))
    {
        auto nvp = prop.getNumber();

        // if the meridian flip state machine is not initialized, return
        if (getMeridianFlipState().isNull())
            return;

        switch (getMeridianFlipState()->getMeridianFlipStage())
        {
            case MeridianFlipState::MF_INITIATED:
                if (nvp->s == IPS_BUSY && m_Mount != nullptr && m_Mount->isSlewing())
                    getMeridianFlipState()->updateMeridianFlipStage(MeridianFlipState::MF_FLIPPING);
                break;

            default:
                break;
        }
    }
    else if (prop.isNameMatch("TELESCOPE_SLEW_RATE"))
    {
        auto svp = prop.getSwitch();
        mountMotion->updateSpeedInfo(svp);
        m_ControlPanel->mountMotion->updateSpeedInfo(svp);
    }
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
}

void Mount::paaStageChanged(int stage)
{
    // Clear the current target position is necessary due to a bug in some mount drivers
    // which report a mount slew instead of a mount motion. For these mounts, ending a slew
    // leads to setting the current target position, which is necessary for meridian flips
    // Since we want to avoid meridian flips during and after finishing PAA, it needs to
    // be set to nullptr.

    if (stage != PolarAlignmentAssistant::PAH_IDLE)
        mf_state->clearTargetPosition();

    switch (stage)
    {
        // deactivate the meridian flip when the first capture is taken
        case PolarAlignmentAssistant::PAH_FIRST_CAPTURE:
        case PolarAlignmentAssistant::PAH_FIRST_SOLVE:
            if (mf_state->isEnabled())
            {
                appendLogText(i18n("Meridian flip set inactive during polar alignment."));
                mf_state->setEnabled(false);
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
            if (executeMeridianFlip->isChecked() && mf_state->isEnabled() == false)
            {
                appendLogText(i18n("Polar alignment motions finished, meridian flip activated."));
                mf_state->setEnabled(executeMeridianFlip->isChecked());
            }
            break;
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

    m_Mount->setAltLimits(minimumAltLimit->value(), maximumAltLimit->value(), enableAltitudeLimitsTrackingOnly->isChecked());
}

void Mount::setAltitudeLimits(bool enable)
{
    minAltLabel->setEnabled(enable);
    maxAltLabel->setEnabled(enable);

    minimumAltLimit->setEnabled(enable);
    maximumAltLimit->setEnabled(enable);

    enableAltitudeLimitsTrackingOnly->setEnabled(enable);

    if (m_Mount)
    {
        if (enable)
            m_Mount->setAltLimits(minimumAltLimit->value(), maximumAltLimit->value(), enableAltitudeLimitsTrackingOnly->isChecked());
        else
            m_Mount->setAltLimits(-91, +91, false);
    }
}

// Used for meridian flip
void Mount::resumeAltLimits()
{
    //Only enable if it was already enabled before and the MinimumAltLimit is currently disabled.
    if (m_AltitudeLimitEnabled && minimumAltLimit->isEnabled() == false)
        setAltitudeLimits(true);
}

// Used for meridian flip
void Mount::suspendAltLimits()
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
    m_ControlPanel->setJ2000Enabled(enabled);
    mountPosition->setJ2000Enabled(enabled);
}

bool Mount::gotoTarget(const QString &target)
{
    auto object = KStarsData::Instance()->skyComposite()->findByName(target, false);

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

void Mount::setTargetName(const QString &name)
{
    mountTarget->setTargetName(name);
    m_ControlPanel->setTargetName(name);
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

bool Mount::slew(double RA, double DEC)
{
    if (m_Mount == nullptr || m_Mount->isConnected() == false)
        return false;

    // calculate the new target
    targetPosition = new SkyPoint(RA, DEC);
    SkyPoint J2000Coord(targetPosition->ra(), targetPosition->dec());
    J2000Coord.catalogueCoord(KStarsData::Instance()->ut().djd());
    targetPosition->setRA0(J2000Coord.ra());
    targetPosition->setDec0(J2000Coord.dec());

    mf_state->setTargetPosition(targetPosition);
    mf_state->resetMeridianFlip();

    m_ControlPanel->setTargetPosition(new SkyPoint(RA, DEC));
    mountTarget->setTargetPosition(new SkyPoint(RA, DEC));

    qCDebug(KSTARS_EKOS_MOUNT) << "Slewing to RA=" <<
                               targetPosition->ra().toHMSString() <<
                               "DEC=" << targetPosition->dec().toDMSString();
    qCDebug(KSTARS_EKOS_MOUNT) << "Initial HA " << initialHA() << ", flipDelayHrs " << mf_state->getFlipDelayHrs() <<
                               "MFStatus " << MeridianFlipState::meridianFlipStatusString(mf_state->getMeridianFlipMountState());

    // start the slew
    return(m_Mount->Slew(targetPosition));
}


SkyPoint Mount::currentTarget()
{
    if (targetPosition != nullptr)
        return *targetPosition;

    qCWarning(KSTARS_EKOS_MOUNT) << "No target position defined!";
    // since we need to answer something, we take the current mount position
    return telescopeCoord;
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
    if (m_ControlPanel->isVisible())
    {
        m_ControlPanel->hide();
        QAction *a = KStars::Instance()->actionCollection()->action("show_mount_box");
        if (a)
            a->setChecked(false);
    }
    else
    {
        m_ControlPanel->show();
        QAction *a = KStars::Instance()->actionCollection()->action("show_mount_box");
        if (a)
            a->setChecked(true);
    }
}

//++++ converters for target coordinate display in Mount Control box

bool Mount::raDecToAzAlt(QString qsRA, QString qsDec)
{
    return m_ControlPanel->mountTarget->raDecToAzAlt(qsRA, qsDec);
}

bool  Mount::raDecToHaDec(QString qsRA)
{
    return m_ControlPanel->mountTarget->raDecToHaDec(qsRA);
}

bool  Mount::azAltToRaDec(QString qsAz, QString qsAlt)
{
    return m_ControlPanel->mountTarget->azAltToRaDec(qsAz, qsAlt);
}

bool  Mount::azAltToHaDec(QString qsAz, QString qsAlt)
{
    return m_ControlPanel->mountTarget->azAltToHaDec(qsAz, qsAlt);
}

bool  Mount::haDecToRaDec(QString qsHA)
{
    return m_ControlPanel->mountTarget->haDecToRaDec(qsHA);
}

bool  Mount::haDecToAzAlt(QString qsHA, QString qsDec)
{
    return m_ControlPanel->mountTarget->haDecToAzAlt(qsHA, qsDec);
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
        m_ControlPanel->statusTextObject->setProperty("text", m_Mount->statusString(status));
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
    autoParkTime->setTime(startup);
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

    auto parkTime = autoParkTime->time();

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
    emit autoParkCountdownUpdated("00:00:00");
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
    emit autoParkCountdownUpdated("00:00:00");
    if (m_Mount)
    {
        if (m_Mount->isParked() == false)
        {
            appendLogText(i18n("Starting auto park..."));
            park();
        }
    }
}

void Mount::syncAxisReversed(INDI_EQ_AXIS axis, bool reversed)
{
    if (axis == AXIS_RA)
        m_ControlPanel->mountMotion->leftRightCheckObject->setProperty("checked", reversed);
    else
        m_ControlPanel->mountMotion->upDownCheckObject->setProperty("checked", reversed);
}

void Mount::setupOpticalTrainManager()
{
    connect(OpticalTrainManager::Instance(), &OpticalTrainManager::updated, this, &Mount::refreshOpticalTrain);
    connect(trainB, &QPushButton::clicked, this, [this]()
    {
        OpticalTrainManager::Instance()->openEditor(opticalTrainCombo->currentText());
    });
    connect(opticalTrainCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index)
    {
        ProfileSettings::Instance()->setOneSetting(ProfileSettings::MountOpticalTrain,
                OpticalTrainManager::Instance()->id(opticalTrainCombo->itemText(index)));
        refreshOpticalTrain();
        emit trainChanged();
    });
}

void Mount::refreshOpticalTrain()
{
    opticalTrainCombo->blockSignals(true);
    opticalTrainCombo->clear();
    opticalTrainCombo->addItems(OpticalTrainManager::Instance()->getTrainNames());
    trainB->setEnabled(true);

    QVariant trainID = ProfileSettings::Instance()->getOneSetting(ProfileSettings::MountOpticalTrain);

    if (trainID.isValid())
    {
        auto id = trainID.toUInt();

        // If train not found, select the first one available.
        if (OpticalTrainManager::Instance()->exists(id) == false)
        {
            qCWarning(KSTARS_EKOS_MOUNT) << "Optical train doesn't exist for id" << id;
            id = OpticalTrainManager::Instance()->id(opticalTrainCombo->itemText(0));
        }

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
        {
            auto map = settings.toJsonObject().toVariantMap();
            if (map != m_Settings)
            {
                m_Settings.clear();
                setAllSettings(map);
            }
        }
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

        // timeEdit
        auto timeEdit = findChild<QTimeEdit*>(name);
        if (timeEdit)
        {
            syncControl(settings, name, timeEdit);
            continue;
        }
    }

    // Sync to options
    for (auto &key : settings.keys())
    {
        auto value = settings[key];
        // Save immediately
        Options::self()->setProperty(key.toLatin1(), value);
        Options::self()->save();

        m_Settings[key] = value;
        m_GlobalSettings[key] = value;
    }

    emit settingsUpdated(getAllSettings());

    // Save to optical train specific settings as well
    OpticalTrainSettings::Instance()->setOpticalTrainID(OpticalTrainManager::Instance()->id(opticalTrainCombo->currentText()));
    OpticalTrainSettings::Instance()->setOneSetting(OpticalTrainSettings::Mount, m_Settings);

    // Restablish connections
    connectSyncSettings();
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
    QRadioButton *pRadioButton = nullptr;
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
        if (value != pCB->isChecked())
            pCB->click();
        return true;
    }
    else if ((pRadioButton = qobject_cast<QRadioButton *>(widget)))
    {
        const bool value = settings[key].toBool();
        if (value)
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
    QRadioButton *rb = nullptr;

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
    else if ( (rb = qobject_cast<QRadioButton*>(sender())))
    {
        key = rb->objectName();
        if (rb->isChecked() == false)
        {
            m_Settings.remove(key);
            return;
        }
        value = true;
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
    m_Settings[key] = value;
    m_GlobalSettings[key] = value;

    m_DebounceTimer.start();
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Mount::settleSettings()
{
    Options::self()->save();
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
        if (value.isValid() && oneWidget->count() > 0)
        {
            oneWidget->setCurrentText(value.toString());
            settings[key] = value;
        }
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
    }

    // initialize meridian flip state machine values
    mf_state->setEnabled(Options::executeMeridianFlip());
    mf_state->setOffset(Options::meridianFlipOffsetDegrees());

    m_GlobalSettings = m_Settings = settings;
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Mount::connectSyncSettings()
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

    // All Radio buttons
    for (auto &oneWidget : findChildren<QRadioButton*>())
        connect(oneWidget, &QCheckBox::toggled, this, &Ekos::Mount::syncSettings);

    // All QDateTimeEdit
    for (auto &oneWidget : findChildren<QDateTimeEdit*>())
        connect(oneWidget, &QDateTimeEdit::editingFinished, this, &Ekos::Mount::syncSettings);
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Mount::disconnectSyncSettings()
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

    // All Radio buttons
    for (auto &oneWidget : findChildren<QRadioButton*>())
        disconnect(oneWidget, &QCheckBox::toggled, this, &Ekos::Mount::syncSettings);

    for (auto &oneWidget : findChildren<QDateTimeEdit*>())
        disconnect(oneWidget, &QDateTimeEdit::editingFinished, this, &Ekos::Mount::syncSettings);
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Mount::connectSettings()
{
    connectSyncSettings();

    // connections to the meridian flip state machine
    connect(executeMeridianFlip, &QCheckBox::toggled, mf_state.get(), &MeridianFlipState::setEnabled);
    connect(meridianFlipOffsetDegrees, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
            mf_state.get(), &MeridianFlipState::setOffset);
    connect(this, &Mount::newParkStatus, mf_state.get(), &MeridianFlipState::setMountParkStatus);
    connect(mf_state.get(), &MeridianFlipState::slewTelescope, [&](SkyPoint pos)
    {
        if (m_Mount)
            m_Mount->Slew(&pos, (m_Mount->canFlip() && Options::forcedFlip()));
    });

    // Train combo box should NOT be synced.
    disconnect(opticalTrainCombo, QOverload<int>::of(&QComboBox::activated), this, &Ekos::Mount::syncSettings);
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Mount::disconnectSettings()
{
    disconnectSyncSettings();

    // cut connections to the meridian flip state machine
    disconnect(executeMeridianFlip, &QCheckBox::toggled, mf_state.get(), &MeridianFlipState::setEnabled);
    disconnect(meridianFlipOffsetDegrees, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
               mf_state.get(), &MeridianFlipState::setOffset);
    disconnect(this, &Mount::newParkStatus, mf_state.get(), &MeridianFlipState::setMountParkStatus);
    disconnect(mf_state.get(), &MeridianFlipState::slewTelescope, nullptr, nullptr);
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
double Mount::initialHA()
{
    return mf_state->initialPositionHA();
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Mount::syncTimeSource()
{
    appendLogText(i18n("Updating master time source to %1.", Options::locationSource()));
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Mount::syncLocationSource()
{
    auto name = Options::locationSource();
    auto it = std::find_if(m_LocationSources.begin(), m_LocationSources.end(), [name](const auto & oneSource)
    {
        return oneSource->getDeviceName() == name;
    });
    if (it != m_LocationSources.end())
    {
        auto property = (*it)->getProperty("GPS_REFRESH");
        if (property)
        {
            auto sw = property.getSwitch();
            sw->at(0)->setState(ISS_ON);
            (*it)->sendNewProperty(property);
            appendLogText(i18n("Updating master location source to %1. Updating GPS...", name));
        }
        else
        {
            property = (*it)->getProperty("GEOGRAPHIC_COORD");
            if (property)
            {
                (*it)->processNumber(property);
                appendLogText(i18n("Updating master location source to %1.", name));
            }
        }
    }
}
}
