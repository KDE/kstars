/***************************************************************************
                          geolocation.h  -  K Desktop Planetarium
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




#ifndef GEOLOCATION_H
#define GEOLOCATION_H

#include <qstring.h>
#include <klocale.h>

#include "dms.h"

/**
	*Contains all relevant information for specifying an observing
	*location on Earth: City Name, State/Country Name, Longitude,
	*Latitude, and Time Zone.
	*@short Relevant data about an observing location on Earth.
	*@author Jason Harris
	*@version 0.4
	*/

class GeoLocation {
public: 
/**
	*Default constructor; sets coordinates to zero.	
	*/
	GeoLocation();
/**
	*Copy Constructor
	*/	
	GeoLocation( const GeoLocation &g );
	GeoLocation( GeoLocation *g );
/**
	*Constructor using dms objects to specify longitude and latitude.
	*/
	
	GeoLocation( dms lng, dms lat, QString name="Nowhere", QString province="Nowhere", QString country="Nowhere", double TZ=0 );
/**
	*Constructor using doubles to specify longitude and latitude.
	*/
	GeoLocation( double lng, double lat, QString name="Nowhere", QString province="Nowhere", QString country="Nowhere", double TZ=0 );
/**
	*Destructor (empty)
	*/
	~GeoLocation() {};

/**
	*Returns longitude
	*/	
	dms lng() const { return Longitude; }
/**
	*Returns latitude
	*/
	dms lat() const { return Latitude; }
/**
	*Return untranslated City name
	*/
	QString name() const { return Name; }

/**
	*Return translated City name
	*/
	QString translatedName() const { return i18n(Name.local8Bit().data()); }
/**
	*Return untranslated Province name
	*/
	QString province() const { return Province; }

/**
	*Return translated City name
	*/
	QString translatedProvince() const { return i18n(Province.local8Bit().data()); }
/**
	*Return untranslated State/Country name
	*/
	QString country() const { return Country; }
/**
	*Return translated State/Country name
	*/
	QString translatedCountry() const { return i18n(Country.local8Bit().data()); }
/**
	*Return time zone
	*/
	double TZ() const { return TimeZone; }

/**
	*Set longitude according to argument.
	*/
	void setLong( dms l ) { Longitude = l; }
/**
	*Set longitude according to argument.
	*Differs from above function only in argument type.
	*/
	void setLong( double l ) { Longitude.setD( l ); }
/**
	*Set latitude according to argument.
	*/
	void setLat( dms l ) { Latitude  = l; }
/**
	*Set latitude according to argument.
	*Differs from above function only in argument type.
	*/
	void setLat( double l ) { Latitude.setD( l ); }
/**
	*Set City name according to argument.
	*/
	void setName( const QString &n ) { Name = n; }
/**
	*Set City name according to argument.
	*/
	void setProvince( const QString &n ) { Province = n; }
/**
	*Set State/Country name according to argument.
	*/
	void setCountry( const QString &s ) { Country = s; }
/**
	*Sets Time Zone according to argument.
	*/
	void setTZ( double tz ) { TimeZone = tz; }
/**
	*Set location data to that of the GeoLocation pointed to by argument.
	*Similar to copy constructor.
	*/
	void reset( GeoLocation *g );		

private:
	dms Longitude, Latitude;
	QString Name, Province, Country;
	double TimeZone;
};

#endif
