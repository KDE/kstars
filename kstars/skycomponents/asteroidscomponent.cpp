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

#include <QFile>
#include <QPen>
#include <QPainter>

#include "asteroidscomponent.h"

#include "Options.h"
#include "ksasteroid.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "ksutils.h"
#include "ksfilereader.h"
#include "skymap.h"

AsteroidsComponent::AsteroidsComponent(SolarSystemComposite *parent, bool (*visibleMethod)(), int msize)
: SolarSystemListComponent(parent, visibleMethod, msize)
{
}

AsteroidsComponent::~AsteroidsComponent()
{
	//object deletion handled in grandparent dtor (ListComponent)
}

void AsteroidsComponent::draw( KStars *ks, QPainter& psky, double scale)
{
	if ( !visible() ) return;
	
	SkyMap *map = ks->map();

	float Width = scale * map->width();
	float Height = scale * map->height();

	foreach ( SkyObject *o, objectList() ) { 
		KSAsteroid *ast = (KSAsteroid*)o;
		if ( ast->mag() > Options::magLimitAsteroid() ) break;

		if ( map->checkVisibility( ast ) )
		{
			psky.setPen( QPen( QColor( "gray" ) ) );
			psky.setBrush( QBrush( QColor( "gray" ) ) );
			QPointF o = map->getXY( ast, Options::useAltAz(), Options::useRefraction(), scale );

			if ( ( o.x() >= 0. && o.x() <= Width && o.y() >= 0. && o.y() <= Height ) )
			{
				float size = ast->angSize() * scale * dms::PI * Options::zoomFactor()/10800.0;
				if ( size < 1 ) size = 1.;
				float x1 = o.x() - 0.5*size;
				float y1 = o.y() - 0.5*size;
				psky.drawEllipse( QRectF( x1, y1, size, size ) );

				//draw Name
				if ( Options::showAsteroidNames() && ast->mag() < Options::magLimitAsteroidName() ) {
					psky.setPen( QColor( ks->data()->colorScheme()->colorNamed( "PNameColor" ) ) );
					drawNameLabel( psky, ast, o.x(), o.y(), scale );
				}
			}
		}
	}
}

void AsteroidsComponent::init(KStarsData *data)
{
	QFile file;

	if ( KSUtils::openDataFile( file, "asteroids.dat" ) ) {
		emitProgressText( i18n("Loading asteroids") );

		KSFileReader fileReader( file );
		while( fileReader.hasMoreLines() ) {
			QString line, name;
			int mJD;
			double a, e, dble_i, dble_w, dble_N, dble_M, H;
			long double JD;
			KSAsteroid *ast = 0;

			line = fileReader.readLine();
			name = line.mid( 6, 17 ).trimmed();
			mJD  = line.mid( 24, 5 ).toInt();
			a    = line.mid( 30, 9 ).toDouble();
			e    = line.mid( 41, 10 ).toDouble();
			dble_i = line.mid( 52, 9 ).toDouble();
			dble_w = line.mid( 62, 9 ).toDouble();
			dble_N = line.mid( 72, 9 ).toDouble();
			dble_M = line.mid( 82, 11 ).toDouble();
			H = line.mid( 94, 5 ).toDouble();

			JD = double( mJD ) + 2400000.5;

			ast = new KSAsteroid( data, name, QString(), JD, a, e, dms(dble_i), dms(dble_w), dms(dble_N), dms(dble_M), H );
			ast->setAngularSize( 0.005 );
			objectList().append( ast );

			//Add name to the list of object names
			objectNames().append( name );
		}
	}
}
