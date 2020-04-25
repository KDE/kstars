/*  INDI DustCap
    Copyright (C) 2015 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include <basedevice.h>
#include <KLocalizedString>
#include <QtDBus/qdbusmetatype.h>

#include "indicap.h"
#include "clientmanager.h"

namespace ISD
{

DustCap::DustCap(GDInterface *iPtr): DeviceDecorator(iPtr)
{
    dType = KSTARS_AUXILIARY;

    qRegisterMetaType<ISD::DustCap::Status>("ISD::DustCap::Status");
    qDBusRegisterMetaType<ISD::DustCap::Status>();

    readyTimer.reset(new QTimer());
    readyTimer.get()->setInterval(250);
    readyTimer.get()->setSingleShot(true);
    connect(readyTimer.get(), &QTimer::timeout, this, &DustCap::ready);
}

void DustCap::registerProperty(INDI::Property *prop)
{
    if (!prop->getRegistered())
        return;

    if (isConnected())
        readyTimer.get()->start();

    DeviceDecorator::registerProperty(prop);
}

void DustCap::processLight(ILightVectorProperty *lvp)
{
    DeviceDecorator::processLight(lvp);
}

void DustCap::processNumber(INumberVectorProperty *nvp)
{
    DeviceDecorator::processNumber(nvp);
}

void DustCap::processSwitch(ISwitchVectorProperty *svp)
{
    if (!strcmp(svp->name, "CAP_PARK"))
    {
        Status currentStatus = CAP_ERROR;

        switch (svp->s)
        {
            case IPS_IDLE:
                if (svp->sp[0].s == ISS_ON)
                    currentStatus = CAP_PARKED;
                else if (svp->sp[1].s == ISS_ON)
                    currentStatus = CAP_IDLE;
                break;

            case IPS_OK:
                if (svp->sp[0].s == ISS_ON)
                    currentStatus = CAP_PARKED;
                else
                    currentStatus = CAP_IDLE;
                break;

            case IPS_BUSY:
                if (svp->sp[0].s == ISS_ON)
                    currentStatus = CAP_PARKING;
                else
                    currentStatus = CAP_UNPARKING;
                break;

            case IPS_ALERT:
                currentStatus = CAP_ERROR;
        }

        if (currentStatus != m_Status)
        {
            m_Status = currentStatus;
            emit newStatus(m_Status);
        }
    }

    DeviceDecorator::processSwitch(svp);
}

void DustCap::processText(ITextVectorProperty *tvp)
{
    DeviceDecorator::processText(tvp);
}

bool DustCap::canPark()
{
    ISwitchVectorProperty *parkSP = baseDevice->getSwitch("CAP_PARK");

    if (parkSP == nullptr)
        return false;
    else
        return true;
}

bool DustCap::isParked()
{
    ISwitchVectorProperty *parkSP = baseDevice->getSwitch("CAP_PARK");

    if (parkSP == nullptr)
        return false;

    return (parkSP->s == IPS_OK && parkSP->sp[0].s == ISS_ON);
}

bool DustCap::isUnParked()
{
    ISwitchVectorProperty *parkSP = baseDevice->getSwitch("CAP_PARK");

    if (parkSP == nullptr)
        return false;

    return (parkSP->s == IPS_OK && parkSP->sp[1].s == ISS_ON);
}

bool DustCap::Park()
{
    ISwitchVectorProperty *parkSP = baseDevice->getSwitch("CAP_PARK");

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

bool DustCap::UnPark()
{
    ISwitchVectorProperty *parkSP = baseDevice->getSwitch("CAP_PARK");

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

bool DustCap::hasLight()
{
    ISwitchVectorProperty *lightSP = baseDevice->getSwitch("FLAT_LIGHT_CONTROL");

    if (lightSP == nullptr)
        return false;
    else
        return true;
}

bool DustCap::isLightOn()
{
    ISwitchVectorProperty *lightSP = baseDevice->getSwitch("FLAT_LIGHT_CONTROL");

    if (lightSP == nullptr)
        return false;

    ISwitch *lightON = IUFindSwitch(lightSP, "FLAT_LIGHT_ON");

    if (lightON == nullptr)
        return false;

    return (lightON->s == ISS_ON);
}

bool DustCap::SetLightEnabled(bool enable)
{
    ISwitchVectorProperty *lightSP = baseDevice->getSwitch("FLAT_LIGHT_CONTROL");

    if (lightSP == nullptr)
        return false;

    ISwitch *lightON  = IUFindSwitch(lightSP, "FLAT_LIGHT_ON");
    ISwitch *lightOFF = IUFindSwitch(lightSP, "FLAT_LIGHT_OFF");

    if (lightON == nullptr || lightOFF == nullptr)
        return false;

    IUResetSwitch(lightSP);

    if (enable)
        lightON->s = ISS_ON;
    else
        lightOFF->s = ISS_ON;

    clientManager->sendNewSwitch(lightSP);

    return true;
}

bool DustCap::SetBrightness(uint16_t val)
{
    INumberVectorProperty *lightIntensity = baseDevice->getNumber("FLAT_LIGHT_INTENSITY");
    if (lightIntensity)
    {
        lightIntensity->np[0].value = val;
        clientManager->sendNewNumber(lightIntensity);
        return true;
    }

    return false;
}

const QString DustCap::getStatusString(DustCap::Status status)
{
    switch (status)
    {
        case ISD::DustCap::CAP_IDLE:
            return i18n("Idle");

        case ISD::DustCap::CAP_PARKED:
            return i18n("Parked");

        case ISD::DustCap::CAP_PARKING:
            return i18n("Parking");

        case ISD::DustCap::CAP_UNPARKING:
            return i18n("UnParking");

        case ISD::DustCap::CAP_ERROR:
            return i18n("Error");
    }

    return i18n("Error");
}

}

QDBusArgument &operator<<(QDBusArgument &argument, const ISD::DustCap::Status &source)
{
    argument.beginStructure();
    argument << static_cast<int>(source);
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, ISD::DustCap::Status &dest)
{
    int a;
    argument.beginStructure();
    argument >> a;
    argument.endStructure();
    dest = static_cast<ISD::DustCap::Status>(a);
    return argument;
}
