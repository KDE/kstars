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

#include <qpainter.h>
#include <qstring.h>
#include <qstringlist.h>
#include <qdatetime.h>
#include <qimage.h>

#include <klocale.h>

#include "skypoint.h"
#include "dms.h"
#include "geolocation.h"

/**@class SkyObject
	*@short Information about an object in the sky.
	*@author Jason Harris
	*@version 1.0
	*Provides all necessary information about an object in the sky:
	*its coordinates, name(s), type, magnitude, and QStringLists of
	*URLs for images and webpages regarding the object.
  */

class SkyObject : public SkyPoint {
public:
/**Default Constructor.  Sets all coordinates to zero.  Sets type to
	*0 (star), magnitude to 0.0, name to "unnamed", all other strings
	*are made empty.
	*/
	SkyObject();

/**Constructor.  Set SkyObject data according to arguments.
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
	*above function only in argument types.
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

/**Copy constructor.
	*@param o SkyObject from which to copy data
	*/
	SkyObject( SkyObject &o );

/**
	*Destructor (empty)
	*/
	~SkyObject() {};

/**@enum TYPE
	*The type classification of the SkyObject.
	*/
	enum TYPE { STAR=0, CATALOG_STAR=1, PLANET=2, OPEN_CLUSTER=3, GLOBULAR_CLUSTER=4,
		GASEOUS_NEBULA=5, PLANETARY_NEBULA=6, SUPERNOVA_REMNANT=7, GALAXY=8, COMET=9,
		ASTEROID=10, UNKNOWN};

/**@return object's primary name.
	*/
	QString name( void ) const { return Name; }

/**@return object's primary name, translated to local language.
	*/
	QString translatedName() const { return i18n(Name.local8Bit().data());}

/**Set the object's primary name.
	*@param name the object's primary name
	*/
	void setName( const QString &name ) { Name = name; }

/**@return object's secondary name
	*/
	QString name2( void ) const { return Name2; }

/**Set the object's secondary name.
	*@param the object's secondary name.
	*/
	void setName2( const QString &name2="" );

/**@return object's common (long) name
	*/
	QString longname( void ) const { return LongName; }

/**Set the object's long name.
	*@param the object's long name.
	*/
	void setLongName( const QString &longname="" );

/**@return a QString identifying the object's primary catalog.
	*@warning this is only used for deep-sky objects.  Possible values are:
	*- "M" for Messier catalog
	*- "NGC" for NGC catalog
	*- "IC" for IC catalog
	*- empty string is presumed to be an object in a custom catalog
	*/
	QString catalog( void ) const { return Catalog; }

/**@return object's type identifier (int)
	*@see enum TYPE
	*/
	int type( void ) const { return Type; }

/**Set the object's type identifier to the argument.
	*@param t the object's type identifier (e.g., "SkyObject::PLANETARY_NEBULA")
	*@see enum TYPE
	*/
	void setType( int t ) { Type = t; }

/**@return a string describing object's type.
	*/
	QString typeName( void ) const;

/**@return object's magnitude
	*/
	float mag( void ) const { return Magnitude; }

/**Set the object's magnitude.
	*@param m the object's magnitude.
	*/
	void setMag( float m ) { Magnitude = m; }

/**@return the object's major axis length, in arcminutes.
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
	int pa( void ) const { return PositionAngle; }

/**@return the object's UGC catalog number.  This is only valid for some
	*deep-sky objects, and will return 0 in other cases.
	*/
	int ugc( void ) const { return UGC; }

/**@return the object's PGC catalog number.  This is only valid for some
	*deep-sky objects, and will return 0 in other cases.
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
	void deleteImage() { delete Image; Image = 0; }

/**Determine the rise or set time of an object.  Because solar system
	*objects move across the sky, it is necessary to iterate on the solution.
	*We compute the rise/set time for the object's current position, then
	*compute the object's position at that time.  Finally, we recompute then
	*rise/set time for the new coordinates.  Further iteration is not necessary,
	*even for the most swiftly-moving object (the Moon).
	*@return the local time that the object will rise
	*@param jd  current Julian date
	*@param geo current geographic location
	*@param rst If TRUE, compute rise time. If FALSE, compute set time.
	*/
	QTime riseSetTime( long double jd, const GeoLocation *geo, bool rst );

/**@return the UT time when the object will rise or set
	*@param jd  current Julian date
	*@param geo pointer to Geographic location
	*@param rst Boolean. If TRUE will compute rise time. If FALSE
	*       will compute set time.
	*/
	QTime riseSetTimeUT( long double jd, const GeoLocation *geo, bool rst);

/**@return the LST time when the object will rise or set
  *@param jd  current Julian date
  *@param geo pointer to Geographic location
  *@param gLt Geographic latitude
  *@param rst Boolean. If TRUE will compute rise time. If FALSE
  *       will compute set time.
  */
	dms riseSetTimeLST( long double jd, const GeoLocation *geo, bool rst);

/**@return the Azimuth time when the object will rise or set. This function
  *recomputes set or rise UT times.
  *@param jd Julian Day
  *@param geo GeoLocation object
  *@param rst Boolen. If TRUE will compute rise time. If FALSE
  *       will compute set time.
  */
	dms riseSetTimeAz(long double jd, const GeoLocation *geo, bool rst);

/**The same iteration technique described in riseSetTime() is used here.
	*@return the local time that the object will transit the meridian.
	*@param jd the julian day
	*@param geo pointer to the geographic location
	*/
	QTime transitTime( long double jd, const GeoLocation *geo );

/**@return the universal time that the object will transit the meridian.
	*@param jd the julian day
	*@param gLng pointer to the geographic longitude
	*/
	QTime transitTimeUT( long double jd, const GeoLocation *geo );

/**@return the altitude of the object at the moment it transits the meridian.
	*@param jd the Julian Day
	*@param geo pointer to the geographic location
	*/
	dms transitAltitude( long double jd, const GeoLocation *geo );

/**Check whether a source is circumpolar or not. True = cirmcumpolar
	*False = Not circumpolar
	*@return true if circumpolar
	*/
	bool checkCircumpolar( const dms *gLng );

/**@return true if the object is in the Messier catalog
	*/
	bool isCatalogM()    { return bIsCatalogM; }

/**@return true if the object is in the NGC catalog
	*/
	bool isCatalogNGC()  { return bIsCatalogNGC; }

/**@return true if the object is in the IC catalog
	*/
	bool isCatalogIC()   { return bIsCatalogIC; }

/**@return true if the object is not in a catalog
	*/
	bool isCatalogNone() { return bIsCatalogNone; }

/**@return true if the object is a solar system body.
	*/
	bool isSolarSystem() { return ( type() == 2 || type() == 9 || type() == 10 ); }

	QStringList ImageList, ImageTitle;
	QStringList InfoList, InfoTitle;
	QString userLog;

private:

/**Compute the UT time when the object will rise or set. It is an auxiliary
	*procedure because it does not use the RA and DEC of the object but values
	*given as parameters. You may want to use riseSetTimeUT() which is
	*public.  riseSetTimeUT() calls this function iteratively.
	*@param jd     current Julian date
	*@param geo    pointer to Geographic location
	*@param righta pointer to Right ascention of the object
	*@param decl   pointer to Declination of the object
	*@param rst    Boolean. If TRUE will compute rise time. If FALSE
	*              will compute set time.
	*@return the time at which the given position will rise or set.
	*/
	QTime auxRiseSetTimeUT( long double jd, const GeoLocation *geo,
				const dms *righta, const dms *decl, bool riseT);

/**Compute the LST time when the object will rise or set. It is an auxiliary
	*procedure because it does not use the RA and DEC of the object but values
	*given as parameters. You may want to use riseSetTimeLST() which is
	*public.  riseSetTimeLST() calls this function iteratively.
	*@param gLt Geographic latitude
	*@param rga Right ascention of the object
	*@param decl Declination of the object
	*@param rst Boolean. If TRUE will compute rise time. If FALSE
	*       will compute set time.
	*/
	dms auxRiseSetTimeLST( const dms *gLt, const dms *rga, const dms *decl, bool rst );

/**Compute the approximate hour angle that an object with declination d will have
	*when its altitude is h (as seen from geographic latitude gLat).
	*This function is only used by auxRiseSetTimeLST().
	*@param h pointer to the altitude of the object
	*@param gLat pointer to the geographic latitude
	*@param d pointer to the declination of the object.
	*@return the Hour Angle, in degrees.
	*/
	double approxHourAngle( const dms *h, const dms *gLat, const dms *d );

/**Correct for the geometric altitude of the center of the body at the
	*time of rising or setting. This is due to refraction at the horizon
	*and to the size of the body. The moon correction is only a rough
	*approximation.
	*
	*Weather status (temperature and pressure basically) is not taken
	*into account.
	*
	*This function is only used by auxRiseSetTimeLST().
	*@return dms object with the correction.
	*/
	dms elevationCorrection(void);

/**This function is only used internally for computing rise and set times.
	*@return the julian day for which JD gives the date and UT the time.
	*@param jd  Julian day from which the date is extracted.
	*@param UT  Universal Time for which the JD is computed.
	*/
	long double newJDfromJDandUT(long double jd, QTime UT);

/**This function is only used internally, to calculate rise/set times.
	*@return the coordinates of the selected object for the time given by jd0
	*@param jd  Julian day for which the coordinates of the object are given.
	*           This parameter is necessary to return the selected object to
	*           its original position.
	*@param jd0 Julian day for which the coords will be recomputed.
	*@param geo pointer to geographic location (used for solar system only)
	*/
	SkyPoint getNewCoords(long double jd, long double jd0, const GeoLocation *geo=0);

/**Set the object's catalog flags based on the value of its Catalog QString member.
	*/
	void calcCatalogFlags();


	int Type, PositionAngle;
	int UGC, PGC;
	float MajorAxis, MinorAxis, Magnitude;
	QString Name;
	QString Name2;
	QString LongName;
	QString Catalog;
	QImage *Image;

	// optimize string handling: precalculate boolean flags
	bool bIsCatalogM;
	bool bIsCatalogNGC;
	bool bIsCatalogIC;
	bool bIsCatalogNone;
};

#endif
