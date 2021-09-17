/*
    SPDX-FileCopyrightText: 2001 Jason Harris <jharris@30doradus.org>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "config-kstars.h"

#include <KXmlGuiWindow>
#include <KLocalizedString>
#include <QLabel>

#include <QDockWidget>
#if QT_VERSION >= QT_VERSION_CHECK(5, 8, 0)
#include <QtDBus/qtdbusglobal.h>
#else
#include <QtDBus/qdbusmacros.h>
#endif
#ifdef HAVE_CFITSIO
#include <QPointer>
#endif

// forward declaration is enough. We only need pointers
class QActionGroup;
class QDockWidget;
class QPalette;
class KActionMenu;
class KConfigDialog;

class KStarsData;
class SkyPoint;
class SkyMap;
class GeoLocation;
class FindDialog;
class TimeStepBox;
class ImageExporter;

class AltVsTime;
class WUTDialog;
class WIView;
class WILPSettings;
class WIEquipSettings;
class ObsConditions;
class AstroCalc;
class SkyCalendar;
class ScriptBuilder;
class PlanetViewer;
class JMoonTool;
class MoonPhaseTool;
class FlagManager;
class Execute;
class ExportImageDialog;
class PrintingWizard;
class HorizonManager;
class EyepieceField;
class AddDeepSkyObject;

class OpsCatalog;
class OpsGuides;
class OpsSolarSystem;
class OpsSatellites;
class OpsSupernovae;
class OpsTerrain;
class OpsColors;
class OpsAdvanced;
class OpsINDI;
class OpsEkos;
class OpsFITS;
class OpsXplanet;

namespace Ekos
{
class Manager;
}

#ifdef HAVE_CFITSIO
class FITSViewer;
#endif

/**
 *@class KStars
 *@short This is the main window for KStars.
 *In addition to the GUI elements, the class contains the program clock,
 KStarsData, and SkyMap objects.  It also contains functions for the \ref DBusInterface D-Bus interface.  KStars is now a singleton class. Use KStars::createInstance() to
 create an instance and KStars::Instance() to get a pointer to the instance
 *@author Jason Harris, Jasem Mutlaq
 *@version 1.1
 */

class KStars : public KXmlGuiWindow
{
        Q_OBJECT
        Q_CLASSINFO("D-Bus Interface", "org.kde.kstars")
        Q_SCRIPTABLE Q_PROPERTY(QString colorScheme READ colorScheme WRITE loadColorScheme NOTIFY colorSchemeChanged)

    public:
        Q_SCRIPTABLE QString colorScheme() const;

    private:
        /**
             * @short Constructor.
             * @param doSplash should the splash panel be displayed during
             * initialization.
             * @param startClockRunning should the clock be running on startup?
             * @param startDateString date (in string representation) to start running from.
             *
             * @todo Refer to documentation on date format.
             */
        explicit KStars(bool doSplash, bool startClockRunning = true, const QString &startDateString = QString());

    public:
        /**
             * @short Create an instance of this class. Destroy any previous instance
             * @param doSplash
             * @param clockrunning
             * @param startDateString
             * @note See KStars::KStars for details on parameters
             * @return a pointer to the instance
             */
        static KStars *createInstance(bool doSplash, bool clockrunning = true, const QString &startDateString = QString());

        /** @return a pointer to the instance of this class */
        inline static KStars *Instance()
        {
            return pinstance;
        }

        /** Destructor. */
        ~KStars() override;

        /** Syncs config file. Deletes objects. */
        void releaseResources();

        /** @return pointer to KStarsData object which contains application data. */
        inline KStarsData *data() const
        {
            return m_KStarsData;
        }

        /** @return pointer to SkyMap object which is the sky display widget. */
        inline SkyMap *map() const
        {
            return m_SkyMap;
        }

        inline FlagManager *flagManager() const
        {
            return m_FlagManager;
        }

        inline PrintingWizard *printingWizard() const
        {
            return m_PrintingWizard;
        }

#ifdef HAVE_CFITSIO
        //        void addFITSViewer(const QSharedPointer<FITSViewer> &fv);
        const QPointer<FITSViewer> &genericFITSViewer();
        const QPointer<FITSViewer> &createFITSViewer();
        void clearAllViewers();
#endif

        /** Add an item to the color-scheme action manu
             * @param name The name to use in the menu
             * @param actionName The internal name for the action (derived from filename)
             */
        void addColorMenuItem(QString name, const QString &actionName);

        /** Remove an item from the color-scheme action manu
             * @param actionName The internal name of the action (derived from filename)
             */
        void removeColorMenuItem(const QString &actionName);

        /** @short Apply config options throughout the program.
             * In most cases, options are set in the "Options" object directly,
             * but for some things we have to manually react to config changes.
             * @param doApplyFocus If true, then focus position will be set
             * from config file
             */
        void applyConfig(bool doApplyFocus = true);

        /** Sync Options to GUI, if any */
        void syncOps();

        void showImgExportDialog();

        void syncFOVActions();

        void hideAllFovExceptFirst();

        void selectNextFov();

        void selectPreviousFov();

        void showWISettingsUI();

        void showWI(ObsConditions *obs);

        /** Load HIPS information and repopulate menu. */
        void repopulateHIPS();

        WIEquipSettings *getWIEquipSettings()
        {
            return m_WIEquipmentSettings;
        }

    public Q_SLOTS:
        /** @defgroup DBusInterface DBus Interface
             KStars provides powerful scripting functionality via DBus. The most common DBus functions can be constructed and executed within the ScriptBuilder tool.
             Any 3rd party language or tool with support for DBus can access several interfaces provided by KStars:
            <ul>
            <li>KStars: Provides functions to manipulate the skymap including zoom, pan, and motion to selected objects. Add and remove object trails and labels. Wait for user input before running further actions.</li>
            <li>SimClock: Provides functions to start and stop time, set a different date and time, and to set the clock scale.</li>
            <li>Ekos: Provides functions to start and stop Ekos Manager, set Ekos connection mode, and access to Ekos modules:
                <ul>
                <li>Capture: Provides functions to capture images, load sequence queues, control filter wheel, and obtain information on job progress.</li>
                <li>Focus: Provides functions to focus control in manual and automated mode. Start and stop focusing procedures and set autofocus options.</li>
                <li>Guide: Provides functions to start and stop calibration and autoguiding procedures. Set calibration and autoguide options.</li>
                <li>Align: Provides functions to solve images use online or offline astrometry.net solver.</li>
                </ul>
            </li>
            </ul>
            */

        /*@{*/

        /** DBUS interface function.
             * Set focus to given Ra/Dec coordinates
             * @param ra the Right Ascension coordinate for the focus (in Hours)
             * @param dec the Declination coordinate for the focus (in Degrees)
             */
        Q_SCRIPTABLE Q_NOREPLY void setRaDec(double ra, double dec);

        /** DBUS interface function.
             * Set focus to given J2000.0 Ra/Dec coordinates
             * @param ra the J2000.0 Right Ascension coordinate for the focus (in Hours)
             * @param dec the J2000.0 Declination coordinate for the focus (in Degrees)
             */
        Q_SCRIPTABLE Q_NOREPLY void setRaDecJ2000(double ra0, double dec0);

        /** DBUS interface function.
             * Set focus to given Alt/Az coordinates.
             * @param alt the Altitude coordinate for the focus (in Degrees)
             * @param az the Azimuth coordinate for the focus (in Degrees)
             * @param altIsRefracted If set to true, the altitude is interpreted as if it were corrected for atmospheric refraction (i.e. the altitude is an apparent altitude)
             */
        Q_SCRIPTABLE Q_NOREPLY void setAltAz(double alt, double az, bool altIsRefracted = false);

        /** DBUS interface function.
             * Point in the direction described by the string argument.
             * @param direction either an object name, a compass direction (e.g., "north"), or "zenith"
             */
        Q_SCRIPTABLE Q_NOREPLY void lookTowards(const QString &direction);

        /** DBUS interface function.
             * Add a name label to the named object
             * @param name the name of the object to which the label will be attached
             */
        Q_SCRIPTABLE Q_NOREPLY void addLabel(const QString &name);

        /** DBUS interface function.
             * Remove a name label from the named object
             * @param name the name of the object from which the label will be removed
             */
        Q_SCRIPTABLE Q_NOREPLY void removeLabel(const QString &name);

        /** DBUS interface function.
             * Add a trail to the named solar system body
             * @param name the name of the body to which the trail will be attached
             */
        Q_SCRIPTABLE Q_NOREPLY void addTrail(const QString &name);

        /** DBUS interface function.
             * Remove a trail from the named solar system body
             * @param name the name of the object from which the trail will be removed
             */
        Q_SCRIPTABLE Q_NOREPLY void removeTrail(const QString &name);

        /** DBUS interface function.  Zoom in one step. */
        Q_SCRIPTABLE Q_NOREPLY void zoomIn();

        /** DBUS interface function.  Zoom out one step. */
        Q_SCRIPTABLE Q_NOREPLY void zoomOut();

        /** DBUS interface function.  reset to the default zoom level. */
        Q_SCRIPTABLE Q_NOREPLY void defaultZoom();

        /** DBUS interface function. Set zoom level to specified value.
             *  @param z the zoom level. Units are pixels per radian. */
        Q_SCRIPTABLE Q_NOREPLY void zoom(double z);

        /** DBUS interface function.  Set local time and date.
             * @param yr year of date
             * @param mth month of date
             * @param day day of date
             * @param hr hour of time
             * @param min minute of time
             * @param sec second of time
             */
        Q_SCRIPTABLE Q_NOREPLY void setLocalTime(int yr, int mth, int day, int hr, int min, int sec);

        /** DBUS interface function.  Set local time and date to present values acc. system clock
             * @note Just a proxy for slotSetTimeToNow(), but it is better to
             * keep the DBus interface separate from the internal methods.
             */
        Q_SCRIPTABLE Q_NOREPLY void setTimeToNow();

        /** DBUS interface function.  Delay further execution of DBUS commands.
             * @param t number of seconds to delay
             */
        Q_SCRIPTABLE Q_NOREPLY void waitFor(double t);

        /** DBUS interface function.  Pause further DBUS execution until a key is pressed.
             * @param k the key which will resume DBUS execution
             */
        Q_SCRIPTABLE Q_NOREPLY void waitForKey(const QString &k);

        /** DBUS interface function.  Toggle tracking.
             * @param track engage tracking if true; else disengage tracking
             */
        Q_SCRIPTABLE Q_NOREPLY void setTracking(bool track);

        /** DBUS interface function.  modify a view option.
             * @param option the name of the option to be modified
             * @param value the option's new value
             */
        Q_SCRIPTABLE Q_NOREPLY void changeViewOption(const QString &option, const QString &value);

        /** DBUS interface function.
             * @param name the name of the option to query
             * @return the current value of the named option
             */
        Q_SCRIPTABLE QString getOption(const QString &name);

        /** DBUS interface function.  Read config file.
             * This function is useful for restoring the user settings from the config file,
             * after having modified the settings in memory.
             * @sa writeConfig()
             */
        Q_SCRIPTABLE Q_NOREPLY void readConfig();

        /** DBUS interface function.  Write current settings to config file.
             * This function is useful for storing user settings before modifying them with a DBUS
             * script.  The original settings can be restored with readConfig().
             * @sa readConfig()
             */
        Q_SCRIPTABLE Q_NOREPLY void writeConfig();

        /** DBUS interface function.  Show text message in a popup window.
             * @note Not Yet Implemented
             * @param x x-coordinate for message window
             * @param y y-coordinate for message window
             * @param message the text to display in the message window
             */
        Q_SCRIPTABLE Q_NOREPLY void popupMessage(int x, int y, const QString &message);

        /** DBUS interface function.  Draw a line on the sky map.
             * @note Not Yet Implemented
             * @param x1 starting x-coordinate of line
             * @param y1 starting y-coordinate of line
             * @param x2 ending x-coordinate of line
             * @param y2 ending y-coordinate of line
             * @param speed speed at which line should appear from start to end points (in pixels per second)
             */
        Q_SCRIPTABLE Q_NOREPLY void drawLine(int x1, int y1, int x2, int y2, int speed);

        /** DBUS interface function.  Set the geographic location.
             * @param city the city name of the location
             * @param province the province name of the location
             * @param country the country name of the location
             * @return True if geographic location is found and set, false otherwise.
             */
        Q_SCRIPTABLE bool setGeoLocation(const QString &city, const QString &province, const QString &country);

        /**
         * @brief location Returns a JSON Object (as string) that contains the following information:
         * name: String
         * province: String
         * country: String
         * longitude: Double (-180 to +180)
         * latitude: Double (-90 to +90)
         * tz0 (Time zone without DST): Double
         * tz (Time zone with DST): Double
         * @return Stringified JSON object as described above.
         */
        Q_SCRIPTABLE QString location();

        /** DBUS interface function.  Set the GPS geographic location.
             * @param longitude longitude in degrees (-180 West to +180 East)
             * @param latitude latitude in degrees (-90 South to +90 North)
             * @param elevation site elevation in meters
             * @param tz0 Time zone offset WITHOUT daylight saving time.
             * @return True if geographic location is set, false otherwise.
             */
        Q_SCRIPTABLE bool setGPSLocation(double longitude, double latitude, double elevation, double tz0);

        /** DBUS interface function.  Modify a color.
             * @param colorName the name of the color to be modified (e.g., "SkyColor")
             * @param value the new color to use
             */
        Q_SCRIPTABLE Q_NOREPLY void setColor(const QString &colorName, const QString &value);

        /** DBUS interface function.  Load a color scheme.
             * @param name the name of the color scheme to load (e.g., "Moonless Night")
             */
        Q_SCRIPTABLE Q_NOREPLY void loadColorScheme(const QString &name);

        /** DBUS interface function.  Export the sky image to a file.
             * @param filename the filename for the exported image
             * @param width the width for the exported image. Map's width will be used if nothing or an invalid value is supplied.
             * @param height the height for the exported image. Map's height will be used if nothing or an invalid value is supplied.
             * @param includeLegend should we include a legend?
             */
        Q_SCRIPTABLE Q_NOREPLY void exportImage(const QString &filename, int width = -1, int height = -1,
                                                bool includeLegend = false);

        /** DBUS interface function.  Return a URL to retrieve Digitized Sky Survey image.
             * @param objectName name of the object.
             * @note If the object is note found, the string "ERROR" is returned.
             */
        Q_SCRIPTABLE QString getDSSURL(const QString &objectName);

        /** DBUS interface function.  Return a URL to retrieve Digitized Sky Survey image.
             * @param RA_J2000 J2000.0 RA
             * @param Dec_J2000 J2000.0 Declination
             * @param width width of the image, in arcminutes (default = 15)
             * @param height height of the image, in arcminutes (default = 15)
             */
        Q_SCRIPTABLE QString getDSSURL(double RA_J2000, double Dec_J2000, float width = 15, float height = 15);

        /** DBUS interface function.  Return XML containing information about a sky object
             * @param objectName name of the object.
             * @param fallbackToInternet Attempt to resolve the name using internet databases if not found
             * @param storeInternetResolved If we fell back to the internet, save the result in DSO database for future offline access
             * @note If the object was not found, the XML is empty.
             */
        Q_SCRIPTABLE QString getObjectDataXML(const QString &objectName,
                                              bool fallbackToInternet = false,
                                              bool storeInternetResolved = true);

        /** DBUS interface function.  Return XML containing position info about a sky object
             * @param objectName name of the object.
             * @note If the object was not found, the XML is empty.
             */
        Q_SCRIPTABLE QString getObjectPositionInfo(const QString &objectName);

        /** DBUS interface function. Render eyepiece view and save it in the file(s) specified
             * @note See EyepieceField::renderEyepieceView() for more info. This is a DBus proxy that calls that method, and then writes the resulting image(s) to file(s).
             * @note Important: If imagePath is empty, but overlay is true, or destPathImage is supplied, this method will make a blocking DSS download.
             */
        Q_SCRIPTABLE Q_NOREPLY void renderEyepieceView(const QString &objectName, const QString &destPathChart,
                const double fovWidth = -1.0, const double fovHeight = -1.0,
                const double rotation = 0.0, const double scale = 1.0,
                const bool flip = false, const bool invert = false,
                QString imagePath            = QString(),
                const QString &destPathImage = QString(), const bool overlay = false,
                const bool invertColors = false);

        /** DBUS interface function.  Set the approx field-of-view
             * @param FOV_Degrees field of view in degrees
             */
        Q_SCRIPTABLE Q_NOREPLY void setApproxFOV(double FOV_Degrees);

        /** DBUS interface function.  Get the dimensions of the Sky Map.
             * @return a string containing widthxheight in pixels.
             */
        Q_SCRIPTABLE QString getSkyMapDimensions();

        /** DBUS interface function.  Return a newline-separated list of objects in the observing wishlist.
             * @note Unfortunately, unnamed objects are troublesome. Hopefully, we don't have them on the observing list.
             */
        Q_SCRIPTABLE QString getObservingWishListObjectNames();

        /** DBUS interface function.  Return a newline-separated list of objects in the observing session plan.
             * @note Unfortunately, unnamed objects are troublesome. Hopefully, we don't have them on the observing list.
             */
        Q_SCRIPTABLE QString getObservingSessionPlanObjectNames();

        /** DBUS interface function.  Print the sky image.
             * @param usePrintDialog if true, the KDE print dialog will be shown; otherwise, default parameters will be used
             * @param useChartColors if true, the "Star Chart" color scheme will be used for the printout, which will save ink.
             */
        Q_SCRIPTABLE Q_NOREPLY void printImage(bool usePrintDialog, bool useChartColors);

        /** DBUS interface function.  Open FITS image.
             * @param imageUrl URL of FITS image to load. For a local file the prefix must be file:// For example
             * if the file is located at /home/john/m42.fits then the full URL is file:///home/john/m42.fits
             */
        Q_SCRIPTABLE Q_NOREPLY void openFITS(const QUrl &imageUrl);

        /** @}*/

    signals:
        /** DBUS interface notification. Color scheme was updated.
         */
        void colorSchemeChanged();

    public Q_SLOTS:
        /**
             * Update time-dependent data and (possibly) repaint the sky map.
             * @param automaticDSTchange change DST status automatically?
             */
        void updateTime(const bool automaticDSTchange = true);

        /** action slot: sync kstars clock to system time */
        void slotSetTimeToNow();

        /** Apply new settings and redraw skymap */
        void slotApplyConfigChanges();

        /** Apply new settings for WI */
        void slotApplyWIConfigChanges();

        /** Called when zoom level is changed. Enables/disables zoom
             *  actions and updates status bar. */
        void slotZoomChanged();

        /** action slot: Allow user to specify a field-of-view angle for the display window in degrees,
             * and set the zoom level accordingly. */
        void slotSetZoom();

        /** action slot: Toggle whether kstars is tracking current position */
        void slotTrack();

        /** action slot: open dialog for selecting a new geographic location */
        void slotGeoLocator();

        /**
         * @brief slotSetTelescopeEnabled call when telescope comes online or goes offline.
         * @param enable True if telescope is online and connected, false otherwise.
         */
        void slotSetTelescopeEnabled(bool enable);

        /**
         * @brief slotSetDomeEnabled call when dome comes online or goes offline.
         * @param enable True if dome is online and connected, false otherwise.
         */
        void slotSetDomeEnabled(bool enable);

        /** Delete FindDialog because ObjNames list has changed in KStarsData due to
             * reloading star data. So list in FindDialog must be new filled with current data. */
        void clearCachedFindDialog();

        /** Remove all trails which may have been added to solar system bodies */
        void slotClearAllTrails();

        /** Display position in the status bar. */
        void slotShowPositionBar(SkyPoint *);

        /** action slot: open Flag Manager */
        void slotFlagManager();

        /** Show the eyepiece view tool */
        void slotEyepieceView(SkyPoint *sp, const QString &imagePath = QString());

        /** Show the DSO Catalog Management GUI */
        void slotDSOCatalogGUI();

        /** action slot: open KStars startup wizard */
        void slotWizard();

        void updateLocationFromWizard(const GeoLocation &geo);

        WIView *wiView()
        {
            return m_WIView;
        }

        bool isWIVisible()
        {
            if (!m_WIView)
                return false;
            if (!m_wiDock)
                return false;
            return m_wiDock->isVisible();
        }

        //FIXME Port to QML2
        //#if 0
        /** action slot: open What's Interesting settings window */
        void slotWISettings();

        /** action slot: toggle What's Interesting window */
        void slotToggleWIView();
        //#endif

    private slots:
        /** action slot: open a dialog for setting the time and date */
        void slotSetTime();

        /** action slot: toggle whether kstars clock is running or not */
        void slotToggleTimer();

        /** action slot: advance one step forward in time */
        void slotStepForward();

        /** action slot: advance one step backward in time */
        void slotStepBackward();

        /** action slot: open dialog for finding a named object */
        void slotFind();

        /** action slot: open KNewStuff window to download extra data. */
        void slotDownload();

        /** action slot: open KStars calculator to compute astronomical ephemeris */
        void slotCalculator();

        /** action slot: open Elevation vs. Time tool */
        void slotAVT();

        /** action slot: open What's up tonight dialog */
        void slotWUT();

        /** action slot: open Sky Calendar tool */
        void slotCalendar();

        /** action slot: open the glossary */
        void slotGlossary();

        /** action slot: open ScriptBuilder dialog */
        void slotScriptBuilder();

        /** action slot: open Solar system viewer */
        void slotSolarSystem();

        /** action slot: open Jupiter Moons tool */
        void slotJMoonTool();

        /** action slot: open Moon Phase Calendar tool */
        void slotMoonPhaseTool();

#if 0
        /** action slot: open Telescope wizard */
        void slotTelescopeWizard();
#endif

        /** action slot: open INDI driver panel */
        void slotINDIDriver();

        /** action slot: open INDI control panel */
        void slotINDIPanel();

        /** action slot: open Ekos panel */
        void slotEkos();

        /** action slot: Track with the telescope (INDI) */
        void slotINDITelescopeTrack();

        /**
         * Action slot: Slew with the telescope (INDI)
         *
         * @param focused_object Slew to the focused object or the mouse pointer if false.
         *
         */
        void slotINDITelescopeSlew(bool focused_object = true);
        void slotINDITelescopeSlewMousePointer();

        /**
         * Action slot: Sync the telescope (INDI)
         *
         * @param focused_object Sync the position of the focused object or the mouse pointer if false.
         *
         */
        void slotINDITelescopeSync(bool focused_object = true);
        void slotINDITelescopeSyncMousePointer();

        /** action slot: Abort any telescope motion (INDI) */
        void slotINDITelescopeAbort();

        /** action slot: Park the telescope (INDI) */
        void slotINDITelescopePark();

        /** action slot: Unpark the telescope (INDI) */
        void slotINDITelescopeUnpark();

        /** action slot: Park the dome (INDI) */
        void slotINDIDomePark();

        /** action slot: UnPark the dome (INDI) */
        void slotINDIDomeUnpark();

        /** action slot: open dialog for setting the view options */
        void slotViewOps();

        /** finish setting up after the kstarsData has finished */
        void datainitFinished();

        /** Open FITS image. */
        void slotOpenFITS();

        /** Action slot to save the sky image to a file.*/
        void slotExportImage();

        /** Action slot to select a DBUS script and run it.*/
        void slotRunScript();

        /** Action slot to print skymap. */
        void slotPrint();

        /** Action slot to start Printing Wizard. */
        void slotPrintingWizard();

        /** Action slot to show tip-of-the-day window. */
        void slotTipOfDay();

        /** Action slot to set focus coordinates manually (opens FocusDialog). */
        void slotManualFocus();

        /** Meta-slot to point the focus at special points (zenith, N, S, E, W).
             * Uses the name of the Action which sent the Signal to identify the
             * desired direction.  */
        void slotPointFocus();

        /** Meta-slot to set the color scheme according to the name of the
             * Action which sent the activating signal.  */
        void slotColorScheme();

        /**
         * @brief slotThemeChanged save theme name in options
         */
        void slotThemeChanged();

        /** Select the Target symbol (a.k.a. field-of-view indicator) */
        void slotTargetSymbol(bool flag);

        /** Select the HIPS Source catalog. */
        void slotHIPSSource();

        /** Invoke the Field-of-View symbol editor window */
        void slotFOVEdit();

        /** Toggle between Equatorial and Ecliptic coordinate systems */
        void slotCoordSys();

        /** Set the map projection according to the menu selection */
        void slotMapProjection();

        /** Toggle display of the observing list tool*/
        void slotObsList();

        /** Meta-slot to handle display toggles for all of the viewtoolbar buttons.
             * uses the name of the sender to identify the item to change.  */
        void slotViewToolBar();

        /** Meta-slot to handle display toggles for all of the INDItoolbar buttons.
             * uses the name of the sender to identify the item to change.  */
        void slotINDIToolBar();

        /** Meta-slot to handle toggling display of GUI elements (toolbars and infoboxes)
             * uses name of the sender action to identify the widget to hide/show.  */
        void slotShowGUIItem(bool);

        /** Toggle to and from full screen mode */
        void slotFullScreen();

        /** Toggle whether to show the terrain image on the skymap. */
        void slotTerrain();

        /** Save data to config file before exiting.*/
        void slotAboutToQuit();

        void slotEquipmentWriter();

        void slotObserverManager();

        void slotHorizonManager();

        void slotExecute();

        void slotPolarisHourAngle();

        /** Update comets orbital elements*/
        void slotUpdateComets(bool isAutoUpdate = false);

        /** Update asteroids orbital elements*/
        void slotUpdateAsteroids(bool isAutoUpdate = false);

        /** Update list of recent supernovae*/
        void slotUpdateSupernovae();

        /** Update satellites orbital elements*/
        void slotUpdateSatellites();

        /** Configure Notifications */
        void slotConfigureNotifications();

    private:
        /** Load FOV information and repopulate menu. */
        void repopulateFOV();

        /**
         * @brief populateThemes Populate application themes
         */
        void populateThemes();

        /** Initialize Menu bar, toolbars and all Actions. */
        void initActions();

        /** Prepare options dialog. */
        KConfigDialog* prepareOps();

        /** Initialize Status bar. */
        void initStatusBar();

        /** Initialize focus position */
        void initFocus();

        /** Build the KStars main window */
        void buildGUI();

        void closeEvent(QCloseEvent *event) override;

    public:
        /** Check if the KStars main window is shown */
        bool isGUIReady()
        {
            return m_SkyMap != nullptr;
        }

        /** Was KStars started with the clock running, or paused? */
        bool isStartedWithClockRunning()
        {
            return StartClockRunning;
        }

        /// Set to true when the application is being closed
        static bool Closing;

        /** @brief Override KStars UI resource file.
         * @note This is used by UI tests, which need to use the same resources with a different app name
         */
        static bool setResourceFile(QString const rc);

    private:
        /// Pointer to an instance of KStars
        static KStars *pinstance;

        // Resource file to load - overridable by UI tests
        static QString m_KStarsUIResource;

        KActionMenu *colorActionMenu { nullptr };
        KActionMenu *fovActionMenu { nullptr };
        KActionMenu *hipsActionMenu { nullptr };

        KStarsData *m_KStarsData { nullptr };
        SkyMap *m_SkyMap { nullptr };

        // Widgets
        TimeStepBox *m_TimeStepBox { nullptr };

        // Dialogs & Tools

        // File Menu
        ExportImageDialog *m_ExportImageDialog { nullptr };
        PrintingWizard *m_PrintingWizard { nullptr };

        // Tool Menu
        AstroCalc *m_AstroCalc { nullptr };
        AltVsTime *m_AltVsTime { nullptr };
        SkyCalendar *m_SkyCalendar { nullptr };
        ScriptBuilder *m_ScriptBuilder { nullptr };
        PlanetViewer *m_PlanetViewer { nullptr };
        WUTDialog *m_WUTDialog { nullptr };
        JMoonTool *m_JMoonTool { nullptr };
        FlagManager *m_FlagManager { nullptr };
        HorizonManager *m_HorizonManager { nullptr };
        EyepieceField *m_EyepieceView { nullptr };
#ifdef HAVE_CFITSIO
        QPointer<FITSViewer> m_GenericFITSViewer;
        QList<QPointer<FITSViewer>> m_FITSViewers;
#endif

#ifdef HAVE_INDI
        QPointer<Ekos::Manager> m_EkosManager;
#endif

        AddDeepSkyObject *m_addDSODialog { nullptr };

        // FIXME Port to QML2
        //#if 0
        WIView *m_WIView { nullptr };
        WILPSettings *m_WISettings { nullptr };
        WIEquipSettings *m_WIEquipmentSettings { nullptr };
        ObsConditions *m_ObsConditions { nullptr };
        QDockWidget *m_wiDock { nullptr };
        //#endif

        QActionGroup *projectionGroup { nullptr };
        QActionGroup *cschemeGroup { nullptr };
        QActionGroup *hipsGroup { nullptr };
        QActionGroup *telescopeGroup { nullptr };
        QActionGroup *domeGroup { nullptr };

        bool DialogIsObsolete { false };
        bool StartClockRunning { false };
        QString StartDateString;
        QLabel AltAzField, RADecField, J2000RADecField;
        //QPalette OriginalPalette, DarkPalette;

        OpsCatalog *opcatalog { nullptr };
        OpsGuides *opguides { nullptr };
        OpsTerrain *opterrain { nullptr };
        OpsSolarSystem *opsolsys { nullptr };
        OpsSatellites *opssatellites { nullptr };
        OpsSupernovae *opssupernovae { nullptr };
        OpsColors *opcolors { nullptr };
        OpsAdvanced *opadvanced { nullptr };
        OpsINDI *opsindi { nullptr };
        OpsEkos *opsekos { nullptr };
        OpsFITS *opsfits { nullptr };
        OpsXplanet *opsxplanet { nullptr };

        friend class TestArtificialHorizon;
};
