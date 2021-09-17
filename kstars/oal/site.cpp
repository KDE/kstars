/*
    SPDX-FileCopyrightText: 2009 Prakash Mohan <prakash.mohan@kdemail.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "oal/site.h"

void OAL::Site::setSite(const QString &_id, const QString &_name, double _lat, const QString &_latUnit, double _lon,
                        const QString &_lonUnit)
{
    m_Id      = _id;
    m_Name    = _name;
    m_Lat     = _lat;
    m_Lon     = _lon;
    m_LatUnit = _latUnit;
    m_LonUnit = _lonUnit;
}
void OAL::Site::setSite(GeoLocation *geo, const QString &id)
{
    m_Id      = id;
    m_Name    = geo->name();
    m_Lat     = geo->lat()->radians();
    m_Lon     = geo->lng()->radians();
    m_LatUnit = m_LonUnit = "rad";
}
