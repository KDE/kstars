/*  Ekos Observatory Module
    Copyright (C) Wolfgang Reissenberger <sterne-jaeger@t-online.de>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#pragma once

#include "../auxiliary/weather.h"
#include "indiweather.h"

#include <QObject>

namespace Ekos
{

struct WeatherActions {
    bool parkDome, closeShutter, stopScheduler;
    int delay;
};

class ObservatoryWeatherModel : public QObject
{

    Q_OBJECT

public:
    ObservatoryWeatherModel() = default;

    void initModel(Weather *weather);
    ISD::Weather::Status status();

    /**
     * @brief Actions to be taken when a weather warning occurs
     */
    WeatherActions getWarningActions() { return warningActions; }
    QString getWarningActionsStatus();
    void setWarningActions(WeatherActions actions);
    bool getWarningActionsActive() { return warningActionsActive; }

    /**
     * @brief Actions to be taken when a weather alert occurs
     */
    WeatherActions getAlertActions() { return alertActions; }
    QString getAlertActionsStatus();
    void setAlertActions(WeatherActions actions);
    bool getAlertActionsActive() { return alertActionsActive; }

public slots:
    /**
     * @brief Activate or deactivate the weather warning actions
     */
    void setWarningActionsActive(bool active);
    /**
     * @brief Activate or deactivate the weather alert actions
     */
    void setAlertActionsActive(bool active);

private:
    Weather *mWeather;
    QTimer warningTimer, alertTimer;
    struct WeatherActions warningActions, alertActions;
    bool warningActionsActive, alertActionsActive;

private slots:
    void weatherChanged(ISD::Weather::Status status);
    void updateWeatherStatus();

signals:
    void newStatus(ISD::Weather::Status status);
    void ready();
    void disconnected();
    /**
     * @brief signal that actions need to be taken due to weather conditions
     */
    void execute(WeatherActions actions);

};

}
