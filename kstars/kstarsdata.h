/***************************************************************************
                          kstarsdata.h  -  K Desktop Planetarium
                             -------------------
    begin                : Sun Jul 29 2001
    copyright            : (C) 2001 by Heiko Evermann
    email                : heiko@evermann.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef KSTARSDATA_H
#define KSTARSDATA_H

//#include <qptrlist.h>
#include <qfile.h>
#include <qstring.h>
#include <qstringlist.h>
#include <qdatetime.h>
#include <kstandarddirs.h>
#include <klocale.h>

#include "geolocation.h"
#include "skyobject.h"
#include "starobject.h"
#include "kstarsoptions.h"
#include "ksplanet.h"
#include "kspluto.h"
#include "kssun.h"
#include "ksmoon.h"
#include "skypoint.h"
#include "skyobjectname.h"
#include "dms.h"

/**KStarsData manages all the data objects used by KStars: Lists of stars, deep-sky objects,
	*planets, geographic locations, and constellations.  Also, the milky way, and URLs for
	*images and information pages.
	*
	*@short KStars Data
	*@author Heiko Evermann
	*@version 0.9
	*/

class KStarsData
{
public:
	//Friend classes can see the private data.
	friend class FindDialog;
	friend class KStars;
	friend class KStarsSplash;
	friend class LocationDialog;
	friend class MapCanvas;
	friend class SkyMap;

	/**Constructor. */
	KStarsData();

	/**Destructor.  Delete data objects. */
  virtual ~KStarsData();

	/**
		*Attempt to open the data file named filename, using the QFile object "file".
		*First look in the standard KDE directories, then look in a local "data"
		*subdirectory.  If the data file cannot be found or opened, display a warning
		*messagebox.
		*@short Open a data file.
		*@param &file The QFile object to be opened
		*@param filename The name of the data file.
		*@param doWarn If true, show a warning on failure (default is true).
		*@param required If true, the warning message indicates that KStars can't function without the file.
		*@returns bool Returns true if data file was opened successfully.
		*/
	static bool openDataFile( QFile &file, QString filename );

	/**Populate list of geographic locations from "Cities.dat". Also check for custom
		*locations file "mycities.dat", but don't require it.  Each line in the file
		*provides the information required to create one GeoLocation object.
		*@short Fill list of geographic locations from file(s)
		*@returns bool Returns true if at least one city read successfully.
		*/
	bool readCityData( void );

	/**Parse one line from a locations database file.  The line contains 10 or 11 fields
		*separated by colons (":").  The fields are:
		*City Name [string]
		*Province Name (optional) [string]
		*Country Name [string]
		*Longitude degrees [int]
		*Latitude arcminutes [int]
		*Latitude arcseconds [int]
		*Latitude sign [char; 'E' or 'W' ]
		*Latitude degrees [int]
		*Latitude arcminutes [int]
		*Latitude arcseconds [int]
		*Latitude sign [char; 'N' or 'S' ]
		*Timezone [float; -12 <= TZ <= 12, or 'x' if TZ unknown]
		*@short Parse one line from a geographic database
		*@param line The line from the geographic database to be parsed
		*@returns true if location successfully parsed; otherwise false.
		*/
	bool processCity( QString line );

	/**Populate list of star objects from the stars database file.
		*Each line in the file provides the information required to construct a
		*SkyObject of type 'star'.  Each line is parsed according to the column
		*position in the line:
		*columns  field
		*0-1      RA hours [int]
		*2-3      RA minutes [int]
		*4-9      RA seconds [float]
		*10-16    dRA/dt (we don't read this field yet)
		*17       Dec sign [char; '+' or '-']
		*18-19    Dec degrees [int]
		*20-21    Dec minutes [int]
		*22-26    Dec seconds [float]
		*27-32    dDec/dt (we don't read this field yet)
		*33-36    magnitude [float]
		*37-39    Spectral type [string]
		*40-      Name(s) [string].  This field may be blank.  The string is the common
             name for the star (e.g., "Betelgeuse").  If there is a colon, then
             everything after the colon is the genetive name for the star (e.g.,
             "alpha Orionis").
		*@short read the stars database, constructing the list of SkyObjects that represent the stars.
		*@returns true if the data file was successfully opened and read.
		*/
	bool readStarData( void );

	/**Populate the list of Messier and NGC objects from the database file.  Each object
		*in the database is in either the NGC or IC catalogs.  Note the lower precision in
		*the positions of these objects.  Each line in the file is parsed
		*according to column position:
		*columns  field
		*0        IC indicator [char]  If 'I' then IC object; if ' ' then NGC object
		*1-4      Catalog number [int]  The catalog ID number
		*5-6      Type ID [int]  Indicates object type; see TypeName array in kstars.cpp
		*8-9      RA hours [int]
		*11-14    RA minutes [float]
		*16       Dec sign [char; '+' or '-']
		*17-18    Dec degrees [int]
		*20-21    Dec minutes [int]
		*23-26    Magnitude [float] can be blank
		*28       Messier indicator [char]  If 'M' then this is a Messier object
		*30-32    Messier catalog number [int]  The Messier catalog ID number
		*34-      Common name [string]
		*@short Read the ngc2000 database.
		*@returns true if data file is successfully read.
		*/
	bool readNGCData( void );

	/**Read in Constellation line data.  The constellations are represented as a list of
		*SkyPoints and an associated list of chars that indicates whether to draw a line
		*between points (i) and (i+1) or to simply move to point (i+1). The lines are parsed
		*according to column position:
		*columns  field
		*0-1      RA hours [int]
		*2-3      RA minutes [int]
		*4-5      RA seconds [int]
		*6        Dec sign [char; '+' or '-']
		*7-9      Dec degrees [int]
		*10-11    Dec minutes [int]
		*12-13    Dec seconds [int]
		*14       draw indicator [char; 'D' or 'M']  'D'==draw line;  'M'==just move
		*@short Read in constellation line data.
		*@returns true if data file was successfully read
		*/
	bool readCLineData( void );

	/**Read constellation names.  The coordinates are where the constellation name text
		*will be centered.  The positions are imprecise, but that's okay since
		*constellations are so large.  The lines are parsed according to column position:
		*column  field
		*0-1     RA hours [int]
		*2-3     RA minutes [int]
		*4-5     RA seconds [int]
		*6       Dec sign [char; '+' or '-']
		*7-8     Dec degrees [int]
		*9-10    Dec minutes [int]
		*11-12   Dec seconds [int]
		*13-15   IAU Abbreviation [string]  e.g., 'Ori' == Orion
		*17-     Constellation name [string]
		*@short Read in constellation name data.
		*@returns true if data file was successfully read.
		*/
	bool readCNameData( void );

	/**Read Milky Way data.  Coordinates for the Milky Way contour are divided into 11
		*files, each representing a simple closed curve that can be drawn with
		*drawPolygon().  The lines in each file are parsed according to column position:
		*column  field
		*0-7     RA [float]
		*9-16    Dec [float]
		*@short read Milky Way contour data.
		*@returns true if all MW files were successfully read
		*/
	bool readMWData( void );

	/**Read in URLs to be attached to a named object's right-click popup menu.  At this
		*point, there is no way to attach URLs to unnamed objects.  There are two
		*kinds of URLs, each with its own data file: image links and webpage links.  In addition,
		*there may be user-specific versions with custom URLs.  Each line contains 3 fields
		*separated by colons (":").  Note that the last field is the URL, and as such it will
		*generally contain a colon itself.  Only the first two colons encountered are treated
		*as field separators.  The fields are:
		*1. Object name.  This must be the "primary" name of the object (the name at the top of the popup menu).
		*2. Menu text.  The string that should appear in the popup menu to activate the link.
		*3. URL.
		*@short Read in image and information URLs.
		*@returns true if data files were successfully read.
		*/
	bool readURLData( QString url, int type=0 );

	/**@short Determine the Julian Date of a given QDateTime.
		*@param t the calendar date/time for which to determine the Julian date.
		*@returns the Julian date as a long double (which gives sub-second precision)
		*/
	long double getJD( QDateTime t);

	/**@short Determine the date and time of a given Julian Date.
		*@param jd the Julian Date for which to determine the calendar date and time.
		*@param the calendar date, returned as a reference.
		*@param the calendar time, returned as a reference.
		*/
//	void getDateTime( long double jd, QDate &date, QTime &time );
	QDateTime getDateTime( long double jd );

	/**@short reset the faint limit for the stellar database
		*@param newMagnitude the new faint limit.
		*/
	void setMagnitude( float newMagnitude );
	
	void saveOptions();
	void restoreOptions();

private:
/*
	* Store the highest magnitude level at the current session and compare with current used
	* magnitude. If current magnitude is equal to maxSetMagnitude reload data on next increment
	* of magnitude level.
	*/	
	float maxSetMagnitude;
/*
	* Store the last position in star data file. Needed by reloading star data.
	*/
	int lastFileIndex;
/**Reload star data up to new limiting magnitude.
	*@short Function to load new star data.
	*@param newMag new faint limit for stars
	*@returns true if data file was successfully read.
	*/
	bool reloadStarData( float newMag );

/*
	* Needed to lock reloadStarData() because asynchronous loading of data
	* will cause problems sometimes if it is not locked. Especially if magnitude
	* level will changed while loading data.
	*/
	bool reloadingInProgress;

	QList<GeoLocation> geoList;
	QList<SkyObject> *objList;
	QList<StarObject> starList;
	QList<SkyObject> messList;
	QList<SkyObject> ngcList;
	QList<SkyObject> icList;
	QList<SkyPoint> clineList;
	QList<QChar> clineModeList;
	QList<SkyObject> cnameList;
	QList<SkyPoint> Equator;
	QList<SkyPoint> Ecliptic;
	QList<SkyPoint> Horizon;
	QList<SkyPoint> MilkyWay[11];
	QList < SkyObjectName > *ObjNames;

	QString cnameFile;  	
	KStandardDirs *stdDirs;

	QDateTime LTime, UTime;
  QTime     LST;
	dms       LSTh, HourAngle;
	int ZoomLevel;
	KLocale *locale;

	QString TypeName[10];

	KSSun *Sun;
	KSMoon *Moon;
	KSPlanet *Mercury, *Venus, *Earth, *Mars, *Jupiter;
	KSPlanet *Saturn, *Uranus, *Neptune;
	KSPluto *Pluto;

	double Obliquity, dObliq, dEcLong;
	long double CurrentEpoch, CurrentDate, LastSkyUpdate, LastPlanetUpdate, LastMoonUpdate;
	long double SkyJD_Mark;
	long double SysJD, SysJD_Mark, SysJD_Elapsed;
	int starCount1, starCount2, starCount3, starCount0;
	
	// options
	KStarsOptions* options;
	KStarsOptions* oldOptions;

};


#endif // KSTARSDATA_H

