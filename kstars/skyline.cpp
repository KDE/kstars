/***************************************************************************
                          skyline.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Mon June 26 2006
    copyright            : (C) 2006 by Jason Harris
    email                : kstarss@30doradus.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "kstarsdata.h"
#include "ksnumbers.h"
#include "skyline.h"

dms SkyLine::angularSize() {
	double dalpha = startPoint()->ra()->radians() - endPoint()->ra()->radians();
	double ddelta = startPoint()->dec()->radians() - endPoint()->dec()->radians() ;

	double sa = sin(dalpha/2.);
	double sd = sin(ddelta/2.);

	double hava = sa*sa;
	double havd = sd*sd;

	double aux = havd + cos (endPoint()->dec()->radians()) * cos(startPoint()->dec()->radians()) * hava;

	dms angDist;
	angDist.setRadians( 2 * fabs(asin( sqrt(aux) )) );

	return angDist;
}

void SkyLine::setAngularSize( double size ) {
	double pa=positionAngle().radians();

	double x = (startPoint()->ra()->Degrees()  + size*cos(pa))/15.0;
	double y = startPoint()->dec()->Degrees() - size*sin(pa);
	
	setEndPoint( SkyPoint( x, y ) );
}

dms SkyLine::positionAngle() {
	double dx = startPoint()->ra()->radians() - endPoint()->ra()->radians();
	double dy = endPoint()->dec()->radians() - startPoint()->dec()->radians();
	
	return dms( atan2( dy, dx )/dms::DegToRad ); 
}

void SkyLine::rotate( const dms &angle ) {
	double dx = endPoint()->ra()->Degrees()  - startPoint()->ra()->Degrees();
	double dy = endPoint()->dec()->Degrees() - startPoint()->dec()->Degrees();

	double s, c;
	angle.SinCos( s, c );

	double dx0 = dx*c - dy*s;
	double dy0 = dx*s + dy*c;

	double x = (startPoint()->ra()->Degrees() + dx0)/15.0;
	double y = startPoint()->dec()->Degrees() + dy0;

	setEndPoint( SkyPoint( x, y ) );
}

void SkyLine::update( KStarsData *d, KSNumbers *num ) {
	if ( num ) {
		startPoint()->updateCoords( num );
		endPoint()->updateCoords( num );
	}

	startPoint()->EquatorialToHorizontal( d->lst(), d->geo()->lat() );
	endPoint()->EquatorialToHorizontal( d->lst(), d->geo()->lat() );
}
