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
#include "dms.h"


/**
	*Encapsulates the sky coordinates of a point in the sky.  The
	*coordinates are stored in both Equatorial (Right Ascension,
	*Declination) and Horizon (Azimuth, Altitude) coordinate systems.
	*Provides set/get functions for each coordinate angle, and functions
	* to convert between the Equatorial and Horizon coordinate systems.
	*
	*Because the coordinate system changes slowly over time (see
	*precession, nutation), the "catalog coordinates" are stored
	*(RA0, Dec0), which were the true coordinates on Jan 1, 2000.
	*The true coordinates (RA, Dec) at any other epoch can be found
	*from the catalog coordinates using precess() and nutate().
	*
	*@short Similar to QPoint, but holds sky coordinates (RA, Dec)
  *@author Jason Harris
  *@version 0.4
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
	~SkyPoint();
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
	dms getRA0() { return RA0; }
/**
	*Returns the catalog Declination.
	*@returns Dec0, the catalog Declination.	
	*/
  dms getDec0() { return Dec0; }
/**
	*Returns the current Right Ascension.
	*@returns RA, the current Right Ascension.	
	*/
	dms getRA() { return RA; }
/**
	*Returns the current Declination.
	*@returns Dec, the current Declination.	
	*/
  dms getDec() { return Dec; }
/**
	*Returns the current Azimuth.
	*@returns Az, the current Azimuth.	
	*/
	dms getAz() { return Az; }
/**
	*Returns the current Altitude.
	*@returns Alt, the current Altitude.	
	*/
	dms getAlt() { return Alt; }
/**
	*Determines the (Altitude, Azimuth) coordinates of the
	*SkyPoint from its (RA, Dec) coordinates, given the local
	*sidereal time and the observer's latitude.
	*@param LST the local sidereal time
	*@param lat the observer's latitude
	*/
	void RADecToAltAz( dms LST, dms lat );
/**
	*Sets the (RA, Dec) coordinates of the
	*SkyPoint, given Ecliptic (Long, Lat) coordinates, and
	*the Julian Date.
	*/
	void setEcliptic( double ELong, double ELat, long double jd );
/**
	*Determines the (RA, Dec) coordinates of the
	*SkyPoint from its (Altitude, Azimuth) coordinates, given the local
	*sidereal time and the observer's latitude.
	*@param LST the local sidereal time
	*@param lat the observer's latitude
	*/
	void AltAzToRADec( dms LSTh, dms lat );
/**
	*Determine the current coordinates (RA, Dec) from the catalog
	*coordinates (RA0, Dec0), accounting for both precession and nutation.
	*@param NewEpoch Julian date to use for calculating current coordinates.
	*@param Obliquity The current Obliquity of the Ecliptic (see kstars::findObliquity())
	*@param dObliq The change in Obliquity caused by nutation.
	*@param dEcLong The change in Ecliptic longitude caused by nutation.
	*/
	void updateCoords( long double NewEpoch, dms Obliquity, double dObliq, double dEcLong );
/**
	*Precess the catalog coordinates to the current Julian Date (Epoch).
	*@param NewEpoch Julian date to use as current Epoch.
	*@param Epoch0 Julian date of catalog coordinates.  The default value
	*corresponds to Jan 1, 2000 (J2000), and should almost always be used.
	*/
	void precess( long double NewEpoch, long double Epoch0=2451545.0 );

private:
  dms RA0, Dec0; //catalog coordinates
  dms RA, Dec;  //current sky coordinates
	dms Alt, Az;
};

#endif
