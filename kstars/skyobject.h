/***************************************************************************
                          skyobject.h  -  K Desktop Planetarium
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

#ifndef SKYOBJECT_H
#define SKYOBJECT_H

#include <qstring.h>
#include <qstringlist.h>
#include <qdatetime.h>
#include <qimage.h>

#include <klocale.h>

#include "skypoint.h"
#include "dms.h"
#include "geolocation.h"

/**
	*Provides all necessary information about an object in the sky:
	*its coordinates, name(s), type, magnitude, and QStringLists of
	*URLs for images and webpages regarding the object.
	*@short Information about an object in the sky
  *@author Jason Harris
	*@version 0.9
  */

class SkyObject : public SkyPoint {
public: 
/**
	*Default Constructor.  Sets all coordinates to zero.  Sets type to
	*0 (star), magnitude to 0.0, name to "unnamed", all other strings
	*are made empty.
	*/
	SkyObject();

/**
	*Copy constructor.
	*@param o SkyObject from which to copy data
	*/
  SkyObject( SkyObject &o );

/**
	*Constructor.  Set SkyObject data according to arguments.
	*@param t Type of object
	*@param r catalog Right Ascension
	*@param d catalog Declination
	*@param m magnitude (brightness)
	*@param a major axis (arcminutes)
	*@param b minor axis (arcminutes)
	*@param pa position angle (degrees)
	*@param pgc PGC catalog number
	*@param ugc UGC catalog number
	*@param n Primary name
	*@param n2 Secondary name
	*@param lname Long name (common name)
	*/
  SkyObject( int t, dms r, dms d, double m,
						QString n="unnamed", QString n2="", QString lname="", QString cat="",
						double a=0.0, double b=0.0,
						int pa=0, int pgc=0, int ugc=0 );

/**
	*Constructor.  Set SkyObject data according to arguments.  Differs from
	*above function only in argument type.
	*@param t Type of object
	*@param r catalog Right Ascension
	*@param d catalog Declination
	*@param m magnitude (brightness)
	*@param n Primary name
	*@param n2 Secondary name
	*@param lname Long name (common name)
	*/
	SkyObject( int t, double r, double d, double m,
						QString n="unnamed", QString n2="", QString lname="", QString cat="",
						double a=0.0, double b=0.0,
						int pa=0, int pgc=0, int ugc=0 );
/**
	*Destructor (empty)
	*/
	~SkyObject() {};

/**@returns object's primary name
	*/
  QString name( void ) const { return Name; }

/**@returns translated primary name
	*/
	QString translatedName() const { return i18n(Name.local8Bit().data());}

/**@returns object's secondary name
	*/
  QString name2( void ) const { return Name2; }

/**@returns object's common (long) name
	*/
  QString longname( void ) const { return LongName; }

/**@returns a code identifying the object's catalog
	*/
	QString catalog( void ) const { return Catalog; }

/**@returns object's type identifier
	*/
	int type( void ) const { return Type; }

/**@returns object's magnitude
	*/
	float mag( void ) const { return Magnitude; }

/**@returns the object's major axis length, which should be in arcminutes.
	*/
	float a( void ) const { return MajorAxis; }

/**@returns the object's minor axis length, which should be in arcminutes.
	*/
	float b( void ) const { return MinorAxis; }

/**@returns the object's aspect ratio (MinorAxis/MajorAxis).  Returns 1.0
	*if the object's MinorAxis=0.0.
	*/
	float e( void ) const;

/**@returns the object's position angle, meausred clockwise from North.
	*/
	int pa( void ) const { return PositionAngle; }

/**@returns the object's UGC catalog number.  This is only valid for some
	*deep-sky objects, and will return 0 in other cases.
	*/
	int ugc( void ) const { return UGC; }

/**@returns the object's PGC catalog number.  This is only valid for some
	*deep-sky objects, and will return 0 in other cases.
	*/
	int pgc( void ) const { return PGC; }

/**Read in this object's image from disk, unless it already exists in memory.
	*@returns pointer to newly-created image.
	*/
	QImage *readImage();
/**@returns pointer to the object's inline image.  If it is currently
	*a null pointer, it loads the image from disk.
	*/
	QImage *image() const { return Image; }

/**@delete the Image pointer, and set it to 0.
	*/
	void deleteImage() { delete Image; Image = 0; }

/**
  *Return the local time that the object will rise
  *@param jd  current Julian date
  *@param geo current geographic location
  */
	QTime riseTime( long double jd, GeoLocation *geo );

/**
  *Return the local time that the object will set
  *@param jd  current Julian date
  *@param geo current geographic location
  */
	QTime setTime( long double jd, GeoLocation *geo );

	QTime transitTime( long double jd, GeoLocation *geo );
	double transitUTTime( long double jd, dms gLng, dms gLat );
	void setThreeCoords (long double jd);
	double approxHourAngle (dms h0, dms gLng);
	dms gstAtCeroUT (long double jd);
	dms transitAltitude(GeoLocation *geo);

	/**
	 * Interpolates the value of a function y=f(n+x2)
	 * given the values of the function at three points:
	 * y1=f(x1)    y2=f(x2)    y3=f(x3)    n=x-x2 / x1<x2<x3
	 *
	 *@param y1    First y value
	 *@param y2    Second y value
	 *@param y3    Third y value
	 *@param n     Interpolation factor
	 *
	 *@returns    interpolated value
	 **/

	double Interpolate (double y1, double y2, double y3, double nx);
	/**
	 * Reduces a double variable which corresponds to time expressed in 
	 * Days, to the interval (0,1). If value is < 0 or > 1, one 
	 * has to add or substract 1.
	 *
	 *@param x    value to correct
	 *@returns    corrected value
	 */
	double reduceToOne(double x);

	/**
	 * Reduces a double variable which corresponds to degrees so that
	 * it remains in the interval (-180,180)
	 * has to add or substract 1.

	 *@param h    angle to correct
	 *@returns    corrected value
	 */
	double reduceTo180(double h);

	/**
	 * Checks whether a source is circumpolar or not. True = cirmcumpolar
	 * False = Not circumpolar
	 *
	 *@returns true if circumpolar
	 */
	bool checkCircumpolar(dms gLng);

	/**
	 * Corrects for the geometric altitude of the center of the body at the
	 * time of rising or setting. This is due to refraction at the horizon
	 * and to the size of the body. The moon correction is only a rough
	 * approximation.
	 *
	 * Weather status (temperature and pressure basically) is not taken 
	 * into account.
	 *
	 *@returns dms object with the correction. 
	 */
	dms elevationCorrection(void);

	QStringList ImageList, ImageTitle;
	QStringList InfoList, InfoTitle;

private:
	int Type, PositionAngle;
	int UGC, PGC;
	float MajorAxis, MinorAxis, Magnitude;
	QString Name;
	QString Name2;
	QString LongName;
	QString Catalog;
	QImage *Image;
	
	// Auxiliary variables
	
	dms ra1, dec1, ra2, dec2, ra3, dec3;
};

#endif
