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


#include "geolocation.h"

GeoLocation::GeoLocation(){
	GeoLocation( 0.0, 0.0 );
}

GeoLocation::GeoLocation( const GeoLocation &g ) {
	Longitude = g.Longitude;
	Latitude  = g.Latitude;
	Name      = g.Name;
	Province  = g.Province;
	Country   = g.Country;
	TimeZone  = g.TimeZone;

	if ( Latitude.Degrees() ==  90.0 ) Latitude.setD(  89.9999 );
	if ( Latitude.Degrees() == -90.0 ) Latitude.setD( -89.9999 );
}

GeoLocation::GeoLocation( GeoLocation *g ) {
	Longitude = g->Longitude;
	Latitude  = g->Latitude;
	Name      = g->Name;
	Province  = g->Province;
	Country   = g->Country;
	TimeZone  = g->TimeZone;

	if ( Latitude.Degrees() ==  90.0 ) Latitude.setD(  89.9999 );
	if ( Latitude.Degrees() == -90.0 ) Latitude.setD( -89.9999 );
}

GeoLocation::GeoLocation( dms lng, dms lat,
				QString name, QString province, QString country, double tz ) {
	Longitude = lng;
	Latitude = lat;
	Name = name;
	Province = province;
	Country = country;
	TimeZone = tz;

	if ( Latitude.Degrees() ==  90.0 ) Latitude.setD(  89.9999 );
	if ( Latitude.Degrees() == -90.0 ) Latitude.setD( -89.9999 );
}

GeoLocation::GeoLocation( double lng, double lat,
				QString name, QString province, QString country, double tz ) {
	Longitude.set( lng );
	Latitude.set( lat );
	Name = name;
	Province = province;
	Country = country;
	TimeZone = tz;

	if ( Latitude.Degrees() ==  90.0 ) Latitude.setD(  89.9999 );
	if ( Latitude.Degrees() == -90.0 ) Latitude.setD( -89.9999 );
}

void GeoLocation::reset( GeoLocation *g ) {
	Longitude = g->lng();
	Latitude  = g->lat();
	Name      = g->name();
	Province  = g->province();
	Country   = g->country();
	TimeZone  = g->TZ();

	if ( Latitude.Degrees() ==  90.0 ) Latitude.setD(  89.9999 );
	if ( Latitude.Degrees() == -90.0 ) Latitude.setD( -89.9999 );
}
