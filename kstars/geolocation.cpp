/***************************************************************************
                          geolocation.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Sun Feb 11 2001
    copyright            : (C) 2001 by Jason Harris
    email                : jharris@30doradus.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <qstring.h>

#include "geolocation.h"
#include "timezonerule.h"

GeoLocation::GeoLocation(){
	GeoLocation( 0.0, 0.0 );
	TZrule = NULL;
}

GeoLocation::GeoLocation( const GeoLocation &g ) {
	Longitude = g.Longitude;
	Latitude  = g.Latitude;
	Name      = g.Name;
	Province  = g.Province;
	Country   = g.Country;
	TimeZone  = g.TimeZone;
	TZrule    = g.TZrule;
	Height    = g.Height;
	indexEllipsoid = g.indexEllipsoid;
	setEllipsoid ( indexEllipsoid );
	geodToCart();
}

GeoLocation::GeoLocation( GeoLocation *g ) {
	Longitude = g->Longitude;
	Latitude  = g->Latitude;
	Name      = g->Name;
	Province  = g->Province;
	Country   = g->Country;
	TimeZone  = g->TimeZone;
	TZrule    = g->TZrule;
	Height    = g->Height;
	indexEllipsoid = g->indexEllipsoid;
	setEllipsoid ( indexEllipsoid );
	geodToCart();
}

GeoLocation::GeoLocation( dms lng, dms lat,
				QString name, QString province, QString country, double tz, TimeZoneRule *tzrule, int iEllips, double hght ) {
	Longitude = lng;
	Latitude = lat;
	Name = name;
	Province = province;
	Country = country;
	TimeZone = tz;
	TZrule = tzrule;
	Height = hght;
	indexEllipsoid = iEllips;
	setEllipsoid ( indexEllipsoid );
	geodToCart();
}

GeoLocation::GeoLocation( double lng, double lat,
				QString name, QString province, QString country, double tz, TimeZoneRule *tzrule, int iEllips, double hght ) {
	Longitude.set( lng );
	Latitude.set( lat );
	Name = name;
	Province = province;
	Country = country;
	TimeZone = tz;
	TZrule = tzrule;
	Height = hght;
	indexEllipsoid = iEllips;
	setEllipsoid ( indexEllipsoid );
	geodToCart();
}

GeoLocation::GeoLocation( double x, double y, double z, QString name, QString province, QString country, double TZ, TimeZoneRule *tzrule, int iEllips ) {
	PosCartX = x;
	PosCartY = y;
	PosCartZ = z;
	Name = name;
	Province = province;
	Country = country;
	TimeZone = TZ;
	TZrule = tzrule;
	indexEllipsoid = iEllips;
	setEllipsoid ( indexEllipsoid );
	cartToGeod();
}

void GeoLocation::reset( GeoLocation *g ) {
	indexEllipsoid = g->ellipsoid();
	setEllipsoid ( indexEllipsoid );
	setLong( g->lng()->Degrees() );
	setLat( g->lat()->Degrees() );
	Name      = g->name();
	Province  = g->province();
	Country   = g->country();
	TimeZone  = g->TZ();
	TZrule    = g->tzrule();
	Height    = g->height();
}


void GeoLocation::setEllipsoid(int index) {
	static const double A[] = { 6378140.0, 6378137.0, 6378137.0, 6378137.0, 6378136.0 };
	static const double F[] = { 0.0033528131779, 0.0033528106812, 0.0033528131779, 0.00335281066474, 0.0033528131779 };

	axis = A[index];
	flattening = F[index];	
}

void GeoLocation::changeEllipsoid(int index) {
	
	setEllipsoid(index);
	cartToGeod();
	
}

void GeoLocation::cartToGeod(void)
{
	static const double RIT = 2.7778e-6;
	double e2, rpro, lat1, xn, s1, sqrtP2, latd, sinl;

	e2 = 2*flattening-flattening*flattening;

	sqrtP2 = sqrt(PosCartX*PosCartX+PosCartY*PosCartY);

	rpro = PosCartZ/sqrtP2;
	latd = atan(rpro/(1-e2));
	lat1 = 0.;

	while ( fabs( latd-lat1 ) > RIT ) {
		lat1 = latd;
		s1 = sin(lat1);
		xn = axis/(sqrt(1-e2*s1*s1));
		latd = atan( rpro*(1+e2*xn*s1/PosCartZ) );
	}

	sinl = sin(latd);
	xn = axis/( sqrt(1-e2*sinl*sinl) );

	Height = sqrtP2/cos(latd)-xn;
	Longitude.setRadians( atan2(PosCartY,PosCartX) );
	Latitude.setRadians(latd);
}

void GeoLocation::geodToCart (void) {

	double e2, xn;
	double sinLong, cosLong, sinLat, cosLat;

	e2 = 2*flattening-flattening*flattening;
	
	Longitude.SinCos(sinLong,cosLong);
	Latitude.SinCos(sinLat,cosLat);
	
	xn = axis/( sqrt(1-e2*sinLat*sinLat) );
	PosCartX = (xn+Height)*cosLat*cosLong;
	PosCartY = (xn+Height)*cosLat*sinLong;
	PosCartZ = (xn*(1-e2)+Height)*sinLat;

}
