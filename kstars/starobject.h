/***************************************************************************
                          starobject.h  -  K Desktop Planetarium
                             -------------------
    begin                : Tue Sep 18 2001
    copyright            : (C) 2001 by Jason Harris
    email                : kstars@30doradus.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/




#ifndef STAROBJECT_H
#define STAROBJECT_H

#include "skyobject.h"

class SkyObjectName;

/**
  *@author Thomas Kabelmann
  */

// StarObject is a subclass of SkyObject and has implemented the color of the star stored in QChar
// n is the latin name of the stars (e.g., Betelgeuse)
// n2 is the genetive name (e.g., alpha Orionis)
class StarObject : public SkyObject {
	public:

/**
  *Default constructor.  Call default SkyObject constructor, and set soName=""
  *and SpType=0
  */
		StarObject();
/**
  *Copy constructor
  */
		StarObject(StarObject & o);
/**
  *Constructor.  Sets sky coordinates, magnitude, latin name, genetive name, and
  *spectral type.
  *@param r Right Ascension
  *@param d Declination
  *@param m magnitude
  *@param n latin name
	*@param n2 genetive name
  *@param SpType Spectral Type
  */
		StarObject( dms r, dms d, double m, QString n="unnamed",
  					 		QString n2="", QString SpType="" );
/**
  *Constructor.  Sets sky coordinates, magnitude, latin name, genetive name, and
  *spectral type.  Differs from above function only in argument type.
  *@param r Right Ascension
  *@param d Declination
  *@param m magnitude
  *@param n latin name
	*@param n2 genetive name
  *@param SpType Spectral Type
  */
		StarObject( double r, double d, double m, QString n="unnamed",
  					 		QString n2="", QString SpType="" );
/**
  *Empty destructor.
  */
		~StarObject();
/**
  *Returns first character of Spectral Type string, which is used to
  *select the temperature-color of the star.
  *@returns first character of SpType string
  */
		QChar color( void ) { return SpType.at(0); }
/**
  *Returns entire spectral type string
  *@returns SpType string
  */
		QString sptype( void ) { return SpType; }
/**
  *Returns the genetive name of the star.
  *@returns genetive name of the star
  */
		QString gname( void ) { return name2; }
/**
  *Returns the greek letter portion of the star's genetive name.
  *Returns empty string if star has no genetive name defined.
  *@returns greek letter portion of genetive name
  */
		QString greekLetter( void );		

//		void setSkyObjectName( SkyObjectName *n ) { soName = n; }
//		SkyObjectName *skyObjectName() { return soName; }
		
	private:
		QString SpType;
		
		SkyObjectName *soName;
};

#endif
