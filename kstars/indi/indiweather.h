/*
    SPDX-FileCopyrightText: 2015 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <memory>
#include <QTimer>

#include "indiconcretedevice.h"

namespace ISD
{
/**
 * @class Weather
 * Focuser class handles control of INDI Weather devices. It reports overall state and the value of each parameter
 *
 * @author Jasem Mutlaq
 */
class Weather : public ConcreteDevice
{
        Q_OBJECT

    public:
        explicit Weather(GenericDevice *parent);

        typedef enum
        {
            WEATHER_IDLE,
            WEATHER_OK,
            WEATHER_WARNING,
            WEATHER_ALERT,
        } Status;

        typedef struct
        {
            QString name;
            QString label;
            double value;
        } WeatherData;

        void processNumber(INumberVectorProperty *nvp) override;
        void processLight(ILightVectorProperty *lvp) override;

        Status status();
        quint16 getUpdatePeriod();
        bool refresh();
        const std::vector<WeatherData> &data() const
        {
            return m_WeatherData;
        }

    signals:
        void newStatus(Status status);
        void newWeatherData(const std::vector<WeatherData> &data);

    private:
        Status m_WeatherStatus { WEATHER_IDLE };
        std::vector<WeatherData> m_WeatherData;
};
}

#ifndef KSTARS_LITE
Q_DECLARE_METATYPE(ISD::Weather::Status)
QDBusArgument &operator<<(QDBusArgument &argument, const ISD::Weather::Status &source);
const QDBusArgument &operator>>(const QDBusArgument &argument, ISD::Weather::Status &dest);
#endif
