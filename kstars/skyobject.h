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

	float a( void ) const { return MajorAxis; }
	float b( void ) const { return MinorAxis; }
	float e( void ) const;
	int pa( void ) const { return PositionAngle; }
	int ugc( void ) const { return UGC; }
	int pgc( void ) const { return PGC; }

/**
  *Return the local time that the object will rise
  *@param jd  current Julian date
  *@param geo current geographic location
  */
	QTime riseTime( long double jd, GeoLocation *geo );

/**
  *Return the local time that the object will set
  */
	QTime setTime( long double jd, GeoLocation *geo );

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
};

#endif
