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

#include "ksnumbers.h"
#include "ksuserdb.h"
#include "datahandlers/catalogdb.h"

#include <QList>
#include <QMap>
#include <QKeySequence>

#include "geolocation.h"
#include "colorscheme.h"
#include "kstarsdatetime.h"
#include "simclock.h"
#include "oal/oal.h"
#include "oal/log.h"

#define MINZOOM 250.
#define MAXZOOM 5000000.
#define DEFAULTZOOM 2000.
#define DZOOM 1.189207115  // 2^(1/4)
#define AU_KM 1.49605e8    //km in one AU


class QFile;

class dms;
class SkyMap;
class SkyMapComposite;
class SkyObject;
class FOV;

class TimeZoneRule;
struct ADVTreeData;


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

protected:
    /**Constructor. */
    KStarsData();

public:
    // FIXME: It uses temporary trail. There must be way to
    //        this better. And resumeKey in DBUS code
    friend class KStars;
    // FIXME: it uses temporary trail and resumeKey
    friend class SkyMap;
    // FIXME: uses geoList and changes it.
    friend class LocationDialog;

    static KStarsData* Create( );

    static inline KStarsData* Instance() { return pinstance; }

    /** Initialize KStarsData while running splash screen.
     *  @return true on success.
     */
    bool initialize();

    /**Destructor.  Delete data objects. */
    virtual ~KStarsData();

    /** Set the NextDSTChange member.
     *  Need this accessor because I could not make KStars::privatedata a friend
     *  class for some reason...:/
     */
    void setNextDSTChange( const KStarsDateTime &dt ) { NextDSTChange = dt; }

    /** Returns true if time is running forward else false. Used by KStars to prevent
     *  double calculations of daylight saving change time.
     */
    bool isTimeRunningForward() const { return TimeRunsForward; }

    /**@return pointer to the localization (KLocale) object */
    //KLocale *getLocale() { return locale; }

    /**@short Find object by name.
     * @param name Object name to find
     * @return pointer to SkyObject matching this name
     */
    SkyObject* objectNamed( const QString &name );

    /**The Sky is updated more frequently than the moon, which is updated more frequently
     * than the planets.  The date of the last update for each category is recorded so we
     * know when we need to do it again (see KStars::updateTime()).
     * Initializing these to -1000000.0 ensures they will be updated immediately
     * on the first call to KStars::updateTime().
     */
    void setFullTimeUpdate();

    /**change the current simulation date/time to the KStarsDateTime argument.
     * Specified DateTime is always universal time.
     * @param newDate the DateTime to set.
     */
    void changeDateTime( const KStarsDateTime &newDate );

    /** @return pointer to the current simulation local time */
    const KStarsDateTime& lt() const { return LTime; }

    /** @return reference to the current simulation universal time */
    const KStarsDateTime& ut() const { return Clock.utc(); }

    /** Sync the LST with the simulation clock. */
    void syncLST();

    /** @return pointer to SkyComposite */
    SkyMapComposite* skyComposite() { return m_SkyComposite; }

    /**@return pointer to the ColorScheme object */
    ColorScheme *colorScheme() { return &CScheme; }

    /**@return pointer to the KSUserDB object */
    KSUserDB *userdb() { return &m_ksuserdb; }

    /**@return pointer to the Catalog DB object */
    CatalogDB *catalogdb() { return &m_catalogdb; }

    /**@return pointer to the simulation Clock object */
    SimClock *clock() { return &Clock; }

    /**@return pointer to the local sidereal time: a dms object */
    dms *lst() { return &LST; }

    /**@return pointer to the GeoLocation object*/
    GeoLocation *geo() { return &m_Geo; }

    /** @return list of all geographic locations */
    QList<GeoLocation*> & getGeoList() { return geoList; }

    GeoLocation *locationNamed( const QString &city, const QString &province=QString(), const QString &country= QString() );

    /**Set the GeoLocation according to the argument.
     * @param l reference to the new GeoLocation
     */
    void setLocation( const GeoLocation &l );

    /** Set the GeoLocation according to the values stored in the configuration file. */
    void setLocationFromOptions();

    /** Return map for daylight saving rules. */
    const QMap<QString, TimeZoneRule>& getRulebook() const { return Rulebook; }

    /** @return whether the next Focus change will omit the slewing animation. */
    bool snapNextFocus() const { return snapToFocus; }

    /**Disable or re-enable the slewing animation for the next Focus change.
     * @note If the user has turned off all animated slewing, setSnapNextFocus(false)
     * will *NOT* enable animation on the next slew.  A false argument would only
     * be used if you have previously called setSnapNextFocus(true), but then decided
     * you didn't want that after all.  In other words, it's extremely unlikely you'd
     * ever want to use setSnapNextFocus(false).
     * @param b when true (the default), the next Focus change will omit the slewing
     * animation.
     */
    void setSnapNextFocus(bool b=true) { snapToFocus = b; }

    /**Execute a script.  This function actually duplicates the DCOP functionality
     * for those cases when invoking DCOP is not practical (i.e., when preparing
     * a sky image in command-line dump mode).
     * @param name the filename of the script to "execute".
     * @param map pointer to the SkyMap object.
     * @return true if the script was successfully parsed.
     */
    bool executeScript( const QString &name, SkyMap *map );

    /** Synchronize list of visible FOVs and list of selected FOVs in Options */
    void syncFOV();

    /**
     *@return the list of visible FOVs
     */
    inline const QList<FOV*> getVisibleFOVs() const { return visibleFOVs; }

    /**
      *@return the list of available FOVs
      */
    inline const QList<FOV*> getAvailableFOVs() const { return availFOVs; }

    /** Return log object */
    OAL::Log *logObject() { return m_logObject; }

    /** Return ADV Tree */
    QList<ADVTreeData*> avdTree() { return ADVtreeList; }

    /*@short Increments the updateID, forcing a recomputation of star positions as well */
    unsigned int incUpdateID();

    unsigned int updateID()    const { return m_updateID; }
    unsigned int updateNumID() const { return m_updateNumID; }
    KSNumbers* updateNum()     { return &m_updateNum; }
    void syncUpdateIDs();

signals:
    /** Signal that specifies the text that should be drawn in the KStarsSplash window. */
    void progressText( const QString& );

    /** Should be used to refresh skymap. */
    void update();

    /** If data changed, emit clearCache signal. */
    void clearCache();

    /** Emitted when geo location changed */
    void geoChanged();

public slots:
    /**@short send a message to the console*/
    void slotConsoleMessage( QString s ) { std::cout << (const char*)(s.toLocal8Bit()) << std::endl; }

    /**Update the Simulation Clock.  Update positions of Planets.  Update
     * Alt/Az coordinates of objects.  Update precession.  Update Focus position.
     * Draw new Skymap.
     *
     * This is ugly.
     * It _will_ change!
     * (JH:)hey, it's much less ugly now...can we lose the comment yet? :p
     */
    void updateTime(GeoLocation *geo, SkyMap * skymap, const bool automaticDSTchange = true);

    /**Sets the direction of time and stores it in bool TimeRunForwards. If scale >= 0
     * time is running forward else time runs backward. We need this to calculate just
     * one daylight saving change time (previous or next DST change).
     */
    void setTimeDirection( float scale );

private:
    /**Populate list of geographic locations from "citydb.sqlite" database. Also check for custom
     * locations file "mycitydb.sqlite" database, but don't require it.  Each line in the file
     * provides the information required to create one GeoLocation object.
     * @short Fill list of geographic locations from file(s)
     * @return true if at least one city read successfully.
     * @see KStarsData::processCity()
     */
    bool readCityData();

    /**Read the data file that contains daylight savings time rules. */
    bool readTimeZoneRulebook();

    //TODO JM: ADV tree should use XML instead
    /**Read Advanced interface structure to be used later to construct the list view in
     * the advanced tab in the Detail Dialog.
     * @li KSLABEL designates a top-level parent label
     * @li KSINTERFACE designates a common URL interface for several objects
     * @li END designates the end of a sub tree structure
     * @short read online database lookup structure.
     * @return true if data is successfully read.
     */
    bool readADVTreeData();

    /**Read INDI hosts from an XML file*/
    bool readINDIHosts();

    //TODO JM: Use XML instead; The logger should have more features
    // that allow users to enter details about their observation logs
    // objects observed, eye pieces, telescope, conditions, mag..etc
    /** @short read user logs.
     *
     * Read user logs. The log file is formatted as following:
     * @li KSLABEL designates the beginning of a log
     * @li KSLogEnd designates the end of a log.
     *
     * @return true if data is successfully read.
     */
    bool readUserLog();

    /**Read in URLs to be attached to a named object's right-click popup menu.  At this
     * point, there is no way to attach URLs to unnamed objects.  There are two
     * kinds of URLs, each with its own data file: image links and webpage links.  In addition,
     * there may be user-specific versions with custom URLs.  Each line contains 3 fields
     * separated by colons (":").  Note that the last field is the URL, and as such it will
     * generally contain a colon itself.  Only the first two colons encountered are treated
     * as field separators.  The fields are:
     *
     * @li Object name.  This must be the "primary" name of the object (the name at the top of the popup menu).
     * @li Menu text.  The string that should appear in the popup menu to activate the link.
     * @li URL.
     * @short Read in image and information URLs.
     * @return true if data files were successfully read.
     */
    bool readURLData( const QString &url, int type=0, bool deepOnly=false );

    /** @short open a file containing URL links.
     *  @param urlfile string representation of the filename to open
     *  @param file reference to the QFile object which will be opened to this file.
     *  @return true if file successfully opened.
     */
    bool openUrlFile(const QString &urlfile, QFile& file);

    /**Reset local time to new daylight saving time. Use this function if DST has changed.
     * Used by updateTime().
     */
    void resetToNewDST(GeoLocation *geo, const bool automaticDSTchange);

    QList<ADVTreeData*> ADVtreeList;
    SkyMapComposite* m_SkyComposite;

    GeoLocation m_Geo;
    SimClock Clock;
    KStarsDateTime LTime;
    KSUserDB m_ksuserdb;
    CatalogDB m_catalogdb;
    ColorScheme CScheme;
    OAL::Log *m_logObject;

    bool TimeRunsForward, temporaryTrail;
    // FIXME: Used in SkyMap only. Check!
    bool snapToFocus;

    //KLocale *locale;

    dms LST;

    QKeySequence resumeKey;

    QList<FOV*> availFOVs;   // List of all available FOVs
    QList<FOV*> visibleFOVs; // List of visible FOVs. Cached from Options::FOVNames

    KStarsDateTime LastNumUpdate, LastSkyUpdate, LastPlanetUpdate, LastMoonUpdate;
    KStarsDateTime NextDSTChange;
    // FIXME: Used in kstarsdcop.cpp only
    KStarsDateTime StoredDate;

    QList<GeoLocation*> geoList;
    QMap<QString, TimeZoneRule> Rulebook;

    quint32   m_preUpdateID,    m_updateID;
    quint32   m_preUpdateNumID, m_updateNumID;
    KSNumbers m_preUpdateNum,   m_updateNum;

    static KStarsData* pinstance;
};


#endif // KSTARSDATA_H_
