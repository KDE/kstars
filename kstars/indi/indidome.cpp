/*  INDI Dome
    Copyright (C) 2015 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include <basedevice.h>
#include <KActionCollection>
#include <KNotification>

#include "indidome.h"
#include "kstars.h"
#include "clientmanager.h"

namespace ISD
{

void Dome::registerProperty(INDI::Property *prop)
{
    if (!strcmp(prop->getName(), "DOME_PARK"))
    {
        ISwitchVectorProperty *svp = prop->getSwitch();

        if (svp)
        {
            ISwitch *sp = IUFindSwitch(svp, "PARK");
            if (sp)
            {
                if ((sp->s == ISS_ON) && svp->s == IPS_OK)
                {
                    parkStatus = PARK_PARKED;

                    QAction *parkAction = KStars::Instance()->actionCollection()->action("dome_park");
                    if (parkAction)
                        parkAction->setEnabled(false);
                    QAction *unParkAction = KStars::Instance()->actionCollection()->action("dome_unpark");
                    if (unParkAction)
                        unParkAction->setEnabled(true);
                }
                else if ((sp->s == ISS_OFF) && svp->s == IPS_OK)
                {
                    parkStatus = PARK_UNPARKED;

                    QAction *parkAction = KStars::Instance()->actionCollection()->action("dome_park");
                    if (parkAction)
                        parkAction->setEnabled(true);
                    QAction *unParkAction = KStars::Instance()->actionCollection()->action("dome_unpark");
                    if (unParkAction)
                        unParkAction->setEnabled(false);
                }
            }
        }
    }

    DeviceDecorator::registerProperty(prop);
}

void Dome::processLight(ILightVectorProperty *lvp)
{
    DeviceDecorator::processLight(lvp);
}

void Dome::processNumber(INumberVectorProperty *nvp)
{
    DeviceDecorator::processNumber(nvp);
}

void Dome::processSwitch(ISwitchVectorProperty *svp)
{
    if (!strcmp(svp->name, "CONNECTION"))
    {
        ISwitch *conSP = IUFindSwitch(svp, "CONNECT");
        if (conSP)
        {
            if (isConnected() == false && conSP->s == ISS_ON)
                KStars::Instance()->slotSetDomeEnabled(true);
            else if (isConnected() && conSP->s == ISS_OFF)
            {
                KStars::Instance()->slotSetDomeEnabled(false);
            }
        }
    }
    else if (!strcmp(svp->name, "DOME_PARK"))
    {
        ISwitch *sp = IUFindSwitch(svp, "PARK");
        if (sp)
        {
            if (svp->s == IPS_ALERT)
            {
                // If alert, set park status to whatever it was opposite to. That is, if it was parking and failed
                // then we set status to unparked since it did not successfully complete parking.
                if (parkStatus == PARK_PARKING)
                    parkStatus = PARK_UNPARKED;
                else if (parkStatus == PARK_UNPARKING)
                    parkStatus = PARK_PARKED;

            }
            else if (svp->s == IPS_BUSY && sp->s == ISS_ON && parkStatus != PARK_PARKING)
            {
                parkStatus = PARK_PARKING;
                KNotification::event(QLatin1String("DomeParking"), i18n("Dome parking is in progress"));
            }
            else if (svp->s == IPS_BUSY && sp->s == ISS_OFF && parkStatus != PARK_UNPARKING)
            {
                parkStatus = PARK_UNPARKING;
                KNotification::event(QLatin1String("DomeUnparking"), i18n("Dome unparking is in progress"));
            }
            else if (svp->s == IPS_OK && sp->s == ISS_ON && parkStatus != PARK_PARKED)
            {
                parkStatus = PARK_PARKED;
                KNotification::event(QLatin1String("DomeParked"), i18n("Dome parked"));

                QAction *parkAction = KStars::Instance()->actionCollection()->action("dome_park");
                if (parkAction)
                    parkAction->setEnabled(false);
                QAction *unParkAction = KStars::Instance()->actionCollection()->action("dome_unpark");
                if (unParkAction)
                    unParkAction->setEnabled(true);
            }
            else if ( (svp->s == IPS_OK || svp->s == IPS_IDLE) && sp->s == ISS_OFF && parkStatus != PARK_UNPARKED)
            {
                parkStatus = PARK_UNPARKED;
                KNotification::event(QLatin1String("DomeUnparked"), i18n("Dome unparked"));

                QAction *parkAction = KStars::Instance()->actionCollection()->action("dome_park");
                if (parkAction)
                    parkAction->setEnabled(true);
                QAction *unParkAction = KStars::Instance()->actionCollection()->action("dome_unpark");
                if (unParkAction)
                    unParkAction->setEnabled(false);
            }
        }
    }

    DeviceDecorator::processSwitch(svp);
}

void Dome::processText(ITextVectorProperty *tvp)
{
    DeviceDecorator::processText(tvp);
}

bool Dome::canPark()
{
    ISwitchVectorProperty *parkSP = baseDevice->getSwitch("DOME_PARK");

    if (parkSP == nullptr)
        return false;

    ISwitch *parkSW = IUFindSwitch(parkSP, "PARK");

    return (parkSW != nullptr);
}

bool Dome::Abort()
{
    ISwitchVectorProperty *motionSP = baseDevice->getSwitch("TELESCOPE_ABORT_MOTION");

    if (motionSP == nullptr)
        return false;

    ISwitch *abortSW = IUFindSwitch(motionSP, "ABORT");

    if (abortSW == nullptr)
        return false;

    abortSW->s = ISS_ON;
    clientManager->sendNewSwitch(motionSP);

    return true;
}

bool Dome::Park()
{
    ISwitchVectorProperty *parkSP = baseDevice->getSwitch("DOME_PARK");

    if (parkSP == nullptr)
        return false;

    ISwitch *parkSW = IUFindSwitch(parkSP, "PARK");

    if (parkSW == nullptr)
        return false;

    IUResetSwitch(parkSP);
    parkSW->s = ISS_ON;
    clientManager->sendNewSwitch(parkSP);

    return true;
}

bool Dome::UnPark()
{
    ISwitchVectorProperty *parkSP = baseDevice->getSwitch("DOME_PARK");

    if (parkSP == nullptr)
        return false;

    ISwitch *parkSW = IUFindSwitch(parkSP, "UNPARK");

    if (parkSW == nullptr)
        return false;

    IUResetSwitch(parkSP);
    parkSW->s = ISS_ON;
    clientManager->sendNewSwitch(parkSP);

    return true;
}

bool Dome::isMoving()
{
    ISwitchVectorProperty *motionSP = baseDevice->getSwitch("DOME_MOTION");

    if (motionSP && motionSP->s == IPS_BUSY)
        return true;

    return false;
}
}
