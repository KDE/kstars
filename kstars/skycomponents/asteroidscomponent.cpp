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

#include <QPen>
#include <QPainter>

#include "skycomponent.h"

#include "Options.h"
#include "ksasteroid.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "ksutils.h"
#include "ksfilereader.h"
#include "skymap.h"

#include "solarsystemcomposite.h"
#include "skylabeler.h"

AsteroidsComponent::AsteroidsComponent(SolarSystemComposite *parent, bool (*visibleMethod)(), int msize)
: SolarSystemListComponent(parent, visibleMethod, msize)
{}

AsteroidsComponent::~AsteroidsComponent()
{
	//object deletion handled in grandparent dtor (ListComponent)
}

bool AsteroidsComponent::selected()
{
    return Options::showAsteroids();
}

void AsteroidsComponent::init(KStarsData *data)
{

    QString line, name;
    int mJD;
    double a, e, dble_i, dble_w, dble_N, dble_M, H;
    long double JD;

    KSFileReader fileReader;
 
	if ( ! fileReader.open("asteroids.dat" ) ) return;

	emitProgressText( i18n("Loading asteroids") );

	while( fileReader.hasMoreLines() ) {
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

		ast = new KSAsteroid( data, name, QString(), JD, a, e, dms(dble_i), 
                              dms(dble_w), dms(dble_N), dms(dble_M), H );
		ast->setAngularSize( 0.005 );
		objectList().append( ast );

		//Add name to the list of object names
		objectNames(SkyObject::ASTEROID).append( name );
    }
}

void AsteroidsComponent::update( KStarsData *data, KSNumbers *num )
{
    if ( ! selected() ) return;

    foreach ( SkyObject *o, objectList() ) {
		KSPlanetBase *p = (KSPlanetBase*) o;
		p->EquatorialToHorizontal( data->lst(), data->geo()->lat() );
    }
}

void AsteroidsComponent::draw( KStars *ks, QPainter& psky, double scale)
{
	if ( ! selected() ) return;
	
	SkyMap *map = ks->map();
	bool hideLabels =  ! Options::showAsteroidNames() ||
		( map->isSlewing() && Options::hideLabels() );

	double lgmin = log10(MINZOOM);
	double lgmax = log10(MAXZOOM);
	double lgz = log10(Options::zoomFactor());
	double labelMagLimit  =  Options::magLimitAsteroidName();
	labelMagLimit += ( 20.0 - labelMagLimit ) * ( lgz - lgmin) / (lgmax - lgmin );
	if ( labelMagLimit > 10.0 ) labelMagLimit = 10.0;
	//printf("labelMagLim = %.1f\n", labelMagLimit );

	float sizeFactor = scale * dms::PI * Options::zoomFactor()/10800.0;

    psky.setBrush( QBrush( QColor( "gray" ) ) );

	foreach ( SkyObject *o, objectList() ) { 
		KSAsteroid *ast = (KSAsteroid*) o;

		if ( ast->mag() > Options::magLimitAsteroid() ) continue;
		if ( ! map->checkVisibility( ast ) ) continue;
		
        QPointF o = map->toScreen( ast, scale );
        if ( ! map->onScreen( o ) ) continue;

		float size = ast->angSize() * sizeFactor;
		if ( size < 1.0 ) size = 1.0;
		float x1 = o.x() - 0.5 * size;
		float y1 = o.y() - 0.5 * size;

		if ( Options::useAntialias() )
			psky.drawEllipse( QRectF( x1, y1, size, size ) );
		else
			psky.drawEllipse( QRect( int(x1), int(y1), int(size), int(size) ) );

		if ( hideLabels || ast->mag() >= labelMagLimit ) continue;
		SkyLabeler::AddOffsetLabel( o, ast->translatedName(), ASTEROID_LABEL );
    }
}

SkyObject* AsteroidsComponent::objectNearest( SkyPoint *p, double &maxrad ) {
	SkyObject *oBest = 0;

	if ( ! selected() ) return 0;

	foreach ( SkyObject *o, objectList() ) {
		if ( o->mag() > Options::magLimitAsteroid() ) continue;

		double r = o->angularDistanceTo( p ).Degrees();
		if ( r < maxrad ) {
			oBest = o;
			maxrad = r;
		}
	}

	return oBest;
}

