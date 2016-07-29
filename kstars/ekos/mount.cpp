/*  Ekos Mount Module
    Copyright (C) 2015 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include <KNotifications/KNotification>
#include "mount.h"
#include "Options.h"

#include "indi/driverinfo.h"
#include "indi/indicommon.h"
#include "indi/clientmanager.h"
#include "indi/indifilter.h"

#include "mountadaptor.h"

#include "ekosmanager.h"

#include "kstars.h"
#include "kstarsdata.h"
#include "ksutils.h"

#include <basedevice.h>

extern const char *libindi_strings_context;

#define UPDATE_DELAY            1000
#define ABORT_DISPATCH_LIMIT    3

namespace Ekos
{

Mount::Mount()
{
    setupUi(this);
    new MountAdaptor(this);
    QDBusConnection::sessionBus().registerObject("/KStars/Ekos/Mount",  this);

    currentTelescope = NULL;

    stopB->setIcon(QIcon::fromTheme("process-stop"));
    northB->setIcon(QIcon::fromTheme("go-up"));
    westB->setIcon(QIcon::fromTheme("go-previous"));
    eastB->setIcon(QIcon::fromTheme("go-next"));
    southB->setIcon(QIcon::fromTheme("go-down"));

    abortDispatch = -1;

    minAltLimit->setValue(Options::minimumAltLimit());
    maxAltLimit->setValue(Options::maximumAltLimit());


    QFile tempFile;

    if (KSUtils::openDataFile( tempFile, "go-nw.png" ) )
    {
        northwestB->setIcon(QIcon(tempFile.fileName()));
        tempFile.close();
    }

    if (KSUtils::openDataFile( tempFile, "go-ne.png" ) )
    {
        northeastB->setIcon(QIcon(tempFile.fileName()));
        tempFile.close();
    }

    if (KSUtils::openDataFile( tempFile, "go-sw.png" ) )
    {
        southwestB->setIcon(QIcon(tempFile.fileName()));
        tempFile.close();
    }

    if (KSUtils::openDataFile( tempFile, "go-se.png" ) )
    {
        southeastB->setIcon(QIcon(tempFile.fileName()));
        tempFile.close();
    }

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

    connect(minAltLimit, SIGNAL(editingFinished()), this, SLOT(saveLimits()));
    connect(maxAltLimit, SIGNAL(editingFinished()), this, SLOT(saveLimits()));

    connect(enableLimitsCheck, SIGNAL(toggled(bool)), this, SLOT(enableAltitudeLimits(bool)));
    enableLimitsCheck->setChecked(Options::enableAltitudeLimits());
    altLimitEnabled = enableLimitsCheck->isChecked();
}

Mount::~Mount()
{

}

void Mount::setTelescope(ISD::GDInterface *newTelescope)
{
    currentTelescope = static_cast<ISD::Telescope*> (newTelescope);

    connect(currentTelescope, SIGNAL(numberUpdated(INumberVectorProperty*)), this, SLOT(updateNumber(INumberVectorProperty*)), Qt::UniqueConnection);
    connect(currentTelescope, SIGNAL(switchUpdated(ISwitchVectorProperty*)), this, SLOT(updateSwitch(ISwitchVectorProperty*)), Qt::UniqueConnection);

    //Disable this for now since ALL INDI drivers now log their messages to verbose output
    //connect(currentTelescope, SIGNAL(messageUpdated(int)), this, SLOT(updateLog(int)), Qt::UniqueConnection);

    if (enableLimitsCheck->isChecked())
        currentTelescope->setAltLimits(minAltLimit->value(), maxAltLimit->value());

    QTimer::singleShot(UPDATE_DELAY, this, SLOT(updateTelescopeCoords()));

    syncTelescopeInfo();
}

void Mount::syncTelescopeInfo()
{
    INumberVectorProperty * nvp = currentTelescope->getBaseDevice()->getNumber("TELESCOPE_INFO");

    if (nvp)
    {

        primaryScopeGroup->setTitle(currentTelescope->getDeviceName());
        guideScopeGroup->setTitle(i18n("%1 guide scope", currentTelescope->getDeviceName()));

        INumber *np = NULL;

        np = IUFindNumber(nvp, "TELESCOPE_APERTURE");
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

        for (int i=0; i < svp->nsp; i++)
            slewSpeedCombo->addItem(i18nc(libindi_strings_context, svp->sp[i].label));

        int index = IUFindOnSwitchIndex(svp);
        slewSpeedCombo->setCurrentIndex(index);
        connect(slewSpeedCombo, SIGNAL(activated(int)), currentTelescope, SLOT(setSlewRate(int)), Qt::UniqueConnection);
    }
    else
    {
        slewSpeedCombo->setEnabled(false);
        disconnect(slewSpeedCombo, SIGNAL(activated(int)), currentTelescope, SLOT(setSlewRate(int)));
    }

    if (currentTelescope->canPark())
    {
        parkB->setEnabled(!currentTelescope->isParked());
        unparkB->setEnabled(currentTelescope->isParked());
        connect(parkB, SIGNAL(clicked()), currentTelescope, SLOT(Park()), Qt::UniqueConnection);
        connect(unparkB, SIGNAL(clicked()), currentTelescope, SLOT(UnPark()), Qt::UniqueConnection);
    }
    else
    {
        parkB->setEnabled(false);
        unparkB->setEnabled(false);
        disconnect(parkB, SIGNAL(clicked()), currentTelescope, SLOT(Park()));
        disconnect(unparkB, SIGNAL(clicked()), currentTelescope, SLOT(UnPark()));
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
        decOUT->setText(telescopeCoord.dec().toDMSString());
        azOUT->setText(telescopeCoord.az().toDMSString());
        altOUT->setText(telescopeCoord.alt().toDMSString());

        dms lst = KStarsData::Instance()->geo()->GSTtoLST( KStarsData::Instance()->clock()->utc().gst() );
        dms ha( lst.Degrees() - telescopeCoord.ra().Degrees() );
        QChar sgn('+');
        if ( ha.Hours() > 12.0 ) {
            ha.setH( 24.0 - ha.Hours() );
            sgn = '-';
        }
        haOUT->setText( QString("%1%2").arg(sgn).arg( ha.toHMSString() ) );
        lstOUT->setText(lst.toHMSString());

        double currentAlt = telescopeCoord.altRefracted().Degrees();

        if (minAltLimit->isEnabled()
             && ( currentAlt < minAltLimit->value() || currentAlt > maxAltLimit->value()))
        {
            if (currentAlt < minAltLimit->value())
            {
                // Only stop if current altitude is less than last altitude indicate worse situation
                if (currentAlt < lastAlt && (abortDispatch == -1 || (currentTelescope->isInMotion()/* && ++abortDispatch > ABORT_DISPATCH_LIMIT*/)))
                {
                    appendLogText(i18n("Telescope altitude is below minimum altitude limit of %1. Aborting motion...", QString::number(minAltLimit->value(), 'g', 3)));
                    currentTelescope->Abort();
                    //KNotification::event( QLatin1String( "OperationFailed" ));
                    KNotification::beep();
                    abortDispatch++;
                }
            }
            else
            {
                // Only stop if current altitude is higher than last altitude indicate worse situation
                if (currentAlt > lastAlt && (abortDispatch == -1 || (currentTelescope->isInMotion()/* && ++abortDispatch > ABORT_DISPATCH_LIMIT*/)))
                {
                    appendLogText(i18n("Telescope altitude is above maximum altitude limit of %1. Aborting motion...", QString::number(maxAltLimit->value(), 'g', 3)));
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

        newStatus(currentTelescope->getStatus());

        if (currentTelescope->isConnected())
            QTimer::singleShot(UPDATE_DELAY, this, SLOT(updateTelescopeCoords()));
    }
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

void Mount::updateSwitch(ISwitchVectorProperty *svp)
{
    if (!strcmp(svp->name, "TELESCOPE_SLEW_RATE"))
    {
        int index = IUFindOnSwitchIndex(svp);
        slewSpeedCombo->setCurrentIndex(index);
    }
    else if (!strcmp(svp->name, "TELESCOPE_PARK"))
    {
        ISwitch *sp = IUFindSwitch(svp, "PARK");
        if (sp)
        {
            parkB->setEnabled((sp->s == ISS_OFF));
            unparkB->setEnabled((sp->s == ISS_ON));
        }
    }
}

void Mount::appendLogText(const QString &text)
{
    logText.insert(0, i18nc("log entry; %1 is the date, %2 is the text", "%1 %2", QDateTime::currentDateTime().toString("yyyy-MM-ddThh:mm:ss"), text));

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

void Mount::move()
{
    QObject* obj = sender();

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
    QObject* obj = sender();

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
    INumberVectorProperty * nvp = currentTelescope->getBaseDevice()->getNumber("TELESCOPE_INFO");

    if (nvp)
    {

        primaryScopeGroup->setTitle(currentTelescope->getDeviceName());
        guideScopeGroup->setTitle(i18n("%1 guide scope", currentTelescope->getDeviceName()));

        INumber *np = NULL;

        np = IUFindNumber(nvp, "TELESCOPE_APERTURE");
        if (np)
            np->value = primaryScopeApertureIN->value();
        np = IUFindNumber(nvp, "TELESCOPE_FOCAL_LENGTH");
        if (np)
            np->value = primaryScopeFocalIN->value();
        np = IUFindNumber(nvp, "GUIDER_APERTURE");
        if (np)
            np->value = guideScopeApertureIN->value() == 1 ? primaryScopeApertureIN->value() : guideScopeApertureIN->value();
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

bool Mount::slew(double RA, double DEC)
{
    if (currentTelescope == NULL || currentTelescope->isConnected() == false)
        return false;

    return currentTelescope->Slew(RA, DEC);
}

bool Mount::abort()
{
    return currentTelescope->Abort();
}

IPState Mount::getSlewStatus()
{
    if (currentTelescope == NULL)
        return IPS_ALERT;

    return currentTelescope->getState("EQUATORIAL_EOD_COORD");
}

QList<double> Mount::getEquatorialCoords()
{
    double ra,dec;
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
    dms lst = KStarsData::Instance()->geo()->GSTtoLST( KStarsData::Instance()->clock()->utc().gst() );
    dms ha( lst.Degrees() - telescopeCoord.ra().Degrees() );
    double HA = ha.Hours();

    if ( HA > 12.0 )
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

void Mount::setTelescopeInfo(double primaryFocalLength, double primaryAperture, double guideFocalLength, double guideAperture)
{
    primaryScopeFocalIN->setValue(primaryFocalLength);
    primaryScopeApertureIN->setValue(primaryAperture);
    guideScopeFocalIN->setValue(guideFocalLength);
    guideScopeApertureIN->setValue(guideAperture);
}

bool Mount::canPark()
{
    if (currentTelescope == NULL)
        return false;

    return currentTelescope->canPark();
}

bool Mount::park()
{
    if (currentTelescope == NULL || currentTelescope->canPark() == false)
        return false;

    return currentTelescope->Park();
}

bool Mount::unpark()
{
    if (currentTelescope == NULL || currentTelescope->canPark() == false)
        return false;

    return currentTelescope->UnPark();
}

Mount::ParkingStatus Mount::getParkingStatus()
{
    if (currentTelescope == NULL || currentTelescope->canPark() == false)
        return PARKING_ERROR;

    ISwitchVectorProperty* parkSP = currentTelescope->getBaseDevice()->getSwitch("TELESCOPE_PARK");

    if (parkSP == NULL)
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

}
