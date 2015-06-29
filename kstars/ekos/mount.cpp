/*  Ekos Mount Module
    Copyright (C) 2015 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

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
#include "ksnotify.h"

#include <basedevice.h>

extern const char *libindi_strings_context;

#define UPDATE_DELAY            1000
#define ABORT_DISPATCH_LIMIT    3

namespace Ekos
{

Mount::Mount()
{
    setupUi(this);

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
    connect(currentTelescope, SIGNAL(messageUpdated(int)), this, SLOT(updateLog(int)), Qt::UniqueConnection);

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
        guideScopeGroup->setTitle(xi18n("%1 guide scope", currentTelescope->getDeviceName()));

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
                    appendLogText(xi18n("Telescope altitude is below minimum altitude limit of %1. Aborting motion...", QString::number(minAltLimit->value(), 'g', 3)));
                    currentTelescope->Abort();
                    KSNotify::play(KSNotify::NOTIFY_ERROR);
                    abortDispatch++;
                }
            }
            else
            {
                // Only stop if current altitude is higher than last altitude indicate worse situation
                if (currentAlt > lastAlt && (abortDispatch == -1 || (currentTelescope->isInMotion()/* && ++abortDispatch > ABORT_DISPATCH_LIMIT*/)))
                {
                    appendLogText(xi18n("Telescope altitude is above maximum altitude limit of %1. Aborting motion...", QString::number(maxAltLimit->value(), 'g', 3)));
                    currentTelescope->Abort();
                    KSNotify::play(KSNotify::NOTIFY_ERROR);
                    abortDispatch++;
                }
            }
        }
        else
            abortDispatch = -1;

        lastAlt = currentAlt;

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
                newMessage = xi18n("Error syncing telescope info. Please fill telescope aperture and focal length.");
            else
                newMessage = xi18n("Error syncing telescope info. Check INDI control panel for more details.");
            if (newMessage != lastNotificationMessage)
            {
                appendLogText(newMessage);
                lastNotificationMessage = newMessage;
            }
        }
        else
        {
                syncTelescopeInfo();
                QString newMessage = xi18n("Telescope info updated successfully.");
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
    logText.insert(0, xi18nc("log entry; %1 is the date, %2 is the text", "%1 %2", QDateTime::currentDateTime().toString("yyyy-MM-ddThh:mm:ss"), text));

    emit newLog();
}

void Mount::updateLog(int messageID)
{
    INDI::BaseDevice *dv = currentTelescope->getBaseDevice();

    QString message = QString::fromStdString(dv->messageQueue(messageID));

    logText.insert(0, xi18nc("Message shown in Ekos Mount module", "%1", message));

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
        guideScopeGroup->setTitle(xi18n("%1 guide scope", currentTelescope->getDeviceName()));

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

        //appendLogText(xi18n("Saving telescope information..."));

    }
    else
        appendLogText(xi18n("Failed to save telescope information."));
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

void Mount::slew(double RA, double DEC)
{
    currentTelescope->Slew(RA, DEC);
}

void Mount::abort()
{
    currentTelescope->Abort();
}

QString Mount::getSlewStatus()
{
    IPState state = currentTelescope->getState("EQUATORIAL_EOD_COORDS");
    QString status;

    switch (state)
    {
        case IPS_IDLE:
            status = "Idle";
         case IPS_OK:
            status = "Complete";
          case IPS_BUSY:
            status = "Busy";
           case IPS_ALERT:
           default:
            status = "Error";
    }

    return status;

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

}
