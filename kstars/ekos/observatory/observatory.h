/*  Ekos Observatory Module
    SPDX-FileCopyrightText: Wolfgang Reissenberger <sterne-jaeger@t-online.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ui_observatory.h"
#include "observatorymodel.h"
#include "observatorydomemodel.h"
#include "observatoryweathermodel.h"

#include <QWidget>
#include <QLineEdit>
#include <KLocalizedString>

namespace Ekos
{

class Observatory : public QWidget, public Ui::Observatory
{
        Q_OBJECT
        Q_CLASSINFO("D-Bus Interface", "org.kde.kstars.Ekos.Observatory")
        Q_PROPERTY(QStringList logText READ logText NOTIFY newLog)

    public:
        Observatory();
        ObservatoryDomeModel *getDomeModel()
        {
            return mObservatoryModel->getDomeModel();
        }
        ObservatoryWeatherModel *getWeatherModel()
        {
            return mObservatoryModel->getWeatherModel();
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

    signals:
        Q_SCRIPTABLE void newLog(const QString &text);

        /**
         * @brief Signal a new observatory status
         */
        Q_SCRIPTABLE void newStatus(bool isReady);

        void newWeatherData(const std::vector<ISD::Weather::WeatherData> &data);

    private:
        ObservatoryModel *mObservatoryModel = nullptr;

        void setDomeModel(ObservatoryDomeModel *model);
        void setWeatherModel(ObservatoryWeatherModel *model);

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
        void updateSensorData(const std::vector<ISD::Weather::WeatherData> &data);
        void updateSensorGraph(QString sensor_label, QDateTime now, double value);


    private slots:
        // observatory status handling
        void setObseratoryStatusControl(ObservatoryStatusControl control);
        void statusControlSettingsChanged();

        void initWeather();
        void enableWeather(bool enable);
        void clearSensorDataHistory();
        void shutdownWeather();
        void setWeatherStatus(ISD::Weather::Status status);

        // sensor data graphs
        void mouseOverLine(QMouseEvent *event);
        void refreshSensorGraph();

        // reacting on weather changes
        void weatherWarningSettingsChanged();
        void weatherAlertSettingsChanged();

        // reacting on sensor selection change
        void selectedSensorChanged(QString id);

        // reacting on observatory status changes
        void observatoryStatusChanged(bool ready);
        void domeAzimuthChanged(double position);

        void initDome();
        void shutdownDome();

        void setDomeStatus(ISD::Dome::Status status);
        void setDomeParkStatus(ISD::ParkStatus status);
        void setShutterStatus(ISD::Dome::ShutterStatus status);
};
}
