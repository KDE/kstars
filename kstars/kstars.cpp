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
#include <kactioncollection.h>
#include <kiconloader.h>
#include <qpalette.h>
#include <kstatusbar.h>

#include "Options.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "kstarssplash.h"
#include "skymap.h"
#include "simclock.h"
#include "finddialog.h"
#include "ksutils.h"
#include "infoboxes.h"
#include "observinglist.h"
#include "imagesequence.h"
#include "toggleaction.h"

// to remove warnings
#include "indimenu.h"
#include "indidriver.h"

KStars::KStars( bool doSplash, bool clockrun, const QString &startdate ) :
	DCOPObject("KStarsInterface"), KMainWindow(),
	skymap(0), centralWidget(0), topLayout(0), viewToolBar(0), TimeStep(0),
	actCoordSys(0), colorActionMenu(0), fovActionMenu(0),
	AAVSODialog(0), findDialog(0), kns(0), 
	indimenu(0), indidriver(0), indiseq(0),
	DialogIsObsolete(false), StartClockRunning( clockrun ), StartDateString( startdate )
{
	pd = new privatedata(this);

	// we're nowhere near ready to take dcop calls
	kapp->dcopClient()->suspend();

	if ( doSplash ) {
		pd->kstarsData = new KStarsData();
		QObject::connect(pd->kstarsData, SIGNAL( initFinished(bool) ),
				this, SLOT( datainitFinished(bool) ) );

		pd->splash = new KStarsSplash(0, "Splash");
		QObject::connect(pd->splash, SIGNAL( closeWindow() ), kapp, SLOT( quit() ) );
		QObject::connect(pd->kstarsData, SIGNAL( progressText(QString) ),
				pd->splash, SLOT( setMessage(QString) ));
		pd->splash->show();
	}

	pd->kstarsData->initialize();

	//Set Geographic Location
	pd->kstarsData->setLocationFromOptions();

	//Pause the clock if the user gave the "--paused" arg
	if ( ! StartClockRunning ) pd->kstarsData->clock()->stop();
	
	//set up Dark color scheme for application windows
	DarkPalette = QPalette(QColor("red4"), QColor("DarkRed"));
	DarkPalette.setColor( QPalette::Normal, QColorGroup::Base, QColor( "black" ) );
	DarkPalette.setColor( QPalette::Normal, QColorGroup::Text, QColor( "red2" ) );
	DarkPalette.setColor( QPalette::Normal, QColorGroup::Highlight, QColor( "red2" ) );
	DarkPalette.setColor( QPalette::Normal, QColorGroup::HighlightedText, QColor( "black" ) );
	//store original color scheme
	OriginalPalette = QApplication::palette();

	#if ( __GLIBC__ >= 2 &&__GLIBC_MINOR__ >= 1 && !defined(__UCLIBC__))
	kdDebug() << "glibc >= 2.1 detected.  Using GNU extension sincos()" << endl;
	#else
	kdDebug() << "Did not find glibc >= 2.1.  Will use ANSI-compliant sin()/cos() functions." << endl;
	#endif

	obsList = new ObservingList( this, this );
}

KStars::~KStars()
{
	//store focus values in Options
	Options::setFocusRA( skymap->focus()->ra()->Hours() );
	Options::setFocusDec( skymap->focus()->dec()->Degrees() );

	//Store Window geometry in Options object
	Options::setWindowWidth( width() );
	Options::setWindowHeight( height() );

	//We need to explicitly save the colorscheme data to the config file
	data()->colorScheme()->saveToConfig( kapp->config() );

	//synch the config file with the Config object
	Options::writeConfig();

	clearCachedFindDialog();

	delete skymap;
	delete pd;
	delete centralWidget;
	delete AAVSODialog;
	delete indimenu;
	delete indidriver;
	delete indiseq;
	
	skymap = 0;
	pd = 0;
	centralWidget = 0;
	AAVSODialog = 0;
	indimenu = 0;
	indidriver = 0;
	indiseq = 0;
}

KStars::privatedata::~privatedata() {
	delete splash;
	delete kstarsData;

	splash = 0;
	kstarsData = 0;
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

void KStars::applyConfig() {
	if ( Options::isTracking() ) {
		actionCollection()->action("track_object")->setText( i18n( "Stop &Tracking" ) );
		actionCollection()->action("track_object")->setIconSet( BarIcon( "encrypted" ) );
	}

	//Toggle actions
	if ( Options::useAltAz() ) ((ToggleAction*)actionCollection()->action("coordsys"))->turnOff();
	((KToggleAction*)actionCollection()->action("show_time_box"))->setChecked( Options::showTimeBox() );
	((KToggleAction*)actionCollection()->action("show_location_box"))->setChecked( Options::showGeoBox() );
	((KToggleAction*)actionCollection()->action("show_focus_box"))->setChecked( Options::showFocusBox() );
	((KToggleAction*)actionCollection()->action("show_mainToolBar"))->setChecked( Options::showMainToolBar() );
	((KToggleAction*)actionCollection()->action("show_viewToolBar"))->setChecked( Options::showViewToolBar() );
	((KToggleAction*)actionCollection()->action("show_statusBar"))->setChecked( Options::showStatusBar() );
	((KToggleAction*)actionCollection()->action("show_sbAzAlt"))->setChecked( Options::showAltAzField() );
	((KToggleAction*)actionCollection()->action("show_sbRADec"))->setChecked( Options::showRADecField() );
	((KToggleAction*)actionCollection()->action("show_stars"))->setChecked( Options::showStars() );
	((KToggleAction*)actionCollection()->action("show_deepsky"))->setChecked( Options::showDeepSky() );
	((KToggleAction*)actionCollection()->action("show_planets"))->setChecked( Options::showPlanets() );
	((KToggleAction*)actionCollection()->action("show_clines"))->setChecked( Options::showCLines() );
	((KToggleAction*)actionCollection()->action("show_cnames"))->setChecked( Options::showCNames() );
	((KToggleAction*)actionCollection()->action("show_cbounds"))->setChecked( Options::showCBounds() );
	((KToggleAction*)actionCollection()->action("show_mw"))->setChecked( Options::showMilkyWay() );
	((KToggleAction*)actionCollection()->action("show_grid"))->setChecked( Options::showGrid() );
	((KToggleAction*)actionCollection()->action("show_horizon"))->setChecked( Options::showGround() );
	
	//color scheme
	pd->kstarsData->colorScheme()->loadFromConfig( kapp->config() );
	if ( Options::darkAppColors() ) {
		QApplication::setPalette( DarkPalette, true );
	} else {
		QApplication::setPalette( OriginalPalette, true );
	}

	//Infoboxes, toolbars, statusbars
	infoBoxes()->setVisible( Options::showInfoBoxes() );
	if ( ! Options::showMainToolBar() ) toolBar( "mainToolBar" )->hide();
	if ( ! Options::showViewToolBar() ) toolBar( "viewToolBar" )->hide();
	if ( ! Options::showAltAzField() ) statusBar()->removeItem(1);
	if ( ! Options::showRADecField() ) statusBar()->removeItem(2);

	//Geographic location
	setGeoLocation( Options::cityName(), Options::provinceName(), Options::countryName() );

	//Focus
	SkyObject *fo = data()->objectNamed( Options::focusObject() );
	if ( fo && fo != map()->focusObject() ) {
		map()->setClickedObject( fo );
		map()->setClickedPoint( fo );
		map()->slotCenter();
	}

	if ( ! fo ) {
		SkyPoint fp( Options::focusRA(), Options::focusDec() );
		if ( fp.ra()->Degrees() != map()->focus()->ra()->Degrees() || fp.dec()->Degrees() != map()->focus()->dec()->Degrees() ) {
			map()->setClickedPoint( &fp );
			map()->slotCenter();
		}
	}
}

void KStars::updateTime( const bool automaticDSTchange ) {
	dms oldLST( LST()->Degrees() );
	// Due to frequently use of this function save data and map pointers for speedup.
	// Save options and geo() to a pointer would not speedup because most of time options
	// and geo will accessed only one time.
	KStarsData *Data = data();
	SkyMap *Map = map();

	Data->updateTime( geo(), Map, automaticDSTchange );
	if ( infoBoxes()->timeChanged( Data->ut(), Data->lt(), LST() ) )
		Map->update();

	//We do this outside of kstarsdata just to get the coordinates
	//displayed in the infobox to update every second.
//	if ( !Options::isTracking() && LST()->Degrees() > oldLST.Degrees() ) {
//		int nSec = int( 3600.*( LST()->Hours() - oldLST.Hours() ) );
//		Map->focus()->setRA( Map->focus()->ra()->Hours() + double( nSec )/3600. );
//		if ( Options::useAltAz() ) Map->focus()->EquatorialToHorizontal( LST(), geo()->lat() );
//		Map->showFocusCoords();
//	}

	//If time is accelerated beyond slewTimescale, then the clock's timer is stopped,
	//so that it can be ticked manually after each update, in order to make each time
	//step exactly equal to the timeScale setting.
	//Wrap the call to manualTick() in a singleshot timer so that it doesn't get called until
	//the skymap has been completely updated.
	if ( Data->clock()->isManualMode() && Data->clock()->isActive() ) {
		QTimer::singleShot( 0, Data->clock(), SLOT( manualTick() ) );
	}
}

KStarsData* KStars::data( void ) { return pd->kstarsData; }

SkyMap* KStars::map( void )  { return skymap; }

InfoBoxes* KStars::infoBoxes( void )  { return map()->infoBoxes(); }

GeoLocation* KStars::geo() { return data()->geo(); }

void KStars::mapGetsFocus() { map()->QWidget::setFocus(); }

dms* KStars::LST() { return data()->LST; }

ObservingList* KStars::observingList() { return obsList; }

#include "kstars.moc"

