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
#include <kconfig.h>
#include <kiconloader.h>
#include <kmessagebox.h>
#include <kstatusbar.h>
#include <kstdaction.h>
#include <ktip.h>

#include <qtooltip.h>
#include <qtextstream.h>

#include "infopanel.h"
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
	new KAction( i18n( "&Set Time..." ), "clock", KAccel::stringToKey( "Ctrl+S"  ),
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
//	new KAction( i18n( "Equatorial &Coordinates" ), KAccel::stringToKey( "C" ),
//			this, SLOT( slotCoordSys() ),  actionCollection(), "coordsys" );
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
	//Info Panel option actions
	KToggleAction *a = new KToggleAction(i18n( "Show the information panel", "Show Infopanel"),
			0, 0, 0, actionCollection(), "show_panel");
	a->setChecked( options()->showInfoPanel );
	QObject::connect(a, SIGNAL( toggled(bool) ), infoPanel, SLOT(showInfoPanel(bool)));
	QObject::connect(a, SIGNAL( toggled(bool) ), this, SLOT(slotShowGUIItem(bool)));
	infoPanel->showInfoPanel( options()->showInfoPanel );

	a = new KToggleAction(i18n( "Show time-related info panel items", "Show Time"),
			0, 0, 0, actionCollection(), "show_time_panel");
	a->setChecked( options()->showIPTime );
	QObject::connect(a, SIGNAL( toggled(bool) ), infoPanel, SLOT(showTimeBar(bool)));
	QObject::connect(a, SIGNAL( toggled(bool) ), this, SLOT(slotShowGUIItem(bool)));
	infoPanel->showTimeBar( options()->showIPTime );

	a = new KToggleAction(i18n( "Show focus-related info panel items", "Show Focus"),
			0, 0, 0, actionCollection(), "show_focus_panel");
	a->setChecked( options()->showIPFocus );
	QObject::connect(a, SIGNAL( toggled(bool) ), infoPanel, SLOT(showFocusBar(bool)));
	QObject::connect(a, SIGNAL( toggled(bool) ), this, SLOT(slotShowGUIItem(bool)));
	infoPanel->showFocusBar( options()->showIPFocus );

	a = new KToggleAction(i18n( "Show location-related info panel items", "Show Location"),
			0, 0, 0, actionCollection(), "show_location_panel");
	a->setChecked( options()->showIPGeo );
	QObject::connect(a, SIGNAL( toggled(bool) ), infoPanel, SLOT(showLocationBar(bool)));
	QObject::connect(a, SIGNAL( toggled(bool) ), this, SLOT(slotShowGUIItem(bool)));
	infoPanel->showLocationBar( options()->showIPGeo );

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
	colorActionMenu->insert( new KAction( i18n( "&Default" ), 0, this, SLOT( slotColorScheme() ), actionCollection(), "cs_default" ) );
	colorActionMenu->insert( new KAction( i18n( "&Star Chart" ), 0, this, SLOT( slotColorScheme() ), actionCollection(), "cs_chart" ) );
	colorActionMenu->insert( new KAction( i18n( "&Night Vision" ), 0, this, SLOT( slotColorScheme() ), actionCollection(), "cs_night" ) );

//Add any user-defined color schemes:
	QFile file;
	QString line, schemeName, filename;
	file.setName( locateLocal( "appdata", "colors.dat" ) ); //determine filename in local user KDE directory tree.
	if ( file.open( IO_ReadOnly ) ) {
		QTextStream stream( &file );

		while ( !stream.eof() ) {
			line = stream.readLine();
			schemeName = line.left( line.find( ':' ) );
			filename = "cs_" + line.mid( line.find( ':' ) +1, line.find( '.' ) - line.find( ':' ) - 1 );
			colorActionMenu->insert( new KAction( i18n( schemeName.local8Bit() ), 0,
					this, SLOT( slotColorScheme() ), actionCollection(), filename.local8Bit() ) );
		}
		file.close();
	}


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


//Help Menu:
	new KAction( i18n( "Tip of the Day..." ), "idea", 0,
			this, SLOT( slotTipOfDay() ), actionCollection(), "help_tipofday" );

//Handbook toolBar item:
	new KAction( i18n( "&Handbook" ), "contents", KAccel::stringToKey( "F1"  ),
			this, SLOT( appHelpActivated() ), actionCollection(), "handbook" );

//viewToolBar actions:

//show_stars:
	if (KSUtils::openDataFile( tempFile, "show_stars.png" ) ) {
		new KAction( i18n( "Toggle Stars" ),
				tempFile.name(), 0,
				this, SLOT( slotViewToolBar() ), actionCollection(), "show_stars" );
		tempFile.close();
	} else {
		new KAction( i18n( "Toggle Stars" ), "wizard", 0,
				this, SLOT( slotViewToolBar() ), actionCollection(), "show_stars" );
	}

//show_deepsky:
	if (KSUtils::openDataFile( tempFile, "show_deepsky.png" ) ) {
		new KAction( i18n( "Toggle Deep Sky Objects" ),
				tempFile.name(), 0,
				this, SLOT( slotViewToolBar() ), actionCollection(), "show_deepsky" );
		tempFile.close();
	} else {
		new KAction( i18n( "Toggle Deep Sky Objects" ), "wizard", 0,
				this, SLOT( slotViewToolBar() ), actionCollection(), "show_deepsky" );
	}

//show_planets:
	if (KSUtils::openDataFile( tempFile, "show_planets.png" ) ) {
		new KAction( i18n( "Toggle Planets" ),
				tempFile.name(), 0,
				this, SLOT( slotViewToolBar() ), actionCollection(), "show_planets" );
		tempFile.close();
	} else {
		new KAction( i18n( "Toggle Planets" ), "wizard", 0,
				this, SLOT( slotViewToolBar() ), actionCollection(), "show_planets" );
	}

//show_clines:
	if (KSUtils::openDataFile( tempFile, "show_clines.png" ) ) {
		new KAction( i18n( "Toggle Constellation Lines" ),
				tempFile.name(), 0,
				this, SLOT( slotViewToolBar() ), actionCollection(), "show_clines" );
		tempFile.close();
	} else {
		new KAction( i18n( "Toggle Constellation Lines" ), "wizard", 0,
				this, SLOT( slotViewToolBar() ), actionCollection(), "show_clines" );
	}

//show_cnames:
	if (KSUtils::openDataFile( tempFile, "show_cnames.png" ) ) {
		new KAction( i18n( "Toggle Constellation Names" ),
				tempFile.name(), 0,
				this, SLOT( slotViewToolBar() ), actionCollection(), "show_cnames" );
		tempFile.close();
	} else {
		new KAction( i18n( "Toggle Constellation Lines" ), "wizard", 0,
				this, SLOT( slotViewToolBar() ), actionCollection(), "show_cnames" );
	}

//show_mw:
	if (KSUtils::openDataFile( tempFile, "show_mw.png" ) ) {
		new KAction( i18n( "Toggle Milky Way" ),
				tempFile.name(), 0,
				this, SLOT( slotViewToolBar() ), actionCollection(), "show_mw" );
		tempFile.close();
	} else {
		new KAction( i18n( "Toggle Milky Way" ), "wizard", 0,
				this, SLOT( slotViewToolBar() ), actionCollection(), "show_mw" );
	}

//show_grid:
	if (KSUtils::openDataFile( tempFile, "show_grid.png" ) ) {
		new KAction( i18n( "Toggle Coordinate Grid" ),
				tempFile.name(), 0,
				this, SLOT( slotViewToolBar() ), actionCollection(), "show_grid" );
		tempFile.close();
	} else {
		new KAction( i18n( "Toggle Coordinate Grid" ), "wizard", 0,
				this, SLOT( slotViewToolBar() ), actionCollection(), "show_grid" );
	}

//show_horizon:
	if (KSUtils::openDataFile( tempFile, "show_horiz.png" ) ) {
		new KAction( i18n( "Toggle Horizon" ),
				tempFile.name(), 0,
				this, SLOT( slotViewToolBar() ), actionCollection(), "show_horizon" );
		tempFile.close();
	} else {
		new KAction( i18n( "Toggle Horizon" ), "wizard", 0,
				this, SLOT( slotViewToolBar() ), actionCollection(), "show_horizon" );
	}
}

void KStars::initStatusBar() {
	statusBar()->insertItem( i18n( " Welcome to KStars " ), 0, 1, true );
	statusBar()->setItemAlignment( 0, AlignLeft | AlignVCenter );
	QString s = "00:00:00,   +00:00:00";

	statusBar()->insertItem( s, 1, 1, true );
	statusBar()->setItemAlignment( 1, AlignRight | AlignVCenter );
	statusBar()->setItemFixed( 1, -1 );
}

void KStars::initLocation() {
	//Initialize geographic location
	bool bFound = false; bool oldConfig = false;
	GeoLocation *GeoData;

	kapp->config()->setGroup( "Location" );
	if ( !kapp->config()->readEntry( "State", "" ).stripWhiteSpace().isEmpty() ) {
		oldConfig = true;
    kapp->config()->writeEntry( "State", "" ); //ignore this key from now on...
	}

	for (GeoData = data()->geoList.first(); GeoData; GeoData = data()->geoList.next())
	{

	//If the config file is old (has a State key) match a city if the city name
  //AND EITHER the province name OR the country name match
  //(old config files set both province and country to "StateName")
  //this can produce the wrong city, if more than one matches these criteria...
		if ( oldConfig ) {
			if ( (GeoData->name().lower() == options()->CityName.lower()) &&
					( (GeoData->province().lower() == options()->ProvinceName.lower()) ||
					(GeoData->country().lower() == options()->CountryName.lower()) ) )
			{
				bFound = TRUE;
				if ( GeoData->province().lower() != options()->ProvinceName.lower() )
					options()->ProvinceName = GeoData->province();
				if ( GeoData->country().lower() != options()->CountryName.lower() )
					options()->CountryName = GeoData->country();
				break ;
			}
		} else {
	//Otherwise, require all three fields (City, Province, and Country) to match
			if ( options()->ProvinceName.stripWhiteSpace().length() ) {
				if ( (GeoData->name().lower() == options()->CityName.lower()) &&
						(GeoData->province().lower() == options()->ProvinceName.lower()) &&
						(GeoData->country().lower() == options()->CountryName.lower()) )
				{
					bFound = TRUE;
					break ;
				}
			} else {
				if ( (GeoData->name().lower() == options()->CityName.lower()) &&
						(GeoData->country().lower() == options()->CountryName.lower()) )
				{
					bFound = TRUE;
					break ;
				}

			}
		}
	}

	if ( !bFound ) { // set city, province and country to default values
		options()->CityName = "Greenwich";
		options()->ProvinceName = "";
		options()->CountryName = "United Kingdom";
		for (GeoData = data()->geoList.first(); GeoData; GeoData = data()->geoList.next())
		{
			if ( (GeoData->name().lower() == options()->CityName.lower()) &&
					(GeoData->province().lower() == options()->ProvinceName.lower()) &&
					(GeoData->country().lower() == options()->CountryName.lower()) )
			{
				bFound = TRUE;
				break ;
			}
		}
	}

	if (bFound) {
		Location = new GeoLocation (GeoData);
	} else { //couldn't set geographic location, so set the "null" location.
		QString message = i18n( "Could not set geographic location!" );
		KMessageBox::sorry( 0, message, i18n( "No location set" ) );
		Location = new GeoLocation();
	}
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
	for (SkyPoint *point = data()->Equator.first(); point; point = data()->Equator.next()) {
		double sinlat, coslat, sindec, cosdec, sinAz, cosAz;
		double HARad;
		dms dec, HA, RA, Az;
		Az = point->ra();
		Az.SinCos( sinAz, cosAz );
		geo()->lat().SinCos( sinlat, coslat );

		dec.setRadians( asin( coslat*cosAz ) );
		dec.SinCos( sindec, cosdec );
   		HARad = acos( -1.0*(sinlat*sindec)/(coslat*cosdec) );
		if ( sinAz > 0.0 ) { HARad = 2.0*PI() - HARad; }
		HA.setRadians( HARad );
		RA = data()->LSTh.Degrees() - HA.Degrees();

		SkyPoint *o = new SkyPoint( RA, dec );
		o->setAlt( 0.0 );
		o->setAz( Az );

		data()->Horizon.append( o );

		//Define the Ecliptic (use the same ListIteration; interpret coordinates as Ecliptic long/lat)
		o = new SkyPoint( 0.0, 0.0 );
		o->setFromEcliptic( num->obliquity(), point->ra(), dms( 0.0 ) );
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

	KTipDialog::showTip( "kstars/tips", true );
}

void KStars::privatedata::buildGUI() {
	// need to set the mainwidget here, in main() this will cause a segfault because
	// skymap will created in this constructor and needs kapp->mainWidget()
	kapp->setMainWidget( ks );
	// here we get the preloaded data (stars, constellations etc.)

	//Instantiate the SimClock object
	ks->clock = new SimClock(ks);

	ks->initLocation();

	// create the widgets
	ks->centralWidget = new QWidget( ks );
	ks->setCentralWidget( ks->centralWidget );
	ks->infoPanel = new InfoPanel( ks->centralWidget, "ip", ks->data()->getLocale() );

	ks->initStatusBar();
	ks->initActions();

	ks->skymap = new SkyMap( ks->centralWidget );
	// update skymap if KStarsData send update signal
	QObject::connect(kstarsData, SIGNAL( update() ), ks->skymap, SLOT( Update() ) );

	// get focus of keyboard and mouse actions (for example zoom in with +)
	ks->skymap->QWidget::setFocus();

	// create the layout of the central widget
	ks->topLayout = new QVBoxLayout( ks->centralWidget );
	ks->topLayout->addWidget( ks->infoPanel );
	ks->topLayout->addWidget( ks->skymap );

	ks->infoPanel->geoChanged(ks->geo());

	ks->createGUI("kstarsui.rc", false); //2nd parameter must be false, or
                                      //plugActionList won't work!

	//Initialize show/hide state of toolbars.
	//These were in initActions, but they must appear after createGUI...
	if ( !ks->options()->showMainToolBar ) ks->toolBar( "mainToolBar" )->hide();
	if ( !ks->options()->showViewToolBar ) ks->toolBar( "viewToolBar" )->hide();

	ks->TimeStep = new TimeStepBox( ks->toolBar() );
//Tooltip is now part of TimeStepBox class definition
//	QToolTip::add( ks->TimeStep, i18n( "Time Step (seconds)" ) );

//Changing the timestep needs to propagate to the clock, check if slew mode should be
//(dis)engaged, and return input focus to the skymap.
	connect( ks->TimeStep, SIGNAL( scaleChanged( float ) ), ks->clock, SLOT( setScale( float )) );
	connect( ks->TimeStep, SIGNAL( scaleChanged( float ) ), ks->skymap, SLOT( slotClockSlewing() ) );
	connect( ks->TimeStep, SIGNAL( scaleChanged( float ) ), ks, SLOT( mapGetsFocus() ) );
	connect( ks->clock, SIGNAL(scaleChanged( float )), ks->TimeStep->tsbox(), SLOT( changeScale( float )) );
	ks->toolBar()->insertWidget( 0, 6, ks->TimeStep, 9 );
	ks->Blank = new QWidget( ks->toolBar() );
	ks->toolBar()->insertWidget( 1, 40, ks->Blank, 10);
	ks->toolBar()->setStretchableWidget( ks->Blank );

	ks->resize( ks->options()->windowWidth, ks->options()->windowHeight );

	//Set DST, if necessary
	ks->geo()->tzrule()->setDST( ks->geo()->tzrule()->isDSTActive( QDateTime::currentDateTime() ) );

	//intitialize time to system clock
	ks->clock->setUTC(QDateTime::currentDateTime().addSecs(int(-3600 * ks->geo()->TZ()) ));
	ks->setLSTh( ks->clock->UTC() );

//compute JD for the next DST adjustment
	QDateTime changetime = ks->geo()->tzrule()->nextDSTChange( QDateTime::currentDateTime() );
	ks->data()->setNextDSTChange( KSUtils::UTtoJulian( changetime.addSecs( int(-3600 * ks->geo()->TZ())) ) );

//compute JD for the previous DST adjustment (in case time is running backwards)
	changetime = ks->geo()->tzrule()->previousDSTChange( QDateTime::currentDateTime() );
	ks->data()->setPrevDSTChange( KSUtils::UTtoJulian( changetime.addSecs( int(-3600 * ks->geo()->TZ())) ) );

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

	//Set focus of Skymap.
  //Set default position in case stored focus is below horizon
	SkyPoint DefaultFocus;
	DefaultFocus.setAz( 180.0 );
	DefaultFocus.setAlt( 45.0 );
	DefaultFocus.HorizontalToEquatorial( ks->LSTh(), ks->geo()->lat() );
	ks->skymap->setDestination( &DefaultFocus );

	//if user was tracking last time, track on same object now.
	if ( ks->options()->isTracking ) {
		if ( (ks->options()->focusObject== i18n( "star" ) ) ||
		     (ks->options()->focusObject== i18n( "nothing" ) ) ) {
			ks->skymap->setClickedPoint( &newPoint );
		} else {
			ks->skymap->setClickedObject( ks->getObjectNamed( ks->options()->focusObject ) );
			if ( ks->skymap->clickedObject() ) {
				ks->skymap->setClickedPoint( ks->skymap->clickedObject() );
			} else {
				ks->skymap->setClickedPoint( &newPoint );
			}
		}
		ks->skymap->slotCenter();
	} else {
		ks->skymap->setClickedPoint( &newPoint );
		ks->skymap->slotCenter();
	}

	ks->skymap->setFocus( ks->skymap->destination() );
	ks->skymap->focus()->EquatorialToHorizontal( ks->LSTh(), ks->geo()->lat() );
	ks->skymap->destination()->EquatorialToHorizontal( ks->LSTh(), ks->geo()->lat() );

	ks->setHourAngle();

	ks->skymap->setOldFocus( ks->skymap->focus() );
	ks->skymap->oldfocus()->setAz( ks->skymap->focus()->az() );
	ks->skymap->oldfocus()->setAlt( ks->skymap->focus()->alt() );

	kapp->dcopClient()->resume();

}
