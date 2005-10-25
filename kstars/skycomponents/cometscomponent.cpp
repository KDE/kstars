/***************************************************************************
                          cometscomponent.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 2005/24/09
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

#include "cometscomponent.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "ksutils.h"
#include "skymap.h"

CometsComponent::CometsComponent() 
: SolarSystemListComponent() 
{
}

CometsComponent::~CometsComponent() {
	//object deletion handled in grandparent class (ListComponent)
}

void CometsComponent::init( KStarsData *data ) {
	QFile file;

	if ( KSUtils::openDataFile( file, "comets.dat" ) ) {
		KSFileReader fileReader( file );

		while( fileReader.hasMoreLines() ) {
			QString line, name;
			int mJD;
			double q, e, dble_i, dble_w, dble_N, Tp;
			long double JD;
			KSComet *com = 0;

			line = fileReader.readLine();
			name = line.mid( 3, 35 ).stripWhiteSpace();
			mJD  = line.mid( 38, 5 ).toInt();
			q    = line.mid( 44, 10 ).toDouble();
			e    = line.mid( 55, 10 ).toDouble();
			dble_i = line.mid( 66, 9 ).toDouble();
			dble_w = line.mid( 76, 9 ).toDouble();
			dble_N = line.mid( 86, 9 ).toDouble();
			Tp = line.mid( 96, 14 ).toDouble();

			JD = double( mJD ) + 2400000.5;

			com = new KSComet( this, name, "", JD, q, e, dms(dble_i), dms(dble_w), dms(dble_N), Tp );
			com->setAngularSize( 0.005 );

			cometList.append( com );

			//TODO: need KStarsData::appendNamedObject()
			data->apppendNamedObject( com );
		}
	}
}

void CometsComponent::updatePlanets(KStarsData *data, KSNumbers *num, bool needNewCoords)
{
	if ( visible() )
	{
		KSPlanet Earth( data, I18N_NOOP( "Earth" ) );
		Earth.findPosition( num );
		foreach ( SkyObject *o, objectsList() ) {
			KSComet *com = (KSComet*)com;
			if ( needNewCoords ) com->findPosition( num, data->geo()->lat(), data->lst(), &Earth );
			com->EquatorialToHorizontal( data->lst(), data->geo()->lat() );
			if ( com->hasTrail() )
			{
			  com->updateTrail( data->lst(), data->geo()->lat() );
			}
		}
	}
}

void CometsComponent::draw( KStars *ks, QPainter& psky, double scale )
{
	if ( !visible() ) return;
	
	SkyMap *map = ks->map();

	foreach ( KSComet *com, cometList ) { 
		if ( com->mag() > Options::magLimitComet() ) break;

		if ( map->checkVisibility( com ) )
		{
			psky.setPen( QPen( QColor( "cyan4" ) ) );
			psky.setBrush( QBrush( QColor( "cyan4" ) ) );
			QPoint o = getXY( com, Options::useAltAz(), Options::useRefraction(), scale );

			if ( ( o.x() >= 0 && o.x() <= Width && o.y() >= 0 && o.y() <= Height ) )
			{
				int size = int( com->angSize() * scale * dms::PI * Options::zoomFactor()/10800.0 );
				if ( size < 1 ) size = 1;
				int x1 = o.x() - size/2;
				int y1 = o.y() - size/2;
				psky.drawEllipse( x1, y1, size, size );

				//draw Name
				if ( Options::showCometNames() && com->rsun() < Options::maxRadCometName() ) {
					psky.setPen( QColor( ks->data()->colorScheme()->colorNamed( "PNameColor" ) ) );
					drawNameLabel(map, psky, com, o.x(), o.y(), scale );
				}
			}
		}
	}
}

#include "cometscomponent.moc"
