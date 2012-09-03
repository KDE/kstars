/***************************************************************************
                          kstars.h  -  K Desktop Planetarium
                             -------------------
    begin                : Mon Feb  5 01:11:45 PST 2001
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

#ifndef KSTARS_H_
#define KSTARS_H_

#include <QtDBus/QtDBus>
#include <kxmlguiwindow.h>
#include <QtDeclarative/QDeclarativeView>

#include <config-kstars.h>

#include "tools/observinglist.h"
#include "oal/equipmentwriter.h"
#include "oal/observeradd.h"

class WIUserSettings;
// forward declaration is enough. We only need pointers
class QPalette;
class KActionMenu;

class dms;
class KStarsData;
class SkyMap;
class GeoLocation;
class FindDialog;
class TimeStepBox;

class GUIManager;
class DriverManager;

class AltVsTime;
class WUTDialog;
class WIView;
class WIUserSettings;
class ObsConditions;
class AstroCalc;
class SkyCalendar;
class ScriptBuilder;
class PlanetViewer;
class JMoonTool;
class FlagManager;
class ObservingList;
class EquipmentWriter;
class ObserverAdd;
class Execute;
class ExportImageDialog;
class PrintingWizard;

class OpsCatalog;
class OpsGuides;
class OpsSolarSystem;
class OpsSatellites;
class OpsSupernovae;
class OpsColors;
class OpsAdvanced;
class OpsINDI;
class EkosManager;
#ifdef HAVE_XPLANET
class OpsXplanet;
#endif

/**
 *@class KStars
 *@short This is the main window for KStars.
 *In addition to the GUI elements, the class contains the program clock,
 *KStarsData, and SkyMap objects.  It also contains functions for the D-Bus interface.
 *@author Jason Harris
 *@version 1.0
 */

// KStars is now a singleton class. Use KStars::createInstance() to
// create an instance and KStars::Instance() to get a pointer to the
// instance
// asimha, 9th May 2009

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

    /**Destructor.  Synchs config file.  Deletes objects. */
    virtual ~KStars();

    /**@return pointer to KStarsData object which contains application data. */
    KStarsData* data() { return kstarsData; }

    /**@return pointer to SkyMap object which is the sky display widget. */
    SkyMap* map() { return skymap; }

    ObservingList* observingList() { return obsList; }

    Execute* getExecute();

    /**Add an item to the color-scheme action manu
     * @param name The name to use in the menu
     * @param actionName The internal name for the action (derived from filename)
     */
    void addColorMenuItem( const QString &name, const QString &actionName );

    /**Remove an item from the color-scheme action manu
     * @param actionName The internal name of the action (derived from filename)
     */
    void removeColorMenuItem( const QString &actionName );

    /**@short Apply config options throughout the program.
     * In most cases, options are set in the "Options" object directly,
     * but for some things we have to manually react to config changes.
     * @param doApplyFocus If true, then focus posiiton will be set
     * from config file
     */
    void applyConfig( bool doApplyFocus = true );

    FlagManager* getFlagManager() { return fm; }

    PrintingWizard* getPrintingWizard() { return printingWizard; }

    void showImgExportDialog();

    void syncFOVActions();

    void hideAllFovExceptFirst();

    void selectNextFov();

    void selectPreviousFov();

    void showWIWizard();

    void showWI(ObsConditions *obs);

public Q_SLOTS:
    /**DBUS interface function.
     * Set focus to given Ra/Dec coordinates
     * @param ra the Right Ascension coordinate for the focus (in Hours)
     * @param dec the Declination coordinate for the focus (in Degrees)
     */
    Q_SCRIPTABLE Q_NOREPLY void setRaDec( double ra, double dec );

    /**DBUS interface function.
     * Set focus to given Alt/Az coordinates.
     * @param alt the Altitude coordinate for the focus (in Degrees)
     * @param az the Azimuth coordinate for the focus (in Degrees)
     */
    Q_SCRIPTABLE Q_NOREPLY void setAltAz(double alt, double az);

    /**DBUS interface function.
     * Point in the direction described by the string argument.
     * @param direction either an object name, a compass direction (e.g., "north"), or "zenith"
     */
    Q_SCRIPTABLE Q_NOREPLY void lookTowards( const QString &direction );

    /**DBUS interface function.
     * Add a name label to the named object
     * @param name the name of the object to which the label will be attached
     */
    Q_SCRIPTABLE Q_NOREPLY void addLabel( const QString &name );

    /**DBUS interface function.
     * Remove a name label from the named object
     * @param name the name of the object from which the label will be removed
     */
    Q_SCRIPTABLE Q_NOREPLY void removeLabel( const QString &name );

    /**DBUS interface function.
     * Add a trail to the named solar system body
     * @param name the name of the body to which the trail will be attached
     */
    Q_SCRIPTABLE Q_NOREPLY void addTrail( const QString &name );

    /**DBUS interface function.
     * Remove a trail from the named solar system body
     * @param name the name of the object from which the trail will be removed
     */
    Q_SCRIPTABLE Q_NOREPLY void removeTrail( const QString &name );

    /**DBUS interface function.  Zoom in one step. */
    Q_SCRIPTABLE Q_NOREPLY void zoomIn();

    /**DBUS interface function.  Zoom out one step. */
    Q_SCRIPTABLE Q_NOREPLY void zoomOut();

    /**DBUS interface function.  reset to the default zoom level. */
    Q_SCRIPTABLE Q_NOREPLY void defaultZoom();

    /** DBUS interface function. Set zoom level to specified value.
     *  @param z the zoom level. Units are pixels per radian. */
    Q_SCRIPTABLE Q_NOREPLY void zoom(double z);

    /**DBUS interface function.  Set local time and date.
     * @param yr year of date
     * @param mth month of date
     * @param day day of date
     * @param hr hour of time
     * @param min minute of time
     * @param sec second of time
     */
    Q_SCRIPTABLE Q_NOREPLY void setLocalTime(int yr, int mth, int day, int hr, int min, int sec);

    /**DBUS interface function.  Delay further execution of DBUS commands.
     * @param t number of seconds to delay
     */
    Q_SCRIPTABLE Q_NOREPLY void waitFor( double t );

    /**DBUS interface function.  Pause further DBUS execution until a key is pressed.
     * @param k the key which will resume DBUS execution
     */
    Q_SCRIPTABLE Q_NOREPLY void waitForKey( const QString &k );

    /**DBUS interface function.  Toggle tracking.
     * @param track engage tracking if true; else disengage tracking
     */
    Q_SCRIPTABLE Q_NOREPLY void setTracking( bool track );

    /**DBUS interface function.  modify a view option.
     * @param option the name of the option to be modified
     * @param value the option's new value
     */
    Q_SCRIPTABLE Q_NOREPLY void changeViewOption( const QString &option, const QString &value );

    /**DBUS interface function.
     * @param name the name of the option to query
     * @return the current value of the named option
     */
    QString getOption( const QString &name );

    /**DBUS interface function.  Read config file.
     * This function is useful for restoring the user settings from the config file,
     * after having modified the settings in memory.
     * @sa writeConfig()
     */
    Q_SCRIPTABLE Q_NOREPLY void readConfig();

    /**DBUS interface function.  Write current settings to config file.
     * This function is useful for storing user settings before modifying them with a DBUS
     * script.  The original settings can be restored with readConfig().
     * @sa readConfig()
     */
    Q_SCRIPTABLE Q_NOREPLY void writeConfig();

    /**DBUS interface function.  Show text message in a popup window.
     * @note Not Yet Implemented
     * @param x x-coordinate for message window
     * @param y y-coordinate for message window
     * @param message the text to display in the message window
     */
    Q_SCRIPTABLE Q_NOREPLY void popupMessage( int x, int y, const QString &message );

    /**DBUS interface function.  Draw a line on the sky map.
     * @note Not Yet Implemented
     * @param x1 starting x-coordinate of line
     * @param y1 starting y-coordinate of line
     * @param x2 ending x-coordinate of line
     * @param y2 ending y-coordinate of line
     * @param speed speed at which line should appear from start to end points (in pixels per second)
     */
    Q_SCRIPTABLE Q_NOREPLY void drawLine( int x1, int y1, int x2, int y2, int speed );

    /**DBUS interface function.  Set the geographic location.
     * @param city the city name of the location
     * @param province the province name of the location
     * @param country the country name of the location
     */
    Q_SCRIPTABLE Q_NOREPLY void setGeoLocation( const QString &city, const QString &province, const QString &country );

    /**DBUS interface function.  Modify a color.
     * @param colorName the name of the color to be modified (e.g., "SkyColor")
     * @param value the new color to use
     */
    Q_SCRIPTABLE Q_NOREPLY void setColor( const QString &colorName, const QString &value );

    /**DBUS interface function.  Load a color scheme.
     * @param name the name of the color scheme to load (e.g., "Moonless Night")
     */
    Q_SCRIPTABLE Q_NOREPLY void loadColorScheme( const QString &name );

    /**DBUS interface function.  Export the sky image to a file.
     * @param filename the filename for the exported image
     * @param width the width for the exported image
     * @param height the height for the exported image
     */
    Q_SCRIPTABLE Q_NOREPLY void exportImage( const QString &filename, int width, int height );

    /**DBUS interface function.  Print the sky image.
     * @param usePrintDialog if true, the KDE print dialog will be shown; otherwise, default parameters will be used
     * @param useChartColors if true, the "Star Chart" color scheme will be used for the printout, which will save ink.
     */
    Q_SCRIPTABLE Q_NOREPLY void printImage( bool usePrintDialog, bool useChartColors );

    // TODO INDI Scripting to be supported in KDE 4.1
    #if 0
    /**DBUS interface function.  Establish an INDI driver.
     * @param deviceName The INDI device name
     * @param useLocal establish driver locally?
     */
    Q_SCRIPTABLE Q_NOREPLY void startINDI (const QString &deviceName, bool useLocal);

    /**DBUS interface function. Set current device. All subsequent functions will
     * communicate with this device until changed.
     * @param deviceName The INDI device name
     */
    Q_SCRIPTABLE Q_NOREPLY void setINDIDevice (const QString &deviceName);

    /**DBUS interface function. Shutdown an INDI driver.
     * @param driverName the name of the driver to be shut down
     */
    Q_SCRIPTABLE Q_NOREPLY void shutdownINDI (const QString &driverName);

    /**DBUS interface function.  Turn INDI driver on/off.
     * @param turnOn if true, turn driver on; otherwise turn off
     */
    Q_SCRIPTABLE Q_NOREPLY void switchINDI(bool turnOn);

    /**DBUS interface function.  Set INDI connection port.
     * @param port the port identifier
     */
    Q_SCRIPTABLE Q_NOREPLY void setINDIPort(const QString &port);

    /**DBUS interface function.  Set INDI target RA/DEC coordinates
     * @param RA the target's Right Ascension coordinate (in Hours)
     * @param DEC the target's Declination coordinate (in Degrees)
     */
    Q_SCRIPTABLE Q_NOREPLY void setINDITargetCoord(double RA, double DEC);

    /**DBUS interface function.  Set INDI target to a named object.
     * @param objectName the name of the object to be targeted
     */
    Q_SCRIPTABLE Q_NOREPLY void setINDITargetName(const QString &objectName);

    /**DBUS interface function.  Set INDI action.
     * @param action the action to set
     */
    Q_SCRIPTABLE Q_NOREPLY void setINDIAction(const QString &action);

    /**DBUS interface function.  Pause DBUS execution until named INDI action is completed.
     * @param action the action which is to be completed before resuming DBUS execution
     */
    Q_SCRIPTABLE Q_NOREPLY void waitForINDIAction(const QString &action);

    /**DBUS interface function.  Set INDI focus speed.
     * @param speed the speed to use
     *
     *  @todo document units for speed
     */
    Q_SCRIPTABLE Q_NOREPLY void setINDIFocusSpeed(unsigned int speed);

    /**DBUS interface function.  Set INDI focus direction and focus.
     * @param focusDir 0 = focus in; 1 = focus out
     */
    Q_SCRIPTABLE Q_NOREPLY void startINDIFocus(int focusDir);

    /**DBUS interface function.  Set INDI geographical information.
     * @param longitude the longitude to set, in Degrees
     * @param latitude the latitude to set, in Degrees
     */
    Q_SCRIPTABLE Q_NOREPLY void setINDIGeoLocation(double longitude, double latitude);

    /**DBUS interface function.  Sets focus operation timeout.
     * @param timeout the timeout interval, in seconds (?)
     */
    Q_SCRIPTABLE Q_NOREPLY void setINDIFocusTimeout(int timeout);

    /**DBUS interface function.  Start camera exposure with a timeout.
     * @param timeout the exposure time, in seconds (?)
     */
    Q_SCRIPTABLE Q_NOREPLY void startINDIExposure(int timeout);

    /**DBUS interface function.  Set INDI UTC date and time.
     * @param UTCDateTime the UTC date and time (e.g., "23 June 2004 12:30:00" ?)
     */
    Q_SCRIPTABLE Q_NOREPLY void setINDIUTC(const QString &UTCDateTime);

    /**DBUS interface function. Set INDI Telescope action.
     * @param action the action to set
     */
    Q_SCRIPTABLE Q_NOREPLY void setINDIScopeAction(const QString &action);

    /**DBUS interface function. Set CCD camera frame type.
     * @param type the frame type
     */
    Q_SCRIPTABLE Q_NOREPLY void setINDIFrameType(const QString &type);

    /**DBUS interface function. Set CCD filter.
     * @param filter_num identifier of the CCD filter
     */
    Q_SCRIPTABLE Q_NOREPLY void setINDIFilterNum(int filter_num);

    /**DBUS interface function. Set CCD target temperature.
     * @param temp the target CCD temperature (in Celsius ?)
     */
    Q_SCRIPTABLE Q_NOREPLY void setINDICCDTemp(int temp);

    #endif

    /**
     * Update time-dependent data and (possibly) repaint the sky map.
     * @param automaticDSTchange change DST status automatically?
     */
    void updateTime( const bool automaticDSTchange = true );

    /** action slot: sync kstars clock to system time */
    void slotSetTimeToNow();

    /** Apply new settings and redraw skymap */
    void slotApplyConfigChanges();

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

    /**Delete FindDialog because ObjNames list has changed in KStarsData due to
     * reloading star data. So list in FindDialog must be new filled with current data. */
    void clearCachedFindDialog();

    /** Remove all trails which may have been added to solar system bodies */
    void slotClearAllTrails();

    /** Display position in the status bar. */
    void slotShowPositionBar(SkyPoint*);

    /** action slot: open Flag Manager */
    void slotFlagManager();

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

    /** action slot: open KStars setup wizard */
    void slotWizard();

    /** action slot: open KNewStuff window to download extra data. */
    void slotDownload();

    /** action slot: open KStars calculator to compute astronomical ephemeris */
    void slotCalculator();

    /** action slot: open Elevation vs. Time tool */
    void slotAVT();

    /** action slot: open What's up tonight dialog */
    void slotWUT();

    /** action slot: open What's Interesting window */
    void slotWI();

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

    /**Action slot to save the sky image to a file.*/
    void slotExportImage();

    /**Action slot to select a DBUS script and run it.*/
    void slotRunScript();

    /**Action slot to print skymap. */
    void slotPrint();

    /**Action slot to start Printing Wizard. */
    void slotPrintingWizard();

    /**Action slot to show tip-of-the-day window. */
    void slotTipOfDay();

    /**Action slot to set focus coordinates manually (opens FocusDialog). */
    void slotManualFocus();

    /**Meta-slot to point the focus at special points (zenith, N, S, E, W).
     * Uses the name of the Action which sent the Signal to identify the
     * desired direction.  */
    void slotPointFocus();

    /**Meta-slot to set the color scheme according to the name of the
     * Action which sent the activating signal.  */
    void slotColorScheme();

    /**Select the Target symbol (a.k.a. field-of-view indicator) */
    void slotTargetSymbol(bool flag);

    /**Invoke the Field-of-View symbol editor window */
    void slotFOVEdit();

    /**Toggle between Equatorial and Ecliptic coordinte systems */
    void slotCoordSys();

    /**Set the map projection according to the menu selection */
    void slotMapProjection();

    /**Toggle display of the observing list tool*/
    void slotObsList();

    /**Meta-slot to handle display toggles for all of the viewtoolbar buttons.
     * uses the name of the sender to identify the item to change.  */
    void slotViewToolBar();

    /**Meta-slot to handle toggling display of GUI elements (toolbars and infoboxes)
     * uses name of the sender action to identify the widget to hide/show.  */
    void slotShowGUIItem( bool );

    /**Toggle to and from full screen mode */
    void slotFullScreen();

    /**Save data to config file before exiting.*/
    void slotAboutToQuit();

    void slotEquipmentWriter();

    void slotObserverAdd();

    void slotExecute();

    /**Update comets orbital elements*/
    void slotUpdateComets();

    /**Update asteroids orbital elements*/
    void slotUpdateAsteroids();

    /**Update list of recent supernovae*/
    void slotUpdateSupernovae();

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

    KStarsData *kstarsData;
    SkyMap *skymap;

    TimeStepBox *TimeStep;

    KActionMenu *colorActionMenu, *fovActionMenu;

    FindDialog *findDialog;
    ExportImageDialog *imgExportDialog;

    //FIXME: move to KStarsData
    ObservingList *obsList;
    EquipmentWriter *eWriter;
    ObserverAdd *oAdd;
    Execute *execute;
    AltVsTime *avt;
    WUTDialog *wut;
    WIView *wi;
    WIUserSettings *wiWiz;
    QDockWidget *wiDock;
    SkyCalendar *skycal;
    ScriptBuilder *sb;
    PlanetViewer *pv;
    JMoonTool *jmt;
    FlagManager *fm;
    AstroCalc *astrocalc;
    PrintingWizard *printingWizard;

    EkosManager *ekosmenu;

    QActionGroup *projectionGroup, *cschemeGroup;

    bool DialogIsObsolete;
    bool StartClockRunning;
    QString StartDateString;

    QPalette OriginalPalette, DarkPalette;

    OpsCatalog *opcatalog;
    OpsGuides *opguides;
    OpsSolarSystem *opsolsys;
    OpsSatellites *opssatellites;
    OpsSupernovae *opssupernovae;
    OpsColors *opcolors;
    OpsAdvanced *opadvanced;
    OpsINDI *opsindi;
#ifdef HAVE_XPLANET
    OpsXplanet  *opsxplanet;
#endif
};

#endif
