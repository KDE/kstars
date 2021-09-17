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

    currentDome = nullptr;
}

void Dome::setDome(ISD::GDInterface *newDome)
{
    if (newDome == currentDome)
        return;

    currentDome = static_cast<ISD::Dome *>(newDome);

    currentDome->disconnect(this);

    connect(currentDome, &ISD::Dome::newStatus, this, &Dome::newStatus);
    connect(currentDome, &ISD::Dome::newStatus, this, &Dome::setStatus);
    connect(currentDome, &ISD::Dome::newParkStatus, [&](ISD::ParkStatus status)
    {
        m_ParkStatus = status;
        emit newParkStatus(status);
    });
    connect(currentDome, &ISD::Dome::newShutterStatus, this, &Dome::newShutterStatus);
    connect(currentDome, &ISD::Dome::newShutterStatus, [&](ISD::Dome::ShutterStatus status)
    {
        m_ShutterStatus = status;
    });
    connect(currentDome, &ISD::Dome::newAutoSyncStatus, this, &Dome::newAutoSyncStatus);
    connect(currentDome, &ISD::Dome::azimuthPositionChanged, this, &Dome::azimuthPositionChanged);
    connect(currentDome, &ISD::Dome::ready, this, &Dome::ready);
    connect(currentDome, &ISD::Dome::Disconnected, this, &Dome::disconnected);
}


bool Dome::canPark()
{
    if (currentDome == nullptr)
        return false;

    return currentDome->canPark();
}

bool Dome::park()
{
    if (currentDome == nullptr || currentDome->canPark() == false)
        return false;

    qCDebug(KSTARS_EKOS) << "Parking dome...";
    return currentDome->Park();
}

bool Dome::unpark()
{
    if (currentDome == nullptr || currentDome->canPark() == false)
        return false;

    qCDebug(KSTARS_EKOS) << "Unparking dome...";
    return currentDome->UnPark();
}

bool Dome::abort()
{
    if (currentDome == nullptr)
        return false;

    qCDebug(KSTARS_EKOS) << "Aborting...";
    return currentDome->Abort();
}

bool Dome::isMoving()
{
    if (currentDome == nullptr)
        return false;

    return currentDome->isMoving();
}

bool Dome::isRolloffRoof()
{
    // a rolloff roof is a dome that can move neither absolutely nor relatively
    return (currentDome && !currentDome->canAbsMove() && !currentDome->canRelMove());
}

bool Dome::canAbsoluteMove()
{
    if (currentDome)
        return currentDome->canAbsMove();

    return false;
}

bool Dome::canRelativeMove()
{
    if (currentDome)
        return currentDome->canRelMove();

    return false;
}

double Dome::azimuthPosition()
{
    if (currentDome)
        return currentDome->azimuthPosition();
    return -1;
}

void Dome::setAzimuthPosition(double position)
{
    if (currentDome)
        currentDome->setAzimuthPosition(position);
}

void Dome::setRelativePosition(double position)
{
    if (currentDome)
        currentDome->setRelativePosition(position);
}

bool Dome::moveDome(bool moveCW, bool start)
{
    if (currentDome == nullptr)
        return false;

    if (isRolloffRoof())
        qCDebug(KSTARS_EKOS) << (moveCW ? "Opening" : "Closing") << "rolloff roof" << (start ? "started." : "stopped.");
    else
        qCDebug(KSTARS_EKOS) << "Moving dome" << (moveCW ? "" : "counter") << "clockwise" << (start ? "started." : "stopped.");
    return currentDome->moveDome(moveCW ? ISD::Dome::DOME_CW : ISD::Dome::DOME_CCW,
                                 start  ? ISD::Dome::MOTION_START : ISD::Dome::MOTION_STOP);
}

bool Dome::isAutoSync()
{
    if (currentDome)
        return currentDome->isAutoSync();
    // value could not be determined
    return false;
}

bool Dome::setAutoSync(bool activate)
{
    if (currentDome)
        return currentDome->setAutoSync(activate);
    // not succeeded
    return false;
}

bool Dome::hasShutter()
{
    if (currentDome)
        return currentDome->hasShutter();
    // no dome, no shutter
    return false;
}

bool Dome::controlShutter(bool open)
{

    if (currentDome)
    {
        qCDebug(KSTARS_EKOS) << (open ? "Opening" : "Closing") << " shutter...";
        return currentDome->ControlShutter(open);
    }
    // no dome, no shutter control
    return false;
}

void Dome::removeDevice(ISD::GDInterface *device)
{
    device->disconnect(this);
    if (currentDome && (currentDome->getDeviceName() == device->getDeviceName()))
    {
        currentDome = nullptr;
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
