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


/**
	*Encapsulates the sky coordinates of a point in the sky.  The
	*coordinates are stored in both Equatorial (Right Ascension,
	*Declination) and Horizontal (Azimuth, Altitude) coordinate systems.
	*Provides set/get functions for each coordinate angle, and functions
	* to convert between the Equatorial and Horizon coordinate systems.
	*
	*Because the coordinate system changes slowly over time (see
	*precession, nutation), the "catalog coordinates" are stored
	*(RA0, Dec0), which were the true coordinates on Jan 1, 2000.
	*The true coordinates (RA, Dec) at any other epoch can be found
	*from the catalog coordinates using precess() and nutate().
	*
	*@short Stores dms coordinates for a point in the sky, and functions for
	*converting between coordinate systems.
  *@author Jason Harris
  *@version 0.9
  */

class SkyPoint {
public: 
/**
	*Default constructor: Sets RA, Dec and RA0, Dec0 according
	*to arguments.  Does not set Altitude or Azimuth.
	*@param r Right Ascension
	*@param d Declination
	*/	

	SkyPoint( dms r, dms d ) { set( r, d ); }
	SkyPoint( const dms *r, const dms *d ) { set( dms(*r), dms(*d) ); }

/**
	*Sets RA, Dec and RA0, Dec0 according to arguments.
	*Does not set Altitude or Azimuth.  Differs from default constructor
	*only in argument types.
	*@param r Right Ascension, expressed as a double	
	*@param d Declination, expressed as a double
	*/	
	SkyPoint( double r=0.0, double d=0.0 ) { set( r, d ); }

/**
	*Empty destructor.
	*/	
	virtual ~SkyPoint();

/**
	*Sets RA, Dec and RA0, Dec0 according to arguments.
	*Does not set Altitude or Azimuth.
	*@param r Right Ascension	
	*@param d Declination
	*/
	//void set( dms r, dms d ) { RA0.set( r ); Dec0.set( d ); RA.set( r ); Dec.set( d ); }
	void set( dms r, dms d );
	void set( const dms *r, const dms *d ) { set( dms(*r), dms(*d) ); }

/**
	*Sets RA, Dec and RA0, Dec0 according to arguments.  Does not set
	*Altitude or Azimuth.  Differs from above function only in argument type.
	*@param r Right Ascension, expressed as a double	
	*@param d Declination, expressed as a double
	*/
	//XYZ_COORD
	//void set( double r, double d ) { RA0.setH( r ); Dec0.setD( d ); RA.setH( r ); Dec.setD( d ); }
	void set( double r, double d );

/**
	*Sets RA0, the catalog Right Ascension.
	*@param r Right Ascension.
	*/
	void setRA0( dms r ) { RA0.set( r ); }

/**
	*Sets RA0, the catalog Right Ascension.  Differs from above function
	*only in argument type.
	*@param r Right Ascension, expressed as a double.
	*/
	void setRA0( double r ) { RA0.setH( r ); }

/**
	*Sets Dec0, the catalog Declination.
	*@param d Declination.
	*/
	void setDec0( dms d ) { Dec0.set( d ); }

/**
	*Sets Dec0, the catalog Declination.  Differs from above function only
	*in argument type.
	*@param d Declination, expressed as a double.
	*/
	void setDec0( double d ) { Dec0.setD( d ); }

/**
	*Sets RA, the current Right Ascension.
	*@param r Right Ascension.
	*/
	void setRA( dms r ) { RA.set( r ); }

/**
	*Sets RA, the current Right Ascension.  Differs from above function only
	*in argument type
	*@param r Right Ascension, expressed as a double.
	*/
	void setRA( double r ) { RA.setH( r ); }

/**
	*Sets Dec, the current Declination
	*@param d Declination.
	*/
	void setDec( dms d ) { Dec.set( d ); }

/**
	*Sets Dec, the current Declination.  Differs from above function only
	*in argument type
	*@param d Declination, expressed as a double.
	*/
	void setDec( double d ) { Dec.setD( d ); }

/**
	*Sets Alt, the Altitude.
	*@param alt Altitude.
	*/
	void setAlt( dms alt ) { Alt.set( alt ); }

/**
	*Sets Alt, the Altitude.  Differs from above function only in argument type.
	*@param alt Altitude, expressed as a double.
	*/
	void setAlt( double alt ) { Alt.setD( alt ); }

/**
	*Sets Az, the Azimuth.
	*@param az Azimuth.
	*/
	void setAz( dms az ) { Az.set( az ); }

/**
	*Sets Az, the Azimuth.  Differs from above function only in argument type.
	*@param az Azimuth, expressed as a double.
	*/
	void setAz( double az ) { Az.setD( az ); }

/**
	*Returns the catalog Right Ascension.
	*@returns RA0, the catalog Right Ascension.	
	*/
	const dms* ra0() const { return &RA0; };

/**
	*Returns the catalog Declination.
	*@returns Dec0, the catalog Declination.	
	*/
	const dms* dec0() const { return &Dec0; };

/**
	*Returns the apparent Right Ascension.
	*@returns RA, the current Right Ascension.	
	*/
	const dms* ra() const { return &RA; }

/**
	*Returns the apparent Declination.
	*@returns Dec, the current Declination.	
	*/
	const dms* dec() const { return &Dec; }

/**
	*Returns the apparent Azimuth.
	*@returns Az, the current Azimuth.	
	*/
	const dms* az() const { return &Az; }

/**
	*Returns the apparent Altitude.
	*@returns Alt, the current Altitude.	
	*/
	const dms* alt() const { return &Alt; }

/**
	*Determines the (Altitude, Azimuth) coordinates of the
	*SkyPoint from its (RA, Dec) coordinates, given the local
	*sidereal time and the observer's latitude.
	*@param LST the local sidereal time
	*@param lat the observer's latitude
	*/
	void EquatorialToHorizontal( const dms* LST, const dms* lat );

/**
	*Determine the Ecliptic coordinates of the SkyPoint, given the Julian Date.
	*The ecliptic coordinates are returned as reference arguments
	*/
	void findEcliptic( const dms *Obliquity, dms &EcLong, dms &EcLat );

/**
	*Sets the (RA, Dec) coordinates of the
	*SkyPoint, given Ecliptic (Long, Lat) coordinates, and
	*the Julian Date.
	*/
	void setFromEcliptic( const dms *Obliquity, const dms *EcLong, const dms *EcLat );

/**
	*Determines the (RA, Dec) coordinates of the
	*SkyPoint from its (Altitude, Azimuth) coordinates, given the local
	*sidereal time and the observer's latitude.
	*@param LST the local sidereal time
	*@param lat the observer's latitude
	*/
	void HorizontalToEquatorial( const dms* LSTh, const dms* lat );

/**
	*Determine the current coordinates (RA, Dec) from the catalog
	*coordinates (RA0, Dec0), accounting for both precession and nutation.
	*@param num KSNumbers object containing current values of time-dependent variables
	*@param forcePlanet does nothing in this implementation.  See KSPlanetBase::updateCoords().
	*/
	virtual void updateCoords( KSNumbers *num, bool includePlanets=true );

/**Determine the effects of nutation for this SkyPoint*/
	void nutate(const KSNumbers *num);

/**Determine the effects of aberration for this SkyPoint*/
	void aberrate(const KSNumbers *num);
	
/**Computes the apparent coordinates for this SkyPoint. The difference 
	*with function updateCoords is that original coordinates might 
	* not be J2000, and final coordinates might not be the present time.
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

protected:
/**Precess this SkyPoint's catalog coordinates */
	void precess(const KSNumbers *num);
	

private:
	dms RA0, Dec0; //catalog coordinates
	dms RA, Dec; //current true sky coordinates
	dms Alt, Az;
};

#endif
