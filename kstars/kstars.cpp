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


#include <kaccel.h>

#include <stdio.h>
#include <stdlib.h>
//#include <iostream.h>
#include <kdebug.h>

#include "finddialog.h"
#include "kstars.h"
#include "simclock.h"
#include "ksutils.h"
#include "infoboxes.h"

KStars::KStars( bool doSplash ) :
	KMainWindow( NULL, NULL ), DCOPObject("KStarsInterface"),
	skymap(0), clock(0), findDialog(0), IBoxes(0), centralWidget(0),
	AAVSODialog(0), DialogIsObsolete(false)
{
	pd = new privatedata(this);

	// we're nowhere near ready to take dcop calls
	kapp->dcopClient()->suspend();

	if ( doSplash ) {
		pd->kstarsData = new KStarsData(this);
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

	//Instantiate the SimClock object
//JH (22 Jan 2002): we instantiated clock already in buildGUI()...
//	clock = new SimClock(this);
	clock->start();
	show();
}

KStars::~KStars()
{
	saveOptions();

	clearCachedFindDialog();

	if (IBoxes) delete IBoxes;
	if (skymap) delete skymap;
	delete pd;
	if (clock) delete clock;
	if (centralWidget) delete centralWidget;
	if (AAVSODialog) delete AAVSODialog;
}

void KStars::changeTime( QDate newDate, QTime newTime ) {
	QDateTime new_time(newDate, newTime);

	GeoLocation *Geo = geo();
	KStarsData *Data = data();

	clock->stop();

	//Make sure Numbers, Moon, planets, and sky objects are updated immediately
	Data->setFullTimeUpdate();

	// reset tzrules data with newlocal time and time direction (forward or backward)
	Geo->tzrule()->reset_with_ltime(new_time, Geo->TZ0(), Data->isTimeRunningForward() );
	
	// reset next dst change time
	Data->setNextDSTChange( KSUtils::UTtoJulian( Geo->tzrule()->nextDSTChange() ) );

	//Turn off animated slews for the next time step.
	options()->setSnapNextFocus();
	clock->setUTC( new_time.addSecs( int(-3600 * Geo->TZ()) ) );

	// reset local sideral time
	setLSTh( clock->UTC() );
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
	QTime oldLST = data()->LST;
	// Due to frequently use of this function save data and map pointers for speedup.
	// Save options() and geo() to a pointer would not speedup because most of time options
	// and geo will accessed only one time.
	KStarsData *Data = data();
	SkyMap *Map = map();

	Data->updateTime( clock, geo(), Map, automaticDSTchange );

	if ( infoBoxes()->timeChanged(Data->UTime, Data->LTime, Data->LST, Data->CurrentDate) )
	        Map->update();

	//We do this outside of kstarsdata just to get the coordinates
	//displayed in the infobox to update every second.
	if ( !options()->isTracking && Data->LST > oldLST ) { 
		int nSec = oldLST.secsTo( Data->LST );
		Map->focus()->setRA( Map->focus()->ra()->Hours() + double( nSec )/3600. );
		if ( options()->useAltAz ) Map->focus()->EquatorialToHorizontal( Data->LSTh, geo()->lat() );
		showFocusCoords();
	}

	//If time is accelerated beyond slewTimescale, then the clock's timer is stopped,
	//so that it can be ticked manually after each update, in order to make each time
	//step exactly equal to the timeScale setting.
	//Wrap the call to manualTick() in a singleshot timer so that it doesn't get called until
	//the skymap has been completely updated.
	if ( clock->isManualMode() && clock->isActive() ) {
		QTimer::singleShot( 0, clock, SLOT( manualTick() ) );
	}
}

SkyObject* KStars::getObjectNamed( QString name ) {
	if ( (name== "star") || (name== "nothing") || name.isEmpty() ) return NULL;
	if ( name== data()->Moon->name() ) return data()->Moon;

	SkyObject *so = data()->PC->findByName(name);

	if (so != 0)
		return so;

	//Stars
	//REVERTED...remove comments after 1/1/2003
	//ARRAY:
	for ( unsigned int i=0; i<data()->starList.count(); ++i ) {
		if ( name==data()->starList.at(i)->name() ) return data()->starList.at(i);
	}
	//for ( unsigned int i=0; i < data()->StarCount; ++i ) {
	//	StarObject *o = &(data()->starArray[i]);
	//	if ( name == o->name() ) return o;
	//}
	
	//Deep-sky catalogs
/*
	for ( unsigned int i=0; i<data()->deepSkyList.count(); ++i ) {
		if ( name==data()->deepSkyList.at(i)->name() ) return data()->deepSkyList.at(i);
	}
*/
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

void KStars::showFocusCoords( void ) {
	//display object info in infoBoxes
	QString oname;

	oname = i18n( "nothing" );
	if ( map()->foundObject() != NULL && options()->isTracking ) {
		oname = map()->foundObject()->translatedName();
		//add genetive name for stars
	  if ( map()->foundObject()->type()==0 && map()->foundObject()->name2().length() )
			oname += " (" + map()->foundObject()->name2() + ")";
	}

	//
	//This is ugly, got to find a way to change this. But for now.
	//
	infoBoxes()->focusObjChanged(oname);
	infoBoxes()->focusCoordChanged(map()->focus());

}

QString KStars::getDateString( QDate date ) {
	QString dummy;
	QString dateString = dummy.sprintf( "%02d / %02d / %04d",
			date.month(), date.day(), date.year() );
	return dateString;
}

//DCOP function
void KStars::setRaDec( double ra, double dec ) {
	map()->setDestination( new SkyPoint( ra, dec ) );
}

//DCOP function
void KStars::setAltAz( double alt, double az ) {
 	map()->setDestinationAltAz(alt,az);
}

//DCOP function
void KStars::lookTowards ( const QString direction ) {
  QString dir = direction.lower();
	if (dir == "zenith" || dir=="z") map()->invokeKey( KAccel::stringToKey( "Z" ) );
	else if (dir == "north" || dir=="n") map()->invokeKey( KAccel::stringToKey( "N" ) );
	else if (dir == "east"  || dir=="e") map()->invokeKey( KAccel::stringToKey( "E" ) );
	else if (dir == "south" || dir=="s") map()->invokeKey( KAccel::stringToKey( "S" ) );
	else if (dir == "west"  || dir=="w") map()->invokeKey( KAccel::stringToKey( "W" ) );
	else if (dir == "northeast" || dir=="ne") {
		map()->setClickedObject( NULL );
		map()->clickedPoint()->setAlt( 15.0 ); map()->clickedPoint()->setAz( 45.0 );
		map()->clickedPoint()->HorizontalToEquatorial( data()->LSTh, geo()->lat() );
		map()->slotCenter();
	} else if (dir == "southeast" || dir=="se") {
		map()->setClickedObject( NULL );
		map()->clickedPoint()->setAlt( 15.0 ); map()->clickedPoint()->setAz( 135.0 );
		map()->clickedPoint()->HorizontalToEquatorial( data()->LSTh, geo()->lat() );
		map()->slotCenter();
	} else if (dir == "southwest" || dir=="sw") {
		map()->setClickedObject( NULL );
		map()->clickedPoint()->setAlt( 15.0 ); map()->clickedPoint()->setAz( 225.0 );
		map()->clickedPoint()->HorizontalToEquatorial( data()->LSTh, geo()->lat() );
		map()->slotCenter();
	} else if (dir == "northwest" || dir=="nw") {
		map()->setClickedObject( NULL );
		map()->clickedPoint()->setAlt( 15.0 ); map()->clickedPoint()->setAz( 315.0 );
		map()->clickedPoint()->HorizontalToEquatorial( data()->LSTh, geo()->lat() );
		map()->slotCenter();
	} else {
		SkyObject *target = getObjectNamed( direction );
		if ( target != NULL ) {
			map()->setClickedObject( target );
			map()->setClickedPoint( target );
			map()->slotCenter();
		}
	}
}

//DCOP function
void KStars::setLocalTime(int yr, int mth, int day, int hr, int min, int sec) {
	changeTime( QDate(yr, mth, day), QTime(hr,min,sec));
}

//DCOP function
void KStars::waitFor( double t ) {
	kapp->dcopClient()->suspend();
	QTimer::singleShot( int( 1000.*t ), this, SLOT( resumeDCOP() ) );
}

//DCOP function
void KStars::waitForKey( const QString k ) {
	data()->resumeKey = KKey( k );
	if ( ! data()->resumeKey.isNull() ) {
		kapp->dcopClient()->suspend();
	} else {
		kdDebug() << i18n( "Error [DCOP waitForKey()]: Invalid key requested." ) << endl;
	}
}

//DCOP function
void KStars::setTracking( bool track ) {
	if ( track != options()->isTracking ) slotTrack();
}

//DCOP function
void KStars::changeOption( QString option, QString value ) {
	//Try to match option argument to an existing option, then parse the argument
	//This function will be tedious to write and maintain with our current 
	//implementation of options.  May re-implement options using a QMap or QDict
	//object to store option values.
}

//DCOP function
void KStars::popupMessage( int x, int y, QString message ){
	//Show a small popup window at (x,y) with a text message
}

//DCOP function
void KStars::drawLine( int x1, int y1, int x2, int y2, int speed ) {
	//Draw a line on the skymap display
}

//DCOP function 
void KStars::setGeoLocation( QString city, QString province, QString country ) {
	//Set the geographic location
	for (GeoLocation *loc = data()->geoList.first(); loc; loc = data()->geoList.next()) {
		if ( loc->translatedName() == city &&
					loc->translatedProvince() == province &&
					loc->translatedCountry() == country ) {
			options()->setLocation( *loc );
			
			//notify on-screen GeoBox
			infoBoxes()->geoChanged( loc );
			
			//configure time zone rule
			QDateTime ltime = data()->UTime.addSecs( int( 3600 * loc->TZ0() ) );
			loc->tzrule()->reset_with_ltime( ltime, loc->TZ0(), data()->isTimeRunningForward() );
			data()->setNextDSTChange( KSUtils::UTtoJulian( loc->tzrule()->nextDSTChange() ) );
			
			//reset LST
			setLSTh( clock->UTC() );
			
			//make sure planets, etc. are updated immediately
			data()->setFullTimeUpdate();
			
			// If the sky is in Horizontal mode and not tracking, reset focus such that
			// Alt/Az remain constant.
			if ( ! options()->isTracking && options()->useAltAz ) {
				map()->focus()->HorizontalToEquatorial( LSTh(), geo()->lat() );
			}

			// recalculate new times and objects
			options()->setSnapNextFocus();
			updateTime();
			
			//no need to keep looking, we're done.
			break;
		}
	}
}


//
//This is bogus, but until all the data members of KStarsData migrate to the
//class where they _really_ belong, we'll do the forwarding.
//
void KStars::setHourAngle() {
	data()->HourAngle->setH( LSTh()->Hours() - map()->focus()->ra()->Hours() );
}

void KStars::setLSTh( QDateTime UTC ) {
	data()->LST = KSUtils::UTtoLST( UTC, geo()->lng() );
	LSTh()->setH( data()->LST.hour(), data()->LST.minute(), data()->LST.second() );
}

KStarsData* KStars::data( void ) { return pd->kstarsData; }

#include "kstars.moc"
