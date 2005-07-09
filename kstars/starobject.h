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

#include <qpoint.h>

#include "skyobject.h"

class QPainter;
class QString;
class KSPopupMenu;

/**@class StarObject 
	*This is a subclass of SkyObject.  It adds the Spectral type, and flags
	*for variability and multiplicity.
	*For stars, the primary name (n) is the latin name (e.g., "Betelgeuse").  The
	*secondary name (n2) is the genetive name (e.g., "alpha Orionis").
	*@short subclass of SkyObject specialized for stars.
	*@author Thomas Kabelmann
	*@version 1.0
	*/

class StarObject : public SkyObject {
	public:

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
	*@param n common name
	*@param n2 genetive name
	*@param sptype Spectral Type
	*@param pmra Proper motion in RA direction [mas/yr]
	*@param pmdec Proper motion in Dec direction [mas/yr]
	*@param par Parallax angle [mas]
	*@param mult Multiplicity flag (false=dingle star; true=multiple star)
	*@param var Variability flag (true if star is a known periodic variable)
	*/
	StarObject( dms r=dms(0.0), dms d=dms(0.0), float m=0.0, QString n="",
				QString n2="", QString sptype="--", double pmra=0.0, double pmdec=0.0,
				double par=0.0, bool mult=false, bool var=false );
/**
	*Constructor.  Sets sky coordinates, magnitude, latin name, genetive name, and
	*spectral type.  Differs from above function only in data type of RA and Dec.
	*@param r Right Ascension
	*@param d Declination
	*@param m magnitude
	*@param n common name
	*@param n2 genetive name
	*@param sptype Spectral Type
	*@param pmra Proper motion in RA direction [mas/yr]
	*@param pmdec Proper motion in Dec direction [mas/yr]
	*@param par Parallax angle [mas]
	*@param mult Multiplicity flag (false=dingle star; true=multiple star)
	*@param var Variability flag (true if star is a known periodic variable)
	*/
	StarObject( double r, double d, float m=0.0, QString n="",
				QString n2="", QString sptype="--", double pmra=0.0, double pmdec=0.0,
				double par=0.0, bool mult=false, bool var=false );

/**
	*Empty destructor.
	*/
	~StarObject() {}

/**
	*If star is unnamed return "star" otherwise return the name
	*/
	virtual QString name( void ) const { return hasName() ? *Name : starString;}

/**
	*If star is unnamed return "star" otherwise return the longname
	*/
	virtual QString longname( void ) const { return hasLongName() ? *LongName : starString; }
/**
	*Returns first character of Spectral Type string, which is used to
	*select the temperature-color of the star.
	*@return first character of Spectral Type string
	*/
	QChar color( void ) const { return SpType.at(0); }

/**
	*Returns entire spectral type string
	*@return Spectral Type string
	*/
	QString sptype( void ) const;

/**
	*Returns the genetive name of the star.
	*@return genetive name of the star
	*/
	QString gname( bool useGreekChars=true ) const;

/**
	*Returns the greek letter portion of the star's genetive name.
	*Returns empty string if star has no genetive name defined.
	*@return greek letter portion of genetive name
	*/
	QString greekLetter( bool useGreekChars=true ) const;

/**@return the genitive form of the star's constellation.
	*/
	QString constell( void ) const;

/**Determine the current coordinates (RA, Dec) from the catalog
	*coordinates (RA0, Dec0), accounting for both precession and nutation.
	*@param num pointer to KSNumbers object containing current values of
	*time-dependent variables.
	*@param includePlanets does nothing in this implementation (see KSPlanetBase::updateCoords()).
	*@param lat does nothing in this implementation (see KSPlanetBase::updateCoords()).
	*@param LST does nothing in this implementation (see KSPlanetBase::updateCoords()).
	*/
	virtual void updateCoords( KSNumbers *num, bool includePlanets=true, const dms *lat=0, const dms *LST=0 );

/**@short Set the Ra and Dec components of the star's proper motion, in milliarcsec/year.
	*Note that the RA component is multiplied by cos(dec).
	*@param pmra the new RA propoer motion
	*@param pmdec the new Dec proper motion
	*/
	void setProperMotion( double pmra, double pmdec ) { PM_RA = pmra; PM_Dec = pmdec; }

/**@return the RA component of the star's proper motion, in mas/yr (multiplied by cos(dec))
	*/
	double pmRA() const { return PM_RA; }

/**@return the Dec component of the star's proper motion, in mas/yr
	*/
	double pmDec() const { return PM_Dec; }

/**@short set the star's parallax angle, in milliarcsec
	*/
	void setParallax( double plx ) { Parallax = plx; }

/**@return the star's parallax angle, in milliarcsec
	*/
	double parallax() const { return Parallax; }

/**@return the star's distance from the Sun in parsecs, as computed from the parallax.
	*/
	double distance() const { return 1000./parallax(); }

/**@short set the star's multiplicity flag (i.e., is it a binary or multiple star?)
	*@param m true if binary/multiple star system
	*/
	void setMultiple( bool m ) { Multiplicity = m; }

/**@return whether the star is a binary or multiple starobject
	*/
	bool isMultiple() const { return Multiplicity; }

/**@short set the star's variability flag
	*@param v true if star is variable
	*/
	void setVariable( bool v ) { Variability = v; }

/**@return whether the star is a binary or multiple starobject
	*/
	bool isVariable() const { return Variability; }

//Not using VRange, VPeriod currently (to save memory)
///**@short set the range in brightness covered by the star's variability
//	*@param r the range of brightness, in magnitudes
//	*/
//	void setVRange( double r ) { VRange = r; }
//
///**@return the range in brightness covered by the star's variability, in magnitudes
//	*/
//	double vrange() const { return VRange; }
//
///**@short set the period of the star's brightness variation, in days.
//	*/
//	void setVPeriod( double p ) { VPeriod = p; }
//
///**@return the period of the star's brightness variation, in days.
//	*/
//	double vperiod() const { return VPeriod; }

	void draw( QPainter &psky, QPixmap *sky, QPixmap *starpix, int x, int y, bool drawMultiple=true, double scale=1.0 );

	//overloaded from SkyObject
	void drawLabel( QPainter &psky, int x, int y, double zoom, bool drawName, bool drawMag, double scale );

/**Show star object popup menu.  Overloaded from virtual 
	*SkyObject::showPopupMenu()
	*@param pmenu pointer to the KSPopupMenu object
	*@param pos QPojnt holding the x,y coordinates for the menu
	*/
	virtual void showPopupMenu( KSPopupMenu *pmenu, QPoint pos ) { pmenu->createStarMenu( this ); pmenu->popup( pos ); }

private:
	QString SpType;

	double PM_RA, PM_Dec, Parallax;  //, VRange, VPeriod;
	bool Multiplicity, Variability;
};

#endif
