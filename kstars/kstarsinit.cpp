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

#include "kstars.h"

#include "fov.h"
#include "kspaths.h"
#include "kstarsdata.h"
#include "kstars_debug.h"
#include "Options.h"
#include "skymap.h"
#include "texturemanager.h"
#include "projections/projector.h"
#include "skycomponents/skymapcomposite.h"
#include "skyobjects/ksplanetbase.h"
#include "widgets/timespinbox.h"
#include "widgets/timestepbox.h"
#include "hips/hipsmanager.h"
#include "auxiliary/thememanager.h"

#ifdef HAVE_INDI
#include "indi/drivermanager.h"
#include "indi/guimanager.h"
#include "ekos/manager.h"
#endif

#include <KActionCollection>
#include <KActionMenu>
#include <KTipDialog>
#include <KToggleAction>
#include <KToolBar>

#include <QMenu>
#include <QStatusBar>

//This file contains functions that kstars calls at startup (except constructors).
//These functions are declared in kstars.h

namespace
{
// A lot of QAction is defined there. In order to decrease amount
// of boilerplate code a trick with << operator overloading is used.
// This makes code more concise and readable.
//
// When data type could not used directly. Either because of
// overloading rules or because one data type have different
// semantics its wrapped into struct.
//
// Downside is unfamiliar syntax and really unhelpful error
// messages due to general abuse of << overloading

// Set QAction text
QAction *operator<<(QAction *ka, QString text)
{
    ka->setText(text);
    return ka;
}
// Set icon for QAction
QAction *operator<<(QAction *ka, const QIcon &icon)
{
    ka->setIcon(icon);
    return ka;
}
// Set keyboard shortcut
QAction *operator<<(QAction *ka, const QKeySequence sh)
{
    KStars::Instance()->actionCollection()->setDefaultShortcut(ka, sh);
    //ka->setShortcut(sh);
    return ka;
}

// Add action to group. AddToGroup struct acts as newtype wrapper
// in order to allow overloading.
struct AddToGroup
{
    QActionGroup *grp;
    AddToGroup(QActionGroup *g) : grp(g) {}
};
QAction *operator<<(QAction *ka, AddToGroup g)
{
    g.grp->addAction(ka);
    return ka;
}

// Set checked property. Checked is newtype wrapper.
struct Checked
{
    bool flag;
    Checked(bool f) : flag(f) {}
};
QAction *operator<<(QAction *ka, Checked chk)
{
    ka->setCheckable(true);
    ka->setChecked(chk.flag);
    return ka;
}

// Set tool tip. ToolTip is used as newtype wrapper.
struct ToolTip
{
    QString tip;
    ToolTip(QString msg) : tip(msg) {}
};
QAction *operator<<(QAction *ka, const ToolTip &tool)
{
    ka->setToolTip(tool.tip);
    return ka;
}

// Create new KToggleAction and connect slot to toggled(bool) signal
QAction *newToggleAction(KActionCollection *col, QString name, QString text, QObject *receiver, const char *member)
{
    QAction *ka = col->add<KToggleAction>(name) << text;
    QObject::connect(ka, SIGNAL(toggled(bool)), receiver, member);
    return ka;
}
}

// Resource file override - used by UI tests
QString KStars::m_KStarsUIResource = "kstarsui.rc";
bool KStars::setResourceFile(QString const rc)
{
    if (QFile(rc).exists())
    {
        m_KStarsUIResource = rc;
        return true;
    }
    else return false;
}


void KStars::initActions()
{
    // Check if we have this specific Breeze icon. If not, try to set the theme search path and if appropriate, the icon theme rcc file
    // in each OS
    if (!QIcon::hasThemeIcon(QLatin1String("kstars_flag")))
        KSTheme::Manager::instance()->setIconTheme(KSTheme::Manager::BREEZE_DARK_THEME);

    QAction *ka;

    // ==== File menu ================
    ka = new QAction(QIcon::fromTheme("favorites"), i18n("Download New Data..."), this);
    connect(ka, &QAction::triggered, this, &KStars::slotDownload);
    ka->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_N));
    ka->setWhatsThis(i18n("Downloads new data"));
    ka->setToolTip(ka->whatsThis());
    ka->setStatusTip(ka->whatsThis());
    actionCollection()->addAction(QStringLiteral("get_data"), ka);

#ifdef HAVE_CFITSIO
    actionCollection()->addAction("open_file", this, SLOT(slotOpenFITS()))
            << i18n("Open Image...") << QIcon::fromTheme("document-open")
            << QKeySequence(Qt::CTRL + Qt::Key_O);
#endif
    actionCollection()->addAction("export_image", this, SLOT(slotExportImage()))
            << i18n("&Save Sky Image...")
            << QIcon::fromTheme("document-export-image");

    // 2017-09-17 Jasem: FIXME! Scripting does not work properly under non UNIX systems.
    // It must be updated to use DBus session bus from Qt (like scheduler)
#ifndef Q_OS_WIN
    actionCollection()->addAction("run_script", this, SLOT(slotRunScript()))
            << i18n("&Run Script...") << QIcon::fromTheme("system-run")
            << QKeySequence(Qt::CTRL + Qt::Key_R);
#endif
    actionCollection()->addAction("printing_wizard", this, SLOT(slotPrintingWizard()))
            << i18nc("start Printing Wizard", "Printing &Wizard");
    ka = actionCollection()->addAction(KStandardAction::Print, "print", this, SLOT(slotPrint()));
    ka->setIcon(QIcon::fromTheme("document-print"));
    //actionCollection()->addAction( KStandardAction::Quit,  "quit",  this, SLOT(close) );
    ka = actionCollection()->addAction(KStandardAction::Quit, "quit", qApp, SLOT(closeAllWindows()));
    ka->setIcon(QIcon::fromTheme("application-exit"));

    // ==== Time Menu ================
    actionCollection()->addAction("time_to_now", this, SLOT(slotSetTimeToNow()))
            << i18n("Set Time to &Now") << QKeySequence(Qt::CTRL + Qt::Key_E)
            << QIcon::fromTheme("clock");

    actionCollection()->addAction("time_dialog", this, SLOT(slotSetTime()))
            << i18nc("set Clock to New Time", "&Set Time...") << QKeySequence(Qt::CTRL + Qt::Key_S)
            << QIcon::fromTheme("clock");

    ka = actionCollection()->add<KToggleAction>("clock_startstop")
         << i18n("Stop &Clock")
         << QIcon::fromTheme("media-playback-pause");
    if (!StartClockRunning)
        ka->toggle();
    QObject::connect(ka, SIGNAL(triggered()), this, SLOT(slotToggleTimer()));

    // If we are started in --paused state make sure the icon reflects that now
    if (StartClockRunning == false)
    {
        QAction *a = actionCollection()->action("clock_startstop");
        if (a)
            a->setIcon(QIcon::fromTheme("run-build-install-root"));
    }

    QObject::connect(data()->clock(), &SimClock::clockToggled, [ = ](bool toggled)
    {
        QAction *a = actionCollection()->action("clock_startstop");
        if (a)
        {
            a->setChecked(toggled);
            // Many users forget to unpause KStars, so we are using run-build-install-root icon which is red
            // and stands out from the rest of the icons so users are aware when KStars is paused visually
            a->setIcon(toggled ? QIcon::fromTheme("run-build-install-root") : QIcon::fromTheme("media-playback-pause"));
            a->setToolTip(toggled ? i18n("Resume Clock") : i18n("Stop Clock"));
        }
    });
    //UpdateTime() if clock is stopped (so hidden objects get drawn)
    QObject::connect(data()->clock(), SIGNAL(clockToggled(bool)), this, SLOT(updateTime()));
    actionCollection()->addAction("time_step_forward", this, SLOT(slotStepForward()))
            << i18n("Advance one step forward in time")
            << QIcon::fromTheme("media-skip-forward")
            << QKeySequence(Qt::Key_Greater);
    actionCollection()->addAction("time_step_backward", this, SLOT(slotStepBackward()))
            << i18n("Advance one step backward in time")
            << QIcon::fromTheme("media-skip-backward")
            << QKeySequence(Qt::Key_Less);

    // ==== Pointing Menu ================
    actionCollection()->addAction("zenith", this, SLOT(slotPointFocus())) << i18n("&Zenith") << QKeySequence("Z");
    actionCollection()->addAction("north", this, SLOT(slotPointFocus())) << i18n("&North") << QKeySequence("N");
    actionCollection()->addAction("east", this, SLOT(slotPointFocus())) << i18n("&East") << QKeySequence("E");
    actionCollection()->addAction("south", this, SLOT(slotPointFocus())) << i18n("&South") << QKeySequence("S");
    actionCollection()->addAction("west", this, SLOT(slotPointFocus())) << i18n("&West") << QKeySequence("W");

    actionCollection()->addAction("find_object", this, SLOT(slotFind()))
            << i18n("&Find Object...") << QIcon::fromTheme("edit-find")
            << QKeySequence(Qt::CTRL + Qt::Key_F);
    actionCollection()->addAction("track_object", this, SLOT(slotTrack()))
            << i18n("Engage &Tracking")
            << QIcon::fromTheme("object-locked")
            << QKeySequence(Qt::CTRL + Qt::Key_T);
    actionCollection()->addAction("manual_focus", this, SLOT(slotManualFocus()))
            << i18n("Set Coordinates &Manually...") << QKeySequence(Qt::CTRL + Qt::Key_M);

    QAction *action;

    // ==== View Menu ================
    action = actionCollection()->addAction(KStandardAction::ZoomIn, "zoom_in", map(), SLOT(slotZoomIn()));
    action->setIcon(QIcon::fromTheme("zoom-in"));

    action = actionCollection()->addAction(KStandardAction::ZoomOut, "zoom_out", map(), SLOT(slotZoomOut()));
    action->setIcon(QIcon::fromTheme("zoom-out"));

    actionCollection()->addAction("zoom_default", map(), SLOT(slotZoomDefault()))
            << i18n("&Default Zoom") << QIcon::fromTheme("zoom-fit-best")
            << QKeySequence(Qt::CTRL + Qt::Key_Z);
    actionCollection()->addAction("zoom_set", this, SLOT(slotSetZoom()))
            << i18n("&Zoom to Angular Size...")
            << QIcon::fromTheme("zoom-original")
            << QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_Z);

    action = actionCollection()->addAction(KStandardAction::FullScreen, this, SLOT(slotFullScreen()));
    action->setIcon(QIcon::fromTheme("view-fullscreen"));

    actionCollection()->addAction("coordsys", this, SLOT(slotCoordSys()))
            << (Options::useAltAz() ? i18n("Switch to star globe view (Equatorial &Coordinates)") :
                i18n("Switch to horizonal view (Horizontal &Coordinates)"))
            << QKeySequence("Space");

    actionCollection()->addAction("toggle_terrain", this, SLOT(slotTerrain()))
            << (Options::showTerrain() ? i18n("Stop showing terrain") :
                i18n("Show terrain"))
            << QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_T);

    actionCollection()->addAction("project_lambert", this, SLOT(slotMapProjection()))
            << i18n("&Lambert Azimuthal Equal-area") << QKeySequence("F5") << AddToGroup(projectionGroup)
            << Checked(Options::projection() == Projector::Lambert);
    actionCollection()->addAction("project_azequidistant", this, SLOT(slotMapProjection()))
            << i18n("&Azimuthal Equidistant") << QKeySequence("F6") << AddToGroup(projectionGroup)
            << Checked(Options::projection() == Projector::AzimuthalEquidistant);
    actionCollection()->addAction("project_orthographic", this, SLOT(slotMapProjection()))
            << i18n("&Orthographic") << QKeySequence("F7") << AddToGroup(projectionGroup)
            << Checked(Options::projection() == Projector::Orthographic);
    actionCollection()->addAction("project_equirectangular", this, SLOT(slotMapProjection()))
            << i18n("&Equirectangular") << QKeySequence("F8") << AddToGroup(projectionGroup)
            << Checked(Options::projection() == Projector::Equirectangular);
    actionCollection()->addAction("project_stereographic", this, SLOT(slotMapProjection()))
            << i18n("&Stereographic") << QKeySequence("F9") << AddToGroup(projectionGroup)
            << Checked(Options::projection() == Projector::Stereographic);
    actionCollection()->addAction("project_gnomonic", this, SLOT(slotMapProjection()))
            << i18n("&Gnomonic") << QKeySequence("F10") << AddToGroup(projectionGroup)
            << Checked(Options::projection() == Projector::Gnomonic);

    //Settings Menu:
    //Info Boxes option actions
    QAction *kaBoxes = actionCollection()->add<KToggleAction>("show_boxes")
                       << i18nc("Show the information boxes", "Show &Info Boxes") << Checked(Options::showInfoBoxes());
    connect(kaBoxes, SIGNAL(toggled(bool)), map(), SLOT(slotToggleInfoboxes(bool)));
    kaBoxes->setChecked(Options::showInfoBoxes());

    ka = actionCollection()->add<KToggleAction>("show_time_box")
         << i18nc("Show time-related info box", "Show &Time Box");
    connect(kaBoxes, SIGNAL(toggled(bool)), ka, SLOT(setEnabled(bool)));
    connect(ka, SIGNAL(toggled(bool)), map(), SLOT(slotToggleTimeBox(bool)));
    ka->setChecked(Options::showTimeBox());
    ka->setEnabled(Options::showInfoBoxes());

    ka = actionCollection()->add<KToggleAction>("show_focus_box")
         << i18nc("Show focus-related info box", "Show &Focus Box");
    connect(kaBoxes, SIGNAL(toggled(bool)), ka, SLOT(setEnabled(bool)));
    connect(ka, SIGNAL(toggled(bool)), map(), SLOT(slotToggleFocusBox(bool)));
    ka->setChecked(Options::showFocusBox());
    ka->setEnabled(Options::showInfoBoxes());

    ka = actionCollection()->add<KToggleAction>("show_location_box")
         << i18nc("Show location-related info box", "Show &Location Box");
    connect(kaBoxes, SIGNAL(toggled(bool)), ka, SLOT(setEnabled(bool)));
    connect(ka, SIGNAL(toggled(bool)), map(), SLOT(slotToggleGeoBox(bool)));
    ka->setChecked(Options::showGeoBox());
    ka->setEnabled(Options::showInfoBoxes());

    //Toolbar options
    newToggleAction(actionCollection(), "show_mainToolBar", i18n("Show Main Toolbar"), toolBar("kstarsToolBar"),
                    SLOT(setVisible(bool)));
    newToggleAction(actionCollection(), "show_viewToolBar", i18n("Show View Toolbar"), toolBar("viewToolBar"),
                    SLOT(setVisible(bool)));

    //Statusbar view options
    newToggleAction(actionCollection(), "show_statusBar", i18n("Show Statusbar"), this, SLOT(slotShowGUIItem(bool)));
    newToggleAction(actionCollection(), "show_sbAzAlt", i18n("Show Az/Alt Field"), this, SLOT(slotShowGUIItem(bool)));
    newToggleAction(actionCollection(), "show_sbRADec", i18n("Show RA/Dec Field"), this, SLOT(slotShowGUIItem(bool)));
    newToggleAction(actionCollection(), "show_sbJ2000RADec", i18n("Show J2000.0 RA/Dec Field"), this,
                    SLOT(slotShowGUIItem(bool)));


    populateThemes();

    //Color scheme actions.  These are added to the "colorschemes" KActionMenu.
    colorActionMenu = actionCollection()->add<KActionMenu>("colorschemes");
    colorActionMenu->setText(i18n("C&olor Schemes"));
    addColorMenuItem(i18n("&Classic"), "cs_classic");
    addColorMenuItem(i18n("&Star Chart"), "cs_chart");
    addColorMenuItem(i18n("&Night Vision"), "cs_night");
    addColorMenuItem(i18n("&Moonless Night"), "cs_moonless-night");

    //Add any user-defined color schemes:
    QFile file(KSPaths::locate(QStandardPaths::GenericDataLocation,
                               "colors.dat")); //determine filename in local user KDE directory tree.
    if (file.exists() && file.open(QIODevice::ReadOnly))
    {
        QTextStream stream(&file);
        while (!stream.atEnd())
        {
            QString line       = stream.readLine();
            QString schemeName = line.left(line.indexOf(':'));
            QString actionname = "cs_" + line.mid(line.indexOf(':') + 1, line.indexOf('.') - line.indexOf(':') - 1);
            addColorMenuItem(i18n(schemeName.toLocal8Bit()), actionname.toLocal8Bit());
        }
        file.close();
    }

    //Add FOV Symbol actions
    fovActionMenu = actionCollection()->add<KActionMenu>("fovsymbols");
    fovActionMenu->setText(i18n("&FOV Symbols"));
    fovActionMenu->setDelayed(false);
    fovActionMenu->setIcon(QIcon::fromTheme("crosshairs"));
    FOVManager::readFOVs();
    repopulateFOV();

    //Add HIPS Sources actions
    hipsActionMenu = actionCollection()->add<KActionMenu>("hipssources");
    hipsActionMenu->setText(i18n("HiPS All Sky Overlay"));
    hipsActionMenu->setDelayed(false);
    hipsActionMenu->setIcon(QIcon::fromTheme("view-preview"));
    HIPSManager::Instance()->readSources();
    repopulateHIPS();

    actionCollection()->addAction("geolocation", this, SLOT(slotGeoLocator()))
            << i18nc("Location on Earth", "&Geographic...")
            << QIcon::fromTheme("kstars_xplanet")
            << QKeySequence(Qt::CTRL + Qt::Key_G);

    // Configure Notifications
#ifdef HAVE_NOTIFYCONFIG
    KStandardAction::configureNotifications(this, SLOT(slotConfigureNotifications()), actionCollection());
#endif

    // Prepare the options dialog early for modules to connect signals
    prepareOps();

    ka = actionCollection()->addAction(KStandardAction::Preferences, "configure", this, SLOT(slotViewOps()));
    //I am not sure what icon preferences is supposed to be.
    //ka->setIcon( QIcon::fromTheme(""));

    actionCollection()->addAction("startwizard", this, SLOT(slotWizard()))
            << i18n("Startup Wizard...")
            << QIcon::fromTheme("tools-wizard");

    // Manual data entry
    actionCollection()->addAction("manual_add_dso", this, SLOT(slotAddDeepSkyObject()))
            << i18n("Manually add a deep-sky object");

    // Updates actions
    actionCollection()->addAction("update_comets", this, SLOT(slotUpdateComets()))
            << i18n("Update comets orbital elements");
    actionCollection()->addAction("update_asteroids", this, SLOT(slotUpdateAsteroids()))
            << i18n("Update asteroids orbital elements");
    actionCollection()->addAction("update_supernovae", this, SLOT(slotUpdateSupernovae()))
            << i18n("Update Recent Supernovae data");
    actionCollection()->addAction("update_satellites", this, SLOT(slotUpdateSatellites()))
            << i18n("Update satellites orbital elements");

    //Tools Menu:
    actionCollection()->addAction("astrocalculator", this, SLOT(slotCalculator()))
            << i18n("Calculator")
            << QIcon::fromTheme("accessories-calculator")
            << QKeySequence(Qt::SHIFT + Qt::CTRL + Qt::Key_C);

    /* FIXME Enable once port to KF5 is complete for moonphasetool
     actionCollection()->addAction("moonphasetool", this, SLOT(slotMoonPhaseTool()) )
         << i18n("Moon Phase Calendar");
    */

    actionCollection()->addAction("obslist", this, SLOT(slotObsList()))
            << i18n("Observation Planner") << QKeySequence(Qt::CTRL + Qt::Key_L);

    actionCollection()->addAction("altitude_vs_time", this, SLOT(slotAVT()))
            << i18n("Altitude vs. Time") << QKeySequence(Qt::CTRL + Qt::Key_A);

    actionCollection()->addAction("whats_up_tonight", this, SLOT(slotWUT()))
            << i18n("What's up Tonight") << QKeySequence(Qt::CTRL + Qt::Key_U);

    //FIXME Port to QML2
    //#if 0
    actionCollection()->addAction("whats_interesting", this, SLOT(slotToggleWIView()))
            << i18n("What's Interesting...") << QKeySequence(Qt::CTRL + Qt::Key_W);
    //#endif

    actionCollection()->addAction("XPlanet", map(), SLOT(slotStartXplanetViewer()))
            << i18n("XPlanet Solar System Simulator") << QKeySequence(Qt::CTRL + Qt::Key_X);

    actionCollection()->addAction("skycalendar", this, SLOT(slotCalendar())) << i18n("Sky Calendar");

#ifdef HAVE_INDI
    ka = actionCollection()->addAction("ekos", this, SLOT(slotEkos()))
         << i18n("Ekos") << QKeySequence(Qt::CTRL + Qt::Key_K);
    ka->setShortcutContext(Qt::ApplicationShortcut);
#endif

    //FIXME: implement glossary
    //     ka = actionCollection()->addAction("glossary");
    //     ka->setText( i18n("Glossary...") );
    //     ka->setShortcuts( QKeySequence(Qt::CTRL+Qt::Key_K ) );
    //     connect( ka, SIGNAL(triggered()), this, SLOT(slotGlossary()) );

    // 2017-09-17 Jasem: FIXME! Scripting does not work properly under non UNIX systems.
    // It must be updated to use DBus session bus from Qt (like scheduler)
#ifndef Q_OS_WIN
    actionCollection()->addAction("scriptbuilder", this, SLOT(slotScriptBuilder()))
            << i18n("Script Builder") << QKeySequence(Qt::CTRL + Qt::Key_B);
#endif

    actionCollection()->addAction("solarsystem", this, SLOT(slotSolarSystem()))
            << i18n("Solar System") << QKeySequence(Qt::CTRL + Qt::Key_Y);

    // Disabled until fixed later
    actionCollection()->addAction("jmoontool", this, SLOT(slotJMoonTool()) )
            << i18n("Jupiter's Moons")
            << QKeySequence(Qt::CTRL + Qt::Key_J );

    actionCollection()->addAction("flagmanager", this, SLOT(slotFlagManager())) << i18n("Flags");

    actionCollection()->addAction("equipmentwriter", this, SLOT(slotEquipmentWriter()))
            << i18n("List your &Equipment...") << QIcon::fromTheme("kstars") << QKeySequence(Qt::CTRL + Qt::Key_0);
    actionCollection()->addAction("manageobserver", this, SLOT(slotObserverManager()))
            << i18n("Manage Observer...") << QIcon::fromTheme("im-user") << QKeySequence(Qt::CTRL + Qt::Key_1);

    //TODO only enable it when finished
    actionCollection()->addAction("artificialhorizon", this, SLOT(slotHorizonManager()))
            << i18n("Artificial Horizon...");

    // ==== observation menu - execute ================
    actionCollection()->addAction("execute", this, SLOT(slotExecute()))
            << i18n("Execute the session Plan...") << QKeySequence(Qt::CTRL + Qt::Key_2);

    // ==== observation menu - polaris hour angle ================
    actionCollection()->addAction("polaris_hour_angle", this, SLOT(slotPolarisHourAngle()))
            << i18n("Polaris Hour Angle...");

    // ==== devices Menu ================
#ifdef HAVE_INDI
#ifndef Q_OS_WIN
#if 0
    actionCollection()->addAction("telescope_wizard", this, SLOT(slotTelescopeWizard()))
            << i18n("Telescope Wizard...")
            << QIcon::fromTheme("tools-wizard");
#endif
#endif
    actionCollection()->addAction("device_manager", this, SLOT(slotINDIDriver()))
            << i18n("Device Manager...")
            << QIcon::fromTheme("network-server")
            << QKeySequence(Qt::SHIFT + Qt::META + Qt::Key_D);
    actionCollection()->addAction("custom_drivers", DriverManager::Instance(), SLOT(showCustomDrivers()))
            << i18n("Custom Drivers...")
            << QIcon::fromTheme("address-book-new");
    ka = actionCollection()->addAction("indi_cpl", this, SLOT(slotINDIPanel()))
         << i18n("INDI Control Panel...")
         << QKeySequence(Qt::CTRL + Qt::Key_I);
    ka->setShortcutContext(Qt::ApplicationShortcut);
    ka->setEnabled(false);
#else
    //FIXME need to disable/hide devices submenu in the tools menu. It is created from the kstarsui.rc file
    //but I don't know how to hide/disable it yet. menuBar()->findChildren<QMenu *>() does not return any children that I can
    //iterate over. Anyway to resolve this?
#endif

    //Help Menu:
    ka = actionCollection()->addAction(KStandardAction::TipofDay, "help_tipofday", this, SLOT(slotTipOfDay()));
    ka->setWhatsThis(i18n("Displays the Tip of the Day"));
    ka->setIcon(QIcon::fromTheme("help-hint"));
    //	KStandardAction::help(this, SLOT(appHelpActivated()), actionCollection(), "help_contents" );

    //Add timestep widget for toolbar
    m_TimeStepBox = new TimeStepBox(toolBar("kstarsToolBar"));
    // Add a tool tip to TimeStep describing the weird nature of time steps
    QString TSBToolTip = i18nc("Tooltip describing the nature of the time step control",
                               "Use this to set the rate at which time in the simulation flows.\nFor time step \'X\' "
                               "up to 10 minutes, time passes at the rate of \'X\' per second.\nFor time steps larger "
                               "than 10 minutes, frames are displayed at an interval of \'X\'.");
    m_TimeStepBox->setToolTip(TSBToolTip);
    m_TimeStepBox->tsbox()->setToolTip(TSBToolTip);
    QWidgetAction *wa = new QWidgetAction(this);
    wa->setDefaultWidget(m_TimeStepBox);
    actionCollection()->addAction("timestep_control", wa) << i18n("Time step control");

    // ==== viewToolBar actions ================
    actionCollection()->add<KToggleAction>("show_stars", this, SLOT(slotViewToolBar()))
            << i18nc("Toggle Stars in the display", "Stars")
            << QIcon::fromTheme("kstars_stars")
            << ToolTip(i18n("Toggle stars"));
    actionCollection()->add<KToggleAction>("show_deepsky", this, SLOT(slotViewToolBar()))
            << i18nc("Toggle Deep Sky Objects in the display", "Deep Sky")
            << QIcon::fromTheme("kstars_deepsky")
            << ToolTip(i18n("Toggle deep sky objects"));
    actionCollection()->add<KToggleAction>("show_planets", this, SLOT(slotViewToolBar()))
            << i18nc("Toggle Solar System objects in the display", "Solar System")
            << QIcon::fromTheme("kstars_planets")
            << ToolTip(i18n("Toggle Solar system objects"));
    actionCollection()->add<KToggleAction>("show_clines", this, SLOT(slotViewToolBar()))
            << i18nc("Toggle Constellation Lines in the display", "Const. Lines")
            << QIcon::fromTheme("kstars_clines")
            << ToolTip(i18n("Toggle constellation lines"));
    actionCollection()->add<KToggleAction>("show_cnames", this, SLOT(slotViewToolBar()))
            << i18nc("Toggle Constellation Names in the display", "Const. Names")
            << QIcon::fromTheme("kstars_cnames")
            << ToolTip(i18n("Toggle constellation names"));
    actionCollection()->add<KToggleAction>("show_cbounds", this, SLOT(slotViewToolBar()))
            << i18nc("Toggle Constellation Boundaries in the display", "C. Boundaries")
            << QIcon::fromTheme("kstars_cbound")
            << ToolTip(i18n("Toggle constellation boundaries"));
    actionCollection()->add<KToggleAction>("show_constellationart", this, SLOT(slotViewToolBar()))
            << xi18nc("Toggle Constellation Art in the display", "C. Art (BETA)")
            << QIcon::fromTheme("kstars_constellationart")
            << ToolTip(xi18n("Toggle constellation art (BETA)"));
    actionCollection()->add<KToggleAction>("show_mw", this, SLOT(slotViewToolBar()))
            << i18nc("Toggle Milky Way in the display", "Milky Way")
            << QIcon::fromTheme("kstars_mw")
            << ToolTip(i18n("Toggle milky way"));
    actionCollection()->add<KToggleAction>("show_equatorial_grid", this, SLOT(slotViewToolBar()))
            << i18nc("Toggle Equatorial Coordinate Grid in the display", "Equatorial coord. grid")
            << QIcon::fromTheme("kstars_grid")
            << ToolTip(i18n("Toggle equatorial coordinate grid"));
    actionCollection()->add<KToggleAction>("show_horizontal_grid", this, SLOT(slotViewToolBar()))
            << i18nc("Toggle Horizontal Coordinate Grid in the display", "Horizontal coord. grid")
            << QIcon::fromTheme("kstars_hgrid")
            << ToolTip(i18n("Toggle horizontal coordinate grid"));
    actionCollection()->add<KToggleAction>("show_horizon", this, SLOT(slotViewToolBar()))
            << i18nc("Toggle the opaque fill of the ground polygon in the display", "Ground")
            << QIcon::fromTheme("kstars_horizon")
            << ToolTip(i18n("Toggle opaque ground"));
    actionCollection()->add<KToggleAction>("show_flags", this, SLOT(slotViewToolBar()))
            << i18nc("Toggle flags in the display", "Flags")
            << QIcon::fromTheme("kstars_flag")
            << ToolTip(i18n("Toggle flags"));
    actionCollection()->add<KToggleAction>("show_satellites", this, SLOT(slotViewToolBar()))
            << i18nc("Toggle satellites in the display", "Satellites")
            << QIcon::fromTheme("kstars_satellites")
            << ToolTip(i18n("Toggle satellites"));
    actionCollection()->add<KToggleAction>("show_supernovae", this, SLOT(slotViewToolBar()))
            << i18nc("Toggle supernovae in the display", "Supernovae")
            << QIcon::fromTheme("kstars_supernovae")
            << ToolTip(i18n("Toggle supernovae"));
    actionCollection()->add<KToggleAction>("show_whatsinteresting", this, SLOT(slotToggleWIView()))
            << i18nc("Toggle What's Interesting", "What's Interesting")
            << QIcon::fromTheme("view-list-details")
            << ToolTip(i18n("Toggle What's Interesting"));

#ifdef HAVE_INDI
    // ==== INDIToolBar actions ================
    actionCollection()->add<KToggleAction>("show_ekos", this, SLOT(slotINDIToolBar()))
            << i18nc("Toggle Ekos in the display", "Ekos")
            << QIcon::fromTheme("kstars_ekos")
            << ToolTip(i18n("Toggle Ekos"));
    ka = actionCollection()->add<KToggleAction>("show_control_panel", this, SLOT(slotINDIToolBar()))
         << i18nc("Toggle the INDI Control Panel in the display", "INDI Control Panel")
         << QIcon::fromTheme("kstars_indi")
         << ToolTip(i18n("Toggle INDI Control Panel"));
    ka->setEnabled(false);
    ka = actionCollection()->add<KToggleAction>("show_fits_viewer", this, SLOT(slotINDIToolBar()))
         << i18nc("Toggle the FITS Viewer in the display", "FITS Viewer")
         << QIcon::fromTheme("kstars_fitsviewer")
         << ToolTip(i18n("Toggle FITS Viewer"));
    ka->setEnabled(false);

    ka = actionCollection()->add<KToggleAction>("show_sensor_fov", this, SLOT(slotINDIToolBar()))
         << i18nc("Toggle the sensor Field of View", "Sensor FOV")
         << QIcon::fromTheme("archive-extract")
         << ToolTip(i18n("Toggle Sensor FOV"));
    ka->setEnabled(false);
    ka->setChecked(Options::showSensorFOV());

    ka = actionCollection()->add<KToggleAction>("show_mount_box", this, SLOT(slotINDIToolBar()))
         << i18nc("Toggle the Mount Control Panel", "Mount Control")
         << QIcon::fromTheme("draw-text")
         << ToolTip(i18n("Toggle Mount Control Panel"));
    telescopeGroup->addAction(ka);

    ka = actionCollection()->add<KToggleAction>("lock_telescope", this, SLOT(slotINDIToolBar()))
         << i18nc("Toggle the telescope center lock in display", "Center Telescope")
         << QIcon::fromTheme("center_telescope", QIcon(":/icons/center_telescope.svg"))
         << ToolTip(i18n("Toggle Lock Telescope Center"));
    telescopeGroup->addAction(ka);

    ka = actionCollection()->add<KToggleAction>("telescope_track", this, SLOT(slotINDITelescopeTrack()))
         << i18n("Toggle Telescope Tracking")
         << QIcon::fromTheme("object-locked");
    telescopeGroup->addAction(ka);
    ka = actionCollection()->addAction("telescope_slew", this, SLOT(slotINDITelescopeSlew()))
         << i18n("Slew telescope to the focused object")
         << QIcon::fromTheme("object-rotate-right");
    telescopeGroup->addAction(ka);
    ka = actionCollection()->addAction("telescope_sync", this, SLOT(slotINDITelescopeSync()))
         << i18n("Sync telescope to the focused object")
         << QIcon::fromTheme("media-record");
    telescopeGroup->addAction(ka);
    ka = actionCollection()->addAction("telescope_abort", this, SLOT(slotINDITelescopeAbort()))
         << i18n("Abort telescope motions")
         << QIcon::fromTheme("process-stop");
    ka->setShortcutContext(Qt::ApplicationShortcut);
    telescopeGroup->addAction(ka);
    ka = actionCollection()->addAction("telescope_park", this, SLOT(slotINDITelescopePark()))
         << i18n("Park telescope")
         << QIcon::fromTheme("flag-red");
    telescopeGroup->addAction(ka);
    ka = actionCollection()->addAction("telescope_unpark", this, SLOT(slotINDITelescopeUnpark()))
         << i18n("Unpark telescope")
         << QIcon::fromTheme("flag-green");
    ka->setShortcutContext(Qt::ApplicationShortcut);
    telescopeGroup->addAction(ka);

    actionCollection()->addAction("telescope_slew_mouse", this, SLOT(slotINDITelescopeSlewMousePointer()))
            << i18n("Slew the telescope to the mouse pointer position");

    actionCollection()->addAction("telescope_sync_mouse", this, SLOT(slotINDITelescopeSyncMousePointer()))
            << i18n("Sync the telescope to the mouse pointer position");

    // Disable all telescope actions by default
    telescopeGroup->setEnabled(false);

    // Dome Actions
    ka = actionCollection()->addAction("dome_park", this, SLOT(slotINDIDomePark()))
         << i18n("Park dome")
         << QIcon::fromTheme("dome-park", QIcon(":/icons/dome-park.svg"));
    domeGroup->addAction(ka);
    ka = actionCollection()->addAction("dome_unpark", this, SLOT(slotINDIDomeUnpark()))
         << i18n("Unpark dome")
         << QIcon::fromTheme("dome-unpark", QIcon(":/icons/dome-unpark.svg"));
    ka->setShortcutContext(Qt::ApplicationShortcut);
    domeGroup->addAction(ka);

    domeGroup->setEnabled(false);
#endif
}

void KStars::repopulateFOV()
{
    // Read list of all FOVs
    //qDeleteAll( data()->availFOVs );
    data()->availFOVs = FOVManager::getFOVs();
    data()->syncFOV();

    // Iterate through FOVs
    fovActionMenu->menu()->clear();
    foreach (FOV *fov, data()->availFOVs)
    {
        KToggleAction *kta = actionCollection()->add<KToggleAction>(fov->name());
        kta->setText(fov->name());
        if (Options::fOVNames().contains(fov->name()))
        {
            kta->setChecked(true);
        }

        fovActionMenu->addAction(kta);
        connect(kta, SIGNAL(toggled(bool)), this, SLOT(slotTargetSymbol(bool)));
    }
    // Add menu bottom
    QAction *ka = actionCollection()->addAction("edit_fov", this, SLOT(slotFOVEdit())) << i18n("Edit FOV Symbols...");
    fovActionMenu->addSeparator();
    fovActionMenu->addAction(ka);
}

void KStars::repopulateHIPS()
{
    // Iterate through actions
    hipsActionMenu->menu()->clear();
    // Remove all actions
    QList<QAction*> actions = hipsGroup->actions();

    for (auto &action : actions)
        hipsGroup->removeAction(action);

    QAction *ka = actionCollection()->addAction(i18n("None"), this, SLOT(slotHIPSSource()))
                  << i18n("None") << AddToGroup(hipsGroup)
                  << Checked(Options::hIPSSource() == "None");

    hipsActionMenu->addAction(ka);
    hipsActionMenu->addSeparator();

    for (QMap<QString, QString> source : HIPSManager::Instance()->getHIPSSources())
    {
        QString title = source.value("obs_title");

        QAction *ka = actionCollection()->addAction(title, this, SLOT(slotHIPSSource()))
                      << title << AddToGroup(hipsGroup)
                      << Checked(Options::hIPSSource() == title);

        hipsActionMenu->addAction(ka);
    }

    // Hips settings
    ka = actionCollection()->addAction("hipssettings", HIPSManager::Instance(),
                                       SLOT(showSettings())) << i18n("HiPS Settings...");
    hipsActionMenu->addSeparator();
    hipsActionMenu->addAction(ka);
}

void KStars::initStatusBar()
{
    statusBar()->showMessage(i18n(" Welcome to KStars "));

    QString s = "000d 00m 00s,   +00d 00\' 00\""; //only need this to set the width
    if (Options::showAltAzField())
    {
        AltAzField.setText(s);
        statusBar()->insertPermanentWidget(0, &AltAzField);
    }

    if (Options::showRADecField())
    {
        RADecField.setText(s);
        statusBar()->insertPermanentWidget(1, &RADecField);
    }

    if (Options::showJ2000RADecField())
    {
        J2000RADecField.setText(s);
        statusBar()->insertPermanentWidget(1, &J2000RADecField);
    }

    if (!Options::showStatusBar())
        statusBar()->hide();
}

void KStars::datainitFinished()
{
    //Time-related connections
    connect(data()->clock(), SIGNAL(timeAdvanced()), this, SLOT(updateTime()));
    connect(data()->clock(), SIGNAL(timeChanged()), this, SLOT(updateTime()));

    //Add GUI elements to main window
    buildGUI();

    connect(data()->clock(), SIGNAL(scaleChanged(float)), map(), SLOT(slotClockSlewing()));

    connect(data(), SIGNAL(skyUpdate(bool)), map(), SLOT(forceUpdateNow()));
    connect(m_TimeStepBox, SIGNAL(scaleChanged(float)), data(), SLOT(setTimeDirection(float)));
    connect(m_TimeStepBox, SIGNAL(scaleChanged(float)), data()->clock(), SLOT(setClockScale(float)));
    connect(m_TimeStepBox, SIGNAL(scaleChanged(float)), map(), SLOT(setFocus()));

    //m_equipmentWriter = new EquipmentWriter();
    //m_observerAdd = new ObserverAdd;

    //Do not start the clock if "--paused" specified on the cmd line
    if (StartClockRunning)
    {
        // The initial time is set when KStars is first executed
        // but until all data is loaded, some time elapsed already so we need to synchronize if no Start Date string
        // was supplied to KStars
        if (StartDateString.isEmpty())
            data()->changeDateTime(KStarsDateTime::currentDateTimeUtc());

        data()->clock()->start();
    }

    // Connect cache function for Find dialog
    connect(data(), SIGNAL(clearCache()), this, SLOT(clearCachedFindDialog()));

    //Propagate config settings
    applyConfig(false);

    //show the window.  must be before kswizard and messageboxes
    show();

    //Initialize focus
    initFocus();

    data()->setFullTimeUpdate();
    updateTime();

    // Initial State
    qCDebug(KSTARS) << "Date/Time is:" << data()->clock()->utc().toString();
    qCDebug(KSTARS) << "Location:" << data()->geo()->fullName();
    qCDebug(KSTARS) << "TZ0:" << data()->geo()->TZ0() << "TZ:" << data()->geo()->TZ();

    KSTheme::Manager::instance()->setCurrentTheme(Options::currentTheme());

    //If this is the first startup, show the wizard
    if (Options::runStartupWizard())
    {
        slotWizard();
    }

    //Show TotD
    KTipDialog::showTip(this, "kstars/tips");

    // Update comets and asteroids if enabled.
    if (Options::orbitalElementsAutoUpdate())
    {
        slotUpdateComets(true);
        slotUpdateAsteroids(true);
    }

#ifdef HAVE_INDI
    Ekos::Manager::Instance()->initialize();
#endif
}

void KStars::initFocus()
{
    //Case 1: tracking on an object
    if (Options::isTracking() && Options::focusObject() != i18n("nothing"))
    {
        SkyObject *oFocus;
        if (Options::focusObject() == i18n("star"))
        {
            SkyPoint p(Options::focusRA(), Options::focusDec());
            double maxrad = 1.0;

            oFocus = data()->skyComposite()->starNearest(&p, maxrad);
        }
        else
        {
            oFocus = data()->objectNamed(Options::focusObject());
        }

        if (oFocus)
        {
            map()->setFocusObject(oFocus);
            map()->setClickedObject(oFocus);
            map()->setFocusPoint(oFocus);
        }
        else
        {
            qWarning() << "Cannot center on " << Options::focusObject() << ": no object found.";
        }

        //Case 2: not tracking, and using Alt/Az coords.  Set focus point using
        //FocusRA as the Azimuth, and FocusDec as the Altitude
    }
    else if (!Options::isTracking() && Options::useAltAz())
    {
        SkyPoint pFocus;
        pFocus.setAz(Options::focusRA());
        pFocus.setAlt(Options::focusDec());
        pFocus.HorizontalToEquatorial(data()->lst(), data()->geo()->lat());
        map()->setFocusPoint(&pFocus);

        //Default: set focus point using FocusRA as the RA and
        //FocusDec as the Dec
    }
    else
    {
        SkyPoint pFocus(Options::focusRA(), Options::focusDec());
        pFocus.EquatorialToHorizontal(data()->lst(), data()->geo()->lat());
        map()->setFocusPoint(&pFocus);
    }
    data()->setSnapNextFocus();
    map()->setDestination(*map()->focusPoint());
    map()->setFocus(map()->destination());

    map()->showFocusCoords();

    //Check whether initial position is below the horizon.
    if (Options::useAltAz() && Options::showGround() && map()->focus()->alt().Degrees() <= SkyPoint::altCrit)
    {
        QString caption = i18n("Initial Position is Below Horizon");
        QString message =
            i18n("The initial position is below the horizon.\nWould you like to reset to the default position?");
        if (KMessageBox::warningYesNo(this, message, caption, KGuiItem(i18n("Reset Position")),
                                      KGuiItem(i18n("Do Not Reset")), "dag_start_below_horiz") == KMessageBox::Yes)
        {
            map()->setClickedObject(nullptr);
            map()->setFocusObject(nullptr);
            Options::setIsTracking(false);

            data()->setSnapNextFocus(true);

            SkyPoint DefaultFocus;
            DefaultFocus.setAz(180.0);
            DefaultFocus.setAlt(45.0);
            DefaultFocus.HorizontalToEquatorial(data()->lst(), data()->geo()->lat());
            map()->setDestination(DefaultFocus);
        }
    }

    //If there is a focusObject() and it is a SS body, add a temporary Trail
    if (map()->focusObject() && map()->focusObject()->isSolarSystem() && Options::useAutoTrail())
    {
        ((KSPlanetBase *)map()->focusObject())->addToTrail();
        data()->temporaryTrail = true;
    }
}

void KStars::buildGUI()
{
    //create the texture manager
    TextureManager::Create();
    //create the skymap
    m_SkyMap = SkyMap::Create();
    connect(m_SkyMap, SIGNAL(mousePointChanged(SkyPoint*)), SLOT(slotShowPositionBar(SkyPoint*)));
    connect(m_SkyMap, SIGNAL(zoomChanged()), SLOT(slotZoomChanged()));
    setCentralWidget(m_SkyMap);

    //Initialize menus, toolbars, and statusbars
    initStatusBar();
    initActions();

    // Setup GUI from the settings file
    // UI tests provide the default settings file from the resources explicitly file to render UI properly
    setupGUI(StandardWindowOptions(Default), m_KStarsUIResource);

    //get focus of keyboard and mouse actions (for example zoom in with +)
    map()->QWidget::setFocus();
    resize(Options::windowWidth(), Options::windowHeight());

    // check zoom in/out buttons
    if (Options::zoomFactor() >= MAXZOOM)
        actionCollection()->action("zoom_in")->setEnabled(false);
    if (Options::zoomFactor() <= MINZOOM)
        actionCollection()->action("zoom_out")->setEnabled(false);
}

void KStars::populateThemes()
{
    KSTheme::Manager::instance()->setThemeMenuAction(new QMenu(i18n("&Themes"), this));
    KSTheme::Manager::instance()->registerThemeActions(this);

    connect(KSTheme::Manager::instance(), SIGNAL(signalThemeChanged()), this, SLOT(slotThemeChanged()));
}

void KStars::slotThemeChanged()
{
    Options::setCurrentTheme(KSTheme::Manager::instance()->currentThemeName());
}
