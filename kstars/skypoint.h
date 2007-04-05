/***************************************************************************
                          skypoint.h  -  K Desktop Planetarium
                             -------------------
    begin                : Sun Feb 11 2001
    copyright            : (C) 2001-2005 by Jason Harris
    email                : jharris@30doradus.org
    copyright            : (C) 2004-2005 by Pablo de Vicente
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

#ifndef SKYPOINT_H
#define SKYPOINT_H

#include <qstring.h>
#include <qptrlist.h>

#include "dms.h"

/**@class SkyPoint
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
	*@short Stores dms coordinates for a point in the sky.
	*for converting between coordinate systems.
	*@author Jason Harris
	*@version 1.0
	*/

class KSNumbers;
class CSegment;
class SkyObject;

class SkyPoint {
public:
/**Default constructor: Sets RA, Dec and RA0, Dec0 according
	*to arguments.  Does not set Altitude or Azimuth.
	*@param r Right Ascension
	*@param d Declination
	*/
	SkyPoint( const dms& r, const dms& d ) { set( r, d ); }

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

////
//// 1.  Setting Coordinates
//// =======================

/**Sets RA, Dec and RA0, Dec0 according to arguments.
	*Does not set Altitude or Azimuth.
	*@param r Right Ascension
	*@param d Declination
	*/
	void set( const dms& r, const dms& d );

/**Overloaded member function, provided for convenience.
	*It behaves essentially like the above function.
	*@param r Right Ascension
	*@param d Declination
	*/
	void set( const dms *r, const dms *d ) { set( *r, *d ); }

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
//	void setGalLong( dms glo ) { galLong.set( glo ); }

/**Overloaded member function, provided for convenience.
	*It behaves essentially like the above function.
	*@param glo Galactic Longitude, expressed as a double.
	*/
//	void setGalLong( double glo ) { galLong.setD( glo ); }

/**Sets Galactic Longitude.
	*@param gla Galactic Longitude.
	*/
//	void setGalLat( dms gla ) { galLat.set( gla ); }

/**Overloaded member function, provided for convenience.
	*It behaves essentially like the above function.
	*@param gla Galactic Longitude, expressed as a double.
	*/
//	void setGalLat( double gla ) { galLat.setD( gla ); }

////
//// 2. Returning coordinates.
//// =========================

/**@return a pointer to the catalog Right Ascension.
	*/
	const dms* ra0() const { return &RA0; }

/**@return a pointer to the catalog Declination.
	*/
	const dms* dec0() const { return &Dec0; }

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
//	const dms* gLat() const { return &galLat; }

/**@return a pointer to the current galactic longitude.
	*/
//	const dms* gLong() const { return &galLong; }

////
//// 3. Coordinate conversions.
//// ==========================

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

	/**@short Convert Right Ascension/Declination to Ecliptic logitude/latitude.
	*/
	void EquatorialToEcliptic( const KSNumbers *num );

	/**@short Convert Ecliptic logitude/latitude to Right Ascension/Declination.
	*/

	void EclipticToEquatorial( const KSNumbers *num );

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

/** Computes galactic coordinates from equatorial coordinates referred to
	* epoch 1950. RA and Dec are, therefore assumed to be B1950
	* coordinates.
	*/
	void Equatorial1950ToGalactic(dms &galLong, dms &galLat);

/** Computes equatorial coordinates referred to 1950 from galactic ones referred to
	* epoch B1950. RA and Dec are, therefore assumed to be B1950
	* coordinates.
	*/
	void GalacticToEquatorial1950(const dms* galLong, const dms* galLat);

////
//// 4. Coordinate update/corrections.
//// =================================

/**Determine the current coordinates (RA, Dec) from the catalog
	*coordinates (RA0, Dec0), accounting for both precession and nutation.
	*@param num pointer to KSNumbers object containing current values of
	*time-dependent variables.
	*@param includePlanets does nothing in this implementation (see KSPlanetBase::updateCoords()).
	*@param lat does nothing in this implementation (see KSPlanetBase::updateCoords()).
	*@param LST does nothing in this implementation (see KSPlanetBase::updateCoords()).
	*/
	virtual void updateCoords( KSNumbers *num, bool includePlanets=true, const dms *lat=0, const dms *LST=0 );

/**Computes the apparent coordinates for this SkyPoint for any epoch,
	*accounting for the effects of precession, nutation, and aberration.
	*Similar to updateCoords(), but the starting epoch need not be
	*J2000, and the target epoch need not be the present time.
	*@param jd0 Julian Day which identifies the original epoch
	*@param jdf Julian Day which identifies the final epoch
	*/
	void apparentCoord(long double jd0, long double jdf);

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

/**General case of precession. It precess from an original epoch to a
	*final epoch. In this case RA0, and Dec0 from SkyPoint object represent
	*the coordinates for the original epoch and not for J2000, as usual.
	*@param jd0 Julian Day which identifies the original epoch
	*@param jdf Julian Day which identifies the final epoch
	*/
	void precessFromAnyEpoch(long double jd0, long double jdf);

	/** Determine the E-terms of aberration 
	 *In the past, the mean places of stars published in catalogs included
	 *the contribution to the aberration due to the ellipticity of the orbit
	 *of the Earth. These terms, known as E-terms were almost constant, and
	 *in the newer catalogs (FK5) are not included. Therefore to convert from
	 *FK4 to FK5 one has to compute these E-terms.
	 */
	SkyPoint Eterms(void);

	/** Exact precession from Besselian epoch 1950 to epoch J2000. The 
	*coordinates referred to the first epoch are in the 
	FK4 catalog, while the latter are in the Fk5 one.
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
	*referred to the first epoch are in the FK4 catalog, while the 
	*latter are in the Fk5 one.
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
	 *catalog do not include these terms. In order to convert from 
	 * FK5 coordinates to B1950 (FK4) one has to use this function. 
	*/
	void subtractEterms(void);

	/** Computes the angular distance between two SkyObjects. The algorithm
	 *  to compute this distance is:
	 *  cos(distance) = sin(d1)*sin(d2) + cos(d1)*cos(d2)*cos(a1-a2)
	 *  where a1,d1 are the coordinates of the first object and a2,d2 are
	 *  the coordinates of the second object.
	 *  However this algorithm is not accurate when the angular separation
	 *  is small.
	 *  Meeus provides a different algorithm in page 111 which we 
	 *  implement here.
	 *  @param sp SkyPoint to which distance is to be calculated
	 *  @return dms angle representing angular separation.
	 **/

	dms angularDistanceTo( SkyPoint *sp);

	bool operator == ( SkyPoint &p ) { return ( ra()->Degrees() == p.ra()->Degrees() && dec()->Degrees() == p.dec()->Degrees() ); }
	
	/** Computes the velocity of the Sun projected on the direction of the source.
	 *
	 * @param jd Epoch expressed as julian day to which the source coordinates refer to.
	 * @return Radial velocity of the source referred to the barycenter of the solar system in km/s
	 **/
	double vRSun(long double jd);

	/** Computes the radial velocity of a source referred to the solar system barycenter 
	 * from the radial velocity referred to the
	 * Local Standard of Rest, aka known as VLSR. To compute it we need the coordinates of the
	 * source the VLSR and the epoch for the source coordinates.
	 *
	 * @param vlsr radial velocity of the source referred to the LSR in km/s
	 * @param jd Epoch expressed as julian day to which the source coordinates refer to.
	 * @return Radial velocity of the source referred to the barycenter of the solar system in km/s
	 **/
	double vHeliocentric(double vlsr, long double jd);

	/** Computes the radial velocity of a source referred to the Local Standard of Rest, also known as VLSR
	 * from the radial velocity referred to the solar system barycenter 
	 *
	 * @param vhelio radial velocity of the source referred to the LSR in km/s
	 * @param jd Epoch expressed as julian day to which the source coordinates refer to.
	 * @return Radial velocity of the source referred to the barycenter of the solar system in km/s
	 **/
	double vHelioToVlsr(double vhelio, long double jd);

	/** Computes the velocity of any object projected on the direction of the source.
	 *  @param jd0 Julian day for which we compute the direction of the source
	 *  @return velocity of the Earth projected on the direction of the source kms-1
	 */
	double vREarth(long double jd0);
	
	/** Computes the radial velocity of a source referred to the center of the earth 
	 * from the radial velocity referred to the solar system barycenter
	 *
	 * @param vhelio radial velocity of the source referred to the barycenter of the 
	 *               solar system in km/s
	 * @param jd     Epoch expressed as julian day to which the source coordinates refer to.
	 * @return Radial velocity of the source referred to the center of the Earth in km/s
	 **/
	double vGeocentric(double vhelio, long double jd);

	/** Computes the radial velocity of a source referred to the solar system barycenter 
	 * from the velocity referred to the center of the earth 
	 *
	 * @param vgeo   radial velocity of the source referred to the center of the Earth
	 *               [km/s]
	 * @param jd     Epoch expressed as julian day to which the source coordinates refer to.
	 * @return Radial velocity of the source referred to the solar system barycenter in km/s
	 **/
	double vGeoToVHelio(double vgeo, long double jd);

	/** Computes the velocity of any object (oberver's site) projected on the 
	 * direction of the source.
	 *  @param vsite velocity of that object in cartesian coordinates
	 *  @return velocity of the object projected on the direction of the source kms-1
	 */
	double vRSite(double vsite[3]);

	/** Computes the radial velocity of a source referred to the observer site on the surface
	 * of the earth from the geocentric velovity and the velocity of the site referred to the center
	 * of the Earth.
	 *
	 * @param vgeo radial velocity of the source referred to the center of the earth in km/s
	 * @param vsite Velocity at which the observer moves referred to the center of the earth.
	 * @return Radial velocity of the source referred to the observer's site in km/s
	 **/
	double vTopocentric(double vgeo, double vsite[3]);

	/** Computes the radial velocity of a source referred to the center of the Earth from 
	 * the radial velocity referred to an observer site on the surface of the earth 
	 * 
	 * @param vtopo radial velocity of the source referred to the observer's site in km/s
	 * @param vsite Velocity at which the observer moves referred to the center of the earth.
	 * @return Radial velocity of the source referred the center of the earth in km/s
	 **/
	double vTopoToVGeo(double vtopo, double vsite[3]);

////
//// 5. Calculating Rise/Set/Transit data.
//// =====================================

//// To Be Moved from SkyObject....

////
//// 6. Constellation Identification
//// =====================================

	QString constellation( QPtrList<CSegment> &seglist, QPtrList<SkyObject> &cnames ) const;


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
};

#endif
