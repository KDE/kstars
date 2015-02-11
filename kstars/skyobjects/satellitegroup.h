/***************************************************************************
                          satellitegroup.h  -  K Desktop Planetarium
                             -------------------
    begin                : Tue 22 Mar 2011
    copyright            : (C) 2011 by Jerome SONRIER
    email                : jsid@emor3j.fr.eu.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef SATELLITEGROUP_H
#define SATELLITEGROUP_H


#include <QString>

#include <QUrl>
#include "skyobjects/satellite.h"
#include "skypainter.h"


/**
    *@class SatelliteGroup
    *Represents a group of artificial satellites.
    *@author Jérôme SONRIER
    *@version 1.0
    */
class SatelliteGroup : public QList<Satellite*>
{
public:
    /**
     *@short Constructor
     */
    SatelliteGroup( QString name, QString tle_filename, QUrl update_url );

    /**
     *@short Destructor
     */
    ~SatelliteGroup();

    /**
     *Read TLE file of the group and create all satellites found in the file.
     */
    void readTLE();

    /**
     *Compute current position of the each satellites in the group.
     */
    void updateSatellitesPos();

    /**
     *@return TLE filename
     */
    QUrl tleFilename();

    /**
     *@return URL from which new TLE file must be download
     */
    QUrl tleUrl();

    /**
     *@return Name of the group
     */
    QString name();

private:
    QString m_name;             // Group name
    QString m_tle_file;         // TLE filename
    QUrl    m_tle_url;          // URL used to update TLE file
};

#endif
