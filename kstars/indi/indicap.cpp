/*
    SPDX-FileCopyrightText: 2015 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
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

void DustCap::registerProperty(INDI::Property prop)
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
    auto parkSP = baseDevice->getSwitch("CAP_PARK");
    if (!parkSP)
        return false;
    else
        return true;
}

bool DustCap::isParked()
{
    auto parkSP = baseDevice->getSwitch("CAP_PARK");
    if (!parkSP)
        return false;

    return ((parkSP->getState() == IPS_OK || parkSP->getState() == IPS_IDLE) && parkSP->at(0)->getState() == ISS_ON);
}

bool DustCap::isUnParked()
{
    auto parkSP = baseDevice->getSwitch("CAP_PARK");
    if (!parkSP)
        return false;

    return ( (parkSP->getState() == IPS_OK || parkSP->getState() == IPS_IDLE) && parkSP->at(1)->getState() == ISS_ON);
}

bool DustCap::Park()
{
    auto parkSP = baseDevice->getSwitch("CAP_PARK");
    if (!parkSP)
        return false;

    auto parkSW = parkSP->findWidgetByName("PARK");
    if (!parkSW)
        return false;

    parkSP->reset();
    parkSW->setState(ISS_ON);
    clientManager->sendNewSwitch(parkSP);

    return true;
}

bool DustCap::UnPark()
{
    auto parkSP = baseDevice->getSwitch("CAP_PARK");
    if (!parkSP)
        return false;

    auto parkSW = parkSP->findWidgetByName("UNPARK");
    if (!parkSW)
        return false;

    parkSP->reset();
    parkSW->setState(ISS_ON);
    clientManager->sendNewSwitch(parkSP);

    return true;
}

bool DustCap::hasLight()
{
    auto lightSP = baseDevice->getSwitch("FLAT_LIGHT_CONTROL");
    if (!lightSP)
        return false;
    else
        return true;
}

bool DustCap::isLightOn()
{
    auto lightSP = baseDevice->getSwitch("FLAT_LIGHT_CONTROL");
    if (!lightSP)
        return false;

    auto lightON = lightSP->findWidgetByName("FLAT_LIGHT_ON");
    if (!lightON)
        return false;

    return lightON->getState() == ISS_ON;
}

bool DustCap::SetLightEnabled(bool enable)
{
    auto lightSP = baseDevice->getSwitch("FLAT_LIGHT_CONTROL");

    if (!lightSP)
        return false;

    auto lightON  = lightSP->findWidgetByName("FLAT_LIGHT_ON");
    auto lightOFF = lightSP->findWidgetByName("FLAT_LIGHT_OFF");

    if (!lightON || !lightOFF)
        return false;

    lightSP->reset();

    if (enable)
        lightON->setState(ISS_ON);
    else
        lightOFF->setState(ISS_ON);

    clientManager->sendNewSwitch(lightSP);

    return true;
}

bool DustCap::SetBrightness(uint16_t val)
{
    auto lightIntensity = baseDevice->getNumber("FLAT_LIGHT_INTENSITY");
    if (!lightIntensity)
        return false;

    lightIntensity->at(0)->setValue(val);
    clientManager->sendNewNumber(lightIntensity);
    return true;
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
