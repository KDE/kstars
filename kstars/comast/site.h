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

#include "comast/comast.h"

#include <QString>

class Comast::Site {
    public:
       Site( QString name, double lat, QString latUnit, double lon, QString lonUnit ) { setSite( name, lat, latUnit, lon, lonUnit ); }
       QString name() { return m_Name; }
       double latitude() { return m_Lat; }
       QString latUnit() { return m_LatUnit; }
       double longitude() { return m_Lon; }
       QString lonUnit() { return m_LonUnit; }
       void setSite( QString _name, double _lat, QString _latUnit, double _lon, QString _lonUnit);
    private:
        QString m_Name, m_LatUnit, m_LonUnit;
        double m_Lat, m_Lon;
};
#endif
