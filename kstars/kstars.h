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

#include <kapplication.h>
#include <kmainwindow.h>
#include <kmenubar.h>
#include <kaction.h>
#include <kstandarddirs.h>
#include <klocale.h>
#include <qlayout.h>
#include <qframe.h>
#include <qfile.h>
//#include <qptrlist.h>
#include <qlabel.h>
#include <qdir.h>

#include "skymap.h"
#include "geolocation.h"
#include "kssun.h"
#include "skyobject.h"
#include "skypoint.h"
#include "timespinbox.h"

#include "kstarsdata.h"
#include "kstarsoptions.h"

// forward declaration is enough. We only need pointers
class TimeDialog;
class LocationDialog;
class FindDialog;
class ViewOpsDialog;

//Define some global constants
#define NCIRCLE 360   //number of points used to define equator, ecliptic and horizon
#define NMILKYWAY 11  //number of Milky Way segments
#define MINZOOMLEVEL 0
#define MAXZOOMLEVEL 12
#define TIMER_INTERVAL 10

/**
	*kstars is the base class for the KStars project.  It is derived from KMainWindow.
	*In addition to the GUI elements, the class contains the program clock,
	*time conversion routines, and event actions.
	*
	*@short KStars base class
	*@author Jason Harris
	*@version 0.9
	*/

class KStars : public KMainWindow
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
			*Destructor.  Synchs config file.  Deletes objects.
			*/
    ~KStars();

		/**
			*@returns pointer to KStarsData object which contains application data.
			*/
		KStarsData* data( void ) { return kstarsData; }

		/**
			*@returns pointer to KStarsOptions object which contains application options.
			*/
		KStarsOptions* options( void ) { return kstarsData->options; }

		/**
			*@returns pointer to SkyMap object which is the sky display widget.
			*/
		SkyMap* map( void ) { return skymap; }

		/**
			*@returns GeoLocation object which is the current geographic location.
			*/
		GeoLocation* geo( void ) const { return Location; }

		/**
			*@sets Location new (currently needed by kapp->geo())
			*/
		void setLocation( GeoLocation *l ) { Location = l; }

		/**Display object name and coordinates in the KStars infoPanel
			*/
		void showFocusCoords( void );

		/**
			*Update epoch-dependent data.
			*/
		void updateEpoch( long double NewEpoch );

		/**
			*Determine the obliquity of the Ecliptic for the current epoch.
			*/
		double findObliquity( long double CurrentEpoch );		

		/**Find object by name.
			*@param name Object name to find
			*@returns pointer to SkyObject matching this name
			*/
		SkyObject* getObjectNamed( QString name );	

		SkyMap *skymap;
		
		KAction *actQuit, *actZoomIn, *actZoomOut, *actFind, *actTrack, *actInfo, *actHandbook;
		KAction *actTimeSet, *actTimeNow, *actTimeRun, *actLocation, *actViewOps;

	public slots:

		/**
			*Zoom in
			*/
		void mZoomIn();

		/**
			*Zoom out
			*/
		void mZoomOut();

	protected slots:
		/**
			*Make sure there are no active downloads before closing the application.
			*/
		void closeEvent (QCloseEvent *e);

	private slots:
		/**
			*Update time-dependent data and (possibly) repaint the sky map.
			*/
		void updateTime( void );

		/**
			*Reset Time Markers when time step changes
			*/
		void changeTimeStep( void );

		/**
			*action slot: synch kstars clock to system time
			*/
		void mSetTimeToNow();

		/**
			*action slot: open a dialog for setting the time and date
			*/
		void mSetTime();

		/**
			*action slot: toggle whether kstars clock is running or not
			*/
		void mToggleTimer();

		/**
			*action slot: point at current Zenith point
			*/
		void mZenith();

		/**
			*action slot: open dialog for finding a named object
			*/	
		void mFind();

		/**
			*action slot: Toggle whether kstars is tracking current position
			*/	
		void mTrack();

		/**
			*action slot: open dialog for selecting a new geographic location
			*/	
		void mGeoLocator();

		/**
			*action slot: open dialog for setting the view options
			*/	
		void mViewOps();

	private:
		/**
			*Initialize Menu bar.
			*/
		void initMenuBar();

		/**
			*Initialize Tool bar.
			*/
		void initToolBar();

		/**
			*Initialize Status bar.
			*/
		void initStatusBar();

		/**
			*Initialize KStars options.
			*/
		void initOptions();

		/**
			*Initialize Geographic location.
			*/
		void initLocation();

		/**
			*Set initial horizontal coordinates of all objects.
			*/
		void initAltAz();

		/**format a date string
			*@param d QDate to format as a string
			*@returns formatted date string for the given date
			*/
		QString getDateString( QDate d );

		/**determine Julian Date for a date and time
			*@param t QDateTime to convert to Julian Date
			*@returns Julian Day number for the given date/time
			*/
		long double getJD( QDateTime t);

		/**Determine nutation.
			*@param Epoch the current epoch's Julian Date
			*/
		void nutate( long double Epoch );

		/**convert universal time to local sidereal time
			*@param UT the Universal Time
			*@param longitude the current location's longitude
			*@returns QTime representing local sidereal time
			*/
		QTime UTtoLST( QDateTime UT, dms longitude );

		/**convert universal time to Greenwich sidereal time
			*@param UT the Universal Time
			*@returns QTime representing the Greenwich sidereal time
			*/
		QTime UTtoGST( QDateTime UT );

		/**convert Greenwich sidereal time to local sidereal time
			*@param GST the Greenwich Sidereal Time
			*@param longitude the current location's longitude
			*@returns QTime representing local sidereal time
			*/
		QTime GSTtoLST( QTime GST, dms longitude );
				
		QHBoxLayout *iplay;
		QGridLayout *coolay;
		QVBoxLayout *tlablay, *timelay, *datelay, *focuslay, *geolay;
		QHBoxLayout *radeclay, *altazlay;
		
		QFrame     *infoPanel;
		QLabel     *FocusObject, *FocusRA, *FocusDec, *FocusAz, *FocusAlt;
		QLabel     *LT, *UT, *LTLabel, *UTLabel;
		QLabel     *ST, *JD, *STLabel;
		QLabel     *LTDate, *UTDate;
		QLabel     *PlaceName;
		QLabel     *Long, *Lat, *LongLabel, *LatLabel;
		QTimer     *tmr;
		SkyObject 	*obj;
		TimeSpinBox *TimeStep;
		KStarsData* kstarsData; // does not need to be public, we have provided an accessor method
		GeoLocation *Location;
		int idSpinBox;

		bool useDefaultOptions;
};

#endif
