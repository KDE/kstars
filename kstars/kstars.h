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

#include <kapp.h>
#include <kmainwindow.h>
#include <kmenubar.h>
#include <kaction.h>
#include <kstddirs.h>
#include <klocale.h>
#include <qlayout.h>
#include <qframe.h>
#include <qfile.h>
#include <qlist.h>
#include <qdatetime.h>
#include <qtimer.h>
#include <qlabel.h>
#include <qdatetime.h>
#include <qtimer.h>
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
#define TIMER_INTERVAL 10

/** KStars is the base class of the project */
class KStars : public KMainWindow
{
  Q_OBJECT 
  public:
    /** construtor */
    KStars( KStarsData* kstarsData );
    /** destructor */
    ~KStars();

		KStarsData* GetData();
		KStarsOptions* GetOptions();

		SkyMap *skymap;
		SkyPoint focus, oldfocus;
		dms LSTh;
		QLabel 		*FocusObject, *FocusRADec, *FocusAltAz;

		QString TypeName[10];
    QString CurrentPosition;
		GeoLocation *geo;
		
		KAction *actQuit, *actZoomIn, *actZoomOut, *actFind, *actTrack, *actInfo, *actHandbook;
		KAction *actTimeSet, *actTimeNow, *actTimeRun, *actLocation, *actViewOps;

		void initMenuBar();
		void initToolBar();
		void initStatusBar();
		void initOptions();
		void initLocation();
		void updateEpoch( long double NewEpoch );
		double findObliquity( long double CurrentEpoch );		
		SkyObject* getObjectNamed( QString name );	
	public slots:
		void mZoomIn();
		void mZoomOut();

	protected slots:
/*
	If closeEvent is called, downloads must be checked. If there are any downloads, the closeEvent
	will be ignored.
*/
		void closeEvent (QCloseEvent *e);

	private slots:
		void updateTime( void );
		void mSetTimeToNow();
		void mSetTime();
		void mToggleTimer();
		void mZenith();
		void mFind();
		void mTrack();
		void mGeoLocator();
		void mReverseVideo();
		void mViewOps();
		void mAstroInfo();
		void initStars();

	private:
    QString getDateString( QDate d );
		long double getJD( QDateTime t);
		void nutate( long double Epoch );
		QTime UTtoLST( QDateTime UT, dms longitude );
		QTime UTtoGST( QDateTime UT );
		QTime GSTtoLST( QTime GST, dms longitude );
				
		QHBoxLayout *iplay;
		QGridLayout *coolay;
		QVBoxLayout *tlablay, *timelay, *datelay, *focuslay, *geolay;
		
		QFrame    *infoPanel;
		QLabel			*LT, *UT, *LTLabel, *UTLabel;
		QLabel			*ST, *JD, *STLabel;
		QLabel			*LTDate, *UTDate;
		QLabel			*PlaceName;
		QLabel			*Long, *Lat, *LongLabel, *LatLabel;
		QTimer		*tmr;
		SkyObject 	*obj;
		TimeSpinBox *TimeStep;
		KStarsData* kstarsData; // does not need to be public, we have provided an accessor method
		int idSpinBox;
};

#endif
