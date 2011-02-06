/***************************************************************************
                          site.h  -  description

                             -------------------
    begin                : Wednesday July 8, 2009
    copyright            : (C) 2009 by Prakash Mohan
    email                : prakash.mohan@kdemail.net
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#ifndef SITE_H_
#define SITE_H_

#include "oal/oal.h"

#include <QString>

#include "geolocation.h"

class OAL::Site {
    public:
       Site( const QString& id,  const QString& name, double lat, const QString& latUnit, double lon, const QString &lonUnit ) { setSite( id, name, lat, latUnit, lon, lonUnit ); }
       Site( GeoLocation *geo, const QString& id ) { setSite( geo, id ); }
       QString id() const { return m_Id; }
       QString name() const { return m_Name; }
       double latitude() const { return m_Lat; }
       QString latUnit() const { return m_LatUnit; }
       double longitude() const { return m_Lon; }
       QString lonUnit() const { return m_LonUnit; }
       void setSite( const QString &_id, const QString& _name, double _lat, const QString& _latUnit, double _lon, const QString& _lonUnit);
       void setSite( GeoLocation *geo, const QString& id );
    private:
        QString m_Name, m_LatUnit, m_LonUnit, m_Id;
        double m_Lat, m_Lon;
};
#endif
