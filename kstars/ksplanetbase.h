
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
#include <qimage.h>
#include <qwmatrix.h>

#include <kdebug.h>

#include "skyobject.h"
#include "dms.h"
#include "ksnumbers.h"

/**Class that encapsulates the ecliptic position of a planet.
	*This includes not only the Ecliptic longitude and latitude, but also the
	*distance from the Sun.
	*@author Mark Hollomon
	*@version 0.9
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

/**
	*A subclass of SkyObject that provides additional information
	*needed for solar system objects. This is a base class for KSPlanet,
	* KSPluto, KSSun and KSMoon.
	*@short Provides necessary information about objects in the solar system.
  *@author Mark Hollomon
  *@version 0.9
  */

class KStars;
class KSPlanetBase : public SkyObject {
public: 

	/**
	 *Constructor.  Calls SkyObject constructor with type=2 (planet),
	 *coordinates=0.0, mag=0.0, primary name s, and all other QStrings empty.
	 *@param s Name of planet
	 *@param im the planet's image
	 */
	KSPlanetBase( KStars *ks, QString s = i18n("unnamed"), QString image_file="" );

	/**
	 *Destructor (empty)
	 */
	virtual ~KSPlanetBase() {}

	/**
	 *@returns Ecliptic Longitude coordinate
	 */
	dms ecLong( void ) const { return ep.longitude; };

	/**
	 *@returns Ecliptic Latitude coordinate
	 */
	dms ecLat( void ) const { return ep.latitude; };

	/**
	 *Set Ecliptic Longitude according to argument.
	 *@param elong Ecliptic Longitude
	 */	
	void setEcLong( dms elong ) { ep.longitude = elong; }

	/**
	 *Set Ecliptic Longitude according to argument.
 	 *Differs from above function only in argument type.
	 *@param elong Ecliptic Longitude
	 */	
	void setEcLong( double elong ) { ep.longitude.setD( elong ); }

	/**
	 *Set Ecliptic Latitude according to argument.
	 *@param elat Ecliptic Latitude
	 */	
	void setEcLat( dms elat ) { ep.latitude = elat; }

	/**
	 *Set Ecliptic Latitude according to argument.
	 *Differs from above function only in argument type.
	 *@param elat Ecliptic Latitude
	 */	
	void setEcLat( double elat ) { ep.latitude.setD( elat ); }

	/**Load the position data from disk.
	 */
	virtual bool loadData() = 0;

	/**
	 *Given the current KSNumbers object (time-dependent), calculate and set 
	 *the RA, Dec coordinates of the Planet.
	 *@param num current KSNumbers object
	 *@param Earth planet Earth (needed to calculate geocentric coords)
	 *@returns true if position was successfully calculated.
	 */
	virtual bool findPosition( const KSNumbers *num, const KSPlanetBase *Earth=NULL ) = 0;

	/**
	 *Convert Ecliptic logitude/latitude to Right Ascension/Declination.
	 *@param Obliquity current Obliquity of the Ecliptic (angle from Equator)
	 */
	void EclipticToEquatorial( dms Obliquity );

	/**
	 *Convert Right Ascension/Declination to Ecliptic logitude/latitude.
	 *@param Obliquity current Obliquity of the Ecliptic (angle from Equator)
	 */
	void EquatorialToEcliptic( dms Obliquity );

	/**
	 *@returns pointer to image of planet
	 */
	QImage* image( void ) { return &Image; };

	/**
	 *@returns pointer to unrotated image of planet
	 */
	QImage* image0( void ) { return &Image0; };

	/**
	 *@returns distance from Sun
	 */
	double rsun( void ) const { return ep.radius; };

	/**
	 *@short Set the solar distance
	 */
	void setRsun( double r ) { ep.radius = r; };

	/**Update position of the planet (reimplemented from SkyPoint)
	 *@param num current KSNumbers object
	 *@param includePlanets this function does nothing if includePlanets=false
	 */
	virtual void updateCoords( KSNumbers *num, bool includePlanets=true );

	/**@returns the Planet's position angle.
		*/
		double pa() const { return PositionAngle; }

	/**Set the Planet's position angle.
		*@param p the new position angle
		*/
		void setPA( double p ) { PositionAngle = p; }

	/**
		*If pa argument is more than 5 degrees different than current internal
		*PositionAngle, then update the internal PA and rotate the Planet image.
		*@param pa the new position angle
		*/
	void updatePA( double pa );

	/**Function coopied from Qt 3.0 QImage...this allows simple rotation of the planet images.
		*/
	QImage xFormImage( const QWMatrix &matrix ) const;

protected:
	virtual bool loadData(QString n) { 
		kdDebug() << "didn't reimplement for " << n << endl; return false;
	};

	EclipticPosition ep;

private:
	QImage Image0, Image;
	double PositionAngle;
	KStars *kstars;
};

/**Helper function for xFormImage */
#ifndef QT_NO_PIXMAP_TRANSFORMATION
#  define QT_XFORM_TYPE_MSBFIRST 0
#  define QT_XFORM_TYPE_LSBFIRST 1
#  if defined(Q_WS_WIN)
#    define QT_XFORM_TYPE_WINDOWSPIXMAP 2
#  endif
bool qt_xForm_helper( const QWMatrix&, int, int, int, uchar*, int, int, int, uchar*, int, int, int );
#endif

#endif
