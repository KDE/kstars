
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

#include <kdebug.h>

#include "skyobject.h"
#include "dms.h"

#define MAXTRAIL 400  //maximum number of points in a planet trail

class QPoint;
class KSNumbers;
class KSPopupMenu;

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
			longitude(plong), latitude(plat), radius(prad) {}

		/**Assignment operator. Copy all values from the target object. */
		EclipticPosition &operator=(EclipticPosition &r) {
			this->longitude = r.longitude;
			this->latitude = r.latitude;
			this->radius = r.radius;
			return *this;
		}
};

/**@class KSPlanetBase
	*A subclass of SkyObject that provides additional information
	*needed for solar system objects. This is a base class for KSPlanet,
	* KSPluto, KSSun and KSMoon.
	*@short Provides necessary information about objects in the solar system.
	*@author Mark Hollomon
	*@version 1.0
	*/

class KStarsData;
class KSPlanetBase : public SkyObject {
public:

/**Constructor.  Calls SkyObject constructor with type=2 (planet),
	*coordinates=0.0, mag=0.0, primary name s, and all other QStrings empty.
	@param kd Some kind of data
	*@param s Name of planet
	*@param image_file filename of the planet's image
	*@param pSize the planet's physical size, in km
	*
	*@todo Figure out what @p kd does.
	*/
	KSPlanetBase( KStarsData *kd, QString s = i18n("unnamed"), QString image_file="", double pSize=0 );

/**
	*Destructor (empty)
	*/
	virtual ~KSPlanetBase() {}

/**@return pointer to Ecliptic Longitude coordinate
	*/
	const dms* ecLong( void ) const { return &ep.longitude; }

/**
	*@return pointer to Ecliptic Latitude coordinate
	*/
	const dms* ecLat( void ) const { return &ep.latitude; }

/**@short Set Ecliptic Geocentric Longitude according to argument.
	*@param elong Ecliptic Longitude
	*/
	void setEcLong( dms elong ) { ep.longitude = elong; }

/**@short Set Ecliptic Geocentric Longitude according to argument.
	*Differs from above function only in argument type.
	*@param elong Ecliptic Longitude
	*/
	void setEcLong( double elong ) { ep.longitude.setD( elong ); }

/**@short Set Ecliptic Geocentric Latitude according to argument.
	*@param elat Ecliptic Latitude
	*/
	void setEcLat( dms elat ) { ep.latitude = elat; }

/**@short Set Ecliptic Geocentric Latitude according to argument.
	*Differs from above function only in argument type.
	*@param elat Ecliptic Latitude
	*/
	void setEcLat( double elat ) { ep.latitude.setD( elat ); }
	
	/**@return pointer to Ecliptic Heliocentric Longitude coordinate
	*/
	const dms* helEcLong( void ) const { return &helEcPos.longitude; }

/**
	*@return pointer to Ecliptic Heliocentric Latitude coordinate
	*/
	const dms* helEcLat( void ) const { return &helEcPos.latitude; }

/**@short Set Ecliptic Heliocentric Longitude according to argument.
	*@param elong Ecliptic Longitude
	*/
	void setHelEcLong( dms elong ) { helEcPos.longitude = elong; }

/**@short Set Ecliptic Heliocentric Longitude according to argument.
	*Differs from above function only in argument type.
	*@param elong Ecliptic Longitude
	*/
	void setHelEcLong( double elong ) { helEcPos.longitude.setD( elong ); }

/**@short Set Ecliptic Heliocentric Latitude according to argument.
	*@param elat Ecliptic Latitude
	*/
	void setHelEcLat( dms elat ) { helEcPos.latitude = elat; }

/**@short Set Ecliptic Heliocentric Latitude according to argument.
	*Differs from above function only in argument type.
	*@param elat Ecliptic Latitude
	*/
	void setHelEcLat( double elat ) { helEcPos.latitude.setD( elat ); }

/**@short Load the planet's orbital data from disk.
	*@return true if data successfully loaded
	*/
	virtual bool loadData() = 0;

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
	QImage* image( void ) { return &Image; }

/**@return pointer to unrotated image of planet
	*/
	QImage* image0( void ) { return &Image0; }

/**@return distance from Sun, in Astronomical Units (1 AU is Earth-Sun distance)
	*/
	double rsun( void ) const { return ep.radius; }

/**@short Set the solar distance in AU.
	*@param r the new solar distance in AU
	*/
	void setRsun( double r ) { ep.radius = r; }

/**@return distance from Earth, in Astronomical Units (1 AU is Earth-Sun distance)
	*/
	double rearth() const { return Rearth; }

/**@short Set the distance from Earth, in AU.
	*@param r the new earth-distance in AU
	*/
	void setRearth( double r ) { Rearth = r; }

/**@short compute and set the distance from Earth, in AU.
	*@param Earth pointer to the Earth from which to calculate the distance.
	*/
	void setRearth( const KSPlanetBase *Earth );

/**Update position of the planet (reimplemented from SkyPoint)
	*@param num current KSNumbers object
	*@param includePlanets this function does nothing if includePlanets=false
	*@param lat pointer to the geographic latitude; if NULL< we skip localizeCoords()
	*@param LST pointer to the local sidereal time; if NULL< we skip localizeCoords()
	*/
	virtual void updateCoords( KSNumbers *num, bool includePlanets=true, const dms *lat=0, const dms *LST=0 );

/**
	*@short Find position, including correction for Figure-of-the-Earth.
	*@param num KSNumbers pointer for the target date/time
	*@param lat pointer to the geographic latitude; if NULL, we skip localizeCoords()
	*@param LST pointer to the local sidereal time; if NULL, we skip localizeCoords()
	*@param Earth pointer to the Earth (not used for the Moon)
	*/
	void findPosition( const KSNumbers *num, const dms *lat=0, const dms *LST=0, const KSPlanetBase *Earth = 0 );

/**@return the Planet's position angle.
	*/
		virtual double pa() const { return PositionAngle; }

/**@short Set the Planet's position angle.
	*@param p the new position angle
	*/
		void setPA( double p ) { PositionAngle = p; }

/**@return the Planet's angular size, in arcminutes
	*/
		double angSize() const { return AngularSize; }

/**@short set the planet's angular size, in km.
	*@p size the planet's size, in km
	*/
	void setAngularSize( double size ) { AngularSize = size; }

/**@return the Planet's physical size, in km
	*/
		double physicalSize() const { return PhysicalSize; }

/**@short set the planet's physical size, in km.
	*@p size the planet's size, in km
	*/
	void setPhysicalSize( double size ) { PhysicalSize = size; }

/**@return true if the KSPlanet is one of the eight major planets
 */
	bool isMajorPlanet() const;

/**@return whether the planet has a trail
 */
	bool hasTrail() const { return ( Trail.count() > 0 ); }

/**@return a reference to the planet's trail
	*/
		QPtrList<SkyPoint>* trail() { return &Trail; }

/**@short adds a point to the planet's trail
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

/**@short scale and rotate Planet image
	*@param scale the scaling factor
	*@param imageAngle the new angle of rotation for the image
	*/
	void scaleRotateImage( int scale, double imageAngle );

/**Show Solar System object popup menu.  Overloaded from virtual 
	*SkyObject::showPopupMenu()
	*@param pmenu pointer to the KSPopupMenu object
	*@param pos QPojnt holding the x,y coordinates for the menu
	*/
	virtual void showPopupMenu( KSPopupMenu *pmenu, QPoint pos ) { pmenu->createPlanetMenu( this ); pmenu->popup( pos ); }

protected:
	virtual bool loadData(QString n) {
		kdDebug() << "didn't reimplement for " << n << endl; return false;
	}

/**@short find the object's current geocentric equatorial coordinates (RA and Dec)
	*This function is pure virtual; it must be overloaded by subclasses.
	*This function is private; it is called by the public function findPosition()
	*which also includes the figure-of-the-earth correction, localizeCoords().
	*@param num pointer to current KSNumbers object
	*@param Earth pointer to planet Earth (needed to calculate geocentric coords)
	*@return true if position was successfully calculated.
	*/
	virtual bool findGeocentricPosition( const KSNumbers *num, const KSPlanetBase *Earth=NULL ) = 0;

/**Determine the position angle of the planet for a given date
	*(used internally by findPosition() )
	*/
	void findPA( const KSNumbers *num );

	// Geocentric ecliptic position, but distance to the Sun
	EclipticPosition ep;

	// Heliocentric ecliptic position referred to the equinox of the epoch 
	// as obtained from VSOP.
	EclipticPosition helEcPos;
	QPtrList<SkyPoint> Trail;
	double  Rearth;

private:
/**@short correct the position for the fact that the location is not at the center of the Earth,
	*but a position on its surface.  This causes a small parallactic shift in a solar system
	*body's apparent position.  The effect is most significant for the Moon.
	*This function is private, and should only be called from the public findPosition() function.
	*@param num pointer to a ksnumbers object for the target date/time
	*@param lat pointer to the geographic latitude of the location.
	*@param LST pointer to the local sidereal time.
	*/
	void localizeCoords( const KSNumbers *num, const dms *lat, const dms *LST );

	/* Computes the visual magnitude for the major planets. 
	 * @param num pointer to a ksnumbers object. Needed for the saturn rings contribution to 
	 *        saturn magnitude.
	 * @param Earth pointer to an Earth object. Needed to know the distance between the Earth and the
	 *        Sun.
	 */
	void findMagnitude(const KSNumbers *num);

	QImage Image0, Image;
	double PositionAngle, ImageAngle, AngularSize, PhysicalSize;
	KStarsData *data;
};

#endif
