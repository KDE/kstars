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

void DustCap::addDustCap(ISD::DustCap *device)
{
    // No duplicates
    for (auto &oneDustCap : m_DustCaps)
    {
        if (oneDustCap->getDeviceName() == device->getDeviceName())
            return;
    }

    for (auto &oneDustCap : m_DustCaps)
        oneDustCap->disconnect(this);

    m_DustCap = device;
    m_DustCaps.append(device);

    // TODO add method to select active dust cap
    connect(m_DustCap, &ISD::DustCap::propertyDefined, this, &DustCap::processProp);
    connect(m_DustCap, &ISD::DustCap::switchUpdated, this, &DustCap::processSwitch);
    connect(m_DustCap, &ISD::DustCap::numberUpdated, this, &DustCap::processNumber);
    connect(m_DustCap, &ISD::DustCap::newStatus, this, &DustCap::newStatus);
    connect(m_DustCap, &ISD::DustCap::ready, this, &DustCap::ready);
}

void DustCap::removeDevice(ISD::GenericDevice *device)
{
    device->disconnect(this);
    for (auto &oneDustCap : m_DustCaps)
    {
        if (oneDustCap->getDeviceName() == device->getDeviceName())
        {
            m_DustCap = nullptr;
            m_DustCaps.removeOne(oneDustCap);
            break;
        }
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
    if (m_DustCap == nullptr)
        return false;

    return m_DustCap->Park();
}

bool DustCap::unpark()
{
    if (m_DustCap == nullptr)
        return false;

    return m_DustCap->UnPark();
}

bool DustCap::canPark()
{
    if (m_DustCap == nullptr)
        return false;

    return m_DustCap->canPark();
}

bool DustCap::hasLight()
{
    if (m_DustCap == nullptr)
        return false;

    return m_DustCap->hasLight();
}

bool DustCap::setLightEnabled(bool enable)
{
    if (m_DustCap == nullptr)
        return false;

    return m_DustCap->SetLightEnabled(enable);
}

bool DustCap::setBrightness(uint16_t val)
{
    if (m_DustCap == nullptr)
        return false;

    return m_DustCap->SetBrightness(val);
}
}
