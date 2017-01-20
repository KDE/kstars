/** *************************************************************************
                          kstars.h  -  K Desktop Planetarium
                             -------------------
    begin                : Mon Feb  5 01:11:45 PST 2001
    copyright            : (C) 2001 by Jason Harris
    email                : jharris@30doradus.org
 ***************************************************************************/
/** *************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef KSTARS_H_
#define KSTARS_H_

#include <QtDBus/QtDBus>
#include <kxmlguiwindow.h>

#include <config-kstars.h>

#include "oal/equipmentwriter.h"
#include "oal/observeradd.h"

// forward declaration is enough. We only need pointers
class QPalette;
class KActionMenu;

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
//class JMoonTool;
class MoonPhaseTool;
class FlagManager;
class EquipmentWriter;
class ObserverAdd;
class Execute;
class ExportImageDialog;
class PrintingWizard;
class EkosManager;
class HorizonManager;
class EyepieceField;
class AddDeepSkyObject;

class OpsCatalog;
class OpsGuides;
class OpsSolarSystem;
class OpsSatellites;
class OpsSupernovae;
class OpsColors;
class OpsAdvanced;
class OpsINDI;
class OpsEkos;

#ifdef HAVE_XPLANET
class OpsXplanet;
#endif

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
    explicit KStars( bool doSplash, bool startClockRunning = true, const QString &startDateString = QString() );

    static KStars *pinstance; // Pointer to an instance of KStars

public:
    /**
     * @short Create an instance of this class. Destroy any previous instance
     * @param doSplash
     * @param clockrunning
     * @param startDateString
     * @note See KStars::KStars for details on parameters
     * @return a pointer to the instance
     */
    static KStars *createInstance( bool doSplash, bool clockrunning = true, const QString &startDateString = QString() );

    /** @return a pointer to the instance of this class */
    inline static KStars *Instance() { return pinstance; }

    /** Destructor.  Synchs config file.  Deletes objects. */
    virtual ~KStars();

    /** @return pointer to KStarsData object which contains application data. */
    inline KStarsData* data() const { return m_KStarsData; }

    /** @return pointer to SkyMap object which is the sky display widget. */
    inline SkyMap* map() const { return m_SkyMap; }

    inline FlagManager* flagManager() const { return m_FlagManager; }

    inline PrintingWizard* printingWizard() const { return m_PrintingWizard; }

    #ifdef HAVE_CFITSIO
    FITSViewer *genericFITSViewer();
    #endif

    #ifdef HAVE_INDI
    EkosManager *ekosManager();
    #endif

    /** Add an item to the color-scheme action manu
     * @param name The name to use in the menu
     * @param actionName The internal name for the action (derived from filename)
     */
    void addColorMenuItem( const QString &name, const QString &actionName );

    /** Remove an item from the color-scheme action manu
     * @param actionName The internal name of the action (derived from filename)
     */
    void removeColorMenuItem( const QString &actionName );

    /** @short Apply config options throughout the program.
     * In most cases, options are set in the "Options" object directly,
     * but for some things we have to manually react to config changes.
     * @param doApplyFocus If true, then focus posiiton will be set
     * from config file
     */
    void applyConfig( bool doApplyFocus = true );

    void showImgExportDialog();

    void syncFOVActions();

    void hideAllFovExceptFirst();

    void selectNextFov();

    void selectPreviousFov();

    void showWISettingsUI();

    void showWI(ObsConditions *obs);

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
    Q_SCRIPTABLE Q_NOREPLY void setRaDec( double ra, double dec );

    /** DBUS interface function.
     * Set focus to given Alt/Az coordinates.
     * @param alt the Altitude coordinate for the focus (in Degrees)
     * @param az the Azimuth coordinate for the focus (in Degrees)
     */
    Q_SCRIPTABLE Q_NOREPLY void setAltAz(double alt, double az);

    /** DBUS interface function.
     * Point in the direction described by the string argument.
     * @param direction either an object name, a compass direction (e.g., "north"), or "zenith"
     */
    Q_SCRIPTABLE Q_NOREPLY void lookTowards( const QString &direction );

    /** DBUS interface function.
     * Add a name label to the named object
     * @param name the name of the object to which the label will be attached
     */
    Q_SCRIPTABLE Q_NOREPLY void addLabel( const QString &name );

    /** DBUS interface function.
     * Remove a name label from the named object
     * @param name the name of the object from which the label will be removed
     */
    Q_SCRIPTABLE Q_NOREPLY void removeLabel( const QString &name );

    /** DBUS interface function.
     * Add a trail to the named solar system body
     * @param name the name of the body to which the trail will be attached
     */
    Q_SCRIPTABLE Q_NOREPLY void addTrail( const QString &name );

    /** DBUS interface function.
     * Remove a trail from the named solar system body
     * @param name the name of the object from which the trail will be removed
     */
    Q_SCRIPTABLE Q_NOREPLY void removeTrail( const QString &name );

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
    Q_SCRIPTABLE Q_NOREPLY void waitFor( double t );

    /** DBUS interface function.  Pause further DBUS execution until a key is pressed.
     * @param k the key which will resume DBUS execution
     */
    Q_SCRIPTABLE Q_NOREPLY void waitForKey( const QString &k );

    /** DBUS interface function.  Toggle tracking.
     * @param track engage tracking if true; else disengage tracking
     */
    Q_SCRIPTABLE Q_NOREPLY void setTracking( bool track );

    /** DBUS interface function.  modify a view option.
     * @param option the name of the option to be modified
     * @param value the option's new value
     */
    Q_SCRIPTABLE Q_NOREPLY void changeViewOption( const QString &option, const QString &value );

    /** DBUS interface function.
     * @param name the name of the option to query
     * @return the current value of the named option
     */
    Q_SCRIPTABLE QString getOption( const QString &name );

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
    Q_SCRIPTABLE Q_NOREPLY void popupMessage( int x, int y, const QString &message );

    /** DBUS interface function.  Draw a line on the sky map.
     * @note Not Yet Implemented
     * @param x1 starting x-coordinate of line
     * @param y1 starting y-coordinate of line
     * @param x2 ending x-coordinate of line
     * @param y2 ending y-coordinate of line
     * @param speed speed at which line should appear from start to end points (in pixels per second)
     */
    Q_SCRIPTABLE Q_NOREPLY void drawLine( int x1, int y1, int x2, int y2, int speed );

    /** DBUS interface function.  Set the geographic location.
     * @param city the city name of the location
     * @param province the province name of the location
     * @param country the country name of the location
     * @return True if geographic location is found and set, false otherwise.
     */
    Q_SCRIPTABLE bool setGeoLocation( const QString &city, const QString &province, const QString &country );

    /** DBUS interface function.  Modify a color.
     * @param colorName the name of the color to be modified (e.g., "SkyColor")
     * @param value the new color to use
     */
    Q_SCRIPTABLE Q_NOREPLY void setColor( const QString &colorName, const QString &value );

    /** DBUS interface function.  Load a color scheme.
     * @param name the name of the color scheme to load (e.g., "Moonless Night")
     */
    Q_SCRIPTABLE Q_NOREPLY void loadColorScheme( const QString &name );

    /** DBUS interface function.  Export the sky image to a file.
     * @param filename the filename for the exported image
     * @param width the width for the exported image. Map's width will be used if nothing or an invalid value is supplied.
     * @param height the height for the exported image. Map's height will be used if nothing or an invalid value is supplied.
     * @param includeLegend should we include a legend?
     */
    Q_SCRIPTABLE Q_NOREPLY void exportImage( const QString &filename, int width = -1, int height = -1, bool includeLegend = false );

    /** DBUS interface function.  Return a URL to retrieve Digitized Sky Survey image.
     * @param objectName name of the object.
     * @note If the object is note found, the string "ERROR" is returned.
     */
    Q_SCRIPTABLE QString getDSSURL( const QString &objectName );

    /** DBUS interface function.  Return a URL to retrieve Digitized Sky Survey image.
     * @param RA_J2000 J2000.0 RA
     * @param Dec_J2000 J2000.0 Declination
     * @param width width of the image, in arcminutes (default = 15)
     * @param height height of the image, in arcminutes (default = 15)
     */
    Q_SCRIPTABLE QString getDSSURL( double RA_J2000, double Dec_J2000, float width = 15, float height = 15 );

    /** DBUS interface function.  Return XML containing information about a sky object
     * @param objectName name of the object.
     * @note If the object was not found, the XML is empty.
     */
    Q_SCRIPTABLE QString getObjectDataXML( const QString &objectName );

    /** DBUS interface function.  Return XML containing position info about a sky object
     * @param objectName name of the object.
     * @note If the object was not found, the XML is empty.
     */
    Q_SCRIPTABLE QString getObjectPositionInfo( const QString &objectName );

    /** DBUS interface function. Render eyepiece view and save it in the file(s) specified
     * @note See EyepieceField::renderEyepieceView() for more info. This is a DBus proxy that calls that method, and then writes the resulting image(s) to file(s).
     * @note Important: If imagePath is empty, but overlay is true, or destPathImage is supplied, this method will make a blocking DSS download.
     */
    Q_SCRIPTABLE Q_NOREPLY void renderEyepieceView( const QString &objectName, const QString &destPathChart, const double fovWidth = -1.0, const double fovHeight = -1.0,
                                                   const double rotation = 0.0, const double scale = 1.0, const bool flip = false, const bool invert = false,
                                                   QString imagePath = QString(), const QString &destPathImage = QString(), const bool overlay = false,
                                                   const bool invertColors = false );

    /** DBUS interface function.  Set the approx field-of-view
     * @param FOV_Degrees field of view in degrees
     */
    Q_SCRIPTABLE Q_NOREPLY void setApproxFOV( double FOV_Degrees );

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
    Q_SCRIPTABLE Q_NOREPLY void printImage( bool usePrintDialog, bool useChartColors );

    /** DBUS interface function.  Open FITS image.
     * @param imageURL URL of FITS image to load. For a local file the prefix must be file:// For example
     * if the file is located at /home/john/m42.fits then the full URL is file:///home/john/m42.fits
     * @return True if successful, false otherwise.
     */
    Q_SCRIPTABLE bool openFITS(const QString & imageURL); // for DBus
    bool openFITS(const QUrl &imageUrl);                 // for C++ code

    /** @}*/

    /**
     * Update time-dependent data and (possibly) repaint the sky map.
     * @param automaticDSTchange change DST status automatically?
     */
    void updateTime( const bool automaticDSTchange = true );

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

    /** Delete FindDialog because ObjNames list has changed in KStarsData due to
     * reloading star data. So list in FindDialog must be new filled with current data. */
    void clearCachedFindDialog();

    /** Remove all trails which may have been added to solar system bodies */
    void slotClearAllTrails();

    /** Display position in the status bar. */
    void slotShowPositionBar(SkyPoint*);

    /** action slot: open Flag Manager */
    void slotFlagManager();

    /** Show the eyepiece view tool */
    void slotEyepieceView( SkyPoint *sp, const QString &imagePath = QString() );

    /** Show the add deep-sky object dialog */
    void slotAddDeepSkyObject();

    /** action slot: open KStars setup wizard */
    void slotWizard();

    void updateLocationFromWizard(GeoLocation geo);
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

    //FIXME Port to QML2
    //#if 0
    /** action slot: open What's Interesting settings window */
    void slotWISettings();

    /** action slot: open What's Interesting window */
    void slotShowWIView(int status);
    //#endif

    /** action slot: open Sky Calendar tool */
    void slotCalendar();

    /** action slot: open the glossary */
    void slotGlossary();

    /** action slot: open ScriptBuilder dialog */
    void slotScriptBuilder();

    /** action slot: open Solar system viewer */
    void slotSolarSystem();

    /** action slot: open Jupiter Moons tool */
//    void slotJMoonTool();

    /** action slot: open Moon Phase Calendar tool */
    void slotMoonPhaseTool();

    /** action slot: open Telescope wizard */
    void slotTelescopeWizard();

    /** action slot: open INDI driver panel */
    void slotINDIDriver();

    /** action slot: open INDI control panel */
    void slotINDIPanel();

    /** action slot: open Ekos panel */
    void slotEkos();

    /** action slot: open dialog for setting the view options */
    void slotViewOps();

    /** finish setting up after the kstarsData has finished
     */
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

    /** Select the Target symbol (a.k.a. field-of-view indicator) */
    void slotTargetSymbol(bool flag);

    /** Invoke the Field-of-View symbol editor window */
    void slotFOVEdit();

    /** Toggle between Equatorial and Ecliptic coordinte systems */
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
    void slotShowGUIItem( bool );

    /** Toggle to and from full screen mode */
    void slotFullScreen();

    /** Save data to config file before exiting.*/
    void slotAboutToQuit();

    void slotEquipmentWriter();

    void slotObserverManager();

    void slotHorizonManager();

    void slotExecute();

    /** Update comets orbital elements*/
    void slotUpdateComets();

    /** Update asteroids orbital elements*/
    void slotUpdateAsteroids();

    /** Update list of recent supernovae*/
    void slotUpdateSupernovae();

    /** Update satellites orbital elements*/
    void slotUpdateSatellites();

private:
    /** Load FOV information and repopulate menu. */
    void repopulateFOV();

    /** Initialize Menu bar, toolbars and all Actions. */
    void initActions();

    /** Initialize Status bar. */
    void initStatusBar();

    /** Initialize focus position */
    void initFocus();

    /** Build the KStars main window */
    void buildGUI();

    KActionMenu *colorActionMenu, *fovActionMenu;

    KStarsData *m_KStarsData;
    SkyMap *m_SkyMap;

    // Widgets
    TimeStepBox *m_TimeStepBox;

    // Dialogs & Tools

    // File Menu
    ExportImageDialog *m_ExportImageDialog;
    PrintingWizard *m_PrintingWizard;

    // Pointing Menu
    FindDialog *m_FindDialog;

    // Tool Menu
    AstroCalc *m_AstroCalc;
    AltVsTime *m_AltVsTime;
    SkyCalendar *m_SkyCalendar;
    ScriptBuilder *m_ScriptBuilder;
    PlanetViewer *m_PlanetViewer;
    WUTDialog *m_WUTDialog;
//    JMoonTool *m_JMoonTool;
    MoonPhaseTool *m_MoonPhaseTool;
    FlagManager *m_FlagManager;
    HorizonManager *m_HorizonManager;
    EyepieceField *m_EyepieceView;
    #ifdef HAVE_CFITSIO
    QPointer<FITSViewer> m_GenericFITSViewer;
    #endif

    #ifdef HAVE_INDI
    QPointer<EkosManager> m_EkosManager;
    #endif

    AddDeepSkyObject *m_addDSODialog;

    // FIXME Port to QML2
    //#if 0
    WIView *m_WIView;
    WILPSettings *m_WISettings;
    WIEquipSettings *m_WIEquipmentSettings;
    ObsConditions *m_ObsConditions;
    QDockWidget *m_wiDock;
    //#endif


    QActionGroup *projectionGroup, *cschemeGroup;

    bool DialogIsObsolete;
    bool StartClockRunning;
    QString StartDateString;
    QLabel AltAzField, RADecField, J2000RADecField;
    QPalette OriginalPalette, DarkPalette;

    OpsCatalog *opcatalog;
    OpsGuides *opguides;
    OpsSolarSystem *opsolsys;
    OpsSatellites *opssatellites;
    OpsSupernovae *opssupernovae;
    OpsColors *opcolors;
    OpsAdvanced *opadvanced;
    OpsINDI *opsindi;
    OpsEkos *opsekos;
#ifdef HAVE_XPLANET
    OpsXplanet  *opsxplanet;
#endif
};

#endif
