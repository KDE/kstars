/***************************************************************************
                          kstars.cpp  -  K Desktop Planetarium
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
//JH 11.06.2002: replaced infoPanel with infoBoxes
//JH 24.08.2001: reorganized infoPanel
//JH 25.08.2001: added toolbar, converted menu items to KAction objects
//JH 25.08.2001: main window now resizable, window size saved in config file


#include <stdio.h>
#include <stdlib.h>
//#include <iostream.h>
#include <kdebug.h>

#include "finddialog.h"
#include "kstars.h"
#include "simclock.h"
#include "ksutils.h"
#include "infoboxes.h"

// to remove warnings
#include "indimenu.h"
#include "indidriver.h"

KStars::KStars( bool doSplash ) :
	KMainWindow( NULL, NULL ), DCOPObject("KStarsInterface"),
	skymap(0), findDialog(0), centralWidget(0),
	AAVSODialog(0), DialogIsObsolete(false)
{
	pd = new privatedata(this);

	// we're nowhere near ready to take dcop calls
	kapp->dcopClient()->suspend();

	if ( doSplash ) {
		pd->kstarsData = new KStarsData();
		QObject::connect(pd->kstarsData, SIGNAL( initFinished(bool) ),
				this, SLOT( datainitFinished(bool) ) );

		pd->splash = new KStarsSplash(0, "Splash");
		QObject::connect(pd->splash, SIGNAL( closeWindow() ), this, SLOT( closeWindow() ) );
		QObject::connect(pd->kstarsData, SIGNAL( progressText(QString) ),
				pd->splash, SLOT( setMessage(QString) ));
		pd->splash->show();
	}
	pd->kstarsData->initialize();

	#if ( __GLIBC__ >= 2 &&__GLIBC_MINOR__ >= 1 )
	kdDebug() << "glibc >= 2.1 detected.  Using GNU extension sincos()" << endl;
	#else
	kdDebug() << "Did not find glibc >= 2.1.  Will use ANSI-compliant sin()/cos() functions." << endl;
	#endif
}

KStars::KStars( KStarsData* kd )
	: KMainWindow( NULL, NULL ), DCOPObject("KStarsInterface")
{
	// The assumption is that kstarsData is fully initialized

	pd = new privatedata(this);
	pd->kstarsData = kd;
	pd->buildGUI();

	clock()->start();

	show();
}

KStars::~KStars()
{
	data()->saveOptions(this);

	clearCachedFindDialog();

	if (skymap) delete skymap;
	if (pd) delete pd;
	if (centralWidget) delete centralWidget;
	if (AAVSODialog) delete AAVSODialog;
	if (indimenu) delete indimenu;
	if (indidriver) delete indidriver;
}

void KStars::changeTime( QDate newDate, QTime newTime ) {
	QDateTime new_time(newDate, newTime);

	GeoLocation *Geo = geo();
	KStarsData *Data = data();

	//Make sure Numbers, Moon, planets, and sky objects are updated immediately
	Data->setFullTimeUpdate();

	// reset tzrules data with newlocal time and time direction (forward or backward)
	Geo->tzrule()->reset_with_ltime(new_time, Geo->TZ0(), Data->isTimeRunningForward() );

	// reset next dst change time
	Data->setNextDSTChange( KSUtils::UTtoJD( Geo->tzrule()->nextDSTChange() ) );

	//Turn off animated slews for the next time step.
	options()->setSnapNextFocus();

	//Set the clock
	clock()->setUTC( new_time.addSecs( int(-3600 * Geo->TZ()) ) );

	// reset local sideral time
	setLST( clock()->UTC() );
}

void KStars::clearCachedFindDialog() {
	if ( findDialog  ) {  // dialog is cached
/**
	*Delete findDialog only if it is not opened
	*/
		if ( findDialog->isHidden() ) {
			delete findDialog;
			findDialog = 0;
			DialogIsObsolete = false;
		}
		else
			DialogIsObsolete = true;  // dialog was opened so it could not deleted
   }
}

void KStars::updateTime( const bool automaticDSTchange ) {
	dms oldLST( LST()->Degrees() );
	// Due to frequently use of this function save data and map pointers for speedup.
	// Save options() and geo() to a pointer would not speedup because most of time options
	// and geo will accessed only one time.
	KStarsData *Data = data();
	SkyMap *Map = map();

	Data->updateTime( geo(), Map, automaticDSTchange );
	if ( infoBoxes()->timeChanged(Data->UTime, Data->LTime, LST(), Data->CurrentDate) )
	        Map->update();

	//We do this outside of kstarsdata just to get the coordinates
	//displayed in the infobox to update every second.
//	if ( !options()->isTracking && LST()->Degrees() > oldLST.Degrees() ) {
//		int nSec = int( 3600.*( LST()->Hours() - oldLST.Hours() ) );
//		Map->focus()->setRA( Map->focus()->ra()->Hours() + double( nSec )/3600. );
//		if ( options()->useAltAz ) Map->focus()->EquatorialToHorizontal( LST(), geo()->lat() );
//		Map->showFocusCoords();
//	}

	//If time is accelerated beyond slewTimescale, then the clock's timer is stopped,
	//so that it can be ticked manually after each update, in order to make each time
	//step exactly equal to the timeScale setting.
	//Wrap the call to manualTick() in a singleshot timer so that it doesn't get called until
	//the skymap has been completely updated.
	if ( Data->clock->isManualMode() && Data->clock->isActive() ) {
		QTimer::singleShot( 0, Data->clock, SLOT( manualTick() ) );
	}
}

SkyObject* KStars::getObjectNamed( QString name ) {
	if ( (name== "star") || (name== "nothing") || name.isEmpty() ) return NULL;
	if ( name== data()->Moon->name() ) return data()->Moon;

	SkyObject *so = data()->PC->findByName(name);

	if (so != 0)
		return so;

	//Stars
	for ( unsigned int i=0; i<data()->starList.count(); ++i ) {
		if ( name==data()->starList.at(i)->name() ) return data()->starList.at(i);
	}

	//Deep sky objects
	for ( unsigned int i=0; i<data()->deepSkyListMessier.count(); ++i ) {
		if ( name==data()->deepSkyListMessier.at(i)->name() ) return data()->deepSkyListMessier.at(i);
	}
	for ( unsigned int i=0; i<data()->deepSkyListNGC.count(); ++i ) {
		if ( name==data()->deepSkyListNGC.at(i)->name() ) return data()->deepSkyListNGC.at(i);
	}
	for ( unsigned int i=0; i<data()->deepSkyListIC.count(); ++i ) {
		if ( name==data()->deepSkyListIC.at(i)->name() ) return data()->deepSkyListIC.at(i);
	}
	for ( unsigned int i=0; i<data()->deepSkyListOther.count(); ++i ) {
		if ( name==data()->deepSkyListOther.at(i)->name() ) return data()->deepSkyListOther.at(i);
	}

	//Constellations
	for ( unsigned int i=0; i<data()->cnameList.count(); ++i ) {
		if ( name==data()->cnameList.at(i)->name() ) return data()->cnameList.at(i);
	}

	//reach here only if argument is not matched
	return NULL;
}


QString KStars::getDateString( QDate date ) {
	QString dummy;
	QString dateString = dummy.sprintf( "%02d / %02d / %04d",
			date.month(), date.day(), date.year() );
	return dateString;
}


//
//This is bogus, but until all the data members of KStarsData migrate to the
//class where they _really_ belong, we'll do the forwarding.
//
void KStars::setHourAngle() {
	data()->HourAngle->setH( LST()->Hours() - map()->focus()->ra()->Hours() );
}

void KStars::setLST( QDateTime UTC ) {
	LST()->set( KSUtils::UTtoLST( UTC, geo()->lng() ) );
}

KStarsData* KStars::data( void ) { return pd->kstarsData; }

#include "kstars.moc"
