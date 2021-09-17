/*
    SPDX-FileCopyrightText: 2001 Heiko Evermann <heiko@evermann.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "colorscheme.h"
#include "geolocation.h"
#include "ksnumbers.h"
#include "kstarsdatetime.h"
#include "ksuserdb.h"
#include "simclock.h"
#include "skyobjectuserdata.h"
#include <qobject.h>
#ifndef KSTARS_LITE
#include "oal/oal.h"
#include "oal/log.h"
#include "polyfills/qstring_hash.h"
#endif

#include <QList>
#include <QMap>
#include <QKeySequence>

#include <iostream>
#include <memory>
#include <unordered_map>

#define MINZOOM     250.
#define MAXZOOM     5000000.
#define DEFAULTZOOM 2000.
#define DZOOM       1.189207115 // 2^(1/4)
#define AU_KM       1.49605e8   //km in one AU

class QFile;

class Execute;
class FOV;
class ImageExporter;
class SkyMap;
class SkyMapComposite;
class SkyObject;
class ObservingList;
class TimeZoneRule;

#ifdef KSTARS_LITE
//Will go away when details window will be implemented in KStars Lite
struct ADVTreeData
{
    QString Name;
    QString Link;
    int Type;
};
#else
struct ADVTreeData;
#endif

/**
 * @class KStarsData
 * KStarsData is the backbone of KStars.  It contains all the data used by KStars,
 * including the SkyMapComposite that contains all items in the skymap
 * (stars, deep-sky objects, planets, constellations, etc).  Other kinds of data
 * are stored here as well: the geographic locations, the timezone rules, etc.
 *
 * @author Heiko Evermann
 * @version 1.0
 */
class KStarsData : public QObject
{
        Q_OBJECT

    protected:
        /** Constructor. */
        KStarsData();

    public:
        // FIXME: It uses temporary trail. There must be way to
        //        this better. And resumeKey in DBUS code
        friend class KStars;
        // FIXME: it uses temporary trail and resumeKey
        friend class SkyMap;
        // FIXME: uses geoList and changes it.
        friend class LocationDialog;
        friend class LocationDialogLite;

        static KStarsData *Create();

        static inline KStarsData *Instance()
        {
            return pinstance;
        }

        /**
         * Initialize KStarsData while running splash screen.
         * @return true on success.
         */
        bool initialize();

        /** Destructor.  Delete data objects. */
        ~KStarsData() override;

        /**
         * Set the NextDSTChange member.
         *  Need this accessor because I could not make KStars::privatedata a friend
         *  class for some reason...:/
         */
        void setNextDSTChange(const KStarsDateTime &dt)
        {
            NextDSTChange = dt;
        }

        /**
         * Returns true if time is running forward else false. Used by KStars to prevent
         *  double calculations of daylight saving change time.
         */
        bool isTimeRunningForward() const
        {
            return TimeRunsForward;
        }

        /** @return pointer to the localization (KLocale) object */
        //KLocale *getLocale() { return locale; }

        /**
         * @short Find object by name.
         * @param name Object name to find
         * @return pointer to SkyObject matching this name
         */
        SkyObject *objectNamed(const QString &name);

        /**
         * The Sky is updated more frequently than the moon, which is updated more frequently
         * than the planets.  The date of the last update for each category is recorded so we
         * know when we need to do it again (see KStars::updateTime()).
         * Initializing these to -1000000.0 ensures they will be updated immediately
         * on the first call to KStars::updateTime().
         */
        void setFullTimeUpdate();

        /**
         * Change the current simulation date/time to the KStarsDateTime argument.
         * Specified DateTime is always universal time.
         * @param newDate the DateTime to set.
         */
        void changeDateTime(const KStarsDateTime &newDate);

        /** @return pointer to the current simulation local time */
        const KStarsDateTime &lt() const
        {
            return LTime;
        }

        /** @return reference to the current simulation universal time */
        const KStarsDateTime &ut() const
        {
            return Clock.utc();
        }

        /** Sync the LST with the simulation clock. */
        void syncLST();

        /** @return pointer to SkyComposite */
        SkyMapComposite *skyComposite()
        {
            return m_SkyComposite.get();
        }

        /** @return pointer to the ColorScheme object */
        ColorScheme *colorScheme()
        {
            return &CScheme;
        }

        /** @return file name of current color scheme **/
        Q_INVOKABLE QString colorSchemeFileName() { return CScheme.fileName(); }

        /** @return file name of the color scheme with the name \p name **/
        QString colorSchemeFileName(const QString &name)
        {
            return m_color_schemes.count(name) > 0 ? m_color_schemes.at(name) : "";
        }

        /** @return file name of the current color scheme **/
        Q_INVOKABLE QString colorSchemeName()
        {
            return colorSchemeName(CScheme.fileName());
        }

        /** @return the name of the color scheme with the name \p name **/
        QString colorSchemeName(const QString &fileName)
        {
            return m_color_scheme_names.count(fileName) > 0 ? m_color_scheme_names.at(fileName) : "";
        }

        /** @return if the color scheme with the name or filename \p scheme is loaded **/
        bool hasColorScheme(const QString &scheme)
        {
            return m_color_scheme_names.count(scheme) || m_color_schemes.count(scheme);
        }

        /** Register a color scheme with \p filename and \p name. */
        void add_color_scheme(const QString &filename, const QString &name)
        {
            m_color_schemes[name] = filename;
            m_color_scheme_names[filename] = name;
        };

        /** \return a map of color scheme names and filenames */
        const std::map<QString, QString> color_schemes() { return m_color_schemes; };

        /** @return pointer to the KSUserDB object */
        KSUserDB *userdb() { return &m_ksuserdb; }

        /** @return pointer to the simulation Clock object */
        Q_INVOKABLE SimClock *clock()
        {
            return &Clock;
        }

        /** @return pointer to the local sidereal time: a dms object */
        CachingDms *lst()
        {
            return &LST;
        }

        /** @return pointer to the GeoLocation object*/
        GeoLocation *geo()
        {
            return &m_Geo;
        }

        /** @return list of all geographic locations */
        QList<GeoLocation *> &getGeoList()
        {
            return geoList;
        }

        GeoLocation *locationNamed(const QString &city, const QString &province = QString(),
                                   const QString &country = QString());

        /**
         * @brief nearestLocation Return nearest location to the given longitude and latitude coordinates
         * @param longitude Longitude (-180 to +180)
         * @param latitude Latitude (-90 to +90)
         * @return nearest geographical location to the parameters above.
         */
        GeoLocation *nearestLocation(double longitude, double latitude);

        /**
         * Set the GeoLocation according to the argument.
         * @param l reference to the new GeoLocation
         */
        void setLocation(const GeoLocation &l);

        /** Set the GeoLocation according to the values stored in the configuration file. */
        void setLocationFromOptions();

        /** Return map for daylight saving rules. */
        const QMap<QString, TimeZoneRule> &getRulebook() const
        {
            return Rulebook;
        }

        /** @return whether the next Focus change will omit the slewing animation. */
        bool snapNextFocus() const
        {
            return snapToFocus;
        }

        /**
         * Disable or re-enable the slewing animation for the next Focus change.
         * @note If the user has turned off all animated slewing, setSnapNextFocus(false)
         * will *NOT* enable animation on the next slew.  A false argument would only
         * be used if you have previously called setSnapNextFocus(true), but then decided
         * you didn't want that after all.  In other words, it's extremely unlikely you'd
         * ever want to use setSnapNextFocus(false).
         * @param b when true (the default), the next Focus change will omit the slewing
         * animation.
         */
        void setSnapNextFocus(bool b = true)
        {
            snapToFocus = b;
        }

        /**
         * Execute a script.  This function actually duplicates the DCOP functionality
         * for those cases when invoking DCOP is not practical (i.e., when preparing
         * a sky image in command-line dump mode).
         * @param name the filename of the script to "execute".
         * @param map pointer to the SkyMap object.
         * @return true if the script was successfully parsed.
         */
        bool executeScript(const QString &name, SkyMap *map);

        /** Synchronize list of visible FOVs and list of selected FOVs in Options */
#ifndef KSTARS_LITE
        void syncFOV();
#endif

        /**
         * @return the list of visible FOVs
         */
        inline const QList<FOV *> getVisibleFOVs() const
        {
            return visibleFOVs;
        }

        /**
         * @return the list of available FOVs
         */
        inline const QList<FOV *> getAvailableFOVs() const
        {
            return availFOVs;
        }

        /**
         * @brief addTransientFOV Adds a new FOV to the list.
         * @param newFOV pointer to FOV object.
         */
        inline void addTransientFOV(std::shared_ptr<FOV> newFOV)
        {
            transientFOVs.append(newFOV);
        }
        inline void clearTransientFOVs()
        {
            transientFOVs.clear();
        }

        /**
         * @return the list of transient FOVs
         */
        inline const QList<std::shared_ptr<FOV>> getTransientFOVs() const
        {
            return transientFOVs;
        }
#ifndef KSTARS_LITE
        /** Return log object */
        OAL::Log *logObject()
        {
            return m_LogObject.get();
        }

        /** Return ADV Tree */
        QList<ADVTreeData *> avdTree()
        {
            return ADVtreeList;
        }

        inline ObservingList *observingList() const
        {
            return m_ObservingList;
        }

        ImageExporter *imageExporter();

        Execute *executeSession();
#endif
        /*@short Increments the updateID, forcing a recomputation of star positions as well */
        unsigned int incUpdateID();

        unsigned int updateID() const
        {
            return m_updateID;
        }
        unsigned int updateNumID() const
        {
            return m_updateNumID;
        }
        KSNumbers *updateNum()
        {
            return &m_updateNum;
        }
        void syncUpdateIDs();

    signals:
        /** Signal that specifies the text that should be drawn in the KStarsSplash window. */
        void progressText(const QString &text);

        /** Should be used to refresh skymap. */
        void skyUpdate(bool);

        /** If data changed, emit clearCache signal. */
        void clearCache();

        /** Emitted when geo location changed */
        void geoChanged();

    public slots:
        /** @short send a message to the console*/
        void slotConsoleMessage(QString s)
        {
            std::cout << (const char *)(s.toLocal8Bit()) << std::endl;
        }

        /**
         * Update the Simulation Clock.  Update positions of Planets.  Update
         * Alt/Az coordinates of objects.  Update precession.
         * emit the skyUpdate() signal so that SkyMap / whatever draws the sky can update itself
         *
         * This is ugly.
         * It _will_ change!
         * (JH:)hey, it's much less ugly now...can we lose the comment yet? :p
         */
        void updateTime(GeoLocation *geo, const bool automaticDSTchange = true);

        /**
         * Sets the direction of time and stores it in bool TimeRunForwards. If scale >= 0
         * time is running forward else time runs backward. We need this to calculate just
         * one daylight saving change time (previous or next DST change).
         */
        void setTimeDirection(float scale);

        // What follows is mostly a port of Arkashs auxdata stuff to a
        // more centralized approach that does not store the data in
        // the skyobjects as they are ephemeral in the new DSO implementation
        //
        // I've tried to reuse as much code as possible and maintain
        // compatibility with peoples data.
        //
        // -- Valentin Boettcher

        /**
         * Get a reference to the user data of an object with the name \p name.
         */
        const SkyObjectUserdata::Data &getUserData(const QString &name);

        /**
         * Adds a link \p data to the user data for the object with \p
         * name, both in memory and on disk.
         *
         * @returns {success, error_message}
         */
        std::pair<bool, QString> addToUserData(const QString &name,
                                               const SkyObjectUserdata::LinkData &data);

        /**
         * Replace \p data in the user data at \p index for the object with \p
         * name, both in memory and on disk.
         *
         * @returns {success, error_message}
         */
        std::pair<bool, QString> editUserData(const QString &name,
                                              const unsigned int index,
                                              const SkyObjectUserdata::LinkData &data);

        /**
         * Remove data of \p type from the user data at \p index for
         * the object with \p name, both in memory and on disk.
         *
         * @returns {success, error_message}
         */
        std::pair<bool, QString> deleteUserData(const QString &name,
                                                const unsigned int index,
                                                SkyObjectUserdata::Type type);
        /**
         * Update the user log of the object with the \p name to
         * contain \p newLog (find and replace).
         *
         * @returns {success, error_message}
         */
        std::pair<bool, QString> updateUserLog(const QString &name,
                                               const QString &newLog);

      private:
        /**
         * Populate list of geographic locations from "citydb.sqlite" database. Also check for custom
         * locations file "mycitydb.sqlite" database, but don't require it.  Each line in the file
         * provides the information required to create one GeoLocation object.
         * @short Fill list of geographic locations from file(s)
         * @return true if at least one city read successfully.
         * @see KStarsData::processCity()
         */
        bool readCityData();

        /** Read the data file that contains daylight savings time rules. */
        bool readTimeZoneRulebook();

        //TODO JM: ADV tree should use XML instead
        /**
         * Read Advanced interface structure to be used later to construct the list view in
         * the advanced tab in the Detail Dialog.
         * @li KSLABEL designates a top-level parent label
         * @li KSINTERFACE designates a common URL interface for several objects
         * @li END designates the end of a sub tree structure
         * @short read online database lookup structure.
         * @return true if data is successfully read.
         */
        bool readADVTreeData();

        /** Read INDI hosts from an XML file */
        bool readINDIHosts();

        //TODO JM: Use XML instead; The logger should have more features
        // that allow users to enter details about their observation logs
        // objects observed, eye pieces, telescope, conditions, mag..etc
        /**
         * @short read user logs.
         *
         * Read user logs. The log file is formatted as following:
         * @li KSLABEL designates the beginning of a log
         * @li KSLogEnd designates the end of a log.
         *
         * @return true if data is successfully read.
         */
        bool readUserLog();

        /**
         * Read in URLs to be attached to a named object's right-click popup menu.  At this
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
        bool readURLData(const QString &url,
                         SkyObjectUserdata::Type type = SkyObjectUserdata::Type::website);

        /**
         * @short open a file containing URL links.
         * @param urlfile string representation of the filename to open
         * @param file reference to the QFile object which will be opened to this file.
         * @return true if file successfully opened.
         */
        bool openUrlFile(const QString &urlfile, QFile &file);

        /**
         * Reset local time to new daylight saving time. Use this function if DST has changed.
         * Used by updateTime().
         */
        void resetToNewDST(GeoLocation *geo, const bool automaticDSTchange);

        /**
         * As KStarsData::getUserData just non-const.
         * @warning This method is not thread safe :) so take care of that when you use it.
         */
        SkyObjectUserdata::Data &findUserData(const QString &name);

        QList<ADVTreeData *> ADVtreeList;
        std::unique_ptr<SkyMapComposite> m_SkyComposite;

        GeoLocation m_Geo;
        SimClock Clock;
        KStarsDateTime LTime;
        KSUserDB m_ksuserdb;
        ColorScheme CScheme;
        std::map<QString, QString> m_color_schemes; // name: filename
        std::map<QString, QString> m_color_scheme_names; // filename: name

#ifndef KSTARS_LITE
        ObservingList* m_ObservingList { nullptr };
        std::unique_ptr<OAL::Log> m_LogObject;
        std::unique_ptr<Execute> m_Execute;
        std::unique_ptr<ImageExporter> m_ImageExporter;
#endif

        //EquipmentWriter *m_equipmentWriter;

        bool TimeRunsForward { false };
        bool temporaryTrail { false };
        // FIXME: Used in SkyMap only. Check!
        bool snapToFocus { false };

        //KLocale *locale;

        CachingDms LST;

        QKeySequence resumeKey;

        QList<FOV *> availFOVs;         // List of all available FOVs
        QList<FOV *> visibleFOVs;       // List of visible FOVs. Cached from Options::FOVNames
        QList<std::shared_ptr<FOV>> transientFOVs;     // List of non-permenant transient FOVs.

        KStarsDateTime LastNumUpdate, LastSkyUpdate, LastPlanetUpdate, LastMoonUpdate;
        KStarsDateTime NextDSTChange;
        // FIXME: Used in kstarsdcop.cpp only
        KStarsDateTime StoredDate;

        QList<GeoLocation *> geoList;
        QMap<QString, TimeZoneRule> Rulebook;

        quint32 m_preUpdateID, m_updateID;
        quint32 m_preUpdateNumID, m_updateNumID;
        KSNumbers m_preUpdateNum, m_updateNum;

        static KStarsData *pinstance;

        std::unordered_map<QString, SkyObjectUserdata::Data> m_user_data;
        QMutex m_user_data_mutex; // for m_user_data
};
