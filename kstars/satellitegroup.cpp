/***************************************************************************
                          satellitegroup.cpp  -  K Desktop Planetarium
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

#include <QFile>

#include <kstandarddirs.h>

#include "satellitegroup.h"
#include "ksutils.h"


SatelliteGroup::SatelliteGroup( QString name, QString tle_filename, KUrl update_url )
{
    m_name = name;
    m_tle_file = tle_filename;
    m_tle_url = update_url;

    // Read TLE file and create satellites
    readTLE();
}

SatelliteGroup::~SatelliteGroup()
{
}

void SatelliteGroup::readTLE()
{
    QFile file;
    QString line1, line2;

    // Delete all satellites
    clear();
    
    // Read TLE file
    if ( KSUtils::openDataFile( file, m_tle_file ) ) {
        QTextStream stream( &file );
        while ( !stream.atEnd() ) {
            // Read satellite name
            QString sat_name = stream.readLine().trimmed();
            line1 = stream.readLine();
            line2 = stream.readLine();
            // Create a new satellite and add it to the list of satellites
            if (line1.startsWith("1") && line2.startsWith("2"))
                append( new Satellite( sat_name, line1, line2 ) );
        }
        file.close();
    }
}

void SatelliteGroup::updateSatellitesPos()
{
    for ( int i=0; i<size(); i++ ) {
        Satellite *sat = at( i );
        if ( sat->selected() )
            sat->updatePos();
    }
}

KUrl SatelliteGroup::tleFilename()
{
    // Return absolute path with "file:" before the path
    return KUrl( "file:" + KStandardDirs::locateLocal("appdata", "") + m_tle_file );
}

KUrl SatelliteGroup::tleUrl()
{
    return m_tle_url;
}

QString SatelliteGroup::name()
{
    return m_name;
}
