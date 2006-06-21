/***************************************************************************
                          milkywaycomposite.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 12 Nov. 2005
    copyright            : (C) 2005 by Jason Harris
    email                : kstars@30doradus.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <klocale.h>

#include <QFile>

#include "kstarsdata.h"

#include "ksutils.h"
#include "ksfilereader.h"

#include "milkywaycomposite.h"
#include "milkywaycomponent.h"

MilkyWayComposite::MilkyWayComposite( SkyComponent *parent, bool (*visibleMethod)() ) 
	: SkyComposite(parent) 
{
    QFile file;
    QString line;
    int index = 0;
    bool first = true;                      // makes code more robust
    double ra, dec, startRa, startDec;
    startRa = startDec = -1000.0;           // to avoid compiler warnings
    MilkyWayComponent *poly = new MilkyWayComponent(this, visibleMethod );
    addComponent( poly );


    emitProgressText( i18n( "Loading milky way" ) );
    if  ( !KSUtils::openDataFile( file, "milkyway.dat" ) ) {
        return;
    }
    
    KSFileReader fileReader( file );

    while ( fileReader.hasMoreLines() ) {
        line = fileReader.readLine();
        ra = line.mid( 2, 8 ).toDouble();
        dec = line.mid( 11, 8 ).toDouble(); 

        if ( first || line.at( 0 ) == 'M' )  {
            if ( !first ) {
                // wrap polygon back to first point 
                poly->addPoint( startRa, startDec );
                poly =  new MilkyWayComponent( this, visibleMethod );
                addComponent( poly );
            }
            startRa = ra;
            startDec = dec;
            first = false;
            index = 0;
        }
        poly->addPoint( ra, dec );
        if ( line.at( 0 ) == 'S' ) poly->skip[index] = true;
        index++;
    }
    // wrap last polygon
    if ( !first ) {
        poly->addPoint( startRa, startDec );
        poly->skip[index] = true;                    // not really needed
    }
}
