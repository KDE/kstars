/*  Ekos Observatory Module
    Copyright (C) Wolfgang Reissenberger <sterne-jaeger@t-online.de>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include "observatorydomemodel.h"
#include <KLocalizedString>

namespace Ekos
{

void ObservatoryDomeModel::initModel(Dome *dome)
{
    domeInterface = dome;

    connect(domeInterface, &Dome::ready, this, &ObservatoryDomeModel::ready);
    connect(domeInterface, &Dome::disconnected, this, &ObservatoryDomeModel::disconnected);
    connect(domeInterface, &Dome::newStatus, this, &ObservatoryDomeModel::newStatus);
    connect(domeInterface, &Dome::newShutterStatus, this, &ObservatoryDomeModel::newShutterStatus);
    connect(domeInterface, &Dome::azimuthPositionChanged, this, &ObservatoryDomeModel::azimuthPositionChanged);
    connect(domeInterface, &Dome::newAutoSyncStatus, this, &ObservatoryDomeModel::newAutoSyncStatus);

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

    emit newLog(i18n("Parking dome..."));
    domeInterface->park();
}


void ObservatoryDomeModel::unpark()
{
    if (domeInterface == nullptr)
        return;

    emit newLog(i18n("Unparking dome..."));
    domeInterface->unpark();
}

void ObservatoryDomeModel::setAutoSync(bool activate)
{
    if (domeInterface == nullptr)
        return;

    if (domeInterface->setAutoSync(activate))
        emit newLog(i18n(activate ? "Slaving activated." : "Slaving deactivated."));

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

void ObservatoryDomeModel::execute(WeatherActions actions)
{
    if (hasShutter() && actions.closeShutter)
        closeShutter();
    if (actions.parkDome)
        park();
}

}
