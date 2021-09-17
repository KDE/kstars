/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <indi/indidevice.h>

#include "Options.h"

#include "clientmanagerlite.h"
#include "inditelescopelite.h"

#include "kstarslite.h"
#include "skymaplite.h"

TelescopeLite::TelescopeLite(INDI::BaseDevice *device)
    : minAlt(-1), maxAlt(-1), IsParked(false), clientManager(KStarsLite::Instance()->clientManagerLite()),
      baseDevice(device), slewRateIndex(0)
{
    setDeviceName(device->getDeviceName());
    //Whenever slew rate is changed (or once it was initialized) we update it
    connect(clientManager, &ClientManagerLite::newINDIProperty, this, &TelescopeLite::updateSlewRate);
    connect(clientManager, &ClientManagerLite::newINDISwitch, this, &TelescopeLite::updateSlewRate);
}

void TelescopeLite::updateSlewRate(const QString &deviceName, const QString &propName)
{
    if (deviceName == baseDevice->getDeviceName() && propName == "TELESCOPE_SLEW_RATE")
    {
        auto slewRateSP = baseDevice->getSwitch("TELESCOPE_SLEW_RATE");
        int index = 0;
        for (int i = 0; i < slewRateSP->count(); ++i)
        {
            if (slewRateSP->at(i).getState() == ISS_ON)
            {
                index = i;
                break;
            }
        }
        m_slewRateLabels.clear();
        for (int i = 0; i < slewRateSP->count(); ++i)
        {
            m_slewRateLabels.push_back(slewRateSP->at(i)->getLabel());
        }
        emit slewRateUpdate(index, m_slewRateLabels.size());
        setSlewRate(index);
    }
}

TelescopeLite::~TelescopeLite()
{
}

void TelescopeLite::setSlewDecreasable(bool slewDecreasable)
{
    if (m_slewDecreasable != slewDecreasable)
    {
        m_slewDecreasable = slewDecreasable;
        emit slewDecreasableChanged(slewDecreasable);
    }
}

void TelescopeLite::setSlewIncreasable(bool slewIncreasable)
{
    if (m_slewIncreasable != slewIncreasable)
    {
        m_slewIncreasable = slewIncreasable;
        emit slewIncreasableChanged(slewIncreasable);
    }
}

void TelescopeLite::setSlewRateLabel(const QString &slewRateLabel)
{
    if (m_slewRateLabel != slewRateLabel)
    {
        m_slewRateLabel = slewRateLabel;
        emit slewRateLabelChanged(slewRateLabel);
    }
}

void TelescopeLite::setDeviceName(const QString &deviceName)
{
    if (m_deviceName != deviceName)
    {
        m_deviceName = deviceName;
        emit deviceNameChanged(deviceName);
    }
}

void TelescopeLite::registerProperty(INDI::Property prop)
{
    if (prop->isNameMatch("TELESCOPE_INFO"))
    {
        auto ti = prop->getNumber();

        if (!ti)
            return;

        //        bool aperture_ok=false, focal_ok=false;
        //        double temp=0;
    }

    if (prop->isNameMatch("TELESCOPE_PARK"))
    {
        auto svp = prop->getSwitch();

        if (svp)
        {
            auto sp = svp->findWidgetByName("PARK");
            if (sp)
            {
                IsParked = ((sp->getState() == ISS_ON) && svp->getState() == IPS_OK);
            }
        }
    }
}

void TelescopeLite::processNumber(INumberVectorProperty *nvp)
{
    if (!strcmp(nvp->name, "EQUATORIAL_EOD_COORD"))
    {
        INumber *RA  = IUFindNumber(nvp, "RA");
        INumber *DEC = IUFindNumber(nvp, "DEC");

        if (RA == nullptr || DEC == nullptr)
            return;

        currentCoord.setRA(RA->value);
        currentCoord.setDec(DEC->value);

        //KStarsLite::Instance()->map()->update();

        //emit numberUpdated(nvp);

        return;
    }

    if (!strcmp(nvp->name, "EQUATORIAL_COORD"))
    {
        INumber *RA  = IUFindNumber(nvp, "RA");
        INumber *DEC = IUFindNumber(nvp, "DEC");

        if (RA == nullptr || DEC == nullptr)
            return;

        currentCoord.setRA0(RA->value);
        currentCoord.setDec0(DEC->value);
        currentCoord.apparentCoord(static_cast<long double>(J2000), KStarsLite::Instance()->data()->ut().djd());

        //KStarsLite::Instance()->map()->update();

        //emit numberUpdated(nvp);

        return;
    }

    if (!strcmp(nvp->name, "HORIZONTAL_COORD"))
    {
        INumber *Az  = IUFindNumber(nvp, "AZ");
        INumber *Alt = IUFindNumber(nvp, "ALT");

        if (Az == nullptr || Alt == nullptr)
            return;

        currentCoord.setAz(Az->value);
        currentCoord.setAlt(Alt->value);
        currentCoord.HorizontalToEquatorial(KStarsLite::Instance()->data()->lst(),
                                            KStarsLite::Instance()->data()->geo()->lat());

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
    auto raPulse  = baseDevice->getNumber("TELESCOPE_TIMED_GUIDE_WE");
    auto decPulse = baseDevice->getNumber("TELESCOPE_TIMED_GUIDE_NS");

    return raPulse && decPulse;
}

bool TelescopeLite::canSync()
{
    auto motionSP = baseDevice->getSwitch("ON_COORD_SET");

    if (!motionSP)
        return false;

    auto syncSW = motionSP->findWidgetByName("SYNC");

    return (syncSW != nullptr);
}

bool TelescopeLite::canPark()
{
    auto parkSP = baseDevice->getSwitch("TELESCOPE_PARK");

    if (!parkSP)
        return false;

    auto parkSW = parkSP->findWidgetByName("PARK");

    return (parkSW != nullptr);
}

bool TelescopeLite::isSlewing()
{
    auto EqProp = baseDevice->getNumber("EQUATORIAL_EOD_COORD");
    if (!EqProp)
        return false;

    return (EqProp->getState() == IPS_BUSY);
}

bool TelescopeLite::isInMotion()
{
    bool inMotion                     = false;
    bool inSlew                       = isSlewing();

    auto movementSP = baseDevice->getSwitch("TELESCOPE_MOTION_NS");
    if (movementSP)
        inMotion = (movementSP->getState() == IPS_BUSY);

    movementSP = baseDevice->getSwitch("TELESCOPE_MOTION_WE");
    if (movementSP)
        inMotion = ((movementSP->getState() == IPS_BUSY) || inMotion);

    return (inSlew || inMotion);
}

bool TelescopeLite::sendCoords(SkyPoint *ScopeTarget)
{
    INumber *RAEle                 = nullptr;
    INumber *DecEle                = nullptr;
    INumber *AzEle                 = nullptr;
    INumber *AltEle                = nullptr;
    double currentRA = 0, currentDEC = 0, currentAlt = 0, currentAz = 0, targetAlt = 0;
    bool useJ2000(false);

    auto EqProp = baseDevice->getNumber("EQUATORIAL_EOD_COORD");
    if (!EqProp)
    {
        // J2000 Property
        EqProp = baseDevice->getNumber("EQUATORIAL_COORD");
        if (EqProp)
            useJ2000 = true;
    }

    auto HorProp = baseDevice->getNumber("HORIZONTAL_COORD");

    if (EqProp && EqProp->getPermission() == IP_RO)
        EqProp = nullptr;

    if (HorProp && HorProp->getPermission() == IP_RO)
        HorProp = nullptr;

    //qDebug() << "Skymap click - RA: " << scope_target->ra().toHMSString() << " DEC: " << scope_target->dec().toDMSString();

    if (EqProp)
    {
        RAEle = IUFindNumber(EqProp, "RA");
        if (!RAEle)
            return false;
        DecEle = IUFindNumber(EqProp, "DEC");
        if (!DecEle)
            return false;

        //if (useJ2000)
            //ScopeTarget->apparentCoord( KStars::Instance()->data()->ut().djd(), static_cast<long double>(J2000));

        currentRA  = RAEle->value;
        currentDEC = DecEle->value;

        ScopeTarget->EquatorialToHorizontal(KStarsData::Instance()->lst(), KStarsData::Instance()->geo()->lat());
    }

    if (HorProp)
    {
        AzEle = IUFindNumber(HorProp, "AZ");
        if (!AzEle)
            return false;
        AltEle = IUFindNumber(HorProp, "ALT");
        if (!AltEle)
            return false;

        currentAz  = AzEle->value;
        currentAlt = AltEle->value;
    }

    /* Could not find either properties! */
    if (EqProp == nullptr && HorProp == nullptr)
        return false;

    //targetAz = ScopeTarget->az().Degrees();
    targetAlt = ScopeTarget->altRefracted().Degrees();
    if (targetAlt < 0)
    {
       return false;
    }

    if (EqProp)
    {
        dms ra, de;

        if (useJ2000)
        {
            // If we have invalid DEC, then convert coords to J2000
            if (ScopeTarget->dec0().Degrees() == 180.0)
            {
               ScopeTarget->setRA0(ScopeTarget->ra());
               ScopeTarget->setDec0(ScopeTarget->dec());
               ScopeTarget->apparentCoord( KStarsLite::Instance()->data()->ut().djd(), static_cast<long double>(J2000));
               ra = ScopeTarget->ra();
               de = ScopeTarget->dec();
            }
            else
            {
                ra = ScopeTarget->ra0();
                de = ScopeTarget->dec0();
            }
        }
        else
        {
            ra = ScopeTarget->ra();
            de = ScopeTarget->dec();
        }

        RAEle->value  = ra.Hours();
        DecEle->value = de.Degrees();
        clientManager->sendNewNumber(EqProp);

        RAEle->value  = currentRA;
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
    auto motionSP = baseDevice->getSwitch("ON_COORD_SET");

    if (!motionSP)
        return false;

    auto slewSW = motionSP->findWidgetByName("TRACK");

    if (!slewSW)
        slewSW = motionSP->findWidgetByName("SLEW");

    if (!slewSW)
        return false;

    if (slewSW->setState() != ISS_ON)
    {
        motionSP->reset();
        slewSW->setState(ISS_ON);
        clientManager->sendNewSwitch(motionSP);

        if (Options::iNDILogging())
            qDebug() << "ISD:Telescope: " << slewSW->getName();
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
    auto motionSP = baseDevice->getSwitch("ON_COORD_SET");

    if (!motionSP)
        return false;

    auto syncSW = motionSP->findWidgetByName("SYNC");

    if (!syncSW)
        return false;

    if (syncSW->getState() != ISS_ON)
    {
        motionSP->reset();
        syncSW->setState(ISS_ON);
        clientManager->sendNewSwitch(motionSP);

        if (Options::iNDILogging())
            qDebug() << "ISD:Telescope: Syncing...";
    }

    return sendCoords(ScopeTarget);
}

bool TelescopeLite::abort()
{
    auto motionSP = baseDevice->getSwitch("TELESCOPE_ABORT_MOTION");

    if (!motionSP)
        return false;

    auto abortSW = motionSP->findWidgetByName("ABORT");

    if (!abortSW)
        return false;

    if (Options::iNDILogging())
        qDebug() << "ISD:Telescope: Aborted." << endl;

    abortSW->setState(ISS_ON);
    clientManager->sendNewSwitch(motionSP);

    return true;
}

bool TelescopeLite::park()
{
    auto parkSP = baseDevice->getSwitch("TELESCOPE_PARK");

    if (!parkSP)
        return false;

    auto parkSW = parkSP->findWidgetByName("PARK");

    if (!parkSW)
        return false;

    if (Options::iNDILogging())
        qDebug() << "ISD:Telescope: Parking..." << endl;

    parkSP->reset();
    parkSW->setState(ISS_ON);
    clientManager->sendNewSwitch(parkSP);

    return true;
}

bool TelescopeLite::unPark()
{
    auto parkSP = baseDevice->getSwitch("TELESCOPE_PARK");

    if (!parkSP)
        return false;

    auto parkSW = parkSP->findWidgetByName("UNPARK");

    if (!parkSW)
        return false;

    if (Options::iNDILogging())
        qDebug() << "ISD:Telescope: UnParking..." << endl;

    parkSP->reset();
    parkSW->setState(ISS_ON);
    clientManager->sendNewSwitch(parkSP);

    return true;
}

bool TelescopeLite::getEqCoords(double *ra, double *dec)
{
    auto EqProp = baseDevice->getNumber("EQUATORIAL_EOD_COORD");

    if (!EqProp)
        return false;

    auto RAEle = EqProp->findWidgetByName("RA");
    if (!RAEle)
        return false;

    auto DecEle = EqProp->findWidgetByName("DEC");
    if (!DecEle)
        return false;

    *ra  = RAEle->getValue();
    *dec = DecEle->getValue();

    return true;
}

bool TelescopeLite::moveNS(TelescopeMotionNS dir, TelescopeMotionCommand cmd)
{
    auto motionSP = baseDevice->getSwitch("TELESCOPE_MOTION_NS");

    if (!motionSP)
        return false;

    auto motionNorth = motionSP->findWidgetByName("MOTION_NORTH");
    auto motionSouth = motionSP->findWidgetByName("MOTION_SOUTH");

    if (!motionNorth || !motionSouth)
        return false;

    // If same direction, return
    if (dir == MOTION_NORTH && motionNorth->getState() == ((cmd == MOTION_START) ? ISS_ON : ISS_OFF))
        return true;

    if (dir == MOTION_SOUTH && motionSouth->getState() == ((cmd == MOTION_START) ? ISS_ON : ISS_OFF))
        return true;

    motionSP->reset();

    if (cmd == MOTION_START)
    {
        if (dir == MOTION_NORTH)
            motionNorth->setState(ISS_ON);
        else
            motionSouth->setState(ISS_ON);
    }

    if (cmd == MOTION_STOP)
    {
        if (dir == MOTION_NORTH)
            motionNorth->setState(ISS_OFF);
        else
            motionSouth->setState(ISS_OFF);
    }

    clientManager->sendNewSwitch(motionSP);

    return true;
}

bool TelescopeLite::moveWE(TelescopeMotionWE dir, TelescopeMotionCommand cmd)
{
    auto motionSP = baseDevice->getSwitch("TELESCOPE_MOTION_WE");

    if (motionSP == nullptr)
        return false;

    auto motionWest = motionSP->findWidgetByName("MOTION_WEST");
    auto motionEast = motionSP->findWidgetByName("MOTION_EAST");

    if (motionWest == nullptr || motionEast == nullptr)
        return false;

    // If same direction, return
    if (dir == MOTION_WEST && motionWest->getState() == ((cmd == MOTION_START) ? ISS_ON : ISS_OFF))
        return true;

    if (dir == MOTION_EAST && motionEast->getState() == ((cmd == MOTION_START) ? ISS_ON : ISS_OFF))
        return true;

    IUResetSwitch(motionSP);

    if (cmd == MOTION_START)
    {
        if (dir == MOTION_WEST)
            motionWest->setState(ISS_ON);
        else
            motionEast->setState(ISS_ON);
    }

    if (cmd == MOTION_STOP)
    {
        if (dir == MOTION_WEST)
            motionWest->setState(ISS_OFF);
        else
            motionEast->setState(ISS_OFF);
    }

    clientManager->sendNewSwitch(motionSP);

    return true;
}

bool TelescopeLite::setSlewRate(int index)
{
    auto slewRateSP = baseDevice->getSwitch("TELESCOPE_SLEW_RATE");

    if (slewRateSP == nullptr)
        return false;

    int maxSlewRate = slewRateSP->count();

    if (index < 0)
    {
        index = 0;
    }
    else if (index >= maxSlewRate)
    {
        index = maxSlewRate - 1;
    }

    if (slewRateSP->at(index)->getState() != ISS_ON || index != slewRateIndex)
    {
        slewRateSP->reset();

        slewRateSP->at(index)->setState(ISS_ON);

        slewRateIndex = index;
        setSlewRateLabel(slewRateSP->at(index)->getLabel());
        setSlewDecreasable(index != 0);
        setSlewIncreasable(index != maxSlewRate - 1);

        clientManager->sendNewSwitch(slewRateSP);
    }

    return true;
}

bool TelescopeLite::decreaseSlewRate()
{
    return setSlewRate(slewRateIndex - 1);
}

bool TelescopeLite::increaseSlewRate()
{
    return setSlewRate(slewRateIndex + 1);
}

void TelescopeLite::setAltLimits(double minAltitude, double maxAltitude)
{
    minAlt = minAltitude;
    maxAlt = maxAltitude;
}

bool TelescopeLite::isParked()
{
    return IsParked;
}
