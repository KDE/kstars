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

#define B1950 2433282.4235 


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
  void set( dms r, dms d ) { RA0.set( r ); Dec0.set( d ); RA.set( r ); Dec.set( d ); }

/**
	*Sets RA, Dec and RA0, Dec0 according to arguments.  Does not set
	*Altitude or Azimuth.  Differs from above function only in argument type.
	*@param r Right Ascension, expressed as a double	
	*@param d Declination, expressed as a double
	*/
  void set( double r, double d ) { RA0.setH( r ); Dec0.setD( d ); RA.setH( r ); Dec.setD( d ); }

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
	dms ra0() const { return RA0; };

/**
	*Returns the catalog Declination.
	*@returns Dec0, the catalog Declination.	
	*/
  dms dec0() const { return Dec0; };

/**
	*Returns the apparent Right Ascension.
	*@returns RA, the current Right Ascension.	
	*/
	dms ra() const { return RA; }

/**
	*Returns the apparent Declination.
	*@returns Dec, the current Declination.	
	*/
  dms dec() const { return Dec; }

/**
	*Returns the apparent Azimuth.
	*@returns Az, the current Azimuth.	
	*/
	dms az() const { return Az; }

/**
	*Returns the apparent Altitude.
	*@returns Alt, the current Altitude.	
	*/
	dms alt() const { return Alt; }

/**
	*Determines the (Altitude, Azimuth) coordinates of the
	*SkyPoint from its (RA, Dec) coordinates, given the local
	*sidereal time and the observer's latitude.
	*@param LST the local sidereal time
	*@param lat the observer's latitude
	*/
	void EquatorialToHorizontal( dms LST, dms lat );

/**
	*Determine the Ecliptic coordinates of the SkyPoint, given the Julian Date.
	*The ecliptic coordinates are returned as reference arguments
	*/
	void findEcliptic( dms Obliquity, dms &EcLong, dms &EcLat );

/**
	*Sets the (RA, Dec) coordinates of the
	*SkyPoint, given Ecliptic (Long, Lat) coordinates, and
	*the Julian Date.
	*/
	void setFromEcliptic( dms Obliquity, dms EcLong, dms EcLat );

/**
	*Determines the (RA, Dec) coordinates of the
	*SkyPoint from its (Altitude, Azimuth) coordinates, given the local
	*sidereal time and the observer's latitude.
	*@param LST the local sidereal time
	*@param lat the observer's latitude
	*/
	void HorizontalToEquatorial( dms LSTh, dms lat );

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

	/** Determine the E-terms of aberration 
	 *In the past, the mean places of stars published in catalogs included
	 *the contribution to the aberration due to the ellipticity of the orbit
	 *of the Earth. These terms, known as E-terms were almost constant, and
	 *in the newer catalogs (FK5) are not included. Therefore to convert from
	 *FK4 to FK5 one has to compute these E-terms.
	 */
	SkyPoint Eterms(void);

	/** Exact precession from Besselian epoch 1950 to epoch J2000. The coordinates
	*referred to the first epoch are in the FK4 catalog, while the latter are in the
	*Fk5 one.
	*Reference: Smith, C. A.; Kaplan, G. H.; Hughes, J. A.; Seidelmann,
	*P. K.; Yallop, B. D.; Hohenkerk, C. Y.
	*Astronomical Journal, vol. 97, Jan. 1989, p. 265-279
	*This transformation requires 4 steps:
	* - Correct E-terms
	* - Precess from B1950 to 1984, January 1st, 0h, using Newcomb expressions
	* - Add zero point correction in right ascension for 1984
	* - Precess from 1984, January 1st, 0h to J2000
	*/
	void B1950ToJ2000(void);

	/** Exact precession from epoch J2000 Besselian epoch 1950. The coordinates
	*referred to the first epoch are in the FK4 catalog, while the latter are in the
	*Fk5 one.
	*Reference: Smith, C. A.; Kaplan, G. H.; Hughes, J. A.; Seidelmann,
	*P. K.; Yallop, B. D.; Hohenkerk, C. Y.
	*Astronomical Journal, vol. 97, Jan. 1989, p. 265-279
	*This transformation requires 4 steps:
	* - Precess from J2000 to 1984, January 1st, 0h
	* - Add zero point correction in right ascension for 1984
	* - Precess from 1984, January 1st, 0h, to B1950 using Newcomb expressions
	* - Correct E-terms
	*/
	void J2000ToB1950(void);

	/** Coordinates in the FK4 catalog include the effect of aberration due
	 *to the ellipticity of the orbit of the Earth. Coordinates in the FK5
	 *catalog do not include these terms. In order to convert from B1950 (FK4)
	 *to actual mean places one has to use this function.
	*/
	void addEterms(void);

	/** Coordinates in the FK4 catalog include the effect of aberration due
	 *to the ellipticity of the orbit of the Earth. Coordinates in the FK5 
	 *catalog do not include these terms. In order to convert from FK5 coordinates
	 * to B1950 (FK4) one has to use this function. 
	*/
	void subtractEterms(void);

protected:
/**Precess this SkyPoint's catalog coordinates */
	void precess(const KSNumbers *num);

private:
	dms RA0, Dec0; //catalog coordinates
	dms RA, Dec; //current true sky coordinates
	dms Alt, Az;
};

#endif
