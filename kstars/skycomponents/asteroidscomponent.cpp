/***************************************************************************
                          asteroidscomponent.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 2005/30/08
    copyright            : (C) 2005 by Thomas Kabelmann
    email                : thomas.kabelmann@gmx.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "asteroidscomponent.h"

#include <QFile>

#include "ksasteroid.h"
#include "kstarsdata.h"
#include "ksutils.h"

AsteroidsComponent::AsteroidsComponent(SolarSystemComposite *parent, bool (*visibleMethod)(), int msize)
: AbstractPlanetComponent(parent, visibleMethod, msize)
{
}

AsteroidsComponent::~AsteroidsComponent()
{
}

void AsteroidsComponent::drawTrail(SkyMap *map, QPainter& psky, double scale)
{
	if ( visible() ) {
		for ( KSAsteroid *ast = asteroidList.first(); ast; ast = asteroidList.next() )
		{
			if ( ast->mag() > Options::magLimitAsteroid() ) break;
			// will be drawn only if available
			drawPlanetTrail(map, psky, ast, scale);
		}
	}
}

void AsteroidsComponent::draw(SkyMap *map, QPainter& psky, double scale)
{
	if ( !visible() ) return;
	
	for ( KSAsteroid *ast = asteroidList.first(); ast; ast = asteroidList.next() ) {
		if ( ast->mag() > Options::magLimitAsteroid() ) break;

		if ( map->checkVisibility( ast, fov(), XRange ) )
		{
			psky.setPen( QPen( QColor( "gray" ) ) );
			psky.setBrush( QBrush( QColor( "gray" ) ) );
			QPoint o = getXY( ast, Options::useAltAz(), Options::useRefraction(), scale );

			if ( ( o.x() >= 0 && o.x() <= Width && o.y() >= 0 && o.y() <= Height ) )
			{
				int size = int( ast->angSize() * scale * dms::PI * Options::zoomFactor()/10800.0 );
				if ( size < 1 ) size = 1;
				int x1 = o.x() - size/2;
				int y1 = o.y() - size/2;
				psky.drawEllipse( x1, y1, size, size );

				//draw Name
				if ( Options::showAsteroidNames() && ast->mag() < Options::magLimitAsteroidName() ) {
					psky.setPen( QColor( map->data()->colorScheme()->colorNamed( "PNameColor" ) ) );
					drawNameLabel(map, psky, ast, o.x(), o.y(), scale );
				}
			}
		}
	}
}

void AsteroidsComponent::init(KStarsData *data)
{
	readAsteroidData(data);
}

void AsteroidsComponent::updatePlanets(KStarsData *data, KSNumbers *num, bool needNewCoords)
{
//	if ( Options::showPlanets() && Options::showAsteroids() )
	if ( visible() )
		for ( KSAsteroid *ast = asteroidList.first(); ast; ast = asteroidList.next() )
		{
			ast->findPosition( num, data->geo->lat(), data->LST, earth() );
		}
}

void AsteroidsComponent::update(KStarsData *data, KSNumbers *num, bool needNewCoords)
{
	if ( visible() )
	{
		for ( KSAsteroid *ast = asteroidList.first(); ast; ast = asteroidList.next() )
		{
			ast->EquatorialToHorizontal( LST, geo->lat() );
			if ( ast->hasTrail() )
			{
				ast->updateTrail( LST, data->geo()->lat() );
			}
		}
	}
}

bool AsteroidsComponent::readAsteroidData(KStarsData *data)
{
	QFile file;

	if ( KSUtils::openDataFile( file, "asteroids.dat" ) ) {
		KSFileReader fileReader( file );

		while( fileReader.hasMoreLines() ) {
			QString line, name;
			int mJD;
			double a, e, dble_i, dble_w, dble_N, dble_M, H;
			long double JD;
			KSAsteroid *ast = 0;

			line = fileReader.readLine();
			name = line.mid( 6, 17 ).stripWhiteSpace();
			mJD  = line.mid( 24, 5 ).toInt();
			a    = line.mid( 30, 9 ).toDouble();
			e    = line.mid( 41, 10 ).toDouble();
			dble_i = line.mid( 52, 9 ).toDouble();
			dble_w = line.mid( 62, 9 ).toDouble();
			dble_N = line.mid( 72, 9 ).toDouble();
			dble_M = line.mid( 82, 11 ).toDouble();
			H = line.mid( 94, 5 ).toDouble();

			JD = double( mJD ) + 2400000.5;

			ast = new KSAsteroid( this, name, "", JD, a, e, dms(dble_i), dms(dble_w), dms(dble_N), dms(dble_M), H );
			ast->setAngularSize( 0.005 );
			asteroidList.append( ast );
			data->ObjNames.append( ast );
		}

		if ( asteroidList.count() ) return true;
	}

	return false;
}
