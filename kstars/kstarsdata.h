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

#include <qfile.h>
#include <qmap.h>
#include <qptrlist.h>
#include <qstring.h>
#include <qstringlist.h>
#include <qdatetime.h>
#include <qtimer.h>

#include <klocale.h>
#include <kshortcut.h>

#include <iostream>

#include "geolocation.h"
#include "skyobject.h"
#include "starobject.h"
#include "kstarsoptions.h"
#include "ksplanet.h"
#include "ksasteroid.h"
#include "kscomet.h"
#include "kspluto.h"
#include "kssun.h"
#include "ksmoon.h"
#include "skypoint.h"
#include "skyobjectname.h"
#include "dms.h"
#include "simclock.h"
#include "skymap.h"
#include "planetcatalog.h"
#include "objectnamelist.h"
#include "timezonerule.h"
#include "lcgenerator.h"
#include "detaildialog.h"
#include "jupitermoons.h"
#include "indidriver.h"
#include "telescopewizardprocess.h"

#define NHIPFILES 42
#define NMWFILES  11
#define NNGCFILES 13
#define NTYPENAME 11
#define NCIRCLE 360   //number of points used to define equator, ecliptic and horizon

#define MINZOOM 400.
#define MAXZOOM 1000000.
#define DEFAULTZOOM 2000.
#define DZOOM 1.10
#define AU_KM 1.49605e8  //km in one AU

class KStandardDirs;
class FileSource;
class StarDataSink;
class QDataPump;
class elts;
class KSFileReader;

/**KStarsData manages all the data objects used by KStars: Lists of stars, deep-sky objects,
	*planets, geographic locations, and constellations.  Also, the milky way, and URLs for
	*images and information pages.
	*
	*@short KStars Data
	*@author Heiko Evermann
	*@version 0.9
	*/

class KStarsData : public QObject
{
	Q_OBJECT
public:
	//Friend classes can see the private data.
	friend class FindDialog;
	friend class KStars;
	friend class LocationDialog;
	friend class MapCanvas;
	friend class SkyMap;
	friend class FileSource;
	friend class StarDataSink;
	friend class LCGenerator;
	friend class DetailDialog;
	friend class AltVsTime;
	friend class KSPopupMenu;
	friend class WUTDialog;
	friend class ViewOpsDialog;
	friend class INDIDriver;
	friend class INDI_P;
	friend class PlanetViewer;
	friend class JMoonTool;
	friend class telescopeWizardProcess;

	/**Constructor. */
	KStarsData();

	/**Destructor.  Delete data objects. */
  virtual ~KStarsData();

	/**Populate list of geographic locations from "Cities.dat". Also check for custom
		*locations file "mycities.dat", but don't require it.  Each line in the file
		*provides the information required to create one GeoLocation object.
		*@short Fill list of geographic locations from file(s)
		*@returns true if at least one city read successfully.
		*/
	bool readCityData( void );

	/**Read the data file that contains daylight savings time rules.
		*/
	bool readTimeZoneRulebook( void );

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
	bool processCity( QString& line );

	/**Populate list of star objects from the stars database file.
		*Each line in the file provides the information required to construct a
		*SkyObject of type 'star'.  Each line is parsed according to the column
		*position in the line:
		*columns  field
		*0-1      RA hours [int]
		*2-3      RA minutes [int]
		*4-8      RA seconds [float]
		*10       Dec sign [char; '+' or '-']
		*11-12    Dec degrees [int]
		*13-14    Dec minutes [int]
		*15-18    Dec seconds [float]
		*20-28    dRA/dt (milli-arcsec/yr) [float]
		*29-37    dDec/dt (milli-arcsec/yr) [float]
		*38-44    Parallax (milli-arcsec) [float]
		*46-50    Magnitude [float]
		*51-55    B-V color index [float]
		*56-57    Spectral type [string]
		*59       Multiplicity flag (1=true, 0=false) [int]
		*61-64    Variability range of brightness (magnitudes; bank if not variable) [float]
		*66-71    Variability period (days; blank if not variable) [float]
		*72-      Name(s) [string].  This field may be blank.  The string is the common
             name for the star (e.g., "Betelgeuse").  If there is a colon, then
             everything after the colon is the genetive name for the star (e.g.,
             "alpha Orionis").
		*@short read the stars database, constructing the list of SkyObjects that represent the stars.
		*@returns true if the data file was successfully opened and read.
		*/
	bool readStarData( void );

//	void processSAO(QString *line, bool reloadedData=false);
	void processStar(QString *line, bool reloadedData=false);

	/**Populate the list of deep-sky objects from the database file.
		*Each line in the file is parsed according to column position:
		*columns  field
		*0        IC indicator [char]  If 'I' then IC object; if ' ' then NGC object
		*1-4      Catalog number [int]  The NGC/IC catalog ID number
		*6-8      Constellation code (IAU abbreviation)
		*10-11    RA hours [int]
		*13-14    RA minutes [int]
		*16-19    RA seconds [float]
		*21       Dec sign [char; '+' or '-']
		*22-23    Dec degrees [int]
		*25-26    Dec minutes [int]
		*28-29    Dec seconds [int]
		*31       Type ID [int]  Indicates object type; see TypeName array in kstars.cpp
		*33-36    Type details [string] (not yet used)
		*38-41    Magnitude [float] can be blank
		*43-48    Major axis length, in arcmin [float] can be blank
		*50-54    Minor axis length, in arcmin [float] can be blank
		*56-58    Position angle, in degrees [int] can be blank
		*60-62    Messier catalog number [int] can be blank
		*64-69    PGC Catalog number [int] can be blank
		*71-75    UGC Catalog number [int] can be blank
		*77-END   Common name [string] can be blank
		*@short Read the ngcic.dat deep-sky database.
		*@returns true if data file is successfully read.
		*/
	bool readDeepSkyData( void );

	bool readAsteroidData( void );
	bool readCometData( void );

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

    /**Read Variable Stars data and stores them in structure of type VariableStarsInfo.
        *0-8 AAVSO Star Designation
        *10-20 Common star name
		*@short read Variable Stars data.
		*@returns true if data is successfully read.
		*/
    bool readVARData(void);

    //TODO JM: ADV tree should use XML instead
    /**Read Advanced interface structure to be used later to construct the list view in
       *the advanced tab in the Detail Dialog.
       *KSLABEL designates a top-level parent label
       *KSINTERFACE designates a common URL interface for several objects
       *END designates the end of a sub tree structure
		*@short read Advanted interface structure.
		*@returns true if data is successfully read.
		*/
   bool readADVTreeData(void);

    /**Read INDI hosts from an XML file*/
   bool readINDIHosts(void);

       //TODO JM: Use XML instead; The logger should have more features
       // that allow users to enter details about their observation logs
       // objects observed, eye pieces, telescope, conditions, mag..etc
       /**Read user logs. The log file is formatted as following:
       *KSLABEL designates the beginning of a log
       *KSLogEnd designates the end of a log.
		*@short read user logs.
		*@returns true if data is successfully read.
		*/
   bool readUserLog(void);

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

   // Used several times in the code, so why not
   bool openURLFile(QString urlfile, QFile& file);

	/**Read in custom object catalog.  Object data is read from a file, and parsed into a
		*QPtrList of SkyObjects which is returned by reference through the 2nd argument.
		*@param filename The custom catalog data file
		*@param olist the list of skyobjects, returned as a reference through this variable
		*@bool showerrs if true, notify user of unparsed lines.
		*/
	bool readCustomData( QString filename, QPtrList<SkyObject> &olist, bool showerrs );

	/**@short reset the faint limit for the stellar database
		*@param newMagnitude the new faint limit.
		*@param forceReload will force a reload also if newMagnitude <= maxSetMagnitude
		*it's needed to set internal maxSetMagnitude and reload data later; is used in
		*checkDataPumpAction() and should not used outside.
		*/
	void setMagnitude( float newMagnitude, bool forceReload=false );

	/**Add a custom object catalog.  The QString name of the catalog and
		*the QPtrList of SkyObjects comprising the data are given as arguments.
		*This function simply appends the QPtrList as an entry in the
		*CustomCatalogs QMap, with the name string used as the key.
		*/
	void addCatalog( QString name, QPtrList<SkyObject> );

	/**Set the NextDSTChange member.
		*Need this accessor because I could not make KStars::privatedata a friend
		*class for some reason...:/
		*/
	void setNextDSTChange( long double jd ) { NextDSTChange = jd; }

	/**
		*return pointer to KStars options.
		*/
	KStarsOptions *getOptions() const { return options; }

	/**
		*Load KStars options.
		*/
	void loadOptions();

	/**
		*Save KStars options.  Optional KStars parameter for saving
		*certain GUI options.
		*/
	void saveOptions(KStars *ks=0);

	/**Make a backup copy of the KStarsOptions object.
		*This is needed in case the user presses "cancel" after making changes in
		*the ViewOpsDialog. */
	void backupOptions();

	/**Restore the KStarsOptions object from the backup copy.
		*This is needed in case the user presses "cancel" after making changes in
		*the ViewOpsDialog. */
	void restoreOptions();

	/**Returns true if time is running forward else false. Used by KStars to prevent
		*2 calculations of daylight saving change time.
		*/
	bool isTimeRunningForward() { return TimeRunsForward; }

	KLocale *getLocale() { return locale; };

	KSPlanet *earth() { return PC->earth(); }

	/**Find object by name.
		*@param name Object name to find
		*@returns pointer to SkyObject matching this name
		*/
	SkyObject* objectNamed( const QString &name );

	/**The Sky is updated more frequently than the moon, which is updated more frequently
		*than the planets.  The date of the last update for each category is recorded so we
		*know when we need to do it again (see KStars::updateTime()).
		*Initializing these to -1000000.0 ensures they will be updated immediately
		*on the first call to KStars::updateTime().
		*/
	void setFullTimeUpdate();

	/**change the current simulation time to the time and date specified as the arguments.
		*Specified date and time is always local time.
		*@param newDate the date to set.
		*@param newTIme the time to set.
		*/
	void changeTime(QDate newDate, QTime newTime );

	/**Set the LST from the simulation clock's UTC value.
		*/
	void setLST();
	/**Set the LST from the UTC specified as an argument.
		*@param UTC the Universal time from which to set the LST.
		*/
	void setLST( QDateTime UTC );

	void setHourAngle( double ha ) { HourAngle->setH( ha ); }

	//Some members need to be accessed outside of the friend classes (i.e., in the main fcn).
	GeoLocation *geo() { return options->Location(); }
	SimClock *clock() { return Clock; }
	dms *lst() { return LST; }

	bool executeScript( const QString &name, SkyMap *map );

	/**
		*Initialize celestial equator, horizon and ecliptic.
		*/
	void initGuides( KSNumbers *num );

	bool useDefaultOptions, startupComplete;

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

	void saveTimeBoxShaded( bool b ) { options->shadeTimeBox = b; }
	void saveGeoBoxShaded( bool b ) { options->shadeGeoBox = b; }
	void saveFocusBoxShaded( bool b ) { options->shadeFocusBox = b; }
	void saveTimeBoxPos( QPoint p ) { options->posTimeBox = p; }
	void saveGeoBoxPos( QPoint p ) { options->posGeoBox = p; }
	void saveFocusBoxPos( QPoint p ) { options->posFocusBox = p; }

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

/**
	*Checks if data transmission is already running or not.
	*/
	void checkDataPumpAction();

/**
	*Send update signal to refresh skymap.
	*/
	void updateSkymap();

/**
	*Send clearCache signal.
	*/
	void sendClearCache();

private:

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

	KSFileReader *starFileReader;

	static QPtrList<GeoLocation> geoList;
	QPtrList<SkyObject> objList;

	QPtrList<StarObject> starList;

	unsigned int StarCount;

  /** List of all deep sky objects */
	QPtrList<SkyObject> deepSkyList;
  /** List of all deep sky objects per type, to speed up drawing the sky map */
	QPtrList<SkyObject> deepSkyListMessier;
  /** List of all deep sky objects per type, to speed up drawing the sky map */
	QPtrList<SkyObject> deepSkyListNGC;
  /** List of all deep sky objects per type, to speed up drawing the sky map */
	QPtrList<SkyObject> deepSkyListIC;
  /** List of all deep sky objects per type, to speed up drawing the sky map */
	QPtrList<SkyObject> deepSkyListOther;

	QPtrList<KSAsteroid> asteroidList;
	QPtrList<KSComet> cometList;

	QPtrList<SkyPoint> MilkyWay[NMWFILES];

	QPtrList<SkyPoint> clineList;
	QPtrList<QChar> clineModeList;
	QPtrList<SkyObject> cnameList;
	QPtrList<SkyObject> ObjLabelList;

	QPtrList<SkyPoint> Equator;
	QPtrList<SkyPoint> Ecliptic;
	QPtrList<SkyPoint> Horizon;
	QPtrList<VariableStarInfo> VariableStarsList;
	QPtrList<ADVTreeData> ADVtreeList;
	QPtrList<INDIHostsInfo> INDIHostsList;
	ObjectNameList ObjNames;

	QMap<QString, QPtrList<SkyObject> > CustomCatalogs;
	static QMap<QString, TimeZoneRule> Rulebook;

	SimClock *Clock;
	QDateTime LTime, UTime;
	bool TimeRunsForward, temporaryTrail;

	QString cnameFile;
	KStandardDirs *stdDirs;
	KLocale *locale;

	dms *LST, *HourAngle;

	QString TypeName[NTYPENAME];
	KKey resumeKey;

	PlanetCatalog *PC;
	KSMoon *Moon;
	JupiterMoons *jmoons;

	double Obliquity, dObliq, dEcLong;
	long double CurrentDate, LastNumUpdate, LastSkyUpdate, LastPlanetUpdate, LastMoonUpdate;
	long double NextDSTChange;

	// options
	KStarsOptions* options;
	KStarsOptions* oldOptions;

	KStars *kstars; //pointer to the parent widget

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

