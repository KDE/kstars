/*  Ekos Observatory Module
    SPDX-FileCopyrightText: Wolfgang Reissenberger <sterne-jaeger@t-online.de>

    SPDX-License-Identifier: GPL-2.0-or-later
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
        bool isActive()
        {
            return initialized;
        }

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
        const std::vector<ISD::Weather::WeatherData> &getWeatherData() const
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
        bool initialized = false;
        Weather *weatherInterface;
        QTimer warningTimer, alertTimer;
        struct WeatherActions warningActions, alertActions;
        bool warningActionsActive, alertActionsActive, m_autoScaleValues;

        void startAlertTimer();
        void startWarningTimer();

        // hold all sensor data received from the weather station
        std::vector<ISD::Weather::WeatherData> m_WeatherData;
        // update the stored values
        void updateWeatherData(const std::vector<ISD::Weather::WeatherData> &data);
        unsigned long findWeatherData(QString name);

    private slots:
        void weatherChanged(ISD::Weather::Status status);
        void updateWeatherStatus();

    signals:
        void newStatus(ISD::Weather::Status status);
        void newWeatherData(const std::vector<ISD::Weather::WeatherData> &data);
        void ready();
        void disconnected();
        /**
         * @brief signal that actions need to be taken due to weather conditions
         */
        void execute(WeatherActions actions);

};

}
