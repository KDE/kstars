/***************************************************************************
                          deepskyobject.h  -  K Desktop Planetarium
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

#ifndef DEEPSKYOBJECT_H
#define DEEPSKYOBJECT_H

#include <qpoint.h>

#include "skyobject.h"
#include "dms.h"

/**@class DeepSkyObject
	*Provides all necessary information about a deep-sky object:
	*data inherited from SkyObject (coordinates, type, magnitude, 
	*2 names, and URLs) and data specific to DeepSkyObjects
	*(common name, angular size, position angle, Image, catalog)
	*@short Information about a "dep-sky" object; i.e., anything 
	*that's not a solar system body or a star.
	*@author Jason Harris
	*@version 1.0
	*/

class QPainter;
class QImage;
class QString;
class KSPopupMenu;

class DeepSkyObject : public SkyObject {
public:
/**Constructor.  Create DeepSkyObject with data according to arguments.
	*@param t Type of object
	*@param r catalog Right Ascension
	*@param d catalog Declination
	*@param m magnitude (brightness)
	*@param n Primary name
	*@param n2 Secondary name
	*@param lname Long name (common name)
	*@param cat catalog ID
	*@param a major axis (arcminutes)
	*@param b minor axis (arcminutes)
	*@param pa position angle (degrees)
	*@param pgc PGC catalog number
	*@param ugc UGC catalog number
	*/
	DeepSkyObject( int t=SkyObject::STAR, dms r=dms(0.0), dms d=dms(0.0), 
			float m=0.0, QString n="unnamed", QString n2="", 
			QString lname="", QString cat="", 
			float a=0.0, float b=0.0, double pa=0.0, 
			int pgc=0, int ugc=0 );

/**Constructor.  Create DeepSkyObject with data according to arguments.  
	*Differs from above function only in data type of RA and Dec.
	*@param t Type of object
	*@param r catalog Right Ascension
	*@param d catalog Declination
	*@param m magnitude (brightness)
	*@param n Primary name
	*@param n2 Secondary name
	*@param lname Long name (common name)
	*@param cat catalog ID
	*@param a major axis (arcminutes)
	*@param b minor axis (arcminutes)
	*@param pa position angle (degrees)
	*@param pgc PGC catalog number
	*@param ugc UGC catalog number
	*/
	DeepSkyObject( int t, double r, double d, float m=0.0,
			QString n="unnamed", QString n2="", QString lname="", 
			QString cat="", float a=0.0, float b=0.0,
			double pa=0.0, int pgc=0, int ugc=0 );

/**Copy constructor.
	*@param o SkyObject from which to copy data
	*/
	DeepSkyObject( DeepSkyObject &o );

/**
	*Destructor 
	*/
	~DeepSkyObject() { if ( Image ) { deleteImage(); } }

/**@enum CATALOG
	*The catalog ID of the DeepSkyObject.
	*/
	enum CATALOG { CAT_MESSIER=0, CAT_NGC=1, CAT_IC=2, CAT_UNKNOWN };

/**@return a QString identifying the object's primary catalog.
	*@warning this is only used for deep-sky objects.  Possible values are:
	*- "M" for Messier catalog
	*- "NGC" for NGC catalog
	*- "IC" for IC catalog
	*- empty string is presumed to be an object in a custom catalog
	*/
	QString catalog( void ) const;

/**Set the internal Catalog value according to the QString
	*argument:
	* "M"   : CAT_MESSIER
	* "NGC" : CAT_NGC
	* "IC"  : CAT_IC
	*/
	void setCatalog( const QString &s );

/**@return the object's major axis length, in arcminute.
	*/
	float a( void ) const { return MajorAxis; }

/**@return the object's minor axis length, in arcminutes.
	*/
	float b( void ) const { return MinorAxis; }

/**@return the object's aspect ratio (MinorAxis/MajorAxis).  Returns 1.0
	*if the object's MinorAxis=0.0.
	*/
	float e( void ) const;

/**@return the object's position angle, meausred clockwise from North.
	*/
	virtual double pa() const { return PositionAngle; }

/**@return the object's UGC catalog number.  Return 0 if the object is not in UGC.
	*/
	int ugc( void ) const { return UGC; }

/**@return the object's PGC catalog number.  Return 0 if the object is not in PGC.
	*/
	int pgc( void ) const { return PGC; }

/**Read in this object's image from disk, unless it already exists in memory.
	*@returns pointer to newly-created image.
	*/
	QImage *readImage();

/**@return pointer to the object's inline image.  If it is currently
	*a null pointer, it loads the image from disk.
	*/
	QImage *image() const { return Image; }

/**delete the Image pointer, and set it to 0.
	*/
	void deleteImage();

/**@return true if the object is in the Messier catalog
	*/
	bool isCatalogM() const { return (Catalog == CAT_MESSIER); }

/**@return true if the object is in the NGC catalog
	*/
	bool isCatalogNGC() const { return (Catalog == CAT_NGC); }

/**@return true if the object is in the IC catalog
	*/
	bool isCatalogIC() const { return (Catalog == CAT_IC); }

/**@return true if the object is not in a catalog
	*/
	bool isCatalogNone() const { return (Catalog == CAT_UNKNOWN); }

/**Draw the object's symbol on the map
	*/
	void drawSymbol( QPainter &psky, int x, int y, double PositionAngle, double zoom, double scale=1.0 );

/**Draw the Object's image on the map
	*/
	void drawImage( QPainter &psky, int x, int y, double PositionAngle, double zoom, double scale=1.0 );

/**Show Deep-sky object popup menu.  Overloaded from virtual 
	*SkyObject::showPopupMenu()
	*@param pmenu pointer to the KSPopupMenu object
	*@param pos QPojnt holding the x,y coordinates for the menu
	*/
	virtual void showPopupMenu( KSPopupMenu *pmenu, QPoint pos ) { pmenu->createDeepSkyObjectMenu( this ); pmenu->popup( pos ); }

private:
	unsigned char Catalog; 
        double PositionAngle;
	int UGC, PGC;
	float MajorAxis, MinorAxis;
	QImage *Image;
};

#endif
