/***************************************************************************
                          kstarsinit.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Mon Feb 25 2002
    copyright            : (C) 2002 by Jason Harris
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
#include <dcopclient.h>
#include <kaccel.h>
#include <kiconloader.h>
#include <kstatusbar.h>
#include <ktip.h>

#include <kmessagebox.h>

//#include "infopanel.h"
#include "infoboxes.h"
#include "kstars.h"
#include "ksutils.h"
#include "toggleaction.h"

//This file contains functions that kstars calls at startup (except constructors).
//These functions are declared in kstars.h

void KStars::initActions() {
//File Menu:
	new KAction(i18n("New Window"), "window_new", KAccel::stringToKey( "Ctrl+N"  ),
			this, SLOT( newWindow() ), actionCollection(), "new_window");
	new KAction(i18n("Close Window"), KAccel::stringToKey( "Ctrl+W"  ),
			this, SLOT( closeWindow() ), actionCollection(), "close_window");
  KStdAction::print(this, SLOT( slotPrint() ), actionCollection(), "print" );
	KStdAction::quit(this, SLOT( close() ), actionCollection(), "quit" );

//Time Menu:
	new KAction( i18n( "Set Time to &Now" ), KAccel::stringToKey( "Ctrl+E"  ),
		this, SLOT( slotSetTimeToNow() ), actionCollection(), "time_to_now" );
	new KAction( i18n( "set clock to a new time", "&Set Time..." ), "clock", KAccel::stringToKey( "Ctrl+S"  ),
		this, SLOT( slotSetTime() ), actionCollection(), "time_dialog" );
	ToggleAction *actTimeRun = new ToggleAction( i18n( "Stop &Clock" ), BarIcon("player_pause"),
				i18n("Start &Clock"), BarIcon("1rightarrow"),
				0, this, SLOT( slotToggleTimer() ), actionCollection(), "timer_control" );
	actTimeRun->setOffToolTip( i18n( "Start Clock" ) );
	actTimeRun->setOnToolTip( i18n( "Stop Clock" ) );
	QObject::connect(clock, SIGNAL(clockStarted()), actTimeRun, SLOT(turnOn()) );
	QObject::connect(clock, SIGNAL(clockStopped()), actTimeRun, SLOT(turnOff()) );
//UpdateTime() if clock is stopped (so hidden objects get drawn)
	QObject::connect(clock, SIGNAL(clockStopped()), this, SLOT(updateTime()) );

//Focus Menu:
	new KAction(i18n( "&Zenith" ), KAccel::stringToKey( "Z" ),
			this, SLOT( slotPointFocus() ),  actionCollection(), "zenith");
	new KAction(i18n( "&North" ), KAccel::stringToKey( "N" ),
			this, SLOT( slotPointFocus() ),  actionCollection(), "north");
	new KAction(i18n( "&East" ), KAccel::stringToKey( "E" ),
			this, SLOT( slotPointFocus() ),  actionCollection(), "east");
	new KAction(i18n( "&South" ), KAccel::stringToKey( "S" ),
			this, SLOT( slotPointFocus() ),  actionCollection(), "south");
	new KAction(i18n( "&West" ), KAccel::stringToKey( "W" ),
			this, SLOT( slotPointFocus() ),  actionCollection(), "west");
	KAction *tmpAction = KStdAction::find( this, SLOT( slotFind() ),
												actionCollection(), "find_object" );
	tmpAction->setText( i18n( "&Find Object..." ) );
	tmpAction->setToolTip( i18n( "Find Object" ) );
	new KAction( i18n( "&Track Object" ), "decrypted", KAccel::stringToKey( "Ctrl+T"  ),
		this, SLOT( slotTrack() ), actionCollection(), "track_object" );
	new KAction( i18n( "Set Focus &Manually..." ), KAccel::stringToKey( "Ctrl+M" ),
			this, SLOT( slotManualFocus() ),  actionCollection(), "manual_focus" );

//View Menu:
	KStdAction::zoomIn(this, SLOT( slotZoomIn() ), actionCollection(), "zoom_in" );
	KStdAction::zoomOut(this, SLOT( slotZoomOut() ), actionCollection(), "zoom_out" );
	new KAction( i18n( "&Default Zoom" ), "viewmagfit.png", KAccel::stringToKey( "Ctrl+Z" ),
		this, SLOT( slotDefaultZoom() ), actionCollection(), "zoom_default" );
	actCoordSys = new ToggleAction( i18n("Horizontal &Coordinates"), i18n( "Equatorial &Coordinates" ),
			0, this, SLOT( slotCoordSys() ), actionCollection(), "coordsys" );
	if ( options()->useAltAz ) actCoordSys->turnOff();

//Settings Menu:
	//
	// MHH - 2002-01-13
	// Setting the slot in the KToggleAction constructor, connects the slot to
	// the activated signal instead of the toggled signal. This seems like a bug
	// to me, but ...
	//
	//Info Boxes option actions
	KToggleAction *a = new KToggleAction(i18n( "Show the information boxes", "Show Info Boxes"),
			0, 0, 0, actionCollection(), "show_boxes");
	a->setChecked( options()->showInfoBoxes );
	QObject::connect(a, SIGNAL( toggled(bool) ), infoBoxes(), SLOT(setVisible(bool)));
	QObject::connect(a, SIGNAL( toggled(bool) ), this, SLOT(slotShowGUIItem(bool)));
	infoBoxes()->setVisible( options()->showInfoBoxes );

	a = new KToggleAction(i18n( "Show time-related info box", "Show Time"),
			0, 0, 0, actionCollection(), "show_time_box");
	a->setChecked( options()->showTimeBox );
	QObject::connect(a, SIGNAL( toggled(bool) ), infoBoxes(), SLOT(showTimeBox(bool)));
	QObject::connect(a, SIGNAL( toggled(bool) ), this, SLOT(slotShowGUIItem(bool)));

	a = new KToggleAction(i18n( "Show focus-related info box", "Show Focus"),
			0, 0, 0, actionCollection(), "show_focus_box");
	a->setChecked( options()->showFocusBox );
	QObject::connect(a, SIGNAL( toggled(bool) ), infoBoxes(), SLOT(showFocusBox(bool)));
	QObject::connect(a, SIGNAL( toggled(bool) ), this, SLOT(slotShowGUIItem(bool)));

	a = new KToggleAction(i18n( "Show location-related info box", "Show Location"),
			0, 0, 0, actionCollection(), "show_location_box");
	a->setChecked( options()->showGeoBox );
	QObject::connect(a, SIGNAL( toggled(bool) ), infoBoxes(), SLOT(showGeoBox(bool)));
	QObject::connect(a, SIGNAL( toggled(bool) ), this, SLOT(slotShowGUIItem(bool)));

//Toolbar view options
	a = new KToggleAction(i18n( "Show Main Toolbar" ),
			0, 0, 0, actionCollection(), "show_mainToolBar");
	a->setChecked( options()->showMainToolBar );
	QObject::connect(a, SIGNAL( toggled(bool) ), this, SLOT(slotShowGUIItem(bool)));

	a = new KToggleAction(i18n( "Show View Toolbar" ),
			0, 0, 0, actionCollection(), "show_viewToolBar");
	a->setChecked( options()->showViewToolBar );
	QObject::connect(a, SIGNAL( toggled(bool) ), this, SLOT(slotShowGUIItem(bool)));

//Color scheme actions.  These are added to the "colorschemes" KActionMenu.
	colorActionMenu = new KActionMenu( i18n( "&Color Schemes" ), actionCollection(), "colorschemes" );
	addColorMenuItem( i18n( "&Default" ), "cs_default" );
	addColorMenuItem( i18n( "&Star Chart" ), "cs_chart" );
	addColorMenuItem( i18n( "&Night Vision" ), "cs_night" );
	addColorMenuItem( i18n( "&Moonless Night" ), "cs_moonless-night" );

//	colorActionMenu->insert( new KAction( i18n( "&Default" ), 0, this, SLOT( slotColorScheme() ), actionCollection(), "cs_default" ) );
//	colorActionMenu->insert( new KAction( i18n( "&Star Chart" ), 0, this, SLOT( slotColorScheme() ), actionCollection(), "cs_chart" ) );
//	colorActionMenu->insert( new KAction( i18n( "&Night Vision" ), 0, this, SLOT( slotColorScheme() ), actionCollection(), "cs_night" ) );

//Add any user-defined color schemes:
	QFile file;
	QString line, schemeName, filename;
	file.setName( locateLocal( "appdata", "colors.dat" ) ); //determine filename in local user KDE directory tree.
	if ( file.open( IO_ReadOnly ) ) {
		QTextStream stream( &file );

		while ( !stream.eof() ) {
			line = stream.readLine();
			schemeName = line.left( line.find( ':' ) );
			//I call it filename here, but it's used as the name of the action!
			filename = "cs_" + line.mid( line.find( ':' ) +1, line.find( '.' ) - line.find( ':' ) - 1 );
			addColorMenuItem( i18n( schemeName.local8Bit() ), filename.local8Bit() );

//			colorActionMenu->insert( new KAction( i18n( schemeName.local8Bit() ), 0,
//					this, SLOT( slotColorScheme() ), actionCollection(), filename.local8Bit() ) );
		}
		file.close();
	}

	//Add Target Symbol actions
	new KAction( i18n( "do not use a target symbol", "No Symbol" ), 0, 
			this, SLOT( slotTargetSymbol() ), actionCollection(), "target_symbol_none" );
	
	new KAction( i18n( "use a circle target symbol", "Circle" ), 0, 
			this, SLOT( slotTargetSymbol() ), actionCollection(), "target_symbol_circle" );
	
	new KAction( i18n( "use a crosshairs target symbol", "Crosshairs" ), 0, 
			this, SLOT( slotTargetSymbol() ), actionCollection(), "target_symbol_crosshairs" );
	
	new KAction( i18n( "use a bullseye target symbol", "Bullseye" ), 0, 
			this, SLOT( slotTargetSymbol() ), actionCollection(), "target_symbol_bullseye" );
	
	new KAction( i18n( "use a rectangle target symbol", "Rectangle" ), 0, 
			this, SLOT( slotTargetSymbol() ), actionCollection(), "target_symbol_rectangle" );
	
	//use custom icon earth.png for the geoLocator icon.  If it is not installed
  //for some reason, use standard icon gohome.png instead.
	QFile tempFile;
	if (KSUtils::openDataFile( tempFile, "earth.png" ) ) {
		tmpAction = new KAction( i18n( "Set Geographic Location..." ),
				tempFile.name(), KAccel::stringToKey( "Ctrl+G"  ),
				this, SLOT( slotGeoLocator() ), actionCollection(), "geolocation" );
		tmpAction->setToolTip( i18n( "Geographic Location" ) );
		tempFile.close();
	} else {
		new KAction( i18n( "Location on Earth", "&Geographic..." ), "gohome", KAccel::stringToKey( "Ctrl+G"  ),
				this, SLOT( slotGeoLocator() ), actionCollection(), "geolocation" );
	}

	KStdAction::preferences( this, SLOT( slotViewOps() ), actionCollection(), "configure" );


//Tools Menu:
	new KAction(i18n( "Calculator..."), KAccel::stringToKey ( "Ctrl+C"),
			this, SLOT( slotCalculator() ), actionCollection(), "astrocalculator");

   // enable action only if file was loaded and processed successfully.   
   if (!data()->VariableStarsList.isEmpty())
   new KAction(i18n( "AAVSO Light Curves..."), KAccel::stringToKey ( "Ctrl+V"),
			this, SLOT( slotLCGenerator() ), actionCollection(), "lightcurvegenerator");

	new KAction(i18n( "Altitude vs. Time..."), KAccel::stringToKey ( "Ctrl+A"),
			                           this, SLOT( slotElTs() ), actionCollection(), "altitude_vs_time");

	new KAction(i18n( "What's up tonight..."), KAccel::stringToKey("Ctrl+U"),
			                           this, SLOT(slotWUT()), actionCollection(), "whats_up_tonight");

//Help Menu:
	new KAction( i18n( "Tip of the Day" ), "idea", 0,
			this, SLOT( slotTipOfDay() ), actionCollection(), "help_tipofday" );

//Handbook toolBar item:
	new KAction( i18n( "&Handbook" ), "contents", KAccel::stringToKey( "F1"  ),
			this, SLOT( appHelpActivated() ), actionCollection(), "handbook" );

//
//viewToolBar actions:
//

//show_stars:
	if (KSUtils::openDataFile( tempFile, "show_stars.png" ) ) {
		KToggleAction *a = new KToggleAction( i18n( "Toggle Stars" ),
				tempFile.name(), 0,
				this, SLOT( slotViewToolBar() ), actionCollection(), "show_stars" );
		a->setChecked( options()->drawSAO );
		tempFile.close();
	} else {
		KToggleAction *a = new KToggleAction( i18n( "Toggle Stars" ), "wizard", 0,
				this, SLOT( slotViewToolBar() ), actionCollection(), "show_stars" );
		a->setChecked( options()->drawSAO );
	}

//show_deepsky:
	if (KSUtils::openDataFile( tempFile, "show_deepsky.png" ) ) {
		KToggleAction *a = new KToggleAction( i18n( "Toggle Deep Sky Objects" ),
				tempFile.name(), 0,
				this, SLOT( slotViewToolBar() ), actionCollection(), "show_deepsky" );
		a->setChecked( options()->drawDeepSky );
		tempFile.close();
	} else {
		KToggleAction *a = new KToggleAction( i18n( "Toggle Deep Sky Objects" ), "wizard", 0,
				this, SLOT( slotViewToolBar() ), actionCollection(), "show_deepsky" );
		a->setChecked( options()->drawDeepSky );
	}

//show_planets:
	if (KSUtils::openDataFile( tempFile, "show_planets.png" ) ) {
		KToggleAction *a = new KToggleAction( i18n( "Toggle Solar System" ),
				tempFile.name(), 0,
				this, SLOT( slotViewToolBar() ), actionCollection(), "show_planets" );
		a->setChecked( options()->drawPlanets );
		tempFile.close();
	} else {
		KToggleAction *a = new KToggleAction( i18n( "Toggle Solar System" ), "wizard", 0,
				this, SLOT( slotViewToolBar() ), actionCollection(), "show_planets" );
		a->setChecked( options()->drawPlanets );
	}

//show_clines:
	if (KSUtils::openDataFile( tempFile, "show_clines.png" ) ) {
		KToggleAction *a = new KToggleAction( i18n( "Toggle Constellation Lines" ),
				tempFile.name(), 0,
				this, SLOT( slotViewToolBar() ), actionCollection(), "show_clines" );
		a->setChecked( options()->drawConstellLines );
		tempFile.close();
	} else {
		KToggleAction *a = new KToggleAction( i18n( "Toggle Constellation Lines" ), "wizard", 0,
				this, SLOT( slotViewToolBar() ), actionCollection(), "show_clines" );
		a->setChecked( options()->drawConstellLines );
	}

//show_cnames:
	if (KSUtils::openDataFile( tempFile, "show_cnames.png" ) ) {
		KToggleAction *a = new KToggleAction( i18n( "Toggle Constellation Names" ),
				tempFile.name(), 0,
				this, SLOT( slotViewToolBar() ), actionCollection(), "show_cnames" );
		a->setChecked( options()->drawConstellNames );
		tempFile.close();
	} else {
		KToggleAction *a = new KToggleAction( i18n( "Toggle Constellation Lines" ), "wizard", 0,
				this, SLOT( slotViewToolBar() ), actionCollection(), "show_cnames" );
		a->setChecked( options()->drawConstellNames );
	}

//show_mw:
	if (KSUtils::openDataFile( tempFile, "show_mw.png" ) ) {
		KToggleAction *a = new KToggleAction( i18n( "Toggle Milky Way" ),
				tempFile.name(), 0,
				this, SLOT( slotViewToolBar() ), actionCollection(), "show_mw" );
		a->setChecked( options()->drawMilkyWay );
		tempFile.close();
	} else {
		KToggleAction *a = new KToggleAction( i18n( "Toggle Milky Way" ), "wizard", 0,
				this, SLOT( slotViewToolBar() ), actionCollection(), "show_mw" );
		a->setChecked( options()->drawMilkyWay );
	}

//show_grid:
	if (KSUtils::openDataFile( tempFile, "show_grid.png" ) ) {
		KToggleAction *a = new KToggleAction( i18n( "Toggle Coordinate Grid" ),
				tempFile.name(), 0,
				this, SLOT( slotViewToolBar() ), actionCollection(), "show_grid" );
		tempFile.close();
		a->setChecked( options()->drawGrid );
	} else {
		KToggleAction *a = new KToggleAction( i18n( "Toggle Coordinate Grid" ), "wizard", 0,
				this, SLOT( slotViewToolBar() ), actionCollection(), "show_grid" );
		a->setChecked( options()->drawGrid );
	}

//show_horizon:
	if (KSUtils::openDataFile( tempFile, "show_horiz.png" ) ) {
		KToggleAction *a = new KToggleAction( i18n( "Toggle Ground" ),
				tempFile.name(), 0,
				this, SLOT( slotViewToolBar() ), actionCollection(), "show_horizon" );
		tempFile.close();
		a->setChecked( options()->drawGround );
	} else {
		KToggleAction *a = new KToggleAction( i18n( "Toggle Ground" ), "wizard", 0,
				this, SLOT( slotViewToolBar() ), actionCollection(), "show_horizon" );
		a->setChecked( options()->drawGround );
	}
}

void KStars::initStatusBar() {
	statusBar()->insertItem( i18n( " Welcome to KStars " ), 0, 1, true );
	statusBar()->setItemAlignment( 0, AlignLeft | AlignVCenter );
	QString s = "00h 00m 00s,   +00d 00\' 00\"";

	statusBar()->insertItem( s, 1, 1, true );
	statusBar()->setItemAlignment( 1, AlignRight | AlignVCenter );
	statusBar()->setItemFixed( 1, -1 );
}

void KStars::initGuides(KSNumbers *num)
{
	// Define the Celestial Equator
	for ( unsigned int i=0; i<NCIRCLE; ++i ) {
		SkyPoint *o = new SkyPoint( i*24./NCIRCLE, 0.0 );
		o->EquatorialToHorizontal( data()->LSTh, geo()->lat() );
		data()->Equator.append( o );
	}

  // Define the horizon.
  // Use the celestial Equator as a convenient starting point, but instead of RA and Dec,
  // interpret the coordinates as azimuth and altitude, and then convert to RA, dec.
  // The horizon will be redefined whenever the positions of sky objects are updated.
	dms temp( 0.0 );
	for (SkyPoint *point = data()->Equator.first(); point; point = data()->Equator.next()) {
		double sinlat, coslat, sindec, cosdec, sinAz, cosAz;
		double HARad;
		dms dec, HA, RA, Az;
		Az = dms(*(point->ra()));
		
		Az.SinCos( sinAz, cosAz );
		geo()->lat()->SinCos( sinlat, coslat );
		
		dec.setRadians( asin( coslat*cosAz ) );
		dec.SinCos( sindec, cosdec );
		
		HARad = acos( -1.0*(sinlat*sindec)/(coslat*cosdec) );
		if ( sinAz > 0.0 ) { HARad = 2.0*dms::PI - HARad; }
		HA.setRadians( HARad );
		RA = LSTh()->Degrees() - HA.Degrees();

		SkyPoint *o = new SkyPoint( RA, dec );
		o->setAlt( 0.0 );
		o->setAz( Az );

		data()->Horizon.append( o );

		//Define the Ecliptic (use the same ListIteration; interpret coordinates as Ecliptic long/lat)
		o = new SkyPoint( 0.0, 0.0 );
		o->setFromEcliptic( num->obliquity(), point->ra(), &temp );
		o->EquatorialToHorizontal( data()->LSTh, geo()->lat() );
		data()->Ecliptic.append( o );
	}
}

void KStars::datainitFinished(bool worked) {
	if (!worked) {
		kapp->quit();
		return;
	}

	if (pd->splash) {
		delete pd->splash;
		pd->splash = 0;
	}

	pd->buildGUI();
	updateTime();
	clock->start();
	
	show();

//Check whether initial position is below the horizon.
//We used to just call slotCenter() in buildGUI() which performs this check.  
//However, on some systems, if the messagebox is shown before show() is called, 
//the program exits.  It does not crash (at least there are no error messages),
//it simply exits.  Very strange.
	if ( options()->useAltAz && options()->drawGround &&
			map()->focus()->alt()->Degrees() < -1.0 ) {
		QString caption = i18n( "Initial Position is Below Horizon" );
		QString message = i18n( "The initial position is below the horizon.\nWould you like to reset to the default position?" );
		if ( KMessageBox::warningYesNo( this, message, caption, 
				KStdGuiItem::yes(), KStdGuiItem::no(), "dag_start_below_horiz" ) == KMessageBox::Yes ) {
			map()->setClickedObject( NULL );
			map()->setFoundObject( NULL );
			options()->isTracking = false;
			options()->setSnapNextFocus(true);
			
			SkyPoint DefaultFocus;
			DefaultFocus.setAz( 180.0 );
			DefaultFocus.setAlt( 45.0 );
			DefaultFocus.HorizontalToEquatorial( LSTh(), geo()->lat() );
			map()->setDestination( &DefaultFocus );
		}
	}

	//If there is a foundObject() and it is a SS body, add a temporary Trail to it.
	if ( map()->foundObject() && data()->isSolarSystem( map()->foundObject() ) 
			&& options()->useAutoTrail ) { 
		((KSPlanetBase*)map()->foundObject())->addToTrail();
		data()->temporaryTrail = true;
	}

// just show dialog if option is set (don't force it)	
	KTipDialog::showTip( "kstars/tips" );
}

void KStars::privatedata::buildGUI() {
	// here we get the preloaded data (stars, constellations etc.)

	//Instantiate the SimClock object
	ks->clock = new SimClock(ks);

	//create the widgets
	ks->centralWidget = new QWidget( ks );
	ks->setCentralWidget( ks->centralWidget );

   //set AAVSO modeless dialog pointer to 0
   ks->AAVSODialog = 0;

	ks->IBoxes = new InfoBoxes( ks->options()->windowWidth, ks->options()->windowHeight,
			ks->options()->posTimeBox, ks->options()->shadeTimeBox,
			ks->options()->posGeoBox, ks->options()->shadeGeoBox,
			ks->options()->posFocusBox, ks->options()->shadeFocusBox,
			ks->options()->colorScheme()->colorNamed( "BoxTextColor" ),
			ks->options()->colorScheme()->colorNamed( "BoxGrabColor" ),
			ks->options()->colorScheme()->colorNamed( "BoxBGColor" ) );

	ks->infoBoxes()->showTimeBox( ks->options()->showTimeBox );
	ks->infoBoxes()->showFocusBox( ks->options()->showFocusBox );
	ks->infoBoxes()->showGeoBox( ks->options()->showGeoBox );
	
	ks->infoBoxes()->timeBox()->setAnchorFlag( ks->options()->stickyTimeBox );
	ks->infoBoxes()->geoBox()->setAnchorFlag( ks->options()->stickyGeoBox );
	ks->infoBoxes()->focusBox()->setAnchorFlag( ks->options()->stickyFocusBox );
	
	ks->infoBoxes()->geoChanged( ks->geo() );

	connect( ks->infoBoxes()->timeBox(),  SIGNAL( shaded(bool) ), ks, SLOT( saveTimeBoxShaded(bool) ) );
	connect( ks->infoBoxes()->geoBox(),   SIGNAL( shaded(bool) ), ks, SLOT( saveGeoBoxShaded(bool) ) );
	connect( ks->infoBoxes()->focusBox(), SIGNAL( shaded(bool) ), ks, SLOT( saveFocusBoxShaded(bool) ) );
	connect( ks->infoBoxes()->timeBox(),  SIGNAL( moved(QPoint) ), ks, SLOT( saveTimeBoxPos(QPoint) ) );
	connect( ks->infoBoxes()->geoBox(),   SIGNAL( moved(QPoint) ), ks, SLOT( saveGeoBoxPos(QPoint) ) );
	connect( ks->infoBoxes()->focusBox(), SIGNAL( moved(QPoint) ), ks, SLOT( saveFocusBoxPos(QPoint) ) );
	
	ks->initStatusBar();
	ks->initActions();

	ks->skymap = new SkyMap( ks->centralWidget );
	// update skymap if KStarsData send update signal
	QObject::connect(kstarsData, SIGNAL( update() ), ks->skymap, SLOT( Update() ) );

	// get focus of keyboard and mouse actions (for example zoom in with +)
	ks->map()->QWidget::setFocus();

	// create the layout of the central widget
	ks->topLayout = new QVBoxLayout( ks->centralWidget );
	ks->topLayout->addWidget( ks->skymap );

	// 2nd parameter must be false, or plugActionList won't work!
	ks->createGUI("kstarsui.rc", false);

	//Initialize show/hide state of toolbars.
	//These were in initActions, but they must appear after createGUI...
	if ( !ks->options()->showMainToolBar ) ks->toolBar( "mainToolBar" )->hide();
	if ( !ks->options()->showViewToolBar ) ks->toolBar( "viewToolBar" )->hide();

	ks->TimeStep = new TimeStepBox( ks->toolBar() );
	ks->toolBar()->insertWidget( 0, 6, ks->TimeStep, 15 );

//Changing the timestep needs to propagate to the clock, check if slew mode should be
//(dis)engaged, and return input focus to the skymap.
	connect( ks->TimeStep, SIGNAL( scaleChanged( float ) ), ks->data(), SLOT( setTimeDirection( float ) ) );
	connect( ks->TimeStep, SIGNAL( scaleChanged( float ) ), ks->clock, SLOT( setScale( float )) );
	connect( ks->TimeStep, SIGNAL( scaleChanged( float ) ), ks->skymap, SLOT( slotClockSlewing() ) );
	connect( ks->TimeStep, SIGNAL( scaleChanged( float ) ), ks, SLOT( mapGetsFocus() ) );

	ks->resize( ks->options()->windowWidth, ks->options()->windowHeight );

	// initialize clock with current time/date
	ks->slotSetTimeToNow();

	//Define the celestial equator, horizon and ecliptic
	KSNumbers tempnum(ks->clock->JD());
	ks->initGuides(&tempnum);

	//Connect the clock.
	QObject::connect( ks->clock, SIGNAL( timeAdvanced() ), ks, SLOT( updateTime() ) );
	QObject::connect( ks->clock, SIGNAL( timeChanged() ), ks, SLOT( updateTime() ) );

	// Connect cache function
	QObject::connect( kstarsData, SIGNAL( clearCache() ), ks, SLOT( clearCachedFindDialog() ) );

	SkyPoint newPoint;
	if ( ks->useDefaultOptions ) {
		newPoint.setAz( ks->options()->focusRA );
		newPoint.setAlt( ks->options()->focusDec + 0.0001 );
		newPoint.HorizontalToEquatorial( ks->LSTh(), ks->geo()->lat() );
	} else {
		newPoint.set( ks->options()->focusRA, ks->options()->focusDec );
	}

//need to set foundObject before updateTime, otherwise tracking is set to false
	if ( (ks->options()->focusObject != i18n( "star" ) ) &&
		     (ks->options()->focusObject != i18n( "nothing" ) ) )
			ks->map()->setFoundObject( ks->getObjectNamed( ks->options()->focusObject ) );

	ks->updateTime();

	//Set focus of Skymap to value stored in config.
	//Set default position in case stored focus is below horizon
//	SkyPoint DefaultFocus;
//	DefaultFocus.setAz( 180.0 );
//	DefaultFocus.setAlt( 45.0 );
//	DefaultFocus.HorizontalToEquatorial( ks->LSTh(), ks->geo()->lat() );
//	ks->map()->setDestination( &DefaultFocus );

	//if user was tracking last time, track on same object now.
	if ( ks->options()->isTracking ) {
		if ( (ks->options()->focusObject== i18n( "star" ) ) ||
		     (ks->options()->focusObject== i18n( "nothing" ) ) ) {
			ks->map()->setClickedPoint( &newPoint );
		} else {
			ks->map()->setClickedObject( ks->getObjectNamed( ks->options()->focusObject ) );
			if ( ks->map()->clickedObject() ) {
				ks->map()->setClickedPoint( ks->map()->clickedObject() );
			} else {
				ks->map()->setClickedPoint( &newPoint );
			}
		}
//		ks->map()->slotCenter();
	} else {
		ks->map()->setClickedPoint( &newPoint );
//		ks->map()->slotCenter();
	}
	
	if ( ks->options()->focusObject== i18n( "star" ) ) ks->options()->focusObject = i18n( "nothing" );
	ks->infoBoxes()->focusObjChanged( ks->options()->focusObject );
	
	ks->map()->setDestination( ks->map()->clickedPoint() );
	ks->map()->destination()->EquatorialToHorizontal( ks->LSTh(), ks->geo()->lat() );
	ks->map()->setFocus( ks->map()->destination() );
	ks->map()->focus()->EquatorialToHorizontal( ks->LSTh(), ks->geo()->lat() );

	ks->setHourAngle();

	ks->map()->setOldFocus( ks->map()->focus() );
	ks->map()->oldfocus()->setAz( ks->map()->focus()->az()->Degrees() );
	ks->map()->oldfocus()->setAlt( ks->map()->focus()->alt()->Degrees() );

	kapp->dcopClient()->resume();

}
