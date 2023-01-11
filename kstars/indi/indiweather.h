/*
    SPDX-FileCopyrightText: 2015 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <memory>
#include <QTimer>
#include <QJsonDocument>

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
        Q_CLASSINFO("D-Bus Interface", "org.kde.kstars.INDI.Weather")
        Q_PROPERTY(ISD::Weather::Status status READ status NOTIFY newStatus)
        Q_PROPERTY(int refreshPeriod READ refreshPeriod WRITE setRefreshPeriod)
        Q_PROPERTY(QByteArray data READ jsonData NOTIFY newJSONData)

    public:
        explicit Weather(GenericDevice *parent);

        typedef enum
        {
            WEATHER_IDLE,
            WEATHER_OK,
            WEATHER_WARNING,
            WEATHER_ALERT,
        } Status;

        void processNumber(INDI::Property prop) override;
        void processLight(INDI::Property prop) override;

        Status status();

        int refreshPeriod();
        void setRefreshPeriod(int value);

        bool refresh();
        const QJsonArray &data() const
        {
            return m_Data;
        }
        /**
         * @brief jsondata Used for DBus
         * @return Weahter data in JSON Format
         */
        QByteArray jsonData() const
        {
            return QJsonDocument(m_Data).toJson();
        }

    signals:
        void newStatus(ISD::Weather::Status status);
        void newData(const QJsonArray &data);
        void newJSONData(QByteArray data);

    private:
        Status m_WeatherStatus { WEATHER_IDLE };
        QJsonArray m_Data;
};
}

#ifndef KSTARS_LITE
Q_DECLARE_METATYPE(ISD::Weather::Status)
QDBusArgument &operator<<(QDBusArgument &argument, const ISD::Weather::Status &source);
const QDBusArgument &operator>>(const QDBusArgument &argument, ISD::Weather::Status &dest);
#endif
