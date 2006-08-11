/***************************************************************************
                          satellitecomponent.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 14 July 2006
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

#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QVarLengthArray>

#include "ksutils.h"
#include "kstarsdata.h"
#include "satellitetrack.h"
#include "satellitecomponent.h"

SatelliteComponent::SatelliteComponent(SkyComponent *parent, bool (*visibleMethod)())
: SkyComponent(parent, visibleMethod)
{
}

SatelliteComponent::~SatelliteComponent() {
	clear();
}

void SatelliteComponent::clear() {
	while ( ! SatList.isEmpty() ) delete SatList.takeFirst();
}

//FIXME: I don't know if SatLib wants the observing station's altitude in meters.
//(which is what geo()->height() returns)
//TODO: Hopefully the geographic data will be removed from SatInit() eventually

void SatelliteComponent::init( KStarsData *data ) {
	//Extract satellite names from every third line of the satellites.dat file
	QFile file;

	if ( KSUtils::openDataFile( file, "satellites.dat" ) ) {
		QString sfPath = QFileInfo( file ).absoluteFilePath();
		QTextStream stream( &file );
		int i = 0;
		while ( !stream.atEnd() ) {
			if ( i % 3 == 0 ) {
				SatelliteNames.append( stream.readLine().trimmed() );
			}

			i++;
		}
		file.close();

		//Read in data from the satellite file and construct paths for 
		//the present geographic location
		SatInit( data->geo()->translatedName().toUtf8().data(), 
			data->geo()->lat()->Degrees(), data->geo()->lng()->Degrees(),
			data->geo()->height(), sfPath.toAscii().data() );
		
		//Need the Julian Day value for current date's midnight:
		double jdstart = data->ut().JDat0hUT();
		double dt = 1./48.; //30-minute time steps == 1/48 day
		int nsteps = 48;
		
		//Loop over desired satellites and add their paths to the list
		foreach ( QString satName, SatelliteNames ) {
			QVarLengthArray<SPositionSat *> pSat(SatelliteNames.size());
			SatFindPosition( satName.toAscii().data(), jdstart, dt, nsteps, pSat.data() );
		
			//Make sure the satellite track is visible before adding it to the list.
			bool isVisible = false;
			for ( int i=0; i<nsteps; i++ ) {
				if ( pSat[i]->sat_ele > 10.0 ) { 
					isVisible = true;
					break;
				}
			}
		
			if ( isVisible ) {
				SatList.append( new SatelliteTrack( pSat.data(), nsteps, data->lst(), data->geo()->lat() ) );
			}
		}
	}
}

void SatelliteComponent::update( KStarsData *data, KSNumbers *num ) {}

void SatelliteComponent::draw( KStars *ks, QPainter& psky, double scale ) {}
