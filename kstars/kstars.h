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
#include <kmainwindow.h>
#include <kmenubar.h>
#include <kaction.h>
#include <klocale.h>
#include <ktoolbar.h>

#include <qlayout.h>
#include <qframe.h>
#include <qfile.h>
#include <qdatetime.h>
#include <qlabel.h>
#include <qtimer.h>
#include <qdir.h>
#include <qwidget.h>
#include <qpoint.h>

#include <kapplication.h>
#include <kstandarddirs.h>
#include <qptrlist.h>

#include "skymap.h"
#include "geolocation.h"
#include "ksnumbers.h"
#include "kssun.h"
#include "skyobject.h"
#include "skypoint.h"
#include "timestepbox.h"

#include "kstarsdata.h"
#include "kstarsoptions.h"
#include "kstarsinterface.h"
#include "kstarssplash.h"
#include "toggleaction.h"

// forward declaration is enough. We only need pointers
class KDialogBase;
class KKey;
class TimeDialog;
class LocationDialog;
class FindDialog;
class ViewOpsDialog;
class SimClock;
class InfoPanel;
class InfoBoxes;
class AstroCalc;

//Define some global constants
#define NCIRCLE 360   //number of points used to define equator, ecliptic and horizon
#define NMILKYWAY 11  //number of Milky Way segments
#define MINZOOMLEVEL 0
#define MAXZOOMLEVEL 26
#define DEFAULTZOOMLEVEL 3
#define TIMER_INTERVAL 10

/**
	*kstars is the base class for the KStars project.  It is derived from KMainWindow.
	*In addition to the GUI elements, the class contains the program clock,
	*KStarsData, KStarsOptions, SkyMap, and InfoPanel child objects.
	*It also contains functions for the DCOP interface.
	*@short KStars base class
	*@author Jason Harris
	*@version 0.9
	*/

class KStars : public KMainWindow, virtual public KStarsInterface
{

  Q_OBJECT 
  public:
    /**
	 *Constructor.
	 *
	 *@param kstarsData the KStars Data object
	 */
    KStars( KStarsData* kstarsData );

	/**
	 * Constructor.
	 *
	 * @param doSplash should the splash panel be displayed during
	 * intialization.
	 */
	KStars( bool doSplash );

    /**
			*Destructor.  Synchs config file.  Deletes objects.
			*/
    ~KStars();

		/**
			*@returns pointer to KStarsData object which contains application data.
			*/
		KStarsData* data();

		/**
			*@returns pointer to KStarsOptions object which contains application options.
			*/
		KStarsOptions* options() { return data()->options; }

		/**
			*@returns pointer to SkyMap object which is the sky display widget.
			*/
		SkyMap* map() { return skymap; }

		/**
			*@returns pointer to GeoLocation object which is the current geographic location.
			*/
		GeoLocation* geo() { return options()->Location(); }

		/**
			*@returns pointer to InfoBoxes object.
			*/
		InfoBoxes* infoBoxes() { return IBoxes; }

		/**Display object name and coordinates in the KStars infoPanel
			*/
		void showFocusCoords();

		/**
			*Load KStars options.
			*/
		void loadOptions();

		/**
			*Save KStars options.
			*/
		void saveOptions();

		/**
			*Add an item to the color-scheme action manu
			*@param name The name to use in the menu
			*@param actionName The internal name for the action (derived from filename)
			*/
		void addColorMenuItem( QString name, QString actionName );

		void removeColorMenuItem( QString actionName );

	/**Find object by name.
		*@param name Object name to find
		*@returns pointer to SkyObject matching this name
		*/
		SkyObject* getObjectNamed( QString name );	

	/**@returns pointer to the simulation clock.
		*/
		SimClock* getClock( void ) { return clock; }

	/**@returns pointer to the local sidereal time.
		*/
		dms* LSTh() { return data()->LSTh; }

	/**@returns the timestep scale of the simulation clock.
		*/
		double clockScale( void ) const { return clock->scale(); }

/**Set the KStarsData::HourAngle according to the current LST and focus->ra
	*/
		void setHourAngle();

	/**DCOP interface function.  Set focus to given Ra/Dec coordinates */
		ASYNC setRaDec( double ra, double dec );
		
	/**DCOP interface function.  Set focus to given Alt/Az coordinates. */
		ASYNC setAltAz(double alt, double az);

	/**DCOP interface function.
		*Point in the direction described by the string argument.  */
		ASYNC lookTowards( const QString direction );

	/**DCOP interface function.  Zoom in. */
		ASYNC zoomIn(void) { slotZoomIn(); };

	/**DCOP interface function.  Zoom out. */
		ASYNC zoomOut(void){ slotZoomOut(); };

	/**DCOP interface function.  Default Zoom. */
		ASYNC defaultZoom(void) { slotDefaultZoom(); }

	/**DCOP interface function.  Set local time and date. */
		ASYNC setLocalTime(int yr, int mth, int day, int hr, int min, int sec);

	/**DCOP interface function.  Pause for t seconds. */
		ASYNC waitFor( double t );
		
	/**DCOP interface function.  Pause until Key k is pressed. */
		ASYNC waitForKey( const QString k );
		
	/**DCOP interface function.  Toggle tracking. */
		ASYNC setTracking( bool track );
		
	/**DCOP interface function.  modify option. */
		ASYNC changeViewOption( const QString option, const QString value );
	
	/**DCOP interface function.  Show text message in a popup window. */
		ASYNC popupMessage( int x, int y, const QString message );
		
	/**DCOP interface function.  Draw a line on the sky. */
		ASYNC drawLine( int x1, int y1, int x2, int y2, int speed=0 );
	
	/**DCOP interface function.  Set the geographic location. */
		ASYNC setGeoLocation( const QString city, const QString province, const QString country );

	public slots:
		/**
			*Update time-dependent data and (possibly) repaint the sky map.
			*/
		void updateTime( const bool automaticDSTchange = true );

		/**
			*Zoom in
			*/
		void slotZoomIn();

		/**
			*Zoom out
			*/
		void slotZoomOut();

		void slotDefaultZoom();

		/**
			*action slot: Toggle whether kstars is tracking current position
			*/	
		void slotTrack();

	/**
 		*Delete FindDialog because ObjNames list has changed in KStarsData due to
   	*reloading star data. So list in FindDialog must be new filled with current data.
   	*/
		void clearCachedFindDialog();

		void resumeDCOP( void ) { kapp->dcopClient()->resume(); }

	private slots:
		/**
			*action slot: synch kstars clock to system time
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
			*action slot: open dialog for selecting a new geographic location
			*/	
		void slotGeoLocator();

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

		void slotElTs();

		/**
		 * action slot: open What's up tonight dialog
		 */
    void slotWUT();

		/**
		 * action slot: open ScriptBuilder dialog
		 */
    void slotScriptBuilder();

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
		
		/**Toggle between Equatorial and Ecliptic coordinte systems */
		void slotCoordSys();

		/**Meta-slot to handle display toggles for all of the viewtoolbar buttons.
			*uses the name of the sender to identify the item to change.  */
		void slotViewToolBar();

		/**Meta-slot to handle toggling display of GUI elements (toolbars and infopanel)
			*uses name of the sender action to identify the widget to hide/show.  */
		void slotShowGUIItem( bool );

		/**Re-assign the input focus to the SkyMap widget.
			*/
		void mapGetsFocus() { map()->QWidget::setFocus(); }

		void saveTimeBoxShaded( bool s );
		void saveGeoBoxShaded( bool s );
		void saveFocusBoxShaded( bool s );
		void saveTimeBoxSticky( int s );
		void saveGeoBoxSticky( int s );
		void saveFocusBoxSticky( int s );
		void saveTimeBoxPos( QPoint p );
		void saveGeoBoxPos( QPoint p );
		void saveFocusBoxPos( QPoint p );

	private:
		/**
			*Initialize Menu bar.
			*/
		void initActions();

		/**
			*Initialize Status bar.
			*/
		void initStatusBar();

		/**
			*Initialize celestial equator, horizon and ecliptic.
			*/
		void initGuides(KSNumbers *num);

		/**format a date string
			*@param d QDate to format as a string
			*@returns formatted date string for the given date
			*/
		QString getDateString( QDate d );

		/**change the current simulation time to the time and date specified as the arguments.
			*Specified date and time is always local time.
			*@param newDate the date to set.
			*@param newTIme the time to set.
			*/
		void changeTime(QDate newDate, QTime newTime );

		/**Set the KStarsData::LSTh member from the current UTC.
		*/
		void setLSTh( QDateTime UTC );

		SkyMap *skymap;
		SimClock *clock;

		FindDialog *findDialog;
		InfoPanel   *infoPanel;
		InfoBoxes   *IBoxes;
		KToolBar *viewToolBar;
		TimeStepBox *TimeStep;

		ToggleAction *actCoordSys;
		KActionMenu *colorActionMenu;
		QWidget *centralWidget;
		QVBoxLayout *topLayout;
       KDialogBase *AAVSODialog;

    
		int idSpinBox;
		bool useDefaultOptions, DialogIsObsolete;

		class privatedata;
		friend class privatedata;
		privatedata *pd;
};

class KStars::privatedata {

	public:
		KStars *ks;
		KStarsSplash *splash;
		KStarsData *kstarsData;

		privatedata(KStars *parent) : ks(parent), splash(0), kstarsData(0) {};
		void buildGUI();

		virtual ~privatedata() {
			if (splash) delete splash;
			if (kstarsData) delete kstarsData;
		};
};

#endif
