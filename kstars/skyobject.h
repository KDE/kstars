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

class SkyObject {
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
	*@param n Primary name
	*@param n2 Secondary name
	*@param lname Long name (common name)
	*/
  SkyObject( int t, dms r, dms d, double m, QString n="unnamed",
  					 QString n2="", QString lname="" );	

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
  SkyObject( int t, double r, double d, double m, QString n="unnamed",
  					 QString n2="", QString lname="" );	
/**
	*Destructor (empty)
	*/
	~SkyObject() {};

/**
	*Returns a pointer to the SkyPoint object containing this object's position information
	*/
	SkyPoint* pos( void ) { return &Position; }

/**
	*Shortcut for retrieving current Right Ascension
	*/
	dms ra( void ) { return pos()->ra(); }

/**
	*Shortcut for retrieving current Declination
	*/
  dms dec( void ) { return pos()->dec(); }

/**
	*Shortcut for setting Right Ascension
	*/
	void setRA( double r ) { pos()->setRA( r ); }

/**
	*Shortcut for setting Declination
	*/
  void setDec( double d ) { pos()->setDec( d ); }

/**
	*Shortcut for setting Right Ascension
	*/
	void setRA( dms r ) { pos()->setRA( r ); }

/**
	*Shortcut for setting Declination
	*/
  void setDec( dms d ) { pos()->setDec( d ); }

/**
	*Shortcut for retrieving Azimuth
	*/
	dms az( void ) { return pos()->az(); }

/**
	*Shortcut for retrieving Altitude
	*/
	dms alt( void ) { return pos()->alt(); }

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

/**@returns object's type identifier
	*/
	int type( void ) const { return Type; }

/**@returns object's magnitude
	*/
	float mag( void ) const { return Magnitude; }

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
	int Type;
	float Magnitude;
	QString Name;
	QString Name2;
	QString LongName;
	SkyPoint Position;
};

#endif
