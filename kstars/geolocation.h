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

#include "timezonerule.h"
#include "dms.h"

/**
	*Contains all relevant information for specifying an observing
	*location on Earth: City Name, State/Province name, Country Name,
	*Longitude, Latitude, and Time Zone.
	*@short Relevant data about an observing location on Earth.
	*@author Jason Harris
	*@version 0.9
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

/**
	*Copy Constructor.  Differs from the above function only in argument type.
	*/	
	GeoLocation( GeoLocation *g );

/**
	*Constructor using dms objects to specify longitude, latitude and 
	*height for a given ellipsoid.
	*/
	GeoLocation( dms lng, dms lat, QString name="Nowhere", QString province="Nowhere", QString country="Nowhere", double TZ=0, TimeZoneRule *TZrule=NULL, int iEllips=4, double hght=-10 );

/**
	*Constructor using doubles to specify longitude, latitude and height.
	*/
	GeoLocation( double lng, double lat, QString name="Nowhere", QString province="Nowhere", QString country="Nowhere", double TZ=0, TimeZoneRule *TZrule=NULL, int iEllips=4, double hght=-10 );

/**
	*Constructor using doubles to specify X, Y and Z referred to the center
	*of the Earth.
	*/
	GeoLocation( double x, double y, double z, QString name="Nowhere", QString province="Nowhere", QString country="Nowhere", double TZ=0, TimeZoneRule *TZrule=NULL, int iEllips=4 );


/**
	*Destructor (empty)
	*/
	~GeoLocation() {};

/**
	*@returns longitude
	*/	
	const dms* lng() const { return &Longitude; }
/**
	*@returns latitude
	*/
	const dms* lat() const { return &Latitude; }
/**
	*@returns height
	*/
	double height() const { return Height; }
/**
	*@returns X position
	*/
	double xPos() const { return PosCartX; }
/**
	*@returns Y position
	*/
	double yPos() const { return PosCartY; }
/**
	*@returns Z position
	*/
	double zPos() const { return PosCartZ; }
/**
	*@returns index identifying the Earth ellipsoid
	*/
	int ellipsoid() const { return indexEllipsoid; }
/**
	*@returns untranslated City name
	*/
	QString name() const { return Name; }

/**
	*@returns translated City name
	*/
	QString translatedName() const { return i18n("City name (optional, probably does not need a translation)", Name.local8Bit().data()); }
/**
	*@returns untranslated Province name
	*/
	QString province() const { return Province; }

/**
	*@returns translated Province name
	*/
	QString translatedProvince() const { return i18n("Region/state name (optional, rarely needs a translation)", Province.local8Bit().data()); }
/**
	*@returns untranslated Country name
	*/
	QString country() const { return Country; }
/**
	*@returns translated Country name
	*/
	QString translatedCountry() const { return i18n("Country name (optional, but should be translated)", Country.local8Bit().data()); }

/**
	*@returns time zone without DST correction
	*/
	double TZ0() const { return TimeZone; }

/**
	*@returns time zone
	*/
	double TZ() const { return TimeZone + TZrule->deltaTZ(); }

/**
	*@returns pointer to time zone rule object
	*/
	TimeZoneRule* tzrule() const { return TZrule; }

/**
	*Set longitude according to argument.
	*/
	void setLong( dms l ) { 
		Longitude = l; 
		geodToCart();
	}

/**
	*Set longitude according to argument.
	*Differs from above function only in argument type.
	*/
	void setLong( double l ) { 
		Longitude.setD( l ); 
		geodToCart();
	}

/**
	*Set latitude according to argument.
	*/
	void setLat( dms l ) { 
		Latitude  = l; 
		geodToCart();
	}
	
/**
	*Set latitude according to argument.
	*Differs from above function only in argument type.
	*/
	void setLat( double l ) { 
		Latitude.setD( l ); 
		geodToCart();
	}
/**
	*Set height 
	*/
	void setHeight( double hg ) { 
		Height  = hg; 
		geodToCart();
	}
/**
	*Set X 
	*/
	void setXPos( double x ) { 
		PosCartX  = x; 
		cartToGeod();
	}
/**
	*Set Y 
	*/
	void setYPos( double y ) { 
		PosCartY  = y; 
		cartToGeod();
	}
/**
	*Set Z 
	*/
	void setZPos( double z ) { 
		PosCartZ  = z; 
		cartToGeod();
	}
	
/**
	*Changes Latitude, Longitude and Height according to new ellipsoid. These are
	*computed from XYZ which do NOT change on changing the ellipsoid.
	*@p i = index to identify the ellipsoid
	*/
	void changeEllipsoid( int i );
	
/**
	*Set City name according to argument.
	*/
	void setName( const QString &n ) { Name = n; }

/**
	*Set Province name according to argument.
	*/
	void setProvince( const QString &n ) { Province = n; }

/**
	*Set Country name according to argument.
	*/
	void setCountry( const QString &s ) { Country = s; }

/**
	*Sets Time Zone according to argument.
	*/
	void setTZ( double tz ) { TimeZone = tz; }

/**
	*Sets Time Zone according to argument.
	*/
	void setTZrule( TimeZoneRule *tzr ) { TZrule = tzr; }

/**
	*Set location data to that of the GeoLocation pointed to by argument.
	*Similar to copy constructor.
	*/
	void reset( GeoLocation *g );

/**
	* Converts from cartesian coordinates in meters to longitude, 
	* latitude and height on a standard geoid for the Earth. The 
	* geoid is characterized by two parameters: the semimajor axis
	* and the flattening.
	*
	* Note: The astronomical zenith is defined as the perpendicular to 
	* the real geoid. The geodetic zenith is the perpendicular to the 
	* standard geoid. Both zeniths differ due to local gravitational 
	* anomalies.
	*
	* Algorithm is from "GPS Satellite Surveying", A. Leick, page 184.
	*/

	void cartToGeod(void);

/**
	* Converts from longitude, latitude and height on a standard 
	* geoid of the Earth to cartesian coordinates in meters. The geoid
	* is characterized by two parameters: the semimajor axis and the 
	* flattening.
	*
	* Note: The astronomical zenith is defined as the perpendicular to 
	* the real geoid. The geodetic zenith is the perpendicular to the 
	* standard geoid. Both zeniths differ due to local gravitational 
	* anomalies.
	*
	* Algorithm is from "GPS Satellite Surveying", A. Leick, page 184.
	*/

	void geodToCart (void);

private:
	dms Longitude, Latitude;
	QString Name, Province, Country;
	TimeZoneRule *TZrule;
	double TimeZone, Height;
	double axis, flattening;
	long double PosCartX, PosCartY, PosCartZ;
	int indexEllipsoid;

/**
	* The geoid is an elliposid which fits the shape of the Earth. It is
	* characterized by two parameters: the semimajor axis and the
	* flattening.
	*
	* @p index is the index which allows to identify the parameters for the
	* chosen elliposid. 1="IAU76", 2="GRS80", 3="MERIT83", 4="WGS84", 
	* 5="IERS89"};
	*/

	void setEllipsoid( int i );


};

#endif
