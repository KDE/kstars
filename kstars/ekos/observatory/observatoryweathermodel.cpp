/*  Ekos Observatory Module
    Copyright (C) Wolfgang Reissenberger <sterne-jaeger@t-online.de>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include "observatoryweathermodel.h"
#include "Options.h"

namespace Ekos
{

void ObservatoryWeatherModel::initModel(Weather *weather)
{
    mWeather = weather;

    // ensure that we start the timers if required
    weatherChanged(status());

    connect(mWeather, &Weather::ready, this, &ObservatoryWeatherModel::updateWeatherStatus);
    connect(mWeather, &Weather::newStatus, this, &ObservatoryWeatherModel::weatherChanged);
    connect(mWeather, &Weather::disconnected, this, &ObservatoryWeatherModel::disconnected);

    // read the default values
    warningActionsActive = Options::warningActionsActive();
    warningActions.parkDome = Options::weatherWarningCloseDome();
    warningActions.closeShutter = Options::weatherWarningCloseShutter();
    warningActions.delay = Options::weatherWarningDelay();
    warningActions.stopScheduler = Options::weatherAlertStopScheduler();
    alertActionsActive = Options::alertActionsActive();
    alertActions.parkDome = Options::weatherAlertCloseDome();
    alertActions.closeShutter = Options::weatherAlertCloseShutter();
    alertActions.stopScheduler = Options::weatherAlertStopScheduler();
    alertActions.delay = Options::weatherAlertDelay();

    warningTimer.setInterval(warningActions.delay * 1000);
    warningTimer.setSingleShot(true);
    alertTimer.setInterval(alertActions.delay * 1000);
    alertTimer.setSingleShot(true);

    connect(&warningTimer, &QTimer::timeout, [this]() { execute(warningActions); });
    connect(&alertTimer, &QTimer::timeout, [this]() { execute(alertActions); });

    if (mWeather->status() != ISD::Weather::WEATHER_IDLE)
        emit ready();
}

ISD::Weather::Status ObservatoryWeatherModel::status()
{
    if (mWeather == nullptr)
        return ISD::Weather::WEATHER_IDLE;

    return mWeather->status();
}

void ObservatoryWeatherModel::setWarningActionsActive(bool active)
{
    warningActionsActive = active;
    Options::setWarningActionsActive(active);

    // stop warning actions if deactivated
    if (!active && warningTimer.isActive())
        warningTimer.stop();
    // start warning timer if activated
    else if (active && !warningTimer.isActive() && mWeather->status() == ISD::Weather::WEATHER_WARNING)
        warningTimer.start();
}

void ObservatoryWeatherModel::setAlertActionsActive(bool active)
{
    alertActionsActive = active;
    Options::setAlertActionsActive(active);

    // stop alert actions if deactivated
    if (!active && alertTimer.isActive())
        alertTimer.stop();
    // start alert timer if activated
    else if (active && !alertTimer.isActive() && mWeather->status() == ISD::Weather::WEATHER_ALERT)
        alertTimer.start();
}

void ObservatoryWeatherModel::setWarningActions(WeatherActions actions) {
    warningActions = actions;
    Options::setWeatherWarningCloseDome(actions.parkDome);
    Options::setWeatherWarningCloseShutter(actions.closeShutter);
    Options::setWeatherWarningDelay(actions.delay);
    warningTimer.setInterval(actions.delay * 1000);
}


QString ObservatoryWeatherModel::getWarningActionsStatus()
{
    if (warningTimer.isActive())
    {
        QString status;
        int remaining = warningTimer.remainingTime()/1000;
        return status.sprintf("%02ds remaining", remaining);
    }

    return "Status: inactive";
}

void ObservatoryWeatherModel::setAlertActions(WeatherActions actions) {
    alertActions = actions;
    Options::setWeatherAlertCloseDome(actions.parkDome);
    Options::setWeatherAlertCloseShutter(actions.closeShutter);
    Options::setWeatherAlertDelay(actions.delay);
    alertTimer.setInterval(actions.delay * 1000);
}

QString ObservatoryWeatherModel::getAlertActionsStatus()
{
    if (alertTimer.isActive())
    {
        QString status;
        int remaining = alertTimer.remainingTime()/1000;
        return status.sprintf("%02ds remaining", remaining);
    }

    return "Status: inactive";
}

void ObservatoryWeatherModel::updateWeatherStatus()
{
    weatherChanged(status());
    emit ready();
}


void ObservatoryWeatherModel::weatherChanged(ISD::Weather::Status status)
{
    switch (status) {
    case ISD::Weather::WEATHER_OK:
        warningTimer.stop();
        alertTimer.stop();
        break;
    case ISD::Weather::WEATHER_WARNING:
        if (warningActionsActive)
            warningTimer.start();
        alertTimer.stop();
        break;
    case ISD::Weather::WEATHER_ALERT:
        warningTimer.stop();
        if (alertActionsActive)
            alertTimer.start();
        break;
    default:
        break;
    }
    emit newStatus(status);
}

} // Ekos
