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
       Site( QString id,  QString name, double lat, QString latUnit, double lon, QString lonUnit ) { setSite( id, name, lat, latUnit, lon, lonUnit ); }
       Site( GeoLocation *geo, QString id ) { setSite( geo, id ); }
       QString id() { return m_Id; }
       QString name() { return m_Name; }
       double latitude() { return m_Lat; }
       QString latUnit() { return m_LatUnit; }
       double longitude() { return m_Lon; }
       QString lonUnit() { return m_LonUnit; }
       void setSite( QString _id, QString _name, double _lat, QString _latUnit, double _lon, QString _lonUnit);
       void setSite( GeoLocation *geo, QString id );
    private:
        QString m_Name, m_LatUnit, m_LonUnit, m_Id;
        double m_Lat, m_Lon;
};
#endif
