/*  Ekos Observatory Module
    SPDX-FileCopyrightText: Wolfgang Reissenberger <sterne-jaeger@t-online.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "observatoryweathermodel.h"
#include "Options.h"
#include <KLocalizedString>

namespace Ekos
{

void ObservatoryWeatherModel::initModel(Weather *weather)
{
    weatherInterface = weather;

    // ensure that we start the timers if required
    weatherChanged(status());

    connect(weatherInterface, &Weather::ready, this, [&]()
    {
        initialized = true;
        emit updateWeatherStatus();
    });
    connect(weatherInterface, &Weather::newStatus, this, &ObservatoryWeatherModel::weatherChanged);
    connect(weatherInterface, &Weather::newWeatherData, this, &ObservatoryWeatherModel::updateWeatherData);
    connect(weatherInterface, &Weather::newWeatherData, this, &ObservatoryWeatherModel::newWeatherData);
    connect(weatherInterface, &Weather::disconnected, this, [&]()
    {
        emit disconnected();
        initialized = false;
    });

    // read the default values
    warningActionsActive = Options::warningActionsActive();
    warningActions.parkDome = Options::weatherWarningCloseDome();
    warningActions.closeShutter = Options::weatherWarningCloseShutter();
    warningActions.delay = Options::weatherWarningDelay();
    alertActionsActive = Options::alertActionsActive();
    alertActions.parkDome = Options::weatherAlertCloseDome();
    alertActions.closeShutter = Options::weatherAlertCloseShutter();
    alertActions.delay = Options::weatherAlertDelay();
    m_autoScaleValues = Options::weatherAutoScaleValues();

    // not implemented yet
    warningActions.stopScheduler = false;
    alertActions.stopScheduler = false;

    warningTimer.setInterval(static_cast<int>(warningActions.delay * 1000));
    warningTimer.setSingleShot(true);
    alertTimer.setInterval(static_cast<int>(alertActions.delay * 1000));
    alertTimer.setSingleShot(true);

    connect(&warningTimer, &QTimer::timeout, [this]()
    {
        execute(warningActions);
    });
    connect(&alertTimer, &QTimer::timeout, [this]()
    {
        execute(alertActions);
    });

    if (weatherInterface->status() != ISD::Weather::WEATHER_IDLE)
        emit ready();

    initialized = true;
}

ISD::Weather::Status ObservatoryWeatherModel::status()
{
    if (weatherInterface == nullptr)
        return ISD::Weather::WEATHER_IDLE;

    return weatherInterface->status();
}

bool ObservatoryWeatherModel::refresh()
{
    return weatherInterface->refresh();
}

void ObservatoryWeatherModel::setWarningActionsActive(bool active)
{
    warningActionsActive = active;
    Options::setWarningActionsActive(active);

    // stop warning actions if deactivated
    if (!active && warningTimer.isActive())
        warningTimer.stop();
    // start warning timer if activated
    else if (weatherInterface->status() == ISD::Weather::WEATHER_WARNING)
        startWarningTimer();
}

void ObservatoryWeatherModel::startWarningTimer()
{
    if (warningActionsActive && (warningActions.parkDome || warningActions.closeShutter || warningActions.stopScheduler))
    {
        if (!warningTimer.isActive())
            warningTimer.start();
    }
    else if (warningTimer.isActive())
        warningTimer.stop();
}

void ObservatoryWeatherModel::setAlertActionsActive(bool active)
{
    alertActionsActive = active;
    Options::setAlertActionsActive(active);

    // stop alert actions if deactivated
    if (!active && alertTimer.isActive())
        alertTimer.stop();
    // start alert timer if activated
    else if (weatherInterface->status() == ISD::Weather::WEATHER_ALERT)
        startAlertTimer();
}

void ObservatoryWeatherModel::setAutoScaleValues(bool value)
{
    m_autoScaleValues = value;
    Options::setWeatherAutoScaleValues(value);
}

void ObservatoryWeatherModel::startAlertTimer()
{
    if (alertActionsActive && (alertActions.parkDome || alertActions.closeShutter || alertActions.stopScheduler))
    {
        if (!alertTimer.isActive())
            alertTimer.start();
    }
    else if (alertTimer.isActive())
        alertTimer.stop();
}

void ObservatoryWeatherModel::setWarningActions(WeatherActions actions)
{
    warningActions = actions;
    Options::setWeatherWarningCloseDome(actions.parkDome);
    Options::setWeatherWarningCloseShutter(actions.closeShutter);
    Options::setWeatherWarningDelay(actions.delay);
    if (!warningTimer.isActive())
        warningTimer.setInterval(static_cast<int>(actions.delay * 1000));

    if (weatherInterface->status() == ISD::Weather::WEATHER_WARNING)
        startWarningTimer();
}


QString ObservatoryWeatherModel::getWarningActionsStatus()
{
    if (warningTimer.isActive())
    {
        int remaining = warningTimer.remainingTime() / 1000;
        return i18np("%1 second remaining", "%1 seconds remaining", remaining);
    }

    return i18n("Status: inactive");
}

void ObservatoryWeatherModel::setAlertActions(WeatherActions actions)
{
    alertActions = actions;
    Options::setWeatherAlertCloseDome(actions.parkDome);
    Options::setWeatherAlertCloseShutter(actions.closeShutter);
    Options::setWeatherAlertDelay(actions.delay);
    if (!alertTimer.isActive())
        alertTimer.setInterval(static_cast<int>(actions.delay * 1000));

    if (weatherInterface->status() == ISD::Weather::WEATHER_ALERT)
        startAlertTimer();
}

QString ObservatoryWeatherModel::getAlertActionsStatus()
{
    if (alertTimer.isActive())
    {
        int remaining = alertTimer.remainingTime() / 1000;
        return i18np("%1 second remaining", "%1 seconds remaining", remaining);
    }

    return i18n("Status: inactive");
}

void ObservatoryWeatherModel::updateWeatherStatus()
{
    weatherChanged(status());
    emit ready();
}


void ObservatoryWeatherModel::weatherChanged(ISD::Weather::Status status)
{
    switch (status)
    {
        case ISD::Weather::WEATHER_OK:
            warningTimer.stop();
            alertTimer.stop();
            break;
        case ISD::Weather::WEATHER_WARNING:
            alertTimer.stop();
            startWarningTimer();
            break;
        case ISD::Weather::WEATHER_ALERT:
            warningTimer.stop();
            startAlertTimer();
            break;
        default:
            break;
    }
    emit newStatus(status);
}

void ObservatoryWeatherModel::updateWeatherData(const std::vector<ISD::Weather::WeatherData> &data)
{
    // add or update all received values
    for (auto &oneEntry : data)
    {
        // update if already existing
        unsigned long pos = findWeatherData(oneEntry.name);
        if (pos < m_WeatherData.size())
            m_WeatherData[pos].value = oneEntry.value;
        // new weather sensor?
        else if (oneEntry.name.startsWith("WEATHER_"))
            m_WeatherData.push_back({QString(oneEntry.name), QString(oneEntry.label), oneEntry.value});
    }
    // update UI
    emit newStatus(status());
}

unsigned long ObservatoryWeatherModel::findWeatherData(const QString name)
{
    unsigned long i;
    for (i = 0; i < m_WeatherData.size(); i++)
    {
        if (m_WeatherData[i].name.compare(name) == 0)
            return i;
    }
    // none found
    return i;
}
} // Ekos
