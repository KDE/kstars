/***************************************************************************
                          geolocation.h  -  K Desktop Planetarium
                             -------------------
    begin                : Sun Feb 11 2001
    copyright            : (C) 2001-2005 by Jason Harris
    email                : jharris@30doradus.org
    copyright            : (C) 2003-2005 by Pablo de Vicente
    email                : p.devicente@wanadoo.es
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

#include <klocale.h>

#include "dms.h"
#include "timezonerule.h"
#include "kstarsdatetime.h"

/**@class GeoLocation
	*Contains all relevant information for specifying a location 
	*on Earth: City Name, State/Province name, Country Name,
	*Longitude, Latitude, Elevation, Time Zone, and Daylight Savings 
	*Time rule.
	*@short Relevant data about an observing location on Earth.
	*@author Jason Harris
	*@version 1.0
	*/

class GeoLocation {
public: 
/**
	*Default constructor; sets coordinates to zero.	
	*/
	GeoLocation();

/**Copy Constructor
	*@param g the GeoLocation to duplicate
	*/
	GeoLocation( const GeoLocation &g );

/**Copy Constructor.  Differs from the above function only in argument type.
	*@param g pointer to the GeoLocation to duplicate
	*/
	GeoLocation( GeoLocation *g );

/**Constructor using dms objects to specify longitude and latitude.
	*@param lng the longitude
	*@param lat the latitude
	*@param name the name of the city/town/location
	*@param province the name of the province/US state
	*@param country the name of the country
	*@param TZ the base time zone offset from Greenwich, UK
	*@param TZrule pointer to the daylight savings time rule
	*@param iEllips type of geodetic ellipsoid model
	*@param hght the elevation above sea level (in meters?)
	*/
	GeoLocation( dms lng, dms lat, QString name="Nowhere", QString province="Nowhere", QString country="Nowhere", double TZ=0, TimeZoneRule *TZrule=NULL, int iEllips=4, double hght=-10 );

/**Constructor using doubles to specify longitude and latitude.
	*@param lng the longitude
	*@param lat the latitude
	*@param name the name of the city/town/location
	*@param province the name of the province/US state
	*@param country the name of the country
	*@param TZ the base time zone offset from Greenwich, UK
	*@param TZrule pointer to the daylight savings time rule
	*@param iEllips type of geodetic ellipsoid model
	*@param hght the elevation above sea level (in meters?)
	*/
	GeoLocation( double lng, double lat, QString name="Nowhere", QString province="Nowhere", QString country="Nowhere", double TZ=0, TimeZoneRule *TZrule=NULL, int iEllips=4, double hght=-10 );

/**Constructor using doubles to specify X, Y and Z referred to the center of the Earth.
	*@param x the x-position, in m
	*@param y the y-position, in m
	*@param z the z-position, in m
	*@param name the name of the city/town/location
	*@param province the name of the province/US state
	*@param country the name of the country
	*@param TZ the base time zone offset from Greenwich, UK
	*@param TZrule pointer to the daylight savings time rule
	*@param iEllips type of geodetic ellipsoid model
	*/
	GeoLocation( double x, double y, double z, QString name="Nowhere", QString province="Nowhere", QString country="Nowhere", double TZ=0, TimeZoneRule *TZrule=NULL, int iEllips=4 );


/**Destructor (empty)
	*/
	~GeoLocation() {}

/**@return pointer to the longitude dms object
	*/
	const dms* lng() const { return &Longitude; }
/**@return pointer to the latitude dms object
	*/
	const dms* lat() const { return &Latitude; }
/**@return elevation above seal level (meters)
	*/
	double height() const { return Height; }
/**@return X position in m
	*/
	double xPos() const { return PosCartX; }
/**@return Y position in m
	*/
	double yPos() const { return PosCartY; }
/**@return Z position in m
	*/
	double zPos() const { return PosCartZ; }
/**@return index identifying the geodetic ellipsoid model
	*/
	int ellipsoid() const { return indexEllipsoid; }

/**@return untranslated City name
	*/
	QString name() const { return Name; }
/**@return translated City name
	*/
	QString translatedName() const { return i18n("City name (optional, probably does not need a translation)", Name.utf8().data()); }
/**@return untranslated Province name
	*/
	QString province() const { return Province; }
/**@return translated Province name
	*/
	QString translatedProvince() const { return i18n("Region/state name (optional, rarely needs a translation)", Province.utf8().data()); }
/**@return untranslated Country name
	*/
	QString country() const { return Country; }
/**@return translated Country name
 */
	QString translatedCountry() const { return i18n("Country name (optional, but should be translated)", Country.utf8().data()); }

/**@return comma-separated city, province, country names (each localized)
 */
	QString fullName() const;

/**@return time zone without DST correction
	*/
	double TZ0() const { return TimeZone; }

/**@return time zone, including any DST correction.
	*/
	double TZ() const { return TimeZone + TZrule->deltaTZ(); }

/**@return pointer to time zone rule object
	*/
	TimeZoneRule* tzrule() const { return TZrule; }

/**Set longitude according to dms argument.
	*@param l the new longitude
	*/
	void setLong( dms l ) { 
		Longitude = l; 
		geodToCart();
	}
/**Set longitude according to argument.
	*Differs from above function only in argument type.
	*@param l the new longitude
	*/
	void setLong( double l ) { 
		Longitude.setD( l ); 
		geodToCart();
	}

/**Set latitude according to dms argument.
	*@param l the new latitude
	*/
	void setLat( dms l ) { 
		Latitude  = l; 
		geodToCart();
	}
	
/**Set latitude according to argument.
	*Differs from above function only in argument type.
	*@param l the new latitude
	*/
	void setLat( double l ) { 
		Latitude.setD( l ); 
		geodToCart();
	}

/**Set elevation above sea level
	*@param hg the new elevation (meters)
	*/
	void setHeight( double hg ) { 
		Height  = hg; 
		geodToCart();
	}

/**Set X 
	*@param x the new x-position (meters)
	*/
	void setXPos( double x ) { 
		PosCartX  = x; 
		cartToGeod();
	}
/**Set Y 
	*@param y the new y-position (meters)
	*/
	void setYPos( double y ) { 
		PosCartY  = y; 
		cartToGeod();
	}
/**Set Z 
	*@param z the new z-position (meters)
	*/
	void setZPos( double z ) { 
		PosCartZ  = z; 
		cartToGeod();
	}
	
/**Update Latitude, Longitude and Height according to new ellipsoid. These are
	*computed from XYZ which do NOT change on changing the ellipsoid.
	*@p i = index to identify the ellipsoid
	*/
	void changeEllipsoid( int i );
	
/**Set City name according to argument.
	*@p n new city name
	*/
	void setName( const QString &n ) { Name = n; }

/**Set Province name according to argument.
	*@p n new province name
	*/
	void setProvince( const QString &n ) { Province = n; }

/**Set Country name according to argument.
	*@p n new country name
	*/
	void setCountry( const QString &n ) { Country = n; }

/**Sets Time Zone according to argument.
	*@p tz new timezone offset
	*/
	void setTZ( double tz ) { TimeZone = tz; }

/**Sets DST rule pointer according to argument.
	*@p txr pointer to the new DST rule
	*/
	void setTZrule( TimeZoneRule *tzr ) { TZrule = tzr; }

/**Set location data to that of the GeoLocation pointed to by argument.
	*Similar to copy constructor.
	*@param g pointer to the GeoLocation which should be duplicated.
	*/
	void reset( GeoLocation *g );

/**Converts from cartesian coordinates in meters to longitude, 
	*latitude and height on a standard geoid for the Earth. The 
	*geoid is characterized by two parameters: the semimajor axis
	*and the flattening.
	*
	*@note The astronomical zenith is defined as the perpendicular to 
	*the real geoid. The geodetic zenith is the perpendicular to the 
	*standard geoid. Both zeniths differ due to local gravitational 
	*anomalies.
	*
	* Algorithm is from "GPS Satellite Surveying", A. Leick, page 184.
	*/
	void cartToGeod(void);

/**Converts from longitude, latitude and height on a standard 
	*geoid of the Earth to cartesian coordinates in meters. The geoid
	*is characterized by two parameters: the semimajor axis and the 
	*flattening.
	*
	*@note The astronomical zenith is defined as the perpendicular to 
	*the real geoid. The geodetic zenith is the perpendicular to the 
	*standard geoid. Both zeniths differ due to local gravitational 
	*anomalies.
	*
	*Algorithm is from "GPS Satellite Surveying", A. Leick, page 184.
	*/
	void geodToCart (void);

/**The geoid is an elliposid which fits the shape of the Earth. It is
	*characterized by two parameters: the semimajor axis and the
	*flattening.
	*
	*@p index is the index which allows to identify the parameters for the
	*chosen elliposid. 1="IAU76", 2="GRS80", 3="MERIT83", 4="WGS84", 
	*5="IERS89"};
	*/
	void setEllipsoid( int i );

	dms GSTtoLST( const dms &gst ) const { return dms( gst.Degrees() + Longitude.Degrees() ); }
	dms LSTtoGST( const dms &lst ) const { return dms( lst.Degrees() - Longitude.Degrees() ); }

	KStarsDateTime UTtoLT( const KStarsDateTime &ut ) const { return ut.addSecs( int( 3600.*TZ() ) ); }
	KStarsDateTime LTtoUT( const KStarsDateTime &lt ) const { return lt.addSecs( int( -3600.*TZ() ) ); }


	/* Computes the velocity in km/s of an observer on the surface of the Earth 
	 * referred to a system whose origin is the center of the Earth. The X and 
	 * Y axis are contained in the equator and the X axis is towards the nodes
	 * line. The Z axis is along the poles.
	 *
	 * @param vtopo[] Topocentric velocity. The resultant velocity is available 
	 *        in this array.
	 * @param gt. Greenwich sideral time for which we want to compute the topocentric velocity.
	 */
	void TopocentricVelocity(double vtopo[], dms gt);

private:
	dms Longitude, Latitude;
	QString Name, Province, Country;
	TimeZoneRule *TZrule;
	double TimeZone, Height;
	double axis, flattening;
	long double PosCartX, PosCartY, PosCartZ;
	int indexEllipsoid;
};

#endif
