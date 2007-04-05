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

#ifndef KSTARS_H
#define KSTARS_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <dcopclient.h>
#include <kapplication.h>
#include <kmainwindow.h>
#include <qwidget.h>

#include "kstarsinterface.h"
#include "observinglist.h"

// forward declaration is enough. We only need pointers
class QPalette;
class QVBoxLayout;
class KActionMenu;
class KDialogBase;
class KKey;
class KToolBar;

class dms;
class KSNewStuff;
class KStarsData;
class KStarsSplash;
class SkyMap;
class GeoLocation;
class FindDialog;
class LocationDialog;
class TimeDialog;
class InfoBoxes;
class ToggleAction;
class TimeStepBox;

//class AstroCalc;
class INDIMenu;
class INDIDriver;
class imagesequence;

/**
	*@class KStars
	*@short This is the main window for KStars.
	*In addition to the GUI elements, the class contains the program clock,
	*KStarsData, and SkyMap objects.  It also contains functions for the DCOP interface.
	*@author Jason Harris
	*@version 1.0
	*/

class KStars : public KMainWindow, virtual public KStarsInterface
{

  Q_OBJECT
  public:
	/**
		*@short Constructor.
		*@param doSplash should the splash panel be displayed during
		*initialization.
		*@param startClockRunning should the clock be running on startup?
		*@param startDateString date (in string representation) to start running from.
		*
		* @todo Refer to documentation on date format.
		*/
		KStars( bool doSplash, bool startClockRunning = true, const QString &startDateString = "" );

	/**Destructor.  Synchs config file.  Deletes objects.
		*/
		~KStars();

	/**@return pointer to KStarsData object which contains application data.
		*/
		KStarsData* data();

	/**@return pointer to the local sidereal time.
		*/
		dms* LST();

	/**@return pointer to SkyMap object which is the sky display widget.
		*/
		SkyMap* map();

        ObservingList* observingList();

	/**@return pointer to GeoLocation object which is the current geographic location.
		*/
		GeoLocation* geo();

	/**@return pointer to InfoBoxes object.
		*/
		InfoBoxes* infoBoxes();

	/**@return pointer to the INDI driver
		*/
		INDIDriver* getINDIDriver(void) { return indidriver; }

	/**@return pointer to the INDI menu
		*/
		INDIMenu* getINDIMenu(void) { return indimenu; }

	/** Establish the INDI system. No GUI
		*/
		void establishINDI();

	/**Add an item to the color-scheme action manu
		*@param name The name to use in the menu
		*@param actionName The internal name for the action (derived from filename)
		*/
		void addColorMenuItem( QString name, QString actionName );

	/**Remove an item from the color-scheme action manu
		*@param actionName The internal name of the action (derived from filename)
		*/
		void removeColorMenuItem( QString actionName );

	/**DCOP interface function.  
		*Set focus to given Ra/Dec coordinates 
		*@p ra the Right Ascension coordinate for the focus (in Hours)
		*@p dec the Declination coordinate for the focus (in Degrees)
		*/
		ASYNC setRaDec( double ra, double dec );

	/**DCOP interface function.  
		*Set focus to given Alt/Az coordinates. 
		*@p alt the Altitude coordinate for the focus (in Degrees)
		*@p az the Azimuth coordinate for the focus (in Degrees)
		*/
		ASYNC setAltAz(double alt, double az);

	/**DCOP interface function.
		*Point in the direction described by the string argument.  
		*@p direction either an object name, a compass direction (e.g., "north"), or "zenith"
		*/
		ASYNC lookTowards( const QString direction );

	/**DCOP interface function.  Zoom in one step. */
		ASYNC zoomIn(void) { slotZoomIn(); }

	/**DCOP interface function.  Zoom out one step. */
		ASYNC zoomOut(void){ slotZoomOut(); }

	/**DCOP interface function.  reset to the default zoom level. */
		ASYNC defaultZoom(void) { slotDefaultZoom(); }

	/**DCOP interface function.  Set zoom level to specified value. 
		*@p a the zoom level.  Units are pixels per radian.
		*/
		ASYNC zoom(double z);

	/**DCOP interface function.  Set local time and date. 
		*@p yr year of date
		*@p mth month of date
		*@p day day of date
		*@p hr hour of time
		*@p min minute of time
		*@p sec second of time
		*/
		ASYNC setLocalTime(int yr, int mth, int day, int hr, int min, int sec);

	/**DCOP interface function.  Delay further execution of DCOP commands. 
		*@p t number of seconds to delay
		*/
		ASYNC waitFor( double t );

	/**DCOP interface function.  Pause further DCOP execution until a key is pressed. 
		*@p k the key which will resume DCOP execution
		*/
		ASYNC waitForKey( const QString k );

	/**DCOP interface function.  Toggle tracking. 
		*@p track engage tracking if true; else disengage tracking
		*/
		ASYNC setTracking( bool track );

	/**DCOP interface function.  modify a view option. 
		*@p option the name of the option to be modified
		*@p value the option's new value
		*/
		ASYNC changeViewOption( const QString option, const QString value );

	/**DCOP interface function.
		*@p name the name of the option to query
		*@return the current value of the named option
		*/
		QString getOption( const QString &name );

	/**DCOP interface function.  Read config file. 
		*This function is useful for restoring the user settings from the config file, 
		*after having modified the settings in memory.
		*@sa writeConfig()
		*/
		ASYNC readConfig();

	/**DCOP interface function.  Write current settings to config file. 
		*This function is useful for storing user settings before modifying them with a DCOP
		*script.  The original settings can be restored with readConfig().
		*@sa readConfig()
		*/
		ASYNC writeConfig();

	/**DCOP interface function.  Show text message in a popup window. 
		*@note Not Yet Implemented
		*@p x x-coordinate for message window
		*@p y y-coordinate for message window
		*@p message the text to display in the message window
		*/
		ASYNC popupMessage( int x, int y, const QString message );

	/**DCOP interface function.  Draw a line on the sky map. 
		*@note Not Yet Implemented
		*@p x1 starting x-coordinate of line
		*@p y1 starting y-coordinate of line
		*@p x2 ending x-coordinate of line
		*@p y2 ending y-coordinate of line
		*@p speed speed at which line should appear from start to end points (in pixels per second)
		*/
		ASYNC drawLine( int x1, int y1, int x2, int y2, int speed );

	/**DCOP interface function.  Set the geographic location. 
		*@p city the city name of the location
		*@p province the province name of the location
		*@p country the country name of the location
		*/
		ASYNC setGeoLocation( const QString city, const QString province, const QString country );

	/**DCOP interface function.  Modify a color. 
		*@p colorName the name of the color to be modified (e.g., "SkyColor")
		*@p value the new color to use
		*/
		ASYNC setColor( const QString colorName, const QString value );

	/**DCOP interface function.  Load a color scheme. 
		*@p name the name of the color scheme to load (e.g., "Moonless Night")
		*/
		ASYNC loadColorScheme( const QString name );

	/**DCOP interface function.  Export the sky image to a file. 
		*@p filename the filename for the exported image
		*@p width the width for the exported image
		*@p height the height for the exported image
		*/
		ASYNC exportImage( const QString filename, int width, int height );

	/**DCOP interface function.  Print the sky image. 
		*@p usePrintDialog if true, the KDE print dialog will be shown; otherwise, default parameters will be used
		*@p useChartColors if true, the "Star Chart" color scheme will be used for the printout, which will save ink.
		*/
		ASYNC printImage( bool usePrintDialog, bool useChartColors );
		
	/**DCOP interface function.  Establish an INDI driver. 
		*@p driverName the name of the driver to be established
		*@p useLocal establish driver locally?
		*/
		ASYNC startINDI (QString driverName, bool useLocal);
		
	/**DCOP interface function. Shutdown an INDI driver. 
		*@p driverName the name of the driver to be shut down
		*/
		ASYNC shutdownINDI (QString driverName);
		
	/**DCOP interface function.  Turn INDI driver on/off. 
		*@p driverName the name of the driver to be switched on/off
		*@p turnOn if true, turn driver on; otherwise turn off
		*/
		ASYNC switchINDI(QString driverName, bool turnOn);
	
	/**DCOP interface function.  Set INDI connection port. 
		*@p driverName the name of the driver for which the port will be set
		*@p port the port identifier
		*/
		ASYNC setINDIPort(QString driverName, QString port);
	
	/**DCOP interface function.  Set INDI target RA/DEC coordinates
		*@p driverName the name of the driver 
		*@p RA the target's Right Ascension coordinate (in Hours) 
		*@p DEC the target's Declination coordinate (in Degrees) 
		*/
		ASYNC setINDITargetCoord(QString driverName, double RA, double DEC);
	
	/**DCOP interface function.  Set INDI target to a named object. 
		*@p driverName the name of the driver 
		*@p objectName the name of the object to be targeted
		*/
		ASYNC setINDITargetName(QString driverName, QString objectName);
	
	/**DCOP interface function.  Set INDI action. 
		*@p driverName the name of the driver 
		*@p action the action to set
		*/
		ASYNC setINDIAction(QString driverName, QString action);
	
	/**DCOP interface function.  Pause DCOP execution until named INDI action is completed. 
		*@p driverName the name of the driver 
		*@p action the action which is to be completed before resuming DCOP execution
		*/
		ASYNC waitForINDIAction(QString driverName, QString action);
	
	/**DCOP interface function.  Set INDI focus speed. 
		*@p driverName the name of the driver 
		*@p action the name of the action (??)
		*/
		ASYNC setINDIFocusSpeed(QString driverName,unsigned int speed);
	
	/**DCOP interface function.  Set INDI focus direction and focus. 
		*@p driverName the name of the driver 
		*@p focusDir 0 = focus in; 1 = focus out
		*/
		ASYNC startINDIFocus(QString driverName, int focusDir);
	
	/**DCOP interface function.  Set INDI geographical information. 
		*@p driverName the name of the driver 
		*@p longitude the longitude to set, in Degrees
		*@p latitude the latitude to set, in Degrees
		*/
		ASYNC setINDIGeoLocation(QString driverName, double longitude, double latitude);
	
	/**DCOP interface function.  Sets focus operation timeout. 
		*@p driverName the name of the driver 
		*@p timeout the timeout interval, in seconds (?)
		*/
		ASYNC setINDIFocusTimeout(QString driverName, int timeout);
	
	/**DCOP interface function.  Start camera exposure with a timeout. 
		*@p driverName the name of the driver 
		*@p timeout the exposure time, in seconds (?)
		*/
		ASYNC startINDIExposure(QString driverName, int timeout);
		
	/**DCOP interface function.  Set INDI UTC date and time. 
		*@p driverName the name of the driver 
		*@p UTCDateTime the UTC date and time (e.g., "23 June 2004 12:30:00" ?)
		*/
		ASYNC setINDIUTC(QString driverName, QString UTCDateTime);
	
	/**DCOP interface function. Set INDI Telescope action. 
		*@p deviceName the name of the telescope device 
		*@p action the action to set
		*/
		ASYNC setINDIScopeAction(QString deviceName, QString action);
		
	/**DCOP interface function. Set CCD camera frame type. 
		*@p deviceName the name of the CCD device 
		*@p type the frame type
		*/
		ASYNC setINDIFrameType(QString deviceName, QString type);
		
	/**DCOP interface function. Set CCD filter. 
		*@p deviceName the name of the CCD device 
		*@p filter_num identifier of the CCD filter
		*/
		ASYNC setINDIFilterNum(QString deviceName, int filter_num);

	/**DCOP interface function. Set CCD target temperature. 
		*@p deviceName the name of the CCD device 
		*@p temp the target CCD temperature (in Celsius ?)
		*/
		ASYNC setINDICCDTemp(QString deviceName, int temp);
		

	/**@short Apply config options throughout the program.
		*In most cases, options are set in the "Options" object directly, 
		*but for some things we have to manually react to config changes.
		*/
		void applyConfig();

	public slots:
		/**
			*Update time-dependent data and (possibly) repaint the sky map.
			*@p automaticDSTchange change DST status automatically?
			*/
		void updateTime( const bool automaticDSTchange = true );

		/**
			*Apply new settings and redraw skymap
			*/
		void slotApplyConfigChanges( void );

		/**
			*action slot: Zoom in one step
			*/
		void slotZoomIn();

		/**
			*action slot: Zoom out one step
			*/
		void slotZoomOut();

		/**
			*action slot: Set the zoom level to its default value
			*/
		void slotDefaultZoom();

		/**
			*action slot: Allow user to specify a field-of-view angle for the display window in degrees, 
			*and set the zoom level accordingly.
			*/
		void slotSetZoom();

	/**
		*action slot: Toggle whether kstars is tracking current position
		*/
		void slotTrack();

	/**
		*action slot: open dialog for selecting a new geographic location
		*/
		void slotGeoLocator();

	/**Delete FindDialog because ObjNames list has changed in KStarsData due to
		*reloading star data. So list in FindDialog must be new filled with current data.
		*/
		void clearCachedFindDialog();

	/**
		*Resume execution of DCOP commands
		*/
		void resumeDCOP( void ) { kapp->dcopClient()->resume(); }

	/**
		*Remove all trails which may have been added to solar system bodies
		*/
		void slotClearAllTrails();

	private slots:
		/**
			*action slot: sync kstars clock to system time
			*/
		void slotSetTimeToNow();

		/**
			*action slot: open a dialog for setting the time and date
			*/
		void slotSetTime();

		/**
			*action slot: toggle whether kstars clock is running or not
			*/
		void slotToggleTimer();

		/**
			*action slot: open dialog for finding a named object
			*/
		void slotFind();

		/**
		 * action slot: open KStars setup wizard
		 */
		void slotWizard();

		/**
		 * action slot: open KNewStuff window to download extra data.
		 */
		void slotDownload();

		/**
		 * action slot: open KStars calculator to compute astronomical
		 * ephemeris
		 */

		void slotCalculator();

		/**
		 * action slot: open KStars AAVSO Light Curve Generator
		 */

		void slotLCGenerator();

		/**
		* action slot: open Elevation vs. Time tool
		*/

		void slotAVT();

		/**
		 * action slot: open What's up tonight dialog
		 */
		 void slotWUT();

//FIXME GLOSSARY
//		 /**
//		  * action slot: open the glossary
//		  */
//		 void slotGlossary();

		/**
		 * action slot: open ScriptBuilder dialog
		 */
		void slotScriptBuilder();

		/**
		 * action slot: open Solar system viewer
		 */
		void slotSolarSystem();

		/**
		 * action slot: open Jupiter Moons tool
		 */
		void slotJMoonTool();

		/**
		 * action slot: open Telescope wizard
		 */
		void slotTelescopeWizard();

		/**
		 * action slot: open Telescope wizard
		 */
		void slotTelescopeProperties();

		/**
		 * action slot: open Image Sequence dialog
		 */
		void slotImageSequence();
		
		 /**
		 * action slot: open INDI driver panel
		 */
		void slotINDIDriver();

		 /**
		 * action slot: open INDI control panel
		 */
		void slotINDIPanel();

		/**
		 * action slot: open INDI configuration dialog
		 */
		void slotINDIConf();

		/**
			*action slot: open dialog for setting the view options
			*/
		void slotViewOps();

		/** finish setting up after the kstarsData has finished
		 */
		void datainitFinished(bool worked);

		/**Open new KStars window. */
		void newWindow();

		/**Close KStars window. */
		void closeWindow();
		
		/** Open FITS image. */
		void slotOpenFITS();

		/**Action slot to save the sky image to a file.*/
		void slotExportImage();

		/**Action slot to select a DCOP script and run it.*/
		void slotRunScript();

		/**Action slot to print skymap. */
		void slotPrint();

		/**Action slot to show tip-of-the-day window. */
		void slotTipOfDay();

		/**Action slot to set focus coordinates manually (opens FocusDialog). */
		void slotManualFocus();

		/**Meta-slot to point the focus at special points (zenith, N, S, E, W).
			*Uses the name of the Action which sent the Signal to identify the
			*desired direction.  */
		void slotPointFocus();

		/**Meta-slot to set the color scheme according to the name of the
			*Action which sent the activating signal.  */
		void slotColorScheme();

		/**Select the Target symbol (a.k.a. field-of-view indicator) */
		void slotTargetSymbol();

		/**Invoke the Field-of-View symbol editor window */
		void slotFOVEdit();

		/**Toggle between Equatorial and Ecliptic coordinte systems */
		void slotCoordSys();

		/**Toggle display of the observing list tool*/
		void slotObsList();

		/**Meta-slot to handle display toggles for all of the viewtoolbar buttons.
			*uses the name of the sender to identify the item to change.  */
		void slotViewToolBar();

		/**Meta-slot to handle toggling display of GUI elements (toolbars and infoboxes)
			*uses name of the sender action to identify the widget to hide/show.  */
		void slotShowGUIItem( bool );

		/**Re-assign the input focus to the SkyMap widget.
			*/
		void mapGetsFocus();
		
		/**Toggle to and from full screen mode */
		void slotFullScreen();

	private:
		/**
			*Initialize Menu bar, toolbars and all Actions.
			*/
		void initActions();

		/**
			*Initialize Field-of-View symbols and FOV submenu
			*/
		void initFOV();

		/**
			*Initialize Status bar.
			*/
		void initStatusBar();

		SkyMap *skymap;

		QWidget *centralWidget;
		QVBoxLayout *topLayout;

		KToolBar *viewToolBar;
		TimeStepBox *TimeStep;

		ToggleAction *actCoordSys;
		ToggleAction *actObsList;
		KActionMenu *colorActionMenu, *fovActionMenu;
		
		KDialogBase *AAVSODialog;
		FindDialog *findDialog;
		KSNewStuff *kns;
		
		INDIMenu *indimenu;
		INDIDriver *indidriver;
		imagesequence *indiseq;  /* We need imgsequence here because it runs in batch mode */

		int idSpinBox;
		bool DialogIsObsolete;
		bool StartClockRunning;
		QString StartDateString;

		QPalette OriginalPalette, DarkPalette;

		class privatedata;
		friend class privatedata;
		privatedata *pd;
        ObservingList *obsList;
};

class KStars::privatedata {
	public:
		KStars *ks;
		KStarsSplash *splash;
		KStarsData *kstarsData;

		/**Constructor */
		privatedata(KStars *parent) : ks(parent), splash(0), kstarsData(0) {}
		/**Destructor */
		~privatedata();

		/**Build the main KStars window */
		void buildGUI();
};

#endif
