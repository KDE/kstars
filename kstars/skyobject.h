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
	void setLongName( const QString &longname="" );

/**@returns a code identifying the object's catalog
	*/
	QString catalog( void ) const { return Catalog; }

/**@returns object's type identifier
	*/
	int type( void ) const { return Type; }

/**@returns a string describing object's type
        */
	QString typeName( void ) const;

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
	QTime riseTime( long double jd, const GeoLocation *geo );

/**
  *Returns the UT time when the object will rise or set
  *@param jd  current Julian date
  *@param gLn Geographic longitude
  *@param gLt Geographic latitude
  *@param rst Boolean. If TRUE will compute rise time. If FALSE 
  *       will compute set time.
  */
	dms riseUTTime( long double jd, const dms *gLn, const dms *gLt, bool rst);

/**
  *Returns the LST time when the object will rise or set
  *@param jd  current Julian date
  *@param gLn Geographic longitude
  *@param gLt Geographic latitude
  *@param rst Boolean. If TRUE will compute rise time. If FALSE 
  *       will compute set time.
  */
	dms riseLSTTime( long double jd, const dms *gLn, const dms *gLt, bool rst);

/**
  *Returns the Azimuth time when the object will rise or set. This function
  *recomputes set or rise UT times. 
  *@param jd Julian Day
  *@param geo GeoLocation object
  *@param rst Boolen. If TRUE will compute rise time. If FALSE 
  *       will compute set time.
  */
	dms riseSetTimeAz (long double jd, const GeoLocation *geo, bool rst);
/**
  *Returns the julian day for which JD gives the date and UT the time. 
  *@param jd  Julian day from which the date is extracted. 
  *@param UT  Universal Time for which the JD is computed. 
  */

	long double newJDfromJDandUT(long double jd, dms UT);
/**
  *Returns the coordinates of the selected object for the time given by jd0
  *@param jd  Julian day for which the coordinates of the object are given.
  *           This parameter is necessary to return the selected object to
  *           its original position.
  *@param jd0 Julian day for which the coords will be recomputed. 
  */

	SkyPoint getNewCoords(long double jd, long double jd0);

/**
  *Return the local time that the object will set
  *@param jd  current Julian date
  *@param geo current geographic location
  */
	QTime setTime( long double jd, const GeoLocation *geo );

//	QTime transitTime( QDateTime currentTime, dms LST );

	dms transitUTTime( long double jd, const dms *gLng );
	QTime transitTime( long double jd, const GeoLocation *geo );

	double approxHourAngle ( const dms *h0, const dms *gLng, const dms *d2 );
	dms gstAtCeroUT ( long double jd );
	dms transitAltitude(long double jd, const GeoLocation *geo);
//	dms transitAltitude(GeoLocation *geo);


	/**
	 * Checks whether a source is circumpolar or not. True = cirmcumpolar
	 * False = Not circumpolar
	 *
	 *@returns true if circumpolar
	 */
	bool checkCircumpolar( const dms *gLng );

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
   QString userLog;

  // optimize stting handling for deep space objects
  bool isCatalogM()    { return bIsCatalogM; }
  bool isCatalogNGC()  { return bIsCatalogNGC; }
  bool isCatalogIC()   { return bIsCatalogIC; }
  bool isCatalogNone() { return bIsCatalogNone; }

private:

/**
  *Returns the UT time when the object will rise or set. It is an auxiliary 
  *procedure because it does not use the RA and DEC of the object but values 
  *given as parameters. You may want to use riseUTTime function which is 
  *public.
  *@param jd  current Julian date
  *@param gLn Geographic longitude
  *@param gLt Geographic latitude
  *@param rga Right ascention of the object
  *@param decl Declination of the object
  *@param rst Boolean. If TRUE will compute rise time. If FALSE 
  *       will compute set time.
  */

	dms auxRiseUTTime( long double jd, const dms *gLng, const dms *gLat, 
				const dms *righta, const dms *decl, bool riseT);
	
/**
  *Returns the LST time when the object will rise or set. It is an auxiliary 
  *procedure because it does not use the RA and DEC of the object but values 
  *given as parameters. You may want to use riseLSTTime function which is 
  *public.
  *@param gLt Geographic latitude
  *@param rga Right ascention of the object
  *@param decl Declination of the object
  *@param rst Boolean. If TRUE will compute rise time. If FALSE 
  *       will compute set time.
  */
	dms auxRiseLSTTime( const dms *gLt, const dms *rga, const dms *decl, bool rst );

  // optimizate string handling
  void calcCatalogFlags();

	QDateTime DMStoQDateTime(long double jd, dms UT);

	dms QTimeToDMS(QTime dT);

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
