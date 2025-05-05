/*  Ekos Observatory Module
    SPDX-FileCopyrightText: Wolfgang Reissenberger <sterne-jaeger@t-online.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ui_observatory.h"

#include "indi/indidome.h"
#include "indi/indiweather.h"

#include <QWidget>
#include <QLineEdit>
#include <KLocalizedString>

namespace Ekos
{

struct ObservatoryStatusControl
{
    bool useDome, useShutter, useWeather;
};

struct WeatherActions
{
    bool parkDome, closeShutter, stopScheduler;
    uint delay;
};


class Observatory : public QWidget, public Ui::Observatory
{
        Q_OBJECT
        Q_CLASSINFO("D-Bus Interface", "org.kde.kstars.Ekos.Observatory")
        Q_PROPERTY(QStringList logText READ logText NOTIFY newLog)
        Q_PROPERTY(ISD::Weather::Status status READ status NOTIFY newStatus)

    public:
        Observatory();

        bool setDome(ISD::Dome *device);
        bool addWeatherSource(ISD::Weather *device);

        ISD::Weather::Status status()
        {
            return m_WeatherStatus;
        }

        // Logging
        QStringList logText()
        {
            return m_LogText;
        }
        QString getLogText()
        {
            return m_LogText.join("\n");
        }

        void clearLog();

        /**
         * @brief Retrieve the settings that define, from which states the
         * "ready" state of the observatory is derived from.
         */
        ObservatoryStatusControl statusControl()
        {
            return m_StatusControl;
        }
        void setStatusControl(ObservatoryStatusControl control);
        void removeDevice(const QSharedPointer<ISD::GenericDevice> &deviceRemoved);
        void setWeatherSource(const QString &name);


    signals:
        Q_SCRIPTABLE void newLog(const QString &text);
        Q_SCRIPTABLE void newWeatherData(const QJsonArray &data);
        Q_SCRIPTABLE void newStatus(ISD::Weather::Status status);

    private:
        // motion control
        void enableMotionControl(bool enabled);

        // slaving control
        void enableAutoSync(bool enabled);
        void showAutoSync(bool enabled);

        // Logging
        QStringList m_LogText;
        void appendLogText(const QString &);

        // timer for refreshing the observatory status
        QTimer weatherStatusTimer;

        // reacting on weather changes
        void setWarningActions(WeatherActions actions);
        void setAlertActions(WeatherActions actions);

        // button handling
        void toggleButtons(QPushButton *buttonPressed, QString titlePressed, QPushButton *buttonCounterpart,
                           QString titleCounterpart);
        void activateButton(QPushButton *button, QString title);
        void buttonPressed(QPushButton *button, QString title);

        // weather sensor data
        QGridLayout* sensorDataBoxLayout;
        // map id -> (label, widget)
        std::map<QString, QPair<QAbstractButton*, QLineEdit*>*> sensorDataWidgets = {};
        // map id -> graph key x value vector
        std::map<QString, QVector<QCPGraphData>*> sensorGraphData = {};

        // map id -> range (+1: values only > 0, 0: values > 0 and < 0; -1: values < 0)
        std::map<QString, int> sensorRanges = {};

        // selected sensor for graph display
        QString selectedSensorID;

        // button group for sensor names to ensure, that only one button is pressed
        QButtonGroup *sensorDataNamesGroup {nullptr};

        ISD::Weather::Status m_WeatherStatus { ISD::Weather::WEATHER_IDLE };

        // Initialize the UI controls that configure reactions upon weather events
        void initWeatherActions(bool enabled);

        void initSensorGraphs();
        void updateSensorData(const QJsonArray &data);
        void updateSensorGraph(const QString &sensor_label, QDateTime now, double value);

        // hold all sensor data received from the weather station
        QJsonArray m_WeatherData;

        /**
         * @brief Activate or deactivate the weather warning actions
         */
        void setWarningActionsActive(bool active);
        /**
         * @brief Activate or deactivate the weather alert actions
         */
        void setAlertActionsActive(bool active);

        /**
         * @brief Flag whether the X axis should be visible in the sensor graph
         */
        bool autoScaleValues()
        {
            return m_autoScaleValues;
        }
        void setAutoScaleValues(bool show);

    private:
        // observatory status handling
        //void setObseratoryStatusControl(ObservatoryStatusControl control);
        void statusControlSettingsChanged();

        void initWeather();
        void enableWeather(bool enable);
        void clearSensorDataHistory();
        void shutdownWeather();
        void setWeatherStatus(ISD::Weather::Status status);

        // sensor data graphs
        void mouseOverLine(QMouseEvent *event);
        void refreshSensorGraph();

        void execute(WeatherActions actions);

        // reacting on weather changes
        void weatherWarningSettingsChanged();
        void weatherAlertSettingsChanged();

        // reacting on sensor selection change
        void selectedSensorChanged(QString id);

        // reacting on observatory status changes
        void observatoryStatusChanged(bool ready);
        void domeAzimuthChanged(double position);

        void shutdownDome();
        void startupDome();
        void connectDomeSignals();

        void setDomeStatus(ISD::Dome::Status status);
        void setDomeParkStatus(ISD::ParkStatus status);
        void setShutterStatus(ISD::Dome::ShutterStatus status);

        /**
         * @brief Actions to be taken when a weather warning occurs
         */
        WeatherActions getWarningActions()
        {
            return m_WarningActions;
        }
        QString getWarningActionsStatus();
        bool getWarningActionsActive()
        {
            return warningActionsActive;
        }

        /**
         * @brief Actions to be taken when a weather alert occurs
         */
        WeatherActions getAlertActions()
        {
            return m_AlertActions;
        }
        QString getAlertActionsStatus();
        bool getAlertActionsActive()
        {
            return alertActionsActive;
        }

    private:
        ISD::Dome *m_Dome {nullptr};
        ISD::Weather *m_WeatherSource {nullptr};
        QList<ISD::Weather *> m_WeatherSources;

        ObservatoryStatusControl m_StatusControl;

        QTimer warningTimer, alertTimer;
        struct WeatherActions m_WarningActions, m_AlertActions;
        bool warningActionsActive, alertActionsActive, m_autoScaleValues;
        void startAlertTimer();
        void startWarningTimer();
};
}
