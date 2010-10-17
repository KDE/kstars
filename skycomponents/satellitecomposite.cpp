/***************************************************************************
                          satellitecomposite.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 22 Nov 2006
    copyright            : (C) 2006 by Jason Harris
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

#include "satellitecomposite.h"
#include "satellitecomponent.h"

#include <QFile>
#include <QFileInfo>
#include <QTextStream>

#include "Options.h"
#include "ksutils.h"
#include "kstarsdata.h"

#define DT 10.0    //10-sec timesteps
#define NSTEPS 360 //360 steps == 1 hour of coverage

SatelliteComposite::SatelliteComposite( SkyComposite *parent )
        : SkyComposite( parent )
{
    for ( uint i=0; i<NSTEPS; ++i )
        pSat.append( new SPositionSat );
    emitProgressText( i18n("Creating Earth satellites" ) );

    KStarsData* data = KStarsData::Instance();
    QFile file;

    //Extract satellite names from every third line of the satellites.dat file
    if ( KSUtils::openDataFile( file, "satellites.dat" ) ) {
        QString sfPath = QFileInfo( file ).absoluteFilePath();
        QTextStream stream( &file );
        int i = 0;
        while ( !stream.atEnd() ) {
            QString name = stream.readLine().trimmed();
            if ( i % 3 == 0 )
                SatelliteNames.append( name );
            i++;
        }
        file.close();

        //Read in data from the satellite file and construct paths for
        //the present geographic location
        SatInit( data->geo()->translatedName().toUtf8().data(),
                 data->geo()->lat()->Degrees(), data->geo()->lng()->Degrees(),
                 data->geo()->height(), sfPath.toAscii().data() );

        update( );
    }
}

SatelliteComposite::~SatelliteComposite()
{
    qDeleteAll(pSat);
}

void SatelliteComposite::update( KSNumbers * ) {
    KStarsData *data = KStarsData::Instance();
    //Julian Day value for current date and time:
    JD_0 = data->ut().djd();

    //Clear the current list of tracks
    components().clear();

    //Loop over desired satellites, construct their paths over the next hour,
    //and add visible paths to the list
    foreach ( const QString &satName, SatelliteNames ) {
        SatFindPosition( satName.toAscii().data(), JD_0, DT, NSTEPS, pSat.data() );

        //Make sure the satellite track is visible before adding it to the list.
        bool isVisible = false;
        for ( int i=0; i<NSTEPS; i++ ) {
            if ( pSat[i]->sat_ele > 10.0 ) {
                isVisible = true;
                break;
            }
        }

        if ( isVisible ) {
            SatelliteComponent *sc = new SatelliteComponent( this );
            sc->initSat( satName, pSat.data(), NSTEPS );
            addComponent( sc );

            //DEBUG
            // foreach ( SPositionSat *ps, pSat ) {
            //     KStarsDateTime dt( ps->jd );
            //     dms alt( ps->sat_ele );
            //     dms az( ps->sat_azi );
            //     kDebug() << ps->name << " " << dt.toString() << " " << alt.toDMSString() << " " << az.toDMSString();
            // }
        }
    }
}
