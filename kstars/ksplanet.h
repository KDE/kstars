/***************************************************************************
                          ksplanet.h  -  K Desktop Planetarium
                             -------------------
    begin                : Sun Jul 22 2001
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

#include <qstring.h>
#include <qimage.h>
#include "skyobject.h"
#include "dms.h"

#ifndef KSPLANET_H
#define KSPLANET_H

/**
	*A subclass of SkyObject that provides additional information
	*needed for solar system objects.  KSPlanet may be used
	*directly for all planets except Pluto.  The Sun and Moon are subclassed from
	*KSPlanet.
	*@short Provides necessary information about objects in the solar system.
  *@author Jason Harris
  *@version 0.4
  */

class KSPlanet : public SkyObject {
public: 
/**
	*Default constructor.  Calls other constructor with s="unnamed" and
	*a null image.
	*/
	KSPlanet();
/**
	*Constructor.  Calls SkyObject constructor with type=2 (planet),
	*coordinates=0.0, mag=0.0, primary name s, and all other QStrings empty.
	*@param s Name of planet
	*/
	KSPlanet( QString s, QImage im=NULL );
/**
	*Destructor (empty)
	*/
	~KSPlanet();
/**
	*Returns Ecliptic Longitude coordinate
	*/
	dms getEcLong( void ) { return EcLong; }
/**
	*Returns Ecliptic Latitude coordinate
	*/
	dms getEcLat( void ) { return EcLat; }
/**
	*Set Ecliptic Longitude according to argument.
	*@param elong Ecliptic Longitude
	*/	
	void setEcLong( dms elong ) { EcLong = elong; }
/**
	*Set Ecliptic Longitude according to argument.
	*Differs from above function only in argument type.
	*@param elong Ecliptic Longitude
	*/	
	void setEcLong( double elong ) { EcLong.setD( elong ); }
/**
	*Set Ecliptic Latitude according to argument.
	*@param elat Ecliptic Latitude
	*/	
	void setEcLat( dms elat ) { EcLat = elat; }
/**
	*Set Ecliptic Latitude according to argument.
	*Differs from above function only in argument type.
	*@param elat Ecliptic Latitude
	*/	
	void setEcLat( double elat ) { EcLat.setD( elat ); }
/**
	*Given the current Julian date (Epoch), calculate the RA, Dec
	*coordinates of the Planet.
	*@param Epoch current Julain Date
	*@param Earth planet Earth (needed to calculate geocentric coords)
	*@returns true if position was successfully calculated.
	*/
	bool findPosition( long double Epoch, KSPlanet *Earth=NULL );
/**
	*Convert Ecliptic logitude/latitude to Right Ascension/Declination,
	*given the current Julian date (Epoch).	
	*@param Epoch current Julain Date
	*/
	void EclipticToEquatorial( long double Epoch );
/**
	*Convert Right Ascension/Declination to Ecliptic logitude/latitude,
	*given the current Julian date (Epoch).	
	*@param Epoch current Julain Date
	*/
	void EquatorialToEcliptic( long double Epoch );
/**
	*Determine effects of nutation on Ecliptic longitude and the obliquity
	*/
	void nutate( long double Epoch, double &delong, double &dobliq );

	QString EnglishName;
	QImage image;
	dms EcLong, EcLat;
	double Rsun;
};

#endif
