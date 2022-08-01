/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "dome.h"

#include "domeadaptor.h"
#include "ekos/manager.h"
#include "indi/driverinfo.h"
#include "indi/clientmanager.h"
#include "kstars.h"
#include "ekos_debug.h"

#include <basedevice.h>

namespace Ekos
{
Dome::Dome()
{
    new DomeAdaptor(this);
    QDBusConnection::sessionBus().registerObject("/KStars/Ekos/Dome", this);

    m_Dome = nullptr;
}

void Dome::addDome(ISD::Dome *device)
{
    // No Duplicates
    for (auto &oneDome : m_Domes)
    {
        if (oneDome->getDeviceName() == device->getDeviceName())
            return;
    }

    for (auto &oneDome : m_Domes)
        oneDome->disconnect(this);

    m_Dome = device;
    m_Domes.append(device);

    // TODO find a way to select active dome.
    // Probably not required in 99.9% of situations but to be consistent.
    connect(m_Dome, &ISD::Dome::newStatus, this, &Dome::newStatus);
    connect(m_Dome, &ISD::Dome::newStatus, this, &Dome::setStatus);
    connect(m_Dome, &ISD::Dome::newParkStatus, [&](ISD::ParkStatus status)
    {
        m_ParkStatus = status;
        emit newParkStatus(status);
    });
    connect(m_Dome, &ISD::Dome::newShutterStatus, this, &Dome::newShutterStatus);
    connect(m_Dome, &ISD::Dome::newShutterStatus, [&](ISD::Dome::ShutterStatus status)
    {
        m_ShutterStatus = status;
    });
    connect(m_Dome, &ISD::Dome::newAutoSyncStatus, this, &Dome::newAutoSyncStatus);
    connect(m_Dome, &ISD::Dome::azimuthPositionChanged, this, &Dome::azimuthPositionChanged);
    connect(m_Dome, &ISD::Dome::ready, this, &Dome::ready);
    connect(m_Dome, &ISD::Dome::Disconnected, this, &Dome::disconnected);
}


bool Dome::canPark()
{
    if (m_Dome == nullptr)
        return false;

    return m_Dome->canPark();
}

bool Dome::park()
{
    if (m_Dome == nullptr || m_Dome->canPark() == false)
        return false;

    qCDebug(KSTARS_EKOS) << "Parking dome...";
    return m_Dome->Park();
}

bool Dome::unpark()
{
    if (m_Dome == nullptr || m_Dome->canPark() == false)
        return false;

    qCDebug(KSTARS_EKOS) << "Unparking dome...";
    return m_Dome->UnPark();
}

bool Dome::abort()
{
    if (m_Dome == nullptr)
        return false;

    qCDebug(KSTARS_EKOS) << "Aborting...";
    return m_Dome->Abort();
}

bool Dome::isMoving()
{
    if (m_Dome == nullptr)
        return false;

    return m_Dome->isMoving();
}

bool Dome::isRolloffRoof()
{
    // a rolloff roof is a dome that can move neither absolutely nor relatively
    return (m_Dome && !m_Dome->canAbsMove() && !m_Dome->canRelMove());
}

bool Dome::canAbsoluteMove()
{
    if (m_Dome)
        return m_Dome->canAbsMove();

    return false;
}

bool Dome::canRelativeMove()
{
    if (m_Dome)
        return m_Dome->canRelMove();

    return false;
}

double Dome::azimuthPosition()
{
    if (m_Dome)
        return m_Dome->azimuthPosition();
    return -1;
}

void Dome::setAzimuthPosition(double position)
{
    if (m_Dome)
        m_Dome->setAzimuthPosition(position);
}

void Dome::setRelativePosition(double position)
{
    if (m_Dome)
        m_Dome->setRelativePosition(position);
}

bool Dome::moveDome(bool moveCW, bool start)
{
    if (m_Dome == nullptr)
        return false;

    if (isRolloffRoof())
        qCDebug(KSTARS_EKOS) << (moveCW ? "Opening" : "Closing") << "rolloff roof" << (start ? "started." : "stopped.");
    else
        qCDebug(KSTARS_EKOS) << "Moving dome" << (moveCW ? "" : "counter") << "clockwise" << (start ? "started." : "stopped.");
    return m_Dome->moveDome(moveCW ? ISD::Dome::DOME_CW : ISD::Dome::DOME_CCW,
                            start  ? ISD::Dome::MOTION_START : ISD::Dome::MOTION_STOP);
}

bool Dome::isAutoSync()
{
    if (m_Dome)
        return m_Dome->isAutoSync();
    // value could not be determined
    return false;
}

bool Dome::setAutoSync(bool activate)
{
    if (m_Dome)
        return m_Dome->setAutoSync(activate);
    // not succeeded
    return false;
}

bool Dome::hasShutter()
{
    if (m_Dome)
        return m_Dome->hasShutter();
    // no dome, no shutter
    return false;
}

bool Dome::controlShutter(bool open)
{

    if (m_Dome)
    {
        qCDebug(KSTARS_EKOS) << (open ? "Opening" : "Closing") << " shutter...";
        return m_Dome->ControlShutter(open);
    }
    // no dome, no shutter control
    return false;
}

void Dome::removeDevice(ISD::GenericDevice *device)
{
    device->disconnect(this);
    for (auto &oneDome : m_Domes)
    {
        if (oneDome->getDeviceName() == device->getDeviceName())
        {
            m_Dome = nullptr;
            m_Domes.removeOne(oneDome);
            break;
        }

    }
}

void Dome::setStatus(ISD::Dome::Status status)
{
    // special case for rolloff roofs.
    if (isRolloffRoof())
    {
        // if a parked rolloff roof starts to move, its state changes to unparking
        // CW ==> Opening = Unparking
        if (status == ISD::Dome::DOME_MOVING_CW && (m_ParkStatus == ISD::PARK_PARKED || m_ParkStatus == ISD::PARK_PARKING))
        {
            m_ParkStatus = ISD::PARK_UNPARKING;
            qCDebug(KSTARS_EKOS) << "Unparking rolloff roof (status = " << status << ").";
            emit newParkStatus(m_ParkStatus);
        }
        // if a unparked rolloff roof starts to move, its state changes to parking
        // CCW ==> Closing = Parking
        else if (status == ISD::Dome::DOME_MOVING_CCW && (m_ParkStatus == ISD::PARK_UNPARKED
                 || m_ParkStatus == ISD::PARK_UNPARKING))
        {
            m_ParkStatus = ISD::PARK_PARKING;
            qCDebug(KSTARS_EKOS) << "Parking rolloff roof (status = " << status << ").";
            emit newParkStatus(m_ParkStatus);
        }
        else
        {
            qCDebug(KSTARS_EKOS) << "Rolloff roof status = " << status << ".";
        }
    }
    // in all other cases, do nothing
}

}
