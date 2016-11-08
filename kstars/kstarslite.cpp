/** *************************************************************************
                          kstarslite.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 30/04/2016
    copyright            : (C) 2016 by Artem Fedoskin
    email                : afedoskin3@gmail.com
 ***************************************************************************/
/** *************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "kstarslite.h"
#include "skymaplite.h"
#include "kstarsdata.h"
#include <QQmlContext>
#include <QGuiApplication>
#include <QQuickWindow>
#include <QQuickStyle>
#include <QSurfaceFormat>
#include "kstarslite/imageprovider.h"
#include "klocalizedcontext.h"
#include "ksplanetbase.h"
#include <QScreen>

#include "indi/clientmanagerlite.h"

#include "kspaths.h"

//Dialogs
#include "kstarslite/dialogs/finddialoglite.h"
#include "kstarslite/dialogs/detaildialoglite.h"
#include "kstarslite/dialogs/locationdialoglite.h"

#include "Options.h"
#include "ksutils.h"

KStarsLite *KStarsLite::pinstance = 0;

KStarsLite::KStarsLite( bool doSplash, bool startClock, const QString &startDateString) {
    // Initialize logging settings
    /*if (Options::disableLogging())
        KSUtils::Logging::Disable();
    else if (Options::logToFile() && Options::verboseLogFile().isEmpty() == false)
        KSUtils::Logging::UseFile(Options::verboseLogFile());
    else
        KSUtils::Logging::UseDefault();*/

    // Set pinstance to yourself
    // Unlike KStars class we set pinstance at the beginning because SkyMapLite needs access to ClientManagerLite
    pinstance = this;

    if ( doSplash )
        showSplash();

    m_KStarsData = KStarsData::Create();
    Q_ASSERT( m_KStarsData );

    //INDI Android Client
    m_clientManager = new ClientManagerLite;
    m_Engine.rootContext()->setContextProperty("ClientManagerLite", m_clientManager);

    //Make instance of KStarsLite and KStarsData available to QML
    m_Engine.rootContext()->setContextProperty("KStarsLite", this);
    m_Engine.rootContext()->setContextProperty("KStarsData", m_KStarsData);
    m_Engine.rootContext()->setContextProperty("Options", Options::self());
    m_Engine.rootContext()->setContextProperty("SimClock", m_KStarsData->clock());
    m_Engine.rootContext()->setContextObject(new KLocalizedContext(this));
    qmlRegisterUncreatableType<Projector>("KStarsLiteEnums", 1, 0, "Projection", "Provides Projection enum");
    qmlRegisterUncreatableType<KStarsLite>("KStarsLiteEnums", 1, 0, "ObjectsToToggle", "Enum for togglint the visibility of sky objects");

    //Dialogs
    m_findDialogLite = new FindDialogLite;
    m_detailDialogLite = new DetailDialogLite;
    m_locationDialogLite = new LocationDialogLite;

    m_Engine.rootContext()->setContextProperty("FindDialogLite", m_findDialogLite);
    m_Engine.rootContext()->setContextProperty("DetailDialogLite", m_detailDialogLite);
    m_Engine.rootContext()->setContextProperty("LocationDialogLite", m_locationDialogLite);

    //Set Geographic Location from Options
    m_KStarsData->setLocationFromOptions();

    //Set style - default is Material
    QQuickStyle::setStyle("Material");
#ifdef Q_OS_ANDROID
    QString main = KSPaths::locate(QStandardPaths::AppDataLocation, "kstarslite/qml/main.qml");
#else
    QString main = QString(QML_IMPORT) + QString("/kstarslite/qml/main.qml");
#endif

    /*SkyMapLite has to be loaded before KStarsData is initialized because SkyComponents derived classes
    have to add SkyItems to the SkyMapLite*/
    m_SkyMapLite = SkyMapLite::createInstance();
    m_Engine.rootContext()->setContextProperty("SkyMapLite", m_SkyMapLite);

    m_Engine.load(QUrl(main));
    Q_ASSERT_X(m_Engine.rootObjects().size(),"loading root object of main.qml",
               "QML file was not loaded. Probably syntax error or failed module import.");

    m_RootObject = m_Engine.rootObjects()[0];

    QQuickItem *skyMapLiteWrapper = m_RootObject->findChild<QQuickItem*>("skyMapLiteWrapper");

    m_SkyMapLite->initialize(skyMapLiteWrapper);
    m_detailDialogLite->initialize();

    m_imgProvider = new ImageProvider;
    m_Engine.addImageProvider(QLatin1String("images"), m_imgProvider);

    QQuickWindow *mainWindow = static_cast<QQuickWindow *>(m_Engine.rootObjects()[0]);

    QSurfaceFormat format = mainWindow->format();
    format.setSamples(4);
    format.setSwapBehavior(QSurfaceFormat::TripleBuffer);
    mainWindow->setFormat(format);


    connect(qApp, SIGNAL(applicationStateChanged(Qt::ApplicationState)), SLOT(handleStateChange(Qt::ApplicationState)));

    //Initialize Time and Date
    if (startDateString.isEmpty() == false)
    {
        KStarsDateTime startDate = KStarsDateTime::fromString( startDateString );
        if (startDate.isValid() )
            data()->changeDateTime( data()->geo()->LTtoUT( startDate ) );
        else
            data()->changeDateTime( KStarsDateTime::currentDateTimeUtc() );
    }
    else data()->changeDateTime( KStarsDateTime::currentDateTimeUtc() );

    // Initialize clock. If --paused is not in the comand line, look in options
    if ( startClock ) StartClockRunning =  Options::runClock();

    // Setup splash screen
    connect( m_KStarsData, SIGNAL( progressText(QString) ), m_KStarsData, SLOT( slotConsoleMessage(QString) ) );

    //set up Dark color scheme for application windows
    DarkPalette = QPalette(QColor("darkred"), QColor("darkred"));
    DarkPalette.setColor( QPalette::Normal, QPalette::Base, QColor( "black" ) );
    DarkPalette.setColor( QPalette::Normal, QPalette::Text, QColor( 238, 0, 0 ) );
    DarkPalette.setColor( QPalette::Normal, QPalette::Highlight, QColor( 238, 0, 0 ) );
    DarkPalette.setColor( QPalette::Normal, QPalette::HighlightedText, QColor( "black" ) );
    DarkPalette.setColor( QPalette::Inactive, QPalette::Text, QColor( 238, 0, 0 ) );
    DarkPalette.setColor( QPalette::Inactive, QPalette::Base, QColor( 30, 10, 10 ) );
    //store original color scheme
    OriginalPalette = QGuiApplication::palette();
    if( !m_KStarsData->initialize() ) return;
    datainitFinished();

}

void KStarsLite::slotTrack() {
    if ( Options::isTracking() ) {
        Options::setIsTracking( false );
        /*actionCollection()->action("track_object")->setText( i18n( "Engage &Tracking" ) );
        actionCollection()->action("track_object")->setIcon( QIcon::fromTheme("document-decrypt") );

        KSPlanetBase* planet = dynamic_cast<KSPlanetBase*>( map()->focusObject() );
        if( planet && data()->temporaryTrail ) {
            planet->clearTrail();
            data()->temporaryTrail = false;
        }*/ // No trail support yet

        map()->setClickedObject( NULL );
        map()->setFocusObject( NULL );//no longer tracking focusObject
        map()->setFocusPoint( NULL );
    } else {
        map()->setClickedPoint( map()->focus() );
        map()->setClickedObject( NULL );
        map()->setFocusObject( NULL );//no longer tracking focusObject
        map()->setFocusPoint( map()->clickedPoint() );
        Options::setIsTracking( true );
        /*actionCollection()->action("track_object")->setText( i18n( "Stop &Tracking" ) );
        actionCollection()->action("track_object")->setIcon( QIcon::fromTheme("document-encrypt") );*/
    }

    map()->forceUpdate();
}

KStarsLite *KStarsLite::createInstance( bool doSplash, bool clockrunning, const QString &startDateString) {
    delete pinstance;
    // pinstance is set directly in constructor.
    new KStarsLite( doSplash, clockrunning, startDateString );
    Q_ASSERT( pinstance && "pinstance must be non NULL");
    return nullptr;
}

KStarsLite::~KStarsLite() {
    delete m_imgProvider;
}

void KStarsLite::fullUpdate() {
    m_KStarsData->setFullTimeUpdate();
    updateTime();

    m_SkyMapLite->forceUpdate();
}

void KStarsLite::updateTime( const bool automaticDSTchange ) {
    // Due to frequently use of this function save data and map pointers for speedup.
    // Save options and geo() to a pointer would not speedup because most of time options
    // and geo will accessed only one time.
    KStarsData *Data = data();
    // dms oldLST( Data->lst()->Degrees() );

    Data->updateTime( Data->geo(), automaticDSTchange );

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

bool KStarsLite::writeConfig()
{
   // It seems two config files are saved to android. Must call them both to save all options
   // First one save color information, 2nd one rest of config. Bug?
   // /data/user/0/org.kde.kstars/files/settings/kstarsrc is used by KSharedConfig::openConfig()
   KSharedConfig::openConfig()->sync();
   // /data/data/org.kde.kstars/files/settings/kstarsrc is used by Options::self()
   Options::self()->save();

    //Store current simulation time
    //Refer to // FIXME: Used in kstarsdcop.cpp only in kstarsdata.cpp
    //data()->StoredDate = data()->lt();
}

void KStarsLite::handleStateChange(Qt::ApplicationState state)
{
    if (state == Qt::ApplicationSuspended)
    {
        // Delete skymaplite. This required to run destructors and save
        // current state in the option.
        //delete m_SkyMapLite;

        //Store Window geometry in Options object
        //Options::setWindowWidth( m_RootObject->width() );
        //Options::setWindowHeight( m_RootObject->height() );

        //explicitly save the colorscheme data to the config file
        //data()->colorScheme()->saveToConfig();
        //synch the config file with the Config object
        writeConfig();
    }
}

void KStarsLite::loadColorScheme( const QString &name )
{
    bool ok = data()->colorScheme()->load( name );
    QString filename = data()->colorScheme()->fileName();

    if ( ok ) {
        //set the application colors for the Night Vision scheme
        if ( Options::darkAppColors() == false && filename == "night.colors" )  {
            Options::setDarkAppColors( true );
            OriginalPalette = QGuiApplication::palette();
            QGuiApplication::setPalette( DarkPalette );
        }

        if ( Options::darkAppColors() && filename != "night.colors" ) {
            Options::setDarkAppColors( false );
            QGuiApplication::setPalette( OriginalPalette );
        }

        Options::setColorSchemeFile( name );

        data()->colorScheme()->saveToConfig();

        //writeConfig();

        //Reinitialize stars textures
        map()->initStarImages();

        map()->forceUpdate();
    }
}

void KStarsLite::slotSetTime(QDateTime time) {
    KStarsDateTime selectedDateTime( time );
    data()->changeDateTime( data()->geo()->LTtoUT( selectedDateTime ) );

    if ( Options::useAltAz() ) {
        if ( map()->focusObject() ) {
            map()->focusObject()->EquatorialToHorizontal( data()->lst(), data()->geo()->lat() );
            map()->setFocus( map()->focusObject() );
        } else
            map()->focus()->HorizontalToEquatorial( data()->lst(), data()->geo()->lat() );
    }

    map()->forceUpdateNow();

    //If focusObject has a Planet Trail, clear it and start anew.
    /*KSPlanetBase* planet = dynamic_cast<KSPlanetBase*>( map()->focusObject() );
        if( planet && planet->hasTrail() ) {
            planet->clearTrail();
            planet->addToTrail();
        }*/
}

void KStarsLite::slotToggleTimer() {
    if ( data()->clock()->isActive() ) {
        data()->clock()->stop();
        updateTime();
    } else {
        if ( fabs( data()->clock()->scale() ) > Options::slewTimeScale() )
            data()->clock()->setManualMode( true );
        data()->clock()->start();
        if ( data()->clock()->isManualMode() )
            map()->forceUpdate();
    }

    // Update clock state in options
    Options::setRunClock( data()->clock()->isActive() );
}

void KStarsLite::slotStepForward() {
    if ( data()->clock()->isActive() )
        data()->clock()->stop();
    data()->clock()->manualTick( true );
    map()->forceUpdate();
}

void KStarsLite::slotStepBackward() {
    if ( data()->clock()->isActive() )
        data()->clock()->stop();
    data()->clock()->setClockScale( -1.0 * data()->clock()->scale() ); //temporarily need negative time step
    data()->clock()->manualTick( true );
    data()->clock()->setClockScale( -1.0 * data()->clock()->scale() ); //reset original sign of time step
    map()->forceUpdate();
}

void KStarsLite::applyConfig(bool doApplyFocus) {
    Q_UNUSED(doApplyFocus);
    //color scheme
    m_KStarsData->colorScheme()->loadFromConfig();
    QGuiApplication::setPalette( Options::darkAppColors() ? DarkPalette : OriginalPalette );
}

void KStarsLite::setProjection(uint proj) {
    Options::setProjection(proj);
    //We update SkyMapLite 2 times because of the bug in Projector::updateClipPoly()
    SkyMapLite::Instance()->forceUpdate();
}

QColor KStarsLite::getColor(QString schemeColor) {
    return KStarsData::Instance()->colorScheme()->colorNamed(schemeColor);
}

QString KStarsLite::getConfigCScheme() {
    return Options::colorSchemeFile();
}

void KStarsLite::toggleObjects(ObjectsToToggle toToggle, bool toggle) {
    switch(toToggle) {
    case ObjectsToToggle::Stars:
        Options::setShowStars(toggle);
        break;
    case ObjectsToToggle::DeepSky:
        Options::setShowDeepSky(toggle);
        break;
    case ObjectsToToggle::Planets:
        Options::setShowSolarSystem(toggle);
        break;
    case ObjectsToToggle::CLines:
        Options::setShowCLines(toggle);
        break;
    case ObjectsToToggle::CBounds:
        Options::setShowCBounds(toggle);
        break;
    case ObjectsToToggle::ConstellationArt:
        Options::setShowConstellationArt(toggle);
        break;
    case ObjectsToToggle::MilkyWay:
        Options::setShowMilkyWay(toggle);
        break;
    case ObjectsToToggle::CNames:
        Options::setShowCNames(toggle);
        break;
    case ObjectsToToggle::EquatorialGrid:
        Options::setShowEquatorialGrid(toggle);
        break;
    case ObjectsToToggle::HorizontalGrid:
        Options::setShowHorizontalGrid(toggle);
        break;
    case ObjectsToToggle::Ground:
        Options::setShowGround(toggle);
        break;
    case ObjectsToToggle::Flags:
        Options::setShowFlags(toggle);
        break;
    case ObjectsToToggle::Satellites:
        Options::setShowSatellites(toggle);
        break;
    case ObjectsToToggle::Supernovae:
        Options::setShowSupernovae(toggle);
        break;
    };

    // update time for all objects because they might be not initialized
    // it's needed when using horizontal coordinates
    data()->setFullTimeUpdate();
    updateTime();

    map()->forceUpdate();
}

bool KStarsLite::isToggled(ObjectsToToggle toToggle) {
    switch(toToggle) {
    case ObjectsToToggle::Stars:
        return Options::showStars();
    case ObjectsToToggle::DeepSky:
        return Options::showDeepSky();
    case ObjectsToToggle::Planets:
        return Options::showSolarSystem();
    case ObjectsToToggle::CLines:
        return Options::showCLines();
    case ObjectsToToggle::CBounds:
        return Options::showCBounds();
    case ObjectsToToggle::ConstellationArt:
        return Options::showConstellationArt();
    case ObjectsToToggle::MilkyWay:
        return Options::showMilkyWay();
    case ObjectsToToggle::CNames:
        return Options::showCNames();
    case ObjectsToToggle::EquatorialGrid:
        return Options::showEquatorialGrid();
    case ObjectsToToggle::HorizontalGrid:
        return Options::showHorizontalGrid();
    case ObjectsToToggle::Ground:
        return Options::showGround();
    case ObjectsToToggle::Flags:
        return Options::showFlags();
    case ObjectsToToggle::Satellites:
        return Options::showSatellites();
    case ObjectsToToggle::Supernovae:
        return Options::showSupernovae();
    default:
        return false;
    };
}

void KStarsLite::setRunTutorial(bool runTutorial) {
    if(Options::runStartupWizard() != runTutorial) {
        Options::setRunStartupWizard(runTutorial);
        emit runTutorialChanged();
    }
}

bool KStarsLite::getRunTutorial() {
    return Options::runStartupWizard();
}
