/*  INDI Weather
    Copyright (C) 2015 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#pragma once

#include <memory>
#include <QTimer>

#include "indistd.h"

namespace ISD
{
/**
 * @class Weather
 * Focuser class handles control of INDI Weather devices. It reports overall state and the value of each parameter
 *
 * @author Jasem Mutlaq
 */
class Weather : public DeviceDecorator
{
        Q_OBJECT

    public:
        explicit Weather(GDInterface *iPtr);

        typedef enum
        {
            WEATHER_IDLE,
            WEATHER_OK,
            WEATHER_WARNING,
            WEATHER_ALERT,
        } Status;

        typedef struct { QString name; QString label; double value; } WeatherData;

        void registerProperty(INDI::Property *prop) override;
        void processSwitch(ISwitchVectorProperty *svp) override;
        void processText(ITextVectorProperty *tvp) override;
        void processNumber(INumberVectorProperty *nvp) override;
        void processLight(ILightVectorProperty *lvp) override;

        DeviceFamily getType() override
        {
            return dType;
        }

        Status getWeatherStatus();
        quint16 getUpdatePeriod();
        bool refresh();

    signals:
        void newStatus(Status status);
        void newWeatherData(std::vector<WeatherData>);
        void ready();

    private:
        Status m_WeatherStatus { WEATHER_IDLE };
        std::unique_ptr<QTimer> readyTimer;
};
}

#ifndef KSTARS_LITE
Q_DECLARE_METATYPE(ISD::Weather::Status)
QDBusArgument &operator<<(QDBusArgument &argument, const ISD::Weather::Status &source);
const QDBusArgument &operator>>(const QDBusArgument &argument, ISD::Weather::Status &dest);
#endif
