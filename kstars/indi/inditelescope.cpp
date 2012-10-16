/*  INDI CCD
    Copyright (C) 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include <indi/indidevice.h>

#include "inditelescope.h"
#include "kstars.h"
#include "skymap.h"
#include "clientmanager.h"

namespace ISD
{

Telescope::Telescope(GDInterface *iPtr) : DeviceDecorator(iPtr)
{
    dType = KSTARS_TELESCOPE;
}

Telescope::~Telescope()
{
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

        KStars::Instance()->map()->update();

        emit numberUpdated(nvp);

        return;
    }

    DeviceDecorator::processNumber(nvp);

}

void Telescope::processSwitch(ISwitchVectorProperty *svp)
{

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
    bool useJ2000 (false);

    EqProp = baseDevice->getNumber("EQUATORIAL_EOD_COORD_REQUEST");
    if (EqProp == NULL)
    {
    // Backward compatibility
        EqProp = baseDevice->getNumber("EQUATORIAL_EOD_COORD");
        if (EqProp == NULL)
    {
    // J2000 Property
        EqProp = baseDevice->getNumber("EQUATORIAL_COORD_REQUEST");
        if (EqProp)
            useJ2000 = true;
        }
    }

    HorProp = baseDevice->getNumber("HORIZONTAL_COORD_REQUEST");

    if (EqProp && EqProp->p == IP_RO)
        EqProp = NULL;

    if (HorProp && HorProp->p == IP_RO)
            HorProp = NULL;

    //kDebug() << "Skymap click - RA: " << scope_target->ra().toHMSString() << " DEC: " << scope_target->dec().toDMSString();

        if (EqProp)
        {
                RAEle  = IUFindNumber(EqProp, "RA");
                if (!RAEle) return false;
                DecEle = IUFindNumber(EqProp, "DEC");
                    if (!DecEle) return false;

           if (useJ2000)
                ScopeTarget->apparentCoord(KStars::Instance()->data()->ut().djd(), (long double) J2000);

              RAEle->value  = ScopeTarget->ra().Hours();
              DecEle->value = ScopeTarget->dec().Degrees();
       }

        if (HorProp)
        {
                AzEle = IUFindNumber(HorProp, "AZ");
                if (!AzEle) return false;
                AltEle = IUFindNumber(HorProp,"ALT");
                if (!AltEle) return false;

            AzEle->value  = ScopeTarget->az().Degrees();
            AltEle->value = ScopeTarget->alt().Degrees();
        }

        /* Could not find either properties! */
        if (EqProp == NULL && HorProp == NULL)
            return false;

        if (EqProp)
            clientManager->sendNewNumber(EqProp);
        if (HorProp)
            clientManager->sendNewNumber(HorProp);

        return true;

}

bool Telescope::Slew(SkyPoint *ScopeTarget)
{
    ISwitchVectorProperty *motionSP = baseDevice->getSwitch("ON_COORD_SET");
    if (motionSP == NULL)
        return false;

    ISwitch *slewSW = IUFindSwitch(motionSP, "SLEW");
    if (slewSW == NULL)
        slewSW = IUFindSwitch(motionSP, "TRACK");

    if (slewSW == NULL)
        return NULL;

    if (slewSW->s != ISS_ON)
    {
        IUResetSwitch(motionSP);
        slewSW->s = ISS_ON;
        clientManager->sendNewSwitch(motionSP);
    }

    return sendCoords(ScopeTarget);

}

bool Telescope::Sync(SkyPoint *ScopeTarget)
{
    ISwitchVectorProperty *motionSP = baseDevice->getSwitch("ON_COORD_SET");
    if (motionSP == NULL)
        return false;

    ISwitch *syncSW = IUFindSwitch(motionSP, "SYNC");
    if (syncSW == NULL)
        return NULL;

    if (syncSW->s != ISS_ON)
    {
        IUResetSwitch(motionSP);
        syncSW->s = ISS_ON;
        clientManager->sendNewSwitch(motionSP);
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
        return NULL;

     abortSW->s = ISS_ON;
     clientManager->sendNewSwitch(motionSP);

     return true;
}


bool Telescope::Park()
{
    ISwitchVectorProperty *motionSP = baseDevice->getSwitch("TELESCOPE_PARK");
    if (motionSP == NULL)
        return false;

    ISwitch *parkSW = IUFindSwitch(motionSP, "PARK");
    if (parkSW == NULL)
        return NULL;

     parkSW->s = ISS_ON;
     clientManager->sendNewSwitch(motionSP);

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


}
