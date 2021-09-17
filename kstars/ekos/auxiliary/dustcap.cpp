/*
    SPDX-FileCopyrightText: 2015 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "dustcap.h"

#include "dustcapadaptor.h"
#include "ekos/manager.h"
#include "kstars.h"

#include <basedevice.h>

namespace Ekos
{
DustCap::DustCap()
{
    new DustCapAdaptor(this);
    QDBusConnection::sessionBus().registerObject("/KStars/Ekos/DustCap", this);
}

void DustCap::setDustCap(ISD::GDInterface *newDustCap)
{
    if (newDustCap == currentDustCap)
        return;

    currentDustCap = static_cast<ISD::DustCap *>(newDustCap);

    currentDustCap->disconnect(this);

    connect(currentDustCap, &ISD::GDInterface::propertyDefined, this, &DustCap::processProp);
    connect(currentDustCap, &ISD::GDInterface::switchUpdated, this, &DustCap::processSwitch);
    connect(currentDustCap, &ISD::GDInterface::numberUpdated, this, &DustCap::processNumber);
    connect(currentDustCap, &ISD::DustCap::newStatus, this, &DustCap::newStatus);
    connect(currentDustCap, &ISD::DustCap::ready, this, &DustCap::ready);
}

void DustCap::removeDevice(ISD::GDInterface *device)
{
    device->disconnect(this);
    if (currentDustCap && (currentDustCap->getDeviceName() == device->getDeviceName()))
    {
        currentDustCap = nullptr;
    }
}

void DustCap::processProp(INDI::Property prop)
{
    if (!prop->getRegistered())
        return;

    if (prop->isNameMatch("FLAT_LIGHT_CONTROL"))
    {
        auto svp = prop->getSwitch();
        if ((svp->at(0)->getState() == ISS_ON) != m_LightEnabled)
        {
            m_LightEnabled = (svp->at(0)->getState() == ISS_ON);
            emit lightToggled(m_LightEnabled);
        }
    }
    else if (prop->isNameMatch("FLAT_LIGHT_INTENSITY"))
    {
        auto nvp = prop->getNumber();
        uint16_t newIntensity = static_cast<uint16_t>(nvp->at(0)->getValue());
        if (newIntensity != m_lightIntensity)
        {
            m_lightIntensity = newIntensity;
            emit lightIntensityChanged(m_lightIntensity);
        }
    }
}
void DustCap::processSwitch(ISwitchVectorProperty *svp)
{
    if (!strcmp(svp->name, "CAP_PARK"))
    {
        ISD::ParkStatus newStatus = ISD::PARK_UNKNOWN;

        switch (svp->s)
        {
            case IPS_IDLE:
                if (svp->sp[0].s == ISS_ON)
                    newStatus = ISD::PARK_PARKED;
                else if (svp->sp[1].s == ISS_ON)
                    newStatus = ISD::PARK_UNPARKED;
                else
                    newStatus = ISD::PARK_UNKNOWN;
                break;

            case IPS_OK:
                if (svp->sp[0].s == ISS_ON)
                    newStatus = ISD::PARK_PARKED;
                else
                    newStatus = ISD::PARK_UNPARKED;
                break;

            case IPS_BUSY:
                if (svp->sp[0].s == ISS_ON)
                    newStatus = ISD::PARK_PARKING;
                else
                    newStatus = ISD::PARK_UNPARKING;
                break;

            case IPS_ALERT:
                newStatus = ISD::PARK_ERROR;
        }

        if (newStatus != m_ParkStatus)
        {
            m_ParkStatus = newStatus;
            emit newParkStatus(newStatus);
        }
    }
    else if (!strcmp(svp->name, "FLAT_LIGHT_CONTROL"))
    {
        if ((svp->sp[0].s == ISS_ON) != m_LightEnabled)
        {
            m_LightEnabled = (svp->sp[0].s == ISS_ON);
            emit lightToggled(m_LightEnabled);
        }
    }
}

void DustCap::processNumber(INumberVectorProperty *nvp)
{
    if (!strcmp(nvp->name, "FLAT_LIGHT_INTENSITY"))
    {
        uint16_t newIntensity = static_cast<uint16_t>(nvp->np[0].value);
        if (newIntensity != m_lightIntensity)
        {
            m_lightIntensity = newIntensity;
            emit lightIntensityChanged(m_lightIntensity);
        }
    }
}

bool DustCap::park()
{
    if (currentDustCap == nullptr)
        return false;

    return currentDustCap->Park();
}

bool DustCap::unpark()
{
    if (currentDustCap == nullptr)
        return false;

    return currentDustCap->UnPark();
}

bool DustCap::canPark()
{
    if (currentDustCap == nullptr)
        return false;

    return currentDustCap->canPark();
}

bool DustCap::hasLight()
{
    if (currentDustCap == nullptr)
        return false;

    return currentDustCap->hasLight();
}

bool DustCap::setLightEnabled(bool enable)
{
    if (currentDustCap == nullptr)
        return false;

    return currentDustCap->SetLightEnabled(enable);
}

bool DustCap::setBrightness(uint16_t val)
{
    if (currentDustCap == nullptr)
        return false;

    return currentDustCap->SetBrightness(val);
}
}
