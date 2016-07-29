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
#include <QDir>
#include <QStandardPaths>

#include "satellitegroup.h"
#include "ksutils.h"
#include "kspaths.h"

SatelliteGroup::SatelliteGroup( QString name, QString tle_filename, QUrl update_url )
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
    QMutableListIterator<Satellite *> sats(*this);
    while (sats.hasNext())
    {
        Satellite *sat = sats.next();

        if ( sat->selected() )
        {
            int rc = sat->updatePos();
            // If position cannot be calculated, remove it from list
            if (rc != 0)
                sats.remove();
        }
    }
}

QUrl SatelliteGroup::tleFilename()
{
    // Return absolute path with "file:" before the path
    //return QUrl( "file:" + (KSPaths::writableLocation(QStandardPaths::GenericDataLocation) + "") + m_tle_file) ;
    return QUrl(KSPaths::writableLocation(QStandardPaths::GenericDataLocation) + m_tle_file) ;
}

QUrl SatelliteGroup::tleUrl()
{
    return m_tle_url;
}

QString SatelliteGroup::name()
{
    return m_name;
}
