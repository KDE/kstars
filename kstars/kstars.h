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

#include <kapplication.h>
#if (KDE_VERSION < 300)
#include <kstddirs.h>
#include <qlist.h>
#else
#include <kstandarddirs.h>
#include <qptrlist.h>
#endif

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
class TimeDialog;
class LocationDialog;
class FindDialog;
class ViewOpsDialog;
class SimClock;
class InfoPanel;
class AstroCalc;

//Define some global constants
#define NCIRCLE 360   //number of points used to define equator, ecliptic and horizon
#define NMILKYWAY 11  //number of Milky Way segments
#define MINZOOMLEVEL 0
#define MAXZOOMLEVEL 26
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
			*@returns GeoLocation object which is the current geographic location.
			*/
		GeoLocation* geo() const { return Location; }

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

	/**Find object by name.
		*@param name Object name to find
		*@returns pointer to SkyObject matching this name
		*/
		SkyObject* getObjectNamed( QString name );	

	/**@returns pointer to the simulation clock.
		*/
		SimClock* getClock( void ) { return clock; }

	/**@returns the local sidereal time.
		*/
		dms LSTh();

	/**@returns the timestep scale of the simulation clock.
		*/
		double clockScale( void ) const { return clock->scale(); }

/**Set the KStarsData::HourAngle according to the current LST and focus->ra
	*/
		void setHourAngle();

	/**DCOP interface function.
		*Point in the direction described by the string argument.  */
		ASYNC lookTowards(QString direction);

	/**DCOP interface function.  Zoom in. */
		ASYNC zoomIn(void) { slotZoomIn(); };

	/**DCOP interface function.  Zoom out. */
		ASYNC zoomOut(void){ slotZoomOut(); };

	/**DCOP interface function.  Set focus to given Alt/Az coordinates. */
		ASYNC setAltAz(double alt, double az);

	/**DCOP interface function.  Set local time and date. */
		ASYNC setLocalTime(int yr, int mth, int day, int hr, int min, int sec);

	public slots:
		/**
			*Update time-dependent data and (possibly) repaint the sky map.
			*/
		void updateTime( void );

		/**
			*Zoom in
			*/
		void slotZoomIn();

		/**
			*Zoom out
			*/
		void slotZoomOut();
	
	/**
 		*Delete FindDialog because ObjNames list has changed in KStarsData due to
   	*reloading star data. So list in FindDialog must be new filled with current data.
   	*/
		void clearCachedFindDialog();

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
			*action slot: Toggle whether kstars is tracking current position
			*/	
		void slotTrack();

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
		void mapGetsFocus();

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
			*Initialize Geographic location.
			*/
		void initLocation();

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
			*@param newDate the date to set.
			*@param newTIme the time to set.
			*/
		void changeTime(QDate newDate, QTime newTime);

/**Set the KStarsData::LSTh member from the current UTC.
	*/
		void setLSTh( QDateTime UTC );

		SkyMap *skymap;
		SimClock *clock;
		GeoLocation *Location;

		FindDialog *findDialog;
		InfoPanel   *infoPanel;
		KToolBar *viewToolBar;
		TimeStepBox *TimeStep;

		ToggleAction *actCoordSys;
		KActionMenu *colorActionMenu;
		QWidget *centralWidget;
		QVBoxLayout *topLayout;

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
			delete splash;
			delete kstarsData;
		};
};

#endif
