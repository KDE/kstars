/***************************************************************************
                          starobject.h  -  K Desktop Planetarium
                             -------------------
    begin                : Tue Sep 18 2001
    copyright            : (C) 2001 by Thomas Kabelmann
    email                : tk78@gmx.de
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
class QPainter;
class QString;

/**StarObject is a subclass of SkyObject.  It adds the Spectral type of the star as a QString.
	*For stars, the primary name (n) is the latin name (e.g., "Betelgeuse").  The
	*secondary name (n2) is the genetive name (e.g., "alpha Orionis").
	*@short subclass of SkyObject specialized for stars.
	*@author Thomas Kabelmann
	*@version 0.9
  */

class StarObject : public SkyObject {
	public:

/**
  *Default constructor.  Call default SkyObject constructor, and set Name=""
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
  					 		QString n2="", QString sptype="--" );
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
  					 		QString n2="", QString sptype="--" );

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
	*@param sptype Spectral Type
	*/
		StarObject( int t, dms r, dms d, double m,
						QString n="unnamed", QString n2="", QString lname="", QString cat="",
						double a=0.0, double b=0.0,
						int pa=0, int pgc=0, int ugc=0, QString sptype="--" );

/**
  *Empty destructor.
  */
		~StarObject() {}
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
//		QString sptype( void ) { return SpType; }
		QString sptype( void );
/**
  *Returns the genetive name of the star.
  *@returns genetive name of the star
  */
		QString gname( void ) { return name2(); }
/**
  *Returns the greek letter portion of the star's genetive name.
  *Returns empty string if star has no genetive name defined.
  *@returns greek letter portion of genetive name
  */
		QString greekLetter( void );

		//overloaded from SkyObject
		void drawLabel( QPainter &psky, int x, int y, double zoom, bool drawName, bool drawMag, double scale );

	private:
		QString SpType;
		SkyObjectName *soName;
};

#endif
