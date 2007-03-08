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

#include <kactioncollection.h>
#include <kactionmenu.h>
#include <kiconloader.h>
#include <kapplication.h>
#include <kmenu.h>
#include <kstatusbar.h>
#include <ktip.h>
#include <kmessagebox.h>
#include <kstandardaction.h>
#include <kstandarddirs.h>
#include <kdeversion.h>
#include <ktoggleaction.h>
#include <ktoolbar.h>
#include <kicon.h>

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
	KIconLoader::global()->addAppDir( "kstars" );

//File Menu:
	QAction *ka = actionCollection()->addAction( "new_window" );
        ka->setIcon( KIcon( "window-new" ) );
        ka->setText( i18n("&New Window") );
	ka->setShortcuts( KShortcut( Qt::CTRL+Qt::Key_N ) );
	connect( ka, SIGNAL( triggered() ), this, SLOT( newWindow() ) );

	ka = actionCollection()->addAction( "close_window");
        ka->setIcon( KIcon( "window-close" ) );
        ka->setText( i18n("&Close Window") );
	ka->setShortcuts( KShortcut( Qt::CTRL+Qt::Key_W ) );
	connect( ka, SIGNAL( triggered() ), this, SLOT( closeWindow() ) );

	ka = actionCollection()->addAction( "get_data" );
        ka->setIcon( KIcon( "get-hot-new-stuff" ) );
        ka->setText( i18n( "&Download Data..." ) );
	ka->setShortcuts( KShortcut( Qt::CTRL+Qt::Key_D ) );
	connect( ka, SIGNAL( triggered() ), this, SLOT( slotDownload() ) );

	ka = actionCollection()->addAction( "open_file");
        ka->setIcon( KIcon( "document-open" ) );
        ka->setText( i18n( "Open FITS...") );
	ka->setShortcuts( KShortcut( Qt::CTRL+Qt::Key_O ) );
	connect( ka, SIGNAL( triggered() ), this, SLOT( slotOpenFITS() ) );

	ka = actionCollection()->addAction( "export_image" );
        ka->setIcon( KIcon( "fileexport" ) );
        ka->setText( i18n( "&Save Sky Image..." ) );
	ka->setShortcuts( KShortcut( Qt::CTRL+Qt::Key_I ) );
	connect( ka, SIGNAL( triggered() ), this, SLOT( slotExportImage() ) );

	ka = actionCollection()->addAction( "run_script" );
        ka->setIcon( KIcon( "launch" ) );
        ka->setText( i18n( "&Run Script..." ) );
	ka->setShortcuts( KShortcut( Qt::CTRL+Qt::Key_R ) );
	connect( ka, SIGNAL( triggered() ), this, SLOT( slotRunScript() ) );

	actionCollection()->addAction( KStandardAction::Print, "print", this, SLOT( slotPrint() ) );
	actionCollection()->addAction( KStandardAction::Quit, "quit", this, SLOT( close() ) );

//Time Menu:
	ka = actionCollection()->addAction( "time_to_now" );
        ka->setText( i18n( "Set Time to &Now" ) );
	ka->setShortcuts( KShortcut( Qt::CTRL+Qt::Key_E  ) );
	connect( ka, SIGNAL( triggered() ), this, SLOT( slotSetTimeToNow() ) );

	ka = actionCollection()->addAction( "time_dialog" );
        ka->setIcon( KIcon( "history" ) );
        ka->setText( i18nc( "set Clock to New Time", "&Set Time..." ) );
	ka->setShortcuts( KShortcut( Qt::CTRL+Qt::Key_S  ) );
	connect( ka, SIGNAL( triggered() ), this, SLOT( slotSetTime() ) );

	ToggleAction *actTimeRun = new ToggleAction( KIcon( "media-playback-pause" ), i18n( "Stop &Clock" ),
				KIcon( "media-playback-start" ), i18n("Start &Clock"),
                                KShortcut(), this, SLOT( slotToggleTimer() ), this );
        actionCollection()->addAction( "clock_startstop", actTimeRun );
	actTimeRun->setOffToolTip( i18n( "Start Clock" ) );
	actTimeRun->setOnToolTip( i18n( "Stop Clock" ) );
	QObject::connect(data()->clock(), SIGNAL(clockStarted()), actTimeRun, SLOT(turnOn()) );
	QObject::connect(data()->clock(), SIGNAL(clockStopped()), actTimeRun, SLOT(turnOff()) );
//UpdateTime() if clock is stopped (so hidden objects get drawn)
	QObject::connect(data()->clock(), SIGNAL(clockStopped()), this, SLOT(updateTime()) );

//Pointing Menu:
	ka = actionCollection()->addAction( "zenith" );
        ka->setText( i18n( "&Zenith" ) );
	ka->setShortcuts( KShortcut( "Z" ) );
	connect( ka, SIGNAL( triggered() ), this, SLOT( slotPointFocus() ) );

	ka = actionCollection()->addAction( "north");
        ka->setText( i18n( "&North" ) );
	ka->setShortcuts( KShortcut( "N" ) );
	connect( ka, SIGNAL( triggered() ), this, SLOT( slotPointFocus() ) );

	ka = actionCollection()->addAction( "east");
        ka->setText( i18n( "&East" ) );
	ka->setShortcuts( KShortcut( "E" ) );
	connect( ka, SIGNAL( triggered() ), this, SLOT( slotPointFocus() ) );

	ka = actionCollection()->addAction( "south");
        ka->setText( i18n( "&South" ) );
	ka->setShortcuts( KShortcut( "S" ) );
	connect( ka, SIGNAL( triggered() ), this, SLOT( slotPointFocus() ) );

	ka = actionCollection()->addAction( "west");
        ka->setText( i18n( "&West" ) );
	ka->setShortcuts( KShortcut( "W" ) );
	connect( ka, SIGNAL( triggered() ), this, SLOT( slotPointFocus() ) );

	ka = actionCollection()->addAction( "find_object" );
        ka->setIcon( KIcon( "edit-find" ) );
        ka->setText( i18n( "&Find Object..." ) );
	ka->setShortcuts( KShortcut( Qt::CTRL+Qt::Key_F ) );
	connect( ka, SIGNAL( triggered() ), this, SLOT( slotFind() ) );

	//FIXME: Use ToggleAction
	ka = actionCollection()->addAction( "track_object" );
        ka->setIcon( KIcon( "decrypted" ) );
        ka->setText( i18n( "Engage &Tracking" ) );
	ka->setShortcuts( KShortcut( Qt::CTRL+Qt::Key_T  ) );
	connect( ka, SIGNAL( triggered() ), this, SLOT( slotTrack() ) );

	ka = actionCollection()->addAction( "manual_focus" );
        ka->setText( i18n( "Set Focus &Manually..." ) );
	ka->setShortcuts( KShortcut( Qt::CTRL+Qt::Key_M ) );
	connect( ka, SIGNAL( triggered() ), this, SLOT( slotManualFocus() ) );

//View Menu:
	actionCollection()->addAction( KStandardAction::ZoomIn, "zoom_in", this, SLOT( slotZoomIn() ) );
	actionCollection()->addAction( KStandardAction::ZoomOut, "zoom_out", this, SLOT( slotZoomOut() ) );

	ka = actionCollection()->addAction( "zoom_default" );
        ka->setIcon( KIcon( "zoom-best-fit" ) );
        ka->setText( i18n( "&Default Zoom" ) );
	ka->setShortcuts( KShortcut( Qt::CTRL+Qt::Key_Z ) );
	connect( ka, SIGNAL( triggered() ), this, SLOT( slotDefaultZoom() ) );

	ka = actionCollection()->addAction( "zoom_set" );
        ka->setIcon( KIcon( "zoom-original" ) );
        ka->setText( i18n( "&Zoom to Angular Size..." ) );
	ka->setShortcuts( KShortcut( Qt::CTRL+Qt::SHIFT+Qt::Key_Z ) );
	connect( ka, SIGNAL( triggered() ), this, SLOT( slotSetZoom() ) );

	actionCollection()->addAction( KStandardAction::FullScreen, this, SLOT( slotFullScreen() ) );

	actCoordSys = new ToggleAction( i18n("Horizontal &Coordinates"), i18n( "Equatorial &Coordinates" ),
                                        KShortcut( "Space" ), this, SLOT( slotCoordSys() ), this );
        actionCollection()->addAction( "coordsys", actCoordSys );

	ka = actionCollection()->addAction( "project_lambert" );
        ka->setText( i18n( "&Lambert Azimuthal Equal-area" ) );
	ka->setShortcuts( KShortcut( "F5" ) );
	ka->setCheckable( true );
	projectionGroup->addAction( ka );
	if ( Options::projection() == SkyMap::Lambert ) ka->setChecked( true );
	connect( ka, SIGNAL( triggered() ), this, SLOT( slotMapProjection() ) );

	ka = actionCollection()->addAction( "project_azequidistant" );
        ka->setText( i18n( "&Azimuthal Equidistant" ) );
	ka->setShortcuts( KShortcut( "F6" ) );
	ka->setCheckable( true );
	projectionGroup->addAction( ka );
	if ( Options::projection() == SkyMap::AzimuthalEquidistant ) ka->setChecked( true );
	connect( ka, SIGNAL( triggered() ), this, SLOT( slotMapProjection() ) );

	ka = actionCollection()->addAction( "project_orthographic" );
        ka->setText( i18n( "&Orthographic" ) );
	ka->setShortcuts( KShortcut( "F7" ) );
	ka->setCheckable( true );
	projectionGroup->addAction( ka );
	if ( Options::projection() == SkyMap::Orthographic ) ka->setChecked( true );
	connect( ka, SIGNAL( triggered() ), this, SLOT( slotMapProjection() ) );

	ka = actionCollection()->addAction( "project_equirectangular" );
        ka->setText( i18n( "&Equirectangular" ) );
	ka->setShortcuts( KShortcut( "F8" ) );
	ka->setCheckable( true );
	projectionGroup->addAction( ka );
	if ( Options::projection() == SkyMap::Equirectangular ) ka->setChecked( true );
	connect( ka, SIGNAL( triggered() ), this, SLOT( slotMapProjection() ) );

	ka = actionCollection()->addAction( "project_stereographic" );
        ka->setText( i18n( "&Stereographic" ) );
	ka->setShortcuts( KShortcut( "F9" ) );
	ka->setCheckable( true );
	projectionGroup->addAction( ka );
	if ( Options::projection() == SkyMap::Stereographic ) ka->setChecked( true );
	connect( ka, SIGNAL( triggered() ), this, SLOT( slotMapProjection() ) );

	ka = actionCollection()->addAction( "project_gnomonic" );
        ka->setText( i18n( "&Gnomonic" ) );
	ka->setShortcuts( KShortcut( "F10" ) );
	ka->setCheckable( true );
	projectionGroup->addAction( ka );
	if ( Options::projection() == SkyMap::Gnomonic ) ka->setChecked( true );
	connect( ka, SIGNAL( triggered() ), this, SLOT( slotMapProjection() ) );

//Settings Menu:
	//Info Boxes option actions
	KToggleAction *a = actionCollection()->add<KToggleAction>( "show_boxes" );
        a->setText( i18nc( "Show the information boxes", "Show &Info Boxes") );
	a->setChecked( Options::showInfoBoxes() );
	QObject::connect(a, SIGNAL( toggled(bool) ), infoBoxes(), SLOT(setVisible(bool)));
	QObject::connect(a, SIGNAL( toggled(bool) ), this, SLOT(slotShowGUIItem(bool)));

	a = actionCollection()->add<KToggleAction>( "show_time_box");
        a->setText( i18nc( "Show time-related info box", "Show &Time Box") );
	QObject::connect(a, SIGNAL( toggled(bool) ), infoBoxes(), SLOT(showTimeBox(bool)));
	QObject::connect(a, SIGNAL( toggled(bool) ), this, SLOT(slotShowGUIItem(bool)));

	a = actionCollection()->add<KToggleAction>( "show_focus_box");
        a->setText( i18nc( "Show focus-related info box", "Show &Focus Box") );
	QObject::connect(a, SIGNAL( toggled(bool) ), infoBoxes(), SLOT(showFocusBox(bool)));
	QObject::connect(a, SIGNAL( toggled(bool) ), this, SLOT(slotShowGUIItem(bool)));

	a = actionCollection()->add<KToggleAction>( "show_location_box");
        a->setText( i18nc( "Show location-related info box", "Show &Location Box") );
	QObject::connect(a, SIGNAL( toggled(bool) ), infoBoxes(), SLOT(showGeoBox(bool)));
	QObject::connect(a, SIGNAL( toggled(bool) ), this, SLOT(slotShowGUIItem(bool)));

//Toolbar options
	a = actionCollection()->add<KToggleAction>( "show_mainToolBar");
        a->setText( i18n( "Show Main Toolbar" ) );
	QObject::connect(a, SIGNAL( toggled(bool) ), this, SLOT(slotShowGUIItem(bool)));

	a = actionCollection()->add<KToggleAction>( "show_viewToolBar");
	a->setText( i18n( "Show View Toolbar" ) );
	QObject::connect(a, SIGNAL( toggled(bool) ), this, SLOT(slotShowGUIItem(bool)));

	actionCollection()->addAction( KStandardAction::ConfigureToolbars, "configure_toolbars",
                                       this, SLOT( slotConfigureToolbars() ) );

//Statusbar view options
	a = actionCollection()->add<KToggleAction>( "show_statusBar");
	a->setText( i18n( "Show Statusbar" ) );
	QObject::connect(a, SIGNAL( toggled(bool) ), this, SLOT(slotShowGUIItem(bool)));

	a = actionCollection()->add<KToggleAction>( "show_sbAzAlt");
	a->setText( i18n( "Show Az/Alt Field" ) );
	QObject::connect(a, SIGNAL( toggled(bool) ), this, SLOT(slotShowGUIItem(bool)));

	a = actionCollection()->add<KToggleAction>( "show_sbRADec");
	a->setText( i18n( "Show RA/Dec Field" ) );
	QObject::connect(a, SIGNAL( toggled(bool) ), this, SLOT(slotShowGUIItem(bool)));

//Color scheme actions.  These are added to the "colorschemes" KActionMenu.
	colorActionMenu = actionCollection()->add<KActionMenu>( "colorschemes" );
	colorActionMenu->setText( i18n( "C&olor Schemes" ) );
	addColorMenuItem( i18n( "&Default" ), "cs_default" );
	addColorMenuItem( i18n( "&Star Chart" ), "cs_chart" );
	addColorMenuItem( i18n( "&Night Vision" ), "cs_night" );
	addColorMenuItem( i18n( "&Moonless Night" ), "cs_moonless-night" );

//Add any user-defined color schemes:
	QFile file;
	QString line, schemeName, filename;
	file.setFileName( KStandardDirs::locate( "appdata", "colors.dat" ) ); //determine filename in local user KDE directory tree.
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
	fovActionMenu = actionCollection()->add<KActionMenu>( "fovsymbols" );
        fovActionMenu->setText( i18n( "&FOV Symbols" ) );
	initFOV();

	ka = actionCollection()->addAction( "geolocation" );
        ka->setIcon( KIcon( "kstars_geo" ) );
        ka->setText( i18nc( "Location on Earth", "&Geographic..." ) );
	ka->setShortcuts( KShortcut( Qt::CTRL+Qt::Key_G  ) );
	connect( ka, SIGNAL( triggered() ), this, SLOT( slotGeoLocator() ) );

	actionCollection()->addAction( KStandardAction::Preferences, "configure", this, SLOT( slotViewOps() ) );

	ka = actionCollection()->addAction( "startwizard" );
        ka->setIcon( KIcon( "wizard" ) );
        ka->setText( i18n( "Startup Wizard..." ) );
	connect( ka, SIGNAL( triggered() ), this, SLOT( slotWizard() ) );

//Tools Menu:
	ka = actionCollection()->addAction( "astrocalculator" );
        ka->setText( i18n( "Calculator...") );
	ka->setShortcuts( KShortcut( Qt::CTRL+Qt::Key_C ) );
	connect( ka, SIGNAL( triggered() ), this, SLOT( slotCalculator() ) );

	ka = actionCollection()->addAction( "obslist" );
        ka->setText( i18n( "Observing List...") );
	ka->setShortcuts( KShortcut( Qt::CTRL+Qt::Key_L ) );
	connect( ka, SIGNAL( triggered() ), this, SLOT( slotObsList() ) );

	// enable action only if file was loaded and processed successfully.
	if (!data()->VariableStarsList.isEmpty()) {
		ka = actionCollection()->addAction( "lightcurvegenerator" );
                ka->setText( i18n( "AAVSO Light Curves...") );
		ka->setShortcuts( KShortcut( Qt::CTRL+Qt::Key_V ) );
		connect( ka, SIGNAL( triggered() ), this, SLOT( slotLCGenerator() ) );
	}

	ka = actionCollection()->addAction( "altitude_vs_time");
        ka->setText( i18n( "Altitude vs. Time...") );
	ka->setShortcuts( KShortcut( Qt::CTRL+Qt::Key_A ) );
	connect( ka, SIGNAL( triggered() ), this, SLOT( slotAVT() ) );

	ka = actionCollection()->addAction( "whats_up_tonight");
        ka->setText( i18n( "What's up Tonight...") );
	ka->setShortcuts( KShortcut(Qt::CTRL+Qt::Key_U ) );
	connect( ka, SIGNAL( triggered() ), this, SLOT( slotWUT() ) );

	ka = actionCollection()->addAction( "glossary");
        ka->setText( i18n( "Glossary...") );
	ka->setShortcuts( KShortcut(Qt::CTRL+Qt::Key_K ) );
	connect( ka, SIGNAL( triggered() ), this, SLOT( slotGlossary() ) );

	ka = actionCollection()->addAction( "scriptbuilder");
        ka->setText( i18n( "Script Builder...") );
	ka->setShortcuts( KShortcut(Qt::CTRL+Qt::Key_B ) );
	connect( ka, SIGNAL( triggered() ), this, SLOT( slotScriptBuilder() ) );

	ka = actionCollection()->addAction( "solarsystem");
        ka->setText( i18n( "Solar System...") );
	ka->setShortcuts( KShortcut(Qt::CTRL+Qt::Key_Y ) );
	connect( ka, SIGNAL( triggered() ), this, SLOT( slotSolarSystem() ) );

	ka = actionCollection()->addAction( "jmoontool");
        ka->setText( i18n( "Jupiter's Moons...") );
	ka->setShortcuts( KShortcut(Qt::CTRL+Qt::Key_J ) );
	connect( ka, SIGNAL( triggered() ), this, SLOT( slotJMoonTool() ) );

// devices Menu
#ifndef Q_WS_WIN
	ka = actionCollection()->addAction( "telescope_wizard");
        ka->setText( i18n("Telescope Wizard...") );
	connect( ka, SIGNAL( triggered() ), this, SLOT( slotTelescopeWizard() ) );

	ka = actionCollection()->addAction( "telescope_properties");
        ka->setText( i18n("Telescope Properties...") );
	connect( ka, SIGNAL( triggered() ), this, SLOT( slotTelescopeProperties() ) );

	ka = actionCollection()->addAction( "device_manager");
        ka->setText( i18n("Device Manager...") );
	connect( ka, SIGNAL( triggered() ), this, SLOT( slotINDIDriver() ) );

	ka = actionCollection()->addAction( "capture_sequence");
        ka->setText( i18n("Capture Image Sequence...") );
	connect( ka, SIGNAL( triggered() ), this, SLOT( slotImageSequence() ) );
	ka->setEnabled(false);

	ka = actionCollection()->addAction( "indi_cpl");
        ka->setText( i18n("INDI Control Panel...") );
	connect( ka, SIGNAL( triggered() ), this, SLOT( slotINDIPanel() ) );
	ka->setEnabled(false);

	ka = actionCollection()->addAction( "configure_indi");
        ka->setText( i18n("Configure INDI...") );
	connect( ka, SIGNAL( triggered() ), this, SLOT( slotINDIConf() ) );
#endif

//Help Menu:
//	KStandardAction::tipOfDay(this, SLOT( slotTipOfDay() ), actionCollection(), "help_tipofday" );

//	KStandardAction::help(this, SLOT( appHelpActivated() ), actionCollection(), "help_contents" );

	//Add timestep widget for toolbar
	TimeStep = new TimeStepBox( toolBar("kstarsToolBar") );
	ka = actionCollection()->addAction( "timestep_control" );
        ka->setText( i18n("Time step control") );
	qobject_cast<KAction*>( ka )->setDefaultWidget( TimeStep );

//
//viewToolBar actions:
//
//show_stars:
	a = actionCollection()->add<KToggleAction>( "show_stars" );
        a->setIcon( KIcon( "kstars_stars" ) );
        a->setText( i18n( "Toggle Stars" ) );
	connect( a, SIGNAL( triggered() ), this, SLOT( slotViewToolBar() ) );

//show_deepsky:
	a = actionCollection()->add<KToggleAction>( "show_deepsky" );
        a->setIcon( KIcon( "kstars_deepsky" ) );
        a->setText( i18n( "Toggle Deep Sky Objects" ) );
	connect( a, SIGNAL( triggered() ), this, SLOT( slotViewToolBar() ) );

//show_planets:
	a = actionCollection()->add<KToggleAction>( "show_planets" );
        a->setIcon( KIcon( "kstars_planets" ) );
        a->setText( i18n( "Toggle Solar System" ) );
	connect( a, SIGNAL( triggered() ), this, SLOT( slotViewToolBar() ) );

//show_clines:
	a = actionCollection()->add<KToggleAction>( "show_clines" );
        a->setIcon( KIcon( "kstars_clines" ) );
        a->setText( i18n( "Toggle Constellation Lines" ) );
	connect( a, SIGNAL( triggered() ), this, SLOT( slotViewToolBar() ) );

//show_cnames:
	a = actionCollection()->add<KToggleAction>( "show_cnames" );
        a->setIcon( KIcon( "kstars_cnames" ) );
        a->setText( i18n( "Toggle Constellation Names" ) );
	connect( a, SIGNAL( triggered() ), this, SLOT( slotViewToolBar() ) );

//show_cbound:
	a = actionCollection()->add<KToggleAction>( "show_cbounds" );
        a->setIcon( KIcon( "kstars_cbound" ) );
        a->setText( i18n( "Toggle Constellation Boundaries" ) );
	connect( a, SIGNAL( triggered() ), this, SLOT( slotViewToolBar() ) );

//show_mw:
	a = actionCollection()->add<KToggleAction>( "show_mw" );
        a->setIcon( KIcon( "kstars_mw" ) );
        a->setText( i18n( "Toggle Milky Way" ) );
	connect( a, SIGNAL( triggered() ), this, SLOT( slotViewToolBar() ) );

//show_grid:
	a = actionCollection()->add<KToggleAction>( "show_grid" );
        a->setIcon( KIcon( "kstars_grid" ) );
        a->setText( i18n( "Toggle Coordinate Grid" ) );
	connect( a, SIGNAL( triggered() ), this, SLOT( slotViewToolBar() ) );

//show_horizon:
	a = actionCollection()->add<KToggleAction>( "show_horizon" );
        a->setIcon( KIcon( "kstars_horizon" ) );
        a->setText( i18n( "Toggle Ground" ) );
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

	f.setFileName( KStandardDirs::locateLocal( "appdata", "fov.dat" ) );

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
				KToggleAction *kta = actionCollection()->add<KToggleAction>( nm.toUtf8() );
                                kta->setText( nm );
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

	fovActionMenu->addSeparator();
	QAction *ka = actionCollection()->addAction( "edit_fov" );
        ka->setText( i18n( "Edit FOV Symbols..." ) );
	connect( ka, SIGNAL( triggered() ), this, SLOT( slotFOVEdit() ) );
	fovActionMenu->addAction( ka );
}

void KStars::initStatusBar() {
	statusBar()->insertPermanentItem( i18n( " Welcome to KStars " ), 0, 1 );
	statusBar()->setItemAlignment( 0, Qt::AlignLeft | Qt::AlignVCenter );

	QString s = "000d 00m 00s,   +00d 00\' 00\""; //only need this to set the width
	if ( Options::showAltAzField() ) {
		statusBar()->insertPermanentFixedItem( s, 1 );
		statusBar()->setItemAlignment( 1, Qt::AlignRight | Qt::AlignVCenter );
		statusBar()->changeItem( QString(), 1 );
	}

	if ( Options::showRADecField() ) {
		statusBar()->insertPermanentFixedItem( s, 2 );
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

	//Show TotD
	KTipDialog::showTip( "kstars/tips" );

	//DEBUG
	kDebug() << "The current Date/Time is: " << ExtDateTime::currentDateTime().toString() << endl;
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
				KGuiItem(i18n("Reset Position")), KGuiItem(i18n("Do Not Reset")), "dag_start_below_horiz" ) == KMessageBox::Yes ) {
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

#ifdef Q_WS_WIN
	createGUI("kstarsui-win.rc");
#else
	createGUI("kstarsui.rc");
#endif
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
