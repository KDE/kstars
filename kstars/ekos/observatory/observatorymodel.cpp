/*  Ekos Observatory Module
    SPDX-FileCopyrightText: Wolfgang Reissenberger <sterne-jaeger@t-online.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "observatorymodel.h"
#include "Options.h"

namespace Ekos
{

ObservatoryModel::ObservatoryModel()
{

    mStatusControl.useDome = Options::observatoryStatusUseDome();
    mStatusControl.useShutter = Options::observatoryStatusUseShutter();
    mStatusControl.useWeather = Options::observatoryStatusUseWeather();

    setDomeModel(new ObservatoryDomeModel());
    setWeatherModel(new ObservatoryWeatherModel());
}

void ObservatoryModel::setDomeModel(ObservatoryDomeModel *model) {
    mDomeModel = model;
    if (model != nullptr)
    {
        connect(mDomeModel, &ObservatoryDomeModel::newStatus, [this](ISD::Dome::Status s) { Q_UNUSED(s); updateStatus(); });
        connect(mDomeModel, &ObservatoryDomeModel::newParkStatus, [this](ISD::ParkStatus s) { Q_UNUSED(s); updateStatus(); });
        connect(mDomeModel, &ObservatoryDomeModel::newShutterStatus, [this](ISD::Dome::ShutterStatus s) { Q_UNUSED(s); updateStatus(); });
        if (mWeatherModel != nullptr)
            connect(mWeatherModel, &ObservatoryWeatherModel::execute, mDomeModel, &ObservatoryDomeModel::execute);
    }
    else
    {
        if (mWeatherModel != nullptr)
            disconnect(mWeatherModel, &ObservatoryWeatherModel::execute, mDomeModel, &ObservatoryDomeModel::execute);
    }

    updateStatus();
}

void ObservatoryModel::setWeatherModel(ObservatoryWeatherModel *model) {
    mWeatherModel = model;
    if (model != nullptr)
    {
        connect(mWeatherModel, &ObservatoryWeatherModel::newStatus, [this](ISD::Weather::Status s) { Q_UNUSED(s); updateStatus(); });
        if (mDomeModel != nullptr)
        {
            connect(mWeatherModel, &ObservatoryWeatherModel::execute, mDomeModel, &ObservatoryDomeModel::execute);
        }
    }
    else
    {
        if (mDomeModel != nullptr)
            disconnect(mWeatherModel, &ObservatoryWeatherModel::execute, mDomeModel, &ObservatoryDomeModel::execute);
    }
    updateStatus();
}


void ObservatoryModel::setStatusControl(ObservatoryStatusControl control)
{
    mStatusControl = control;
    Options::setObservatoryStatusUseDome(control.useDome);
    Options::setObservatoryStatusUseShutter(control.useShutter);
    Options::setObservatoryStatusUseWeather(control.useWeather);
    updateStatus();
}

bool ObservatoryModel::isReady()
{
    // dome relevant for the status and dome is ready
    if (mStatusControl.useDome && (getDomeModel() == nullptr || getDomeModel()->parkStatus() != ISD::PARK_UNPARKED))
        return false;

    // shutter relevant for the status and shutter open
    if (mStatusControl.useShutter && (getDomeModel() == nullptr ||
                                      (getDomeModel()->hasShutter() && getDomeModel()->shutterStatus() != ISD::Dome::SHUTTER_OPEN)))
        return false;

    // weather relevant for the status and weather is OK
    if (mStatusControl.useWeather && (getWeatherModel() == nullptr || getWeatherModel()->status() != ISD::Weather::WEATHER_OK))
        return false;

    return true;
}

void ObservatoryModel::updateStatus()
{
    emit newStatus(isReady());
}

void ObservatoryModel::makeReady()
{
    // dome relevant for the status and dome is ready
    if (mStatusControl.useDome && (getDomeModel() == nullptr || getDomeModel()->status() != ISD::Dome::DOME_IDLE))
        getDomeModel()->unpark();

    // shutter relevant for the status and shutter open
    if (mStatusControl.useShutter && (getDomeModel() == nullptr ||
                                      (getDomeModel()->hasShutter() && getDomeModel()->shutterStatus() != ISD::Dome::SHUTTER_OPEN)))
        getDomeModel()->openShutter();

    // weather relevant for the status and weather is OK
    // Haha, weather we can't change
}

}
