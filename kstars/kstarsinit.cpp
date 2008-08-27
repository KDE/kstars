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
#include <knewstuff2/ui/knewstuffaction.h>

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
#include "indimenu.h"
#include "simclock.h"
#include "widgets/timestepbox.h"

#include "config-kstars.h"

//This file contains functions that kstars calls at startup (except constructors).
//These functions are declared in kstars.h

void KStars::initActions() {
    KIconLoader::global()->addAppDir( "kstars" );

    //File Menu:
//FIXME: New window disabled
//     QAction *ka = actionCollection()->addAction( "new_window" );
//     ka->setIcon( KIcon( "window-new" ) );
//     ka->setText( i18n("&New Window") );
//     ka->setShortcuts( KShortcut( Qt::CTRL+Qt::Key_N ) );
//     connect( ka, SIGNAL( triggered() ), this, SLOT( newWindow() ) );
// 
//     ka = actionCollection()->addAction( "close_window");
//     ka->setIcon( KIcon( "window-close" ) );
//     ka->setText( i18n("&Close Window") );
//     ka->setShortcuts( KShortcut( Qt::CTRL+Qt::Key_W ) );
//     connect( ka, SIGNAL( triggered() ), this, SLOT( closeWindow() ) );

    QAction *ka = KNS::standardAction(i18n("Download New Data..."), this, SLOT(slotDownload()), actionCollection(), "get_data");
    ka->setShortcuts( KShortcut( Qt::CTRL+Qt::Key_D ) );
    ka->setWhatsThis(i18n("Downloads new data"));
    ka->setToolTip(ka->whatsThis());
    ka->setStatusTip(ka->whatsThis());

#ifdef HAVE_CFITSIO_H
    ka = actionCollection()->addAction( "open_file");
    ka->setIcon( KIcon( "document-open" ) );
    ka->setText( i18n( "Open FITS...") );
    ka->setShortcuts( KShortcut( Qt::CTRL+Qt::Key_O ) );
    connect( ka, SIGNAL( triggered() ), this, SLOT( slotOpenFITS() ) );
#endif

    ka = actionCollection()->addAction( "export_image" );
    ka->setIcon( KIcon( "document-export-image" ) );
    ka->setText( i18n( "&Save Sky Image..." ) );
    ka->setShortcuts( KShortcut( Qt::CTRL+Qt::Key_I ) );
    connect( ka, SIGNAL( triggered() ), this, SLOT( slotExportImage() ) );

    ka = actionCollection()->addAction( "run_script" );
    ka->setIcon( KIcon( "system-run" ) );
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
    ka->setIcon( KIcon( "view-history" ) );
    ka->setText( i18nc( "set Clock to New Time", "&Set Time..." ) );
    ka->setShortcuts( KShortcut( Qt::CTRL+Qt::Key_S  ) );
    connect( ka, SIGNAL( triggered() ), this, SLOT( slotSetTime() ) );

    KToggleAction *ta = actionCollection()->add<KToggleAction>( "clock_startstop" );
    ta->setIcon( KIcon( "media-playback-pause" ) );
    ta->setText( i18n( "Stop &Clock" ) );
    QObject::connect( ta, SIGNAL( triggered() ), this, SLOT( slotToggleTimer() ) );
    QObject::connect(data()->clock(), SIGNAL(clockStarted()), ta, SLOT(slotToggled(false)) );
    QObject::connect(data()->clock(), SIGNAL(clockStopped()), ta, SLOT(slotToggled(true)) );
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

    ka = actionCollection()->addAction( "track_object" );
    ka->setIcon( KIcon( "object-locked" ) );
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
    ka->setIcon( KIcon( "zoom-fit-best" ) );
    ka->setText( i18n( "&Default Zoom" ) );
    ka->setShortcuts( KShortcut( Qt::CTRL+Qt::Key_Z ) );
    connect( ka, SIGNAL( triggered() ), this, SLOT( slotDefaultZoom() ) );

    ka = actionCollection()->addAction( "zoom_set" );
    ka->setIcon( KIcon( "zoom-original" ) );
    ka->setText( i18n( "&Zoom to Angular Size..." ) );
    ka->setShortcuts( KShortcut( Qt::CTRL+Qt::SHIFT+Qt::Key_Z ) );
    connect( ka, SIGNAL( triggered() ), this, SLOT( slotSetZoom() ) );

    actionCollection()->addAction( KStandardAction::FullScreen, this, SLOT( slotFullScreen() ) );

    ka = actionCollection()->addAction( "coordsys" );
    QString text = i18n("Equatorial &Coordinates");
    if ( Options::useAltAz() ) text = i18n("Horizontal &Coordinates");
    ka->setText( text );
    ka->setShortcuts( KShortcut( "Space" ) );
    connect( ka, SIGNAL( triggered() ), this, SLOT( slotCoordSys() ) );

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
    ta = actionCollection()->add<KToggleAction>( "show_boxes" );
    ta->setText( i18nc( "Show the information boxes", "Show &Info Boxes") );
    ta->setChecked( Options::showInfoBoxes() );
    QObject::connect(ta, SIGNAL( toggled(bool) ), infoBoxes(), SLOT(setVisible(bool)));
    QObject::connect(ta, SIGNAL( toggled(bool) ), this, SLOT(slotShowGUIItem(bool)));

    ta = actionCollection()->add<KToggleAction>( "show_time_box");
    ta->setText( i18nc( "Show time-related info box", "Show &Time Box") );
    QObject::connect(ta, SIGNAL( toggled(bool) ), infoBoxes(), SLOT(showTimeBox(bool)));
    QObject::connect(ta, SIGNAL( toggled(bool) ), this, SLOT(slotShowGUIItem(bool)));

    ta = actionCollection()->add<KToggleAction>( "show_focus_box");
    ta->setText( i18nc( "Show focus-related info box", "Show &Focus Box") );
    QObject::connect(ta, SIGNAL( toggled(bool) ), infoBoxes(), SLOT(showFocusBox(bool)));
    QObject::connect(ta, SIGNAL( toggled(bool) ), this, SLOT(slotShowGUIItem(bool)));

    ta = actionCollection()->add<KToggleAction>( "show_location_box");
    ta->setText( i18nc( "Show location-related info box", "Show &Location Box") );
    QObject::connect(ta, SIGNAL( toggled(bool) ), infoBoxes(), SLOT(showGeoBox(bool)));
    QObject::connect(ta, SIGNAL( toggled(bool) ), this, SLOT(slotShowGUIItem(bool)));

    //Toolbar options
    ta = actionCollection()->add<KToggleAction>( "show_mainToolBar");
    ta->setText( i18n( "Show Main Toolbar" ) );
    QObject::connect(ta, SIGNAL( toggled(bool) ), this, SLOT(slotShowGUIItem(bool)));

    ta = actionCollection()->add<KToggleAction>( "show_viewToolBar");
    ta->setText( i18n( "Show View Toolbar" ) );
    QObject::connect(ta, SIGNAL( toggled(bool) ), this, SLOT(slotShowGUIItem(bool)));

    //Statusbar view options
    ta = actionCollection()->add<KToggleAction>( "show_statusBar");
    ta->setText( i18n( "Show Statusbar" ) );
    QObject::connect(ta, SIGNAL( toggled(bool) ), this, SLOT(slotShowGUIItem(bool)));

    ta = actionCollection()->add<KToggleAction>( "show_sbAzAlt");
    ta->setText( i18n( "Show Az/Alt Field" ) );
    QObject::connect(ta, SIGNAL( toggled(bool) ), this, SLOT(slotShowGUIItem(bool)));

    ta = actionCollection()->add<KToggleAction>( "show_sbRADec");
    ta->setText( i18n( "Show RA/Dec Field" ) );
    QObject::connect(ta, SIGNAL( toggled(bool) ), this, SLOT(slotShowGUIItem(bool)));

    //Color scheme actions.  These are added to the "colorschemes" KActionMenu.
    colorActionMenu = actionCollection()->add<KActionMenu>( "colorschemes" );
    colorActionMenu->setText( i18n( "C&olor Schemes" ) );
    addColorMenuItem( i18n( "&Classic" ), "cs_classic" );
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
    ka->setIcon( KIcon( "world" ) );
    ka->setText( i18nc( "Location on Earth", "&Geographic..." ) );
    ka->setShortcuts( KShortcut( Qt::CTRL+Qt::Key_G  ) );
    connect( ka, SIGNAL( triggered() ), this, SLOT( slotGeoLocator() ) );

    actionCollection()->addAction( KStandardAction::Preferences, "configure", this, SLOT( slotViewOps() ) );

    ka = actionCollection()->addAction( "startwizard" );
    ka->setIcon( KIcon( "tools-wizard" ) );
    ka->setText( i18n( "Startup Wizard..." ) );
    connect( ka, SIGNAL( triggered() ), this, SLOT( slotWizard() ) );

    //Tools Menu:
    ka = actionCollection()->addAction( "astrocalculator" );
    ka->setIcon( KIcon( "accessories-calculator" ) );
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

//FIXME: implement glossary
//     ka = actionCollection()->addAction( "glossary");
//     ka->setText( i18n( "Glossary...") );
//     ka->setShortcuts( KShortcut(Qt::CTRL+Qt::Key_K ) );
//     connect( ka, SIGNAL( triggered() ), this, SLOT( slotGlossary() ) );

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
    ta = actionCollection()->add<KToggleAction>( "show_stars" );
    ta->setIcon( KIcon( "kstars_stars" ) );
    ta->setText( i18nc( "Toggle Stars in the display", "Stars" ) );
    ta->setToolTip( i18n("Toggle stars") );
    connect( ta, SIGNAL( triggered() ), this, SLOT( slotViewToolBar() ) );

    //show_deepsky:
    ta = actionCollection()->add<KToggleAction>( "show_deepsky" );
    ta->setIcon( KIcon( "kstars_deepsky" ) );
    ta->setText( i18nc( "Toggle Deep Sky Objects in the display", "Deep Sky" ) );
    ta->setToolTip( i18n("Toggle deep sky objects") );
    connect( ta, SIGNAL( triggered() ), this, SLOT( slotViewToolBar() ) );

    //show_planets:
    ta = actionCollection()->add<KToggleAction>( "show_planets" );
    ta->setIcon( KIcon( "kstars_planets" ) );
    ta->setText( i18nc( "Toggle Solar System objects in the display", "Solar System" ) );
    ta->setToolTip( i18n("Toggle Solar system objects") );
    connect( ta, SIGNAL( triggered() ), this, SLOT( slotViewToolBar() ) );

    //show_clines:
    ta = actionCollection()->add<KToggleAction>( "show_clines" );
    ta->setIcon( KIcon( "kstars_clines" ) );
    ta->setText( i18nc( "Toggle Constellation Lines in the display", "Const. Lines" ) );
    ta->setToolTip( i18n("Toggle constellation lines") );
    connect( ta, SIGNAL( triggered() ), this, SLOT( slotViewToolBar() ) );

    //show_cnames:
    ta = actionCollection()->add<KToggleAction>( "show_cnames" );
    ta->setIcon( KIcon( "kstars_cnames" ) );
    ta->setText( i18nc( "Toggle Constellation Names in the display", "Const. Names" ) );
    ta->setToolTip( i18n("Toggle constellation names") );
    connect( ta, SIGNAL( triggered() ), this, SLOT( slotViewToolBar() ) );

    //show_cbound:
    ta = actionCollection()->add<KToggleAction>( "show_cbounds" );
    ta->setIcon( KIcon( "kstars_cbound" ) );
    ta->setText( i18nc( "Toggle Constellation Boundaries in the display", "C. Boundaries" ) );
    ta->setToolTip( i18n("Toggle constellation boundaries") );
    connect( ta, SIGNAL( triggered() ), this, SLOT( slotViewToolBar() ) );

    //show_mw:
    ta = actionCollection()->add<KToggleAction>( "show_mw" );
    ta->setIcon( KIcon( "kstars_mw" ) );
    ta->setText( i18nc( "Toggle Milky Way in the display", "Milky Way" ) );
    ta->setToolTip( i18n("Toggle milky way") );
    connect( ta, SIGNAL( triggered() ), this, SLOT( slotViewToolBar() ) );

    //show_grid:
    ta = actionCollection()->add<KToggleAction>( "show_grid" );
    ta->setIcon( KIcon( "kstars_grid" ) );
    ta->setText( i18nc( "Toggle Coordinate Grid in the display", "Coord. grid" ) );
    ta->setToolTip( i18n("Toggle coordinate grid") );
    connect( ta, SIGNAL( triggered() ), this, SLOT( slotViewToolBar() ) );

    //show_horizon:
    ta = actionCollection()->add<KToggleAction>( "show_horizon" );
    ta->setIcon( KIcon( "kstars_horizon" ) );
    ta->setText( i18nc( "Toggle the opaque fill of the ground polygon in the display", "Ground" ) );
    ta->setToolTip( i18n("Toggle opaque ground") );
    connect( ta, SIGNAL( triggered() ), this, SLOT( slotViewToolBar() ) );

    setXMLFile( "kstarsui.rc" );

    if (Options::fitsSaveDirectory().isEmpty())
        Options::setFitsSaveDirectory(QDir:: homePath());
}

void KStars::initFOV() {
    //Read in the user's fov.dat and populate the FOV menu with its symbols.  
    //If no fov.dat exists, create a default version.
    QFile f;
    QStringList fields;
    QString nm;

    f.setFileName( KStandardDirs::locateLocal( "appdata", "fov.dat" ) );

    //if file s empty, let's start over
    if ( (uint)f.size() == 0 ) f.remove();

    if ( ! f.exists() ) {
        if ( ! f.open( QIODevice::WriteOnly ) ) {
            kDebug() << i18n( "Could not open fov.dat." );
        } else {
            QTextStream ostream(&f);
            ostream << i18nc( "Do not use a field-of-view indicator", "No FOV" ) <<  ":0.0:0:#AAAAAA" << endl;
            ostream << i18nc( "use field-of-view for binoculars", "7x35 Binoculars" ) << ":558:1:#AAAAAA" << endl;
            ostream << i18nc( "use a Telrad field-of-view indicator", "Telrad" ) << ":30:3:#AA0000" << endl;
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

                kta->setObjectName( nm.toUtf8() );
                kta->setActionGroup( fovGroup );
                if ( nm == Options::fOVName() ) kta->setChecked( true );
                fovActionMenu->addAction( kta );
            }
        }
    } else {
        kDebug() << i18n( "Could not open file: %1", f.fileName() );
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
        qApp->quit();
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

    //Do not start the clock if "--paused" specified on the cmd line
    if ( StartClockRunning )
        data()->clock()->start();

    // Connect cache function for Find dialog
    connect( data(), SIGNAL( clearCache() ), this,
             SLOT( clearCachedFindDialog() ) );

    //Propagate config settings
    applyConfig( false );

    //show the window.  must be before kswizard and messageboxes
    show();

    //Initialize focus
    initFocus();

    data()->setFullTimeUpdate();
    updateTime();

    //If this is the first startup, show the wizard
    if ( Options::runStartupWizard() ) {
        slotWizard();
    }

    //Show TotD
    KTipDialog::showTip(this, "kstars/tips");

    //DEBUG
    kDebug() << "The current Date/Time is: " << KStarsDateTime::currentDateTime().toString();
}

void KStars::initFocus() {
    //Case 1: tracking on an object
    if ( Options::isTracking() && Options::focusObject() != i18n("nothing") ) {
        SkyObject *oFocus;
        if ( Options::focusObject() == i18n("star") ) {
            SkyPoint p( Options::focusRA(), Options::focusDec() );
            double maxrad = 1.0;

            oFocus = data()->skyComposite()->starNearest( &p, maxrad );
        } else {
            oFocus = data()->objectNamed( Options::focusObject() );
        }

        if ( oFocus ) {
            map()->setFocusObject( oFocus );
            map()->setClickedObject( oFocus );
            map()->setFocusPoint( oFocus );
        } else {
            kWarning() << "Cannot center on " 
                       << Options::focusObject() 
                       << ": no object found." << endl;
        }

    //Case 2: not tracking, and using Alt/Az coords.  Set focus point using
    //FocusRA as the Azimuth, and FocusDec as the Altitude
    } else if ( ! Options::isTracking() && Options::useAltAz() ) {
        SkyPoint pFocus;
        pFocus.setAz( Options::focusRA() );
        pFocus.setAlt( Options::focusDec() );
        pFocus.HorizontalToEquatorial( LST(), geo()->lat() );
        map()->setFocusPoint( &pFocus );

    //Default: set focus point using FocusRA as the RA and 
    //FocusDec as the Dec
    } else {
        SkyPoint pFocus( Options::focusRA(), Options::focusDec() );
        pFocus.EquatorialToHorizontal( LST(), geo()->lat() );
        map()->setFocusPoint( &pFocus );
    }
    data()->setSnapNextFocus();
    map()->setDestination( map()->focusPoint() );
    map()->setFocus( map()->destination() );

    map()->showFocusCoords();

    map()->setOldFocus( map()->focus() );
    map()->oldfocus()->setAz( map()->focus()->az()->Degrees() );
    map()->oldfocus()->setAlt( map()->focus()->alt()->Degrees() );

    //Check whether initial position is below the horizon.
    if ( Options::useAltAz() && Options::showHorizon() && Options::showGround() &&
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
}

void KStars::buildGUI() {
    //create the skymap
    skymap = SkyMap::Create( data(), this );
    setCentralWidget( skymap );

    //Initialize menus, toolbars, and statusbars
    initStatusBar();
    initActions();

#ifdef Q_WS_WIN
    createGUI("kstarsui-win.rc");
#else
    createGUI("kstarsui.rc");
#endif
    StandardWindowOptions opt = Default;
    opt &= ~Create;
    setupGUI(opt); // setupGUI needs to be called after createGUI if no Create flag is passed to
                   // setupGUI. Once you have called to createGUI you don't want for it to be called
                   // again. (ereslibre)

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
