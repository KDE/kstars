/***************************************************************************
                          skypoint.h  -  K Desktop Planetarium
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




#ifndef SKYPOINT_H
#define SKYPOINT_H

#include <qstring.h>

#include "ksnumbers.h"
#include "dms.h"


/**@class SkyPoint
	*@short Stores dms coordinates for a point in the sky, and functions
	*for converting between coordinate systems.
	*@author Jason Harris
	*@version 1.0
	*
	*The sky coordinates of a point in the sky.  The
	*coordinates are stored in both Equatorial (Right Ascension,
	*Declination) and Horizontal (Azimuth, Altitude) coordinate systems.
	*Provides set/get functions for each coordinate angle, and functions
	*to convert between the Equatorial and Horizon coordinate systems.
	*
	*Because the coordinate values change slowly over time (due to
	*precession, nutation), the "catalog coordinates" are stored
	*(RA0, Dec0), which were the true coordinates on Jan 1, 2000.
	*The true coordinates (RA, Dec) at any other epoch can be found
	*from the catalog coordinates using updateCoords().
	*/

class SkyPoint {
public:
/**Default constructor: Sets RA, Dec and RA0, Dec0 according
	*to arguments.  Does not set Altitude or Azimuth.
	*@param r Right Ascension
	*@param d Declination
	*/
	SkyPoint( dms r, dms d ) { set( r, d ); }

/**Alternate constructor using pointer arguments, for convenience.
	*It behaves essentially like the default constructor.
	*@param r Right Ascension pointer
	*@param d Declination pointer
	*/
	SkyPoint( const dms *r, const dms *d ) { set( dms(*r), dms(*d) ); }

/**Alternate constructor using double arguments, for convenience.
	*It behaves essentially like the default constructor.
	*@param r Right Ascension, expressed as a double
	*@param d Declination, expressed as a double
	*/
	SkyPoint( double r=0.0, double d=0.0 ) { set( r, d ); }

/**
	*Empty destructor.
	*/
	virtual ~SkyPoint();

/**Sets RA, Dec and RA0, Dec0 according to arguments.
	*Does not set Altitude or Azimuth.
	*@param r Right Ascension
	*@param d Declination
	*/
	void set( dms r, dms d );

/**Overloaded member function, provided for convenience.
	*It behaves essentially like the above function.
	*@param r Right Ascension
	*@param d Declination
	*/
	void set( const dms *r, const dms *d ) { set( dms(*r), dms(*d) ); }

/**Overloaded member function, provided for convenience.
	*It behaves essentially like the above function.
	*@param r Right Ascension
	*@param d Declination
	*/
	void set( double r, double d );

/**Sets RA0, the catalog Right Ascension.
	*@param r catalog Right Ascension.
	*/
	void setRA0( dms r ) { RA0.set( r ); }

/**Overloaded member function, provided for convenience.
	*It behaves essentially like the above function.
	*@param r Right Ascension, expressed as a double.
	*/
	void setRA0( double r ) { RA0.setH( r ); }

/**Sets Dec0, the catalog Declination.
	*@param d catalog Declination.
	*/
	void setDec0( dms d ) { Dec0.set( d ); }

/**Overloaded member function, provided for convenience.
	*It behaves essentially like the above function.
	*@param d Declination, expressed as a double.
	*/
	void setDec0( double d ) { Dec0.setD( d ); }

/**Sets RA, the current Right Ascension.
	*@param r Right Ascension.
	*/
	void setRA( dms r ) { RA.set( r ); }

/**Overloaded member function, provided for convenience.
	*It behaves essentially like the above function.
	*@param r Right Ascension, expressed as a double.
	*/
	void setRA( double r ) { RA.setH( r ); }

/**Sets Dec, the current Declination
	*@param d Declination.
	*/
	void setDec( dms d ) { Dec.set( d ); }

/**Overloaded member function, provided for convenience.
	*It behaves essentially like the above function.
	*@param d Declination, expressed as a double.
	*/
	void setDec( double d ) { Dec.setD( d ); }

/**Sets Alt, the Altitude.
	*@param alt Altitude.
	*/
	void setAlt( dms alt ) { Alt.set( alt ); }

/**Overloaded member function, provided for convenience.
	*It behaves essentially like the above function.
	*@param alt Altitude, expressed as a double.
	*/
	void setAlt( double alt ) { Alt.setD( alt ); }

/**Sets Az, the Azimuth.
	*@param az Azimuth.
	*/
	void setAz( dms az ) { Az.set( az ); }

/**Overloaded member function, provided for convenience.
	*It behaves essentially like the above function.
	*@param az Azimuth, expressed as a double.
	*/
	void setAz( double az ) { Az.setD( az ); }

/**Sets Galactic Longitude.
	*@param glo Galactic Longitude.
	*/
	void setGalLong( dms glo ) { galLong.set( glo ); }

/**Overloaded member function, provided for convenience.
	*It behaves essentially like the above function.
	*@param glo Galactic Longitude, expressed as a double.
	*/
	void setGalLong( double glo ) { galLong.setD( glo ); }

/**Sets Galactic Longitude.
	*@param gla Galactic Longitude.
	*/
	void setGalLat( dms gla ) { galLat.set( gla ); }

/**Overloaded member function, provided for convenience.
	*It behaves essentially like the above function.
	*@param gla Galactic Longitude, expressed as a double.
	*/
	void setGalLat( double gla ) { galLat.setD( gla ); }

/**@return a pointer to the catalog Right Ascension.
	*/
	const dms* ra0() const { return &RA0; };

/**@return a pointer to the catalog Declination.
	*/
	const dms* dec0() const { return &Dec0; };

/**@returns a pointer to the current Right Ascension.
	*/
	const dms* ra() const { return &RA; }

/**@return a pointer to the current Declination.
	*/
	const dms* dec() const { return &Dec; }

/**@return a pointer to the current Azimuth.
	*/
	const dms* az() const { return &Az; }

/**@return a pointer to the current Altitude.
	*/
	const dms* alt() const { return &Alt; }

/**@return a pointer to the current galactic latitude.
	*/
	const dms* gLat() const { return &galLat; }

/**@return a pointer to the current galactic longitude.
	*/
	const dms* gLong() const { return &galLong; }

/**Determine the (Altitude, Azimuth) coordinates of the
	*SkyPoint from its (RA, Dec) coordinates, given the local
	*sidereal time and the observer's latitude.
	*@param LST pointer to the local sidereal time
	*@param lat pointer to the geographic latitude
	*/
	void EquatorialToHorizontal( const dms* LST, const dms* lat );

/**Determine the (RA, Dec) coordinates of the
	*SkyPoint from its (Altitude, Azimuth) coordinates, given the local
	*sidereal time and the observer's latitude.
	*@param LST pointer to the local sidereal time
	*@param lat pointer to the geographic latitude
	*/
	void HorizontalToEquatorial( const dms* LST, const dms* lat );

/**Determine the Ecliptic coordinates of the SkyPoint, given the Julian Date.
	*The ecliptic coordinates are returned as reference arguments (since
	*they are not stored internally)
	*/
	void findEcliptic( const dms *Obliquity, dms &EcLong, dms &EcLat );

/**Set the current (RA, Dec) coordinates of the
	*SkyPoint, given pointers to its Ecliptic (Long, Lat) coordinates, and
	*to the current obliquity angle (the angle between the equator and ecliptic).
	*/
	void setFromEcliptic( const dms *Obliquity, const dms *EcLong, const dms *EcLat );

/**Determine the current coordinates (RA, Dec) from the catalog
	*coordinates (RA0, Dec0), accounting for both precession and nutation.
	*@param num pointer to KSNumbers object containing current values of
	*time-dependent variables.
	*@param includePlanets does nothing in this implementation (see KSPlanetBase::updateCoords()).
	*@param lat does nothing in this implementation (see KSPlanetBase::updateCoords()).
	*@param LST does nothing in this implementation (see KSPlanetBase::updateCoords()).
	*/
	virtual void updateCoords( KSNumbers *num, bool includePlanets=true, const dms *lat=0, const dms *LST=0 );

/**Determine the effects of nutation for this SkyPoint.
	*@param num pointer to KSNumbers object containing current values of
	*time-dependent variables.
	*/
	void nutate(const KSNumbers *num);

/**Determine the effects of aberration for this SkyPoint.
	*@param num pointer to KSNumbers object containing current values of
	*time-dependent variables.
	*/
	void aberrate(const KSNumbers *num);

/**Computes the apparent coordinates for this SkyPoint for any epoch,
	*accounting for the effects of precession, nutation, and aberration.
	*Similar to updateCoords(), but the starting epoch need not be
	*J2000, and the target epoch need not be the present time.
	*@param jd0 Julian Day which identifies the original epoch
	*@param jdf Julian Day which identifies the final epoch
	*/
	void apparentCoord(long double jd0, long double jdf);

/**General case of precession. It precess from an original epoch to a
	*final epoch. In this case RA0, and Dec0 from SkyPoint object represent
	*the coordinates for the original epoch and not for J2000, as usual.
	*@param jd0 Julian Day which identifies the original epoch
	*@param jdf Julian Day which identifies the final epoch
	*/
	void precessFromAnyEpoch(long double jd0, long double jdf);

/** Computes galactic coordinates from equatorial coordinates referred to
	* epoch 1950. RA and Dec are, therefore assumed to be B1950
	* coordinates.
	*/
	void Equatorial1950ToGalactic(void);

/** Computes equatorial coordinates referred to 1950 from galactic ones referred to
	* epoch B1950. RA and Dec are, therefore assumed to be B1950
	* coordinates.
	*/
	void GalacticToEquatorial1950(void);

protected:
/**Precess this SkyPoint's catalog coordinates to the epoch described by the
	*given KSNumbers object.
	*@param num pointer to a KSNumbers object describing the target epoch.
	*/
	void precess(const KSNumbers *num);


private:
	dms RA0, Dec0; //catalog coordinates
	dms RA, Dec; //current true sky coordinates
	dms Alt, Az;
	dms galLat, galLong; // Galactic coordinates
};

#endif
