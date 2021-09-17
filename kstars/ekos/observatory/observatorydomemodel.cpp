/*  Ekos Observatory Module
    SPDX-FileCopyrightText: Wolfgang Reissenberger <sterne-jaeger@t-online.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "observatorydomemodel.h"
#include <KLocalizedString>

namespace Ekos
{

void ObservatoryDomeModel::initModel(Dome *dome)
{
    domeInterface = dome;

    connect(domeInterface, &Dome::ready, this, [&]() {initialized = true; emit ready();});
    connect(domeInterface, &Dome::disconnected, this, [&]() {emit disconnected(); initialized = false;});
    connect(domeInterface, &Dome::newStatus, this, &ObservatoryDomeModel::newStatus);
    connect(domeInterface, &Dome::newParkStatus, this, &ObservatoryDomeModel::newParkStatus);
    connect(domeInterface, &Dome::newShutterStatus, this, &ObservatoryDomeModel::newShutterStatus);
    connect(domeInterface, &Dome::azimuthPositionChanged, this, &ObservatoryDomeModel::azimuthPositionChanged);
    connect(domeInterface, &Dome::newAutoSyncStatus, this, &ObservatoryDomeModel::newAutoSyncStatus);

    initialized = true;
}

ISD::Dome::Status ObservatoryDomeModel::status()
{
    if (domeInterface == nullptr)
        return ISD::Dome::DOME_IDLE;

    return domeInterface->status();
}

ISD::Dome::ShutterStatus ObservatoryDomeModel::shutterStatus()
{
    if (domeInterface == nullptr)
        return ISD::Dome::SHUTTER_UNKNOWN;

    return domeInterface->shutterStatus();
}

void ObservatoryDomeModel::park()
{
    if (domeInterface == nullptr)
        return;

    emit newLog(i18n("Parking %1...", isRolloffRoof() ? i18n("rolloff roof") : i18n("dome")));
    domeInterface->park();
}


void ObservatoryDomeModel::unpark()
{
    if (domeInterface == nullptr)
        return;

    emit newLog(i18n("Unparking %1...", isRolloffRoof() ? i18n("rolloff roof") : i18n("dome")));
    domeInterface->unpark();
}

ISD::ParkStatus ObservatoryDomeModel::parkStatus()
{
    if (domeInterface == nullptr)
        return ISD::PARK_UNKNOWN;
    else if (isRolloffRoof())
    {
        // we need to override the parking status of the dome interface for opening and closing rolloff roofs
        if (domeInterface->status() == ISD::Dome::DOME_MOVING_CW)
            return ISD::PARK_UNPARKING;
        else if (domeInterface->status() == ISD::Dome::DOME_MOVING_CCW)
            return ISD::PARK_PARKING;
    }

    // in all other cases use the underlying park status
    return domeInterface->parkStatus();
}

void ObservatoryDomeModel::setAutoSync(bool activate)
{
    if (domeInterface == nullptr)
        return;

    if (domeInterface->setAutoSync(activate))
        emit newLog(activate ? i18n("Slaving activated.") : i18n("Slaving deactivated."));

}

void ObservatoryDomeModel::abort()
{
    if (domeInterface == nullptr)
        return;

    emit newLog(i18n("Aborting..."));
    domeInterface->abort();
}

void ObservatoryDomeModel::openShutter()
{
    if (domeInterface == nullptr)
        return;

    emit newLog(i18n("Opening shutter..."));
    domeInterface->controlShutter(true);
}

void ObservatoryDomeModel::closeShutter()
{
    if (domeInterface == nullptr)
        return;

    emit newLog(i18n("Closing shutter..."));
    domeInterface->controlShutter(false);
}

bool ObservatoryDomeModel::moveDome(bool moveCW, bool start)
{
    if (domeInterface == nullptr)
        return false;

    if (isRolloffRoof())
        emit newLog(i18nc("%2 dome or rolloff roof motion %1...", "%2 rolloff roof %1...", moveCW ? i18n("opening") : i18n("closing"), start ? i18n("Start") : i18n("Stop")));
    else
        emit newLog(i18nc("%2 dome or rolloff roof motion %1...", "%2 dome motion %1...", moveCW ? i18n("clockwise") : i18n("counter clockwise"), start ? i18n("Start") : i18n("Stop")));
    return domeInterface->moveDome(moveCW, start);
}

void ObservatoryDomeModel::execute(WeatherActions actions)
{
    if (hasShutter() && actions.closeShutter)
        closeShutter();
    if (actions.parkDome)
        park();
}

}
