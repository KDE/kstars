/*  Ekos Observatory Module
    Copyright (C) Wolfgang Reissenberger <sterne-jaeger@t-online.de>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#pragma once

#include "ui_observatory.h"
#include "observatorymodel.h"
#include "observatorydomemodel.h"
#include "observatoryweathermodel.h"

#include <QWidget>
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

    private slots:
        // observatory status handling
        void setObseratoryStatusControl(ObservatoryStatusControl control);
        void statusControlSettingsChanged();

        void initWeather();
        void shutdownWeather();
        void setWeatherStatus(ISD::Weather::Status status);


        // reacting on weather changes
        void weatherWarningSettingsChanged();
        void weatherAlertSettingsChanged();

        // reacting on observatory status changes
        void observatoryStatusChanged(bool ready);
        void domeAzimuthChanged(double position);

        void initDome();
        void shutdownDome();

        void setDomeStatus(ISD::Dome::Status status);
        void setShutterStatus(ISD::Dome::ShutterStatus status);
};
}
