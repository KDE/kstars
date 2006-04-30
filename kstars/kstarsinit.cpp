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

#include <QFile>
#include <QDir>
#include <QTextStream>

#include <dcopclient.h>
#include <kiconloader.h>
#include <kmenu.h>
#include <kstatusbar.h>
#include <ktip.h>
#include <kmessagebox.h>
#include <kstandarddirs.h>
#include <kdeversion.h>
#include <ktoolbar.h>

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
#include "widgets/timestepbox.h"

//This file contains functions that kstars calls at startup (except constructors).
//These functions are declared in kstars.h

void KStars::initActions() {
//File Menu:
	KAction *ka = new KAction( KIcon( "window_new" ), i18n("&New Window"), actionCollection(), "new_window" );
	ka->setDefaultShortcut( KShortcut( "Ctrl+N" ) );
	connect( ka, SIGNAL( triggered() ), this, SLOT( newWindow() ) );

	ka = new KAction( KIcon( "fileclose" ), i18n("&Close Window"), actionCollection(), "close_window");
	ka->setDefaultShortcut( KShortcut( "Ctrl+W" ) );
	connect( ka, SIGNAL( triggered() ), this, SLOT( closeWindow() ) );

	ka = new KAction( KIcon( "knewstuff" ), i18n( "&Download Data..." ),  actionCollection(), "get_data" );
	ka->setDefaultShortcut( KShortcut( "Ctrl+D" ) );
	connect( ka, SIGNAL( triggered() ), this, SLOT( slotDownload() ) );

	ka = new KAction( KIcon( "fileopen" ), i18n( "Open FITS..."), actionCollection(), "open_file");
	ka->setDefaultShortcut( KShortcut( "Ctrl+O" ) );
	connect( ka, SIGNAL( triggered() ), this, SLOT( slotOpenFITS() ) );

	ka = new KAction( KIcon( "fileexport" ), i18n( "&Save Sky Image..." ), actionCollection(), "export_image" );
	ka->setDefaultShortcut( KShortcut( "Ctrl+I" ) );
	connect( ka, SIGNAL( triggered() ), this, SLOT( slotExportImage() ) );

	ka = new KAction( KIcon( "launch" ), i18n( "&Run Script..." ), actionCollection(), "run_script" );
	ka->setDefaultShortcut( KShortcut( "Ctrl+R" ) );
	connect( ka, SIGNAL( triggered() ), this, SLOT( slotRunScript() ) );

	KStdAction::print(this, SLOT( slotPrint() ), actionCollection(), "print" );
	KStdAction::quit(this, SLOT( close() ), actionCollection(), "quit" );

//Time Menu:
	ka = new KAction( i18n( "Set Time to &Now" ), actionCollection(), "time_to_now" );
	ka->setDefaultShortcut( KShortcut( "Ctrl+E"  ) );
	connect( ka, SIGNAL( triggered() ), this, SLOT( slotSetTimeToNow() ) );

	ka = new KAction( KIcon( "clock" ), i18nc( "set Clock to New Time", "&Set Time..." ), actionCollection(), "time_dialog" );
	ka->setDefaultShortcut( KShortcut( "Ctrl+S"  ) );
	connect( ka, SIGNAL( triggered() ), this, SLOT( slotSetTime() ) );

	ToggleAction *actTimeRun = new ToggleAction( KIcon( "player_pause" ), i18n( "Stop &Clock" ),
				KIcon( "1rightarrow" ), i18n("Start &Clock"),
				KShortcut(), this, SLOT( slotToggleTimer() ), actionCollection(), "timer_control" );
	actTimeRun->setOffToolTip( i18n( "Start Clock" ) );
	actTimeRun->setOnToolTip( i18n( "Stop Clock" ) );
	QObject::connect(data()->clock(), SIGNAL(clockStarted()), actTimeRun, SLOT(turnOn()) );
	QObject::connect(data()->clock(), SIGNAL(clockStopped()), actTimeRun, SLOT(turnOff()) );
//UpdateTime() if clock is stopped (so hidden objects get drawn)
	QObject::connect(data()->clock(), SIGNAL(clockStopped()), this, SLOT(updateTime()) );

//Focus Menu:
	ka = new KAction( i18n( "&Zenith" ), actionCollection(), "zenith" );
	ka->setDefaultShortcut( KShortcut( "Z" ) );
	connect( ka, SIGNAL( triggered() ), this, SLOT( slotPointFocus() ) );

	ka = new KAction( i18n( "&North" ), actionCollection(), "north");
	ka->setDefaultShortcut( KShortcut( "N" ) );
	connect( ka, SIGNAL( triggered() ), this, SLOT( slotPointFocus() ) );

	ka = new KAction( i18n( "&East" ), actionCollection(), "east");
	ka->setDefaultShortcut( KShortcut( "E" ) );
	connect( ka, SIGNAL( triggered() ), this, SLOT( slotPointFocus() ) );

	ka = new KAction( i18n( "&South" ), actionCollection(), "south");
	ka->setDefaultShortcut( KShortcut( "S" ) );
	connect( ka, SIGNAL( triggered() ), this, SLOT( slotPointFocus() ) );

	ka = new KAction( i18n( "&West" ), actionCollection(), "west");
	ka->setDefaultShortcut( KShortcut( "W" ) );
	connect( ka, SIGNAL( triggered() ), this, SLOT( slotPointFocus() ) );

	ka = new KAction( KIcon( "find" ), i18n( "&Find Object..." ), actionCollection(), "find_object" );
	ka->setDefaultShortcut( KShortcut( "Ctrl+F" ) );
	connect( ka, SIGNAL( triggered() ), this, SLOT( slotFind() ) );

        //FIXME: Use ToggleAction
	ka = new KAction( KIcon( "decrypted" ), i18n( "Engage &Tracking" ), actionCollection(), "track_object" );
	ka->setDefaultShortcut( KShortcut( "Ctrl+T"  ) );
	connect( ka, SIGNAL( triggered() ), this, SLOT( slotTrack() ) );

	ka = new KAction( i18n( "Set Focus &Manually..." ), actionCollection(), "manual_focus" );
	ka->setDefaultShortcut( KShortcut( "Ctrl+M" ) );
	connect( ka, SIGNAL( triggered() ), this, SLOT( slotManualFocus() ) );

//View Menu:
	KStdAction::zoomIn(this, SLOT( slotZoomIn() ), actionCollection(), "zoom_in" );
	KStdAction::zoomOut(this, SLOT( slotZoomOut() ), actionCollection(), "zoom_out" );

	ka = new KAction( KIcon( "viewmagfit" ), i18n( "&Default Zoom" ), actionCollection(), "zoom_default" );
	ka->setDefaultShortcut( KShortcut( "Ctrl+Z" ) );
	connect( ka, SIGNAL( triggered() ), this, SLOT( slotDefaultZoom() ) );

	ka = new KAction( KIcon( "viewmag" ), i18n( "&Zoom to Angular Size..." ), actionCollection(), "zoom_set" );
	ka->setDefaultShortcut( KShortcut( "Ctrl+Shift+Z" ) );
	connect( ka, SIGNAL( triggered() ), this, SLOT( slotSetZoom() ) );

	actCoordSys = new ToggleAction( i18n("Horizontal &Coordinates"), i18n( "Equatorial &Coordinates" ),
			KShortcut( Qt::Key_Space ), this, SLOT( slotCoordSys() ), actionCollection(), "coordsys" );

	KStdAction::fullScreen( this, SLOT( slotFullScreen() ), actionCollection(), 0 );


//Settings Menu:
	//Info Boxes option actions
	KToggleAction *a = new KToggleAction(i18nc( "Show the information boxes", "Show &Info Boxes"),
			actionCollection(), "show_boxes" );
	a->setChecked( Options::showInfoBoxes() );
	QObject::connect(a, SIGNAL( toggled(bool) ), infoBoxes(), SLOT(setVisible(bool)));
	QObject::connect(a, SIGNAL( toggled(bool) ), this, SLOT(slotShowGUIItem(bool)));

	a = new KToggleAction(i18nc( "Show time-related info box", "Show &Time Box"),
			actionCollection(), "show_time_box");
	QObject::connect(a, SIGNAL( toggled(bool) ), infoBoxes(), SLOT(showTimeBox(bool)));
	QObject::connect(a, SIGNAL( toggled(bool) ), this, SLOT(slotShowGUIItem(bool)));

	a = new KToggleAction(i18nc( "Show focus-related info box", "Show &Focus Box"),
			actionCollection(), "show_focus_box");
	QObject::connect(a, SIGNAL( toggled(bool) ), infoBoxes(), SLOT(showFocusBox(bool)));
	QObject::connect(a, SIGNAL( toggled(bool) ), this, SLOT(slotShowGUIItem(bool)));

	a = new KToggleAction(i18nc( "Show location-related info box", "Show &Location Box"),
			actionCollection(), "show_location_box");
	QObject::connect(a, SIGNAL( toggled(bool) ), infoBoxes(), SLOT(showGeoBox(bool)));
	QObject::connect(a, SIGNAL( toggled(bool) ), this, SLOT(slotShowGUIItem(bool)));

//Toolbar view options
	a = new KToggleAction(i18n( "Show Main Toolbar" ),
			actionCollection(), "show_mainToolBar");
	QObject::connect(a, SIGNAL( toggled(bool) ), this, SLOT(slotShowGUIItem(bool)));

	a = new KToggleAction(i18n( "Show View Toolbar" ),
			actionCollection(), "show_viewToolBar");
	QObject::connect(a, SIGNAL( toggled(bool) ), this, SLOT(slotShowGUIItem(bool)));

//Statusbar view options
	a = new KToggleAction(i18n( "Show Statusbar" ),
			actionCollection(), "show_statusBar");
	QObject::connect(a, SIGNAL( toggled(bool) ), this, SLOT(slotShowGUIItem(bool)));

	a = new KToggleAction(i18n( "Show Az/Alt Field" ),
			actionCollection(), "show_sbAzAlt");
	QObject::connect(a, SIGNAL( toggled(bool) ), this, SLOT(slotShowGUIItem(bool)));

	a = new KToggleAction(i18n( "Show RA/Dec Field" ),
			actionCollection(), "show_sbRADec");
	QObject::connect(a, SIGNAL( toggled(bool) ), this, SLOT(slotShowGUIItem(bool)));

//Color scheme actions.  These are added to the "colorschemes" KActionMenu.
	colorActionMenu = new KActionMenu( i18n( "C&olor Schemes" ), actionCollection(), "colorschemes" );
	addColorMenuItem( i18n( "&Default" ), "cs_default" );
	addColorMenuItem( i18n( "&Star Chart" ), "cs_chart" );
	addColorMenuItem( i18n( "&Night Vision" ), "cs_night" );
	addColorMenuItem( i18n( "&Moonless Night" ), "cs_moonless-night" );

//Add any user-defined color schemes:
	QFile file;
	QString line, schemeName, filename;
	file.setFileName( locate( "appdata", "colors.dat" ) ); //determine filename in local user KDE directory tree.
	if ( file.exists() && file.open( QIODevice::ReadOnly ) ) {
		QTextStream stream( &file );

		while ( !stream.atEnd() ) {
			line = stream.readLine();
			schemeName = line.left( line.indexOf( ':' ) );
			//I call it filename here, but it's used as the name of the action!
			filename = "cs_" + line.mid( line.indexOf( ':' ) +1, line.indexOf( '.' ) - line.indexOf( ':' ) - 1 );
			addColorMenuItem( i18n( schemeName.toLocal8Bit() ), filename.toLocal8Bit() );
		}
		file.close();
	}

	//Add FOV Symbol actions
	fovActionMenu = new KActionMenu( i18n( "&FOV Symbols" ), actionCollection(), "fovsymbols" );
	initFOV();

	ka = new KAction( KIcon( "kstars_geo" ), i18nc( "Location on Earth", "&Geographic..." ), actionCollection(), "geolocation" );
	ka->setDefaultShortcut( KShortcut( "Ctrl+G"  ) );
	connect( ka, SIGNAL( triggered() ), this, SLOT( slotGeoLocator() ) );

	KStdAction::preferences( this, SLOT( slotViewOps() ), actionCollection(), "configure" );

	ka = new KAction( KIcon( "wizard" ), i18n( "Startup Wizard..." ), actionCollection(), "startwizard" );
	connect( ka, SIGNAL( triggered() ), this, SLOT( slotWizard() ) );

//Tools Menu:
	new KAction( i18n( "Calculator..."), actionCollection(), "astrocalculator" );
	ka->setDefaultShortcut( KShortcut( "Ctrl+C") );
	connect( ka, SIGNAL( triggered() ), this, SLOT( slotCalculator() ) );

	ka = new KAction( i18n( "Observing List..."), actionCollection(), "obslist" );
	ka->setDefaultShortcut( KShortcut( "Ctrl+L") );
	connect( ka, SIGNAL( triggered() ), this, SLOT( slotObsList() ) );

	// enable action only if file was loaded and processed successfully.
	if (!data()->VariableStarsList.isEmpty()) {
		ka = new KAction( i18n( "AAVSO Light Curves..."), actionCollection(), "lightcurvegenerator");
		ka->setDefaultShortcut( KShortcut( "Ctrl+V") );
		connect( ka, SIGNAL( triggered() ), this, SLOT( slotLCGenerator() ) );
	}

	ka = new KAction( i18n( "Altitude vs. Time..."), actionCollection(), "altitude_vs_time");
	ka->setDefaultShortcut( KShortcut( "Ctrl+A") );
	connect( ka, SIGNAL( triggered() ), this, SLOT( slotAVT() ) );

	ka = new KAction( i18n( "What's up Tonight..."), actionCollection(), "whats_up_tonight");
	ka->setDefaultShortcut( KShortcut("Ctrl+U") );
	connect( ka, SIGNAL( triggered() ), this, SLOT( slotWUT() ) );

	ka = new KAction( i18n( "Glossary..."), actionCollection(), "glossary");
	ka->setDefaultShortcut( KShortcut("Ctrl+K") );
	connect( ka, SIGNAL( triggered() ), this, SLOT( slotGlossary() ) );

	ka = new KAction( i18n( "Script Builder..."), actionCollection(), "scriptbuilder");
	ka->setDefaultShortcut( KShortcut("Ctrl+B") );
	connect( ka, SIGNAL( triggered() ), this, SLOT( slotScriptBuilder() ) );

	ka = new KAction( i18n( "Solar System..."), actionCollection(), "solarsystem");
	ka->setDefaultShortcut( KShortcut("Ctrl+Y") );
	connect( ka, SIGNAL( triggered() ), this, SLOT( slotSolarSystem() ) );

	ka = new KAction( i18n( "Jupiter's Moons..."), actionCollection(), "jmoontool");
	ka->setDefaultShortcut( KShortcut("Ctrl+J") );
	connect( ka, SIGNAL( triggered() ), this, SLOT( slotJMoonTool() ) );

// devices Menu
	ka = new KAction( i18n("Telescope Wizard..."), actionCollection(), "telescope_wizard");
	connect( ka, SIGNAL( triggered() ), this, SLOT( slotTelescopeWizard() ) );

	ka = new KAction( i18n("Telescope Properties..."), actionCollection(), "telescope_properties");
	connect( ka, SIGNAL( triggered() ), this, SLOT( slotTelescopeProperties() ) );

	ka = new KAction( i18n("Device Manager..."), actionCollection(), "device_manager");
	connect( ka, SIGNAL( triggered() ), this, SLOT( slotINDIDriver() ) );

	ka = new KAction( i18n("Capture Image Sequence..."), actionCollection(), "capture_sequence");
	connect( ka, SIGNAL( triggered() ), this, SLOT( slotImageSequence() ) );
	ka->setEnabled(false);

	ka = new KAction( i18n("INDI Control Panel..."), actionCollection(), "indi_cpl");
	connect( ka, SIGNAL( triggered() ), this, SLOT( slotINDIPanel() ) );
	ka->setEnabled(false);

	ka = new KAction( i18n("Configure INDI..."), actionCollection(), "configure_indi");
	connect( ka, SIGNAL( triggered() ), this, SLOT( slotINDIConf() ) );

//Help Menu:
	ka = new KAction( KIcon( "idea" ), i18n( "Tip of the Day" ), actionCollection(), "help_tipofday" );
	connect( ka, SIGNAL( triggered() ), this, SLOT( slotTipOfDay() ) );

//Handbook toolBar item:
	ka = new KAction( KIcon( "contents" ), i18n( "&Handbook" ), actionCollection(), "handbook" );
	ka->setDefaultShortcut( KShortcut( "F1"  ) );
	connect( ka, SIGNAL( triggered() ), this, SLOT( appHelpActivated() ) );

//
//viewToolBar actions:
//

//show_stars:
	a = new KToggleAction( KIcon( "kstars_stars" ), i18n( "Toggle Stars" ), actionCollection(), "show_stars" );
	connect( a, SIGNAL( triggered() ), this, SLOT( slotViewToolBar() ) );

//show_deepsky:
	a = new KToggleAction( KIcon( "kstars_deepsky" ), i18n( "Toggle Deep Sky Objects" ), actionCollection(), "show_deepsky" );
	connect( a, SIGNAL( triggered() ), this, SLOT( slotViewToolBar() ) );

//show_planets:
	a = new KToggleAction( KIcon( "kstars_planets" ), i18n( "Toggle Solar System" ), actionCollection(), "show_planets" );
	connect( a, SIGNAL( triggered() ), this, SLOT( slotViewToolBar() ) );

//show_clines:
	a = new KToggleAction( KIcon( "kstars_clines" ), i18n( "Toggle Constellation Lines" ), actionCollection(), "show_clines" );
	connect( a, SIGNAL( triggered() ), this, SLOT( slotViewToolBar() ) );

//show_cnames:
	a = new KToggleAction( KIcon( "kstars_cnames" ), i18n( "Toggle Constellation Names" ), actionCollection(), "show_cnames" );
	connect( a, SIGNAL( triggered() ), this, SLOT( slotViewToolBar() ) );

//show_cbound:
	a = new KToggleAction( KIcon( "kstars_cbound" ), i18n( "Toggle Constellation Boundaries" ), actionCollection(), "show_cbounds" );
	connect( a, SIGNAL( triggered() ), this, SLOT( slotViewToolBar() ) );

//show_mw:
	a = new KToggleAction( KIcon( "kstars_mw" ), i18n( "Toggle Milky Way" ), actionCollection(), "show_mw" );
	connect( a, SIGNAL( triggered() ), this, SLOT( slotViewToolBar() ) );

//show_grid:
	a = new KToggleAction( KIcon( "kstars_grid" ), i18n( "Toggle Coordinate Grid" ), actionCollection(), "show_grid" );
	connect( a, SIGNAL( triggered() ), this, SLOT( slotViewToolBar() ) );

//show_horizon:
	a = new KToggleAction( KIcon( "kstars_horizon" ), i18n( "Toggle Ground" ), actionCollection(), "show_horizon" );
	connect( a, SIGNAL( triggered() ), this, SLOT( slotViewToolBar() ) );

	if (Options::fitsSaveDirectory().isEmpty())
			Options::setFitsSaveDirectory(QDir:: homePath());
}

void KStars::initFOV() {
	//Read in the user's fov.dat and populate the FOV menu with its symbols.  If no fov.dat exists, populate
	//create a default version.
	QFile f;
	QStringList fields;
	QString nm;

	f.setFileName( locateLocal( "appdata", "fov.dat" ) );

	//if file s empty, let's start over
	if ( (uint)f.size() == 0 ) f.remove();

	if ( ! f.exists() ) {
		if ( ! f.open( QIODevice::WriteOnly ) ) {
			kDebug() << i18n( "Could not open fov.dat." ) << endl;
		} else {
			QTextStream ostream(&f);
			ostream << i18nc( "Do not use a field-of-view indicator", "No FOV" ) <<  ":0.0:0:#AAAAAA" << endl;
			ostream << i18nc( "use field-of-view for binoculars", "7x35 Binoculars" ) << ":558:1:#AAAAAA" << endl;
			ostream << i18nc( "use 1-degree field-of-view indicator", "One Degree" ) << ":60:2:#AAAAAA" << endl;
			ostream << i18nc( "use HST field-of-view indicator", "HST WFPC2" ) << ":2.4:0:#AAAAAA" << endl;
			ostream << i18nc( "use Radiotelescope HPBW", "30m at 1.3cm" ) << ":1.79:1:#AAAAAA" << endl;
			f.close();
		}
	}

	//just populate the FOV menu with items, don't need to fully parse the lines
	if ( f.open( QIODevice::ReadOnly ) ) {
		QTextStream stream( &f );
		while ( !stream.atEnd() ) {
			QString line = stream.readLine();
			fields = line.split( ":" );

			if ( fields.count() == 4 ) {
				nm = fields[0].trimmed();
				KToggleAction *kta = new KToggleAction( nm, actionCollection(), nm.toUtf8() );
				connect( kta, SIGNAL( toggled( bool ) ), this, SLOT( slotTargetSymbol() ) );

				/* FIXME update deprecated KToggleFunctions */
				//kta->setExclusiveGroup( "fovsymbol" );
				if ( nm == Options::fOVName() ) kta->setChecked( true );
				fovActionMenu->addAction( kta );
			}
		}
	} else {
		kDebug() << i18n( "Could not open file: %1", f.fileName() ) << endl;
	}

	fovActionMenu->popupMenu()->addSeparator();
	KAction *ka = new KAction( i18n( "Edit FOV Symbols..." ), actionCollection(), "edit_fov" );
	connect( ka, SIGNAL( triggered() ), this, SLOT( slotFOVEdit() ) );
	fovActionMenu->addAction( ka );
}

void KStars::initStatusBar() {
	statusBar()->insertPermanentItem( i18n( " Welcome to KStars " ), 0, 1 );
	statusBar()->setItemAlignment( 0, Qt::AlignLeft | Qt::AlignVCenter );

	QString s = "000d 00m 00s,   +00d 00\' 00\""; //only need this to set the width
	if ( Options::showAltAzField() ) {
		statusBar()->insertItem( s, 1 );
		statusBar()->setItemAlignment( 1, Qt::AlignRight | Qt::AlignVCenter );
		statusBar()->changeItem( QString(), 1 );
	}

	if ( Options::showRADecField() ) {
		statusBar()->insertItem( s, 2 );
		statusBar()->setItemAlignment( 2, Qt::AlignRight | Qt::AlignVCenter );
		statusBar()->changeItem( QString(), 2 );
	}

	if ( ! Options::showStatusBar() ) statusBar()->hide();
}

void KStars::datainitFinished(bool worked) {
	//Quit program if something went wrong with initialization of data
	if (!worked) {
		kapp->quit();
		return;
	}

	//delete the splash screen window
	if ( splash ) {
		delete splash;
		splash = 0;
	}

	//Add GUI elements to main window
	buildGUI();

	//Time-related connections
	connect( data()->clock(), SIGNAL( timeAdvanced() ), this,
		 SLOT( updateTime() ) );
	connect( data()->clock(), SIGNAL( timeChanged() ), this,
		 SLOT( updateTime() ) );
 	connect( data()->clock(), SIGNAL( scaleChanged( float ) ), map(),
		 SLOT( slotClockSlewing() ) );
	connect(data(), SIGNAL( update() ), map(), SLOT( forceUpdateNow() ) );
	connect( TimeStep, SIGNAL( scaleChanged( float ) ), data(),
		 SLOT( setTimeDirection( float ) ) );
	connect( TimeStep, SIGNAL( scaleChanged( float ) ), data()->clock(),
		 SLOT( setScale( float )) );
	connect( TimeStep, SIGNAL( scaleChanged( float ) ), this,
		 SLOT( mapGetsFocus() ) );

	//Initialize INDIMenu
	indimenu = new INDIMenu(this);

	//Initialize Observing List
	obsList = new ObservingList( this );

	data()->setFullTimeUpdate();
	updateTime();

	//Do not start the clock if "--paused" specified on the cmd line
	if ( StartClockRunning )
		data()->clock()->start();

	// Connect cache function for Find dialog
	connect( data(), SIGNAL( clearCache() ), this,
		 SLOT( clearCachedFindDialog() ) );

	//show the window.  must be before kswizard and messageboxes
	show();

	//Initialize focus
	initFocus();

	//Propagate config settings
	applyConfig();

	//If this is the first startup, show the wizard
	if ( Options::runStartupWizard() ) {
		slotWizard();
	}

	//Start listening for DCOP
	kapp->dcopClient()->resume();

	//Show TotD
	KTipDialog::showTip( "kstars/tips" );
}

void KStars::initFocus() {
	SkyPoint newPoint;
	//If useDefaultOptions, then we set Az/Alt.  Otherwise, set RA/Dec
	if ( data()->useDefaultOptions ) {
		newPoint.setAz( Options::focusRA() );
		newPoint.setAlt( Options::focusDec() + 0.0001 );
		newPoint.HorizontalToEquatorial( LST(), geo()->lat() );
	} else {
		newPoint.set( Options::focusRA(), Options::focusDec() );
		newPoint.EquatorialToHorizontal( LST(), geo()->lat() );
	}

//need to set focusObject before updateTime, otherwise tracking is set to false
	if ( (Options::focusObject() != i18n( "star" ) ) &&
		     (Options::focusObject() != i18n( "nothing" ) ) )
			map()->setFocusObject( data()->objectNamed( Options::focusObject() ) );

	//if user was tracking last time, track on same object now.
	if ( Options::isTracking() ) {
		map()->setClickedObject( data()->objectNamed( Options::focusObject() ) );
		if ( map()->clickedObject() ) {
		  map()->setFocusPoint( map()->clickedObject() );
		  map()->setFocusObject( map()->clickedObject() );
		} else {
		  map()->setFocusPoint( &newPoint );
		}
	} else {
		map()->setFocusPoint( &newPoint );
	}

	data()->setSnapNextFocus();
	map()->setDestination( map()->focusPoint() );
	map()->destination()->EquatorialToHorizontal( LST(), geo()->lat() );
	map()->setFocus( map()->destination() );
	map()->focus()->EquatorialToHorizontal( LST(), geo()->lat() );

	map()->showFocusCoords();

	map()->setOldFocus( map()->focus() );
	map()->oldfocus()->setAz( map()->focus()->az()->Degrees() );
	map()->oldfocus()->setAlt( map()->focus()->alt()->Degrees() );

	//Check whether initial position is below the horizon.
	if ( Options::useAltAz() && Options::showGround() &&
			map()->focus()->alt()->Degrees() < -1.0 ) {
		QString caption = i18n( "Initial Position is Below Horizon" );
		QString message = i18n( "The initial position is below the horizon.\nWould you like to reset to the default position?" );
		if ( KMessageBox::warningYesNo( this, message, caption,
				i18n("Reset Position"), i18n("Do Not Reset"), "dag_start_below_horiz" ) == KMessageBox::Yes ) {
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

	//If there is a focusObject() and it is a SS body, add a temporary Trail
	if ( map()->focusObject() && map()->focusObject()->isSolarSystem()
			&& Options::useAutoTrail() ) {
		((KSPlanetBase*)map()->focusObject())->addToTrail();
		data()->temporaryTrail = true;
	}

	//Store focus coords in Options object before calling applyConfig()
	Options::setFocusRA( map()->focus()->ra()->Hours() );
	Options::setFocusDec( map()->focus()->dec()->Degrees() );
}
void KStars::buildGUI() {
	//create the skymap
	skymap = new SkyMap( data(), this );
	setCentralWidget( skymap );

	//Initialize menus, toolbars, and statusbars
	initStatusBar();
	initActions();

	// 2nd parameter must be false, or plugActionList won't work!
	createGUI("kstarsui.rc", false);

	//Add timestep widget to toolbar
	//FIXME: Need to add the widget to kstarsToolBar,
	//but 'toolBar("kstarsToolBar")' doesn't work...
	TimeStep = new TimeStepBox( toolBar() );
	toolBar()->addWidget( TimeStep );

	//Initialize FOV symbol from options
	data()->fovSymbol.setName( Options::fOVName() );
	data()->fovSymbol.setSize( Options::fOVSize() );
	data()->fovSymbol.setShape( Options::fOVShape() );
	data()->fovSymbol.setColor( Options::fOVColor() );

	//get focus of keyboard and mouse actions (for example zoom in with +)
	map()->QWidget::setFocus();

	resize( Options::windowWidth(), Options::windowHeight() );

	// check zoom in/out buttons
	if ( Options::zoomFactor() >= MAXZOOM ) actionCollection()->action("zoom_in")->setEnabled( false );
	if ( Options::zoomFactor() <= MINZOOM ) actionCollection()->action("zoom_out")->setEnabled( false );
}
