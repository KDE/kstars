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
    mDome = dome;

    connect(mDome, &Dome::ready, this, &ObservatoryDomeModel::ready);
    connect(mDome, &Dome::disconnected, this, &ObservatoryDomeModel::disconnected);
    connect(mDome, &Dome::newStatus, this, &ObservatoryDomeModel::newStatus);
    connect(mDome, &Dome::newShutterStatus, this, &ObservatoryDomeModel::newShutterStatus);

}


ISD::Dome::Status ObservatoryDomeModel::status()
{
    if (mDome == nullptr)
        return ISD::Dome::DOME_IDLE;

    return mDome->status();
}

ISD::Dome::ShutterStatus ObservatoryDomeModel::shutterStatus()
{
    if (mDome == nullptr)
        return ISD::Dome::SHUTTER_UNKNOWN;

    return mDome->shutterStatus();
}

void ObservatoryDomeModel::park()
{
    if (mDome == nullptr)
        return;

    emit newLog(i18n("Parking dome..."));
    mDome->park();
}


void ObservatoryDomeModel::unpark()
{
    if (mDome == nullptr)
        return;

    emit newLog(i18n("Unparking dome..."));
    mDome->unpark();
}

void ObservatoryDomeModel::openShutter()
{
    if (mDome == nullptr)
        return;

    emit newLog(i18n("Opening shutter..."));
    mDome->controlShutter(true);
}

void ObservatoryDomeModel::closeShutter()
{
    if (mDome == nullptr)
        return;

    emit newLog(i18n("Closing shutter..."));
    mDome->controlShutter(false);
}

void ObservatoryDomeModel::execute(WeatherActions actions)
{
    if (hasShutter() && actions.closeShutter)
        closeShutter();
    if (actions.parkDome)
        park();
}

}
