
/***************************************************************************
                          ksplanetbase.h  -  K Desktop Planetarium
                             -------------------
    begin                : Sun Jan 29 2002
    copyright            : (C) 2002 by Mark Hollomon
    email                : mhh@mindspring.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef KSPLANETBASE_H
#define KSPLANETBASE_H

#include <qstring.h>
#include <qptrlist.h>
#include <qimage.h>
#include <qwmatrix.h>

#include <kdebug.h>

#include "skyobject.h"
#include "dms.h"
#include "ksnumbers.h"

#define MAXTRAIL 400  //maximum number of points in a planet trail

/**@class EclipticPosition
	*@short The ecliptic position of a planet (Longitude, Latitude, and distance from Sun).
	*@author Mark Hollomon
	*@version 1.0
	*/
class EclipticPosition {
	public:
		dms longitude;
		dms latitude;
		double radius;

		/**Constructor. */
		EclipticPosition(dms plong = 0.0, dms plat = 0.0, double prad = 0.0) :
			longitude(plong), latitude(plat), radius(prad) {};

		/**Assignment operator. Copy all values from the target object. */
		EclipticPosition &operator=(EclipticPosition &r) {
			this->longitude = r.longitude;
			this->latitude = r.latitude;
			this->radius = r.radius;
			return *this;
		};
};

/**@class KSPlanetBase
	*@short Provides necessary information about objects in the solar system.
	*@author Mark Hollomon
	*@version 1.0
	*A subclass of SkyObject that provides additional information
	*needed for solar system objects. This is a base class for KSPlanet,
	* KSPluto, KSSun and KSMoon.
	*/

class KStarsData;
class KSPlanetBase : public SkyObject {
public: 

/**Constructor.  Calls SkyObject constructor with type=2 (planet),
	*coordinates=0.0, mag=0.0, primary name s, and all other QStrings empty.
	*@param s Name of planet
	*@param im the planet's image
	*/
	KSPlanetBase( KStarsData *kd, QString s = i18n("unnamed"), QString image_file="" );

/**
	*Destructor (empty)
	*/
	virtual ~KSPlanetBase() {}

/**@return pointer to Ecliptic Longitude coordinate
	*/
	const dms* ecLong( void ) const { return &ep.longitude; };

/**
	*@return pointer to Ecliptic Latitude coordinate
	*/
	const dms* ecLat( void ) const { return &ep.latitude; };

/**@short Set Ecliptic Longitude according to argument.
	*@param elong Ecliptic Longitude
	*/
	void setEcLong( dms elong ) { ep.longitude = elong; }

/**@short Set Ecliptic Longitude according to argument.
	*Differs from above function only in argument type.
	*@param elong Ecliptic Longitude
	*/
	void setEcLong( double elong ) { ep.longitude.setD( elong ); }

/**@short Set Ecliptic Latitude according to argument.
	*@param elat Ecliptic Latitude
	*/
	void setEcLat( dms elat ) { ep.latitude = elat; }

/**@short Set Ecliptic Latitude according to argument.
	*Differs from above function only in argument type.
	*@param elat Ecliptic Latitude
	*/
	void setEcLat( double elat ) { ep.latitude.setD( elat ); }

/**@short Load the planet's orbital data from disk.
	*@return true if data succesfully loaded
	*/
	virtual bool loadData() = 0;

/**@short find the object's current equatorial coordinates (RA and Dec)
	*Given the current KSNumbers object (time-dependent), calculate and set 
	*the RA, Dec coordinates of the Planet.
	*@param num pointer to current KSNumbers object
	*@param Earth pointer to planet Earth (needed to calculate geocentric coords)
	*@return true if position was successfully calculated.
	*/
	virtual bool findPosition( const KSNumbers *num, const KSPlanetBase *Earth=NULL ) = 0;

/**@short Convert Ecliptic logitude/latitude to Right Ascension/Declination.
	*@param Obliquity current Obliquity of the Ecliptic (angle from Equator)
	*/
	void EclipticToEquatorial( const dms *Obliquity );

/**@short Convert Right Ascension/Declination to Ecliptic logitude/latitude.
	*@param Obliquity current Obliquity of the Ecliptic (angle from Equator)
	*/
	void EquatorialToEcliptic( const dms *Obliquity );

/**@return pointer to image of planet
	*/
	QImage* image( void ) { return &Image; };

/**@return pointer to unrotated image of planet
	*/
	QImage* image0( void ) { return &Image0; };

/**@return distance from Sun, in Astronomical Units (1 AU is Earth-Sun distance)
	*/
	double rsun( void ) const { return ep.radius; };

/**@short Set the solar distance in AU.
	*@param r the new solar distance in AU
	*/
	void setRsun( double r ) { ep.radius = r; };

/**Update position of the planet (reimplemented from SkyPoint)
	*@param num current KSNumbers object
	*@param includePlanets this function does nothing if includePlanets=false
	*/
	virtual void updateCoords( KSNumbers *num, bool includePlanets=true );

/**@return the Planet's position angle.
	*/
		double pa() const { return PositionAngle; }

/**@short Set the Planet's position angle.
	*@param p the new position angle
	*/
		void setPA( double p ) { PositionAngle = p; }

/**@return the Planet's angular size, in arcminutes
	*/
		double angSize() const { return AngularSize; }
		
/**@short Set the Planet's angular size in arcminutes
	*@param a the new angular size
	*/
		void setAngSize( double a ) { AngularSize = a; }

/**@return whether the planet has a trail
	*/
		bool hasTrail() const { return ( Trail.count() > 0 ); }
		 
/**@return a reference to the planet's trail
	*/
		QPtrList<SkyPoint>* trail() { return &Trail; } 
		
/**@short adds a point to the planet's trail
	*@param sp a pointer to the SkyPoint to add (will be AutoDeleted)
	*/
		void addToTrail() { Trail.append( new SkyPoint( ra(), dec() ) ); }
		
/**@short removes the oldest point from the trail
	*/
		void clipTrail() { Trail.removeFirst(); }
		
/**@short clear the Trail
	*/
		void clearTrail() { Trail.clear(); }
		
/**@short update Horizontal coords of the trail
	*/
		void updateTrail( dms *LST, const dms *lat );

/**@short rotate Planet image
	*@param imageAngle the new angle of rotation for the image
	*/
	void rotateImage( double imageAngle );

protected:
	virtual bool loadData(QString n) { 
		kdDebug() << "didn't reimplement for " << n << endl; return false;
	};

/**Determine the position angle of the planet for a given date 
	*(used internally by findPosition() )
	*/
	void findPA( const KSNumbers *num );
	
	EclipticPosition ep;
	QPtrList<SkyPoint> Trail;

private:
	QImage Image0, Image;
	double PositionAngle, ImageAngle, AngularSize;
	KStarsData *data;
};

#endif
