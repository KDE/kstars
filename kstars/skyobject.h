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
#include "skypoint.h"
#include "dms.h"

/**
	*Provides all necessary information about an object in the sky:
	*its coordinates, name, long name, type, magnitude, and an
	*optional image URL.  A future version may provide a QStringList of
	*URLs.
	*@short Information about an object in the sky
  *@author Jason Harris
	*@version 0.4
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
	dms getRA( void ) { return pos()->getRA(); }
/**
	*Shortcut for retrieving current Declination
	*/
  dms getDec( void ) { return pos()->getDec(); }
/**
	*Shortcut for retrieving Azimuth
	*/
	dms getAz( void ) { return pos()->getAz(); }
/**
	*Shortcut for retrieving Altitude
	*/
	dms getAlt( void ) { return pos()->getAlt(); }

  int type;
  float mag;
  QString name;
  QString name2;
  QString longname;
	QStringList ImageList, ImageTitle;
	QStringList InfoList, InfoTitle;
private:
	SkyPoint Position;
};

#endif
