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
//JH 24.08.2001: reorganized infoPanel
//JH 25.08.2001: added toolbar, converted menu items to KAction objects
//JH 25.08.2001: main window now resizable, window size saved in config file

#include <qfont.h>
#include <qpopupmenu.h>
#include <qtextstream.h>
#include <qlineedit.h>
#include <qsizepolicy.h>
#include <qlayout.h>

#include <kaccel.h>
#include <kconfig.h>
#include <kstdaction.h>
#include <kpopupmenu.h>

#include <stdio.h>
#include <stdlib.h>
#include <stream.h>
#include <kdebug.h>
#include <dcopclient.h>

#include "finddialog.h"
#include "kstars.h"
#include "kstarssplash.h"
#include "simclock.h"
#include "ksutils.h"
#include "infopanel.h"

KStars::KStars( bool doSplash) :
	KMainWindow( NULL, NULL ), DCOPObject("KStarsInterface"),
	findDialog( 0 ), DialogIsObsolete( false )
{
	pd = new privatedata(this);

	//
	// we're nowhere near ready to take dcop calls
	//
	kapp->dcopClient()->suspend();

	pd->kstarsData = new KStarsData();
	QObject::connect(pd->kstarsData, SIGNAL( initFinished(bool) ),
				this, SLOT( datainitFinished(bool) ) );

	if (doSplash) {
		pd->splash = new KStarsSplash(0, "Splash");
		QObject::connect(pd->kstarsData, SIGNAL( progressText(QString) ),
				pd->splash, SLOT( setMessage(QString) ));
		pd->splash->show();
	}

	pd->kstarsData->initialize();
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
	//Sync the config file
	kapp->config()->setGroup( "Location" );
	kapp->config()->writeEntry( "City", options()->CityName );
	kapp->config()->writeEntry( "Province", options()->ProvinceName );
	kapp->config()->writeEntry( "Country", options()->CountryName );

	kapp->config()->setGroup( "Catalogs" );
	kapp->config()->writeEntry( "CatalogCount", options()->CatalogCount );
	for ( unsigned int i=0; i<options()->CatalogCount; ++i ) {
		QString cname = QString( "CatalogName%1" ).arg( i );
		QString cfile = QString( "CatalogFile%1" ).arg( i );
		QString cshow = QString( "ShowCatalog%1" ).arg( i );
		kapp->config()->writeEntry( cname, options()->CatalogName[i] );
		kapp->config()->writeEntry( cfile, options()->CatalogFile[i] );
		kapp->config()->writeEntry( cshow, options()->drawCatalog[i] );
	}

	kapp->config()->setGroup( "GUI" );
	kapp->config()->writeEntry( "ShowInfoPanel", options()->showInfoPanel );
	kapp->config()->writeEntry( "ShowIPTime", options()->showIPTime );
	kapp->config()->writeEntry( "ShowIPFocus", options()->showIPFocus );
	kapp->config()->writeEntry( "ShowIPGeo", options()->showIPGeo );
	kapp->config()->writeEntry( "ShowMainToolBar", options()->showMainToolBar );
	kapp->config()->writeEntry( "ShowViewToolBar", options()->showViewToolBar );

	kapp->config()->setGroup( "View" );
	kapp->config()->writeEntry( "SkyColor", 	options()->colorSky );
	kapp->config()->writeEntry( "MWColor", 		options()->colorMW );
	kapp->config()->writeEntry( "EqColor", 		options()->colorEq );
	kapp->config()->writeEntry( "EclColor", 		options()->colorEcl );
	kapp->config()->writeEntry( "HorzColor", 	options()->colorHorz );
	kapp->config()->writeEntry( "GridColor", 	options()->colorGrid );
	kapp->config()->writeEntry( "MessColor", 	options()->colorMess );
	kapp->config()->writeEntry( "NGCColor", 	options()->colorNGC );
	kapp->config()->writeEntry( "ICColor", 		options()->colorIC );
	kapp->config()->writeEntry( "CLineColor", options()->colorCLine );
	kapp->config()->writeEntry( "CNameColor", options()->colorCName );
	kapp->config()->writeEntry( "PNameColor", options()->colorPName );
	kapp->config()->writeEntry( "SNameColor", options()->colorSName );
	kapp->config()->writeEntry( "HSTColor", 	options()->colorHST );
	kapp->config()->writeEntry( "StarColorMode", options()->starColorMode );
	kapp->config()->writeEntry( "StarColorsIntensity", options()->starColorIntensity );
	kapp->config()->writeEntry( "ShowSAO", 		options()->drawSAO );
	kapp->config()->writeEntry( "ShowMess", 	options()->drawMessier );
	kapp->config()->writeEntry( "ShowMessImages", 	options()->drawMessImages );
	kapp->config()->writeEntry( "ShowNGC", 		options()->drawNGC );
	kapp->config()->writeEntry( "ShowIC", 		options()->drawIC );
	kapp->config()->writeEntry( "ShowCLines", options()->drawConstellLines );
	kapp->config()->writeEntry( "ShowCNames", options()->drawConstellNames );
	kapp->config()->writeEntry( "UseLatinConstellationNames", options()->useLatinConstellNames );
	kapp->config()->writeEntry( "UseLocalConstellationNames", options()->useLocalConstellNames );
	kapp->config()->writeEntry( "UseAbbrevConstellationNames", options()->useAbbrevConstellNames );
	kapp->config()->writeEntry( "ShowMilkyWay", options()->drawMilkyWay );
	kapp->config()->writeEntry( "ShowGrid", options()->drawGrid );
	kapp->config()->writeEntry( "ShowEquator", options()->drawEquator );
	kapp->config()->writeEntry( "ShowEcliptic", options()->drawEcliptic );
	kapp->config()->writeEntry( "ShowHorizon", options()->drawHorizon );
	kapp->config()->writeEntry( "ShowGround", options()->drawGround );
	kapp->config()->writeEntry( "ShowSun", 		options()->drawSun );
	kapp->config()->writeEntry( "ShowMoon", 	options()->drawMoon );
	kapp->config()->writeEntry( "ShowMercury", 	options()->drawMercury );
	kapp->config()->writeEntry( "ShowVenus", 	options()->drawVenus );
	kapp->config()->writeEntry( "ShowMars", 	options()->drawMars );
	kapp->config()->writeEntry( "ShowJupiter", 	options()->drawJupiter );
	kapp->config()->writeEntry( "ShowSaturn", 	options()->drawSaturn );
	kapp->config()->writeEntry( "ShowUranus", 	options()->drawUranus );
	kapp->config()->writeEntry( "ShowNeptune", 	options()->drawNeptune );
	kapp->config()->writeEntry( "ShowPluto", 	options()->drawPluto );
	kapp->config()->writeEntry( "ShowPlanets", 	options()->drawPlanets );
	kapp->config()->writeEntry( "ShowDeepSky", 	options()->drawDeepSky );
	kapp->config()->writeEntry( "IsTracking", 	options()->isTracking );
	if ( skymap->foundObject() != NULL ) {
		kapp->config()->writeEntry( "FocusObject",  skymap->foundObject()->name() );
	} else {
		kapp->config()->writeEntry( "FocusObject", i18n( "not focused on any object", "nothing" ) );
	}
	kapp->config()->writeEntry( "UseAltAz", 	options()->useAltAz );
	kapp->config()->writeEntry( "FocusRA", skymap->focus()->ra().Hours() );
	kapp->config()->writeEntry( "FocusDec", skymap->focus()->dec().Degrees() );
	kapp->config()->writeEntry( "SlewTimeScale", options()->slewTimeScale );
	kapp->config()->writeEntry( "ZoomLevel", data()->ZoomLevel );
	kapp->config()->writeEntry( "windowWidth", width() );
	kapp->config()->writeEntry( "windowHeight", height() );
	kapp->config()->writeEntry( "magLimitDrawStar", 	 options()->magLimitDrawStar );
	kapp->config()->writeEntry( "magLimitDrawStarInfo",options()->magLimitDrawStarInfo );
	kapp->config()->writeEntry( "magLimitHideStar",options()->magLimitHideStar );
	kapp->config()->writeEntry( "drawStarName", options()->drawStarName );
	kapp->config()->writeEntry( "drawPlanetName", options()->drawPlanetName );
	kapp->config()->writeEntry( "drawPlanetImage", options()->drawPlanetImage );
	kapp->config()->writeEntry( "drawStarMagnitude",   options()->drawStarMagnitude );
	kapp->config()->writeEntry( "UseRefraction", options()->useRefraction );
	kapp->config()->writeEntry( "AnimateSlewing", options()->useAnimatedSlewing );
	kapp->config()->writeEntry( "HideOnSlew", options()->hideOnSlew );
	kapp->config()->writeEntry( "HideStars", options()->hideStars );
	kapp->config()->writeEntry( "HidePlanets", options()->hidePlanets );
	kapp->config()->writeEntry( "HideMess", options()->hideMess );
	kapp->config()->writeEntry( "HideNGC", options()->hideNGC );
	kapp->config()->writeEntry( "HideIC", options()->hideIC );
	kapp->config()->writeEntry( "HideMW", options()->hideMW );
	kapp->config()->writeEntry( "HideCNames", options()->hideCNames );
	kapp->config()->writeEntry( "HideCLines", options()->hideCLines );
	kapp->config()->writeEntry( "HideGrid", options()->hideGrid );
	kapp->config()->sync();

//	colorActions.clear(); //dispose of colorActions...

	delete pd;
	delete Location;
	delete skymap;
	delete clock;
	delete Blank;
	delete centralWidget;
}

void KStars::changeTime( QDate newDate, QTime newTime ) {
	clock->stop();

	//Make sure Numbers, Moon, planets, and sky objects are updated immediately
	data()->LastNumUpdate = -1000000.0;
	data()->LastMoonUpdate = -1000000.0;
	data()->LastPlanetUpdate = -1000000.0;
	data()->LastSkyUpdate = -1000000.0;

	clock->setUTC(QDateTime(newDate, newTime).addSecs(int(-3600 * geo()->TZ()) ));

}

void KStars::clearCachedFindDialog() {
	if ( findDialog  ) {  // dialog is in memory
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

void KStars::updateTime( void ) {
	data()->updateTime(clock, geo(), skymap);

	showFocusCoords();

	infoPanel->timeChanged(data()->UTime, data()->LTime, data()->LST, data()->CurrentDate);
}

SkyObject* KStars::getObjectNamed( QString name ) {
	if ( (name== "star") || (name== "nothing") || name.isEmpty() ) return NULL;
	if ( name== data()->Moon->name() ) return data()->Moon;

	SkyObject *so = data()->PC.findByName(name);

	if (so != 0)
		return so;

	//Stars
	for ( unsigned int i=0; i<data()->starList.count(); ++i ) {
		if ( name==data()->starList.at(i)->name() ) return data()->starList.at(i);
	}

	//Deep-sky catalogs
	for ( unsigned int i=0; i<data()->deepSkyList.count(); ++i ) {
		if ( name==data()->deepSkyList.at(i)->name() ) return data()->deepSkyList.at(i);
	}

	//Constellations
	for ( unsigned int i=0; i<data()->cnameList.count(); ++i ) {
		if ( name==data()->cnameList.at(i)->name() ) return data()->cnameList.at(i);
	}

	//reach here only if argument is not matched
	return NULL;
}

void KStars::showFocusCoords( void ) {
	//display object info in infoPanel
	QString oname;

	oname = i18n( "nothing" );
	if ( skymap->foundObject() != NULL && options()->isTracking ) {
		oname = skymap->foundObject()->translatedName();
		//add genetive name for stars
	  if ( skymap->foundObject()->type()==0 && skymap->foundObject()->name2().length() )
			oname += " (" + skymap->foundObject()->name2() + ")";
	}

	//
	//This is ugly, got to find a way to change this. But for now.
	//
	infoPanel->focusObjChanged(oname);
	
	infoPanel->focusCoordChanged(skymap->focus());

}

QString KStars::getDateString( QDate date ) {
  QString dummy;
  QString dateString = dummy.sprintf( "%02d / %02d / %04d",
			  date.month(), date.day(), date.year() );
  return dateString;
}

void KStars::setAltAz(double alt, double az) {
 	skymap->setFocusAltAz(alt,az);
}

void KStars::lookTowards (QString direction) {
  QString dir = direction.lower();
	if (dir == "zenith" || dir=="z") skymap->invokeKey( KAccel::stringToKey( "Z" ) );
	else if (dir == "north" || dir=="n") skymap->invokeKey( KAccel::stringToKey( "N" ) );
	else if (dir == "east"  || dir=="e") skymap->invokeKey( KAccel::stringToKey( "E" ) );
	else if (dir == "south" || dir=="s") skymap->invokeKey( KAccel::stringToKey( "S" ) );
	else if (dir == "west"  || dir=="w") skymap->invokeKey( KAccel::stringToKey( "W" ) );
	else if (dir == "northeast" || dir=="ne") {
		skymap->setClickedObject( NULL );
		skymap->clickedPoint()->setAlt( 15.0 ); skymap->clickedPoint()->setAz( 45.0 );
		skymap->clickedPoint()->HorizontalToEquatorial( data()->LSTh, geo()->lat() );
		skymap->slotCenter();
	} else if (dir == "southeast" || dir=="se") {
		skymap->setClickedObject( NULL );
		skymap->clickedPoint()->setAlt( 15.0 ); skymap->clickedPoint()->setAz( 135.0 );
		skymap->clickedPoint()->HorizontalToEquatorial( data()->LSTh, geo()->lat() );
		skymap->slotCenter();
	} else if (dir == "southwest" || dir=="sw") {
		skymap->setClickedObject( NULL );
		skymap->clickedPoint()->setAlt( 15.0 ); skymap->clickedPoint()->setAz( 225.0 );
		skymap->clickedPoint()->HorizontalToEquatorial( data()->LSTh, geo()->lat() );
		skymap->slotCenter();
	} else if (dir == "northwest" || dir=="nw") {
		skymap->setClickedObject( NULL );
		skymap->clickedPoint()->setAlt( 15.0 ); skymap->clickedPoint()->setAz( 315.0 );
		skymap->clickedPoint()->HorizontalToEquatorial( data()->LSTh, geo()->lat() );
		skymap->slotCenter();
	} else {
		SkyObject *target = getObjectNamed( direction );
		if ( target != NULL ) {
			skymap->setClickedObject( target );
			skymap->setClickedPoint( target );
			skymap->slotCenter();
		}
	}
}

void KStars::setLocalTime(int yr, int mth, int day, int hr, int min, int sec) {
	changeTime( QDate(yr, mth, day), QTime(hr,min,sec));
}


//
//This is bogus, but until all the data members of KStarsData migrate to the
//class where they _really_ belong, we'll do the forwarding.
//
void KStars::setHourAngle() {
	data()->HourAngle.setH( data()->LSTh.Hours() - skymap->focus()->ra().Hours() );
}

void KStars::mapGetsFocus() { skymap->QWidget::setFocus(); }

void KStars::setLSTh( QDateTime UTC ) {
	data()->LST = KSUtils::UTtoLST( UTC, geo()->lng() );
	data()->LSTh.setH( data()->LST.hour(), data()->LST.minute(), data()->LST.second() );
}

dms KStars::LSTh() { return data()->LSTh; }

KStarsData* KStars::data() { return pd->kstarsData; }

#include "kstars.moc"
