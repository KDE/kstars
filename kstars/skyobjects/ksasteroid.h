/***************************************************************************
                          ksasteroid.h  -  K Desktop Planetarium
                             -------------------
    begin                : Wed 19 Feb 2003
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

#ifndef KSASTEROID_H_
#define KSASTEROID_H_

#include "ksplanetbase.h"

class KSNumbers;
class dms;

/** @class KSAsteroid
	*@short A subclass of KSPlanetBase that implements asteroids.
	*
	*The orbital elements are stored as private member variables, and it
	*provides methods to compute the ecliptic coordinates for any time
	*from the orbital elements.
	*
	*The orbital elements are:
	*@li JD    Epoch of element values
	*@li a     semi-major axis length (AU)
	*@li e     eccentricity of orbit
	*@li i     inclination angle (with respect to J2000.0 ecliptic plane)
	*@li w     argument of perihelion (w.r.t. J2000.0 ecliptic plane)
	*@li N     longitude of ascending node (J2000.0 ecliptic)
	*@li M     mean anomaly at epoch JD
	*@li H     absolute magnitude
	*@li G     slope parameter
	*
	*@author Jason Harris
	*@version 1.0
	*/
class KSAsteroid : public KSPlanetBase
{
public:

    /** Constructor.
        *@p catN number of asteroid
    	*@p s    the name of the asteroid
    	*@p image_file the filename for an image of the asteroid
    	*@p JD the Julian Day for the orbital elements
    	*@p a the semi-major axis of the asteroid's orbit (AU)
    	*@p e the eccentricity of the asteroid's orbit
    	*@p i the inclination angle of the asteroid's orbit
    	*@p w the argument of the orbit's perihelion
    	*@p N the longitude of the orbit's ascending node
    	*@p M the mean anomaly for the Julian Day
    	*@p H absolute magnitude
        *@p G slope parameter
    	*/
    KSAsteroid( int catN, const QString &s, const QString &image_file,
                long double JD, double a, double e, dms i, dms w, dms N, dms M, double H, double G );

    virtual KSAsteroid* clone() const;
    virtual SkyObject::UID getUID() const;

    /** Destructor (empty)*/
    virtual ~KSAsteroid() {}

    /** This is inherited from KSPlanetBase.  We don't use it in this class,
    	*so it is empty.
    	*/
    virtual bool loadData();

    /** This lets other classes like KSPlanetBase access H and G values
	*Used by KSPlanetBase::FindMagnitude
	*/
    double inline getAbsoluteMagnitude() const { return H; }
    double inline getSlopeParameter() const { return G; }

    /**
     *@short Sets the asteroid's perihelion distance
     */
    void setPerihelion( double perihelion );

    /**
     *@return Perihelion distance
     */
    inline double getPerihelion() const { return q; }

   /**
     *@short Sets the asteroid's earth minimum orbit intersection distance
     */
    void setEarthMOID( double earth_moid );

    /**
     *@return the asteroid's earth minimum orbit intersection distance in AU
     */
    inline double getEarthMOID() const { return EarthMOID; }

    /**
     *@short Sets the asteroid's orbit solution ID
     */
    void setOrbitID( QString orbit_id );

    /**
     *@return the asteroid's orbit solution ID
     */
    inline QString getOrbitID() const { return OrbitID; }

    /**
     *@short Sets the asteroid's orbit class
     */
    void setOrbitClass( QString orbit_class );

    /**
     *@return the asteroid's orbit class
     */
    inline QString getOrbitClass() const { return OrbitClass; }

    /**
     *@short Sets if the comet is a near earth object
     */
    void setNEO( bool neo );

    /**
     *@return true if the asteroid is a near earth object
     */
    inline bool isNEO() const { return NEO; }

    /**
     *@short Sets the asteroid's albedo
     */
    void setAlbedo( float albedo );

    /**
     *@return the asteroid's albedo
     */
    inline float getAlbedo() const { return Albedo; }

    /**
     *@short Sets the asteroid's diameter
     */
    void setDiameter( float diam );

    /**
     *@return the asteroid's diameter
     */
    inline float getDiameter() const { return Diameter; }

    /**
     *@short Sets the asteroid's dimensions
     */
    void setDimensions( QString dim );

    /**
     *@return the asteroid's dimensions
     */
    inline QString getDimensions() const { return Dimensions; }

     /**
     *@short Sets the asteroid's rotation period
     */
    void setRotationPeriod( float rot_per );

    /**
     *@return the asteroid's rotation period
     */
    inline float getRotationPeriod() const { return RotationPeriod; }

     /**
     *@short Sets the asteroid's period
     */
    void setPeriod( float per );

    /**
     *@return the asteroid's period
     */
    inline float getPeriod() const { return Period; }

protected:
    /** Calculate the geocentric RA, Dec coordinates of the Asteroid.
    	*@note reimplemented from KSPlanetBase
    	*@param num time-dependent values for the desired date
    	*@param Earth planet Earth (needed to calculate geocentric coords)
    	*@return true if position was successfully calculated.
    	*/
    virtual bool findGeocentricPosition( const KSNumbers *num, const KSPlanetBase *Earth=NULL );

    //these set functions are needed for the new KSPluto subclass
    void set_a( double newa ) { a = newa; }
    void set_e( double newe ) { e = newe; }
    void set_P( double newP ) { P = newP; }
    void set_i( double newi ) { i.setD( newi ); }
    void set_w( double neww ) { w.setD( neww ); }
    void set_M( double newM ) { M.setD( newM ); }
    void set_N( double newN ) { N.setD( newN ); }
    void setJD( long double jd ) { JD = jd; }

private:
    virtual void findMagnitude(const KSNumbers*);
    
    int catN;
    long double JD;
    double q, a, e, P, EarthMOID;
    float Albedo, Diameter, RotationPeriod, Period;
    dms i, w, M, N;
    double H, G;
    QString OrbitID, OrbitClass, Dimensions;
    bool NEO;
};

#endif
