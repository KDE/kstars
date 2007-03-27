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
 
#ifndef KSTARSDATA_H_
#define KSTARSDATA_H_

#include <iostream>

#include <QList>
#include <QMap>
#include <QKeySequence>

#include "fov.h"
#include "geolocation.h"
#include "colorscheme.h"
#include "kstarsdatetime.h"
#include "simclock.h"
#include "skycomponents/skymapcomposite.h"

#define MINZOOM 200.
#define MAXZOOM 1000000.
#define DEFAULTZOOM 2000.
#define DZOOM 1.10
#define AU_KM 1.49605e8  //km in one AU

#define MINDRAWSTARMAG 6.5 // min. magnitude to load all stars which are needed for constellation lines

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
class KSFileReader;
class INDIHostsInfo;
class ADVTreeData;
class CSegment;
class CustomCatalog;

struct VariableStarInfo
{
	QString Name;
	QString Designation;
};

/**@class KStarsData
	*KStarsData is the backbone of KStars.  It contains all the data used by KStars, 
	*including the SkyMapComposite that contains all items in the skymap 
	*(stars, deep-sky objects, planets, constellations, etc).  Other kinds of data 
	*are stored here as well: the geographic locations, the timezone rules, etc.
	*
	*@author Heiko Evermann
	*@version 1.0
	*/
class KStarsData : public QObject
{
	Q_OBJECT
public:
	//Friend classes can see the private data.
	//FIXME: can we avoid having so many friend classes?
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
	friend class WUTDialog;
	friend class INDIDriver;
	friend class INDI_P;
	friend class INDIStdProperty;
	friend class PlanetViewer;
	friend class JMoonTool;
	friend class telescopeWizardProcess;
	friend class KSNewStuff;
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
	bool processCity( const QString& line );

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
		*@short read online database lookup structure.
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
	bool readURLData( const QString &url, int type=0, bool deepOnly=false );

	/**@short open a file containing URL links.
		*@param urlfile string representation of the filename to open
		*@param file reference to the QFile object which will be opened to this file.
		*@return true if file successfully opened. 
		*/
	bool openUrlFile(const QString &urlfile, QFile& file);

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
//FIXME: can we live without this, or do we need a SkyMapComposite fcn?
//	KSPlanet *earth() { return PCat->earth(); }

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

	SkyMapComposite* skyComposite() { return m_SkyComposite; }

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
	
	QString typeName( int );
 
	/**@return reference to the CustomCatalogs list
		*/
// 	QPtrList<CustomCatalog>& customCatalogs() { return CustomCatalogs; }
 
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
		*@param b when true (the default), the next Focus chnage will omit the slewing 
		*animation. 
		*/
	void setSnapNextFocus(bool b=true) { snapToFocus = b; }

	/**Execute a script.  This function actually duplicates the DCOP functionality 
		*for those cases when invoking DCOP is not practical (i.e., when preparing 
		*a sky image in command-line dump mode).
		*@param name the filename of the script to "execute".
		*@param map pointer to the SkyMap object.
		*@return true if the script was successfully parsed.
		*/
	bool executeScript( const QString &name, SkyMap *map );

	/**@short Initialize celestial equator, horizon and ecliptic.
		*@param num pointer to a KSNumbers object to use.
		*/
//	void initGuides( KSNumbers *num );

	bool useDefaultOptions, startupComplete;

	/*@short Appends telescope sky object to the list of INDI telescope objects. This enables KStars to track all telescopes properly.
		*@param object pointer to telescope sky object
	
	void appendTelescopeObject(SkyObject * object);*/

signals:
	/**Signal that specifies the text that should be drawn in the KStarsSplash window.
		*/
	void progressText( const QString& );

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
	void slotConsoleMessage( QString s ) { std::cout << (const char*)(s.toLocal8Bit()) << std::endl; }

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
		*@param b true if the box is shaded
		*/
	void saveTimeBoxShaded( bool b );

	/**@short Save the shaded state of the Geo infobox.
		*@param b true if the box is shaded
		*/
	void saveGeoBoxShaded( bool b );

	/**@short Save the shaded state of the Focus infobox.
		*@param b true if the box is shaded
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
 
private:

/**Display an Error messagebox if a data file could not be opened.  If the file
	*was marked as "required", then abort the program when the messagebox is closed.
	*Otherwise, continue loading the program.
	*@param fn the name of the file which could not be opened.
	*@param required if true, then the error message is more severe, and the program 
	*exits when the messagebox is closed.
	*/
	void initError(const QString &fn, bool required);

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

	QList<SkyObject> objList;

// 	QPtrList<StarObject> starList;

	unsigned int StarCount;

	QList<VariableStarInfo*> VariableStarsList;
	QList<ADVTreeData*> ADVtreeList;
	QList<INDIHostsInfo*> INDIHostsList;
	
	SkyMapComposite* m_SkyComposite;
	
	GeoLocation Geo;
	SimClock Clock;
	KStarsDateTime LTime;
	ColorScheme CScheme;


	bool TimeRunsForward, temporaryTrail, snapToFocus;

//	QString cnameFile;
	KStandardDirs *stdDirs;
	KLocale *locale;

	dms *LST, *HourAngle;

	QKeySequence resumeKey;

	FOV fovSymbol;

	double Obliquity, dObliq, dEcLong;
	KStarsDateTime LastNumUpdate, LastSkyUpdate, LastPlanetUpdate, LastMoonUpdate;
	KStarsDateTime NextDSTChange;
	KStarsDateTime StoredDate;

	QTimer *initTimer;
	int initCounter;

	QString TypeName[12];

	//--- Static member variables
	//the number of KStarsData objects.
	static int objects;
	static QList<GeoLocation*> geoList;
	static QMap<QString, TimeZoneRule> Rulebook;
};


#endif // KSTARSDATA_H_

