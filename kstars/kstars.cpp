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
#include <klocale.h>

#include <stdio.h>
#include <stdlib.h>
#include <iostream.h>
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

	//resize( 640, 600 );
	initMenuBar();
	initToolBar();
	initStatusBar();
	initOptions();
	initLocation();

// create the widgets
	QWidget *centralWidget = new QWidget( this );
	setCentralWidget( centralWidget );
	infoPanel = new QFrame( centralWidget );
	skymap = new SkyMap( centralWidget );
	skymap->QWidget::setFocus();		// get focus of keyboard and mouse actions (for example zoom in with +)
	skymap->setMinimumSize( 380, 250 );
	skymap->setSizePolicy( QSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding ) );

// create the layout of the central widget
	QVBoxLayout *topLayout = new QVBoxLayout( centralWidget );
	topLayout->addWidget( infoPanel );
	topLayout->addWidget( skymap );

/*	//time settings that we couldn't do in KStarsSplash:
	data()->UTime = data()->now.addSecs( int(geo()->TZ()*-3600) );
	data()->LST   = UTtoLST( data()->UTime, geo()->lng() );
	data()->LSTh.setH( data()->LST.hour(), data()->LST.minute(), data()->LST.second() );

	data()->CurrentDate = data()->getJD( data()->UTime );
	data()->LastSkyUpdate = data()->CurrentDate;
	updateEpoch( data()->CurrentDate );
*/

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
	LT->setText( data()->LTime.time().toString() );
	UT = new QLabel( data()->UTime.time().toString(), infoPanel );
	ST = new QLabel( "00:00:00", infoPanel );
	LT->setPalette( pal );
	UT->setPalette( pal );
	ST->setPalette( pal );
	LT->setFont( ipFont );
	UT->setFont( ipFont );
	ST->setFont( ipFont );

	LTDate = new QLabel( data()->locale->formatDate( data()->LTime.date(), true ), infoPanel );
	UTDate = new QLabel( data()->locale->formatDate( data()->UTime.date(), true ), infoPanel );
	KLocale localjd;
	JD = new QLabel( "JD: " + localjd.formatNumber( data()->CurrentEpoch, 2 ), infoPanel );
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
	if ( geo()->province().isEmpty() ) {
		PlaceName->setText( geo()->translatedName() + ", " + geo()->translatedCountry() );
	} else {
		PlaceName->setText( geo()->translatedName() + ", " + geo()->translatedProvince() + ",  " + geo()->translatedCountry() );
	}
	LongLabel = new QLabel( i18n( "Longitude", "Long: " ), infoPanel );
	LatLabel = new QLabel( i18n( "Latitude", "Lat:  " ), infoPanel );
	KLocale localeLong;
	Long = new QLabel( localeLong.formatNumber( geo()->lng().Degrees(),3) , infoPanel );
	Long->setAlignment( AlignRight );
	KLocale localeLat;
	Lat = new QLabel( localeLat.formatNumber( geo()->lat().Degrees(),3 ), infoPanel );
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
	resize( options()->windowWidth, options()->windowHeight );

	//Start the clock; intitialize time to system clock
	data()->SysJD_Mark = data()->getJD( QDateTime::currentDateTime() );
	data()->SkyJD_Mark = data()->SysJD_Mark;
	
	tmr = new QTimer( this );
	tmr->start( TIMER_INTERVAL, FALSE );
	QObject::connect( tmr, SIGNAL( timeout() ), this, SLOT( updateTime() ) );

//Try updateTime() and updateEpoch() here to initialize CurrentDate and CurrentEpoch...
	data()->CurrentEpoch = 0.0; //guarantee that updateTime() calls updateEpoch()

	updateTime();
	initAltAz();

	SkyPoint newPoint;
	if ( useDefaultOptions ) {
		newPoint.setAz( options()->focusRA );
		newPoint.setAlt( options()->focusDec + 0.0001 );
		newPoint.HorizontalToEquatorial( data()->LSTh, geo()->lat() );
	} else {
		newPoint.set( (double)options()->focusRA, (double)options()->focusDec );
	}

//Set focus of Skymap.
//if user was tracking last time, track on same object now.
	if ( options()->isTracking ) {

    //Set default position in case focus object is below horizon
		SkyPoint DefaultFocus;
		DefaultFocus.setAz( 180.0 );
		DefaultFocus.setAlt( 45.0 );
		DefaultFocus.HorizontalToEquatorial( data()->LSTh, geo()->lat() );
		skymap->setFocus( &DefaultFocus );

		if ( (options()->focusObject== i18n( "star" ) ) ||
		     (options()->focusObject== i18n( "nothing" ) ) ) {
			skymap->setClickedPoint( &newPoint );
		} else {
			skymap->setClickedObject( getObjectNamed( options()->focusObject ) );
			if ( skymap->clickedObject() ) {
				skymap->setClickedPoint( skymap->clickedObject()->pos() );
			} else {
				skymap->setClickedPoint( &newPoint );
			}
		}
	} else {
		skymap->setClickedPoint( &newPoint );
	}

	skymap->slotCenter();

	data()->HourAngle.setH( data()->LSTh.Hours() - skymap->focus()->ra().Hours() );

	skymap->setOldFocus( skymap->focus() );
	skymap->oldfocus()->setAz( skymap->focus()->az() );
	skymap->oldfocus()->setAlt( skymap->focus()->alt() );
}

KStars::~KStars()
{
	//Sync the config file
	kapp->config()->setGroup( "Location" );
	kapp->config()->writeEntry( "City", options()->CityName );
	kapp->config()->writeEntry( "Province", options()->ProvinceName );
	kapp->config()->writeEntry( "Country", options()->CountryName );
	kapp->config()->setGroup( "View" );
	kapp->config()->writeEntry( "SkyColor", 	options()->colorSky );
	kapp->config()->writeEntry( "MWColor", 		options()->colorMW );
	kapp->config()->writeEntry( "EqColor", 		options()->colorEq );
	kapp->config()->writeEntry( "EclColor", 		options()->colorEcl );
	kapp->config()->writeEntry( "HorzColor", 	options()->colorHorz );
	kapp->config()->writeEntry( "GridColor", 	options()->colorGrid );
	kapp->config()->writeEntry( "MessColor", 	options()->colorMess );
	kapp->config()->writeEntry( "NGCColor", 	options()->colorNGC );
	kapp->config()->writeEntry( "ICColor", 		options()->colorIC );
	kapp->config()->writeEntry( "CLineColor", options()->colorCLine );
	kapp->config()->writeEntry( "CNameColor", options()->colorCName );
	kapp->config()->writeEntry( "SNameColor", options()->colorSName );
	kapp->config()->writeEntry( "HSTColor", 	options()->colorHST );
	kapp->config()->writeEntry( "StarColorMode", options()->starColorMode );
	kapp->config()->writeEntry( "StarColorsIntensity", options()->starColorIntensity );
	kapp->config()->writeEntry( "ShowBSC", 		options()->drawBSC );
	kapp->config()->writeEntry( "ShowMess", 	options()->drawMessier );
	kapp->config()->writeEntry( "ShowMessImages", 	options()->drawMessImages );
	kapp->config()->writeEntry( "ShowNGC", 		options()->drawNGC );
	kapp->config()->writeEntry( "ShowIC", 		options()->drawIC );
	kapp->config()->writeEntry( "ShowCLines", options()->drawConstellLines );
	kapp->config()->writeEntry( "ShowCNames", options()->drawConstellNames );
	kapp->config()->writeEntry( "UseLatinConstellationNames", options()->useLatinConstellNames );
	kapp->config()->writeEntry( "UseLocalConstellationNames", options()->useLocalConstellNames );
	kapp->config()->writeEntry( "UseAbbrevConstellationNames", options()->useAbbrevConstellNames );
	kapp->config()->writeEntry( "ShowMilkyWay", options()->drawMilkyWay );
	kapp->config()->writeEntry( "ShowGrid", options()->drawGrid );
	kapp->config()->writeEntry( "ShowEquator", options()->drawEquator );
	kapp->config()->writeEntry( "ShowEcliptic", options()->drawEcliptic );
	kapp->config()->writeEntry( "ShowHorizon", options()->drawHorizon );
	kapp->config()->writeEntry( "ShowGround", options()->drawGround );
	kapp->config()->writeEntry( "ShowSun", 		options()->drawSun );
	kapp->config()->writeEntry( "ShowMoon", 	options()->drawMoon );
	kapp->config()->writeEntry( "ShowMercury", 	options()->drawMercury );
	kapp->config()->writeEntry( "ShowVenus", 	options()->drawVenus );
	kapp->config()->writeEntry( "ShowMars", 	options()->drawMars );
	kapp->config()->writeEntry( "ShowJupiter", 	options()->drawJupiter );
	kapp->config()->writeEntry( "ShowSaturn", 	options()->drawSaturn );
	kapp->config()->writeEntry( "ShowUranus", 	options()->drawUranus );
	kapp->config()->writeEntry( "ShowNeptune", 	options()->drawNeptune );
	kapp->config()->writeEntry( "ShowPluto", 	options()->drawPluto );
	kapp->config()->writeEntry( "IsTracking", 	options()->isTracking );
	if ( skymap->foundObject() != NULL ) {
		kapp->config()->writeEntry( "FocusObject",  skymap->foundObject()->name() );
	} else {
		kapp->config()->writeEntry( "FocusObject", i18n( "nothing" ) );
	}
	kapp->config()->writeEntry( "UseAltAz", 	options()->useAltAz );
	kapp->config()->writeEntry( "FocusRA", skymap->focus()->ra().Hours() );
	kapp->config()->writeEntry( "FocusDec", skymap->focus()->dec().Degrees() );
	kapp->config()->writeEntry( "ZoomLevel", data()->ZoomLevel );
	kapp->config()->writeEntry( "windowWidth", width() );
	kapp->config()->writeEntry( "windowHeight", height() );
	kapp->config()->writeEntry( "magLimitDrawStar", 	 options()->magLimitDrawStar );
	kapp->config()->writeEntry( "magLimitDrawStarInfo",options()->magLimitDrawStarInfo );
	kapp->config()->writeEntry( "drawStarName", 			 options()->drawStarName );
	kapp->config()->writeEntry( "drawStarMagnitude",   options()->drawStarMagnitude );
	kapp->config()->sync();

	// remove data object
	delete kstarsData;
	//kstarsData = 0;

	delete Location;
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
	connect( TimeStep, SIGNAL( valueChanged( int ) ), this, SLOT( changeTimeStep() ) );

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
	QString s = "00:00:00,   +00:00:00";

	statusBar()->insertItem( s, 1, 1, true );
	statusBar()->setItemAlignment( 1, AlignRight | AlignVCenter );
	statusBar()->setItemFixed( 1, -1 );
}

/** Menu Slot Functions **/
void KStars::mSetTimeToNow() {
	data()->SysJD_Mark = data()->getJD( QDateTime::currentDateTime() );
	data()->SkyJD_Mark = data()->SysJD_Mark;

//	data()->LTime.setTime( QTime::currentTime() );
//	data()->LTime.setDate( QDate::currentDate() );
//	data()->UTime.setTime( data()->LTime.time() );
//	data()->UTime.setDate( data()->LTime.date() );
//	data()->UTime = data()->UTime.addSecs( int( geo()->TZ()*-3600) );

//	data()->then = QDateTime::currentDateTime();
	updateTime();
}

void KStars::initOptions()
{
	if ( kapp->config()->hasGroup( "Location" ) ) useDefaultOptions = false;
	else useDefaultOptions = true;

	// Get initial Location from config()
	kapp->config()->setGroup( "Location" );
	options()->CityName = kapp->config()->readEntry( "City", "Greenwich" );

	if ( kapp->config()->readEntry( "State", "" ).length() ) { //old version of config file
		options()->ProvinceName = kapp->config()->readEntry( "State", "" );
		options()->CountryName = kapp->config()->readEntry( "State", "United Kingdom" );
	} else {
		options()->ProvinceName = kapp->config()->readEntry( "Province", "" );
		options()->CountryName = kapp->config()->readEntry( "Country", "United Kingdom" );
	}

	kapp->config()->setGroup( "View" );
	options()->colorSky 		= kapp->config()->readEntry( "SkyColor", "#002" );
//	options()->colorStar 	= kapp->config()->readEntry( "StarColor", "#FFF" );
	options()->colorMW 		= kapp->config()->readEntry( "MWColor", "#123" );
	options()->colorEq 		= kapp->config()->readEntry( "EqColor", "#FFF" );
	options()->colorEcl		= kapp->config()->readEntry( "EclColor", "#663" );
	options()->colorHorz 	= kapp->config()->readEntry( "HorzColor", "#5A3" );
	options()->colorGrid 	= kapp->config()->readEntry( "GridColor", "#456" );
	options()->colorMess 	= kapp->config()->readEntry( "MessColor", "#0F0" );
	options()->colorNGC 		= kapp->config()->readEntry( "NGCColor", "#066" );
	options()->colorIC 		= kapp->config()->readEntry( "ICColor", "#439" );
	options()->colorCLine 	= kapp->config()->readEntry( "CLineColor", "#555" );
	options()->colorCName 	= kapp->config()->readEntry( "CNameColor", "#AA7" );
	options()->colorSName 	= kapp->config()->readEntry( "SNameColor", "#7AA" );
	options()->colorHST 		= kapp->config()->readEntry( "HSTColor", "#A00" );
	options()->drawBSC 		= kapp->config()->readBoolEntry( "ShowBSC", true );
	options()->drawMessier = kapp->config()->readBoolEntry( "ShowMess", true );
	options()->drawMessImages = kapp->config()->readBoolEntry( "ShowMessImages", true );
	options()->drawNGC 		= kapp->config()->readBoolEntry( "ShowNGC", true );
	options()->drawIC 			= kapp->config()->readBoolEntry( "ShowIC", true );
	options()->drawConstellLines = kapp->config()->readBoolEntry( "ShowCLines", true );
	options()->drawConstellNames = kapp->config()->readBoolEntry( "ShowCNames", true );
	options()->useLatinConstellNames = kapp->config()->readBoolEntry( "UseLatinConstellationNames", true );
	options()->useLocalConstellNames = kapp->config()->readBoolEntry( "UseLocalConstellationNames", false );
	options()->useAbbrevConstellNames = kapp->config()->readBoolEntry( "UseAbbrevConstellationNames", false );
	options()->drawMilkyWay = kapp->config()->readBoolEntry( "ShowMilkyWay", true );
	options()->drawGrid = kapp->config()->readBoolEntry( "ShowGrid", true );
	options()->drawEquator = kapp->config()->readBoolEntry( "ShowEquator", true );
	options()->drawEcliptic = kapp->config()->readBoolEntry( "ShowEcliptic", true );
	options()->drawHorizon = kapp->config()->readBoolEntry( "ShowHorizon", true );
	options()->drawGround = kapp->config()->readBoolEntry( "ShowGround", true );
	options()->drawSun = kapp->config()->readBoolEntry( "ShowSun", true );
	options()->drawMoon = kapp->config()->readBoolEntry( "ShowMoon", true );
	options()->drawMercury = kapp->config()->readBoolEntry( "ShowMercury", true );
	options()->drawVenus = kapp->config()->readBoolEntry( "ShowVenus", true );
	options()->drawMars = kapp->config()->readBoolEntry( "ShowMars", true );
	options()->drawJupiter = kapp->config()->readBoolEntry( "ShowJupiter", true );
	options()->drawSaturn = kapp->config()->readBoolEntry( "ShowSaturn", true );
	options()->drawUranus = kapp->config()->readBoolEntry( "ShowUranus", true );
	options()->drawNeptune = kapp->config()->readBoolEntry( "ShowNeptune", true );
	options()->drawPluto = kapp->config()->readBoolEntry( "ShowPluto", true );
	options()->useAltAz = kapp->config()->readBoolEntry( "UseAltAz", true );
	options()->isTracking = kapp->config()->readBoolEntry( "IsTracking", false );
	options()->focusObject = kapp->config()->readEntry( "FocusObject", "nothing" );
	options()->focusDec = kapp->config()->readDoubleNumEntry( "FocusDec", 45.0 );
	options()->focusRA = kapp->config()->readDoubleNumEntry( "FocusRA", 180.0 );
//	options()->magLimitDrawStar = kapp->config()->readDoubleNumEntry( "magLimitDrawStar", 8.0 );		// readed in KStarsOptions()
	options()->magLimitDrawStarInfo = kapp->config()->readDoubleNumEntry( "magLimitDrawStarInfo", 2.0 );
	options()->drawStarName = kapp->config()->readBoolEntry( "drawStarName", false );
	options()->drawStarMagnitude = kapp->config()->readBoolEntry( "drawStarMagnitude", false );
	options()->windowWidth = kapp->config()->readNumEntry( "windowWidth", 600 );
	options()->windowHeight = kapp->config()->readNumEntry( "windowHeight", 600 );
	options()->starColorMode = kapp->config()->readNumEntry( "StarColorMode", 0 );
	options()->starColorIntensity = kapp->config()->readNumEntry ("StarColorsIntensity", 4);
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
// With KDE 3 this needs explicite set (why?)
	((KStars*) kapp)->setLocation( Location );

	if ( geo()->lat().Degrees()==  90.0 ) geo()->lat().setD(  89.9999 );
	if ( geo()->lat().Degrees()== -90.0 ) geo()->lat().setD( -89.9999 );
}

void KStars::initAltAz()
{
	// Recompute Alt, Az coords for all objects. This was not possible
	// within loading the KStarsData object. Do it now.
	// Solar System
	data()->Sun->pos()->EquatorialToHorizontal( data()->LSTh, geo()->lat() );
	data()->Mercury->pos()->EquatorialToHorizontal( data()->LSTh, geo()->lat() );
	data()->Venus->pos()->EquatorialToHorizontal( data()->LSTh, geo()->lat() );
	data()->Mars->pos()->EquatorialToHorizontal( data()->LSTh, geo()->lat() );
	data()->Jupiter->pos()->EquatorialToHorizontal( data()->LSTh, geo()->lat() );
	data()->Saturn->pos()->EquatorialToHorizontal( data()->LSTh, geo()->lat() );
	data()->Uranus->pos()->EquatorialToHorizontal( data()->LSTh, geo()->lat() );
	data()->Neptune->pos()->EquatorialToHorizontal( data()->LSTh, geo()->lat() );
	data()->Pluto->pos()->EquatorialToHorizontal( data()->LSTh, geo()->lat() );

	// Stars
	for (SkyObject *obj = data()->starList.first(); obj; obj = data()->starList.next()) {
		obj->pos()->updateCoords( data()->CurrentEpoch, data()->Obliquity, data()->dObliq, data()->dEcLong );
		obj->pos()->EquatorialToHorizontal( data()->LSTh, geo()->lat() );
	}

	// Messier
	for (SkyObject *obj = data()->messList.first(); obj; obj = data()->messList.next()) {
		obj->pos()->updateCoords( data()->CurrentEpoch, data()->Obliquity, data()->dObliq, data()->dEcLong );
		obj->pos()->EquatorialToHorizontal( data()->LSTh, geo()->lat() );
	}

	// NGC
	for (SkyObject *obj = data()->ngcList.first(); obj; obj = data()->ngcList.next()) {
		obj->pos()->updateCoords( data()->CurrentEpoch, data()->Obliquity, data()->dObliq, data()->dEcLong );
		obj->pos()->EquatorialToHorizontal( data()->LSTh, geo()->lat() );
	}

	// IC
	for (SkyObject *obj = data()->icList.first(); obj; obj = data()->icList.next()) {
		obj->pos()->updateCoords( data()->CurrentEpoch, data()->Obliquity, data()->dObliq, data()->dEcLong );
		obj->pos()->EquatorialToHorizontal( data()->LSTh, geo()->lat() );
	}

	// Milky Way
	for ( unsigned int j=0; j<11; ++j ) {
		for (SkyPoint *obj = data()->MilkyWay[j].first(); obj; obj = data()->MilkyWay[j].next()) {
			obj->updateCoords( data()->CurrentEpoch, data()->Obliquity, data()->dObliq, data()->dEcLong );
			obj->EquatorialToHorizontal( data()->LSTh, geo()->lat() );
		}
	}

	// CLines
	for (SkyPoint *obj = data()->clineList.first(); obj; obj = data()->clineList.next()) {
		obj->updateCoords( data()->CurrentEpoch, data()->Obliquity, data()->dObliq, data()->dEcLong );
		obj->EquatorialToHorizontal( data()->LSTh, geo()->lat() );
	}

	// CNames
	for (SkyObject *obj = data()->cnameList.first(); obj; obj = data()->cnameList.next()) {
		obj->pos()->updateCoords( data()->CurrentEpoch, data()->Obliquity, data()->dObliq, data()->dEcLong );
		obj->pos()->EquatorialToHorizontal( data()->LSTh, geo()->lat() );
	}

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
		double ELong, ELat;
		ELong = point->ra().Degrees();
		ELat = 0.0;
		o = new SkyPoint( 0.0, 0.0 );
		o->setEcliptic( ELong, ELat, data()->CurrentDate );
		o->EquatorialToHorizontal( data()->LSTh, geo()->lat() );
		data()->Ecliptic.append( o );
	}
}

void KStars::mSetTime() {
	TimeDialog timedialog ( data()->LTime, this );

	if ( timedialog.exec() == QDialog::Accepted ) {

		if (tmr->isActive() ) {
			tmr->stop();
			actTimeRun->setIconSet( BarIcon( "1rightarrow" ) );
			actTimeRun->setText( i18n( "Start &Clock" ) );
			actTimeRun->setToolTip( i18n( "Start Clock" ) );
		}

		QTime newTime( timedialog.selectedTime() );
		QDate newDate( timedialog.selectedDate() );

		data()->SysJD_Mark = data()->getJD( QDateTime::currentDateTime() );
		data()->SkyJD_Mark = data()->getJD( QDateTime( newDate, newTime ) );
		
		//Make sure Moon, planets, and sky objects are updated immediately
		data()->LastMoonUpdate = data()->CurrentDate - 1.0;
		data()->LastPlanetUpdate = data()->CurrentDate - 1.0;
		data()->LastSkyUpdate = data()->CurrentDate - 1.0;

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
		data()->SysJD_Mark = data()->getJD( QDateTime::currentDateTime() );
		data()->SkyJD_Mark = data()->getJD( data()->LTime );

		tmr->start( TIMER_INTERVAL, FALSE );
		actTimeRun->setIconSet( BarIcon( "player_pause" ) );
		actTimeRun->setText( i18n( "Stop &Clock" ) );
		actTimeRun->setToolTip( i18n( "Stop Clock" ) );
	}
}

void KStars::mZenith() {
	skymap->focus()->setAlt( 90.0 );
	skymap->focus()->HorizontalToEquatorial( data()->LSTh, geo()->lat() );
	data()->HourAngle.setH( data()->LSTh.Hours() - skymap->focus()->ra().Hours() );
	skymap->Update();
}

void KStars::mZoomIn() {
	actZoomOut->setEnabled (true);
	if ( data()->ZoomLevel < MAXZOOMLEVEL ) {
		++data()->ZoomLevel;
		skymap->Update();
	}
	if ( data()->ZoomLevel == MAXZOOMLEVEL )
		actZoomIn->setEnabled (false);
}

void KStars::mZoomOut() {
	actZoomIn->setEnabled (true);
	if ( data()->ZoomLevel > MINZOOMLEVEL ) {
		--data()->ZoomLevel;
		skymap->Update();
	}
	if ( data()->ZoomLevel == MINZOOMLEVEL )
		actZoomOut->setEnabled (false);
}

void KStars::mFind() {
	FindDialog finddialog (this);
	if ( finddialog.exec() == QDialog::Accepted && finddialog.currentItem() ) {
		skymap->setClickedObject( finddialog.currentItem()->objName()->skyObject() );
		skymap->setClickedPoint( skymap->clickedObject()->pos() );
		skymap->slotCenter();

	}
}

void KStars::mTrack() {
	if ( options()->isTracking ) {
		options()->isTracking = false;
		actTrack->setIconSet( BarIcon( "unlock" ) );
		skymap->setClickedObject( NULL );
		skymap->setFoundObject( NULL );//no longer tracking foundObject
	} else {
		options()->isTracking = true;
		actTrack->setIconSet( BarIcon( "lock" ) );
	}
}

void KStars::mViewOps() {
	// save options for cancel
	data()->saveOptions();

	ViewOpsDialog viewopsdialog (this);
	// ask for the new options
	if ( viewopsdialog.exec() != QDialog::Accepted ) {
		// cancelled
		data()->restoreOptions();
		skymap->Update();
	}
}

void KStars::mGeoLocator() {
	LocationDialog locationdialog (this);
	if ( locationdialog.exec() == QDialog::Accepted ) {
		if ( !locationdialog.selectedCityName().isEmpty() ) { //user closed the location dialog without adding their new city;
			locationdialog.addCity();                   //call addCity() for them!
		}

	 	int ii = locationdialog.getCityIndex();
 		if ( ii >= 0 ) {
 			geo()->reset( data()->geoList.at(ii) );
			options()->CityName = geo()->name();
			options()->ProvinceName = geo()->province();
			options()->CountryName = geo()->country();
			if ( geo()->province().isEmpty() )
	 			PlaceName->setText( geo()->translatedName() + ",  " + geo()->translatedCountry() );
			else
	 			PlaceName->setText( geo()->translatedName() + ", " + geo()->translatedProvince() + ",  " + geo()->translatedCountry() );

 			Long->setText( QString::number( geo()->lng().Degrees(), 'f', 3 ) );
 			Lat->setText( QString::number( geo()->lat().Degrees(), 'f', 3 ) );

 			// Adjust Local time for new time zone
	 		data()->LTime.setDate( data()->UTime.date() );
 			data()->LTime.setTime( data()->UTime.time() );
 			data()->LTime = data()->LTime.addSecs( int(geo()->TZ()*3600) );

			data()->LST = UTtoLST( data()->UTime, geo()->lng() );
			data()->LSTh.setH( data()->LST.hour(), data()->LST.minute(), data()->LST.second() );
			data()->HourAngle.setH( data()->LSTh.Hours() - skymap->focus()->ra().Hours() );

      //need to recompute Alt/Az coordinates of all objects, so
			//adjust LastSkyUpdate to ensure computation.  Then
			//explicitly call updateTime()
			data()->LastSkyUpdate -= 1.0; // a full day, should be plenty
			updateTime();
	 	}
 	}
}

void KStars::updateTime( void ) {
	data()->SysJD = data()->getJD( QDateTime::currentDateTime() );
	data()->SysJD_Elapsed = data()->SysJD - data()->SysJD_Mark;

	//advance clock by factor*(time elapsed since system time last marked)
	float factor = ((TimeSpinBox *)toolBar()->getWidget( idSpinBox ))->text().toFloat();
	data()->LTime = data()->getDateTime( data()->SkyJD_Mark + factor*data()->SysJD_Elapsed );
	data()->UTime = data()->LTime.addSecs( int( -3600*geo()->TZ() ) );

	LT->setText( data()->LTime.time().toString() );
	UT->setText( data()->UTime.time().toString() );

	LTDate->setText( data()->locale->formatDate( data()->LTime.date(), true ) );
	UTDate->setText( data()->locale->formatDate( data()->UTime.date(), true ) );

	data()->CurrentDate = data()->getJD( data()->UTime );
	JD->setText( "JD: " + QString("%1").arg( data()->CurrentDate, 10, 'f', 2) );

	bool isNewEpoch = false;
	if ( fabs( data()->CurrentDate - data()->CurrentEpoch ) > 365.25 ) { //update epoch-dependent numbers annually
		updateEpoch( data()->CurrentDate );
		isNewEpoch = true;
	}

  data()->LST = UTtoLST( data()->UTime, geo()->lng() );

	QString dummy;
	QString STString = dummy.sprintf( "%02d:%02d:%02d",
			  data()->LST.hour(), data()->LST.minute(), data()->LST.second() );
	ST->setText( STString );

	data()->LSTh.setH( data()->LST.hour(), data()->LST.minute(), data()->LST.second() );

  // Update positions of objects, if necessary
  // Sun and Planet positions change rather slowly, so only update them twice daily
  if ( fabs( data()->CurrentDate - data()->LastPlanetUpdate ) > 0.5 ) {
  	data()->LastPlanetUpdate = data()->CurrentDate;

		data()->Sun->findPosition( data()->CurrentDate );
		data()->Earth->findPosition( data()->CurrentDate );
		data()->Mercury->findPosition( data()->CurrentDate, data()->Earth );
		data()->Venus->findPosition( data()->CurrentDate, data()->Earth );
		data()->Mars->findPosition( data()->CurrentDate, data()->Earth );
		data()->Jupiter->findPosition( data()->CurrentDate, data()->Earth );
		data()->Saturn->findPosition( data()->CurrentDate, data()->Earth );
		data()->Uranus->findPosition( data()->CurrentDate, data()->Earth );
		data()->Neptune->findPosition( data()->CurrentDate, data()->Earth );
		data()->Pluto->findPosition( data()->CurrentDate, data()->Earth );

		//Recompute the Ecliptic
		data()->Ecliptic.clear();
		for ( unsigned int i=0; i<data()->Equator.count(); ++i ) {
			double ELong, ELat;
			ELong = data()->Equator.at(i)->ra().Degrees();
			ELat = 0.0;
			SkyPoint *o = new SkyPoint( 0.0, 0.0 );
			o->setEcliptic( ELong, ELat, data()->CurrentDate );
			data()->Ecliptic.append( o );
		}
	}

	// Moon moves ~30 arcmin/hr, so update its position every minute.
	if ( fabs( data()->CurrentDate - data()->LastMoonUpdate ) > 0.00069444 ) {
		data()->LastMoonUpdate = data()->CurrentDate;
		data()->Moon->findPosition( data()->CurrentDate, geo()->lat(), data()->LSTh );
		data()->Moon->findPhase( data()->Sun );
	}


	//update focus
	if ( options()->isTracking ) {
		//if tracking on a planet, update focus to keep up with changing position.
		bool onPlanet = false;
		if ( skymap->foundObject() == data()->Sun ) onPlanet = true;
		if ( skymap->foundObject() == data()->Moon ) onPlanet = true;
		if ( skymap->foundObject() == data()->Mercury ) onPlanet = true;
		if ( skymap->foundObject() == data()->Venus ) onPlanet = true;
		if ( skymap->foundObject() == data()->Mars ) onPlanet = true;
		if ( skymap->foundObject() == data()->Jupiter ) onPlanet = true;
		if ( skymap->foundObject() == data()->Saturn ) onPlanet = true;
		if ( skymap->foundObject() == data()->Uranus ) onPlanet = true;
		if ( skymap->foundObject() == data()->Neptune ) onPlanet = true;
		if ( skymap->foundObject() == data()->Pluto ) onPlanet = true;

		if ( onPlanet) skymap->setFocus( skymap->foundObject()->pos() );

		data()->HourAngle.setH( data()->LSTh.Hours() - skymap->focus()->ra().Hours() );
		skymap->focus()->EquatorialToHorizontal( data()->LSTh, geo()->lat() );
	} else {
		skymap->focus()->setRA( data()->LSTh.Hours() - data()->HourAngle.Hours() );
		skymap->focus()->EquatorialToHorizontal( data()->LSTh, geo()->lat() );
  }

	showFocusCoords();

	//Update Alt/Az coordinates.  Timescale varies with zoom level
	if ( isNewEpoch || fabs( data()->CurrentDate - data()->LastSkyUpdate) > 0.25/skymap->getPixelScale() ) {
		data()->LastSkyUpdate = data()->CurrentDate;

		//Recompute Alt, Az coords for all objects
		//Planets
		data()->Sun->pos()->EquatorialToHorizontal( data()->LSTh, geo()->lat() );
		data()->Moon->pos()->EquatorialToHorizontal( data()->LSTh, geo()->lat() );
		data()->Mercury->pos()->EquatorialToHorizontal( data()->LSTh, geo()->lat() );
		data()->Venus->pos()->EquatorialToHorizontal( data()->LSTh, geo()->lat() );
		data()->Mars->pos()->EquatorialToHorizontal( data()->LSTh, geo()->lat() );
		data()->Jupiter->pos()->EquatorialToHorizontal( data()->LSTh, geo()->lat() );
		data()->Saturn->pos()->EquatorialToHorizontal( data()->LSTh, geo()->lat() );
		data()->Uranus->pos()->EquatorialToHorizontal( data()->LSTh, geo()->lat() );
		data()->Neptune->pos()->EquatorialToHorizontal( data()->LSTh, geo()->lat() );
		data()->Pluto->pos()->EquatorialToHorizontal( data()->LSTh, geo()->lat() );

		//Stars
		if ( options()->drawBSC ) {
			for ( unsigned int i=0; i<data()->starList.count(); ++i ) {
				if (isNewEpoch) data()->starList.at(i)->pos()->updateCoords( data()->CurrentEpoch, data()->Obliquity, data()->dObliq, data()->dEcLong );
				data()->starList.at(i)->pos()->EquatorialToHorizontal( data()->LSTh, geo()->lat() );
			}
		}

		//Messier
		if ( options()->drawMessier || options()->drawMessImages ) {
			for ( unsigned int i=0; i<data()->messList.count(); ++i ) {
				if (isNewEpoch) data()->messList.at(i)->pos()->updateCoords( data()->CurrentEpoch, data()->Obliquity, data()->dObliq, data()->dEcLong );
				data()->messList.at(i)->pos()->EquatorialToHorizontal( data()->LSTh, geo()->lat() );
			}
		}

		//NGC
		if ( options()->drawNGC ) {
			for ( unsigned int i=0; i<data()->ngcList.count(); ++i ) {
				if (isNewEpoch) data()->ngcList.at(i)->pos()->updateCoords( data()->CurrentEpoch, data()->Obliquity, data()->dObliq, data()->dEcLong );
				data()->ngcList.at(i)->pos()->EquatorialToHorizontal( data()->LSTh, geo()->lat() );
			}
		}

		//IC
		if ( options()->drawIC ) {
			for ( unsigned int i=0; i<data()->icList.count(); ++i ) {
				if (isNewEpoch) data()->icList.at(i)->pos()->updateCoords( data()->CurrentEpoch, data()->Obliquity, data()->dObliq, data()->dEcLong );
				data()->icList.at(i)->pos()->EquatorialToHorizontal( data()->LSTh, geo()->lat() );
			}
		}

		//Milky Way
		if ( options()->drawMilkyWay ) {
			for ( unsigned int j=0; j<11; ++j ) {
				for ( unsigned int i=0; i<data()->MilkyWay[j].count(); ++i) {
					if (isNewEpoch) data()->MilkyWay[j].at(i)->updateCoords( data()->CurrentEpoch, data()->Obliquity, data()->dObliq, data()->dEcLong );
					data()->MilkyWay[j].at(i)->EquatorialToHorizontal( data()->LSTh, geo()->lat() );
				}
			}
		}

		//CLines
		if ( options()->drawConstellLines ) {
			for ( unsigned int i=0; i<data()->clineList.count(); ++i) {
				if (isNewEpoch) data()->clineList.at(i)->updateCoords( data()->CurrentEpoch, data()->Obliquity, data()->dObliq, data()->dEcLong );
				data()->clineList.at(i)->EquatorialToHorizontal( data()->LSTh, geo()->lat() );
			}
		}

		//CNames
		if ( options()->drawConstellNames ) {
			for ( unsigned int i=0; i<data()->cnameList.count(); ++i ) {
				if (isNewEpoch) data()->cnameList.at(i)->pos()->updateCoords( data()->CurrentEpoch, data()->Obliquity, data()->dObliq, data()->dEcLong );
				data()->cnameList.at(i)->pos()->EquatorialToHorizontal( data()->LSTh, geo()->lat() );
			}
		}

		//Celestial Equator
		if ( options()->drawEquator ) {
			for ( unsigned int i=0; i<data()->Equator.count(); ++i ) {
				data()->Equator.at(i)->EquatorialToHorizontal( data()->LSTh, geo()->lat() );
			}
		}

		//Ecliptic
		if ( options()->drawEcliptic ) {
			for ( unsigned int i=0; i<data()->Ecliptic.count(); ++i ) {
				data()->Ecliptic.at(i)->EquatorialToHorizontal( data()->LSTh, geo()->lat() );
			}
		}

		//Horizon: differnet than the others; Alt & Az remain constant, RA, Dec must keep up
		if ( options()->drawHorizon || options()->drawGround ) {
			for ( unsigned int i=0; i<data()->Horizon.count(); ++i ) {
				data()->Horizon.at(i)->HorizontalToEquatorial( data()->LSTh, geo()->lat() );
			}
		}

		skymap->setOldFocus( skymap->focus() );
		skymap->oldfocus()->EquatorialToHorizontal( data()->LSTh, geo()->lat() );
		skymap->Update();
	}
}

SkyObject* KStars::getObjectNamed( QString name ) {
	if ( (name== "star") || (name== "nothing") || name.isEmpty() ) return NULL;
	if ( name== data()->Sun->name() ) return data()->Sun;
	if ( name== data()->Moon->name() ) return data()->Moon;
	if ( name== data()->Mercury->name() ) return data()->Mercury;
	if ( name== data()->Venus->name() ) return data()->Venus;
	if ( name== data()->Mars->name() ) return data()->Mars;
	if ( name== data()->Jupiter->name() ) return data()->Jupiter;
	if ( name== data()->Saturn->name() ) return data()->Saturn;
	if ( name== data()->Uranus->name() ) return data()->Uranus;
	if ( name== data()->Neptune->name() ) return data()->Neptune;
	if ( name== data()->Pluto->name() ) return data()->Pluto;

	//Stars
	for ( unsigned int i=0; i<data()->starList.count(); ++i ) {
		if ( name==data()->starList.at(i)->name() ) return data()->starList.at(i);
	}

	//Messier
	for ( unsigned int i=0; i<data()->messList.count(); ++i ) {
		if ( name==data()->messList.at(i)->name() ) return data()->messList.at(i);
	}

	//NGC
	for ( unsigned int i=0; i<data()->ngcList.count(); ++i ) {
		if ( name==data()->ngcList.at(i)->name() ) return data()->ngcList.at(i);
	}

	//IC
	for ( unsigned int i=0; i<data()->icList.count(); ++i ) {
		if ( name==data()->icList.at(i)->name() ) return data()->icList.at(i);
	}

	//Constellations
	for ( unsigned int i=0; i<data()->cnameList.count(); ++i ) {
		if ( name==data()->cnameList.at(i)->name() ) return data()->cnameList.at(i);
	}

	//should never get here
	return NULL;
}

void KStars::showFocusCoords( void ) {
	//display object info in infoPanel
	QString s, oname;
	char dsgn = '+', azsgn = '+', altsgn = '+';

	oname = i18n( "nothing" );
	if ( skymap->foundObject() != NULL && options()->isTracking ) {
		oname = skymap->foundObject()->translatedName();
		//add genetive name for stars
	  if ( skymap->foundObject()->type()==0 && skymap->foundObject()->name2().length() )
			oname += " (" + skymap->foundObject()->name2() + ")";
	}

	FocusObject->setText( i18n( "Focused on: " ) + oname );

	if ( skymap->focus()->dec().Degrees() < 0 ) dsgn = '-';
	int dd = abs( skymap->focus()->dec().degree() );
	int dm = abs( skymap->focus()->dec().getArcMin() );
	int ds = abs( skymap->focus()->dec().getArcSec() );
	FocusRA->setText( i18n( "Right Ascension", "RA" ) + s.sprintf( ": %02d:%02d:%02d", skymap->focus()->ra().hour(), skymap->focus()->ra().minute(),  skymap->focus()->ra().second() ) );
	FocusDec->setText( i18n( "Declination", "Dec" ) +  s.sprintf( ": %c%02d:%02d:%02d", dsgn, dd, dm, ds ) );

	if ( skymap->focus()->alt().Degrees() < 0 ) altsgn = '-';
	if ( skymap->focus()->az().Degrees() < 0 ) azsgn = '-';
	int altd = abs( skymap->focus()->alt().degree() );
	int altm = abs( skymap->focus()->alt().getArcMin() );
	int alts = abs( skymap->focus()->alt().getArcSec() );
	int azd = abs( skymap->focus()->az().degree() );
	int azm = abs( skymap->focus()->az().getArcMin() );
	int azs = abs( skymap->focus()->az().getArcSec() );
	FocusAlt->setText( i18n( "Altitude", "Alt" ) + s.sprintf( ": %c%02d:%02d:%02d", altsgn, altd, altm, alts ) );
	FocusAz->setText( i18n( "Azimuth", "Az" ) + s.sprintf( ": %c%02d:%02d:%02d", azsgn, azd, azm, azs ) );
}

QString KStars::getDateString( QDate date ) {
  QString dummy;
  QString dateString = dummy.sprintf( "%02d / %02d / %04d",
			  date.month(), date.day(), date.year() );
  return dateString;
}

/*
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

//  If the date is after 10/15/1582, then take Pope Gregory's modification
//	 to the Julian calendar into account

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
*/

void KStars::updateEpoch( long double NewEpoch ) {
	//Several quantities change slowly with time:
	//Obliquity, Ecliptic Longitude,
	//eccentricity of Earth's Orbit
	//Earth's longitude of perihelion
	data()->CurrentEpoch = NewEpoch;
	data()->Obliquity = findObliquity( data()->CurrentEpoch );
	nutate( data()->CurrentEpoch ); //find dEcLong and dObliq
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
	O2.setD( 2.0*O.Degrees() );
	L2.setD( 2.0*( 280.4665 + 36000.7698*T ) ); //twice mean ecl. long. of Sun
	M2.setD( 2.0*( 218.3165 + 481267.8813*T ) );//twice mean ecl. long. of Moon

	O.SinCos( sinO, cosO );
	O2.SinCos( sin2O, cos2O );
	L2.SinCos( sin2L, cos2L );
	M2.SinCos( sin2M, cos2M );

	data()->dEcLong = ( -17.2*sinO - 1.32*sin2L - 0.23*sin2M + 0.21*sin2O)/3600.0; //Ecl. long. correction
	data()->dObliq = (   9.2*cosO + 0.57*cos2L + 0.10*cos2M - 0.09*cos2O)/3600.0; //Obliq. correction
}

QTime KStars::UTtoLST( QDateTime UT, dms longitude) {
  QTime GST = UTtoGST( UT );
  QTime LST = GSTtoLST( GST, longitude );
  return LST;
}

QTime KStars::UTtoGST( QDateTime UT ) {
  QDateTime t0 = UT;
  t0.setTime( QTime(0,0,0) );
  long double jd0 = data()->getJD( t0 );
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
			   double(GST.second())/60.0)/60.0 + longitude.Degrees()/15.0;

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

void KStars::changeTimeStep( void ) {
	data()->SysJD_Mark = data()->getJD( QDateTime::currentDateTime() );
	data()->SkyJD_Mark = data()->getJD( data()->LTime );

	//return input focus to skymap
	skymap->QWidget::setFocus();

}

#include "kstars.moc"
