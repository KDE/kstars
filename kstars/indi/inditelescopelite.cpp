/*  INDI CCD
    Copyright (C) 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include <indi/indidevice.h>

#include "Options.h"

#include "clientmanagerlite.h"
#include "inditelescopelite.h"

#include "kstarslite.h"
#include "skymaplite.h"

TelescopeLite::TelescopeLite(INDI::BaseDevice *device)
    :minAlt(-1), maxAlt(-1), IsParked(false), clientManager(KStarsLite::Instance()->clientManagerLite()),
      baseDevice(device), slewRateIndex(0)
{
    setDeviceName(device->getDeviceName());
    //Whenever slew rate is changed (or once it was initialized) we update it
    connect(clientManager, &ClientManagerLite::newINDIProperty, this, &TelescopeLite::updateSlewRate);
    connect(clientManager, &ClientManagerLite::newINDISwitch, this, &TelescopeLite::updateSlewRate);
}

void TelescopeLite::updateSlewRate(QString deviceName, QString propName) {
    if(deviceName == baseDevice->getDeviceName() && propName == "TELESCOPE_SLEW_RATE") {
        ISwitchVectorProperty *slewRateSP = baseDevice->getSwitch("TELESCOPE_SLEW_RATE");
        int index = 0;
        for(int i = 0; i < slewRateSP->nsp; ++i) {
            if(slewRateSP->sp[i].s == ISS_ON) {
                index = i;
                break;
            }
        }
        setSlewRate(index);
    }
}

TelescopeLite::~TelescopeLite()
{

}

void TelescopeLite::setSlewDecreasable(bool slewDecreasable) {
    if(m_slewDecreasable != slewDecreasable) {
        m_slewDecreasable = slewDecreasable;
        emit slewDecreasableChanged(slewDecreasable);
    }
}

void TelescopeLite::setSlewIncreasable(bool slewIncreasable) {
    if(m_slewIncreasable != slewIncreasable) {
        m_slewIncreasable = slewIncreasable;
        emit slewIncreasableChanged(slewIncreasable);
    }
}

void TelescopeLite::setSlewRateLabel(QString slewRateLabel) {
    if(m_slewRateLabel != slewRateLabel) {
        m_slewRateLabel = slewRateLabel;
        emit slewRateLabelChanged(slewRateLabel);
    }
}

void TelescopeLite::setDeviceName(QString deviceName) {
    if(m_deviceName != deviceName) {
        m_deviceName = deviceName;
        emit deviceNameChanged(deviceName);
    }
}

void TelescopeLite::registerProperty(INDI::Property *prop)
{
    if (!strcmp(prop->getName(), "TELESCOPE_INFO"))
    {
        INumberVectorProperty *ti = prop->getNumber();
        if (ti == NULL)
            return;

        //        bool aperture_ok=false, focal_ok=false;
        //        double temp=0;
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
}

void TelescopeLite::processNumber(INumberVectorProperty *nvp)
{

    if (!strcmp(nvp->name, "EQUATORIAL_EOD_COORD"))
    {
        INumber *RA = IUFindNumber(nvp, "RA");
        INumber *DEC = IUFindNumber(nvp, "DEC");
        if (RA == NULL || DEC == NULL)
            return;

        currentCoord.setRA(RA->value);
        currentCoord.setDec(DEC->value);

        //KStarsLite::Instance()->map()->update();

        //emit numberUpdated(nvp);

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
        currentCoord.HorizontalToEquatorial( KStarsLite::Instance()->data()->lst(), KStarsLite::Instance()->data()->geo()->lat() );

        //KStarsLite::Instance()->map()->update();

        //emit numberUpdated(nvp);

        return;
    }

    //DeviceDecorator::processNumber(nvp);

}

void TelescopeLite::processSwitch(ISwitchVectorProperty *svp)
{
    Q_UNUSED(svp);
    /*if (!strcmp(svp->name, "TELESCOPE_PARK"))
    {
        ISwitch *sp = IUFindSwitch(svp, "PARK");
        if (sp)
        {
            IsParked = ( (sp->s == ISS_ON) && svp->s == IPS_OK);
        }

        //emit switchUpdated(svp);

        return;

    }*/
}

bool TelescopeLite::canGuide()
{
    INumberVectorProperty *raPulse  = baseDevice->getNumber("TELESCOPE_TIMED_GUIDE_WE");
    INumberVectorProperty *decPulse = baseDevice->getNumber("TELESCOPE_TIMED_GUIDE_NS");

    if (raPulse && decPulse)
        return true;
    else
        return false;
}

bool TelescopeLite::canSync()
{
    ISwitchVectorProperty *motionSP = baseDevice->getSwitch("ON_COORD_SET");
    if (motionSP == NULL)
        return false;

    ISwitch *syncSW = IUFindSwitch(motionSP, "SYNC");

    return (syncSW != NULL);
}

bool TelescopeLite::canPark()
{
    ISwitchVectorProperty *parkSP = baseDevice->getSwitch("TELESCOPE_PARK");
    if (parkSP == NULL)
        return false;

    ISwitch *parkSW = IUFindSwitch(parkSP, "PARK");

    return (parkSW != NULL);
}

bool TelescopeLite::isSlewing()
{
    INumberVectorProperty *EqProp(NULL);

    EqProp = baseDevice->getNumber("EQUATORIAL_EOD_COORD");
    if (EqProp == NULL)
        return false;

    return (EqProp->s == IPS_BUSY);
}

bool TelescopeLite::isInMotion()
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

bool TelescopeLite::sendCoords(SkyPoint *ScopeTarget)
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
            ScopeTarget->apparentCoord(KStarsLite::Instance()->data()->ut().djd(), (long double) J2000);

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
            //KMessageBox::error(NULL, i18n("Requested altitude %1 is outside the specified altitude limit boundary (%2,%3).", QString::number(targetAlt, 'g', 3), QString::number(minAlt, 'g', 3), QString::number(maxAlt, 'g', 3)),
            //                 i18n("Telescope Motion"));
            return false;
        }
    }

    if (targetAlt < 0)
    {
        if (false/*KMessageBox::warningContinueCancel(NULL, i18n("Requested altitude is below the horizon. Are you sure you want to proceed?"), i18n("Telescope Motion"),
                                                                                   KStandardGuiItem::cont(), KStandardGuiItem::cancel(), QString("telescope_coordintes_below_horizon_warning")) == KMessageBox::Cancel*/)
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

bool TelescopeLite::slew(double ra, double dec)
{
    SkyPoint target;

    target.setRA(ra);
    target.setDec(dec);

    return slew(&target);
}

bool TelescopeLite::slew(SkyPoint *ScopeTarget)
{
    abort();
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

bool TelescopeLite::sync(double ra, double dec)
{
    SkyPoint target;

    target.setRA(ra);
    target.setDec(dec);

    return sync(&target);
}

bool TelescopeLite::sync(SkyPoint *ScopeTarget)
{
    abort();
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

bool TelescopeLite::abort()
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


bool TelescopeLite::park()
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

bool TelescopeLite::unPark()
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

bool TelescopeLite::getEqCoords(double *ra, double *dec)
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

bool TelescopeLite::moveNS(TelescopeMotionNS dir, TelescopeMotionCommand cmd)
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

bool TelescopeLite::moveWE(TelescopeMotionWE dir, TelescopeMotionCommand cmd)
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

bool TelescopeLite::setSlewRate(int index)
{
    ISwitchVectorProperty *slewRateSP = baseDevice->getSwitch("TELESCOPE_SLEW_RATE");
    if (slewRateSP == NULL) {
        return false;
    }

    int maxSlewRate = slewRateSP->nsp;

    if (index < 0) {
        index = 0;
    } else if( index >= maxSlewRate) {
        index = maxSlewRate - 1;
    }

    if(slewRateSP->sp[index].s != ISS_ON || index != slewRateIndex) {
        IUResetSwitch(slewRateSP);

        slewRateSP->sp[index].s = ISS_ON;

        slewRateIndex = index;
        setSlewRateLabel(slewRateSP->sp[index].label);
        setSlewDecreasable(index != 0);
        setSlewIncreasable(index != maxSlewRate - 1);

        clientManager->sendNewSwitch(slewRateSP);
    }

    return true;
}

bool TelescopeLite::decreaseSlewRate() {
    return setSlewRate(slewRateIndex - 1);
}

bool TelescopeLite::increaseSlewRate() {
    return setSlewRate(slewRateIndex + 1);
}

void TelescopeLite::setAltLimits(double minAltitude, double maxAltitude)
{
    minAlt=minAltitude;
    maxAlt=maxAltitude;
}

bool TelescopeLite::isParked()
{
    return IsParked;
}
