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
//JH 24.08.2001: reorganized infoPanel
//JH 25.08.2001: added toolbar, converted menu items to KAction objects
//JH 25.08.2001: main window now resizable, window size saved in config file

#include <qfont.h>
#include <qpopupmenu.h>
#include <qtextstream.h>
#include <qlineedit.h>
#include <qsizepolicy.h>
#include <qtooltip.h>
#include <qlayout.h>
#include <qtimer.h>

#include <kapplication.h>
#include <kconfig.h>
#include <kstdaction.h>
#include <kaccel.h>
#include <kiconloader.h>
#include <kmessagebox.h>
#include <kstatusbar.h>
#include <kpopupmenu.h>

#include <stdio.h>
#include <stream.h>
#include <kdebug.h>

#include "timedialog.h"
#include "locationdialog.h"
#include "finddialog.h"
#include "viewopsdialog.h"

#include "kstars.h"

KStars::KStars( KStarsData* kstarsData )
	: KMainWindow( NULL, NULL )
{
// need to set the mainwidget here, in main() this will cause a segfault because
// skymap will created in this constructor and needs kapp->mainWidget()
	kapp->setMainWidget( this );
	// here we get the preloaded data (stars, constellations etc.)

	this->kstarsData = kstarsData;

  //Initialize object type strings
	TypeName[0] = i18n( "star" );
	TypeName[1] = i18n( "star" );
	TypeName[2] = i18n( "planet" );
	TypeName[3] = i18n( "open cluster" );
	TypeName[4] = i18n( "globular cluster" );
	TypeName[5] = i18n( "gaseous nebula" );
	TypeName[6] = i18n( "planetary nebula" );
	TypeName[7] = i18n( "supernova remnant" );
	TypeName[8] = i18n( "galaxy" );
	TypeName[9] = i18n( "Users should never see this", "UNUSED_TYPE" );

	//resize( 640, 600 );
	initMenuBar();
	initToolBar();
	initStatusBar();
	initOptions();
	initLocation();

	tmr = new QTimer( this );
	tmr->start( TIMER_INTERVAL, FALSE );
	QObject::connect( tmr, SIGNAL( timeout() ), this, SLOT( updateTime() ) );

// create the widgets
	QWidget *centralWidget = new QWidget( this );
	setCentralWidget( centralWidget );
	infoPanel = new QFrame( centralWidget );
	skymap = new SkyMap( centralWidget );
	skymap->setFocus();		// get focus of keyboard and mouse actions (for example zoom in with +)

// create the layout of the central widget
	QVBoxLayout *topLayout = new QVBoxLayout( centralWidget );
	topLayout->addWidget( infoPanel );
	topLayout->addWidget( skymap );

	//time settings that we couldn't do in KStarsSplash:
	GetData()->UTime = GetData()->now.addSecs( int(geo->TZ()*-3600) );
	GetData()->LST   = UTtoLST( GetData()->UTime, geo->lng() );
	GetData()->CurrentDate = getJD( GetData()->UTime );
	GetData()->LastSkyUpdate = GetData()->CurrentDate;
	updateEpoch( GetData()->CurrentDate );

	skymap->setMinimumSize( 380, 250 );
	skymap->setSizePolicy( QSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding ) );
	skymap->LSTh.setH( GetData()->LST.hour(), GetData()->LST.minute(), GetData()->LST.second() );

	initStars();

	//Create the infoPanel, which displays time/date/location data
	infoPanel->setFrameShape( QFrame::Panel );

	//white-on-black color scheme for infoPanel
	QPalette pal( infoPanel->palette() );
	pal.setColor( QPalette::Normal, QColorGroup::Background, QColor( "Black" ) );
	pal.setColor( QPalette::Normal, QColorGroup::Foreground, QColor( "White" ) );
	pal.setColor( QPalette::Inactive, QColorGroup::Foreground, QColor( "White" ) );
	infoPanel->setPalette( pal );

	//Create main Layout manager for infoPanel
	iplay = new QHBoxLayout( infoPanel, 2, 2, "iplay" );

	//Create Widgets to appear in the info panel
	LTLabel = new QLabel( i18n( "Local Time", "LT:" ), infoPanel );
	UTLabel = new QLabel( i18n( "Universal Time", "UT:" ), infoPanel );
	STLabel = new QLabel( i18n( "Sidereal Time", "ST:" ), infoPanel );
	LTLabel->setPalette( pal );
	UTLabel->setPalette( pal );
	STLabel->setPalette( pal );

  //Set font for infoPanel widgets; for now just use default font.
	QFont ipFont = LTLabel->font();
	//Uncomment to set smaller font for infoPanel
	//ipFont.setPointSize( ipFont.pointSize() - 2 );
  //Uncomment for fixed-width font
	//ipFont.setFixedPitch( true );

	LTLabel->setFont( ipFont );
	UTLabel->setFont( ipFont );
	STLabel->setFont( ipFont );

	LT = new QLabel( "00:00:00", infoPanel );
	LT->setFixedSize( LT->sizeHint() ); //otherwise, placement of other labels not static

//not using locale.formatTime() because I couldn't find a way to control whether "pm" is displayed...
	LT->setText( GetData()->LTime.time().toString() );
	UT = new QLabel( GetData()->UTime.time().toString(), infoPanel );
	ST = new QLabel( "00:00:00", infoPanel );
	LT->setPalette( pal );
	UT->setPalette( pal );
	ST->setPalette( pal );
	LT->setFont( ipFont );
	UT->setFont( ipFont );
	ST->setFont( ipFont );

	LTDate = new QLabel( GetData()->locale->formatDate( GetData()->LTime.date(), true ), infoPanel );
	UTDate = new QLabel( GetData()->locale->formatDate( GetData()->UTime.date(), true ), infoPanel );
	JD = new QLabel( "JD: " + QString("%1").arg(GetData()->CurrentEpoch, 10, 'f', 2), infoPanel );
	LTDate->setPalette( pal );
	UTDate->setPalette( pal );
	JD->setPalette( pal );
	LTDate->setFont( ipFont );
	UTDate->setFont( ipFont );
	JD->setFont( ipFont );

	FocusObject = new QLabel( i18n( "Focused on: " ) + i18n( "nothing" ), infoPanel );
	FocusRA = new QLabel( i18n( "Right Ascension", "RA" ) + ": 00:00:00    ", infoPanel );
	FocusDec = new QLabel( i18n( "Declination", "Dec" ) + ": +00:00:00", infoPanel );
	FocusAz = new QLabel( i18n( "Azimuth", "Az" ) + ": +000:00:00 ", infoPanel );
	FocusAlt = new QLabel( i18n( "Altitude", "Alt" ) + ": +00:00:00", infoPanel );
	FocusRA->setFixedSize( FocusRA->sizeHint() );
	FocusAz->setFixedSize( FocusAz->sizeHint() );
	FocusObject->setPalette( pal );
	FocusRA->setPalette( pal );
	FocusDec->setPalette( pal );
	FocusAz->setPalette( pal );
	FocusAlt->setPalette( pal );
	FocusObject->setFont( ipFont );
	FocusRA->setFont( ipFont );
	FocusDec->setFont( ipFont );
	FocusAz->setFont( ipFont );
	FocusAlt->setFont( ipFont );

	PlaceName = new QLabel( i18n("nowhere"), infoPanel );
	if ( geo->province().isEmpty() ) {
		PlaceName->setText( geo->translatedName() + ", " + geo->translatedCountry() );
	} else {
		PlaceName->setText( geo->translatedName() + ", " + geo->translatedProvince() + ",  " + geo->translatedCountry() );
	}
	LongLabel = new QLabel( i18n( "Longitude", "Long: " ), infoPanel );
	LatLabel = new QLabel( i18n( "Latitude", "Lat:  " ), infoPanel );
	Long = new QLabel( QString::number( geo->lng().getD(), 'f', 3 ), infoPanel );
	Long->setAlignment( AlignRight );
	Lat = new QLabel( QString::number( geo->lat().getD(), 'f', 3 ), infoPanel );
	Lat->setAlignment( AlignRight );
	PlaceName->setPalette( pal );
	LongLabel->setPalette( pal );
	LatLabel->setPalette( pal );
	Long->setPalette( pal );
	Lat->setPalette( pal );
	PlaceName->setFont( ipFont );
	LongLabel->setFont( ipFont );
	LatLabel->setFont( ipFont );
	Long->setFont( ipFont );
	Lat->setFont( ipFont );

	//create layouts to be added to iplay
	tlablay = new QVBoxLayout( iplay, 1, "tlablay" );
	iplay->addSpacing( 6 );
	timelay = new QVBoxLayout( iplay, 1, "timelay" );
	iplay->addSpacing( 4 );
	datelay = new QVBoxLayout( iplay, 1, "datelay" );
	iplay->addSpacing( 16 );
	focuslay = new QVBoxLayout( iplay, 1, "focuslay" );
	iplay->addStretch();
	geolay = new QVBoxLayout( iplay, 1, "geolay" );

	//Pack widgets into infoPanel
	tlablay->addWidget( LTLabel );
	tlablay->addWidget( UTLabel );
	tlablay->addWidget( STLabel );

	timelay->addWidget( LT );
	timelay->addWidget( UT );
	timelay->addWidget( ST );

	datelay->addWidget( LTDate );
	datelay->addWidget( UTDate );
	datelay->addWidget( JD );

	focuslay->addWidget( FocusObject );

	radeclay = new QHBoxLayout( focuslay, 1, "radeclay" );
	radeclay->addWidget( FocusRA );
	radeclay->addWidget( FocusDec );

	altazlay = new QHBoxLayout( focuslay, 1, "altazlay" );
	altazlay->addWidget( FocusAz );
	altazlay->addWidget( FocusAlt );

	geolay->addWidget( PlaceName );

	coolay = new QGridLayout( geolay, 3, 2, 2, "coolay" );
	coolay->addItem( new QSpacerItem( 10, 10, QSizePolicy::Minimum, QSizePolicy::Expanding ), 0, 0 );
	coolay->addItem( new QSpacerItem( 10, 10, QSizePolicy::Minimum, QSizePolicy::Expanding ), 1, 0 );
	coolay->addWidget( LongLabel, 0, 1 );
	coolay->addWidget( LatLabel, 1, 1 );
	coolay->addWidget( Long, 0, 2 );
	coolay->addWidget( Lat, 1, 2 );

	infoPanel->setFixedHeight( infoPanel->sizeHint().height() );	// use sizeHint() to get accurate size
	resize( GetOptions()->windowWidth, GetOptions()->windowHeight );

//Set focus of Skymap.
//if user was tracking last time, track on same object now.
	if ( GetOptions()->isTracking ) {
    //Set default position in case focus object is below horizon
		focus.setAz( 180.0 );
		focus.setAlt( 45.0 );
		focus.AltAzToRADec( skymap->LSTh, geo->lat() );

		if ( (GetOptions()->focusObject== i18n( "star" ) ) ||
		     (GetOptions()->focusObject== i18n( "nothing" ) ) ) {
			skymap->clickedPoint.set( GetOptions()->focusRA, GetOptions()->focusDec );
		} else {
			skymap->clickedObject = getObjectNamed( GetOptions()->focusObject );
			if ( skymap->clickedObject ) {
				skymap->clickedPoint.set( skymap->clickedObject->getRA(), skymap->clickedObject->getDec() );
			} else {
				skymap->clickedPoint.set( GetOptions()->focusRA, GetOptions()->focusDec );
			}
		}
	} else {
		skymap->clickedPoint.set( GetOptions()->focusRA, GetOptions()->focusDec );
	}

	skymap->slotCenter();

	skymap->HourAngle.setH( skymap->LSTh.getH() - skymap->focus.getRA().getH() );
	skymap->oldfocus.set( skymap->focus.getRA(), skymap->focus.getDec() );
	skymap->oldfocus.setAz( skymap->focus.getAz() );
	skymap->oldfocus.setAlt( skymap->focus.getAlt() );
}

KStars::~KStars()
{
	//Sync the config file
	kapp->config()->setGroup( "Location" );
	kapp->config()->writeEntry( "City", GetOptions()->CityName );
	kapp->config()->writeEntry( "Province", GetOptions()->ProvinceName );
	kapp->config()->writeEntry( "Country", GetOptions()->CountryName );
	kapp->config()->setGroup( "View" );
	kapp->config()->writeEntry( "SkyColor", 	GetOptions()->colorSky );
	kapp->config()->writeEntry( "MWColor", 		GetOptions()->colorMW );
	kapp->config()->writeEntry( "EqColor", 		GetOptions()->colorEq );
	kapp->config()->writeEntry( "EclColor", 		GetOptions()->colorEcl );
	kapp->config()->writeEntry( "HorzColor", 	GetOptions()->colorHorz );
	kapp->config()->writeEntry( "GridColor", 	GetOptions()->colorGrid );
	kapp->config()->writeEntry( "MessColor", 	GetOptions()->colorMess );
	kapp->config()->writeEntry( "NGCColor", 	GetOptions()->colorNGC );
	kapp->config()->writeEntry( "ICColor", 		GetOptions()->colorIC );
	kapp->config()->writeEntry( "CLineColor", GetOptions()->colorCLine );
	kapp->config()->writeEntry( "CNameColor", GetOptions()->colorCName );
	kapp->config()->writeEntry( "SNameColor", GetOptions()->colorSName );
	kapp->config()->writeEntry( "HSTColor", 	GetOptions()->colorHST );
	kapp->config()->writeEntry( "StarColorMode", GetOptions()->starColorMode );
	kapp->config()->writeEntry( "StarColorsIntensity", GetOptions()->starColorIntensity );
	kapp->config()->writeEntry( "ShowBSC", 		GetOptions()->drawBSC );
	kapp->config()->writeEntry( "ShowMess", 	GetOptions()->drawMessier );
	kapp->config()->writeEntry( "ShowMessImages", 	GetOptions()->drawMessImages );
	kapp->config()->writeEntry( "ShowNGC", 		GetOptions()->drawNGC );
	kapp->config()->writeEntry( "ShowIC", 		GetOptions()->drawIC );
	kapp->config()->writeEntry( "ShowCLines", GetOptions()->drawConstellLines );
	kapp->config()->writeEntry( "ShowCNames", GetOptions()->drawConstellNames );
	kapp->config()->writeEntry( "UseLatinConstellationNames", GetOptions()->useLatinConstellNames );
	kapp->config()->writeEntry( "UseLocalConstellationNames", GetOptions()->useLocalConstellNames );
	kapp->config()->writeEntry( "UseAbbrevConstellationNames", GetOptions()->useAbbrevConstellNames );
	kapp->config()->writeEntry( "ShowMilkyWay", GetOptions()->drawMilkyWay );
	kapp->config()->writeEntry( "ShowGrid", GetOptions()->drawGrid );
	kapp->config()->writeEntry( "ShowEquator", GetOptions()->drawEquator );
	kapp->config()->writeEntry( "ShowEcliptic", GetOptions()->drawEcliptic );
	kapp->config()->writeEntry( "ShowHorizon", GetOptions()->drawHorizon );
	kapp->config()->writeEntry( "ShowGround", GetOptions()->drawGround );
	kapp->config()->writeEntry( "ShowSun", 		GetOptions()->drawSun );
	kapp->config()->writeEntry( "ShowMoon", 	GetOptions()->drawMoon );
	kapp->config()->writeEntry( "ShowMercury", 	GetOptions()->drawMercury );
	kapp->config()->writeEntry( "ShowVenus", 	GetOptions()->drawVenus );
	kapp->config()->writeEntry( "ShowMars", 	GetOptions()->drawMars );
	kapp->config()->writeEntry( "ShowJupiter", 	GetOptions()->drawJupiter );
	kapp->config()->writeEntry( "ShowSaturn", 	GetOptions()->drawSaturn );
	kapp->config()->writeEntry( "ShowUranus", 	GetOptions()->drawUranus );
	kapp->config()->writeEntry( "ShowNeptune", 	GetOptions()->drawNeptune );
	kapp->config()->writeEntry( "ShowPluto", 	GetOptions()->drawPluto );
	kapp->config()->writeEntry( "IsTracking", 	GetOptions()->isTracking );
	if ( skymap->foundObject != NULL ) {
		kapp->config()->writeEntry( "FocusObject",  skymap->foundObject->name );
	} else {
		kapp->config()->writeEntry( "FocusObject", i18n( "nothing" ) );
	}
	kapp->config()->writeEntry( "UseAltAz", 	GetOptions()->useAltAz );
	kapp->config()->writeEntry( "FocusRA", skymap->focus.getRA().getH() );
	kapp->config()->writeEntry( "FocusDec", skymap->focus.getDec().getD() );
	kapp->config()->writeEntry( "ZoomLevel", skymap->ZoomLevel );
	kapp->config()->writeEntry( "windowWidth", width() );
	kapp->config()->writeEntry( "windowHeight", height() );
	kapp->config()->writeEntry( "magLimitDrawStar", 	 GetOptions()->magLimitDrawStar );
	kapp->config()->writeEntry( "magLimitDrawStarInfo",GetOptions()->magLimitDrawStarInfo );
	kapp->config()->writeEntry( "drawStarName", 			 GetOptions()->drawStarName );
	kapp->config()->writeEntry( "drawStarMagnitude",   GetOptions()->drawStarMagnitude );
	kapp->config()->sync();

	// remove data object
	delete kstarsData;
	//kstarsData = 0;

	delete geo;
	delete LT;
	delete UT;
	delete ST;
	delete JD;
	delete LTDate;
	delete UTDate;
	delete LTLabel;
	delete UTLabel;
	delete STLabel;
	delete PlaceName;
	delete LongLabel;
	delete LatLabel;

	delete skymap;
}

KStarsData* KStars::GetData()
{
	return kstarsData;
}

KStarsOptions* KStars::GetOptions()
{
	return kstarsData->options;
}


void KStars::initMenuBar() {
	KPopupMenu *p;

	p = new KPopupMenu;
	actQuit = KStdAction::quit(this, SLOT( close() ), actionCollection() );
	actQuit->plug( p );
	menuBar()->insertItem( i18n( "&File" ), p );

	p = new KPopupMenu;
	actTimeNow = new KAction( i18n( "Set Time to &Now" ), 0, this, SLOT( mSetTimeToNow() ), actionCollection() );
	actTimeNow->setAccel( KAccel::stringToKey( "Ctrl+N"  ) );
	actTimeSet = new KAction( i18n( "&Set Time..." ), BarIcon( "clock" ), 0, this, SLOT( mSetTime() ), actionCollection() );
	actTimeSet->setAccel( KAccel::stringToKey( "Ctrl+S"  ) );
	actTimeRun = new KAction( i18n( "Stop &Clock" ), BarIcon( "player_pause" ), 0, this, SLOT( mToggleTimer() ), actionCollection() );
	actTimeNow->plug( p );
	actTimeSet->plug( p );
	actTimeRun->plug( p );
	menuBar()->insertItem( i18n( "&Time" ), p );

	p = new KPopupMenu;
	p->insertItem( i18n( "&Zenith" ), this, SLOT( mZenith() ));
	actFind = KStdAction::find( this, SLOT( mFind() ), actionCollection() );
	actFind->setText( i18n( "&Find Object..." ) );
	actFind->setToolTip( i18n( "Find Object" ) );
	actFind->plug( p );
	actTrack = new KAction( i18n( "&Track Object" ), BarIcon( "unlock" ), 0, this, SLOT( mTrack() ), actionCollection() );
	actTrack->setAccel( KAccel::stringToKey( "Ctrl+T"  ) );
	actTrack->plug( p );
	p->insertSeparator();

	//use custom icon earth.png for the geoLocator icon.  If it is not installed
  //for some reason, use standard icon gohome.png instead.
	QFile tempFile;
	if (KStarsData::openDataFile( tempFile, "earth.png" ) ) {
		actLocation = new KAction( i18n( "&Geographic..." ), QPixmap( tempFile.name() ), 0, this, SLOT( mGeoLocator() ), actionCollection() );
		actLocation->setToolTip( i18n( "Geographic Location" ) );
		tempFile.close();
	} else {
		actLocation = new KAction( i18n( "&Geographic..." ), BarIcon( "gohome" ), 0, this, SLOT( mGeoLocator() ), actionCollection() );
	}
	actLocation->setAccel( KAccel::stringToKey( "Ctrl+G"  ) );
	actLocation->plug( p );
	menuBar()->insertItem( i18n( "&Location" ), p );

	p = new KPopupMenu;
//	p->insertItem( i18n( "&Reverse Video" ), this, SLOT( mReverseVideo() ));
	actZoomIn = KStdAction::zoomIn(this, SLOT( mZoomIn() ), actionCollection() );
	actZoomOut = KStdAction::zoomOut(this, SLOT( mZoomOut() ), actionCollection() );
	actZoomIn->plug( p );
	actZoomOut->plug( p );

	p->insertSeparator();
	actViewOps = KStdAction::preferences( this, SLOT( mViewOps() ), actionCollection() );
	actViewOps->plug( p );
	menuBar()->insertItem( i18n( "&View" ), p );

	p = helpMenu( 0, false );

	menuBar()->insertItem( i18n( "&Help" ), p );

}

void KStars::initToolBar() {
	toolBar()->setFullSize();
  toolBar()->enableMoving( false );

	actQuit->plug( toolBar() );
	toolBar()->insertSeparator( );
	actZoomIn->plug( toolBar() );
	actZoomOut->plug( toolBar() );
	toolBar()->insertSeparator( );
	actFind->plug( toolBar() );
	actTrack->plug( toolBar() );
	actLocation->plug( toolBar() );
	actViewOps->plug( toolBar() );
	toolBar()->insertSeparator( );
	actTimeSet->plug( toolBar() );
	actTimeRun->plug( toolBar() );
	toolBar()->insertSeparator( );

	TimeStep = new TimeSpinBox( toolBar() );
	QToolTip::add( TimeStep, i18n( "Time Step (seconds)" ) );
	QWidget *Blank = new QWidget( toolBar() );

	idSpinBox = 0;
	toolBar()->insertWidget( idSpinBox, 6, TimeStep );
	toolBar()->insertWidget( 1, 40, Blank );
	toolBar()->setStretchableWidget( Blank );
//	actInfo->plug( toolBar() );
	actHandbook = new KAction( i18n( "&Handbook" ), BarIcon( "contents" ), 0, this, SLOT( appHelpActivated() ), actionCollection() );
	actHandbook->plug( toolBar() );
}

void KStars::initStatusBar() {
	statusBar()->insertItem( i18n( " Welcome to KStars " ), 0, 1, true );
	statusBar()->setItemAlignment( 0, AlignLeft | AlignVCenter );
	CurrentRA = "00:00:00";
	CurrentDec = "  +00:00:00";
	QString s = CurrentRA + ",  " + CurrentDec;

	statusBar()->insertItem( s, 1, 1, true );
	statusBar()->setItemAlignment( 1, AlignRight | AlignVCenter );
	statusBar()->setItemFixed( 1, -1 );
}

/** Menu Slot Functions **/
void KStars::mSetTimeToNow() {
	GetData()->LTime.setTime( QTime::currentTime() );
	GetData()->LTime.setDate( QDate::currentDate() );
	GetData()->UTime.setTime( GetData()->LTime.time() );
	GetData()->UTime.setDate( GetData()->LTime.date() );
	GetData()->UTime = GetData()->UTime.addSecs( int( geo->TZ()*-3600) );

	GetData()->then = QDateTime::currentDateTime();
	updateTime();
}

void KStars::initOptions()
{
	// Get initial Location from config()
	kapp->config()->setGroup( "Location" );
	GetOptions()->CityName = kapp->config()->readEntry( "City", "Greenwich" );

	if ( kapp->config()->readEntry( "State", "" ).length() ) { //old version of config file
		GetOptions()->ProvinceName = kapp->config()->readEntry( "State", "" );
		GetOptions()->CountryName = kapp->config()->readEntry( "State", "United Kingdom" );
	} else {
		GetOptions()->ProvinceName = kapp->config()->readEntry( "Province", "" );
		GetOptions()->CountryName = kapp->config()->readEntry( "Country", "United Kingdom" );
	}

	kapp->config()->setGroup( "View" );
	GetOptions()->colorSky 		= kapp->config()->readEntry( "SkyColor", "#002" );
//	GetOptions()->colorStar 	= kapp->config()->readEntry( "StarColor", "#FFF" );
	GetOptions()->colorMW 		= kapp->config()->readEntry( "MWColor", "#123" );
	GetOptions()->colorEq 		= kapp->config()->readEntry( "EqColor", "#FFF" );
	GetOptions()->colorEcl		= kapp->config()->readEntry( "EclColor", "#663" );
	GetOptions()->colorHorz 	= kapp->config()->readEntry( "HorzColor", "#5A3" );
	GetOptions()->colorGrid 	= kapp->config()->readEntry( "GridColor", "#456" );
	GetOptions()->colorMess 	= kapp->config()->readEntry( "MessColor", "#0F0" );
	GetOptions()->colorNGC 		= kapp->config()->readEntry( "NGCColor", "#066" );
	GetOptions()->colorIC 		= kapp->config()->readEntry( "ICColor", "#439" );
	GetOptions()->colorCLine 	= kapp->config()->readEntry( "CLineColor", "#555" );
	GetOptions()->colorCName 	= kapp->config()->readEntry( "CNameColor", "#AA7" );
	GetOptions()->colorSName 	= kapp->config()->readEntry( "SNameColor", "#7AA" );
	GetOptions()->colorHST 		= kapp->config()->readEntry( "HSTColor", "#A00" );
	GetOptions()->drawBSC 		= kapp->config()->readBoolEntry( "ShowBSC", true );
	GetOptions()->drawMessier = kapp->config()->readBoolEntry( "ShowMess", true );
	GetOptions()->drawMessImages = kapp->config()->readBoolEntry( "ShowMessImages", true );
	GetOptions()->drawNGC 		= kapp->config()->readBoolEntry( "ShowNGC", true );
	GetOptions()->drawIC 			= kapp->config()->readBoolEntry( "ShowIC", true );
	GetOptions()->drawConstellLines = kapp->config()->readBoolEntry( "ShowCLines", true );
	GetOptions()->drawConstellNames = kapp->config()->readBoolEntry( "ShowCNames", true );
	GetOptions()->useLatinConstellNames = kapp->config()->readBoolEntry( "UseLatinConstellationNames", true );
	GetOptions()->useLocalConstellNames = kapp->config()->readBoolEntry( "UseLocalConstellationNames", false );
	GetOptions()->useAbbrevConstellNames = kapp->config()->readBoolEntry( "UseAbbrevConstellationNames", false );
	GetOptions()->drawMilkyWay = kapp->config()->readBoolEntry( "ShowMilkyWay", true );
	GetOptions()->drawGrid = kapp->config()->readBoolEntry( "ShowGrid", true );
	GetOptions()->drawEquator = kapp->config()->readBoolEntry( "ShowEquator", true );
	GetOptions()->drawEcliptic = kapp->config()->readBoolEntry( "ShowEcliptic", true );
	GetOptions()->drawHorizon = kapp->config()->readBoolEntry( "ShowHorizon", true );
	GetOptions()->drawGround = kapp->config()->readBoolEntry( "ShowGround", true );
	GetOptions()->drawSun = kapp->config()->readBoolEntry( "ShowSun", true );
	GetOptions()->drawMoon = kapp->config()->readBoolEntry( "ShowMoon", true );
	GetOptions()->drawMercury = kapp->config()->readBoolEntry( "ShowMercury", true );
	GetOptions()->drawVenus = kapp->config()->readBoolEntry( "ShowVenus", true );
	GetOptions()->drawMars = kapp->config()->readBoolEntry( "ShowMars", true );
	GetOptions()->drawJupiter = kapp->config()->readBoolEntry( "ShowJupiter", true );
	GetOptions()->drawSaturn = kapp->config()->readBoolEntry( "ShowSaturn", true );
	GetOptions()->drawUranus = kapp->config()->readBoolEntry( "ShowUranus", true );
	GetOptions()->drawNeptune = kapp->config()->readBoolEntry( "ShowNeptune", true );
	GetOptions()->drawPluto = kapp->config()->readBoolEntry( "ShowPluto", true );
	GetOptions()->useAltAz = kapp->config()->readBoolEntry( "UseAltAz", true );
	GetOptions()->isTracking = kapp->config()->readBoolEntry( "IsTracking", false );
	GetOptions()->focusObject = kapp->config()->readEntry( "FocusObject", "nothing" );
	GetOptions()->focusDec = kapp->config()->readDoubleNumEntry( "FocusDec", 45.0 );
	GetOptions()->focusRA = kapp->config()->readDoubleNumEntry( "FocusRA", 180.0 );
//	GetOptions()->magLimitDrawStar = kapp->config()->readDoubleNumEntry( "magLimitDrawStar", 8.0 );		// readed in KStarsOptions()
	GetOptions()->magLimitDrawStarInfo = kapp->config()->readDoubleNumEntry( "magLimitDrawStarInfo", 2.0 );
	GetOptions()->drawStarName = kapp->config()->readBoolEntry( "drawStarName", false );
	GetOptions()->drawStarMagnitude = kapp->config()->readBoolEntry( "drawStarMagnitude", false );
	GetOptions()->windowWidth = kapp->config()->readNumEntry( "windowWidth", 600 );
	GetOptions()->windowHeight = kapp->config()->readNumEntry( "windowHeight", 600 );
	GetOptions()->starColorMode = kapp->config()->readNumEntry( "StarColorMode", 0 );
	GetOptions()->starColorIntensity = kapp->config()->readNumEntry ("StarColorsIntensity", 4);
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

	for (GeoData = GetData()->geoList.first(); GeoData; GeoData = GetData()->geoList.next())
	{

	//If the config file is old (has a State key) match a city if the city name
  //AND EITHER the province name OR the country name match
  //(old config files set both province and country to "StateName")
  //this can produce the wrong city, if more than one matches these criteria...
		if ( oldConfig ) {
			if ( (GeoData->name().lower() == GetOptions()->CityName.lower()) &&
					( (GeoData->province().lower() == GetOptions()->ProvinceName.lower()) ||
					(GeoData->country().lower() == GetOptions()->CountryName.lower()) ) )
			{
				bFound = TRUE;
				if ( GeoData->province().lower() != GetOptions()->ProvinceName.lower() )
					GetOptions()->ProvinceName = GeoData->province();
				if ( GeoData->country().lower() != GetOptions()->CountryName.lower() )
					GetOptions()->CountryName = GeoData->country();
				break ;
			}
		} else {
	//Otherwise, require all three fields (City, Province, and Country) to match
			if ( GetOptions()->ProvinceName.stripWhiteSpace().length() ) {
				if ( (GeoData->name().lower() == GetOptions()->CityName.lower()) &&
						(GeoData->province().lower() == GetOptions()->ProvinceName.lower()) &&
						(GeoData->country().lower() == GetOptions()->CountryName.lower()) )
				{
					bFound = TRUE;
					break ;
				}
			} else {
				if ( (GeoData->name().lower() == GetOptions()->CityName.lower()) &&
						(GeoData->country().lower() == GetOptions()->CountryName.lower()) )
				{
					bFound = TRUE;
					break ;
				}

			}
		}
	}

	if ( !bFound ) { // set city, province and country to default values
		GetOptions()->CityName = "Greenwich";
		GetOptions()->ProvinceName = "";
		GetOptions()->CountryName = "United Kingdom";
		for (GeoData = GetData()->geoList.first(); GeoData; GeoData = GetData()->geoList.next())
		{
			if ( (GeoData->name().lower() == GetOptions()->CityName.lower()) &&
					(GeoData->province().lower() == GetOptions()->ProvinceName.lower()) &&
					(GeoData->country().lower() == GetOptions()->CountryName.lower()) )
			{
				bFound = TRUE;
				break ;
			}
		}
	}

	if (bFound) {
		geo = new GeoLocation (GeoData);
	} else { //couldn't set geographic location, so set the "null" location.
		QString message = i18n( "Could not set geographic location!" );
		KMessageBox::sorry( 0, message, i18n( "No location set" ) );
		geo = new GeoLocation();
	}
}

void KStars::initStars()
{
	// Recompute Alt, Az coords for all objects. This was not possible
	// within loading the KStarsData object. Do it now.
	// Solar System
	GetData()->Sun->pos()->RADecToAltAz( skymap->LSTh, geo->lat() );
	GetData()->Mercury->pos()->RADecToAltAz( skymap->LSTh, geo->lat() );
	GetData()->Venus->pos()->RADecToAltAz( skymap->LSTh, geo->lat() );
	GetData()->Mars->pos()->RADecToAltAz( skymap->LSTh, geo->lat() );
	GetData()->Jupiter->pos()->RADecToAltAz( skymap->LSTh, geo->lat() );
	GetData()->Saturn->pos()->RADecToAltAz( skymap->LSTh, geo->lat() );
	GetData()->Uranus->pos()->RADecToAltAz( skymap->LSTh, geo->lat() );
	GetData()->Neptune->pos()->RADecToAltAz( skymap->LSTh, geo->lat() );
	GetData()->Pluto->pos()->RADecToAltAz( skymap->LSTh, geo->lat() );

	// Stars
	for (SkyObject *data = GetData()->starList.first(); data; data = GetData()->starList.next()) {
		data->pos()->updateCoords( GetData()->CurrentEpoch, GetData()->Obliquity, GetData()->dObliq, GetData()->dEcLong );
		data->pos()->RADecToAltAz( skymap->LSTh, geo->lat() );
	}

	// Messier
	for (SkyObject *data = GetData()->messList.first(); data; data = GetData()->messList.next()) {
		data->pos()->updateCoords( GetData()->CurrentEpoch, GetData()->Obliquity, GetData()->dObliq, GetData()->dEcLong );
		data->pos()->RADecToAltAz( skymap->LSTh, geo->lat() );
	}

	// NGC
	for (SkyObject *data = GetData()->ngcList.first(); data; data = GetData()->ngcList.next()) {
		data->pos()->updateCoords( GetData()->CurrentEpoch, GetData()->Obliquity, GetData()->dObliq, GetData()->dEcLong );
		data->pos()->RADecToAltAz( skymap->LSTh, geo->lat() );
	}

	// IC
	for (SkyObject *data = GetData()->icList.first(); data; data = GetData()->icList.next()) {
		data->pos()->updateCoords( GetData()->CurrentEpoch, GetData()->Obliquity, GetData()->dObliq, GetData()->dEcLong );
		data->pos()->RADecToAltAz( skymap->LSTh, geo->lat() );
	}

	// Milky Way
	for ( unsigned int j=0; j<11; ++j ) {
		for (SkyPoint *data = GetData()->MilkyWay[j].first(); data; data = GetData()->MilkyWay[j].next()) {
			data->updateCoords( GetData()->CurrentEpoch, GetData()->Obliquity, GetData()->dObliq, GetData()->dEcLong );
			data->RADecToAltAz( skymap->LSTh, geo->lat() );
		}
	}

	// CLines
	for (SkyPoint *data = GetData()->clineList.first(); data; data = GetData()->clineList.next()) {
		data->updateCoords( GetData()->CurrentEpoch, GetData()->Obliquity, GetData()->dObliq, GetData()->dEcLong );
		data->RADecToAltAz( skymap->LSTh, geo->lat() );
	}

	// CNames
	for (SkyObject *data = GetData()->cnameList.first(); data; data = GetData()->cnameList.next()) {
		data->pos()->updateCoords( GetData()->CurrentEpoch, GetData()->Obliquity, GetData()->dObliq, GetData()->dEcLong );
		data->pos()->RADecToAltAz( skymap->LSTh, geo->lat() );
	}

	// Define the Celestial Equator
	for ( unsigned int i=0; i<NCIRCLE; ++i ) {
		SkyPoint *o = new SkyPoint( i*24./NCIRCLE, 0.0 );
		o->RADecToAltAz( skymap->LSTh, geo->lat() );
		GetData()->Equator.append( o );
	}

  // Define the horizon.
  // Use the celestial Equator as a convenient starting point, but instead of RA and Dec,
  // interpret the coordinates as azimuth and altitude, and then convert to RA, dec.
  // The horizon will be redefined whenever the positions of sky objects are updated.
	for (SkyPoint *data = GetData()->Equator.first(); data; data = GetData()->Equator.next()) {
		double sinlat, coslat, sindec, cosdec, sinAz, cosAz;
		double HARad;
		dms dec, HA, RA, Az;
		Az = data->getRA();
		Az.SinCos( sinAz, cosAz );
		geo->lat().SinCos( sinlat, coslat );

		dec.setRadians( asin( coslat*cosAz ) );
		dec.SinCos( sindec, cosdec );
   		HARad = acos( -1.0*(sinlat*sindec)/(coslat*cosdec) );
		if ( sinAz > 0.0 ) { HARad = 2.0*PI() - HARad; }
		HA.setRadians( HARad );
		RA = skymap->LSTh.getD() - HA.getD();

		SkyPoint *o = new SkyPoint( RA, dec );
		o->setAlt( 0.0 );
		o->setAz( Az );

		GetData()->Horizon.append( o );

		//Define the Ecliptic (use the same ListIteration; interpret coordinates as Ecliptic long/lat)
		double ELong, ELat;
		ELong = data->getRA().getD();
		ELat = 0.0;
		o = new SkyPoint( 0.0, 0.0 );
		o->setEcliptic( ELong, ELat, GetData()->CurrentDate );
		o->RADecToAltAz( skymap->LSTh, geo->lat() );
		GetData()->Ecliptic.append( o );
	}
}

void KStars::mSetTime() {

	TimeDialog timedialog ( GetData()->LTime, this );

	if ( timedialog.exec() == QDialog::Accepted ) {
		if (tmr->isActive() ) {
			tmr->stop();
	    actTimeRun->setIconSet( BarIcon( "1rightarrow" ) );
			actTimeRun->setText( i18n( "Start &Clock" ) );
			actTimeRun->setToolTip( i18n( "Start Clock" ) );
		}

		QTime newTime( timedialog.HourBox->value(),
					   timedialog.MinuteBox->value(),
					   timedialog.SecondBox->value() );

		QDate newDate( timedialog.dPicker->getDate() );

		GetData()->LTime.setTime( newTime );
		GetData()->LTime.setDate( newDate );
		GetData()->UTime.setTime( newTime );
		GetData()->UTime.setDate( newDate );
		GetData()->UTime = GetData()->UTime.addSecs( int(geo->TZ()*-3600) );
		GetData()->CurrentDate = getJD( GetData()->UTime );

		//Make sure Moon, planets, and sky objects are updated immediately
		GetData()->LastMoonUpdate = GetData()->CurrentDate - 1.0;
		GetData()->LastPlanetUpdate = GetData()->CurrentDate - 1.0;
		GetData()->LastSkyUpdate = GetData()->CurrentDate - 1.0;

		GetData()->then = QDateTime::currentDateTime();
		updateTime();
	}
}

void KStars::mToggleTimer() {
	if ( tmr->isActive() ) {
		tmr->stop();
    actTimeRun->setIconSet( BarIcon( "1rightarrow" ) );
		actTimeRun->setText( i18n( "Start &Clock" ) );
		actTimeRun->setToolTip( i18n( "Start Clock" ) );
	} else {
		GetData()->then = QDateTime::currentDateTime();
		tmr->start( TIMER_INTERVAL, FALSE );
		actTimeRun->setIconSet( BarIcon( "player_pause" ) );
		actTimeRun->setText( i18n( "Stop &Clock" ) );
		actTimeRun->setToolTip( i18n( "Stop Clock" ) );
	}
}

void KStars::mZenith() {
	skymap->focus.setAlt( 90.0 );
	skymap->focus.AltAzToRADec( skymap->LSTh, geo->lat() );
	skymap->HourAngle.setH( skymap->LSTh.getH() - skymap->focus.getRA().getH() );
	skymap->Update();
}

void KStars::mZoomIn() {
	actZoomOut->setEnabled (true);
	if ( skymap->ZoomLevel < 12 ) {
		++skymap->ZoomLevel;
		//If zoomed in and using Alt-Az, update the Alt-Az coordinates
		if ( GetOptions()->useAltAz && skymap->ZoomLevel > 10 ) {
			oldfocus.set( oldfocus.getRA().getD(), oldfocus.getDec().getD()+5.0 );
			updateTime();
		}
		skymap->Update();
	}
	if (skymap->ZoomLevel == 12)
		actZoomIn->setEnabled (false);
}

void KStars::mZoomOut() {
	actZoomIn->setEnabled (true);
	if ( skymap->ZoomLevel > 0 ) {
		--skymap->ZoomLevel;
		skymap->Update();
	}
	if (skymap->ZoomLevel == 0)
		actZoomOut->setEnabled (false);
}

void KStars::mFind() {
	FindDialog finddialog (this);
	if ( finddialog.exec() == QDialog::Accepted && finddialog.currentItem() ) {
		skymap->clickedObject = finddialog.currentItem()->objName()->skyObject();
		skymap->clickedPoint.set( skymap->clickedObject->getRA(), skymap->clickedObject->getDec() );
		skymap->slotCenter();
	}
}

void KStars::mTrack() {
	if ( GetOptions()->isTracking ) {
		GetOptions()->isTracking = false;
		actTrack->setIconSet( BarIcon( "unlock" ) );
		skymap->clickedObject = NULL;
		skymap->foundObject = NULL;//no longer tracking foundObject
	} else {
		GetOptions()->isTracking = true;
		actTrack->setIconSet( BarIcon( "lock" ) );
	}
}

void KStars::mViewOps() {
	// save options for cancel
	GetData()->saveOptions();

	ViewOpsDialog viewopsdialog (this);
	// ask for the new options
	if ( viewopsdialog.exec() != QDialog::Accepted ) {
		// cancelled
		GetData()->restoreOptions();
	}
}

void KStars::mAstroInfo() {
	QFile file;
	if ( KStarsData::openDataFile( file, "astroinfo.html" ) ) {
		file.close();
		kapp->invokeBrowser( file.name() );
	} else {
		QString message = "I could not find the AstroInfo table of contents (astroinfo.html).";
		KMessageBox::sorry( 0, message, i18n( "Could not launch AstroInfo" ) );
	}
}

void KStars::mReverseVideo() {
/*
	QString temp = GetOptions()->colorSky;
	GetOptions()->SkyColor = StarColor;
	GetOptions()->StarColor = temp;
	skymap->update();
*/
}

void KStars::mGeoLocator() {
	LocationDialog locationdialog (this);
	if ( locationdialog.exec() == QDialog::Accepted ) {
		if ( !locationdialog.NewCityName->text().isEmpty() ) { //user closed the location dialog without adding their new city;
			locationdialog.addCity();                   //call addCity() for them!
		}

	 	int ii = locationdialog.getCityIndex();
 		if ( ii >= 0 ) {
 			geo->reset( GetData()->geoList.at(ii) );
			GetOptions()->CityName = geo->name();
			GetOptions()->ProvinceName = geo->province();
			GetOptions()->CountryName = geo->country();
			if ( geo->province().isEmpty() )
	 			PlaceName->setText( geo->translatedName() + ",  " + geo->translatedCountry() );
			else
	 			PlaceName->setText( geo->translatedName() + ", " + geo->translatedProvince() + ",  " + geo->translatedCountry() );

 			Long->setText( QString::number( geo->lng().getD(), 'f', 3 ) );
 			Lat->setText( QString::number( geo->lat().getD(), 'f', 3 ) );

 			// Adjust Local time for new time zone
	 		GetData()->LTime.setDate( GetData()->UTime.date() );
 			GetData()->LTime.setTime( GetData()->UTime.time() );
 			GetData()->LTime = GetData()->LTime.addSecs( int(geo->TZ()*3600) );

			GetData()->LST = UTtoLST( GetData()->UTime, geo->lng() );
			skymap->LSTh.setH( GetData()->LST.hour(), GetData()->LST.minute(), GetData()->LST.second() );
			skymap->HourAngle.setH( skymap->LSTh.getH() - skymap->focus.getRA().getH() );

      //need to recompute Alt/Az coordinates of all objects, so
			//adjust LastSkyUpdate to ensure computation.  Then
			//explicitly call updateTime()
			GetData()->LastSkyUpdate -= 1.0; // a full day, should be plenty
			updateTime();
	 	}
 	}
}

void KStars::updateTime( void ) {
	GetData()->now = QDateTime::currentDateTime(); //update current time marker

	//advance clocks
	long double nowJD = getJD( GetData()->now );
	long double thenJD = getJD( GetData()->then );
	long double dJD = nowJD - thenJD; //dJD is the number of days since "then"

	float factor = ((TimeSpinBox *)toolBar()->getWidget( idSpinBox ))->text().toFloat();
	int dSeconds = int( dJD*24*3600*factor );
	int dMSeconds = int( 1000*(dJD*24*3600*factor - dSeconds ) );

	//Using QDateTime::addSecs() directly, instead of setTime()
	//will automatically advance the date, when required
	GetData()->LTime = GetData()->LTime.addSecs( dSeconds );
	GetData()->UTime = GetData()->UTime.addSecs( dSeconds );

	//...but there is no QDateTime::addMSecs(), so must use
	//setTime().  So, the date won't advance if the clock rolls over
	//with this step.  Add oldTime to check for date change
	QTime oldLTime = GetData()->LTime.time();
	QTime oldUTime = GetData()->UTime.time();

	GetData()->LTime.setTime( GetData()->LTime.time().addMSecs( dMSeconds ) );
	GetData()->UTime.setTime( GetData()->UTime.time().addMSecs( dMSeconds ) );

	//The following checks assume that time is running forward
	//Will need to change this if backward time is ever implemented
	if ( GetData()->LTime.time().hour() < oldLTime.hour() ) {
		GetData()->LTime = GetData()->LTime.addDays( 1 );
	}

	if ( GetData()->UTime.time().hour() < oldUTime.hour() ) {
		GetData()->UTime = GetData()->UTime.addDays( 1 );
	}

	LT->setText( GetData()->LTime.time().toString() );
	UT->setText( GetData()->UTime.time().toString() );
	LTDate->setText( GetData()->locale->formatDate( GetData()->LTime.date(), true ) );
	UTDate->setText( GetData()->locale->formatDate( GetData()->UTime.date(), true ) );

	GetData()->CurrentDate = getJD( GetData()->UTime );
	JD->setText( "JD: " + QString("%1").arg( GetData()->CurrentDate, 10, 'f', 2) );

	bool isNewEpoch = false;
	if ( fabs( GetData()->CurrentDate - GetData()->CurrentEpoch ) > 365.25 ) { //update epoch-dependent numbers annually
		updateEpoch( GetData()->CurrentDate );
		isNewEpoch = true;
	}

  GetData()->LST = UTtoLST( GetData()->UTime, geo->lng() );
	GetData()->then = GetData()->now; //update previous time marker

	QString dummy;
	QString STString = dummy.sprintf( "%02d:%02d:%02d",
			  GetData()->LST.hour(), GetData()->LST.minute(), GetData()->LST.second() );
	ST->setText( STString );

	skymap->LSTh.setH( GetData()->LST.hour(), GetData()->LST.minute(), GetData()->LST.second() );

  // Update positions of objects, if necessary
  // Sun and Planet positions change rather slowly, so only update them twice daily
  if ( fabs( GetData()->CurrentDate - GetData()->LastPlanetUpdate ) > 0.5 ) {
  	GetData()->LastPlanetUpdate = GetData()->CurrentDate;

		GetData()->Sun->findPosition( GetData()->CurrentDate );
		GetData()->Earth->findPosition( GetData()->CurrentDate );
		GetData()->Mercury->findPosition( GetData()->CurrentDate, GetData()->Earth );
		GetData()->Venus->findPosition( GetData()->CurrentDate, GetData()->Earth );
		GetData()->Mars->findPosition( GetData()->CurrentDate, GetData()->Earth );
		GetData()->Jupiter->findPosition( GetData()->CurrentDate, GetData()->Earth );
		GetData()->Saturn->findPosition( GetData()->CurrentDate, GetData()->Earth );
		GetData()->Uranus->findPosition( GetData()->CurrentDate, GetData()->Earth );
		GetData()->Neptune->findPosition( GetData()->CurrentDate, GetData()->Earth );
		GetData()->Pluto->findPosition( GetData()->CurrentDate, GetData()->Earth );

		//Recompute the Ecliptic
		GetData()->Ecliptic.clear();
		for ( unsigned int i=0; i<GetData()->Equator.count(); ++i ) {
			double ELong, ELat;
			ELong = GetData()->Equator.at(i)->getRA().getD();
			ELat = 0.0;
			SkyPoint *o = new SkyPoint( 0.0, 0.0 );
			o->setEcliptic( ELong, ELat, GetData()->CurrentDate );
			GetData()->Ecliptic.append( o );
		}
	}

	// Moon moves ~30 arcmin/hr, so update its position every minute.
	if ( fabs( GetData()->CurrentDate - GetData()->LastMoonUpdate ) > 0.00069444 ) {
		GetData()->LastMoonUpdate = GetData()->CurrentDate;
		GetData()->Moon->findPosition( GetData()->CurrentDate, geo->lat(), skymap->LSTh );
		GetData()->Moon->findPhase( GetData()->Sun );
	}


	//update focus
	if ( GetOptions()->isTracking ) {
		//if tracking on a planet, update focus to keep up with changing position.
		bool onPlanet = false;
		if ( skymap->foundObject == GetData()->Sun ) onPlanet = true;
		if ( skymap->foundObject == GetData()->Moon ) onPlanet = true;
		if ( skymap->foundObject == GetData()->Mercury ) onPlanet = true;
		if ( skymap->foundObject == GetData()->Venus ) onPlanet = true;
		if ( skymap->foundObject == GetData()->Mars ) onPlanet = true;
		if ( skymap->foundObject == GetData()->Jupiter ) onPlanet = true;
		if ( skymap->foundObject == GetData()->Saturn ) onPlanet = true;
		if ( skymap->foundObject == GetData()->Uranus ) onPlanet = true;
		if ( skymap->foundObject == GetData()->Neptune ) onPlanet = true;
		if ( skymap->foundObject == GetData()->Pluto ) onPlanet = true;

		if ( onPlanet) skymap->focus.set( skymap->foundObject->pos()->getRA(), skymap->foundObject->pos()->getDec() );

		skymap->HourAngle.setH( skymap->LSTh.getH() - skymap->focus.getRA().getH() );
		skymap->focus.RADecToAltAz( skymap->LSTh, geo->lat() );
	} else {
		skymap->focus.setRA( skymap->LSTh.getH() - skymap->HourAngle.getH() );
		skymap->focus.RADecToAltAz( skymap->LSTh, geo->lat() );
  }
	skymap->showFocusCoords();

	//Update Alt/Az coordinates.  Timescale varies with zoom level
	if ( fabs( GetData()->CurrentDate - GetData()->LastSkyUpdate) > 0.25/skymap->pixelScale[skymap->ZoomLevel] ) {
		GetData()->LastSkyUpdate = GetData()->CurrentDate;

		//Recompute Alt, Az coords for all objects
		//Planets
		GetData()->Sun->pos()->RADecToAltAz( skymap->LSTh, geo->lat() );
		GetData()->Moon->pos()->RADecToAltAz( skymap->LSTh, geo->lat() );
		GetData()->Mercury->pos()->RADecToAltAz( skymap->LSTh, geo->lat() );
		GetData()->Venus->pos()->RADecToAltAz( skymap->LSTh, geo->lat() );
		GetData()->Mars->pos()->RADecToAltAz( skymap->LSTh, geo->lat() );
		GetData()->Jupiter->pos()->RADecToAltAz( skymap->LSTh, geo->lat() );
		GetData()->Saturn->pos()->RADecToAltAz( skymap->LSTh, geo->lat() );
		GetData()->Uranus->pos()->RADecToAltAz( skymap->LSTh, geo->lat() );
		GetData()->Neptune->pos()->RADecToAltAz( skymap->LSTh, geo->lat() );
		GetData()->Pluto->pos()->RADecToAltAz( skymap->LSTh, geo->lat() );

		//Stars
		if ( GetOptions()->drawBSC ) {
			for ( unsigned int i=0; i<GetData()->starList.count(); ++i ) {
				if (isNewEpoch) GetData()->starList.at(i)->pos()->updateCoords( GetData()->CurrentEpoch, GetData()->Obliquity, GetData()->dObliq, GetData()->dEcLong );
				GetData()->starList.at(i)->pos()->RADecToAltAz( skymap->LSTh, geo->lat() );
			}
		}

		//Messier
		if ( GetOptions()->drawMessier || GetOptions()->drawMessImages ) {
			for ( unsigned int i=0; i<GetData()->messList.count(); ++i ) {
				if (isNewEpoch) GetData()->messList.at(i)->pos()->updateCoords( GetData()->CurrentEpoch, GetData()->Obliquity, GetData()->dObliq, GetData()->dEcLong );
				GetData()->messList.at(i)->pos()->RADecToAltAz( skymap->LSTh, geo->lat() );
			}
		}

		//NGC
		if ( GetOptions()->drawNGC ) {
			for ( unsigned int i=0; i<GetData()->ngcList.count(); ++i ) {
				if (isNewEpoch) GetData()->ngcList.at(i)->pos()->updateCoords( GetData()->CurrentEpoch, GetData()->Obliquity, GetData()->dObliq, GetData()->dEcLong );
				GetData()->ngcList.at(i)->pos()->RADecToAltAz( skymap->LSTh, geo->lat() );
			}
		}

		//IC
		if ( GetOptions()->drawIC ) {
			for ( unsigned int i=0; i<GetData()->icList.count(); ++i ) {
				if (isNewEpoch) GetData()->icList.at(i)->pos()->updateCoords( GetData()->CurrentEpoch, GetData()->Obliquity, GetData()->dObliq, GetData()->dEcLong );
				GetData()->icList.at(i)->pos()->RADecToAltAz( skymap->LSTh, geo->lat() );
			}
		}

		//Milky Way
		if ( GetOptions()->drawMilkyWay ) {
			for ( unsigned int j=0; j<11; ++j ) {
				for ( unsigned int i=0; i<GetData()->MilkyWay[j].count(); ++i) {
					if (isNewEpoch) GetData()->MilkyWay[j].at(i)->updateCoords( GetData()->CurrentEpoch, GetData()->Obliquity, GetData()->dObliq, GetData()->dEcLong );
					GetData()->MilkyWay[j].at(i)->RADecToAltAz( skymap->LSTh, geo->lat() );
				}
			}
		}

		//CLines
		if ( GetOptions()->drawConstellLines ) {
			for ( unsigned int i=0; i<GetData()->clineList.count(); ++i) {
				if (isNewEpoch) GetData()->clineList.at(i)->updateCoords( GetData()->CurrentEpoch, GetData()->Obliquity, GetData()->dObliq, GetData()->dEcLong );
				GetData()->clineList.at(i)->RADecToAltAz( skymap->LSTh, geo->lat() );
			}
		}

		//CNames
		if ( GetOptions()->drawConstellNames ) {
			for ( unsigned int i=0; i<GetData()->cnameList.count(); ++i ) {
				if (isNewEpoch) GetData()->cnameList.at(i)->pos()->updateCoords( GetData()->CurrentEpoch, GetData()->Obliquity, GetData()->dObliq, GetData()->dEcLong );
				GetData()->cnameList.at(i)->pos()->RADecToAltAz( skymap->LSTh, geo->lat() );
			}
		}

		//Celestial Equator
		if ( GetOptions()->drawEquator ) {
			for ( unsigned int i=0; i<GetData()->Equator.count(); ++i ) {
				GetData()->Equator.at(i)->RADecToAltAz( skymap->LSTh, geo->lat() );
			}
		}

		//Ecliptic
		if ( GetOptions()->drawEcliptic ) {
			for ( unsigned int i=0; i<GetData()->Ecliptic.count(); ++i ) {
				GetData()->Ecliptic.at(i)->RADecToAltAz( skymap->LSTh, geo->lat() );
			}
		}

		//Horizon: differnet than the others; Alt & Az remain constant, RA, Dec must keep up
		if ( GetOptions()->drawHorizon || GetOptions()->drawGround ) {
			for ( unsigned int i=0; i<GetData()->Horizon.count(); ++i ) {
				GetData()->Horizon.at(i)->AltAzToRADec( skymap->LSTh, geo->lat() );
			}
		}

		skymap->oldfocus.set( skymap->focus.getRA(), skymap->focus.getDec() );
		skymap->oldfocus.RADecToAltAz( skymap->LSTh, geo->lat() );

		skymap->Update();
	}

}

SkyObject* KStars::getObjectNamed( QString name ) {
	if ( (name== "star") || (name== "nothing") || name.isEmpty() ) return NULL;
	if ( name== GetData()->Sun->name ) return GetData()->Sun;
	if ( name== GetData()->Moon->name ) return GetData()->Moon;
	if ( name== GetData()->Mercury->name ) return GetData()->Mercury;
	if ( name== GetData()->Venus->name ) return GetData()->Venus;
	if ( name== GetData()->Mars->name ) return GetData()->Mars;
	if ( name== GetData()->Jupiter->name ) return GetData()->Jupiter;
	if ( name== GetData()->Saturn->name ) return GetData()->Saturn;
	if ( name== GetData()->Uranus->name ) return GetData()->Uranus;
	if ( name== GetData()->Neptune->name ) return GetData()->Neptune;
	if ( name== GetData()->Pluto->name ) return GetData()->Pluto;

	//Stars
	for ( unsigned int i=0; i<GetData()->starList.count(); ++i ) {
		if ( name==GetData()->starList.at(i)->name ) return GetData()->starList.at(i);
	}

	//Messier
	for ( unsigned int i=0; i<GetData()->messList.count(); ++i ) {
		if ( name==GetData()->messList.at(i)->name ) return GetData()->messList.at(i);
	}

	//NGC
	for ( unsigned int i=0; i<GetData()->ngcList.count(); ++i ) {
		if ( name==GetData()->ngcList.at(i)->name ) return GetData()->ngcList.at(i);
	}

	//IC
	for ( unsigned int i=0; i<GetData()->icList.count(); ++i ) {
		if ( name==GetData()->icList.at(i)->name ) return GetData()->icList.at(i);
	}

	//Constellations
	for ( unsigned int i=0; i<GetData()->cnameList.count(); ++i ) {
		if ( name==GetData()->cnameList.at(i)->name ) return GetData()->cnameList.at(i);
	}

	//should never get here
	return NULL;
}

QString KStars::getDateString( QDate date ) {
  QString dummy;
  QString dateString = dummy.sprintf( "%02d / %02d / %04d",
			  date.month(), date.day(), date.year() );
  return dateString;
}

long double KStars::getJD( QDateTime t ) {
  int year = t.date().year();
  int month = t.date().month();
  int day = t.date().day();
  int hour = t.time().hour();
  int minute = t.time().minute();
  double second = t.time().second() + 0.001*t.time().msec();
  int m, y, A, B, C, D;

  if (month > 2) {
	  m = month;
	  y = year;
  } else {
	  y = year - 1;
	  m = month + 12;
  }

 /*  If the date is after 10/15/1582, then take Pope Gregory's modification
	 to the Julian calendar into account */

	 if (( year  >1582 ) ||
		 ( year ==1582 && month  >9 ) ||
		 ( year ==1582 && month ==9 && day >15 ))
	 {
		 A = int(y/100);
		 B = 2 - A + int(A/4);
	 } else {
		 B = 0;
	 }

  if (y < 0) {
	  C = int((365.25*y) - 0.75);
  } else {
	  C = int(365.25*y);
  }

  D = int(30.6001*(m+1));

  long double d = double(day) + (double(hour) + (double(minute) + second/60.0)/60.0)/24.0;
  long double jd = B + C + D + d + 1720994.5;

  return jd;
}

void KStars::updateEpoch( long double NewEpoch ) {
	//Several quantities change slowly with time:
	//Obliquity, Ecliptic Longitude,
	//eccentricity of Earth's Orbit
	//Earth's longitude of perihelion
	GetData()->CurrentEpoch = NewEpoch;
	GetData()->Obliquity = findObliquity( GetData()->CurrentEpoch );
	nutate( GetData()->CurrentEpoch ); //find dEcLong and dObliq
//	Obliquity += dObliq/3600.0;
}

double KStars::findObliquity( long double JD ) {
	double U = ( JD - J2000 )/3652500.0;
	double dObliq = -4680.93*U - 1.55*U*U + 1999.25*U*U*U
									- 51.38*U*U*U*U - 249.67*U*U*U*U*U
									- 39.05*U*U*U*U*U*U + 7.12*U*U*U*U*U*U*U
									+ 27.87*U*U*U*U*U*U*U*U + 5.79*U*U*U*U*U*U*U*U*U
									+ 2.45*U*U*U*U*U*U*U*U*U*U;
	double Obliq = 23.43929111 + dObliq/3600.0;
	return Obliq;
}

void KStars::nutate( long double JD ) {
	//This uses the pre-1984 theory of nutation.  The results
	//are accurate to 0.5 arcsec, should be fine for KStars.
	//calculations from "Practical Astronomy with Your Calculator"
	//by Peter Duffett-Smith
	//
	//Nutation is a small, quasi-periodic oscillation of the Earth's
	//spin axis (in addition to the more regular precession).  It
	//is manifested as a change in the zeropoint of the Ecliptic
	//Longitude (dEcLong) and a change in the Obliquity angle (dObliq)
	//see the Astronomy Help pages for more info.

	dms  L2, M2, O, O2;
	double T;
	double sin2L, cos2L, sin2M, cos2M;
	double sinO, cosO, sin2O, cos2O;

	T = ( JD - J2000 )/36525.0; //centuries since J2000.0
  O.setD( 125.04452 - 1934.136261*T + 0.0020708*T*T + T*T*T/450000.0 ); //ecl. long. of Moon's ascending node
	O2.setD( 2.0*O.getD() );
	L2.setD( 2.0*( 280.4665 + 36000.7698*T ) ); //twice mean ecl. long. of Sun
	M2.setD( 2.0*( 218.3165 + 481267.8813*T ) );//twice mean ecl. long. of Moon

	O.SinCos( sinO, cosO );
	O2.SinCos( sin2O, cos2O );
	L2.SinCos( sin2L, cos2L );
	M2.SinCos( sin2M, cos2M );

	GetData()->dEcLong = ( -17.2*sinO - 1.32*sin2L - 0.23*sin2M + 0.21*sin2O)/3600.0; //Ecl. long. correction
	GetData()->dObliq = (   9.2*cosO + 0.57*cos2L + 0.10*cos2M - 0.09*cos2O)/3600.0; //Obliq. correction
}

QTime KStars::UTtoLST( QDateTime UT, dms longitude) {
  QTime GST = UTtoGST( UT );
  QTime LST = GSTtoLST( GST, longitude );
  return LST;
}

QTime KStars::UTtoGST( QDateTime UT ) {
  QDateTime t0 = UT;
  t0.setTime( QTime(0,0,0) );
  long double jd0 = getJD( t0 );
  long double s = jd0 - 2451545.0;
  double t = s/36525.0;
  double t1 = 6.697374558 + 2400.051336*t + 0.000025862*t*t;

	 while (t1 >= 24.0) {t1 -= 24.0;}
	 while (t1 <   0.0) {t1 += 24.0;}

  double hr = double( UT.time().hour() );
  double mn = double( UT.time().minute() );
  double sc = double( UT.time().second() ) + double ( 0.001 * UT.time().msec() );

  double t2 = (hr + ( mn + sc/60.0)/60.0)*1.002737909;
  double gsth = t1 + t2;

  while (gsth < 0.0) gsth += 24.0;
  while (gsth >24.0) gsth -= 24.0;

  int gh = int(gsth);
  int gm = int((gsth - gh)*60.0);
  int gs = int(((gsth - gh)*60.0 - gm)*60.0);

  QTime gst(gh, gm, gs);

  return gst;
}

QTime KStars::GSTtoLST( QTime GST, dms longitude) {
  double lsth = double(GST.hour()) + (double(GST.minute()) +
			   double(GST.second())/60.0)/60.0 + longitude.getD()/15.0;

  while (lsth < 0.0) lsth += 24.0;
  while (lsth >24.0) lsth -= 24.0;

  int lh = int(lsth);
  int lm = int((lsth - lh)*60.0);
  int ls = int(((lsth - lh)*60.0 - lm)*60.0);

  QTime lst(lh, lm, ls);

  return lst;
}

void KStars::closeEvent (QCloseEvent *e)
{
	if (!skymap->runningDownloads())
	{
//		emit closeAllWindows();
		e->accept();
		kapp->quit();	// quits the program
	}
	else
		e->ignore();
}

#include "kstars.moc"
