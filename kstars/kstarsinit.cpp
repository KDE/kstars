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

#include <qlayout.h>
#include <qfile.h>
#include <qdir.h>
#include <dcopclient.h>
#include <kshortcut.h>
#include <kiconloader.h>
#include <kpopupmenu.h>
#include <kstatusbar.h>
#include <ktip.h>
#include <kmessagebox.h>
#include <kstandarddirs.h>
#include <kdeversion.h>

#include "Options.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "kstarssplash.h"
#include "skymap.h"
#include "skyobject.h"
#include "ksplanetbase.h"
#include "ksutils.h"
#include "ksnumbers.h"
#include "infoboxes.h"
#include "toggleaction.h"
#include "indimenu.h"
#include "simclock.h"
#include "timestepbox.h"

//This file contains functions that kstars calls at startup (except constructors).
//These functions are declared in kstars.h

void KStars::initActions() {
//File Menu:
	new KAction(i18n("&New Window"), "window_new", KShortcut( "Ctrl+N"  ),
			this, SLOT( newWindow() ), actionCollection(), "new_window");
	new KAction(i18n("&Close Window"), "fileclose", KShortcut( "Ctrl+W"  ),
			this, SLOT( closeWindow() ), actionCollection(), "close_window");
	new KAction( i18n( "&Download Data..." ), "lknewstuff", KShortcut( "Ctrl+D" ),
			this, SLOT( slotDownload() ), actionCollection(), "get_data" );
	new KAction( i18n( "Open FITS..."), "fileopen", KShortcut( "Ctrl+O"), this, SLOT( slotOpenFITS()), actionCollection(), "open_file");
	new KAction( i18n( "&Save Sky Image..." ), "fileexport", KShortcut( "Ctrl+I" ),
			this, SLOT( slotExportImage() ), actionCollection(), "export_image" );
	new KAction( i18n( "&Run Script..." ), "launch", KShortcut( "Ctrl+R" ),
			this, SLOT( slotRunScript() ), actionCollection(), "run_script" );
	KStdAction::print(this, SLOT( slotPrint() ), actionCollection(), "print" );
	KStdAction::quit(this, SLOT( close() ), actionCollection(), "quit" );

//Time Menu:
	new KAction( i18n( "Set Time to &Now" ), KShortcut( "Ctrl+E"  ),
		this, SLOT( slotSetTimeToNow() ), actionCollection(), "time_to_now" );
	new KAction( i18n( "set Clock to New Time", "&Set Time..." ), "clock", KShortcut( "Ctrl+S"  ),
		this, SLOT( slotSetTime() ), actionCollection(), "time_dialog" );
	ToggleAction *actTimeRun = new ToggleAction( i18n( "Stop &Clock" ), BarIcon("player_pause"),
				i18n("Start &Clock"), BarIcon("1rightarrow"),
				0, this, SLOT( slotToggleTimer() ), actionCollection(), "timer_control" );
	actTimeRun->setOffToolTip( i18n( "Start Clock" ) );
	actTimeRun->setOnToolTip( i18n( "Stop Clock" ) );
	QObject::connect(data()->clock(), SIGNAL(clockStarted()), actTimeRun, SLOT(turnOn()) );
	QObject::connect(data()->clock(), SIGNAL(clockStopped()), actTimeRun, SLOT(turnOff()) );
//UpdateTime() if clock is stopped (so hidden objects get drawn)
	QObject::connect(data()->clock(), SIGNAL(clockStopped()), this, SLOT(updateTime()) );

//Focus Menu:
	new KAction(i18n( "&Zenith" ), KShortcut( "Z" ),
			this, SLOT( slotPointFocus() ),  actionCollection(), "zenith");
	new KAction(i18n( "&North" ), KShortcut( "N" ),
			this, SLOT( slotPointFocus() ),  actionCollection(), "north");
	new KAction(i18n( "&East" ), KShortcut( "E" ),
			this, SLOT( slotPointFocus() ),  actionCollection(), "east");
	new KAction(i18n( "&South" ), KShortcut( "S" ),
			this, SLOT( slotPointFocus() ),  actionCollection(), "south");
	new KAction(i18n( "&West" ), KShortcut( "W" ),
			this, SLOT( slotPointFocus() ),  actionCollection(), "west");
	KAction *tmpAction = KStdAction::find( this, SLOT( slotFind() ),
												actionCollection(), "find_object" );
	tmpAction->setText( i18n( "&Find Object..." ) );
	tmpAction->setToolTip( i18n( "Find object" ) );

	new KAction( i18n( "Engage &Tracking" ), "decrypted", KShortcut( "Ctrl+T"  ),
		this, SLOT( slotTrack() ), actionCollection(), "track_object" );
	if ( Options::isTracking() ) {
		actionCollection()->action("track_object")->setText( i18n( "Stop &Tracking" ) );
		actionCollection()->action("track_object")->setIconSet( BarIcon( "encrypted" ) );
	}

	new KAction( i18n( "Set Focus &Manually..." ), KShortcut( "Ctrl+M" ),
			this, SLOT( slotManualFocus() ),  actionCollection(), "manual_focus" );

//View Menu:
	KStdAction::zoomIn(this, SLOT( slotZoomIn() ), actionCollection(), "zoom_in" );
	KStdAction::zoomOut(this, SLOT( slotZoomOut() ), actionCollection(), "zoom_out" );
	new KAction( i18n( "&Default Zoom" ), "viewmagfit.png", KShortcut( "Ctrl+Z" ),
		this, SLOT( slotDefaultZoom() ), actionCollection(), "zoom_default" );
	new KAction( i18n( "&Zoom to Angular Size..." ), "viewmag.png", KShortcut( "Ctrl+Shift+Z" ),
		this, SLOT( slotSetZoom() ), actionCollection(), "zoom_set" );
	actCoordSys = new ToggleAction( i18n("Horizontal &Coordinates"), i18n( "Equatorial &Coordinates" ),
			Key_Space, this, SLOT( slotCoordSys() ), actionCollection(), "coordsys" );
	if ( Options::useAltAz() ) actCoordSys->turnOff();
	KStdAction::fullScreen( this, SLOT( slotFullScreen() ), actionCollection(), 0 );


//Settings Menu:
	//
	// MHH - 2002-01-13
	// Setting the slot in the KToggleAction constructor, connects the slot to
	// the activated signal instead of the toggled signal. This seems like a bug
	// to me, but ...
	//
	//Info Boxes option actions
	KToggleAction *a = new KToggleAction(i18n( "Show the information boxes", "Show &Info Boxes"),
			0, 0, 0, actionCollection(), "show_boxes");
#if KDE_IS_VERSION( 3, 2, 90 )
	a->setCheckedState(i18n("Hide &Info Boxes"));
#endif
	a->setChecked( Options::showInfoBoxes() );
	QObject::connect(a, SIGNAL( toggled(bool) ), infoBoxes(), SLOT(setVisible(bool)));
	QObject::connect(a, SIGNAL( toggled(bool) ), this, SLOT(slotShowGUIItem(bool)));
	infoBoxes()->setVisible( Options::showInfoBoxes() );

	a = new KToggleAction(i18n( "Show time-related info box", "Show &Time Box"),
			0, 0, 0, actionCollection(), "show_time_box");
#if KDE_IS_VERSION( 3, 2, 90 )
	a->setCheckedState(i18n("Hide &Time Box"));
#endif
	a->setChecked( Options::showTimeBox() );
	QObject::connect(a, SIGNAL( toggled(bool) ), infoBoxes(), SLOT(showTimeBox(bool)));
	QObject::connect(a, SIGNAL( toggled(bool) ), this, SLOT(slotShowGUIItem(bool)));

	a = new KToggleAction(i18n( "Show focus-related info box", "Show &Focus Box"),
			0, 0, 0, actionCollection(), "show_focus_box");
#if KDE_IS_VERSION( 3, 2, 90 )
	a->setCheckedState(i18n("Hide &Focus Box"));
#endif
	a->setChecked( Options::showFocusBox() );
	QObject::connect(a, SIGNAL( toggled(bool) ), infoBoxes(), SLOT(showFocusBox(bool)));
	QObject::connect(a, SIGNAL( toggled(bool) ), this, SLOT(slotShowGUIItem(bool)));

	a = new KToggleAction(i18n( "Show location-related info box", "Show &Location Box"),
			0, 0, 0, actionCollection(), "show_location_box");
#if KDE_IS_VERSION( 3, 2, 90 )
	a->setCheckedState(i18n("Hide &Location Box"));
#endif
	a->setChecked( Options::showGeoBox() );
	QObject::connect(a, SIGNAL( toggled(bool) ), infoBoxes(), SLOT(showGeoBox(bool)));
	QObject::connect(a, SIGNAL( toggled(bool) ), this, SLOT(slotShowGUIItem(bool)));

//Toolbar view options
	a = new KToggleAction(i18n( "Show Main Toolbar" ),
			0, 0, 0, actionCollection(), "show_mainToolBar");
#if KDE_IS_VERSION( 3, 2, 90 )
	a->setCheckedState(i18n("Hide Main Toolbar"));
#endif
	a->setChecked( Options::showMainToolBar() );
	QObject::connect(a, SIGNAL( toggled(bool) ), this, SLOT(slotShowGUIItem(bool)));

	a = new KToggleAction(i18n( "Show View Toolbar" ),
			0, 0, 0, actionCollection(), "show_viewToolBar");
#if KDE_IS_VERSION( 3, 2, 90 )
	a->setCheckedState(i18n("Hide View Toolbar"));
#endif
	a->setChecked( Options::showViewToolBar() );
	QObject::connect(a, SIGNAL( toggled(bool) ), this, SLOT(slotShowGUIItem(bool)));

//Statusbar view options
	a = new KToggleAction(i18n( "Show Statusbar" ),
			0, 0, 0, actionCollection(), "show_statusBar");
#if KDE_IS_VERSION( 3, 2, 90 )
	a->setCheckedState(i18n("Hide Statusbar"));
#endif
	QObject::connect(a, SIGNAL( toggled(bool) ), this, SLOT(slotShowGUIItem(bool)));
	a->setChecked( Options::showStatusBar() );
	a = new KToggleAction(i18n( "Show Az/Alt Field" ),
			0, 0, 0, actionCollection(), "show_sbAzAlt");
#if KDE_IS_VERSION( 3, 2, 90 )
	a->setCheckedState(i18n("Hide Az/Alt Field"));
#endif
	a->setChecked( Options::showAltAzField() );
	QObject::connect(a, SIGNAL( toggled(bool) ), this, SLOT(slotShowGUIItem(bool)));
	a = new KToggleAction(i18n( "Show RA/Dec Field" ),
			0, 0, 0, actionCollection(), "show_sbRADec");
#if KDE_IS_VERSION( 3, 2, 90 )
	a->setCheckedState(i18n("Hide RA/Dec Field"));
#endif
	a->setChecked( Options::showRADecField() );
	QObject::connect(a, SIGNAL( toggled(bool) ), this, SLOT(slotShowGUIItem(bool)));

//Color scheme actions.  These are added to the "colorschemes" KActionMenu.
	colorActionMenu = new KActionMenu( i18n( "C&olor Schemes" ), actionCollection(), "colorschemes" );
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

	//Add FOV Symbol actions
	fovActionMenu = new KActionMenu( i18n( "&FOV Symbols" ), actionCollection(), "fovsymbols" );
	initFOV();

// 	new KAction( i18n( "do not use a target symbol", "No Symbol" ), 0,
// 			this, SLOT( slotTargetSymbol() ), actionCollection(), "target_symbol_none" );
//
// 	new KAction( i18n( "use a circle target symbol", "Circle" ), 0,
// 			this, SLOT( slotTargetSymbol() ), actionCollection(), "target_symbol_circle" );
//
// 	new KAction( i18n( "use a crosshairs target symbol", "Crosshairs" ), 0,
// 			this, SLOT( slotTargetSymbol() ), actionCollection(), "target_symbol_crosshairs" );
//
// 	new KAction( i18n( "use a bullseye target symbol", "Bullseye" ), 0,
// 			this, SLOT( slotTargetSymbol() ), actionCollection(), "target_symbol_bullseye" );
//
// 	new KAction( i18n( "use a rectangle target symbol", "Rectangle" ), 0,
// 			this, SLOT( slotTargetSymbol() ), actionCollection(), "target_symbol_rectangle" );

	//use custom icon earth.png for the geoLocator icon.  If it is not installed
  //for some reason, use standard icon gohome.png instead.
	QFile tempFile;
	if (KSUtils::openDataFile( tempFile, "earth.png" ) ) {
		tmpAction = new KAction( i18n( "Set Geographic Location..." ),
				tempFile.name(), KShortcut( "Ctrl+G"  ),
				this, SLOT( slotGeoLocator() ), actionCollection(), "geolocation" );
		tmpAction->setToolTip( i18n( "Geographic location" ) );
		tempFile.close();
	} else {
		new KAction( i18n( "Location on Earth", "&Geographic..." ), "gohome", KShortcut( "Ctrl+G"  ),
				this, SLOT( slotGeoLocator() ), actionCollection(), "geolocation" );
	}

	KStdAction::preferences( this, SLOT( slotViewOps() ), actionCollection(), "configure" );

	new KAction(i18n( "Startup Wizard..." ), 0, this, SLOT( slotWizard() ),
		actionCollection(), "startwizard" );

//Tools Menu:
	new KAction(i18n( "Calculator..."), KShortcut( "Ctrl+C"),
			this, SLOT( slotCalculator() ), actionCollection(), "astrocalculator");

	// enable action only if file was loaded and processed successfully.
	if (!data()->VariableStarsList.isEmpty())
		new KAction(i18n( "AAVSO Light Curves..."), KShortcut( "Ctrl+V"),
						this, SLOT( slotLCGenerator() ), actionCollection(), "lightcurvegenerator");

	new KAction(i18n( "Altitude vs. Time..."), KShortcut( "Ctrl+A"),
						this, SLOT( slotAVT() ), actionCollection(), "altitude_vs_time");
	new KAction(i18n( "What's up Tonight..."), KShortcut("Ctrl+U"),
						this, SLOT(slotWUT()), actionCollection(), "whats_up_tonight");
	new KAction(i18n( "Script Builder..."), KShortcut("Ctrl+B"),
						this, SLOT(slotScriptBuilder()), actionCollection(), "scriptbuilder");
	new KAction(i18n( "Solar System..."), KShortcut("Ctrl+Y"),
						this, SLOT(slotSolarSystem()), actionCollection(), "solarsystem");
	new KAction(i18n( "Jupiter's Moons..."), KShortcut("Ctrl+J"),
						this, SLOT(slotJMoonTool()), actionCollection(), "jmoontool");

// devices Menu
	new KAction(i18n("Telescope Wizard..."), 0, this, SLOT(slotTelescopeWizard()), actionCollection(), "telescope_wizard");
	new KAction(i18n("Capture Image Sequence..."), 0, this, SLOT(slotImageSequence()), actionCollection(), "capture_sequence");
	new KAction(i18n("Device Manager..."), 0, this, SLOT(slotINDIDriver()), actionCollection(), "device_manager");
	new KAction(i18n("INDI Control Panel"), 0, this, SLOT(slotINDIPanel()), actionCollection(), "indi_control_panel");
	new KAction(i18n("Configure INDI..."), 0, this, SLOT(slotINDIConf()), actionCollection(), "configure_indi");



//Help Menu:
	new KAction( i18n( "Tip of the Day" ), "idea", 0,
			this, SLOT( slotTipOfDay() ), actionCollection(), "help_tipofday" );

//Handbook toolBar item:
	new KAction( i18n( "&Handbook" ), "contents", KShortcut( "F1"  ),
			this, SLOT( appHelpActivated() ), actionCollection(), "handbook" );

//
//viewToolBar actions:
//

//show_stars:
	if (KSUtils::openDataFile( tempFile, "show_stars.png" ) ) {
		KToggleAction *a = new KToggleAction( i18n( "Toggle Stars" ),
				tempFile.name(), 0,
				this, SLOT( slotViewToolBar() ), actionCollection(), "show_stars" );
		a->setChecked( Options::showStars() );
		tempFile.close();
	} else {
		KToggleAction *a = new KToggleAction( i18n( "Toggle Stars" ), "wizard", 0,
				this, SLOT( slotViewToolBar() ), actionCollection(), "show_stars" );
		a->setChecked( Options::showStars() );
	}

//show_deepsky:
	if (KSUtils::openDataFile( tempFile, "show_deepsky.png" ) ) {
		KToggleAction *a = new KToggleAction( i18n( "Toggle Deep Sky Objects" ),
				tempFile.name(), 0,
				this, SLOT( slotViewToolBar() ), actionCollection(), "show_deepsky" );
		a->setChecked( Options::showDeepSky() );
		tempFile.close();
	} else {
		KToggleAction *a = new KToggleAction( i18n( "Toggle Deep Sky Objects" ), "wizard", 0,
				this, SLOT( slotViewToolBar() ), actionCollection(), "show_deepsky" );
		a->setChecked( Options::showDeepSky() );
	}

//show_planets:
	if (KSUtils::openDataFile( tempFile, "show_planets.png" ) ) {
		KToggleAction *a = new KToggleAction( i18n( "Toggle Solar System" ),
				tempFile.name(), 0,
				this, SLOT( slotViewToolBar() ), actionCollection(), "show_planets" );
		a->setChecked( Options::showPlanets() );
		tempFile.close();
	} else {
		KToggleAction *a = new KToggleAction( i18n( "Toggle Solar System" ), "wizard", 0,
				this, SLOT( slotViewToolBar() ), actionCollection(), "show_planets" );
		a->setChecked( Options::showPlanets() );
	}

//show_clines:
	if (KSUtils::openDataFile( tempFile, "show_clines.png" ) ) {
		KToggleAction *a = new KToggleAction( i18n( "Toggle Constellation Lines" ),
				tempFile.name(), 0,
				this, SLOT( slotViewToolBar() ), actionCollection(), "show_clines" );
		a->setChecked( Options::showCLines() );
		tempFile.close();
	} else {
		KToggleAction *a = new KToggleAction( i18n( "Toggle Constellation Lines" ), "wizard", 0,
				this, SLOT( slotViewToolBar() ), actionCollection(), "show_clines" );
		a->setChecked( Options::showCLines() );
	}

//show_cnames:
	if (KSUtils::openDataFile( tempFile, "show_cnames.png" ) ) {
		KToggleAction *a = new KToggleAction( i18n( "Toggle Constellation Names" ),
				tempFile.name(), 0,
				this, SLOT( slotViewToolBar() ), actionCollection(), "show_cnames" );
		a->setChecked( Options::showCNames() );
		tempFile.close();
	} else {
		KToggleAction *a = new KToggleAction( i18n( "Toggle Constellation Lines" ), "wizard", 0,
				this, SLOT( slotViewToolBar() ), actionCollection(), "show_cnames" );
		a->setChecked( Options::showCNames() );
	}

//show_cbound:
	if (KSUtils::openDataFile( tempFile, "show_cbound.png" ) ) {
		KToggleAction *a = new KToggleAction( i18n( "Toggle Constellation Boundaries" ),
				tempFile.name(), 0,
				this, SLOT( slotViewToolBar() ), actionCollection(), "show_cbounds" );
		a->setChecked( Options::showCBounds() );
		tempFile.close();
	} else {
		KToggleAction *a = new KToggleAction( i18n( "Toggle Constellation Boundaries" ), "wizard", 0,
				this, SLOT( slotViewToolBar() ), actionCollection(), "show_cbounds" );
		a->setChecked( Options::showCBounds() );
	}

//show_mw:
	if (KSUtils::openDataFile( tempFile, "show_mw.png" ) ) {
		KToggleAction *a = new KToggleAction( i18n( "Toggle Milky Way" ),
				tempFile.name(), 0,
				this, SLOT( slotViewToolBar() ), actionCollection(), "show_mw" );
		a->setChecked( Options::showMilkyWay() );
		tempFile.close();
	} else {
		KToggleAction *a = new KToggleAction( i18n( "Toggle Milky Way" ), "wizard", 0,
				this, SLOT( slotViewToolBar() ), actionCollection(), "show_mw" );
		a->setChecked( Options::showMilkyWay() );
	}

//show_grid:
	if (KSUtils::openDataFile( tempFile, "show_grid.png" ) ) {
		KToggleAction *a = new KToggleAction( i18n( "Toggle Coordinate Grid" ),
				tempFile.name(), 0,
				this, SLOT( slotViewToolBar() ), actionCollection(), "show_grid" );
		tempFile.close();
		a->setChecked( Options::showGrid() );
	} else {
		KToggleAction *a = new KToggleAction( i18n( "Toggle Coordinate Grid" ), "wizard", 0,
				this, SLOT( slotViewToolBar() ), actionCollection(), "show_grid" );
		a->setChecked( Options::showGrid() );
	}

//show_horizon:
	if (KSUtils::openDataFile( tempFile, "show_horiz.png" ) ) {
		KToggleAction *a = new KToggleAction( i18n( "Toggle Ground" ),
				tempFile.name(), 0,
				this, SLOT( slotViewToolBar() ), actionCollection(), "show_horizon" );
		tempFile.close();
		a->setChecked( Options::showGround() );
	} else {
		KToggleAction *a = new KToggleAction( i18n( "Toggle Ground" ), "wizard", 0,
				this, SLOT( slotViewToolBar() ), actionCollection(), "show_horizon" );
		a->setChecked( Options::showGround() );
	}
	
        if (Options::fitsSaveDirectory().isEmpty())
            Options::setFitsSaveDirectory(QDir:: homeDirPath());
}

void KStars::initFOV() {
	//Read in the user's fov.dat and populate the FOV menu with its symbols.  If no fov.dat exists, populate
	//create a default version.
	QFile f;
	QStringList fields;
	QString nm;

	f.setName( locateLocal( "appdata", "fov.dat" ) );

	//if file s empty, let's start over
	if ( (uint)f.size() == 0 ) f.remove();

	if ( ! f.exists() ) {
		if ( ! f.open( IO_WriteOnly ) ) {
			kdDebug() << i18n( "Could not open fov.dat." ) << endl;
		} else {
			QTextStream ostream(&f);
			ostream << i18n( "Do not use a field-of-view indicator", "No FOV" ) <<  ":0.0:0:#AAAAAA" << endl;
			ostream << i18n( "use field-of-view for binoculars", "7x35 Binoculars" ) << ":558:1:#AAAAAA" << endl;
			ostream << i18n( "use 1-degree field-of-view indicator", "One Degree" ) << ":60:2:#AAAAAA" << endl;
			ostream << i18n( "use HST field-of-view indicator", "HST WFPC2" ) << ":2.4:0:#AAAAAA" << endl;
			f.close();
		}
	}

	//just populate the FOV menu with items, don't need to fully parse the lines
	if ( f.open( IO_ReadOnly ) ) {
		QTextStream stream( &f );
		while ( !stream.eof() ) {
			QString line = stream.readLine();
			fields = QStringList::split( ":", line );

			if ( fields.count() == 4 ) {
				nm = fields[0].stripWhiteSpace();
				fovActionMenu->insert( new KAction( nm, 0, this, SLOT( slotTargetSymbol() ), actionCollection(), nm.utf8() ) );
			}
		}
	} else {
		kdDebug() << i18n( "Could not open file: %1" ).arg( f.name() ) << endl;
	}

	fovActionMenu->popupMenu()->insertSeparator();
	fovActionMenu->insert( new KAction( i18n( "Edit FOV Symbols..." ), 0, this, SLOT( slotFOVEdit() ), actionCollection(), "edit_fov" ) );
}

void KStars::initStatusBar() {
	statusBar()->insertItem( i18n( " Welcome to KStars " ), 0, 1, true );
	statusBar()->setItemAlignment( 0, AlignLeft | AlignVCenter );

	QString s = "000d 00m 00s,   +00d 00\' 00\""; //only need this to set the width

	if ( Options::showAltAzField() ) {
		statusBar()->insertFixedItem( s, 1, true );
		statusBar()->setItemAlignment( 1, AlignRight | AlignVCenter );
		statusBar()->changeItem( "", 1 );
	}

	if ( Options::showRADecField() ) {
		statusBar()->insertFixedItem( s, 2, true );
		statusBar()->setItemAlignment( 2, AlignRight | AlignVCenter );
		statusBar()->changeItem( "", 2 );
	}

	if ( ! Options::showStatusBar() ) statusBar()->hide();
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
	data()->setFullTimeUpdate();
	updateTime();

	data()->clock()->start();

//Initialize FOV symbol from options
	data()->fovSymbol.setName( Options::fOVName() );
	data()->fovSymbol.setSize( Options::fOVSize() );
	data()->fovSymbol.setShape( Options::fOVShape() );
	data()->fovSymbol.setColor( Options::fOVColor().name() );

	show();

	//If this is the first startup, show the wizard
	if ( Options::runStartupWizard() ) {
		slotWizard();
	}

	//Check whether initial position is below the horizon.
	//We used to just call slotCenter() in buildGUI() which performs this check.
	//However, on some systems, if the messagebox is shown before show() is called,
	//the program exits.  It does not crash (at least there are no error messages),
	//it simply exits.  Very strange.
	if ( Options::useAltAz() && Options::showGround() &&
			map()->focus()->alt()->Degrees() < -1.0 ) {
		QString caption = i18n( "Initial Position is Below Horizon" );
		QString message = i18n( "The initial position is below the horizon.\nWould you like to reset to the default position?" );
		if ( KMessageBox::warningYesNo( this, message, caption,
				KStdGuiItem::yes(), KStdGuiItem::no(), "dag_start_below_horiz" ) == KMessageBox::Yes ) {
			map()->setClickedObject( NULL );
			map()->setFocusObject( NULL );
			Options::setIsTracking( false );

			data()->setSnapNextFocus(true);

			SkyPoint DefaultFocus;
			DefaultFocus.setAz( 180.0 );
			DefaultFocus.setAlt( 45.0 );
			DefaultFocus.HorizontalToEquatorial( LST(), geo()->lat() );
			map()->setDestination( &DefaultFocus );
		}
	}

	//If there is a focusObject() and it is a SS body, add a temporary Trail to it.
	if ( map()->focusObject() && map()->focusObject()->isSolarSystem()
			&& Options::useAutoTrail() ) {
		((KSPlanetBase*)map()->focusObject())->addToTrail();
		data()->temporaryTrail = true;
	}

// just show dialog if option is set (don't force it)
	KTipDialog::showTip( "kstars/tips" );
}

void KStars::privatedata::buildGUI() {
	//create the widgets
	ks->centralWidget = new QWidget( ks );
	ks->setCentralWidget( ks->centralWidget );

	//set AAVSO modaless dialog pointer to 0
	ks->AAVSODialog = 0;

	//INDI menu started without GUI
	ks->indimenu = new INDIMenu(ks);

	//INDI driver set to null
	ks->indidriver = 0;
	
	//INDI img sequence, set to null
	ks->indiseq = 0;

	ks->skymap = new SkyMap( ks->data(), ks->centralWidget );
	// update skymap if KStarsData send update signal
	QObject::connect(kstarsData, SIGNAL( update() ), ks->skymap, SLOT( forceUpdateNow() ) );

	// get focus of keyboard and mouse actions (for example zoom in with +)
	ks->map()->QWidget::setFocus();

	ks->initStatusBar();
	ks->initActions();

	// create the layout of the central widget
	ks->topLayout = new QVBoxLayout( ks->centralWidget );
	ks->topLayout->addWidget( ks->skymap );

	// 2nd parameter must be false, or plugActionList won't work!
	ks->createGUI("kstarsui.rc", false);

	//Do not show text on the view toolbar buttons
	//FIXME: after strings freeze, remove this and make the
	//text of each button shorter
	ks->toolBar( "viewToolBar" )->setIconText( KToolBar::IconOnly );

	//Initialize show/hide state of toolbars.
	//These were in initActions, but they must appear after createGUI...
	if ( !Options::showMainToolBar() ) ks->toolBar( "mainToolBar" )->hide();
	if ( !Options::showViewToolBar() ) ks->toolBar( "viewToolBar" )->hide();

	ks->TimeStep = new TimeStepBox( ks->toolBar() );
	ks->toolBar()->insertWidget( 0, 6, ks->TimeStep, 15 );

//Changing the timestep needs to propagate to the clock, check if slew mode should be
//(dis)engaged, and return input focus to the skymap.
	connect( ks->TimeStep, SIGNAL( scaleChanged( float ) ), ks->data(), SLOT( setTimeDirection( float ) ) );
	connect( ks->TimeStep, SIGNAL( scaleChanged( float ) ), ks->data()->clock(), SLOT( setScale( float )) );
//	connect( ks->TimeStep, SIGNAL( scaleChanged( float ) ), ks->skymap, SLOT( slotClockSlewing() ) );
	connect( ks->data()->clock(), SIGNAL( scaleChanged( float ) ), ks->map(), SLOT( slotClockSlewing() ) );
	connect( ks->TimeStep, SIGNAL( scaleChanged( float ) ), ks, SLOT( mapGetsFocus() ) );

	ks->resize( Options::windowWidth(), Options::windowHeight() );

	// initialize clock with current time/date
	ks->slotSetTimeToNow();

	//Define the celestial equator, horizon and ecliptic
	KSNumbers tempnum(ks->data()->ut().djd());
	ks->data()->initGuides(&tempnum);

	//Connect the clock.
	QObject::connect( ks->data()->clock(), SIGNAL( timeAdvanced() ), ks, SLOT( updateTime() ) );
	QObject::connect( ks->data()->clock(), SIGNAL( timeChanged() ), ks, SLOT( updateTime() ) );

	// Connect cache function
	QObject::connect( kstarsData, SIGNAL( clearCache() ), ks, SLOT( clearCachedFindDialog() ) );

	SkyPoint newPoint;
	if ( ks->data()->useDefaultOptions ) {
		newPoint.setAz( Options::focusRA() );
		newPoint.setAlt( Options::focusDec() + 0.0001 );
		newPoint.HorizontalToEquatorial( ks->LST(), ks->geo()->lat() );
	} else {
		newPoint.set( Options::focusRA(), Options::focusDec() );
	}

//need to set focusObject before updateTime, otherwise tracking is set to false
	if ( (Options::focusObject() != i18n( "star" ) ) &&
		     (Options::focusObject() != i18n( "nothing" ) ) )
			ks->map()->setFocusObject( ks->data()->objectNamed( Options::focusObject() ) );

	ks->updateTime();

	//Set focus of Skymap to value stored in config.
	//Set default position in case stored focus is below horizon
//	SkyPoint DefaultFocus;
//	DefaultFocus.setAz( 180.0 );
//	DefaultFocus.setAlt( 45.0 );
//	DefaultFocus.HorizontalToEquatorial( ks->LST(), ks->geo()->lat() );
//	ks->map()->setDestination( &DefaultFocus );

	//if user was tracking last time, track on same object now.
	if ( Options::isTracking() ) {
		if ( (Options::focusObject() == i18n( "star" ) ) ||
		     (Options::focusObject() == i18n( "nothing" ) ) ) {
			ks->map()->setFocusPoint( &newPoint );
		} else {
			ks->map()->setClickedObject( ks->data()->objectNamed( Options::focusObject() ) );
			if ( ks->map()->clickedObject() ) {
				ks->map()->setFocusPoint( ks->map()->clickedObject() );
			} else {
				ks->map()->setFocusPoint( &newPoint );
			}
		}
//		ks->map()->slotCenter();
	} else {
		ks->map()->setFocusPoint( &newPoint );
//		ks->map()->slotCenter();
	}

	if ( Options::focusObject() == i18n( "star" ) ) Options::setFocusObject( i18n( "nothing" ) );

	ks->map()->setDestination( ks->map()->focusPoint() );
	ks->map()->destination()->EquatorialToHorizontal( ks->LST(), ks->geo()->lat() );
	ks->map()->setFocus( ks->map()->destination() );
	ks->map()->focus()->EquatorialToHorizontal( ks->LST(), ks->geo()->lat() );

	ks->infoBoxes()->focusObjChanged( Options::focusObject() );
	ks->infoBoxes()->focusCoordChanged( ks->map()->focus() );

	ks->data()->setHourAngle( ks->LST()->Hours() - ks->map()->focus()->ra()->Hours() );

	ks->map()->setOldFocus( ks->map()->focus() );
	ks->map()->oldfocus()->setAz( ks->map()->focus()->az()->Degrees() );
	ks->map()->oldfocus()->setAlt( ks->map()->focus()->alt()->Degrees() );

	// check zoom in/out buttons
	if ( Options::zoomFactor() >= MAXZOOM ) ks->actionCollection()->action("zoom_in")->setEnabled( false );
	if ( Options::zoomFactor() <= MINZOOM ) ks->actionCollection()->action("zoom_out")->setEnabled( false );

	kapp->dcopClient()->resume();
}
