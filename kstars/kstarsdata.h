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

#include <qmap.h>
#include <qptrlist.h>
#include <qstring.h>

#include <kshortcut.h>

#include <iostream>

#include "fov.h"
#include "geolocation.h"
#include "colorscheme.h"
#include "objectnamelist.h"
#include "planetcatalog.h"
#include "tools/lcgenerator.h"
#include "kstarsdatetime.h"
#include "simclock.h"

#define NHIPFILES 127
#define NMWFILES  11
#define NNGCFILES 14
#define NTYPENAME 12
#define NCIRCLE 360   //number of points used to define equator, ecliptic and horizon

#define MINZOOM 200.
#define MAXZOOM 1000000.
#define DEFAULTZOOM 2000.
#define DZOOM 1.10
#define AU_KM 1.49605e8  //km in one AU

#define MINDRAWSTARMAG 6.5 // min. magnitude to load all stars which are needed for constellation lines

class QDataPump;
class QFile;
class QTimer;

class KStandardDirs;

class dms;
class SkyMap;
class SkyPoint;
class SkyObject;
class DeepSkyObject;
class StarObject;
class KSPlanet;
class KSAsteroid;
class KSComet;
class KSMoon;
class PlanetCatalog;
class JupiterMoons;

class TimeZoneRule;
class FileSource;
class StarDataSink;
class KSFileReader;
class INDIHostsInfo;
class ADVTreeData;
class CSegment;
class CustomCatalog;

/**@class KStarsData
	*KStarsData manages all the data objects used by KStars: Lists of stars, deep-sky objects,
	*planets, geographic locations, and constellations.  Also, the milky way, and URLs for
	*images and information pages.
	*
	*@author Heiko Evermann
	*@version 1.0
	*/

class KStarsData : public QObject
{
	Q_OBJECT
public:
	//Friend classes can see the private data.
	friend class FindDialog;
	friend class KStars;
	friend class KSWizard;
	friend class LocationDialog;
	friend class FOVDialog;
	friend class MapCanvas;
	friend class SkyMap;
	friend class FileSource;
	friend class StarDataSink;
	friend class LCGenerator;
	friend class DetailDialog;
	friend class AltVsTime;
	friend class KSPopupMenu;
	friend class WUTDialog;
	friend class INDIDriver;
	friend class INDI_P;
	friend class INDIStdProperty;
	friend class PlanetViewer;
	friend class JMoonTool;
	friend class telescopeWizardProcess;
	friend class KSNewStuff;
	friend class ObservingList;
	friend class ObsListWizard;

	/**Constructor. */
	KStarsData();

	/**Destructor.  Delete data objects. */
  virtual ~KStarsData();

	/**Populate list of geographic locations from "Cities.dat". Also check for custom
		*locations file "mycities.dat", but don't require it.  Each line in the file
		*provides the information required to create one GeoLocation object.
		*@short Fill list of geographic locations from file(s)
		*@return true if at least one city read successfully.
		*@see KStarsData::processCity()
		*/
	bool readCityData( void );

	/**Read the data file that contains daylight savings time rules.
		*/
	bool readTimeZoneRulebook( void );

	/**Parse one line from a locations database file.  The line contains 10 or 11 fields
		*separated by colons (":").  The fields are:
		*@li City Name [string]
		*@li Province Name (optional) [string]
		*@li Country Name [string]
		*@li Longitude degrees [int]
		*@li Latitude arcminutes [int]
		*@li Latitude arcseconds [int]
		*@li Latitude sign [char; 'E' or 'W' ]
		*@li Latitude degrees [int]
		*@li Latitude arcminutes [int]
		*@li Latitude arcseconds [int]
		*@li Latitude sign [char; 'N' or 'S' ]
		*@li Timezone [float; -12 <= TZ <= 12, or 'x' if TZ unknown]
		*
		*@short Parse one line from a geographic database
		*@param line The line from the geographic database to be parsed
		*@return true if location successfully parsed; otherwise false.
		*@see KStarsData::readCityData()
		*/
	bool processCity( QString& line );

	/**Populate list of star objects from the stars database file.
		*Each line in the file provides the information required to construct a
		*SkyObject of type 'star'.  
		*@short read the stars database, constructing the list of SkyObjects that represent the stars.
		*@return true if the data file was successfully opened and read.
		*@see KStarsData::processStar()
		*/
	bool readStarData( void );

	/**Parse a line from a stars data file, constructing a StarObject from the data.
		*The StarObject is added to the list of stars.
		*
		*Each line is parsed according to the column
		*position in the line:
		*@li 0-1      RA hours [int]
		*@li 2-3      RA minutes [int]
		*@li 4-8      RA seconds [float]
		*@li 10       Dec sign [char; '+' or '-']
		*@li 11-12    Dec degrees [int]
		*@li 13-14    Dec minutes [int]
		*@li 15-18    Dec seconds [float]
		*@li 20-28    dRA/dt (milli-arcsec/yr) [float]
		*@li 29-37    dDec/dt (milli-arcsec/yr) [float]
		*@li 38-44    Parallax (milli-arcsec) [float]
		*@li 46-50    Magnitude [float]
		*@li 51-55    B-V color index [float]
		*@li 56-57    Spectral type [string]
		*@li 59       Multiplicity flag (1=true, 0=false) [int]
		*@li 61-64    Variability range of brightness (magnitudes; bank if not variable) [float]
		*@li 66-71    Variability period (days; blank if not variable) [float]
		*@li 72-END   Name(s) [string].  This field may be blank.  The string is the common
		*             name for the star (e.g., "Betelgeuse").  If there is a colon, then
		*             everything after the colon is the genetive name for the star (e.g.,
		*             "alpha Orionis").
		*
		*@param line pointer to the line of data to be processed as a StarObject
		*@param reloadMode makes additional calculations in reload mode, not needed at start up
		*@see KStarsData::readStarData()
		*/
	void processStar( QString *line, bool reloadMode = false );

	/**Populate the list of deep-sky objects from the database file.
		*Each line in the file is parsed according to column position:
		*@li 0        IC indicator [char]  If 'I' then IC object; if ' ' then NGC object
		*@li 1-4      Catalog number [int]  The NGC/IC catalog ID number
		*@li 6-8      Constellation code (IAU abbreviation)
		*@li 10-11    RA hours [int]
		*@li 13-14    RA minutes [int]
		*@li 16-19    RA seconds [float]
		*@li 21       Dec sign [char; '+' or '-']
		*@li 22-23    Dec degrees [int]
		*@li 25-26    Dec minutes [int]
		*@li 28-29    Dec seconds [int]
		*@li 31       Type ID [int]  Indicates object type; see TypeName array in kstars.cpp
		*@li 33-36    Type details [string] (not yet used)
		*@li 38-41    Magnitude [float] can be blank
		*@li 43-48    Major axis length, in arcmin [float] can be blank
		*@li 50-54    Minor axis length, in arcmin [float] can be blank
		*@li 56-58    Position angle, in degrees [int] can be blank
		*@li 60-62    Messier catalog number [int] can be blank
		*@li 64-69    PGC Catalog number [int] can be blank
		*@li 71-75    UGC Catalog number [int] can be blank
		*@li 77-END   Common name [string] can be blank
		*@short Read the ngcic.dat deep-sky database.
		*@return true if data file is successfully read.
		*/
	bool readDeepSkyData( void );

	/**Populate the list of Asteroids from the data file.
		*Each line in the data file is parsed as follows:
		*@li 6-23 Name [string]
		*@li 24-29 Modified Julian Day of orbital elements [int]
		*@li 30-39 semi-major axis of orbit in AU [double]
		*@li 41-51 eccentricity of orbit [double]
		*@li 52-61 inclination angle of orbit in degrees [double]
		*@li 62-71 argument of perihelion in degrees [double]
		*@li 72-81 Longitude of the Ascending Node in degrees [double]
		*@li 82-93 Mean Anomaly in degrees [double]
		*@li 94-98 Magnitude [double]
		*/
	bool readAsteroidData( void );

	/**Populate the list of Comets from the data file.
		*Each line in the data file is parsed as follows:
		*@li 3-37 Name [string]
		*@li 38-42 Modified Julian Day of orbital elements [double]
		*@li 44-54 Perihelion distance in AU [double]
		*@li 55-65 Eccentricity of orbit [double]
		*@li 66-75 inclination of orbit in degrees [double]
		*@li 76-85 argument of perihelion in degrees [double]
		*@li 86-95 longitude of the ascending node in degrees[double]
		*@li 96-110 Period of orbit in years [double]
		*/
	bool readCometData( void );

	/**Read in Constellation line data.  The constellations are represented as a list of
		*SkyPoints and an associated list of chars that indicates whether to draw a line
		*between points (i) and (i+1) or to simply move to point (i+1). The lines are parsed
		*according to column position:
		*@li 0-1      RA hours [int]
		*@li 2-3      RA minutes [int]
		*@li 4-5      RA seconds [int]
		*@li 6        Dec sign [char; '+' or '-']
		*@li 7-9      Dec degrees [int]
		*@li 10-11    Dec minutes [int]
		*@li 12-13    Dec seconds [int]
		*@li 14       draw indicator [char; 'D' or 'M']  'D'==draw line;  'M'==just move
		*
		*@short Read in constellation line data.
		*@return true if data file was successfully read
		*/
	bool readCLineData( void );

	/**Read constellation names.  The coordinates are where the constellation name text
		*will be centered.  The positions are imprecise, but that's okay since
		*constellations are so large.  The lines are parsed according to column position:
		*@li 0-1     RA hours [int]
		*@li 2-3     RA minutes [int]
		*@li 4-5     RA seconds [int]
		*@li 6       Dec sign [char; '+' or '-']
		*@li 7-8     Dec degrees [int]
		*@li 9-10    Dec minutes [int]
		*@li 11-12   Dec seconds [int]
		*@li 13-15   IAU Abbreviation [string]  e.g., 'Ori' == Orion
		*@li 17-     Constellation name [string]
		*@short Read in constellation name data.
		*@return TRUE if data file was successfully read.
		*/
	bool readCNameData( void );

	/**Read constellation boundary data.  The boundary data is defined by a series of 
		*RA,Dec coordinate pairs defining the "nodes" of the boundaries.  The nodes are 
		*organized into "segments", such that each segment represents a continuous series 
		*of boundary-line intervals that divide two particular constellations.
		*
		*The definition of a segment begins with an integer describing the number of Nodes
		*in the segment.  This is followed by that number of RA,Dec pairs (RA in hours, 
		*Dec in degrees).  Finally, there is an integer indicating the number of 
		*constellations bordered by this segment (this number is always 2), followed by
		*the IAU abbreviations of the two constellations.
		*
		*Since the definition of a segment can span multiple lines, we parse this file 
		*word-by-word, rather than line-by-line as we do in other files.
		*
		*@short Read in the constellation boundary data.
		*@return TRUE if the boundary data is successfully parsed.
		*/
	bool readCBoundData( void );

	/**Read Milky Way data.  Coordinates for the Milky Way contour are divided into 11
		*files, each representing a simple closed curve that can be drawn with
		*drawPolygon().  The lines in each file are parsed according to column position:
		*@li 0-7     RA [float]
		*@li 9-16    Dec [float]
		*@short read Milky Way contour data.
		*@return true if all MW files were successfully read
		*/
	bool readMWData( void );

	/**Read Variable Stars data and stores them in structure of type VariableStarsInfo.
		*@li 0-8 AAVSO Star Designation
		*@li 10-20 Common star name
		*@short read Variable Stars data.
		*@return true if data is successfully read.
		*/
	bool readVARData(void);

	//TODO JM: ADV tree should use XML instead
	/**Read Advanced interface structure to be used later to construct the list view in
		*the advanced tab in the Detail Dialog.
		*@li KSLABEL designates a top-level parent label
		*@li KSINTERFACE designates a common URL interface for several objects
		*@li END designates the end of a sub tree structure
		*@short read Advanted interface structure.
		*@return true if data is successfully read.
		*/
	bool readADVTreeData(void);

	/**Read INDI hosts from an XML file*/
	bool readINDIHosts(void);

	//TODO JM: Use XML instead; The logger should have more features
	// that allow users to enter details about their observation logs
	// objects observed, eye pieces, telescope, conditions, mag..etc
	/**Read user logs. The log file is formatted as following:
		*@li KSLABEL designates the beginning of a log
		*@li KSLogEnd designates the end of a log.
		*@short read user logs.
		*@return true if data is successfully read.
		*/
   bool readUserLog(void);

	/**Read in URLs to be attached to a named object's right-click popup menu.  At this
		*point, there is no way to attach URLs to unnamed objects.  There are two
		*kinds of URLs, each with its own data file: image links and webpage links.  In addition,
		*there may be user-specific versions with custom URLs.  Each line contains 3 fields
		*separated by colons (":").  Note that the last field is the URL, and as such it will
		*generally contain a colon itself.  Only the first two colons encountered are treated
		*as field separators.  The fields are:
		*@li Object name.  This must be the "primary" name of the object (the name at the top of the popup menu).
		*@li Menu text.  The string that should appear in the popup menu to activate the link.
		*@li URL.
		*@short Read in image and information URLs.
		*@return true if data files were successfully read.
		*/
	bool readURLData( QString url, int type=0, bool deepOnly=false );

	/**@short open a file containing URL links.
		*@param urlfile string representation of the filename to open
		*@param file reference to the QFile object which will be opened to this file.
		*@return TRUE if file successfully opened. 
		*/
	bool openURLFile(QString urlfile, QFile& file);

	/**Initialize custom object catalogs from the files listed in the config file
		*/
	bool readCustomCatalogs();

	/**Add a user-defined object catalog to the list of custom catalogs.
		*(Basically just calls createCustomCatalog() )
		*/
	bool addCatalog( QString filename );

	/**Remove a user-defined object catalog from the list of custom catalogs.
		*Also removes the objects from the ObjNames list.
		*@param i the index identifier of the catalog to be removed
		*/
	bool removeCatalog( int i );

	/**Read in and parse a custom object catalog.  Object data are read from a file, and 
		*parsed into a CustomCatalog object.
		*@param filename name of the custom catalog file
		*@param showerrs show GUI window summarizing parsing errors
		*@return pointer to the new catalog
		*/
	CustomCatalog* createCustomCatalog( QString filename, bool showerrs = false );

	/**@short Parse the header of the custom object catalog.
		*@param lines string list containing the lines from the custom catalog file
		*@param Columns reference to list of descriptors of the data columns
		*@param catName reference to the name of the catalog (read from header)
		*@param catPrefix reference to the prefix for ID-number-based names (read from header)
		*@param catColor reference to the color for the object symbols (read from header)
		*@param catEpoch reference to the coordinate epoch of the catalog (read from header)
		*@param iStart reference to the line number of the first data line (following the header)
		*@param showerrs if true, notify user of problems parsing the header.
		*@param errs reference to the cumulative list of error reports
		*/
	bool parseCustomDataHeader( QStringList lines, QStringList &Columns, 
			QString &catName, QString &catPrefix, QString &catColor, float &catEpoch, int &iStart, 
			bool showerrs, QStringList &errs );

	/**@short Parse a line from custom object catalog.  If parsing is successful, add
		*the object to the object list
		*@param num the line number being processed
		*@param d list of fields in the line
		*@param Columns the list of field descriptors (read from the header)
		*@param Prefix the string prefix to be prepended to ID numbers (read from the header)
		*@param objList reference to the list of SkyObjects in the catalog
		*@param showerrs if true, notify user of problems parsing the header.
		*@param errs reference to the cumulative list of error reports
		*/
	bool processCustomDataLine( int num, QStringList d, QStringList Columns, 
			QString Prefix, QPtrList<SkyObject> &objList, bool showerrs, QStringList &errs );

	/**@short reset the faint limit for the stellar database
		*@param newMagnitude the new faint limit.
		*@param forceReload will force a reload also if newMagnitude <= maxSetMagnitude
		*it's needed to set internal maxSetMagnitude and reload data later; is used in
		*checkDataPumpAction() and should not used outside.
		*/
	void setMagnitude( float newMagnitude, bool forceReload=false );

	/**Set the NextDSTChange member.
		*Need this accessor because I could not make KStars::privatedata a friend
		*class for some reason...:/
		*/
	void setNextDSTChange( const KStarsDateTime &dt ) { NextDSTChange = dt; }

	/**Returns true if time is running forward else false. Used by KStars to prevent
		*2 calculations of daylight saving change time.
		*/
	bool isTimeRunningForward() { return TimeRunsForward; }

	/**@return pointer to the localization (KLocale) object
		*/
	KLocale *getLocale() { return locale; }

	/**@return pointer to the Earth object
		*/
	KSPlanet *earth() { return PCat->earth(); }

	/**@short Find object by name.
		*@param name Object name to find
		*@return pointer to SkyObject matching this name
		*/
	SkyObject* objectNamed( const QString &name );

	/**The Sky is updated more frequently than the moon, which is updated more frequently
		*than the planets.  The date of the last update for each category is recorded so we
		*know when we need to do it again (see KStars::updateTime()).
		*Initializing these to -1000000.0 ensures they will be updated immediately
		*on the first call to KStars::updateTime().
		*/
	void setFullTimeUpdate();

	/**change the current simulation date/time to the KStarsDateTime argument.
		*Specified DateTime is always universal time.
		*@param newDate the DateTime to set.
		*/
	void changeDateTime( const KStarsDateTime &newDate );

	/**@return pointer to the current simulation local time
		*/
	const KStarsDateTime& lt() const { return LTime; }
	
	/**@return reference to the current simulation universal time
		*/
	const KStarsDateTime& ut() const { return Clock.utc(); }
	
	/**Sync the LST with the simulation clock.
		*/
	void syncLST();
	
	/**Set the HourAngle member variable according to the argument.
		*@param ha The new HourAngle
		*/
	void setHourAngle( double ha ) { HourAngle->setH( ha ); }

	//Some members need to be accessed outside of the friend classes (i.e., in the main fcn).

	/**@return pointer to the ColorScheme object
		*/
	ColorScheme *colorScheme() { return &CScheme; }
	
	/**@return pointer to the simulation Clock object
		*/
	SimClock *clock() { return &Clock; }
	
	/**@return pointer to the local sidereal time: a dms object
		*/
	dms *lst() { return LST; }
	
	/**@return pointer to the GeoLocation object*/
	GeoLocation *geo() { return &Geo; }
	
	/**@return reference to the CustomCatalogs list
		*/
	QPtrList<CustomCatalog>& customCatalogs() { return CustomCatalogs; }
 
	/**Set the GeoLocation according to the argument.
		*@param l reference to the new GeoLocation
		*/
	void setLocation( const GeoLocation &l );
	
	/**Set the GeoLocation according to the values stored in the configuration file.
		*/
	void setLocationFromOptions();

	/**@return whether the next Focus change will omit the slewing animation.
		*/
	bool snapNextFocus() const { return snapToFocus; }
	
	/**Disable or re-enable the slewing animation for the next Focus change.
		*@note If the user has turned off all animated slewing, setSnapNextFocus(false)
		*will *NOT* enable animation on the next slew.  A false argument would only
		*be used if you have previously called setSnapNextFocus(true), but then decided 
		*you didn't want that after all.  In other words, it's extremely unlikely you'd
		*ever want to use setSnapNextFocus(false).
		*@param b when TRUE (the default), the next Focus chnage will omit the slewing 
		*animation. 
		*/
	void setSnapNextFocus(bool b=true) { snapToFocus = b; }

	/**Execute a script.  This function actually duplicates the DCOP functionality 
		*for those cases when invoking DCOP is not practical (i.e., when preparing 
		*a sky image in command-line dump mode).
		*@param name the filename of the script to "execute".
		*@param map pointer to the SkyMap object.
		*@return TRUE if the script was successfully parsed.
		*/
	bool executeScript( const QString &name, SkyMap *map );

	/**@short Initialize celestial equator, horizon and ecliptic.
		*@param num pointer to a KSNumbers object to use.
		*/
	void initGuides( KSNumbers *num );

	bool useDefaultOptions, startupComplete;

	/**@short Appends telescope sky object to the list of INDI telescope objects. This enables KStars to track all telescopes properly.
		*@param object pointer to telescope sky object
	*/
	void appendTelescopeObject(SkyObject * object);

signals:
	/**Signal that specifies the text that should be drawn in the KStarsSplash window.
		*/
	void progressText(QString);

	/**Signal that the Data initialization has finished.
		*/
	void initFinished(bool);

/**
	*Should be used to refresh skymap.
	*/
	void update();

/**
	*If data changed, emit clearCache signal.
	*/
	void clearCache();

public slots:

	/**Create a timer and connect its timeout() signal to slotInitialize(). */
	void initialize();

	/**@short send a message to the console*/
	void slotConsoleMessage( QString s ) { std::cout << s.utf8() << std::endl; }

	/**Update the Simulation Clock.  Update positions of Planets.  Update
		*Alt/Az coordinates of objects.  Update precession.  Update Focus position.
		*Draw new Skymap.
		*
		* This is ugly.
		* It _will_ change!
		*(JH:)hey, it's much less ugly now...can we lose the comment yet? :p
		*/
	void updateTime(GeoLocation *geo, SkyMap * skymap, const bool automaticDSTchange = true);

	/**Sets the direction of time and stores it in bool TimeRunForwards. If scale >= 0
		*time is running forward else time runs backward. We need this to calculate just
		*one daylight saving change time (previous or next DST change).
		*/
	void setTimeDirection( float scale );

	/**@short Save the shaded state of the Time infobox.
		*@param b TRUE if the box is shaded
		*/
	void saveTimeBoxShaded( bool b );

	/**@short Save the shaded state of the Geo infobox.
		*@param b TRUE if the box is shaded
		*/
	void saveGeoBoxShaded( bool b );

	/**@short Save the shaded state of the Focus infobox.
		*@param b TRUE if the box is shaded
		*/
	void saveFocusBoxShaded( bool b );

	/**@short Save the screen position of the Time infobox.
		*@param p the position of the box
		*/
	void saveTimeBoxPos( QPoint p );

	/**@short Save the screen position of the Time infobox.
		*@param p the position of the box
		*/
	void saveGeoBoxPos( QPoint p );

	/**@short Save the screen position of the Time infobox.
		*@param p the position of the box
		*/
	void saveFocusBoxPos( QPoint p );

private slots:
	/**This function runs while the splash screen is displayed as KStars is
		*starting up.  It is connected to the timeout() signal of a timer
		*created in the initialize() slot.  It consists of a large switch
		*statement, in which each case causes the next data object to be
		*initialized (which usually consists of reading data from a file on disk,
		*and storing it in the appropriate object in memory).
		*At the end of this function, the integer which the switch statement is
		*checking is incremented, so that the next case label will be executed when
		*the next timeout() signal is fired.
		*/
	void slotInitialize();

/**Checks if data transmission is already running or not.
	*/
	void checkDataPumpAction();

/**Send update signal to refresh skymap.
	*/
	void updateSkymap();

/**Send clearCache signal.
	*/
	void sendClearCache();

private:

/**Display an Error messagebox if a data file could not be opened.  If the file
	*was marked as "required", then abort the program when the messagebox is closed.
	*Otherwise, continue loading the program.
	*@param fn the name of the file which could not be opened.
	*@param required if TRUE, then the error message is more severe, and the program 
	*exits when the messagebox is closed.
	*/
	void initError(QString fn, bool required);

/**Reset local time to new daylight saving time. Use this function if DST has changed.
	*Used by updateTime().
	*/
	void resetToNewDST(const GeoLocation *geo, const bool automaticDSTchange);

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

	bool reloadingData();  // is currently reloading of data in progress

/*	bool openSAOFile(int i);*/
	bool openStarFile(int i);

	static QPtrList<GeoLocation> geoList;
	QPtrList<SkyObject> objList;

	QPtrList<StarObject> starList;

	unsigned int StarCount;

  /** List of all deep sky objects */
	QPtrList<DeepSkyObject> deepSkyList;
  /** List of all deep sky objects per type, to speed up drawing the sky map */
	QPtrList<DeepSkyObject> deepSkyListMessier;
  /** List of all deep sky objects per type, to speed up drawing the sky map */
	QPtrList<DeepSkyObject> deepSkyListNGC;
  /** List of all deep sky objects per type, to speed up drawing the sky map */
	QPtrList<DeepSkyObject> deepSkyListIC;
  /** List of all deep sky objects per type, to speed up drawing the sky map */
	QPtrList<DeepSkyObject> deepSkyListOther;

	QPtrList<KSAsteroid> asteroidList;
	QPtrList<KSComet> cometList;

	QPtrList<SkyPoint> MilkyWay[NMWFILES];

	QPtrList<SkyPoint> clineList;
	QPtrList<CSegment> csegmentList;
	QPtrList<QChar> clineModeList;
	QPtrList<SkyObject> cnameList;
	QPtrList<SkyObject> ObjLabelList;

	QPtrList<SkyPoint> Equator;
	QPtrList<SkyPoint> Ecliptic;
	QPtrList<SkyPoint> Horizon;
	QPtrList<VariableStarInfo> VariableStarsList;
	QPtrList<ADVTreeData> ADVtreeList;
	QPtrList<INDIHostsInfo> INDIHostsList;
	QPtrList<SkyObject> INDITelescopeList;
	
	QPtrList<CustomCatalog> CustomCatalogs;

	ObjectNameList ObjNames;

	static QMap<QString, TimeZoneRule> Rulebook;
	static QStringList CustomColumns;
	
	GeoLocation Geo;
	SimClock Clock;
	ColorScheme CScheme;

	KStarsDateTime LTime;

	bool TimeRunsForward, temporaryTrail, snapToFocus;

	QString cnameFile;
	KStandardDirs *stdDirs;
	KLocale *locale;

	dms *LST, *HourAngle;

	QString TypeName[NTYPENAME];
	KKey resumeKey;

	PlanetCatalog *PCat;
	KSMoon *Moon;
	JupiterMoons *jmoons;

	KSFileReader *starFileReader;

	FOV fovSymbol;

	double Obliquity, dObliq, dEcLong;
	KStarsDateTime LastNumUpdate, LastSkyUpdate, LastPlanetUpdate, LastMoonUpdate;
	KStarsDateTime NextDSTChange;
	KStarsDateTime StoredDate;

	QTimer *initTimer;
	int initCounter;

/**
	*Reloading of star data asynchronous:
	*QDataPump connects FileSource and StarDataSink and starts data transmission.
	*/
	FileSource *source;
	StarDataSink *loader;
	QDataPump *pump;

/**
	*Count the number of KStarsData objects.
	*/
	static int objects;
};


#endif // KSTARSDATA_H

