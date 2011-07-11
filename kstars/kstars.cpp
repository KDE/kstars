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

#include "kstars.h"

#include <QApplication>

#include <KGlobal>
#include <KLocale>
#include <kdebug.h>
#include <kactioncollection.h>
#include <kstatusbar.h>
#include <ktoolbar.h>
#include <kicon.h>

#include "Options.h"
#include "kstarsdata.h"
#include "kstarssplash.h"
#include "skymap.h"
#include "simclock.h"
#include "dialogs/finddialog.h"
#include "observinglist.h"
#include "oal/execute.h"

#include "kstarsadaptor.h"

#include <config-kstars.h>

#ifdef HAVE_INDI_H
#include "indi/indimenu.h"
#include "indi/indidriver.h"
#include "indi/imagesequence.h"
#endif

KStars *KStars::pinstance = 0;

KStars::KStars( bool doSplash, bool clockrun, const QString &startdate ) :
        KXmlGuiWindow(), kstarsData(0), skymap(0), TimeStep(0),
        colorActionMenu(0), fovActionMenu(0),
        AAVSODialog(0), findDialog(0), imgExportDialog(0), obsList(0),
        execute(0),
        avt(0), wut(0), skycal(0),
        sb(0), pv(0), jmt(0), fm(0), astrocalc(0), indimenu(0), indidriver(0), indiseq(0),
        DialogIsObsolete(false), StartClockRunning( clockrun ),
        StartDateString( startdate )
{
    new KstarsAdaptor(this);
    QDBusConnection::sessionBus().registerObject("/KStars",  this);
    QDBusConnection::sessionBus().registerService("org.kde.kstars");

    // Set pinstance to yourself
    pinstance = this;

    connect( qApp, SIGNAL( aboutToQuit() ), this, SLOT( slotAboutToQuit() ) );

    //Initialize QActionGroups
    projectionGroup = new QActionGroup( this );
    cschemeGroup    = new QActionGroup( this );

    kstarsData = KStarsData::Create();
    Q_ASSERT( kstarsData );
    //Set Geographic Location from Options
    kstarsData->setLocationFromOptions();

    //Initialize Time and Date
    KStarsDateTime startDate = KStarsDateTime::fromString( StartDateString );
    if ( ! StartDateString.isEmpty() && startDate.isValid() )
        data()->changeDateTime( data()->geo()->LTtoUT( startDate ) );
    else
        data()->changeDateTime( KStarsDateTime::currentUtcDateTime() );

    // Setup splash screen
    KStarsSplash *splash = 0;
    if ( doSplash ) {
        splash = new KStarsSplash(0);
        connect( kstarsData, SIGNAL( progressText(QString) ), splash, SLOT( setMessage(QString) ));
        splash->show();
    } else {
        connect( kstarsData, SIGNAL( progressText(QString) ), kstarsData, SLOT( slotConsoleMessage(QString) ) );
    }

    //set up Dark color scheme for application windows
    DarkPalette = QPalette(QColor("darkred"), QColor("darkred"));
    DarkPalette.setColor( QPalette::Normal, QPalette::Base, QColor( "black" ) );
    DarkPalette.setColor( QPalette::Normal, QPalette::Text, QColor( 238, 0, 0 ) );
    DarkPalette.setColor( QPalette::Normal, QPalette::Highlight, QColor( 238, 0, 0 ) );
    DarkPalette.setColor( QPalette::Normal, QPalette::HighlightedText, QColor( "black" ) );
    //store original color scheme
    OriginalPalette = QApplication::palette();

    //Initialize data.  When initialization is complete, it will run dataInitFinished()
    if( !kstarsData->initialize() )
        return;
    delete splash;
    datainitFinished();

#if ( __GLIBC__ >= 2 &&__GLIBC_MINOR__ >= 1  && !defined(__UCLIBC__) )
    kDebug() << "glibc >= 2.1 detected.  Using GNU extension sincos()";
#else
    kDebug() << "Did not find glibc >= 2.1.  Will use ANSI-compliant sin()/cos() functions.";
#endif
}

KStars *KStars::createInstance( bool doSplash, bool clockrun, const QString &startdate ) {
    delete pinstance;
    // pinstance is set directly in constructor.
    new KStars( doSplash, clockrun, startdate );
    Q_ASSERT( pinstance && "pinstance must be non NULL");
    return pinstance;
}

KStars::~KStars()
{
    Q_ASSERT( pinstance );

    delete kstarsData;
    pinstance = 0;
}

void KStars::clearCachedFindDialog() {
    if ( findDialog  ) {  // dialog is cached
        /** Delete findDialog only if it is not opened */
        if ( findDialog->isHidden() ) {
            delete findDialog;
            findDialog = 0;
            DialogIsObsolete = false;
        }
        else
            DialogIsObsolete = true;  // dialog was opened so it could not deleted
    }
}

void KStars::applyConfig( bool doApplyFocus ) {
    if ( Options::isTracking() ) {
        actionCollection()->action("track_object")->setText( i18n( "Stop &Tracking" ) );
        actionCollection()->action("track_object")->setIcon( KIcon("document-encrypt") );
    }

    actionCollection()->action("coordsys")->setText(
        Options::useAltAz() ? i18n("Switch to star globe view (Equatorial &Coordinates)"): i18n("Switch to horizonal view (Horizontal &Coordinates)") );

    #ifdef HAVE_OPENGL
    Q_ASSERT( SkyMap::Instance() ); // This assert should not fail, because SkyMap is already created by now. Just throwing it in anyway.
    actionCollection()->action("opengl")->setText( (Options::useGL() ? i18n("Switch to QPainter backend"): i18n("Switch to OpenGL backend")) );
    #endif


    actionCollection()->action("show_time_box"    )->setChecked( Options::showTimeBox() );
    actionCollection()->action("show_location_box")->setChecked( Options::showGeoBox() );
    actionCollection()->action("show_focus_box"   )->setChecked( Options::showFocusBox() );
    actionCollection()->action("show_statusBar"   )->setChecked( Options::showStatusBar() );
    actionCollection()->action("show_sbAzAlt"     )->setChecked( Options::showAltAzField() );
    actionCollection()->action("show_sbRADec"     )->setChecked( Options::showRADecField() );
    actionCollection()->action("show_stars"       )->setChecked( Options::showStars() );
    actionCollection()->action("show_deepsky"     )->setChecked( Options::showDeepSky() );
    actionCollection()->action("show_planets"     )->setChecked( Options::showSolarSystem() );
    actionCollection()->action("show_clines"      )->setChecked( Options::showCLines() );
    actionCollection()->action("show_cnames"      )->setChecked( Options::showCNames() );
    actionCollection()->action("show_cbounds"     )->setChecked( Options::showCBounds() );
    actionCollection()->action("show_mw"          )->setChecked( Options::showMilkyWay() );
    actionCollection()->action("show_grid"        )->setChecked( Options::showGrid() );
    actionCollection()->action("show_horizon"     )->setChecked( Options::showGround() );
    actionCollection()->action("show_flags"       )->setChecked( Options::showFlags() );
    statusBar()->setVisible( Options::showStatusBar() );

    //color scheme
    kstarsData->colorScheme()->loadFromConfig();
    QApplication::setPalette( Options::darkAppColors() ? DarkPalette : OriginalPalette );

    //Set toolbar options from config file
    toolBar("kstarsToolBar")->applySettings( KGlobal::config()->group( "MainToolBar" ) );
    toolBar( "viewToolBar" )->applySettings( KGlobal::config()->group( "ViewToolBar" ) );

    //Geographic location
    data()->setLocationFromOptions();

    //Focus
    if ( doApplyFocus ) {
        SkyObject *fo = data()->objectNamed( Options::focusObject() );
        if ( fo && fo != map()->focusObject() ) {
            map()->setClickedObject( fo );
            map()->setClickedPoint( fo );
            map()->slotCenter();
        }
    
        if ( ! fo ) {
            SkyPoint fp( Options::focusRA(), Options::focusDec() );
            if ( fp.ra().Degrees() != map()->focus()->ra().Degrees() || fp.dec().Degrees() != map()->focus()->dec().Degrees() ) {
                map()->setClickedPoint( &fp );
                map()->slotCenter();
            }
        }
    }
}

FlagManager* KStars::getFlagManager() {
    return fm;
}

void KStars::updateTime( const bool automaticDSTchange ) {
    // Due to frequently use of this function save data and map pointers for speedup.
    // Save options and geo() to a pointer would not speedup because most of time options
    // and geo will accessed only one time.
    KStarsData *Data = data();
    SkyMap *Map = map();
    // dms oldLST( Data->lst()->Degrees() );

    Data->updateTime( Data->geo(), Map, automaticDSTchange );

    //We do this outside of kstarsdata just to get the coordinates
    //displayed in the infobox to update every second.
    //	if ( !Options::isTracking() && LST()->Degrees() > oldLST.Degrees() ) {
    //		int nSec = int( 3600.*( LST()->Hours() - oldLST.Hours() ) );
    //		Map->focus()->setRA( Map->focus()->ra().Hours() + double( nSec )/3600. );
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

Execute* KStars::getExecute() {
    if( !execute )
        execute = new Execute();
    return execute;
}

#include "kstars.moc"

