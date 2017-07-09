/*  Ekos Mount Module
    Copyright (C) 2015 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include <QtQuick/QQuickView>
#include <QtQuick/QQuickItem>

#include <KNotifications/KNotification>
#include <KLocalizedContext>

#include "mount.h"
#include "Options.h"

#include "indi/driverinfo.h"
#include "indi/indicommon.h"
#include "indi/clientmanager.h"
#include "indi/indifilter.h"

#include "mountadaptor.h"

#include "ekos/ekosmanager.h"

#include "kstars.h"
#include "kspaths.h"
#include "dialogs/finddialog.h"
#include "kstarsdata.h"
#include "ksutils.h"

#include <basedevice.h>

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

    currentTelescope = nullptr;

    stopB->setIcon(QIcon::fromTheme("process-stop", QIcon(":/icons/breeze/default/process-stop.svg")));
    northB->setIcon(QIcon::fromTheme("go-up", QIcon(":/icons/breeze/default/go-up.svg")));
    westB->setIcon(QIcon::fromTheme("go-previous", QIcon(":/icons/breeze/default/go-previous.svg")));
    eastB->setIcon(QIcon::fromTheme("go-next", QIcon(":/icons/breeze/default/go-next.svg")));
    southB->setIcon(QIcon::fromTheme("go-down", QIcon(":/icons/breeze/default/go-down.svg")));

    abortDispatch = -1;

    minAltLimit->setValue(Options::minimumAltLimit());
    maxAltLimit->setValue(Options::maximumAltLimit());

    northwestB->setIcon(QIcon(":/icons/go-nw.svg"));
    northeastB->setIcon(QIcon(":/icons/go-ne.svg"));
    southwestB->setIcon(QIcon(":/icons/go-sw.svg"));
    southeastB->setIcon(QIcon(":/icons/go-se.svg"));

    connect(northB, SIGNAL(pressed()), this, SLOT(move()));
    connect(northB, SIGNAL(released()), this, SLOT(stop()));
    connect(westB, SIGNAL(pressed()), this, SLOT(move()));
    connect(westB, SIGNAL(released()), this, SLOT(stop()));
    connect(southB, SIGNAL(pressed()), this, SLOT(move()));
    connect(southB, SIGNAL(released()), this, SLOT(stop()));
    connect(eastB, SIGNAL(pressed()), this, SLOT(move()));
    connect(eastB, SIGNAL(released()), this, SLOT(stop()));
    connect(northeastB, SIGNAL(pressed()), this, SLOT(move()));
    connect(northeastB, SIGNAL(released()), this, SLOT(stop()));
    connect(northwestB, SIGNAL(pressed()), this, SLOT(move()));
    connect(northwestB, SIGNAL(released()), this, SLOT(stop()));
    connect(southeastB, SIGNAL(pressed()), this, SLOT(move()));
    connect(southeastB, SIGNAL(released()), this, SLOT(stop()));
    connect(southwestB, SIGNAL(pressed()), this, SLOT(move()));
    connect(southwestB, SIGNAL(released()), this, SLOT(stop()));
    connect(stopB, SIGNAL(clicked()), this, SLOT(stop()));
    connect(saveB, SIGNAL(clicked()), this, SLOT(save()));

    connect(slewSpeedCombo, SIGNAL(activated(int)), this, SLOT(setSlewRate(int)));

    connect(minAltLimit, SIGNAL(editingFinished()), this, SLOT(saveLimits()));
    connect(maxAltLimit, SIGNAL(editingFinished()), this, SLOT(saveLimits()));

    connect(mountToolBoxB, SIGNAL(clicked()), this, SLOT(showMountToolBox()));

    connect(clearAlignmentModelB, &QPushButton::clicked, this, [this]()
    {
        if (currentTelescope->hasAlignmentModel() == false)
            return;

        if (currentTelescope->clearAlignmentModel())
            appendLogText(i18n("Alignment Model cleared."));
        else
            appendLogText(i18n("Failed to clear Alignment Model."));
    });

    connect(enableLimitsCheck, SIGNAL(toggled(bool)), this, SLOT(enableAltitudeLimits(bool)));
    enableLimitsCheck->setChecked(Options::enableAltitudeLimits());
    altLimitEnabled = enableLimitsCheck->isChecked();

    updateTimer.setInterval(UPDATE_DELAY);
    connect(&updateTimer, SIGNAL(timeout()), this, SLOT(updateTelescopeCoords()));

    // QML Stuff
    m_BaseView = new QQuickView();
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

    m_BaseView->setMaximumSize(QSize(200, 480));
    m_BaseView->setMinimumSize(QSize(200, 480));
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
    m_Park         = m_BaseObj->findChild<QQuickItem *>("parkButtonObject");
    m_Unpark       = m_BaseObj->findChild<QQuickItem *>("unparkButtonObject");
    m_statusText   = m_BaseObj->findChild<QQuickItem *>("statusTextObject");
}

Mount::~Mount()
{
    delete(m_BaseView);
}

void Mount::setTelescope(ISD::GDInterface *newTelescope)
{
    if (newTelescope == currentTelescope)
    {
        syncTelescopeInfo();
        return;
    }

    currentTelescope = static_cast<ISD::Telescope *>(newTelescope);

    connect(currentTelescope, SIGNAL(numberUpdated(INumberVectorProperty *)), this,
            SLOT(updateNumber(INumberVectorProperty *)), Qt::UniqueConnection);
    connect(currentTelescope, SIGNAL(switchUpdated(ISwitchVectorProperty *)), this,
            SLOT(updateSwitch(ISwitchVectorProperty *)), Qt::UniqueConnection);
    connect(currentTelescope, SIGNAL(newTarget(QString)), this, SIGNAL(newTarget(QString)), Qt::UniqueConnection);

    //Disable this for now since ALL INDI drivers now log their messages to verbose output
    //connect(currentTelescope, SIGNAL(messageUpdated(int)), this, SLOT(updateLog(int)), Qt::UniqueConnection);

    if (enableLimitsCheck->isChecked())
        currentTelescope->setAltLimits(minAltLimit->value(), maxAltLimit->value());

    updateTimer.start();

    syncTelescopeInfo();
}

void Mount::syncTelescopeInfo()
{
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
        slewSpeedCombo->clear();
        slewSpeedCombo->setEnabled(true);

        for (int i = 0; i < svp->nsp; i++)
            slewSpeedCombo->addItem(i18nc(libindi_strings_context, svp->sp[i].label));

        int index = IUFindOnSwitchIndex(svp);
        slewSpeedCombo->setCurrentIndex(index);

        // QtQuick
        m_SpeedSlider->setEnabled(true);
        m_SpeedSlider->setProperty("maximumValue", svp->nsp - 1);
        m_SpeedSlider->setProperty("value", index);

        m_SpeedLabel->setProperty("text", slewSpeedCombo->currentText());
        m_SpeedLabel->setEnabled(true);
    }
    else
    {
        slewSpeedCombo->setEnabled(false);
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
}

void Mount::updateTelescopeCoords()
{
    double ra, dec;

    if (currentTelescope && currentTelescope->getEqCoords(&ra, &dec))
    {
        telescopeCoord.setRA(ra);
        telescopeCoord.setDec(dec);
        telescopeCoord.EquatorialToHorizontal(KStarsData::Instance()->lst(), KStarsData::Instance()->geo()->lat());

        raOUT->setText(telescopeCoord.ra().toHMSString());
        m_raValue->setProperty("text", telescopeCoord.ra().toHMSString());
        decOUT->setText(telescopeCoord.dec().toDMSString());
        m_deValue->setProperty("text", telescopeCoord.dec().toDMSString());

        azOUT->setText(telescopeCoord.az().toDMSString());
        m_azValue->setProperty("text", telescopeCoord.az().toDMSString());
        altOUT->setText(telescopeCoord.alt().toDMSString());
        m_altValue->setProperty("text", telescopeCoord.alt().toDMSString());

        dms lst = KStarsData::Instance()->geo()->GSTtoLST(KStarsData::Instance()->clock()->utc().gst());
        dms ha(lst.Degrees() - telescopeCoord.ra().Degrees());
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

        newCoords(raOUT->text(), decOUT->text(), azOUT->text(), altOUT->text());

        ISD::Telescope::TelescopeStatus currentStatus = currentTelescope->getStatus();
        if (lastStatus != currentStatus)
        {
            lastStatus = currentStatus;
            parkB->setEnabled(!currentTelescope->isParked());
            unparkB->setEnabled(currentTelescope->isParked());

            m_Park->setEnabled(!currentTelescope->isParked());
            m_Unpark->setEnabled(currentTelescope->isParked());

            m_statusText->setProperty("text", currentTelescope->getStatusString(currentStatus));

            emit newStatus(lastStatus);
        }

        if (currentTelescope->isConnected() == false)
            updateTimer.stop();
        else if (updateTimer.isActive() == false)
            updateTimer.start();
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
}

bool Mount::setSlewRate(int index)
{
    if (currentTelescope && slewSpeedCombo->isEnabled())
        return currentTelescope->setSlewRate(index);

    return false;
}

void Mount::updateSwitch(ISwitchVectorProperty *svp)
{
    if (!strcmp(svp->name, "TELESCOPE_SLEW_RATE"))
    {
        int index = IUFindOnSwitchIndex(svp);

        slewSpeedCombo->blockSignals(true);
        slewSpeedCombo->setCurrentIndex(index);
        slewSpeedCombo->blockSignals(false);

        m_SpeedSlider->setProperty("value", index);
        m_SpeedLabel->setProperty("text", slewSpeedCombo->currentText());
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
    logText.insert(0, i18nc("log entry; %1 is the date, %2 is the text", "%1 %2",
                            QDateTime::currentDateTime().toString("yyyy-MM-ddThh:mm:ss"), text));

    if (Options::verboseLogging())
        qDebug() << "Mount: " << text;

    emit newLog();
}

void Mount::updateLog(int messageID)
{
    INDI::BaseDevice *dv = currentTelescope->getBaseDevice();

    QString message = QString::fromStdString(dv->messageQueue(messageID));

    logText.insert(0, i18nc("Message shown in Ekos Mount module", "%1", message));

    emit newLog();
}

void Mount::clearLog()
{
    logText.clear();
    emit newLog();
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

void Mount::move()
{
    QObject *obj = sender();

    if (obj == northB)
    {
        currentTelescope->MoveNS(ISD::Telescope::MOTION_NORTH, ISD::Telescope::MOTION_START);
    }
    else if (obj == westB)
    {
        currentTelescope->MoveWE(ISD::Telescope::MOTION_WEST, ISD::Telescope::MOTION_START);
    }
    else if (obj == southB)
    {
        currentTelescope->MoveNS(ISD::Telescope::MOTION_SOUTH, ISD::Telescope::MOTION_START);
    }
    else if (obj == eastB)
    {
        currentTelescope->MoveWE(ISD::Telescope::MOTION_EAST, ISD::Telescope::MOTION_START);
    }
    else if (obj == northwestB)
    {
        currentTelescope->MoveNS(ISD::Telescope::MOTION_NORTH, ISD::Telescope::MOTION_START);
        currentTelescope->MoveWE(ISD::Telescope::MOTION_WEST, ISD::Telescope::MOTION_START);
    }
    else if (obj == northeastB)
    {
        currentTelescope->MoveNS(ISD::Telescope::MOTION_NORTH, ISD::Telescope::MOTION_START);
        currentTelescope->MoveWE(ISD::Telescope::MOTION_EAST, ISD::Telescope::MOTION_START);
    }
    else if (obj == southwestB)
    {
        currentTelescope->MoveNS(ISD::Telescope::MOTION_SOUTH, ISD::Telescope::MOTION_START);
        currentTelescope->MoveWE(ISD::Telescope::MOTION_WEST, ISD::Telescope::MOTION_START);
    }
    else if (obj == southeastB)
    {
        currentTelescope->MoveNS(ISD::Telescope::MOTION_SOUTH, ISD::Telescope::MOTION_START);
        currentTelescope->MoveWE(ISD::Telescope::MOTION_EAST, ISD::Telescope::MOTION_START);
    }
}

void Mount::stop()
{
    QObject *obj = sender();

    if (obj == stopB)
        currentTelescope->Abort();
    else if (obj == northB)
    {
        currentTelescope->MoveNS(ISD::Telescope::MOTION_NORTH, ISD::Telescope::MOTION_STOP);
    }
    else if (obj == westB)
    {
        currentTelescope->MoveWE(ISD::Telescope::MOTION_WEST, ISD::Telescope::MOTION_STOP);
    }
    else if (obj == southB)
    {
        currentTelescope->MoveNS(ISD::Telescope::MOTION_SOUTH, ISD::Telescope::MOTION_STOP);
    }
    else if (obj == eastB)
    {
        currentTelescope->MoveWE(ISD::Telescope::MOTION_EAST, ISD::Telescope::MOTION_STOP);
    }
    else if (obj == northwestB)
    {
        currentTelescope->MoveNS(ISD::Telescope::MOTION_NORTH, ISD::Telescope::MOTION_STOP);
        currentTelescope->MoveWE(ISD::Telescope::MOTION_WEST, ISD::Telescope::MOTION_STOP);
    }
    else if (obj == northeastB)
    {
        currentTelescope->MoveNS(ISD::Telescope::MOTION_NORTH, ISD::Telescope::MOTION_STOP);
        currentTelescope->MoveWE(ISD::Telescope::MOTION_EAST, ISD::Telescope::MOTION_STOP);
    }
    else if (obj == southwestB)
    {
        currentTelescope->MoveNS(ISD::Telescope::MOTION_SOUTH, ISD::Telescope::MOTION_STOP);
        currentTelescope->MoveWE(ISD::Telescope::MOTION_WEST, ISD::Telescope::MOTION_STOP);
    }
    else if (obj == southeastB)
    {
        currentTelescope->MoveNS(ISD::Telescope::MOTION_SOUTH, ISD::Telescope::MOTION_STOP);
        currentTelescope->MoveWE(ISD::Telescope::MOTION_EAST, ISD::Telescope::MOTION_STOP);
    }
}

void Mount::save()
{
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

QList<double> Mount::getAltitudeLimits()
{
    QList<double> limits;

    limits.append(minAltLimit->value());
    limits.append(maxAltLimit->value());

    return limits;
}

void Mount::setAltitudeLimits(double minAltitude, double maxAltitude, bool enabled)
{
    minAltLimit->setValue(minAltitude);
    maxAltLimit->setValue(maxAltitude);

    enableLimitsCheck->setChecked(enabled);
}

bool Mount::isLimitsEnabled()
{
    return enableLimitsCheck->isChecked();
}

bool Mount::slew(const QString &RA, const QString &DEC)
{
    dms ra, de;

    ra = dms::fromString(RA, false);
    de = dms::fromString(DEC, true);

    return slew(ra.Hours(), de.Degrees());
}

bool Mount::slew(double RA, double DEC)
{
    if (currentTelescope == nullptr || currentTelescope->isConnected() == false)
        return false;

    return currentTelescope->Slew(RA, DEC);
}

bool Mount::sync(const QString &RA, const QString &DEC)
{
    dms ra, de;

    ra = dms::fromString(RA, false);
    de = dms::fromString(DEC, true);

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

IPState Mount::getSlewStatus()
{
    if (currentTelescope == nullptr)
        return IPS_ALERT;

    return currentTelescope->getState("EQUATORIAL_EOD_COORD");
}

QList<double> Mount::getEquatorialCoords()
{
    double ra, dec;
    QList<double> coords;

    currentTelescope->getEqCoords(&ra, &dec);
    coords.append(ra);
    coords.append(dec);

    return coords;
}

QList<double> Mount::getHorizontalCoords()
{
    QList<double> coords;

    coords.append(telescopeCoord.az().Degrees());
    coords.append(telescopeCoord.alt().Degrees());

    return coords;
}

double Mount::getHourAngle()
{
    dms lst = KStarsData::Instance()->geo()->GSTtoLST(KStarsData::Instance()->clock()->utc().gst());
    dms ha(lst.Degrees() - telescopeCoord.ra().Degrees());
    double HA = ha.Hours();

    if (HA > 12.0)
        return (24 - HA);
    else
        return HA;
}

QList<double> Mount::getTelescopeInfo()
{
    QList<double> info;

    info.append(primaryScopeFocalIN->value());
    info.append(primaryScopeApertureIN->value());
    info.append(guideScopeFocalIN->value());
    info.append(guideScopeApertureIN->value());

    return info;
}

void Mount::setTelescopeInfo(double primaryFocalLength, double primaryAperture, double guideFocalLength,
                             double guideAperture)
{
    primaryScopeFocalIN->setValue(primaryFocalLength);
    primaryScopeApertureIN->setValue(primaryAperture);
    guideScopeFocalIN->setValue(guideFocalLength);
    guideScopeApertureIN->setValue(guideAperture);
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

Mount::ParkingStatus Mount::getParkingStatus()
{
    if (currentTelescope == nullptr || currentTelescope->canPark() == false)
        return PARKING_ERROR;

    ISwitchVectorProperty *parkSP = currentTelescope->getBaseDevice()->getSwitch("TELESCOPE_PARK");

    if (parkSP == nullptr)
        return PARKING_ERROR;

    switch (parkSP->s)
    {
        case IPS_IDLE:
            return PARKING_IDLE;

        case IPS_OK:
            if (parkSP->sp[0].s == ISS_ON)
                return PARKING_OK;
            else
                return UNPARKING_OK;
            break;

        case IPS_BUSY:
            if (parkSP->sp[0].s == ISS_ON)
                return PARKING_BUSY;
            else
                return UNPARKING_BUSY;

        case IPS_ALERT:
            return PARKING_ERROR;
    }

    return PARKING_ERROR;
}

void Mount::showMountToolBox()
{
    m_BaseView->show();
}

void Mount::findTarget()
{
    KStarsData *data        = KStarsData::Instance();
    QPointer<FindDialog> fd = new FindDialog(KStars::Instance());
    if (fd->exec() == QDialog::Accepted)
    {
        SkyObject *object = fd->targetObject();
        if (object != 0)
        {
            SkyObject *o = object->clone();
            o->updateCoords(data->updateNum(), true, data->geo()->lat(), data->lst(), false);

            m_targetText->setProperty("text", o->name());
            m_targetRAText->setProperty("text", o->ra().toHMSString());
            m_targetDEText->setProperty("text", o->dec().toDMSString());
        }
    }
    delete fd;
}

void Mount::centerMount()
{
    if (currentTelescope)
        currentTelescope->runCommand(INDI_ENGAGE_TRACKING);
}
}
