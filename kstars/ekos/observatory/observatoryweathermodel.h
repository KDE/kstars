/*  Ekos Observatory Module
    Copyright (C) Wolfgang Reissenberger <sterne-jaeger@t-online.de>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#pragma once

#include "ekos/auxiliary/weather.h"

#include <QObject>

namespace Ekos
{

struct WeatherActions
{
    bool parkDome, closeShutter, stopScheduler;
    uint delay;
};

class ObservatoryWeatherModel : public QObject
{

        Q_OBJECT

    public:
        ObservatoryWeatherModel() = default;

        void initModel(Weather *weather);
        ISD::Weather::Status status();

        bool refresh();

        /**
         * @brief Actions to be taken when a weather warning occurs
         */
        WeatherActions getWarningActions()
        {
            return warningActions;
        }
        QString getWarningActionsStatus();
        void setWarningActions(WeatherActions actions);
        bool getWarningActionsActive()
        {
            return warningActionsActive;
        }

        /**
         * @brief Actions to be taken when a weather alert occurs
         */
        WeatherActions getAlertActions()
        {
            return alertActions;
        }
        QString getAlertActionsStatus();
        void setAlertActions(WeatherActions actions);
        bool getAlertActionsActive()
        {
            return alertActionsActive;
        }

        /**
         * @brief Retrieve the currently known weather sensor values
         */
        std::vector<ISD::Weather::WeatherData> getWeatherData()
        {
            return m_WeatherData;
        }

        /**
         * @brief Flag whether the X axis should be visible in the sensor graph
         */
        bool autoScaleValues()
        {
            return m_autoScaleValues;
        }
        void setAutoScaleValues(bool show);

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
        Weather *weatherInterface;
        QTimer warningTimer, alertTimer;
        struct WeatherActions warningActions, alertActions;
        bool warningActionsActive, alertActionsActive, m_autoScaleValues;

        void startAlertTimer();
        void startWarningTimer();

        // hold all sensor data received from the weather station
        std::vector<ISD::Weather::WeatherData> m_WeatherData;
        // update the stored values
        void updateWeatherData(std::vector<ISD::Weather::WeatherData> entries);
        unsigned long findWeatherData(QString name);

    private slots:
        void weatherChanged(ISD::Weather::Status status);
        void updateWeatherStatus();

    signals:
        void newStatus(ISD::Weather::Status status);
        void newWeatherData(std::vector<ISD::Weather::WeatherData>);
        void ready();
        void disconnected();
        /**
         * @brief signal that actions need to be taken due to weather conditions
         */
        void execute(WeatherActions actions);

};

}
