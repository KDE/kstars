/*
    SPDX-FileCopyrightText: 2009 Prakash Mohan <prakash.mohan@kdemail.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "oal/oal.h"

#include <QString>

#include "geolocation.h"

/**
 * @class OAL::Site
 *
 * Information on site of observation.
 */
class OAL::Site
{
    public:
        Site(const QString &id, const QString &name, double lat, const QString &latUnit, double lon, const QString &lonUnit)
        {
            setSite(id, name, lat, latUnit, lon, lonUnit);
        }
        Site(GeoLocation *geo, const QString &id)
        {
            setSite(geo, id);
        }
        QString id() const
        {
            return m_Id;
        }
        QString name() const
        {
            return m_Name;
        }
        double latitude() const
        {
            return m_Lat;
        }
        QString latUnit() const
        {
            return m_LatUnit;
        }
        double longitude() const
        {
            return m_Lon;
        }
        QString lonUnit() const
        {
            return m_LonUnit;
        }
        void setSite(const QString &_id, const QString &_name, double _lat, const QString &_latUnit, double _lon,
                     const QString &_lonUnit);
        void setSite(GeoLocation *geo, const QString &id);

    private:
        QString m_Name, m_LatUnit, m_LonUnit, m_Id;
        double m_Lat, m_Lon;
};
