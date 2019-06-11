/*  Ekos Mount Module
    Copyright (C) 2015 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include "mount.h"

#include <QQuickView>
#include <QQuickItem>

#include <KNotifications/KNotification>
#include <KLocalizedContext>
#include <KActionCollection>

#include "Options.h"

#include "indi/driverinfo.h"
#include "indi/indicommon.h"
#include "indi/clientmanager.h"
#include "indi/indifilter.h"


#include "mountadaptor.h"

#include "ekos/manager.h"

#include "kstars.h"
#include "skymapcomposite.h"
#include "kspaths.h"
#include "dialogs/finddialog.h"
#include "kstarsdata.h"
#include "ksutils.h"

#include <basedevice.h>

#include <ekos_mount_debug.h>

extern const char *libindi_strings_context;

#define UPDATE_DELAY         1000
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
    QDBusConnection::sessionBus().connect("org.kde.kstars", "/KStars/Ekos", "org.kde.kstars.Ekos", "newModule", this, SLOT(registerNewModule(QString)));
    //connect(ekosInterface, SIGNAL(newModule(QString)), this, SLOT(registerNewModule(QString)));

    currentTelescope = nullptr;

    abortDispatch = -1;

    minAltLimit->setValue(Options::minimumAltLimit());
    maxAltLimit->setValue(Options::maximumAltLimit());

    connect(minAltLimit, SIGNAL(editingFinished()), this, SLOT(saveLimits()));
    connect(maxAltLimit, SIGNAL(editingFinished()), this, SLOT(saveLimits()));

    connect(mountToolBoxB, SIGNAL(clicked()), this, SLOT(toggleMountToolBox()));

    connect(saveB, SIGNAL(clicked()), this, SLOT(save()));

    connect(clearAlignmentModelB, &QPushButton::clicked, this, [this]()
    {
        resetModel();
    });

    connect(enableLimitsCheck, SIGNAL(toggled(bool)), this, SLOT(enableAltitudeLimits(bool)));
    enableLimitsCheck->setChecked(Options::enableAltitudeLimits());
    altLimitEnabled = enableLimitsCheck->isChecked();

    // meridian flip
    meridianFlipCheckBox->setChecked(Options::executeMeridianFlip());
    meridianFlipTimeBox->setValue(Options::meridianFlipOffset());
    connect(meridianFlipCheckBox, &QCheckBox::toggled, this, &Ekos::Mount::meridianFlipSetupChanged);
    connect(meridianFlipTimeBox, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), this, &Ekos::Mount::meridianFlipSetupChanged);


    updateTimer.setInterval(UPDATE_DELAY);
    connect(&updateTimer, SIGNAL(timeout()), this, SLOT(updateTelescopeCoords()));

    QDateTime now = KStarsData::Instance()->lt();
    // Set seconds to zero
    now = now.addSecs(now.time().second() * -1);
    startupTimeEdit->setDateTime(now);

    connect(&autoParkTimer, &QTimer::timeout, this, &Mount::startAutoPark);
    connect(startTimerB, &QPushButton::clicked, this, &Mount::startParkTimer);
    connect(stopTimerB, &QPushButton::clicked, this, &Mount::stopParkTimer);

    stopTimerB->setEnabled(false);

    // QML Stuff
    m_BaseView = new QQuickView();

#if 0
    QString MountBox_Location;
#if defined(Q_OS_OSX)
    MountBox_Location = QCoreApplication::applicationDirPath() + "/../Resources/data/ekos/mount/qml/mountbox.qml";
    if (!QFileInfo(MountBox_Location).exists())
        MountBox_Location = KSPaths::locate(QStandardPaths::AppDataLocation, "ekos/mount/qml/mountbox.qml");
#elif defined(Q_OS_WIN)
    MountBox_Location = KSPaths::locate(QStandardPaths::GenericDataLocation, "ekos/mount/qml/mountbox.qml");
#else
    MountBox_Location = KSPaths::locate(QStandardPaths::AppDataLocation, "ekos/mount/qml/mountbox.qml");
#endif

    m_BaseView->setSource(QUrl::fromLocalFile(MountBox_Location));
#endif

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

    m_BaseView->setMinimumSize(QSize(210, 540));
    m_BaseView->setResizeMode(QQuickView::SizeRootObjectToView);

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
    m_equatorialCheck   = m_BaseObj->findChild<QQuickItem *>("equatorialCheckObject");
    m_horizontalCheck    = m_BaseObj->findChild<QQuickItem *>("horizontalCheckObject");

    //Note:  This is to prevent a button from being called the default button
    //and then executing when the user hits the enter key such as when on a Text Box
    QList<QPushButton *> qButtons = findChildren<QPushButton *>();
    for (auto &button : qButtons)
        button->setAutoDefault(false);
}

Mount::~Mount()
{
    delete(m_Ctxt);
    delete(m_BaseObj);
}

void Mount::setTelescope(ISD::GDInterface *newTelescope)
{
    if (newTelescope == currentTelescope)
    {
        updateTimer.start();
        if (enableLimitsCheck->isChecked())
            currentTelescope->setAltLimits(minAltLimit->value(), maxAltLimit->value());
        syncTelescopeInfo();
        return;
    }

    if (currentGPS != nullptr)
        syncGPS();

    currentTelescope = static_cast<ISD::Telescope *>(newTelescope);

    currentTelescope->disconnect(this);

    connect(currentTelescope, &ISD::GDInterface::numberUpdated, this, &Mount::updateNumber);
    connect(currentTelescope, &ISD::GDInterface::switchUpdated, this, &Mount::updateSwitch);
    connect(currentTelescope, &ISD::GDInterface::textUpdated, this, &Mount::updateText);
    connect(currentTelescope, &ISD::Telescope::newTarget, this, &Mount::newTarget);
    connect(currentTelescope, &ISD::Telescope::slewRateChanged, this, &Mount::slewRateChanged);
    connect(currentTelescope, &ISD::Telescope::pierSideChanged, this, &Mount::pierSideChanged);
    connect(currentTelescope, &ISD::Telescope::Disconnected, [this]()
    {
        updateTimer.stop();
        m_BaseView->hide();
    });
    connect(currentTelescope, &ISD::Telescope::newParkStatus, [&](ISD::ParkStatus status)
    {
        m_ParkStatus = status;
        emit newParkStatus(status);
    });
    connect(currentTelescope, &ISD::Telescope::ready, this, &Mount::ready);

    //Disable this for now since ALL INDI drivers now log their messages to verbose output
    //connect(currentTelescope, SIGNAL(messageUpdated(int)), this, SLOT(updateLog(int)), Qt::UniqueConnection);

    if (enableLimitsCheck->isChecked())
        currentTelescope->setAltLimits(minAltLimit->value(), maxAltLimit->value());

    updateTimer.start();

    syncTelescopeInfo();

    // Send initial status
    m_Status = currentTelescope->status();
    emit newStatus(m_Status);

    m_ParkStatus = currentTelescope->parkStatus();
    emit newParkStatus(m_ParkStatus);
}

void Mount::syncTelescopeInfo()
{
    if (currentTelescope == nullptr || currentTelescope->isConnected() == false)
        return;

    INumberVectorProperty *nvp = currentTelescope->getBaseDevice()->getNumber("TELESCOPE_INFO");

    if (nvp)
    {
        primaryScopeGroup->setTitle(currentTelescope->getDeviceName());
        guideScopeGroup->setTitle(i18n("%1 guide scope", currentTelescope->getDeviceName()));

        INumber *np = IUFindNumber(nvp, "TELESCOPE_APERTURE");

        if (np && np->value > 0)
            primaryScopeApertureIN->setValue(np->value);

        np = IUFindNumber(nvp, "TELESCOPE_FOCAL_LENGTH");
        if (np && np->value > 0)
            primaryScopeFocalIN->setValue(np->value);

        np = IUFindNumber(nvp, "GUIDER_APERTURE");
        if (np && np->value > 0)
            guideScopeApertureIN->setValue(np->value);

        np = IUFindNumber(nvp, "GUIDER_FOCAL_LENGTH");
        if (np && np->value > 0)
            guideScopeFocalIN->setValue(np->value);
    }

    ISwitchVectorProperty *svp = currentTelescope->getBaseDevice()->getSwitch("TELESCOPE_SLEW_RATE");

    if (svp)
    {
        int index = IUFindOnSwitchIndex(svp);

        // QtQuick
        m_SpeedSlider->setEnabled(true);
        m_SpeedSlider->setProperty("maximumValue", svp->nsp - 1);
        m_SpeedSlider->setProperty("value", index);

        m_SpeedLabel->setProperty("text", i18nc(libindi_strings_context, svp->sp[index].label));
        m_SpeedLabel->setEnabled(true);
    }
    else
    {
        // QtQuick
        m_SpeedSlider->setEnabled(false);
        m_SpeedLabel->setEnabled(false);
    }

    if (currentTelescope->canPark())
    {
        parkB->setEnabled(!currentTelescope->isParked());
        unparkB->setEnabled(currentTelescope->isParked());
        connect(parkB, SIGNAL(clicked()), currentTelescope, SLOT(Park()), Qt::UniqueConnection);
        connect(unparkB, SIGNAL(clicked()), currentTelescope, SLOT(UnPark()), Qt::UniqueConnection);

        // QtQuick
        m_Park->setEnabled(!currentTelescope->isParked());
        m_Unpark->setEnabled(currentTelescope->isParked());
    }
    else
    {
        parkB->setEnabled(false);
        unparkB->setEnabled(false);
        disconnect(parkB, SIGNAL(clicked()), currentTelescope, SLOT(Park()));
        disconnect(unparkB, SIGNAL(clicked()), currentTelescope, SLOT(UnPark()));

        // QtQuick
        m_Park->setEnabled(false);
        m_Unpark->setEnabled(false);
    }

    // Configs
    svp = currentTelescope->getBaseDevice()->getSwitch("APPLY_SCOPE_CONFIG");
    if (svp)
    {
        scopeConfigCombo->disconnect();
        scopeConfigCombo->clear();
        for (int i = 0; i < svp->nsp; i++)
            scopeConfigCombo->addItem(svp->sp[i].label);

        scopeConfigCombo->setCurrentIndex(IUFindOnSwitchIndex(svp));
        connect(scopeConfigCombo, SIGNAL(activated(int)), this, SLOT(setScopeConfig(int)));
    }

    // Tracking State
    svp = currentTelescope->getBaseDevice()->getSwitch("TELESCOPE_TRACK_STATE");
    if (svp)
    {
        trackingGroup->setEnabled(true);
        trackOnB->disconnect();
        trackOffB->disconnect();
        connect(trackOnB, &QPushButton::clicked, [&]()
        {
            currentTelescope->setTrackEnabled(true);
        });
        connect(trackOffB, &QPushButton::clicked, [&]()
        {
            if (KMessageBox::questionYesNo(KStars::Instance(),
                                           i18n("Are you sure you want to turn off mount tracking?"),
                                           i18n("Mount Tracking"), KStandardGuiItem::yes(), KStandardGuiItem::no(),
                                           "turn_off_mount_tracking_dialog") == KMessageBox::Yes)
                currentTelescope->setTrackEnabled(false);
        });
    }
    else
    {
        trackOnB->setChecked(false);
        trackOffB->setChecked(false);
        trackingGroup->setEnabled(false);
    }

    ITextVectorProperty *tvp = currentTelescope->getBaseDevice()->getText("SCOPE_CONFIG_NAME");
    if (tvp)
        scopeConfigNameEdit->setText(tvp->tp[0].text);
}

void Mount::registerNewModule(const QString &name)
{
    if (name == "Capture")
    {
        captureInterface = new QDBusInterface("org.kde.kstars", "/KStars/Ekos/Capture", "org.kde.kstars.Ekos.Capture",
                                              QDBusConnection::sessionBus(), this);
    }

}


void Mount::updateText(ITextVectorProperty *tvp)
{
    if (!strcmp(tvp->name, "SCOPE_CONFIG_NAME"))
    {
        scopeConfigNameEdit->setText(tvp->tp[0].text);
    }
}

bool Mount::setScopeConfig(int index)
{
    ISwitchVectorProperty *svp = currentTelescope->getBaseDevice()->getSwitch("APPLY_SCOPE_CONFIG");
    if (svp == nullptr)
        return false;

    IUResetSwitch(svp);
    svp->sp[index].s = ISS_ON;

    // Clear scope config name so that it gets filled by INDI
    scopeConfigNameEdit->clear();

    currentTelescope->getDriverInfo()->getClientManager()->sendNewSwitch(svp);
    return true;
}

void Mount::updateTelescopeCoords()
{
    // No need to update coords if we are still parked.
    if (m_Status == ISD::Telescope::MOUNT_PARKED && m_Status == currentTelescope->status())
        return;

    double ra = 0, dec = 0;
    if (currentTelescope && currentTelescope->getEqCoords(&ra, &dec))
    {
        if (currentTelescope->isJ2000())
        {
            telescopeCoord.setRA0(ra);
            telescopeCoord.setDec0(dec);
            // Get JNow as well
            telescopeCoord.apparentCoord(static_cast<long double>(J2000), KStars::Instance()->data()->ut().djd());
        }
        else
        {
            telescopeCoord.setRA(ra);
            telescopeCoord.setDec(dec);
        }

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
            // If epoch is already J2000, then we don't need to convert to JNow
            if (currentTelescope->isJ2000() == false)
            {
                SkyPoint J2000Coord(telescopeCoord.ra(), telescopeCoord.dec());
                J2000Coord.apparentCoord(KStars::Instance()->data()->ut().djd(), static_cast<long double>(J2000));
                //J2000Coord.precessFromAnyEpoch(KStars::Instance()->data()->ut().djd(), static_cast<long double>(J2000));
                telescopeCoord.setRA0(J2000Coord.ra());
                telescopeCoord.setDec0(J2000Coord.dec());
            }

            m_raValue->setProperty("text", telescopeCoord.ra0().toHMSString());
            m_deValue->setProperty("text", telescopeCoord.dec0().toDMSString());
        }

        // Get horizontal coords
        telescopeCoord.EquatorialToHorizontal(KStarsData::Instance()->lst(), KStarsData::Instance()->geo()->lat());
        azOUT->setText(telescopeCoord.az().toDMSString());
        m_azValue->setProperty("text", telescopeCoord.az().toDMSString());
        altOUT->setText(telescopeCoord.alt().toDMSString());
        m_altValue->setProperty("text", telescopeCoord.alt().toDMSString());

        dms lst = KStarsData::Instance()->geo()->GSTtoLST(KStarsData::Instance()->clock()->utc().gst());
        dms ha(lst - telescopeCoord.ra());
        QChar sgn('+');
        if (ha.Hours() > 12.0)
        {
            ha.setH(24.0 - ha.Hours());
            sgn = '-';
        }
        haOUT->setText(QString("%1%2").arg(sgn).arg(ha.toHMSString()));

        m_haValue->setProperty("text", haOUT->text());
        lstOUT->setText(lst.toHMSString());

        double currentAlt = telescopeCoord.altRefracted().Degrees();

        m_zaValue->setProperty("text", dms(90 - currentAlt).toDMSString());

        if (minAltLimit->isEnabled() && (currentAlt < minAltLimit->value() || currentAlt > maxAltLimit->value()))
        {
            if (currentAlt < minAltLimit->value())
            {
                // Only stop if current altitude is less than last altitude indicate worse situation
                if (currentAlt < lastAlt &&
                        (abortDispatch == -1 ||
                         (currentTelescope->isInMotion() /* && ++abortDispatch > ABORT_DISPATCH_LIMIT*/)))
                {
                    appendLogText(i18n("Telescope altitude is below minimum altitude limit of %1. Aborting motion...",
                                       QString::number(minAltLimit->value(), 'g', 3)));
                    currentTelescope->Abort();
                    //KNotification::event( QLatin1String( "OperationFailed" ));
                    KNotification::beep();
                    abortDispatch++;
                }
            }
            else
            {
                // Only stop if current altitude is higher than last altitude indicate worse situation
                if (currentAlt > lastAlt &&
                        (abortDispatch == -1 ||
                         (currentTelescope->isInMotion() /* && ++abortDispatch > ABORT_DISPATCH_LIMIT*/)))
                {
                    appendLogText(i18n("Telescope altitude is above maximum altitude limit of %1. Aborting motion...",
                                       QString::number(maxAltLimit->value(), 'g', 3)));
                    currentTelescope->Abort();
                    //KNotification::event( QLatin1String( "OperationFailed" ));
                    KNotification::beep();
                    abortDispatch++;
                }
            }
        }
        else
            abortDispatch = -1;

        lastAlt = currentAlt;

        emit newCoords(raOUT->text(), decOUT->text(), azOUT->text(), altOUT->text());

        ISD::Telescope::Status currentStatus = currentTelescope->status();
        bool isTracking = (currentStatus == ISD::Telescope::MOUNT_TRACKING);
        if (m_Status != currentStatus)
        {
            qCDebug(KSTARS_EKOS_MOUNT) << "Mount status changed from " << m_Status << " to " << currentStatus;
            // If we just finished a slew, let's update initialHA and the current target's position
            if (currentStatus == ISD::Telescope::MOUNT_TRACKING && m_Status == ISD::Telescope::MOUNT_SLEWING)
            {
                setInitialHA((sgn == '-' ? -1 : 1) * ha.Hours());
                delete currentTargetPosition;
                currentTargetPosition = new SkyPoint(telescopeCoord.ra(), telescopeCoord.dec());
            }

            m_Status = currentStatus;
            parkB->setEnabled(!currentTelescope->isParked());
            unparkB->setEnabled(currentTelescope->isParked());

            m_Park->setEnabled(!currentTelescope->isParked());
            m_Unpark->setEnabled(currentTelescope->isParked());

            m_statusText->setProperty("text", currentTelescope->getStatusString(currentStatus));

            QAction *a = KStars::Instance()->actionCollection()->action("telescope_track");
            if (a != nullptr)
                a->setChecked(currentStatus == ISD::Telescope::MOUNT_TRACKING);

            emit newStatus(m_Status);
        }

        if (trackingGroup->isEnabled())
        {
            trackOnB->setChecked(isTracking);
            trackOffB->setChecked(!isTracking);
        }

        if (currentTelescope->isConnected() == false)
            updateTimer.stop();
        else if (updateTimer.isActive() == false)
            updateTimer.start();

        // Auto Park Timer
        if (autoParkTimer.isActive())
        {
            QTime remainingTime(0, 0, 0);
            remainingTime = remainingTime.addMSecs(autoParkTimer.remainingTime());
            countdownLabel->setText(remainingTime.toString("hh:mm:ss"));
        }

        if (isTracking && checkMeridianFlip(lst))
            executeMeridianFlip();
    }
    else
        updateTimer.stop();
}

void Mount::updateNumber(INumberVectorProperty *nvp)
{
    if (!strcmp(nvp->name, "TELESCOPE_INFO"))
    {
        if (nvp->s == IPS_ALERT)
        {
            QString newMessage;
            if (primaryScopeApertureIN->value() == 1 || primaryScopeFocalIN->value() == 1)
                newMessage = i18n("Error syncing telescope info. Please fill telescope aperture and focal length.");
            else
                newMessage = i18n("Error syncing telescope info. Check INDI control panel for more details.");
            if (newMessage != lastNotificationMessage)
            {
                appendLogText(newMessage);
                lastNotificationMessage = newMessage;
            }
        }
        else
        {
            syncTelescopeInfo();
            QString newMessage = i18n("Telescope info updated successfully.");
            if (newMessage != lastNotificationMessage)
            {
                appendLogText(newMessage);
                lastNotificationMessage = newMessage;
            }
        }
    }

    if (currentGPS != nullptr && !strcmp(nvp->device, currentGPS->getDeviceName()) && !strcmp(nvp->name, "GEOGRAPHIC_COORD") && nvp->s == IPS_OK)
        syncGPS();
}

bool Mount::setSlewRate(int index)
{
    if (currentTelescope)
        return currentTelescope->setSlewRate(index);

    return false;
}

void Mount::setMeridianFlipValues(bool activate, double hours)
{
    meridianFlipCheckBox->setChecked(activate);
    meridianFlipTimeBox->setValue(hours);
    Options::setExecuteMeridianFlip(meridianFlipCheckBox->isChecked());
    Options::setMeridianFlipOffset(meridianFlipTimeBox->value());
}

void Mount::meridianFlipSetupChanged()
{
    if (meridianFlipCheckBox->isChecked() == false)
        // reset meridian flip
        setMeridianFlipStatus(FLIP_NONE);

    Options::setExecuteMeridianFlip(meridianFlipCheckBox->isChecked());
    Options::setMeridianFlipOffset(meridianFlipTimeBox->value());
}


void Mount::setMeridianFlipStatus(MeridianFlipStatus status)
{
    if (m_MFStatus != status)
    {
        m_MFStatus = status;
        qCDebug (KSTARS_EKOS_MOUNT) << "Setting meridian flip status to " << status;

        meridianFlipStatusChanged(status);
        emit newMeridianFlipStatus(status);
    }
}

void Mount::updateSwitch(ISwitchVectorProperty *svp)
{
    if (!strcmp(svp->name, "TELESCOPE_SLEW_RATE"))
    {
        int index = IUFindOnSwitchIndex(svp);

        m_SpeedSlider->setProperty("value", index);
        m_SpeedLabel->setProperty("text", i18nc(libindi_strings_context, svp->sp[index].label));
    }
    /*else if (!strcmp(svp->name, "TELESCOPE_PARK"))
    {
        ISwitch *sp = IUFindSwitch(svp, "PARK");
        if (sp)
        {
            parkB->setEnabled((sp->s == ISS_OFF));
            unparkB->setEnabled((sp->s == ISS_ON));
        }
    }*/
}

void Mount::appendLogText(const QString &text)
{
    m_LogText.insert(0, i18nc("log entry; %1 is the date, %2 is the text", "%1 %2",
                              QDateTime::currentDateTime().toString("yyyy-MM-ddThh:mm:ss"), text));

    qCInfo(KSTARS_EKOS_MOUNT) << text;

    emit newLog(text);
}

void Mount::updateLog(int messageID)
{
    INDI::BaseDevice *dv = currentTelescope->getBaseDevice();

    QString message = QString::fromStdString(dv->messageQueue(messageID));

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
    if (NS != -1)
    {
        currentTelescope->MoveNS(static_cast<ISD::Telescope::TelescopeMotionNS>(NS),
                                 static_cast<ISD::Telescope::TelescopeMotionCommand>(command));
    }

    if (WE != -1)
    {
        currentTelescope->MoveWE(static_cast<ISD::Telescope::TelescopeMotionWE>(WE),
                                 static_cast<ISD::Telescope::TelescopeMotionCommand>(command));
    }
}

void Mount::save()
{
    if (currentTelescope == nullptr)
        return;

    if (scopeConfigNameEdit->text().isEmpty() == false)
    {
        ITextVectorProperty *tvp = currentTelescope->getBaseDevice()->getText("SCOPE_CONFIG_NAME");
        if (tvp)
        {
            IUSaveText(&(tvp->tp[0]), scopeConfigNameEdit->text().toLatin1().constData());
            currentTelescope->getDriverInfo()->getClientManager()->sendNewText(tvp);
        }
    }

    INumberVectorProperty *nvp = currentTelescope->getBaseDevice()->getNumber("TELESCOPE_INFO");

    if (nvp)
    {
        primaryScopeGroup->setTitle(currentTelescope->getDeviceName());
        guideScopeGroup->setTitle(i18n("%1 guide scope", currentTelescope->getDeviceName()));

        INumber *np = IUFindNumber(nvp, "TELESCOPE_APERTURE");

        if (np)
            np->value = primaryScopeApertureIN->value();

        np = IUFindNumber(nvp, "TELESCOPE_FOCAL_LENGTH");
        if (np)
            np->value = primaryScopeFocalIN->value();

        np = IUFindNumber(nvp, "GUIDER_APERTURE");
        if (np)
            np->value =
                guideScopeApertureIN->value() == 1 ? primaryScopeApertureIN->value() : guideScopeApertureIN->value();

        np = IUFindNumber(nvp, "GUIDER_FOCAL_LENGTH");
        if (np)
            np->value = guideScopeFocalIN->value() == 1 ? primaryScopeFocalIN->value() : guideScopeFocalIN->value();

        ClientManager *clientManager = currentTelescope->getDriverInfo()->getClientManager();

        clientManager->sendNewNumber(nvp);

        currentTelescope->setConfig(SAVE_CONFIG);

        //appendLogText(i18n("Saving telescope information..."));
    }
    else
        appendLogText(i18n("Failed to save telescope information."));
}

void Mount::saveLimits()
{
    Options::setMinimumAltLimit(minAltLimit->value());
    Options::setMaximumAltLimit(maxAltLimit->value());
    currentTelescope->setAltLimits(minAltLimit->value(), maxAltLimit->value());
}

void Mount::enableAltitudeLimits(bool enable)
{
    Options::setEnableAltitudeLimits(enable);

    if (enable)
    {
        minAltLabel->setEnabled(true);
        maxAltLabel->setEnabled(true);

        minAltLimit->setEnabled(true);
        maxAltLimit->setEnabled(true);

        if (currentTelescope)
            currentTelescope->setAltLimits(minAltLimit->value(), maxAltLimit->value());
    }
    else
    {
        minAltLabel->setEnabled(false);
        maxAltLabel->setEnabled(false);

        minAltLimit->setEnabled(false);
        maxAltLimit->setEnabled(false);

        if (currentTelescope)
            currentTelescope->setAltLimits(-1, -1);
    }
}

void Mount::enableAltLimits()
{
    //Only enable if it was already enabled before and the minAltLimit is currently disabled.
    if (altLimitEnabled && minAltLimit->isEnabled() == false)
        enableAltitudeLimits(true);
}

void Mount::disableAltLimits()
{
    altLimitEnabled = enableLimitsCheck->isChecked();

    enableAltitudeLimits(false);
}

QList<double> Mount::altitudeLimits()
{
    QList<double> limits;

    limits.append(minAltLimit->value());
    limits.append(maxAltLimit->value());

    return limits;
}

void Mount::setAltitudeLimits(QList<double> limits)
{
    minAltLimit->setValue(limits[0]);
    maxAltLimit->setValue(limits[1]);
}

void Mount::setAltitudeLimitsEnabled(bool enable)
{
    enableLimitsCheck->setChecked(enable);
}

bool Mount::altitudeLimitsEnabled()
{
    return enableLimitsCheck->isChecked();
}

void Mount::setJ2000Enabled(bool enabled)
{
    m_J2000Check->setProperty("checked", enabled);
}

bool Mount::gotoTarget(const QString &target)
{
    SkyObject *object = KStarsData::Instance()->skyComposite()->findByName(target);

    if (object != nullptr)
        return slew(object->ra().Hours(), object->dec().Degrees());

    return false;
}

bool Mount::syncTarget(const QString &target)
{
    SkyObject *object = KStarsData::Instance()->skyComposite()->findByName(target);

    if (object != nullptr)
        return sync(object->ra().Hours(), object->dec().Degrees());

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
    else
    {
        dms az = dms::fromString(RA, true);
        dms at = dms::fromString(DEC, true);
        SkyPoint horizontalTarget;
        horizontalTarget.setAz(az);
        horizontalTarget.setAlt(at);
        horizontalTarget.HorizontalToEquatorial(KStars::Instance()->data()->lst(), KStars::Instance()->data()->geo()->lat());

        ra = horizontalTarget.ra();
        de = horizontalTarget.dec();
    }

    // If J2000 was checked and the Mount is _not_ already using native J2000 coordinates
    // then we need to convert J2000 to JNow. Otherwise, we send J2000 as is.
    if (m_J2000Check->property("checked").toBool() && currentTelescope && currentTelescope->isJ2000() == false)
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
    if (currentTelescope == nullptr || currentTelescope->isConnected() == false)
        return false;

    dms lst = KStarsData::Instance()->geo()->GSTtoLST(KStarsData::Instance()->clock()->utc().gst());
    double HA = lst.Hours() - RA;
    if (HA > 12.0)
        HA -= 24.0;
    setInitialHA(HA);
    // reset the meridian flip status if the slew is not the meridian flip itself
    if (m_MFStatus != FLIP_RUNNING)
        setMeridianFlipStatus(FLIP_NONE);

    delete currentTargetPosition;
    currentTargetPosition = new SkyPoint(RA, DEC);

    qCDebug(KSTARS_EKOS_MOUNT) << "Slewing to RA=" <<
                                  currentTargetPosition->ra().toHMSString() <<
                                  "DEC=" << currentTargetPosition->dec().toDMSString();

    return currentTelescope->Slew(currentTargetPosition);
}

bool Mount::checkMeridianFlip(dms lst)
{

    if (currentTelescope == nullptr || currentTelescope->isConnected() == false)
    {
        meridianFlipStatusText->setText("Status: inactive");
        setMeridianFlipStatus(FLIP_NONE);
        return false;
    }

    if (currentTelescope->isTracking() == false)
    {
        meridianFlipStatusText->setText("Status: inactive (not tracking)");
        return false;
    }


    double deltaHA = meridianFlipTimeBox->value() - lst.Hours() + telescopeCoord.ra().Hours();
    int hh = static_cast<int> (deltaHA);
    int mm = static_cast<int> ((deltaHA - hh) * 60);
    int ss = static_cast<int> ((deltaHA - hh - mm / 60.0) * 3600);

    switch (m_MFStatus)
    {
        case FLIP_NONE:

            if (initialHA() >= 0 || meridianFlipCheckBox->isChecked() == false)
            {
                meridianFlipStatusText->setText("Status: inactive");
                return false;
            }

            if (deltaHA > 0)
            {
                QString message = QString("Meridian flip in %1h:%2m:%3s")
                                  .arg(hh, 2, 10, QChar('0'))
                                  .arg(mm, 2, 10, QChar('0'))
                                  .arg(ss, 2, 10, QChar('0'));
                meridianFlipStatusText->setText(message);
                return false;
            }
            else if (initialHA() < 0)
            {
                qCDebug(KSTARS_EKOS_MOUNT) << "Meridian flip planned with LST=" <<
                                             lst.toHMSString() <<
                                             " scope RA=" << telescopeCoord.ra().toHMSString() <<
                                             " and meridian diff=" << meridianFlipTimeBox->value();

                setMeridianFlipStatus(FLIP_PLANNED);
                return false;
            }
            break;

        case FLIP_PLANNED:
            return false;
            break;

        case FLIP_ACCEPTED:
            return true;
            break;

        case FLIP_RUNNING:
            if (currentTelescope->isTracking())
            {
                // meridian flip completed
                setMeridianFlipStatus(FLIP_COMPLETED);
            }
            break;

        case FLIP_COMPLETED:
            setMeridianFlipStatus(FLIP_NONE);
            break;

        default:
            // do nothing
            break;
    }
    return false;
}

bool Mount::executeMeridianFlip()
{
    if (initialHA() > 0)
        // no meridian flip necessary
        return false;

    if (currentTelescope->status() != ISD::Telescope::MOUNT_TRACKING)
    {
        // no meridian flip necessary
        return false;
    }

    dms lst = KStarsData::Instance()->geo()->GSTtoLST(KStarsData::Instance()->clock()->utc().gst());
    double HA = lst.Hours() - currentTargetPosition->ra().Hours();
    if (HA > 12.0)
        // no meridian flip necessary
        return false;

    // execute meridian flip
    qCInfo(KSTARS_EKOS_MOUNT) << "Meridian flip: slewing to RA=" <<
                                 currentTargetPosition->ra().toHMSString() <<
                                 "DEC=" << currentTargetPosition->dec().toDMSString();
    setMeridianFlipStatus(FLIP_RUNNING);

    bool result = slew(currentTargetPosition->ra().Hours(), currentTargetPosition->dec().Degrees());

    if (result == false)
        qCWarning(KSTARS_EKOS_MOUNT) << "Meridian flip FAILED: slewing to RA=" <<
                                     currentTargetPosition->ra().toHMSString() <<
                                     "DEC=" << currentTargetPosition->dec().toDMSString();
    return result;

}

void Mount::meridianFlipStatusChanged(Mount::MeridianFlipStatus status)
{

    m_MFStatus = status;

    switch (status)
    {
        case FLIP_NONE:
            meridianFlipStatusText->setText("Status: inactive");
            break;

        case FLIP_PLANNED:
            meridianFlipStatusText->setText("Meridian flip planned...");
            break;

        case FLIP_WAITING:
            meridianFlipStatusText->setText("Meridian flip waiting...");
            break;

        case FLIP_ACCEPTED:
            if (currentTelescope == nullptr || currentTelescope->isTracking() == false)
                // if the mount is not tracking, we go back one step
                setMeridianFlipStatus(FLIP_PLANNED);
            // otherwise do nothing, execution of meridian flip initianted in updateTelescopeCoords()
            break;

        case FLIP_RUNNING:
            meridianFlipStatusText->setText("Meridian flip running...");
            break;

        case FLIP_COMPLETED:
            meridianFlipStatusText->setText("Meridian flip completed.");
            break;
        default:
            break;
    }


}

SkyPoint Mount::currentTarget()
{
    return *currentTargetPosition;
}



bool Mount::sync(const QString &RA, const QString &DEC)
{
    dms ra, de;

    ra = dms::fromString(RA, false);
    de = dms::fromString(DEC, true);

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
    if (currentTelescope == nullptr || currentTelescope->isConnected() == false)
        return false;

    return currentTelescope->Sync(RA, DEC);
}

bool Mount::abort()
{
    return currentTelescope->Abort();
}

IPState Mount::slewStatus()
{
    if (currentTelescope == nullptr)
        return IPS_ALERT;

    return currentTelescope->getState("EQUATORIAL_EOD_COORD");
}

QList<double> Mount::equatorialCoords()
{
    double ra, dec;
    QList<double> coords;

    currentTelescope->getEqCoords(&ra, &dec);
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

double Mount::hourAngle()
{
    dms lst = KStarsData::Instance()->geo()->GSTtoLST(KStarsData::Instance()->clock()->utc().gst());
    dms ha(lst.Degrees() - telescopeCoord.ra().Degrees());
    double HA = ha.Hours();

    if (HA > 12.0)
        return (HA - 24.0);
    else
        return HA;
}

QList<double> Mount::telescopeInfo()
{
    QList<double> info;

    info.append(primaryScopeFocalIN->value());
    info.append(primaryScopeApertureIN->value());
    info.append(guideScopeFocalIN->value());
    info.append(guideScopeApertureIN->value());

    return info;
}

void Mount::setTelescopeInfo(const QList<double> &info)
{
    if (info[0] > 0)
        primaryScopeFocalIN->setValue(info[0]);
    if (info[1] > 0)
        primaryScopeApertureIN->setValue(info[1]);
    if (info[2] > 0)
        guideScopeFocalIN->setValue(info[2]);
    if (info[3] > 0)
        guideScopeApertureIN->setValue(info[3]);

    if (scopeConfigNameEdit->text().isEmpty() == false)
        appendLogText(i18n("Warning: Overriding %1 configuration.", scopeConfigNameEdit->text()));

    save();
}

bool Mount::canPark()
{
    if (currentTelescope == nullptr)
        return false;

    return currentTelescope->canPark();
}

bool Mount::park()
{
    if (currentTelescope == nullptr || currentTelescope->canPark() == false)
        return false;

    return currentTelescope->Park();
}

bool Mount::unpark()
{
    if (currentTelescope == nullptr || currentTelescope->canPark() == false)
        return false;

    return currentTelescope->UnPark();
}

#if 0
Mount::ParkingStatus Mount::getParkingStatus()
{
    if (currentTelescope == nullptr)
        return PARKING_ERROR;

    // In the case mount can't park, return mount is unparked
    if (currentTelescope->canPark() == false)
        return UNPARKING_OK;

    ISwitchVectorProperty *parkSP = currentTelescope->getBaseDevice()->getSwitch("TELESCOPE_PARK");

    if (parkSP == nullptr)
        return PARKING_ERROR;

    switch (parkSP->s)
    {
        case IPS_IDLE:
            // If mount is unparked on startup, state is OK and switch is UNPARK
            if (parkSP->sp[1].s == ISS_ON)
                return UNPARKING_OK;
            else
                return PARKING_IDLE;
        //break;

        case IPS_OK:
            if (parkSP->sp[0].s == ISS_ON)
                return PARKING_OK;
            else
                return UNPARKING_OK;
        //break;

        case IPS_BUSY:
            if (parkSP->sp[0].s == ISS_ON)
                return PARKING_BUSY;
            else
                return UNPARKING_BUSY;

        case IPS_ALERT:
            // If mount replied with an error to the last un/park request,
            // assume state did not change in order to return a clear state
            if (parkSP->sp[0].s == ISS_ON)
                return PARKING_OK;
            else
                return UNPARKING_OK;
    }

    return PARKING_ERROR;
}
#endif

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
    KStarsData *data        = KStarsData::Instance();
    if (FindDialog::Instance()->exec() == QDialog::Accepted)
    {
        SkyObject *object = FindDialog::Instance()->targetObject();
        if (object != nullptr)
        {
            SkyObject *o = object->clone();
            o->updateCoords(data->updateNum(), true, data->geo()->lat(), data->lst(), false);

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

void Mount::centerMount()
{
    if (currentTelescope)
        currentTelescope->runCommand(INDI_ENGAGE_TRACKING);
}

bool Mount::resetModel()
{
    if (currentTelescope == nullptr)
        return false;

    if (currentTelescope->hasAlignmentModel() == false)
        return false;

    if (currentTelescope->clearAlignmentModel())
    {
        appendLogText(i18n("Alignment Model cleared."));
        return true;
    }

    appendLogText(i18n("Failed to clear Alignment Model."));
    return false;
}

void Mount::setGPS(ISD::GDInterface *newGPS)
{
    if (newGPS == currentGPS)
        return;

    //Options::setUseComputerSource(false);
    //Options::setUseDeviceSource(true);

    if (Options::useGPSSource() == false && KMessageBox::questionYesNo(KStars::Instance(),
            i18n("GPS is detected. Do you want to switch time and location source to GPS?"),
            i18n("GPS Settings"), KStandardGuiItem::yes(), KStandardGuiItem::no(),
            "use_gps_source_dialog") == KMessageBox::Yes)
    {
        Options::setUseKStarsSource(false);
        Options::setUseMountSource(false);
        Options::setUseGPSSource(true);
    }

    if (Options::useGPSSource() == false)
        return;

    currentGPS = newGPS;
    connect(newGPS, SIGNAL(numberUpdated(INumberVectorProperty*)), this, SLOT(updateNumber(INumberVectorProperty*)), Qt::UniqueConnection);

    appendLogText(i18n("GPS driver detected. KStars and mount time and location settings are now synced to the GPS driver."));

    syncGPS();
}

void Mount::syncGPS()
{
    // We only update when location is OK
    INumberVectorProperty *location = currentGPS->getBaseDevice()->getNumber("GEOGRAPHIC_COORD");
    if (location == nullptr || location->s != IPS_OK)
        return;

    // Sync name
    if (currentTelescope)
    {
        ITextVectorProperty *activeDevices = currentTelescope->getBaseDevice()->getText("ACTIVE_DEVICES");
        if (activeDevices)
        {
            IText *activeGPS = IUFindText(activeDevices, "ACTIVE_GPS");
            if (activeGPS)
            {
                if (strcmp(activeGPS->text, currentGPS->getDeviceName()))
                {
                    IUSaveText(activeGPS, currentGPS->getDeviceName());
                    currentTelescope->getDriverInfo()->getClientManager()->sendNewText(activeDevices);
                }
            }
        }
    }

    // GPS Refresh should only be called once automatically.
    if (GPSInitialized == false)
    {
        ISwitchVectorProperty *refreshGPS = currentGPS->getBaseDevice()->getSwitch("GPS_REFRESH");
        if (refreshGPS)
        {
            refreshGPS->sp[0].s = ISS_ON;
            currentGPS->getDriverInfo()->getClientManager()->sendNewSwitch(refreshGPS);
            GPSInitialized = true;
        }
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
    if (currentTelescope == nullptr)
        return -1;

    return currentTelescope->getSlewRate();
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

void Mount::startParkTimer()
{
    if (currentTelescope == nullptr)
        return;

    if (currentTelescope->isParked())
    {
        appendLogText("Mount already parked.");
        return;
    }

    QDateTime parkTime = startupTimeEdit->dateTime();
    qint64 parkSeconds = parkTime.msecsTo(KStarsData::Instance()->lt());
    if (parkSeconds > 0)
    {
        appendLogText(i18n("Parking time cannot be in the past."));
        return;
    }

    parkSeconds = std::abs(parkSeconds);

    if (parkSeconds > 24 * 60 * 60 * 1000)
    {
        appendLogText(i18n("Parking time must be within 24 hours of current time."));
        return;
    }

    appendLogText(i18n("Caution: do not use Auto Park while scheduler is active."));

    autoParkTimer.setInterval(parkSeconds);
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
    if (currentTelescope)
    {
        if (currentTelescope->isParked() == false)
        {
            appendLogText(i18n("Starting auto park..."));
            park();
        }
    }
}

}
