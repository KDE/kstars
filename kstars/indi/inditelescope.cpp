/*  INDI CCD
    Copyright (C) 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include <indi/indidevice.h>

#include "Options.h"

#include "inditelescope.h"
#include "kstars.h"
#include "skymap.h"
#include "clientmanager.h"
#include "driverinfo.h"

namespace ISD
{

Telescope::Telescope(GDInterface *iPtr) : DeviceDecorator(iPtr)
{
    dType = KSTARS_TELESCOPE;
    minAlt=-1;
    maxAlt=-1;
    IsParked=false;
}

Telescope::~Telescope()
{
}

void Telescope::registerProperty(INDI::Property *prop)
{
    if (!strcmp(prop->getName(), "TELESCOPE_INFO"))
    {
        INumberVectorProperty *ti = prop->getNumber();
        if (ti == NULL)
            return;

        bool aperture_ok=false, focal_ok=false;
        double temp=0;

        INumber *aperture = IUFindNumber(ti, "TELESCOPE_APERTURE");
        if (aperture && aperture->value == 0)
        {
            if (getDriverInfo()->getAuxInfo().contains("TELESCOPE_APERTURE"))
            {
                temp = getDriverInfo()->getAuxInfo().value("TELESCOPE_APERTURE").toDouble(&aperture_ok);
                if (aperture_ok)
                {
                    aperture->value = temp;
                    INumber *g_aperture = IUFindNumber(ti, "GUIDER_APERTURE");
                    if (g_aperture && g_aperture->value == 0)
                        g_aperture->value = aperture->value;
                }
            }


        }

        INumber *focal_length = IUFindNumber(ti, "TELESCOPE_FOCAL_LENGTH");
        if (focal_length && focal_length->value == 0)
        {
            if (getDriverInfo()->getAuxInfo().contains("TELESCOPE_FOCAL_LENGTH"))
            {
                    temp = getDriverInfo()->getAuxInfo().value("TELESCOPE_FOCAL_LENGTH").toDouble(&focal_ok);
                    if (focal_ok)
                    {
                        focal_length->value = temp;
                        INumber *g_focal = IUFindNumber(ti, "GUIDER_FOCAL_LENGTH");
                        if (g_focal && g_focal->value == 0)
                            g_focal->value = focal_length->value;
                    }
            }
        }

        if (aperture_ok && focal_ok)
            clientManager->sendNewNumber(ti);
    }

    if (!strcmp(prop->getName(), "TELESCOPE_PARK"))
    {
         ISwitchVectorProperty *svp = prop->getSwitch();

         if (svp)
         {
             ISwitch *sp = IUFindSwitch(svp, "PARK");
             if (sp)
             {
                 IsParked = ( (sp->s == ISS_ON) && svp->s == IPS_OK);
             }
         }
    }

    DeviceDecorator::registerProperty(prop);
}

void Telescope::processNumber(INumberVectorProperty *nvp)
{

    if (!strcmp(nvp->name, "EQUATORIAL_EOD_COORD"))
    {
        INumber *RA = IUFindNumber(nvp, "RA");
        INumber *DEC = IUFindNumber(nvp, "DEC");
        if (RA == NULL || DEC == NULL)
            return;

        currentCoord.setRA(RA->value);
        currentCoord.setDec(DEC->value);

        KStars::Instance()->map()->update();

        emit numberUpdated(nvp);

        return;
    }

    if (!strcmp(nvp->name, "HORIZONTAL_COORD"))
    {
        INumber *Az = IUFindNumber(nvp, "AZ");
        INumber *Alt = IUFindNumber(nvp, "ALT");
        if (Az == NULL || Alt == NULL)
            return;

        currentCoord.setAz(Az->value);
        currentCoord.setAlt(Alt->value);
        currentCoord.HorizontalToEquatorial( KStars::Instance()->data()->lst(), KStars::Instance()->data()->geo()->lat() );

        KStars::Instance()->map()->update();

        emit numberUpdated(nvp);

        return;
    }

    DeviceDecorator::processNumber(nvp);

}

void Telescope::processSwitch(ISwitchVectorProperty *svp)
{

    if (!strcmp(svp->name, "TELESCOPE_PARK"))
    {
        ISwitch *sp = IUFindSwitch(svp, "PARK");
        if (sp)
        {
            IsParked = ( (sp->s == ISS_ON) && svp->s == IPS_OK);
        }

        emit switchUpdated(svp);

        return;

    }

    DeviceDecorator::processSwitch(svp);
}

void Telescope::processText(ITextVectorProperty *tvp)
{

    DeviceDecorator::processText(tvp);
}

bool Telescope::canGuide()
{
    INumberVectorProperty *raPulse  = baseDevice->getNumber("TELESCOPE_TIMED_GUIDE_WE");
    INumberVectorProperty *decPulse = baseDevice->getNumber("TELESCOPE_TIMED_GUIDE_NS");

    if (raPulse && decPulse)
        return true;
    else
        return false;
}

bool Telescope::canSync()
{
    ISwitchVectorProperty *motionSP = baseDevice->getSwitch("ON_COORD_SET");
    if (motionSP == NULL)
        return false;

    ISwitch *syncSW = IUFindSwitch(motionSP, "SYNC");

    return (syncSW != NULL);
}

bool Telescope::canPark()
{
    ISwitchVectorProperty *parkSP = baseDevice->getSwitch("TELESCOPE_PARK");
    if (parkSP == NULL)
        return false;

    ISwitch *parkSW = IUFindSwitch(parkSP, "PARK");

    return (parkSW != NULL);
}

bool Telescope::isSlewing()
{
    INumberVectorProperty *EqProp(NULL);

    EqProp = baseDevice->getNumber("EQUATORIAL_EOD_COORD");
    if (EqProp == NULL)
        return false;

    return (EqProp->s == IPS_BUSY);
}

bool Telescope::isInMotion()
{
    ISwitchVectorProperty *movementSP(NULL);
    bool inMotion=false;
    bool inSlew=isSlewing();

    movementSP = baseDevice->getSwitch("TELESCOPE_MOTION_NS");
    if (movementSP)
        inMotion = (movementSP->s == IPS_BUSY);

    movementSP = baseDevice->getSwitch("TELESCOPE_MOTION_WE");
    if (movementSP)
        inMotion = ((movementSP->s == IPS_BUSY) || inMotion);

    return (inSlew || inMotion);
}

bool Telescope::doPulse(GuideDirection ra_dir, int ra_msecs, GuideDirection dec_dir, int dec_msecs )
{
    if (canGuide() == false)
        return false;

    bool raOK=false, decOK=false;
    raOK  = doPulse(ra_dir, ra_msecs);
    decOK = doPulse(dec_dir, dec_msecs);

    if (raOK && decOK)
        return true;
    else
        return false;
}

bool Telescope::doPulse(GuideDirection dir, int msecs )
{
    INumberVectorProperty *raPulse  = baseDevice->getNumber("TELESCOPE_TIMED_GUIDE_WE");
    INumberVectorProperty *decPulse = baseDevice->getNumber("TELESCOPE_TIMED_GUIDE_NS");
    INumberVectorProperty *npulse = NULL;
    INumber *dirPulse=NULL;

    if (raPulse == NULL || decPulse == NULL)
        return false;

    switch(dir)
    {
    case RA_INC_DIR:
    dirPulse = IUFindNumber(raPulse, "TIMED_GUIDE_W");
    if (dirPulse == NULL)
        return false;

    npulse = raPulse;
    break;

    case RA_DEC_DIR:
    dirPulse = IUFindNumber(raPulse, "TIMED_GUIDE_E");
    if (dirPulse == NULL)
        return false;

    npulse = raPulse;
    break;

    case DEC_INC_DIR:
    dirPulse = IUFindNumber(decPulse, "TIMED_GUIDE_N");
    if (dirPulse == NULL)
        return false;

    npulse = decPulse;
    break;

    case DEC_DEC_DIR:
    dirPulse = IUFindNumber(decPulse, "TIMED_GUIDE_S");
    if (dirPulse == NULL)
        return false;

    npulse = decPulse;
    break;

    default:
        return false;

    }

    if (dirPulse == NULL || npulse == NULL)
        return false;

    dirPulse->value = msecs;

    clientManager->sendNewNumber(npulse);

    //qDebug() << "Sending pulse for " << npulse->name << " in direction " << dirPulse->name << " for " << msecs << " ms " << endl;

    return true;


}

bool Telescope::runCommand(int command, void *ptr)
{
    //qDebug() << "Telescope run command is called!!!" << endl;

    switch (command)
    {
       case INDI_SEND_COORDS:
        if (ptr == NULL)
            sendCoords(KStars::Instance()->map()->clickedPoint());
        else
            sendCoords(static_cast<SkyPoint *> (ptr));

        break;


        case INDI_ENGAGE_TRACKING:
            KStars::Instance()->map()->setClickedPoint(&currentCoord);
            KStars::Instance()->map()->slotCenter();
            break;

        default:
            return DeviceDecorator::runCommand(command, ptr);
            break;

    }

    return true;

}

bool Telescope::sendCoords(SkyPoint *ScopeTarget)
{

    INumber *RAEle(NULL), *DecEle(NULL), *AzEle(NULL), *AltEle(NULL);
    INumberVectorProperty *EqProp(NULL), *HorProp(NULL);
    double currentRA=0, currentDEC=0, currentAlt=0, currentAz=0, targetAlt=0;
    bool useJ2000 (false);

    EqProp = baseDevice->getNumber("EQUATORIAL_EOD_COORD");
    if (EqProp == NULL)
    {
    // J2000 Property
        EqProp = baseDevice->getNumber("EQUATORIAL_COORD");
        if (EqProp)
            useJ2000 = true;

    }

    HorProp = baseDevice->getNumber("HORIZONTAL_COORD");

    if (EqProp && EqProp->p == IP_RO)
        EqProp = NULL;

    if (HorProp && HorProp->p == IP_RO)
            HorProp = NULL;

    //qDebug() << "Skymap click - RA: " << scope_target->ra().toHMSString() << " DEC: " << scope_target->dec().toDMSString();

        if (EqProp)
        {
                RAEle  = IUFindNumber(EqProp, "RA");
                if (!RAEle) return false;
                DecEle = IUFindNumber(EqProp, "DEC");
                    if (!DecEle) return false;

           if (useJ2000)
                ScopeTarget->apparentCoord(KStars::Instance()->data()->ut().djd(), (long double) J2000);

              currentRA  = RAEle->value;
              currentDEC = DecEle->value;              

              ScopeTarget->EquatorialToHorizontal(KStarsData::Instance()->lst(), KStarsData::Instance()->geo()->lat());
       }

        if (HorProp)
        {
                AzEle = IUFindNumber(HorProp, "AZ");
                if (!AzEle) return false;
                AltEle = IUFindNumber(HorProp,"ALT");
                if (!AltEle) return false;

            currentAz  = AzEle->value;
            currentAlt = AltEle->value;           
        }

        /* Could not find either properties! */
        if (EqProp == NULL && HorProp == NULL)
            return false;

        //targetAz = ScopeTarget->az().Degrees();
        targetAlt= ScopeTarget->altRefracted().Degrees();

        if (minAlt != -1 && maxAlt != -1)
        {
            if (targetAlt < minAlt || targetAlt > maxAlt)
            {
                KMessageBox::error(NULL, i18n("Requested altitude %1 is outside the specified altitude limit boundary (%2,%3).", QString::number(targetAlt, 'g', 3), QString::number(minAlt, 'g', 3), QString::number(maxAlt, 'g', 3)),
                                   i18n("Telescope Motion"));
                return false;
            }
        }

        if (targetAlt < 0)
        {
            if (KMessageBox::warningContinueCancel(NULL, i18n("Requested altitude is below the horizon. Are you sure you want to proceed?"), i18n("Telescope Motion"),
                                                   KStandardGuiItem::cont(), KStandardGuiItem::cancel(), QString("telescope_coordintes_below_horizon_warning")) == KMessageBox::Cancel)
            {
                if (EqProp)
                {
                    RAEle->value = currentRA;
                    DecEle->value = currentDEC;
                }
                if (HorProp)
                {
                    AzEle->value  = currentAz;
                    AltEle->value = currentAlt;
                }

                return false;
            }
        }

        if (EqProp)
        {
            RAEle->value  = ScopeTarget->ra().Hours();
            DecEle->value = ScopeTarget->dec().Degrees();
            clientManager->sendNewNumber(EqProp);

            if (Options::iNDILogging())
                qDebug() << "ISD:Telescope: Sending coords RA " << RAEle->value << " DEC " << DecEle->value;

            RAEle->value = currentRA;
            DecEle->value = currentDEC;
        }
        // Only send Horizontal Coord property if Equatorial is not available.
        else if (HorProp)
        {
            AzEle->value  = ScopeTarget->az().Degrees();
            AltEle->value = ScopeTarget->alt().Degrees();
            clientManager->sendNewNumber(HorProp);
            AzEle->value  = currentAz;
            AltEle->value = currentAlt;
        }

        return true;

}

bool Telescope::Slew(double ra, double dec)
{
    SkyPoint target;

    target.setRA(ra);
    target.setDec(dec);

    return Slew(&target);
}

bool Telescope::Slew(SkyPoint *ScopeTarget)
{
    ISwitchVectorProperty *motionSP = baseDevice->getSwitch("ON_COORD_SET");
    if (motionSP == NULL)
        return false;

    ISwitch *slewSW = IUFindSwitch(motionSP, "TRACK");
    if (slewSW == NULL)
        slewSW = IUFindSwitch(motionSP, "SLEW");

    if (slewSW == NULL)
        return false;

    if (slewSW->s != ISS_ON)
    {
        IUResetSwitch(motionSP);
        slewSW->s = ISS_ON;
        clientManager->sendNewSwitch(motionSP);

        if (Options::iNDILogging())
            qDebug() << "ISD:Telescope: " << slewSW->name;
    }

    return sendCoords(ScopeTarget);

}

bool Telescope::Sync(double ra, double dec)
{
    SkyPoint target;

    target.setRA(ra);
    target.setDec(dec);    

    return Sync(&target);
}

bool Telescope::Sync(SkyPoint *ScopeTarget)
{
    ISwitchVectorProperty *motionSP = baseDevice->getSwitch("ON_COORD_SET");
    if (motionSP == NULL)
        return false;

    ISwitch *syncSW = IUFindSwitch(motionSP, "SYNC");
    if (syncSW == NULL)
        return false;

    if (syncSW->s != ISS_ON)
    {
        IUResetSwitch(motionSP);
        syncSW->s = ISS_ON;
        clientManager->sendNewSwitch(motionSP);

        if (Options::iNDILogging())
            qDebug() << "ISD:Telescope: Syncing...";
    }

    return sendCoords(ScopeTarget);
}

bool Telescope::Abort()
{
    ISwitchVectorProperty *motionSP = baseDevice->getSwitch("TELESCOPE_ABORT_MOTION");
    if (motionSP == NULL)
        return false;

    ISwitch *abortSW = IUFindSwitch(motionSP, "ABORT");
    if (abortSW == NULL)
        return false;

    if (Options::iNDILogging())
        qDebug() << "ISD:Telescope: Aborted." << endl;

     abortSW->s = ISS_ON;
     clientManager->sendNewSwitch(motionSP);

     return true;
}


bool Telescope::Park()
{
    ISwitchVectorProperty *parkSP = baseDevice->getSwitch("TELESCOPE_PARK");
    if (parkSP == NULL)
        return false;

    ISwitch *parkSW = IUFindSwitch(parkSP, "PARK");
    if (parkSW == NULL)
        return false;

    if (Options::iNDILogging())
        qDebug() << "ISD:Telescope: Parking..." << endl;

     IUResetSwitch(parkSP);
     parkSW->s = ISS_ON;
     clientManager->sendNewSwitch(parkSP);

     return true;
}

bool Telescope::UnPark()
{
    ISwitchVectorProperty *parkSP = baseDevice->getSwitch("TELESCOPE_PARK");
    if (parkSP == NULL)
        return false;

    ISwitch *parkSW = IUFindSwitch(parkSP, "UNPARK");
    if (parkSW == NULL)
        return false;

    if (Options::iNDILogging())
        qDebug() << "ISD:Telescope: UnParking..." << endl;

     IUResetSwitch(parkSP);
     parkSW->s = ISS_ON;
     clientManager->sendNewSwitch(parkSP);

     return true;
}

bool Telescope::getEqCoords(double *ra, double *dec)
{
    INumberVectorProperty *EqProp(NULL);
    INumber *RAEle(NULL), *DecEle(NULL);

    EqProp = baseDevice->getNumber("EQUATORIAL_EOD_COORD");
    if (EqProp == NULL)
        return false;

    RAEle  = IUFindNumber(EqProp, "RA");
    if (!RAEle)
        return false;
    DecEle = IUFindNumber(EqProp, "DEC");
    if (!DecEle)
         return false;

    *ra  = RAEle->value;
    *dec = DecEle->value;

    return true;

}

bool Telescope::MoveNS(TelescopeMotionNS dir, TelescopeMotionCommand cmd)
{
    ISwitchVectorProperty *motionSP = baseDevice->getSwitch("TELESCOPE_MOTION_NS");
    if (motionSP == NULL)
        return false;

    ISwitch *motionNorth = IUFindSwitch(motionSP, "MOTION_NORTH");
    ISwitch *motionSouth = IUFindSwitch(motionSP, "MOTION_SOUTH");

    if (motionNorth == NULL || motionSouth == NULL)
        return false;

    // If same direction, return
    if (dir == MOTION_NORTH && motionNorth->s == ((cmd == MOTION_START) ? ISS_ON : ISS_OFF))
         return true;

    if (dir == MOTION_SOUTH && motionSouth->s == ((cmd == MOTION_START) ? ISS_ON : ISS_OFF))
         return true;

    IUResetSwitch(motionSP);

    if (cmd == MOTION_START)
    {
        if (dir == MOTION_NORTH)
            motionNorth->s = ISS_ON;
        else
            motionSouth->s = ISS_ON;
    }

    clientManager->sendNewSwitch(motionSP);

    return true;
}

bool Telescope::MoveWE(TelescopeMotionWE dir, TelescopeMotionCommand cmd)
{
    ISwitchVectorProperty *motionSP = baseDevice->getSwitch("TELESCOPE_MOTION_WE");
    if (motionSP == NULL)
        return false;

    ISwitch *motionWest = IUFindSwitch(motionSP, "MOTION_WEST");
    ISwitch *motionEast = IUFindSwitch(motionSP, "MOTION_EAST");

    if (motionWest == NULL || motionEast == NULL)
        return false;

    // If same direction, return
    if (dir == MOTION_WEST && motionWest->s == ((cmd == MOTION_START) ? ISS_ON : ISS_OFF))
         return true;

    if (dir == MOTION_EAST && motionEast->s == ((cmd == MOTION_START) ? ISS_ON : ISS_OFF))
         return true;

    IUResetSwitch(motionSP);

    if (cmd == MOTION_START)
    {
        if (dir == MOTION_WEST)
            motionWest->s = ISS_ON;
        else
            motionEast->s = ISS_ON;
    }

    clientManager->sendNewSwitch(motionSP);

    return true;
}

bool Telescope::setSlewRate(int index)
{
    ISwitchVectorProperty *slewRateSP = baseDevice->getSwitch("TELESCOPE_SLEW_RATE");
    if (slewRateSP == NULL)
        return false;

    if (index < 0 || index > slewRateSP->nsp)
        return false;

    IUResetSwitch(slewRateSP);

    slewRateSP->sp[index].s = ISS_ON;

    clientManager->sendNewSwitch(slewRateSP);

    return true;
}

void Telescope::setAltLimits(double minAltitude, double maxAltitude)
{
    minAlt=minAltitude;
    maxAlt=maxAltitude;
}

bool Telescope::isParked()
{
    return IsParked;
}

Telescope::TelescopeStatus Telescope::getStatus()
{
    INumberVectorProperty *EqProp(NULL);

    EqProp = baseDevice->getNumber("EQUATORIAL_EOD_COORD");
    if (EqProp == NULL)
        return MOUNT_ERROR;

    switch (EqProp->s)
    {
    case IPS_IDLE:
        if (isParked())
            return MOUNT_PARKED;
        else
            return MOUNT_IDLE;
        break;

    case IPS_OK:
        return MOUNT_TRACKING;
        break;


    case IPS_BUSY:
    {
        ISwitchVectorProperty *parkSP = baseDevice->getSwitch("TELESCOPE_PARK");
        if (parkSP && parkSP->s == IPS_BUSY)
            return MOUNT_PARKING;
        else
            return MOUNT_SLEWING;
    }
        break;

    case IPS_ALERT:
        return MOUNT_ERROR;
        break;

    }

    return MOUNT_ERROR;
}

const QString Telescope::getStatusString(Telescope::TelescopeStatus status)
{
    switch (status)
    {
    case ISD::Telescope::MOUNT_IDLE:
        return i18n("Idle");
        break;

    case ISD::Telescope::MOUNT_PARKED:
        return i18n("Parked");
        break;

    case ISD::Telescope::MOUNT_PARKING:
        return i18n("Parking");
        break;

    case ISD::Telescope::MOUNT_SLEWING:
        return i18n("Slewing");
        break;

    case ISD::Telescope::MOUNT_TRACKING:
        return i18n("Tracking");
        break;

    case ISD::Telescope::MOUNT_ERROR:
        return i18n("Error");
        break;
    }

    return i18n("Error");
}

}
