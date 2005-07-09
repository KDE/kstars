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

#include <klocale.h>

#include "skypoint.h"
#include "dms.h"
#include "kspopupmenu.h"

/**@class SkyObject
	*Provides all necessary information about an object in the sky:
	*its coordinates, name(s), type, magnitude, and QStringLists of
	*URLs for images and webpages regarding the object.
	*@short Information about an object in the sky.
	*@author Jason Harris
	*@version 1.0
	*/

class QPoint;
class GeoLocation;
class KStarsDateTime;

class SkyObject : public SkyPoint {
public:
/**Constructor.  Set SkyObject data according to arguments.
	*@param t Type of object
	*@param r catalog Right Ascension
	*@param d catalog Declination
	*@param m magnitude (brightness)
	*@param n Primary name
	*@param n2 Secondary name
	*@param lname Long name (common name)
	*/
	SkyObject( int t=TYPE_UNKNOWN, dms r=dms(0.0), dms d=dms(0.0),
						float m=0.0, QString n="", QString n2="", QString lname="" );
/**
	*Constructor.  Set SkyObject data according to arguments.  Differs from
	*above function only in data type of RA and Dec.
	*@param t Type of object
	*@param r catalog Right Ascension
	*@param d catalog Declination
	*@param m magnitude (brightness)
	*@param n Primary name
	*@param n2 Secondary name
	*@param lname Long name (common name)
	*/
	SkyObject( int t, double r, double d, float m=0.0,
						QString n="", QString n2="", QString lname="" );

/**Copy constructor.
	*@param o SkyObject from which to copy data
	*/
	SkyObject( SkyObject &o );

/**
	*Destructor (empty)
	*/
	~SkyObject();

/**@enum TYPE
	*The type classification of the SkyObject.
	*/
	enum TYPE { STAR=0, CATALOG_STAR=1, PLANET=2, OPEN_CLUSTER=3, GLOBULAR_CLUSTER=4,
		GASEOUS_NEBULA=5, PLANETARY_NEBULA=6, SUPERNOVA_REMNANT=7, GALAXY=8, COMET=9,
		ASTEROID=10, CONSTELLATION=11, TYPE_UNKNOWN };

/**@return object's primary name.
	*/
	virtual QString name( void ) const { return hasName() ? *Name : unnamedString;}

/**@return object's primary name, translated to local language.
	*/
	QString translatedName() const { return i18n( name().utf8() );}

/**Set the object's primary name.
	*@param name the object's primary name
	*/
	void setName( const QString &name );

/**@return object's secondary name
	*/
	QString name2( void ) const { return hasName2() ? *Name2 : emptyString; }

/**@return object's secondary name, translated to local language.
	*/
	QString translatedName2() const { return i18n( name2().utf8() );}

/**Set the object's secondary name.
	*@param name2 the object's secondary name.
	*/
	void setName2( const QString &name2="" );

/**@return object's common (long) name
	*/
	virtual QString longname( void ) const { return hasLongName() ? *LongName : unnamedObjectString; }

/**@return object's common (long) name, translated to local language.
	*/
	QString translatedLongName() const { return i18n( longname().utf8() );}

/**Set the object's long name.
	*@param longname the object's long name.
	*/
	void setLongName( const QString &longname="" );

/**@return object's type identifier (int)
	*@see enum TYPE
	*/
	int type( void ) const { return (int)Type; }

/**Set the object's type identifier to the argument.
	*@param t the object's type identifier (e.g., "SkyObject::PLANETARY_NEBULA")
	*@see enum TYPE
	*/
	void setType( int t ) { Type = (unsigned char)t; }

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

/**@return the object's position angle.  This is overridden in KSPlanetBase 
	*and DeepSkyObject; for all other SkyObjects, this returns 0.0.
	*/
	virtual double pa() const { return 0.0; }

/**@return true if the object is a solar system body.
	*/
	bool isSolarSystem() { return ( type() == 2 || type() == 9 || type() == 10 ); }

/**Show Type-specific popup menu.  This is a two-line function that needs to be 
	*overloaded by each subclass of SkyObject, to make sure that the correct popupmenu 
	*function gets called.  By overloading the function, we don't have to check the 
	*object type when we need the menu.
	*/
	virtual void showPopupMenu( KSPopupMenu *pmenu, QPoint pos ) { pmenu->createEmptyMenu( this ); pmenu->popup( pos ); }

/**Determine the time at which the point will rise or set.  Because solar system
	*objects move across the sky, it is necessary to iterate on the solution.
	*We compute the rise/set time for the object's current position, then
	*compute the object's position at that time.  Finally, we recompute then
	*rise/set time for the new coordinates.  Further iteration is not necessary,
	*even for the most swiftly-moving object (the Moon).
	*@return the local time that the object will rise
	*@param dt current UT date/time
	*@param geo current geographic location
	*@param rst If TRUE, compute rise time. If FALSE, compute set time.
	*/
	QTime riseSetTime( const KStarsDateTime &dt, const GeoLocation *geo, bool rst );

/**@return the UT time when the object will rise or set
	*@param dt  target date/time
	*@param geo pointer to Geographic location
	*@param rst Boolean. If TRUE will compute rise time. If FALSE
	*       will compute set time.
	*/
	QTime riseSetTimeUT( const KStarsDateTime &dt, const GeoLocation *geo, bool rst);

/**@return the LST time when the object will rise or set
  *@param dt  target date/time
  *@param geo pointer to Geographic location
  *@param rst Boolean. If TRUE will compute rise time. If FALSE
  *       will compute set time.
  */
	dms riseSetTimeLST( const KStarsDateTime &dt, const GeoLocation *geo, bool rst);

/**@return the Azimuth time when the object will rise or set. This function
  *recomputes set or rise UT times.
  *@param dt  target date/time
  *@param geo GeoLocation object
  *@param rst Boolen. If TRUE will compute rise time. If FALSE
  *       will compute set time.
  */
	dms riseSetTimeAz( const KStarsDateTime &dt, const GeoLocation *geo, bool rst);

/**The same iteration technique described in riseSetTime() is used here.
	*@return the local time that the object will transit the meridian.
	*@param dt  target date/time
	*@param geo pointer to the geographic location
	*/
	QTime transitTime( const KStarsDateTime &dt, const GeoLocation *geo );

/**@return the universal time that the object will transit the meridian.
	*@param dt   target date/time
	*@param geo pointer to the geographic location
	*/
	QTime transitTimeUT( const KStarsDateTime &dt, const GeoLocation *geo );

/**@return the altitude of the object at the moment it transits the meridian.
	*@param dt  target date/time
	*@param geo pointer to the geographic location
	*/
	dms transitAltitude( const KStarsDateTime &dt, const GeoLocation *geo );

/**Check whether a source is circumpolar or not. True = cirmcumpolar
	*False = Not circumpolar
	*@return true if circumpolar
	*/
	bool checkCircumpolar( const dms *gLng );

/**The coordinates for the object on date dt are computed and returned,
	*but the object's internal coordinates are not permanently modified.
	*@return the coordinates of the selected object for the time given by jd
	*@param dt  date/time for which the coords will be computed.
	*@param geo pointer to geographic location (used for solar system only)
	*/
	SkyPoint recomputeCoords( const KStarsDateTime &dt, const GeoLocation *geo=0 );

	const bool hasName() const { return Name != 0; }
	
	const bool hasName2() const { return Name2 != 0; }
	
	const bool hasLongName() const { return LongName != 0; }
	
/**@short Given the Image title from a URL file, try to convert it to an image credit string.
	*/
	QString messageFromTitle( const QString &imageTitle );

/**@short Save new user log text
  */
	void saveUserLog( const QString &newLog );

	QStringList ImageList, ImageTitle;
	QStringList InfoList, InfoTitle;
	QString userLog;

private:

/**Compute the UT time when the object will rise or set. It is an auxiliary
	*procedure because it does not use the RA and DEC of the object but values
	*given as parameters. You may want to use riseSetTimeUT() which is
	*public.  riseSetTimeUT() calls this function iteratively.
	*@param dt     target date/time
	*@param geo    pointer to Geographic location
	*@param righta pointer to Right ascention of the object
	*@param decl   pointer to Declination of the object
	*@param rst    Boolean. If TRUE will compute rise time. If FALSE
	*              will compute set time.
	*@return the time at which the given position will rise or set.
	*/
	QTime auxRiseSetTimeUT( const KStarsDateTime &dt, const GeoLocation *geo,
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
	*and to the size of the body. The moon correction has also to take into
	*account parallax. The value we use here is a rough approximation
	*suggeted by J. Meeus.
	*
	*Weather status (temperature and pressure basically) is not taken
	*into account although change of conditions between summer and 
	*winter could shift the times of sunrise and sunset by 20 seconds.
	*
	*This function is only used by auxRiseSetTimeLST().
	*@return dms object with the correction.
	*/
	dms elevationCorrection(void);

	unsigned char Type;
	float Magnitude;

protected:

	QString *Name, *Name2, *LongName;

	// store often used name strings in static variables
	static QString emptyString;
	static QString unnamedString;
	static QString unnamedObjectString;
	static QString starString;
};

#endif
