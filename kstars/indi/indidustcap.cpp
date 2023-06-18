/*
    SPDX-FileCopyrightText: 2015 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <basedevice.h>
#include <KLocalizedString>
#include <qdbusmetatype.h>

#include "indidustcap.h"
#include "dustcapadaptor.h"

namespace ISD
{

const QList<const char *> DustCap::capStates = { I18N_NOOP("Idle"), I18N_NOOP("Parking"), I18N_NOOP("UnParking"),
                                                 I18N_NOOP("Parked"), I18N_NOOP("Error")
                                               };

DustCap::DustCap(GenericDevice *parent): ConcreteDevice(parent)
{
    qRegisterMetaType<ISD::DustCap::Status>("ISD::DustCap::Status");
    qDBusRegisterMetaType<ISD::DustCap::Status>();

    new DustCapAdaptor(this);
    m_DBusObjectPath = QString("/KStars/INDI/DustCap/%1").arg(getID());
    QDBusConnection::sessionBus().registerObject(m_DBusObjectPath, this);
}

void DustCap::processSwitch(INDI::Property prop)
{
    auto svp = prop.getSwitch();
    if (svp->isNameMatch("CAP_PARK"))
    {
        Status currentStatus = CAP_ERROR;
        ParkStatus currentParkStatus = PARK_UNKNOWN;

        switch (svp->getState())
        {
            case IPS_IDLE:
                if (svp->at(0)->getState() == ISS_ON)
                {
                    currentStatus = CAP_PARKED;
                    currentParkStatus = PARK_PARKED;
                }
                else if (svp->at(1)->getState() == ISS_ON)
                {
                    currentStatus = CAP_IDLE;
                    currentParkStatus = PARK_UNPARKED;
                }
                break;

            case IPS_OK:
                if (svp->at(0)->getState() == ISS_ON)
                {
                    currentStatus = CAP_PARKED;
                    currentParkStatus = PARK_PARKED;
                }
                else
                {
                    currentStatus = CAP_IDLE;
                    currentParkStatus = PARK_UNPARKED;
                }
                break;

            case IPS_BUSY:
                if (svp->at(0)->getState() == ISS_ON)
                {
                    currentStatus = CAP_PARKING;
                    currentParkStatus = PARK_PARKING;
                }
                else
                {
                    currentStatus = CAP_UNPARKING;
                    currentParkStatus = PARK_UNPARKING;
                }
                break;

            case IPS_ALERT:
                currentStatus = CAP_ERROR;
                currentParkStatus = PARK_ERROR;
        }

        if (currentStatus != m_Status)
        {
            m_Status = currentStatus;
            emit newStatus(m_Status);
        }

        if (currentParkStatus != m_ParkStatus)
        {
            m_ParkStatus = currentParkStatus;
            emit newParkStatus(m_ParkStatus);
        }


    }
}

bool DustCap::canPark()
{
    auto parkSP = getSwitch("CAP_PARK");
    if (!parkSP)
        return false;
    else
        return true;
}

bool DustCap::isParked()
{
    auto parkSP = getSwitch("CAP_PARK");
    if (!parkSP)
        return false;

    return ((parkSP->getState() == IPS_OK || parkSP->getState() == IPS_IDLE) && parkSP->at(0)->getState() == ISS_ON);
}

bool DustCap::isUnParked()
{
    auto parkSP = getSwitch("CAP_PARK");
    if (!parkSP)
        return false;

    return ( (parkSP->getState() == IPS_OK || parkSP->getState() == IPS_IDLE) && parkSP->at(1)->getState() == ISS_ON);
}

bool DustCap::park()
{
    auto parkSP = getSwitch("CAP_PARK");
    if (!parkSP)
        return false;

    auto parkSW = parkSP->findWidgetByName("PARK");
    if (!parkSW)
        return false;

    parkSP->reset();
    parkSW->setState(ISS_ON);
    sendNewProperty(parkSP);

    return true;
}

bool DustCap::unpark()
{
    auto parkSP = getSwitch("CAP_PARK");
    if (!parkSP)
        return false;

    auto parkSW = parkSP->findWidgetByName("UNPARK");
    if (!parkSW)
        return false;

    parkSP->reset();
    parkSW->setState(ISS_ON);
    sendNewProperty(parkSP);

    return true;
}

const QString DustCap::getStatusString(DustCap::Status status, bool translated)
{
    return translated ? i18n(capStates[status]) : capStates[status];
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
