/***************************************************************************
                          viewopsdialog.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Sat Apr 28 2001
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


#include <qtabwidget.h>
#include <qbuttongroup.h>
#include <qradiobutton.h>
#include <qcheckbox.h>
#include <qlistview.h>
#include <qspinbox.h>

#include <knuminput.h>
#include <klineeditdlg.h>
#include <kmessagebox.h>
#include <kdebug.h>
#include <kcolordialog.h>

#include "kstars.h"
#include "magnitudespinbox.h"
#include "timestepbox.h"
#include "addcatdialog.h"
#include "viewopsdialog.h"

#include "viewopsdialog.moc"

ViewOpsDialog::ViewOpsDialog( QWidget *parent )
	: KDialogBase( KDialogBase::Plain, i18n( "Display Options" ), Ok|Cancel, Ok, parent ) {
	QFrame *page = plainPage();
	ksw = (KStars *)parent;

	// Layout manager for entire dialog
	hlay = new QHBoxLayout( page, 2, 2 );
	hlay->setSpacing( 2 );
	hlay->setMargin( 4 );

	DisplayBox = new QGroupBox( page, "DisplayBox" );
	DisplayBox->setMinimumWidth( 230 );
	DisplayBox->setTitle( i18n( "Display Options" ) );

	DisplayBoxLayout = new QVBoxLayout( DisplayBox );
	DisplayBoxLayout->setSpacing( 6 );
	DisplayBoxLayout->setMargin( 11 );

	DisplayTabs = new QTabWidget( DisplayBox, "DisplayTabs" );
	DisplayTabs->move( 10, 24 );
	DisplayTabs->setMinimumSize( 220, 300 );

	CoordsGroup = new QButtonGroup( 1, Qt::Vertical, i18n( "Coordinate System" ), DisplayBox );

	EquatRadio = new QRadioButton( i18n( "The Equatorial (RA/Dec) coordinate system", "Equatorial" ), CoordsGroup );
	EquatRadio->setChecked( !ksw->options()->useAltAz );

	AltAzRadio = new QRadioButton( i18n( "The Horizontal (Alt/Az) coordinate system", "Horizontal" ), CoordsGroup );
	AltAzRadio->setChecked( ksw->options()->useAltAz );

	DisplayBoxLayout->addSpacing( 10 );
	DisplayBoxLayout->addWidget( DisplayTabs );
	DisplayBoxLayout->addWidget( CoordsGroup );


//*** Construct Catalogs tab ***//
  //This Tab now includes Stars (18 Feb 2002; JH)
	CatalogTab = new QWidget( DisplayTabs, "CatalogTab" );
	vlayCatTab = new QVBoxLayout( CatalogTab );
	vlayCatTab->setSpacing( 6 );
	vlayCatTab->setMargin( 11 );

//Place stars-related widgets in a group box
	StarsBox = new QGroupBox( i18n( "Stars" ), CatalogTab, "StarsBox" );
	vlayStarsBox = new QVBoxLayout( StarsBox );
	vlayStarsBox->setSpacing( 6 );
	vlayStarsBox->setMargin( 11 );

	showSAO = new QCheckBox( i18n( "Smithsonian Astrophysical Observatory (SAO) star catalog", "SAO star catalog" ), StarsBox );
	QFont stdFont(  showSAO->font() );
	showSAO->setFont( stdFont );
	showSAO->setChecked( ksw->options()->drawSAO );

	// Spin box : show stars brighter than magnitude limit
	int intMagLimitDrawStars = int ( 10.0 * ksw->options()->magLimitDrawStar );
	magSpinBoxDrawStars = new MagnitudeSpinBox( 0, 129, StarsBox );	// max mag = 12.9
	magSpinBoxDrawStars->setFont( stdFont );
	magSpinBoxDrawStars->setValue( intMagLimitDrawStars );

	textLabelMagStars = new QLabel( StarsBox, "LabelMagStars" );
	textLabelMagStars->setText( i18n( "Show stars brighter than" ) );
	textLabelMagStars->setFont( stdFont );

	// Spin box : show stars names for stars brighter than magnitude limit
	int intMagLimitDrawStarInfo = 10*((int)(ksw->options()->magLimitDrawStarInfo));
	magSpinBoxDrawStarInfo = new MagnitudeSpinBox( 0, 90, StarsBox );
	magSpinBoxDrawStarInfo->setFont( stdFont );
	magSpinBoxDrawStarInfo->setValue( intMagLimitDrawStarInfo );

	textLabelMagStarInfo = new QLabel( StarsBox, "LabelMagStarNames" );
	textLabelMagStarInfo->setText( i18n( "For stars brighter than" ) );
	textLabelMagStarInfo->setFont( stdFont );

	showStarNames = new QCheckBox( i18n( "Show name" ), StarsBox );
	showStarNames->setFont( stdFont );
	showStarNames->setChecked( ksw->options()->drawStarName );

	showStarMagnitude = new QCheckBox( i18n( "Show magnitude" ), StarsBox );
	showStarMagnitude->setFont( stdFont );
	showStarMagnitude->setChecked( ksw->options()->drawStarMagnitude );

	showStarNames->setEnabled( showSAO->isChecked() );
	showStarMagnitude->setEnabled( showSAO->isChecked() );
	magSpinBoxDrawStars->setEnabled( showSAO->isChecked() );
	textLabelMagStars->setEnabled( showSAO->isChecked() );
	magSpinBoxDrawStarInfo->setEnabled( showSAO->isChecked() );
	textLabelMagStarInfo->setEnabled( showSAO->isChecked() );

	QSpacerItem *spacerDrawLim = new QSpacerItem( 10, 10, QSizePolicy::Expanding, QSizePolicy::Minimum );
	QSpacerItem *spacerLabelLim = new QSpacerItem( 10, 10, QSizePolicy::Expanding, QSizePolicy::Minimum );

	hlayDrawLimit = new QHBoxLayout( 4 );
	hlayDrawLimit->addWidget( textLabelMagStars );
	hlayDrawLimit->addWidget( magSpinBoxDrawStars );
	hlayDrawLimit->addItem( spacerDrawLim );

	hlayLabelLimit = new QHBoxLayout( 4 );
	hlayLabelLimit->addWidget( textLabelMagStarInfo );
	hlayLabelLimit->addWidget( magSpinBoxDrawStarInfo );
	hlayLabelLimit->addItem( spacerLabelLim );

	vlayStarsBox->addSpacing( 10 );
	vlayStarsBox->addWidget( showSAO );
	vlayStarsBox->addLayout( hlayDrawLimit );
	vlayStarsBox->addSpacing( 10 );
	vlayStarsBox->addLayout( hlayLabelLimit );
	vlayStarsBox->addWidget( showStarNames );
	vlayStarsBox->addWidget( showStarMagnitude );

//Deep-sky catalogs
//Store these in a list box, so that custom catalogs can be added
	CatalogList = new QListView( CatalogTab, "catalog_list" );
	CatalogList->addColumn( i18n( "Other Catalogs" ) );

//Incompatible with KDE2.2:
//	CatalogList->setResizeMode( QListView::LastColumn );
	CatalogList->setVScrollBarMode( QScrollView::AlwaysOn );
	CatalogList->setSorting( -1 );
	CatalogList->setMaximumHeight( 128 );

	showIC = new QCheckListItem( CatalogList, i18n( "Index Catalog (IC)" ), QCheckListItem::CheckBox );
	showIC->setOn( ksw->options()->drawIC );

	showNGC = new QCheckListItem( CatalogList, i18n( "New General Catalog (NGC)" ), QCheckListItem::CheckBox );
	showNGC->setOn( ksw->options()->drawNGC );

	showMessImages = new QCheckListItem( CatalogList, i18n( "Messier Catalog (images)" ), QCheckListItem::CheckBox );
	showMessImages->setOn( ksw->options()->drawMessImages );

	showMessier = new QCheckListItem( CatalogList, i18n( "Messier Catalog (symbols)" ), QCheckListItem::CheckBox );
	showMessier->setOn( ksw->options()->drawMessier );

//Add custom catalogs, if necessary
	for ( unsigned int i=0; i<ksw->options()->CatalogCount; ++i ) { //loop over custom catalogs
		QCheckListItem *newItem = new QCheckListItem( CatalogList, ksw->options()->CatalogName[i], QCheckListItem::CheckBox );
		newItem->setOn( ksw->options()->drawCatalog[i] );
	}

	hlayCatButtons = new QHBoxLayout( 4 );

	AddCatalog = new QPushButton( i18n( "Add Custom Catalog..." ), CatalogTab );
	AddCatalog->setFont( stdFont );

	RemoveCatalog = new QPushButton( i18n( "Remove Custom Catalog" ), CatalogTab );
	RemoveCatalog->setFont( stdFont );
	RemoveCatalog->setEnabled( false );

	hlayCatButtons->addWidget( AddCatalog );
	hlayCatButtons->addWidget( RemoveCatalog );

	QSpacerItem *spacerCatTab = new QSpacerItem( 20, 20, QSizePolicy::Minimum, QSizePolicy::Expanding );

	vlayCatTab->addWidget( StarsBox );
	vlayCatTab->addSpacing( 20 );
	vlayCatTab->addWidget( CatalogList );
	vlayCatTab->addLayout( hlayCatButtons );
	vlayCatTab->addItem( spacerCatTab );

	DisplayTabs->insertTab( CatalogTab, i18n( "Catalogs" ) );


//*** Construct Planets tab //
	PlanetTab = new QWidget( DisplayTabs, "PlanetTab" );
	vlayPlanetTab = new QVBoxLayout( PlanetTab );
	vlayPlanetTab->setSpacing( 6 );
	vlayPlanetTab->setMargin( 11 );

	PlanetBox = new QGroupBox( i18n( "Sun, Moon and Planets" ), PlanetTab, "PlanetBox" );
	vlayPlanetBox = new QVBoxLayout( PlanetBox );
	glayPlanetBox = new QGridLayout( 5, 3 );
	vlayPlanetBox->setSpacing( 6 );
	vlayPlanetBox->setMargin( 11 );

	showSun = new QCheckBox( i18n( "The sun" ), PlanetBox, "show_sun" );
	showSun->setFont( stdFont );
	showSun->setChecked( ksw->options()->drawSun );

	showMoon = new QCheckBox( i18n( "The moon" ), PlanetBox, "show_moon" );
  showMoon->setFont( stdFont );
  showMoon->setChecked( ksw->options()->drawMoon );
  showMoon->setChecked( ksw->options()->drawMoon );

	showMercury = new QCheckBox( i18n( "Mercury" ), PlanetBox, "show_mercury" );
	showMercury->setFont( stdFont );
	showMercury->setChecked( ksw->options()->drawMercury );

	showVenus = new QCheckBox( i18n( "Venus" ), PlanetBox, "show_venus" );
	showVenus->setFont( stdFont );
	showVenus->setChecked( ksw->options()->drawVenus );

	showMars = new QCheckBox( i18n( "Mars" ), PlanetBox, "show_mars" );
	showMars->setFont( stdFont );
	showMars->setChecked( ksw->options()->drawMars );

	showJupiter = new QCheckBox( i18n( "Jupiter" ), PlanetBox, "show_jupiter" );
	showJupiter->setFont( stdFont );
	showJupiter->setChecked( ksw->options()->drawJupiter );

	showSaturn = new QCheckBox( i18n( "Saturn" ), PlanetBox, "show_saturn" );
	showSaturn->setFont( stdFont );
	showSaturn->setChecked( ksw->options()->drawSaturn );

	showUranus = new QCheckBox( i18n( "Uranus" ), PlanetBox, "show_uranus" );
	showUranus->setFont( stdFont );
	showUranus->setChecked( ksw->options()->drawUranus );

	showNeptune = new QCheckBox( i18n( "Neptune" ), PlanetBox, "show_neptune" );
	showNeptune->setFont( stdFont );
	showNeptune->setChecked( ksw->options()->drawNeptune );

	showPluto = new QCheckBox( i18n( "Pluto" ), PlanetBox, "show_pluto" );
	showPluto->setFont( stdFont );
	showPluto->setChecked( ksw->options()->drawPluto );

	showAll = new QPushButton( i18n( "Show all planets", "Select All" ), PlanetBox, "show_all_planets" );
	showAll->setFont( stdFont );

	showNone = new QPushButton( i18n( "Hide all planets", "Select None" ), PlanetBox, "show_none_planets" );
	showNone->setFont( stdFont );

	showPlanetNames = new QCheckBox( i18n( "Use name labels" ), PlanetBox, "show_planet_names" );
	showPlanetNames->setFont( stdFont );
	showPlanetNames->setChecked( ksw->options()->drawPlanetName );

	showPlanetImages = new QCheckBox( i18n( "Use images" ), PlanetBox, "show_planet_images" );
	showPlanetImages->setFont( stdFont );
	showPlanetImages->setChecked( ksw->options()->drawPlanetImage );

	QSpacerItem *planetBoxSpacer = new QSpacerItem( 20, 20, QSizePolicy::Minimum, QSizePolicy::Minimum );


	glayPlanetBox->addWidget( showSun, 0, 0 );
	glayPlanetBox->addWidget( showMoon, 1, 0 );
	glayPlanetBox->addWidget( showMercury, 2, 0 );
	glayPlanetBox->addWidget( showVenus, 3, 0 );
	glayPlanetBox->addWidget( showMars, 4, 0 );
	glayPlanetBox->addWidget( showJupiter, 0, 1 );
	glayPlanetBox->addWidget( showSaturn, 1, 1 );
	glayPlanetBox->addWidget( showUranus, 2, 1 );
	glayPlanetBox->addWidget( showNeptune, 3, 1 );
	glayPlanetBox->addWidget( showPluto, 4, 1 );
	glayPlanetBox->addWidget( showAll, 0, 2 );
	glayPlanetBox->addWidget( showNone, 1, 2 );
	glayPlanetBox->addWidget( showPlanetNames, 2, 2 );
	glayPlanetBox->addWidget( showPlanetImages, 3, 2 );
	glayPlanetBox->addItem( planetBoxSpacer, 4, 2 );

	//Minor Planets
	MinorBox = new QGroupBox( i18n( "Minor Planets" ), PlanetTab, "MinorBox" );
	vlayMinorBox = new QVBoxLayout( MinorBox );
	glayMinorBox = new QGridLayout( 3, 3 );
	vlayMinorBox->setSpacing( 6 );
	vlayMinorBox->setMargin( 11 );

	showAsteroids = new QCheckBox( i18n( "Asteroids" ), MinorBox, "show_asteroids" );
	showAsteroids->setFont( stdFont );
	showAsteroids->setChecked( ksw->options()->drawAsteroids );

	textLabelMagAsteroids = new QLabel( MinorBox, "LabelMagAsteroids" );
	textLabelMagAsteroids->setFont( stdFont );
	textLabelMagAsteroids->setText( i18n( "Show asteroids brighter than " ) );

	int intMagLimitAsteroid = int( 10 * ksw->options()->magLimitAsteroid );
	astDrawSpinBox = new MagnitudeSpinBox( 25, 120, MinorBox );	// min mag = 2.5; max mag = 12.0
	astDrawSpinBox->setFont( stdFont );
	astDrawSpinBox->setValue( intMagLimitAsteroid );
	astDrawSpinBox->setEnabled( ksw->options()->drawAsteroids );

	showAsteroidNames = new QCheckBox( i18n( "Show names of asteroids brighter than: " ), MinorBox, "show_asteroid_names" );
	showAsteroidNames->setFont( stdFont );
	showAsteroidNames->setChecked( ksw->options()->drawAsteroidName );
	showAsteroidNames->setEnabled( ksw->options()->drawAsteroids );

	int intMagLimitAsteroidName = int( 10 * ksw->options()->magLimitAsteroidName );
	astNameSpinBox = new MagnitudeSpinBox( 25, 120, MinorBox );	// min mag = 2.5; max mag = 12.0
	astNameSpinBox->setFont( stdFont );
	astNameSpinBox->setValue( intMagLimitAsteroidName );
	astNameSpinBox->setEnabled( ksw->options()->drawAsteroids );
	astNameSpinBox->setEnabled( ksw->options()->drawAsteroids && ksw->options()->drawAsteroidName );

	showComets = new QCheckBox( i18n( "Comets" ), MinorBox, "show_comets" );
	showComets->setFont( stdFont );
	showComets->setChecked( ksw->options()->drawComets );

	showCometNames = new QCheckBox( i18n( "Show names of comets within (AU): " ), MinorBox, "show_comet_names" );
	showCometNames->setFont( stdFont );
	showCometNames->setChecked( ksw->options()->drawCometName );
	showCometNames->setEnabled( ksw->options()->drawComets );

	int intMaxRadCometName = int( 10 * ksw->options()->maxRadCometName );
	comNameSpinBox = new MagnitudeSpinBox( 0, 1000, MinorBox );	// max rad = 100.0 AU
	comNameSpinBox->setFont( stdFont );
	comNameSpinBox->setValue( intMaxRadCometName );
	comNameSpinBox->setEnabled( ksw->options()->drawComets && ksw->options()->drawCometName );

	QSpacerItem *spacerAsteroid  = new QSpacerItem( 20, 20, QSizePolicy::Minimum, QSizePolicy::Minimum );

	glayMinorBox->addWidget( showAsteroids, 0, 0 );
	glayMinorBox->addItem( spacerAsteroid, 1, 0 );
	glayMinorBox->addWidget( showComets, 2, 0 );
	glayMinorBox->addWidget( textLabelMagAsteroids, 0, 1 );
	glayMinorBox->addWidget( showAsteroidNames, 1, 1 );
	glayMinorBox->addWidget( showCometNames, 2, 1 );
	glayMinorBox->addWidget( astDrawSpinBox, 0, 2 );
	glayMinorBox->addWidget( astNameSpinBox, 1, 2 );
	glayMinorBox->addWidget( comNameSpinBox, 2, 2 );

	//Planet Trails
	TrailBox = new QGroupBox( i18n( "Planetary Trails" ), PlanetTab, "TrailBox" );
	vlayTrailBox = new QVBoxLayout( TrailBox );
	glayTrailBox = new QGridLayout( 2, 2 );
	vlayTrailBox->setSpacing( 6 );
	vlayTrailBox->setMargin( 11 );

	autoTrail = new QCheckBox( i18n( "Always show trail when tracking a solar system body" ), TrailBox );
	autoTrail->setFont( stdFont );
	autoTrail->setChecked( ksw->options()->useAutoTrail );

	fadePlanetTrails = new QCheckBox( i18n( "Planet trails fade to background color" ), TrailBox );
	fadePlanetTrails->setFont( stdFont );
	fadePlanetTrails->setChecked( ksw->options()->fadePlanetTrails );

	ClearAllTrails = new QPushButton( i18n( "Remove All Trails" ), TrailBox );
	ClearAllTrails->setFont( stdFont );

	glayTrailBox->addWidget( autoTrail, 0, 0 );
	glayTrailBox->addWidget( fadePlanetTrails, 1, 0 );
	glayTrailBox->addItem( new QSpacerItem( 20, 10, QSizePolicy::Minimum, QSizePolicy::Minimum ), 0, 1 );
	glayTrailBox->addWidget( ClearAllTrails, 1, 1 );

	QSpacerItem *spacerPlanetTab  = new QSpacerItem( 20, 20, QSizePolicy::Minimum, QSizePolicy::Expanding );
	QSpacerItem *spacerPlanetBox  = new QSpacerItem( 20, 20, QSizePolicy::Minimum, QSizePolicy::Expanding );
	QSpacerItem *spacerMinorBox   = new QSpacerItem( 20, 20, QSizePolicy::Minimum, QSizePolicy::Expanding );
	QSpacerItem *spacerTrailBox   = new QSpacerItem( 20, 20, QSizePolicy::Minimum, QSizePolicy::Expanding );

	vlayPlanetBox->addItem( spacerPlanetBox );
	vlayPlanetBox->addLayout( glayPlanetBox );
	vlayMinorBox->addItem( spacerMinorBox );
	vlayMinorBox->addLayout( glayMinorBox );
	vlayTrailBox->addItem( spacerTrailBox );
	vlayTrailBox->addLayout( glayTrailBox );

	vlayPlanetTab->addWidget( PlanetBox );
	vlayPlanetTab->addWidget( MinorBox );
	vlayPlanetTab->addWidget( TrailBox );

	vlayPlanetTab->addItem( spacerPlanetTab );

	DisplayTabs->insertTab( PlanetTab, i18n( "Solar System" ) );


//*** Construct Guides tab: ***//
	GuideTab = new QWidget( DisplayTabs, "GuideTab" );
	vlayGuideTab = new QVBoxLayout( GuideTab );
	vlayGuideTab->setSpacing( 6 );
	vlayGuideTab->setMargin( 11 );

	showConstellLines = new QCheckBox( i18n( "Constellation lines" ), GuideTab );
	showConstellLines->setFont( stdFont );
	showConstellLines->setChecked( ksw->options()->drawConstellLines );

	showConstellNames = new QCheckBox( i18n( "Constellation names" ), GuideTab );
	showConstellNames->setFont( stdFont );
	showConstellNames->setChecked( ksw->options()->drawConstellNames );
	ConstellOptions = new QButtonGroup( 1, Qt::Vertical, i18n( "Constellation Name Options" ), GuideTab );
	useLatinConstellNames = new QRadioButton( i18n( "Standard Latin constellation names (e.g., 'Lyra')", "Latin" ), ConstellOptions );
	useLatinConstellNames->setFont ( stdFont );
	useLatinConstellNames->setChecked( ksw->options()->useLatinConstellNames );
  useLocalConstellNames = new QRadioButton( i18n( "Local translated constellation names (e.g., en='The Lyre'", "Localized" ), ConstellOptions );
	useLocalConstellNames->setFont ( stdFont );
	useLocalConstellNames->setChecked( ksw->options()->useLocalConstellNames );
  useAbbrevConstellNames = new QRadioButton( i18n( "Standard IAU constellation abbreviation (e.g., 'Lyr')", "Abbreviated" ), ConstellOptions );
	useAbbrevConstellNames->setFont ( stdFont );
	useAbbrevConstellNames->setChecked( ksw->options()->useAbbrevConstellNames );
	ConstellOptions->setEnabled( showConstellNames->isChecked() );

	showMilkyWay = new QCheckBox( i18n( "refers to the band of stars in the sky due to the Galactic plane", "Milky Way" ), GuideTab );
	showMilkyWay->setFont( stdFont );
	showMilkyWay->setChecked( ksw->options()->drawMilkyWay );

	showMilkyWayFilled = new QCheckBox( i18n( "display the Milky way as a solid polygon (instead of just an outline)", "Fill Milky Way" ), GuideTab );
	showMilkyWayFilled->setFont( stdFont );
	showMilkyWayFilled->setChecked( ksw->options()->fillMilkyWay );
  showMilkyWayFilled->setEnabled( showMilkyWay->isChecked() );

	showGrid = new QCheckBox( i18n( "Coordinate grid" ), GuideTab );
	showGrid->setFont( stdFont );
	showGrid->setChecked( ksw->options()->drawGrid );

	showEquator = new QCheckBox( i18n( "Celestial equator" ), GuideTab );
	showEquator->setFont( stdFont );
	showEquator->setChecked( ksw->options()->drawEquator );

	showEcliptic = new QCheckBox( i18n( "Ecliptic" ), GuideTab );
	showEcliptic->setFont( stdFont );
	showEcliptic->setChecked( ksw->options()->drawEcliptic );

	showHorizon = new QCheckBox( i18n( "Horizon (line)" ), GuideTab );
	showHorizon->setFont( stdFont );
	showHorizon->setChecked( ksw->options()->drawHorizon );

	showGround = new QCheckBox( i18n( "display the ground as solid, opaque green", "Opaque ground" ), GuideTab );
	showGround->setFont( stdFont );
	showGround->setChecked( ksw->options()->drawGround );
	if ( !ksw->options()->useAltAz ) showGround->setEnabled( false );

	QSpacerItem *spacerGuideTab = new QSpacerItem( 20, 20, QSizePolicy::Minimum, QSizePolicy::Expanding );
	QSpacerItem *smallspacer = new QSpacerItem( 20, 12, QSizePolicy::Minimum, QSizePolicy::Minimum );
	QSpacerItem *smallspacer2= new QSpacerItem( 20, 12, QSizePolicy::Minimum, QSizePolicy::Minimum );

	vlayGuideTab->addWidget( showConstellLines );
	vlayGuideTab->addWidget( showConstellNames );
	vlayGuideTab->addWidget( ConstellOptions );
	vlayGuideTab->addItem( smallspacer );
	vlayGuideTab->addWidget( showMilkyWay );
	vlayGuideTab->addWidget( showMilkyWayFilled );
	vlayGuideTab->addItem( smallspacer2 );
	vlayGuideTab->addWidget( showGrid );
	vlayGuideTab->addWidget( showEquator );
	vlayGuideTab->addWidget( showEcliptic );
	vlayGuideTab->addWidget( showHorizon );
	vlayGuideTab->addWidget( showGround );
	vlayGuideTab->addItem( spacerGuideTab );

	DisplayTabs->insertTab( GuideTab, i18n( "Guides are imaginary lines displayed in the sky map", "Guides" ) );


//*** Construct the Color Tab ***//
	ColorTab = new QWidget( DisplayTabs, "ColorTab" );
	LeftBox = new QGroupBox( ColorTab, "LeftBox" );
	LeftBox->setTitle( i18n( "current color scheme settings", "Current Colors" ) );

	RightBox = new QGroupBox( ColorTab, "RightBox" );
	RightBox->setTitle( i18n( "list of defined color schemes", "Preset Color Schemes" ) );

	hlayColorTab = new QHBoxLayout( ColorTab );
	vlayLeftBox = new QVBoxLayout( LeftBox );
	hlayLeftBox = new QHBoxLayout( 4 );
	hlayLeftBox2 = new QHBoxLayout( 4 );
	vlayRightBox = new QVBoxLayout( RightBox );

	hlayColorTab->setSpacing( 4 );
	hlayColorTab->setMargin( 8 );
	vlayLeftBox->setSpacing( 6 );
	vlayLeftBox->setMargin( 12 );
	vlayRightBox->setSpacing( 6 );
	vlayRightBox->setMargin( 12 );

	ColorPalette = new QListBox( LeftBox );
	ColorPalette->setSelectionMode( QListBox::Single );

	for ( unsigned int i=0; i < ksw->options()->colorScheme()->numberOfColors(); ++i ) {
		QPixmap col( 30, 20 );
		col.fill( QColor( ksw->options()->colorScheme()->colorAt( i ) ) );
		ColorPalette->insertItem( col, ksw->options()->colorScheme()->nameAt( i ) );
	}

  // the spinbox for colorintensity of stars
	IntensityBox = new KIntSpinBox (0, 10, 1, ksw->options()->colorScheme()->starColorIntensity(), 10, LeftBox);
	QLabel *IntensityLabel = new QLabel (IntensityBox, i18n ( "scale for the color saturation of stars", "Star color &intensity:" ), LeftBox);
	IntensityLabel->setAlignment ( AlignRight | AlignVCenter );

  // the QComboBox for the starcolor mode
	StarColorMode = new QComboBox( LeftBox );
	QLabel *ColorModeLabel = new QLabel( StarColorMode, i18n( "How stars are colored", "Star color &mode:" ), LeftBox );
	ColorModeLabel->setAlignment ( AlignRight | AlignVCenter );
	StarColorMode->insertItem( i18n( "use realistic star colors", "Real Colors" ) );
	StarColorMode->insertItem( i18n( "show stars as red circles", "Solid Red" ) );
	StarColorMode->insertItem( i18n( "show stars as black circles", "Solid Black" ) );
	StarColorMode->insertItem( i18n( "show stars as white circles", "Solid White" ) );

  //The list of preset color schemes
	PresetBox = new QListBox( RightBox );
  PresetBox->setSelectionMode( QListBox::Single );
	PresetBox->insertItem( i18n( "use default color scheme", "Default Colors" ) );
	PresetBox->insertItem( i18n( "use 'star chart' color scheme", "Star Chart" ) );
	PresetBox->insertItem( i18n( "use 'night vision' color scheme", "Night Vision" ) );
	PresetBox->insertItem( i18n( "use 'moonless night' color scheme", "Moonless Night" ) );

	PresetFileList.append( "default.colors" );
	PresetFileList.append( "chart.colors" );
	PresetFileList.append( "night.colors" );
	PresetFileList.append( "moonless-night.colors" );

	QFile file;
	QString line, schemeName, filename;
	file.setName( locateLocal( "appdata", "colors.dat" ) ); //determine filename in local user KDE directory tree.
	if ( file.open( IO_ReadOnly ) ) {
		QTextStream stream( &file );

  	while ( !stream.eof() ) {
			line = stream.readLine();
			schemeName = line.left( line.find( ':' ) );
			filename = line.mid( line.find( ':' ) +1, line.length() );
			PresetBox->insertItem( schemeName );
			PresetFileList.append( filename );
	  }
		file.close();
	}

	AddPreset = new QPushButton( i18n( "Save Current Colors" ), RightBox );
	RemovePreset = new QPushButton( i18n( "Remove Color Scheme" ), RightBox );
	RemovePreset->setEnabled( false );

  //Pack the widgets into the layouts
	hlayLeftBox2->addWidget ( IntensityLabel );
	hlayLeftBox2->addWidget ( IntensityBox );

	hlayLeftBox->addWidget ( ColorModeLabel );
	hlayLeftBox->addWidget ( StarColorMode );

	QSpacerItem *LeftSpacer = new QSpacerItem( 20, 20, QSizePolicy::Expanding, QSizePolicy::Minimum );
	QSpacerItem *RightSpacer = new QSpacerItem( 20, 20, QSizePolicy::Expanding, QSizePolicy::Minimum );

	vlayLeftBox->addItem( LeftSpacer );
	vlayLeftBox->addWidget( ColorPalette );
	vlayLeftBox->addLayout( hlayLeftBox );
	vlayLeftBox->addLayout( hlayLeftBox2 );

	vlayRightBox->addItem( RightSpacer );
	vlayRightBox->addWidget( PresetBox );
	vlayRightBox->addWidget( AddPreset );
	vlayRightBox->addWidget( RemovePreset );

	hlayColorTab->addWidget ( LeftBox );
	hlayColorTab->addWidget ( RightBox );

	DisplayTabs->insertTab( ColorTab, i18n( "Color configuration options (tab label)", "Colors" ) );


//*** Construct the Advanced Tab ***//
	AdvancedTab = new QWidget( DisplayTabs, "AdvancedTab" );
	vlayAdvancedTab = new QVBoxLayout( AdvancedTab );
	vlayAdvancedTab->setSpacing( 6 );
	vlayAdvancedTab->setMargin( 11 );

	useRefraction = new QCheckBox( i18n( "Correct for atmospheric refraction" ), AdvancedTab );
	useRefraction->setFont( stdFont );
	useRefraction->setChecked( ksw->options()->useRefraction );

	animateSlewing = new QCheckBox( i18n( "Use animated slewing" ), AdvancedTab );
	animateSlewing->setFont( stdFont );
	animateSlewing->setChecked( ksw->options()->useAnimatedSlewing );

	autoLabel = new QCheckBox( i18n( "Attach label to centered object" ), AdvancedTab );
	autoLabel->setFont( stdFont );
	autoLabel->setChecked( ksw->options()->useAutoLabel );

	hideSpinBox = new TimeStepBox( AdvancedTab, "HideSpinBox" );
	hideSpinBox->tsbox()->changeScale( (float)ksw->options()->slewTimeScale );
	QLabel *hsbLabel = new QLabel( AdvancedTab, "HSBLabel" );
	hsbLabel->setText( i18n( "Also hide if time scale greater than:" ) );

	hideObjects = new QCheckBox( i18n( "Hide objects while moving" ), AdvancedTab );
	hideObjects->setFont( stdFont );
	hideObjects->setChecked( ksw->options()->hideOnSlew );

	HideBox = new QGroupBox( AdvancedTab, "HideBox" );
	HideBox->setTitle( i18n( "Configure Hidden Objects" ) );

	hideStars = new QCheckBox( i18n( "Hide stars fainter than XX magnitude", "Stars fainter than " ), HideBox );
	hideStars->setFont( stdFont );
	hideStars->setChecked( ksw->options()->hideStars );

	hidePlanets = new QCheckBox( i18n( "Planets" ), HideBox );
	hidePlanets->setFont( stdFont );
	hidePlanets->setChecked( ksw->options()->hidePlanets );

	hideMess = new QCheckBox( i18n( "Messier objects" ), HideBox );
	hideMess->setFont( stdFont );
	hideMess->setChecked( ksw->options()->hideMess );

	hideNGC = new QCheckBox( i18n( "NGC objects" ), HideBox );
	hideNGC->setFont( stdFont );
	hideNGC->setChecked( ksw->options()->hideNGC );

	hideIC = new QCheckBox( i18n( "IC objects" ), HideBox );
	hideIC->setFont( stdFont );
	hideIC->setChecked( ksw->options()->hideIC );

	hideMW = new QCheckBox( i18n( "Milky Way" ), HideBox );
	hideMW->setFont( stdFont );
	hideMW->setChecked( ksw->options()->hideMW );

	hideCNames = new QCheckBox( i18n( "Constellation names" ), HideBox );
	hideCNames->setFont( stdFont );
	hideCNames->setChecked( ksw->options()->hideCNames );

	hideCLines = new QCheckBox( i18n( "Constellation lines" ), HideBox );
	hideCLines->setFont( stdFont );
	hideCLines->setChecked( ksw->options()->hideCLines );

	hideGrid = new QCheckBox( i18n( "Coordinate grid" ), HideBox );
	hideGrid->setFont( stdFont );
	hideGrid->setChecked( ksw->options()->hideGrid );

	int intMagLimitHideStars = int ( 10.0 * ksw->options()->magLimitHideStar );
	magSpinBoxHideStars = new MagnitudeSpinBox( 0, 79, HideBox );	// max mag = 7.9
	magSpinBoxHideStars->setFont( stdFont );
	magSpinBoxHideStars->setValue( intMagLimitHideStars );
	magSpinBoxHideStars->setMinimumWidth( 20 );

	QSpacerItem *spacerAdvTab = new QSpacerItem( 20, 20, QSizePolicy::Minimum, QSizePolicy::Expanding );
	QSpacerItem *spacerAdvHStars = new QSpacerItem( 20, 20, QSizePolicy::Expanding, QSizePolicy::Minimum );
	QSpacerItem *spacerAdvHTime = new QSpacerItem( 20, 20, QSizePolicy::Expanding, QSizePolicy::Minimum );

	//Construct layouts and add widgets to them
	vlayAdvHideObj = new QVBoxLayout( HideBox );
	vlayAdvHideObj->setSpacing( 6 );
	vlayAdvHideObj->setMargin( 11 );
	vlayAdvHideObj->addSpacing( 10 ); //Otherwise first widget overlaps title

	hlayAdvHideStars = new QHBoxLayout( 4 );  //add to vlayAdvHideObj
	hlayAdvHideTimeScale = new QHBoxLayout( 4 );  //add to vlayAdvHideObj
	glayAdvHideObj = new QGridLayout( 4, 2 ); //add to vlayAdvHideObj

	hlayAdvHideStars->addWidget( hideStars );
	hlayAdvHideStars->addWidget( magSpinBoxHideStars );
	hlayAdvHideStars->addItem( spacerAdvHStars );

	hlayAdvHideTimeScale->addWidget( hsbLabel );
	hlayAdvHideTimeScale->addWidget( hideSpinBox );
	hlayAdvHideTimeScale->addItem( spacerAdvHTime );

	glayAdvHideObj->addWidget( hidePlanets, 0, 0 );
	glayAdvHideObj->addWidget( hideMess, 1, 0 );
	glayAdvHideObj->addWidget( hideNGC, 2, 0 );
	glayAdvHideObj->addWidget( hideIC, 3, 0 );
	glayAdvHideObj->addWidget( hideMW, 0, 1 );
	glayAdvHideObj->addWidget( hideCNames, 1, 1 );
	glayAdvHideObj->addWidget( hideCLines, 2, 1 );
	glayAdvHideObj->addWidget( hideGrid, 3, 1 );

	vlayAdvHideObj->addLayout( hlayAdvHideStars );
	vlayAdvHideObj->addLayout( glayAdvHideObj );

	vlayAdvancedTab->addWidget( useRefraction );
	vlayAdvancedTab->addWidget( animateSlewing );
	vlayAdvancedTab->addWidget( autoLabel );
	vlayAdvancedTab->addSpacing( 20 );
	vlayAdvancedTab->addWidget( hideObjects );
	vlayAdvancedTab->addLayout( hlayAdvHideTimeScale );
	vlayAdvancedTab->addWidget( HideBox );
	vlayAdvancedTab->addItem( spacerAdvTab );

	DisplayTabs->insertTab( AdvancedTab, i18n( "Advanced configuration options (tab label)", "Advanced" ) );

//*** Done Making Tabs ***//

//Activate main layout
	hlay->addWidget( DisplayBox );
	hlay->activate();

//Connect Signals to Slots.  Each modifiable option widget shoudl be connected to updateDisplay()
//Dialog buttons
  connect( this, SIGNAL( okClicked() ), this, SLOT( accept() ) ) ;
  connect( this, SIGNAL( cancelClicked() ), this, SLOT( reject() ) );

//Catalog Tab
	connect( showSAO, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( CatalogList, SIGNAL( clicked( QListViewItem* ) ), this, SLOT( updateDisplay() ) );
	connect( CatalogList, SIGNAL( selectionChanged() ), this, SLOT( selectCatalog() ) );
	connect( showStarNames, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( showStarMagnitude, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( magSpinBoxDrawStars, SIGNAL( valueChanged( int ) ), SLOT( changeMagDrawStars( int) ) );
	connect( magSpinBoxDrawStarInfo, SIGNAL( valueChanged( int ) ), SLOT( changeMagDrawInfo( int) ) );
	connect( AddCatalog, SIGNAL( clicked() ), this, SLOT( slotAddCatalog() ) );
	connect( RemoveCatalog, SIGNAL( clicked() ), this, SLOT( slotRemoveCatalog() ) );

//Solar System Tab
	connect( showSun, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( showMoon, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( showMercury, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( showVenus, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( showMars, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( showJupiter, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( showSaturn, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( showUranus, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( showNeptune, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( showPluto, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( showAll, SIGNAL( clicked() ), this, SLOT( markPlanets() ) );
	connect( showNone, SIGNAL( clicked() ), this, SLOT( unMarkPlanets() ) );
	connect( showPlanetNames, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( showPlanetImages, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( showAsteroids, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( showAsteroidNames, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( showComets, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( showCometNames, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( astDrawSpinBox, SIGNAL( valueChanged( int ) ), this, SLOT( changeAstDrawMagLimit( int ) ) );
	connect( astNameSpinBox, SIGNAL( valueChanged( int ) ), this, SLOT( changeAstNameMagLimit( int ) ) );
	connect( comNameSpinBox, SIGNAL( valueChanged( int ) ), this, SLOT( changeComNameMaxRad( int ) ) );
	connect( autoTrail, SIGNAL( clicked() ), this, SLOT( changeAutoTrail() ) );
	connect( fadePlanetTrails, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( ClearAllTrails, SIGNAL( clicked() ), this, SLOT( clearPlanetTrails() ) );

//Guide Tab
	connect( showConstellLines, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( showConstellNames, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( useLatinConstellNames, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( useLatinConstellNames, SIGNAL( clicked() ), this, SLOT( sendClearCache() ) );
	connect( useLocalConstellNames, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( useLocalConstellNames, SIGNAL( clicked() ), this, SLOT( sendClearCache() ) );
	connect( useAbbrevConstellNames, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( useAbbrevConstellNames, SIGNAL( clicked() ), this, SLOT( sendClearCache() ) );
	connect( showMilkyWay, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( showMilkyWayFilled, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( showGrid, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( showEquator, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( showEcliptic, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( showHorizon, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( showGround, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );

//Color Tab
	connect( ColorPalette, SIGNAL( clicked( QListBoxItem* ) ), this, SLOT( newColor( QListBoxItem* ) ) );
	connect( PresetBox, SIGNAL( highlighted( int ) ), this, SLOT( slotPreset( int ) ) );
	connect( AddPreset, SIGNAL( clicked() ), this, SLOT( slotAddPreset() ) );
	connect( RemovePreset, SIGNAL( clicked() ), this, SLOT( slotRemovePreset() ) );
	connect( IntensityBox, SIGNAL (valueChanged (int)), SLOT (changeStarColorIntensity(int)) );
	connect( StarColorMode, SIGNAL( activated( int ) ), this, SLOT( changeStarColorMode( int ) ) );

//Advanced Tab
	connect( useRefraction, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( animateSlewing, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( autoLabel, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( hideSpinBox, SIGNAL( scaleChanged( float ) ), this, SLOT( changeSlewTimeScale( float ) ) );
	connect( hideObjects, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( hideStars, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( hidePlanets, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( hideMess, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( hideNGC, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( hideIC, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( hideMW, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( hideCNames, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( hideCLines, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( hideGrid, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( magSpinBoxHideStars, SIGNAL( valueChanged( int ) ), SLOT( changeMagHideStars( int) ) );

//Coordinate Systems
	connect( EquatRadio, SIGNAL( clicked() ), this, SLOT( changeCoordSys() ) );
	connect( AltAzRadio, SIGNAL( clicked() ), this, SLOT( changeCoordSys() ) );

}

ViewOpsDialog::~ViewOpsDialog(){
}

void ViewOpsDialog::changeMagDrawStars( int newValue )
{
	float fNewValue = ( newValue * 1.0) / 10.0;

	ksw->data()->setMagnitude( fNewValue );

	// force redraw
	ksw->map()->forceUpdate();
}

void ViewOpsDialog::changeMagHideStars( int newValue )
{
	float fNewValue = ( newValue * 1.0) / 10.0;
	ksw->options()->magLimitHideStar = fNewValue;

//no need to redraw
//	ksw->map()->forceUpdate();
}

void ViewOpsDialog::changeMagDrawInfo( int newValue )
{
    kdDebug()<<"magnitude limit draw star info "<<newValue<<endl;
	float fNewValue = ( newValue * 1.0) / 10.0;
	ksw->options()->magLimitDrawStarInfo = fNewValue;
	// force redraw
	ksw->map()->forceUpdate();
}

void ViewOpsDialog::newColor( QListBoxItem *item ) {
	QPixmap temp( 30, 20 );
	QColor newColor;
	unsigned int i;

	for ( i=0; i < ksw->options()->colorScheme()->numberOfColors(); ++i ) {
		if ( item->text() == ksw->options()->colorScheme()->nameAt( i ) ) {
                    QColor col( ksw->options()->colorScheme()->colorAt( i ) );
                    if(KColorDialog::getColor( col ))
                        newColor = col;
			break;
		}
	}

	//newColor will only be valid if the above if statement was found to be true during one of the for loop iterations
	if ( newColor.isValid() ) {
		temp.fill( newColor );
		ColorPalette->changeItem( temp, item->text(), ColorPalette->index( item ) );
		ksw->options()->colorScheme()->setColor( ksw->options()->colorScheme()->keyAt( i ), newColor.name() );
	}

	ksw->map()->forceUpdate();
}

void ViewOpsDialog::slotPreset( int index ) {
	QStringList::Iterator it = PresetFileList.at( index );
	bool result = setColors( *it );
	if (!result) {
		QString message = i18n( "The specified color scheme file (%1) could not be found, or was corrupt." ).arg( QString(*it) );
		KMessageBox::sorry( 0, message, i18n( "Could Not Set Color Scheme" ) );
	}
}

void ViewOpsDialog::slotAddPreset( void ) {
//	QFile file;

//KDE3-only version (incompatible w/KDE2 because of different arguments)
//	bool okPressed = false;
//  QString schemename = KLineEditDlg::getText( i18n( "New Color Scheme" ),
//											i18n( "Enter a name for the new color scheme:" ),
//											QString::null, &okPressed, 0 );
	KLineEditDlg schemeDlg( i18n( "Enter a name for the new color scheme:" ),
											QString::null, 0 );
	int result = schemeDlg.exec();
	if ( result ) {
		QString filename = schemeDlg.text().lower().stripWhiteSpace();

		if ( !filename.isEmpty() ) {
			if ( ksw->options()->colorScheme()->save( schemeDlg.text() ) ) {
				PresetBox->insertItem( schemeDlg.text() );
				PresetFileList.append( filename + ".colors" );
				ksw->addColorMenuItem( schemeDlg.text(), "cs_" + filename.local8Bit() );
			}
		}
	}
}

void ViewOpsDialog::slotRemovePreset( void ) {
	QString name = PresetBox->currentText();
	QString filename = PresetFileList[ PresetBox->currentItem() ];
	QFile cdatFile;
	cdatFile.setName( locateLocal( "appdata", "colors.dat" ) ); //determine filename in local user KDE directory tree.

	//Remove entry from the ListBox and from the QStringList holding filenames.
	//We don't want another color scheme to be selected, so first
	//temporarily disconnect the "highlighted" signal.
	disconnect( PresetBox, SIGNAL( highlighted( int ) ), this, SLOT( slotPreset( int ) ) );

	PresetFileList.remove( PresetFileList.at( PresetBox->currentItem() ) );
	PresetBox->removeItem( PresetBox->currentItem() );

	//Reconnect the "highlighted" signal
	connect( PresetBox, SIGNAL( highlighted( int ) ), this, SLOT( slotPreset( int ) ) );

	//Remove action from color-schemes menu
	ksw->removeColorMenuItem( QString("cs_" + filename.left( filename.find(".colors"))).local8Bit() );

	if ( !cdatFile.exists() || !cdatFile.open( IO_ReadWrite ) ) {
		QString message = i18n( "Local color scheme index file could not be opened.\nScheme cannot be removed." );
		KMessageBox::sorry( 0, message, i18n( "Could Not Open File" ) );
	} else {
		//Read the contents of colors.dat into a QStringList, except for the entry to be removed.
		QTextStream stream( &cdatFile );
		QStringList slist;
		bool removed = false;

		while ( !stream.eof() ) {
			QString line = stream.readLine();
			if ( line.left( line.find(':') ) != name ) slist.append( line );
			else removed = true;
		}

		if ( removed ) { //Entry was removed; delete the corresponding .colors file.
			QFile colorFile;
			colorFile.setName( locateLocal( "appdata", filename ) ); //determine filename in local user KDE directory tree.
			if ( !colorFile.remove() ) {
				QString message = i18n( "Could not delete the file: %1" ).arg( colorFile.name() );
				KMessageBox::sorry( 0, message, i18n( "Error Deleting File" ) );
			}

			//remove the old colors.dat file, and rebuild it with the modified string list.
			cdatFile.remove();
			cdatFile.open( IO_ReadWrite );
			QTextStream stream2( &cdatFile );
			for( unsigned int i=0; i<slist.count(); ++i )
				stream << slist[i] << endl;
		} else {
			QString message = i18n( "Could not find an entry named %1 in colors.dat" ).arg( name );
			KMessageBox::sorry( 0, message, i18n( "Scheme Not Found in colors.dat" ) );
		}
		cdatFile.close();
	}
}

bool ViewOpsDialog::setColors( QString filename ) {
	QPixmap *temp = new QPixmap( 30, 20 );

	QFile test;
	test.setName( locateLocal( "appdata", filename ) ); //try filename in local user KDE directory tree.
	if ( test.exists() ) RemovePreset->setEnabled( true );
	else RemovePreset->setEnabled( false );
	test.close();

	if ( ksw->options()->colorScheme()->load( filename ) ) {
	  if ( ksw->map()->starColorMode() != ksw->options()->colorScheme()->starColorMode() )
	    ksw->map()->setStarColorMode( ksw->options()->colorScheme()->starColorMode() );

		for ( unsigned int i=0; i < ksw->options()->colorScheme()->numberOfColors(); ++i ) {
			temp->fill( QColor( ksw->options()->colorScheme()->colorAt( i ) ) );
			ColorPalette->changeItem( *temp, ksw->options()->colorScheme()->nameAt( i ), i );
		}
	} else {
		return false;
	}

	ksw->map()->forceUpdate();
	return true;
}

void ViewOpsDialog::updateDisplay( void ) {
//Set the drawPlanets or drawDeepSky meta-options to true if the options which
//these substitute for are altered.
	if ( sender()->name() == QString( "catalog_name" ) )
		ksw->options()->drawDeepSky = true;

	if ( sender()->name() == QString( "show_sun" ) ||
			sender()->name() == QString( "show_moon" ) ||
			sender()->name() == QString( "show_mercury" ) ||
			sender()->name() == QString( "show_venus" ) ||
			sender()->name() == QString( "show_mars" ) ||
			sender()->name() == QString( "show_jupiter" ) ||
			sender()->name() == QString( "show_saturn" ) ||
			sender()->name() == QString( "show_uranus" ) ||
			sender()->name() == QString( "show_neptune" ) ||
			sender()->name() == QString( "show_pluto" ) ||
			sender()->name() == QString( "show_asteroids" ) ||
			sender()->name() == QString( "show_asteroid_names" ) ||
			sender()->name() == QString( "show_comets" ) ||
			sender()->name() == QString( "show_comet_names" ) ||
			sender()->name() == QString( "show_planet_names" ) ||
			sender()->name() == QString( "show_planet_images" ) ||
			sender()->name() == QString( "show_all_planets" ) ||
			sender()->name() == QString( "show_none_planets" ) )
		ksw->options()->drawPlanets = true;

//Set options according to current settings
//Catalogs Tab
	ksw->options()->drawSAO = showSAO->isChecked();
	ksw->options()->drawMessier = showMessier->isOn();
	ksw->options()->drawMessImages = showMessImages->isOn();
	ksw->options()->drawNGC = showNGC->isOn();
	ksw->options()->drawIC = showIC->isOn();
	for ( unsigned int i=0; i<ksw->options()->CatalogCount; ++i ) {
		//KDE2.2 has no findItem function
		//QCheckListItem *item = (QCheckListItem*)(CatalogList->findItem( ksw->options()->CatalogName[i], 0 ));
		for ( QCheckListItem *item = (QCheckListItem*)CatalogList->firstChild(); item; item = (QCheckListItem*)item->nextSibling() )
			if ( item->text( 0 ) == ksw->options()->CatalogName[i] )
				ksw->options()->drawCatalog[i] = item->isOn();
	}

	ksw->options()->drawStarName = showStarNames->isChecked();
	ksw->options()->drawStarMagnitude = showStarMagnitude->isChecked();
	//star options enabled only if showSAO is checked...
	showStarNames->setEnabled( showSAO->isChecked() );
	showStarMagnitude->setEnabled( showSAO->isChecked() );
	magSpinBoxDrawStars->setEnabled( showSAO->isChecked() );
	textLabelMagStars->setEnabled( showSAO->isChecked() );
	magSpinBoxDrawStarInfo->setEnabled( showSAO->isChecked() );
	textLabelMagStarInfo->setEnabled( showSAO->isChecked() );

//Planets Tab
	ksw->options()->drawSun = showSun->isChecked();
	ksw->options()->drawMoon = showMoon->isChecked();
	ksw->options()->drawMercury = showMercury->isChecked();
	ksw->options()->drawVenus = showVenus->isChecked();
	ksw->options()->drawMars = showMars->isChecked();
	ksw->options()->drawJupiter = showJupiter->isChecked();
	ksw->options()->drawSaturn = showSaturn->isChecked();
	ksw->options()->drawUranus = showUranus->isChecked();
	ksw->options()->drawNeptune = showNeptune->isChecked();
	ksw->options()->drawPluto = showPluto->isChecked();
	ksw->options()->drawAsteroids = showAsteroids->isChecked();
	ksw->options()->drawComets = showComets->isChecked();
	ksw->options()->drawAsteroidName = showAsteroidNames->isChecked();
	ksw->options()->drawCometName = showCometNames->isChecked();
	ksw->options()->drawPlanetName = showPlanetNames->isChecked();
	ksw->options()->drawPlanetImage = showPlanetImages->isChecked();

	showAsteroidNames->setEnabled( showAsteroids->isChecked() );
	showCometNames->setEnabled( showComets->isChecked() );
	astDrawSpinBox->setEnabled( showAsteroids->isChecked() );
	astNameSpinBox->setEnabled( showAsteroids->isChecked() && showAsteroidNames->isChecked() );
	comNameSpinBox->setEnabled( showComets->isChecked() && showCometNames->isChecked() );

	ksw->options()->fadePlanetTrails = fadePlanetTrails->isChecked();
	ksw->options()->useAutoTrail = autoTrail->isChecked();

//Guides Tab
	ksw->options()->drawConstellLines = showConstellLines->isChecked();
	ksw->options()->drawConstellNames = showConstellNames->isChecked();
	ksw->options()->useLatinConstellNames = useLatinConstellNames->isChecked();
	ksw->options()->useLocalConstellNames = useLocalConstellNames->isChecked();
	ksw->options()->useAbbrevConstellNames = useAbbrevConstellNames->isChecked();
	ksw->options()->drawMilkyWay = showMilkyWay->isChecked();
	ksw->options()->fillMilkyWay = showMilkyWayFilled->isChecked();
	ksw->options()->drawGrid = showGrid->isChecked();
	ksw->options()->drawEquator = showEquator->isChecked();
	ksw->options()->drawEcliptic = showEcliptic->isChecked();
	ksw->options()->drawHorizon = showHorizon->isChecked();
	ksw->options()->drawGround = showGround->isChecked();
	//constellation name options enabled only if showConstellationNames is checked...
	ConstellOptions->setEnabled( showConstellNames->isChecked() );
	//fill MW checkbox enabled only if showMilkyWay is checked...
	showMilkyWayFilled->setEnabled( showMilkyWay->isChecked() );

	//Advanced Tab
	ksw->options()->useRefraction = useRefraction->isChecked();
	ksw->options()->useAnimatedSlewing = animateSlewing->isChecked();
	ksw->options()->useAutoLabel = autoLabel->isChecked();
	ksw->options()->hideOnSlew = hideObjects->isChecked();
	ksw->options()->hideStars = hideStars->isChecked();
	ksw->options()->hidePlanets = hidePlanets->isChecked();
	ksw->options()->hideMess = hideMess->isChecked();
	ksw->options()->hideNGC = hideNGC->isChecked();
	ksw->options()->hideIC = hideIC->isChecked();
	ksw->options()->hideMW = hideMW->isChecked();
	ksw->options()->hideCNames = hideCNames->isChecked();
	ksw->options()->hideCLines = hideCLines->isChecked();
	ksw->options()->hideGrid = hideGrid->isChecked();
	//HideBox widgets enabled only if hideObjects is checked...
	hideSpinBox->setEnabled( hideObjects->isChecked() );
	hideStars->setEnabled( hideObjects->isChecked() );
	magSpinBoxHideStars->setEnabled( hideObjects->isChecked() );
	hidePlanets->setEnabled( hideObjects->isChecked() );
	hideMess->setEnabled( hideObjects->isChecked() );
	hideNGC->setEnabled( hideObjects->isChecked() );
	hideIC->setEnabled( hideObjects->isChecked() );
	hideMW->setEnabled( hideObjects->isChecked() );
	hideCNames->setEnabled( hideObjects->isChecked() );
	hideCLines->setEnabled( hideObjects->isChecked() );
	hideGrid->setEnabled( hideObjects->isChecked() );

	// update time for all objects because they might be not initialized
	// it's needed when using horizontal coordinates
	ksw->data()->setFullTimeUpdate();
	ksw->updateTime();

	ksw->map()->forceUpdate();
}

void ViewOpsDialog::changeCoordSys( void ) {
	ksw->options()->useAltAz = AltAzRadio->isChecked();
	showGround->setEnabled( AltAzRadio->isChecked() );
	ksw->map()->forceUpdate();
}

void ViewOpsDialog::changeStarColorIntensity( int newValue ) {
	ksw->map()->setStarColorIntensity( newValue );
//	ksw->options()->starColorIntensity = ksw->map()->starColorIntensity();
	ksw->options()->colorScheme()->setStarColorIntensity( newValue );
	ksw->map()->forceUpdate();
}

void ViewOpsDialog::changeStarColorMode( int newValue ) {
	ksw->map()->setStarColorMode( newValue );
	ksw->options()->colorScheme()->setStarColorMode( newValue );
	if (newValue) IntensityBox->setEnabled( false );
	else IntensityBox->setEnabled( true );
	ksw->map()->forceUpdate();
}

void ViewOpsDialog::changeAstDrawMagLimit( int newValue ) {
	float fNewValue = float( newValue ) / 10.0;
	ksw->options()->magLimitAsteroid = fNewValue;
	// force redraw
	ksw->map()->forceUpdate();
}

void ViewOpsDialog::changeAstNameMagLimit( int newValue ) {
	float fNewValue = float( newValue ) / 10.0;
	ksw->options()->magLimitAsteroidName = fNewValue;
	// force redraw
	ksw->map()->forceUpdate();
}

void ViewOpsDialog::changeComNameMaxRad( int newValue ) {
	float fNewValue = float( newValue ) / 10.0;
	ksw->options()->maxRadCometName = fNewValue;
	// force redraw
	ksw->map()->forceUpdate();
}

void ViewOpsDialog::markPlanets( void ) {
	showSun->setChecked( true );
	showMoon->setChecked( true );
	showMercury->setChecked( true );
	showVenus->setChecked( true );
	showMars->setChecked( true );
	showJupiter->setChecked( true );
	showSaturn->setChecked( true );
	showUranus->setChecked( true );
	showNeptune->setChecked( true );
	showPluto->setChecked( true );
	updateDisplay();
}

void ViewOpsDialog::unMarkPlanets( void ) {
	showSun->setChecked( false );
	showMoon->setChecked( false );
 	showMercury->setChecked( false );
	showVenus->setChecked( false );
	showMars->setChecked( false );
	showJupiter->setChecked( false );
	showSaturn->setChecked( false );
	showUranus->setChecked( false );
	showNeptune->setChecked( false );
	showPluto->setChecked( false );
	updateDisplay();
}

void ViewOpsDialog::sendClearCache() {
	emit clearCache();
}

void ViewOpsDialog::slotAddCatalog() {
	AddCatDialog ac(this);
	if ( ac.exec()==QDialog::Accepted ) {
		//compute Horizontal coords for custom objects:
		for ( unsigned int i=0; i < ac.objectList().count(); ++i )
			ac.objectList().at(i)->EquatorialToHorizontal( ksw->LST(), ksw->geo()->lat() );

		//Add new custom catalog, based on the list of SkyObjects we just parsed
		ksw->data()->addCatalog( ac.name(), ac.objectList() );
		QCheckListItem *newCat = new QCheckListItem( CatalogList, ac.name(), QCheckListItem::CheckBox );
		newCat->setOn( true );
		CatalogList->insertItem( newCat );

		ksw->options()->CatalogCount = ++ksw->options()->CatalogCount;
		ksw->options()->CatalogName.append( ac.name() );
		ksw->options()->CatalogFile.append( ac.filename() );
		ksw->options()->drawCatalog.append( true );
//		kdWarning() << "CatalogCount: " << ksw->options()->CatalogCount << endl;
//		kdWarning() << "CatalogName.count(): " << ksw->options()->CatalogName.count() << endl;
		ksw->map()->forceUpdate();
	}
}

void ViewOpsDialog::slotRemoveCatalog() {
//Remove CatalogName, CatalogFile, and drawCatalog entries, and decrement CatalogCount
	for ( unsigned int i=0; i < ksw->options()->CatalogName.count(); ++i ) {
		if ( CatalogList->currentItem()->text( 0 ) == ksw->options()->CatalogName[i] ) {
			ksw->options()->CatalogName.remove( ksw->options()->CatalogName.at(i) );
			ksw->options()->CatalogFile.remove( ksw->options()->CatalogFile.at(i) );
			ksw->options()->drawCatalog.remove( ksw->options()->drawCatalog.at(i) );
			--ksw->options()->CatalogCount;
			break;
		}
	}

//Remove entry in the QListView
//	int i = CatalogList->itemPos( CatalogList->currentItem() );
	CatalogList->takeItem( CatalogList->currentItem() );
}

void ViewOpsDialog::selectCatalog() {
//If selected item is a custom catalog, enable the remove button (otherwise, disable it)
	RemoveCatalog->setEnabled( false );
	for ( unsigned int i=0; i < ksw->options()->CatalogName.count(); ++i ) {
		if ( CatalogList->currentItem()->text( 0 ) == ksw->options()->CatalogName[i] ) {
			RemoveCatalog->setEnabled( true );
			break;
		}
	}
}

void ViewOpsDialog::changeSlewTimeScale( float f ) {
	if ( f < 0.0 ) {
		f = 0.0;
		hideSpinBox->tsbox()->changeScale( 0.0 );
	}
	ksw->options()->slewTimeScale = f;
}

void ViewOpsDialog::changeAutoTrail( void ) {
	if ( ksw->data()->isSolarSystem( ksw->map()->focusObject() ) ) {
		if ( autoTrail->isChecked() ) {
			//add the temporary trail
			((KSPlanetBase*)ksw->map()->focusObject())->addToTrail();
			ksw->data()->temporaryTrail = true;
		} else {
			//remove the temporary trail
			((KSPlanetBase*)ksw->map()->focusObject())->clearTrail();
			ksw->data()->temporaryTrail = false;
		}
	}

	updateDisplay();
}

void ViewOpsDialog::clearPlanetTrails( void ) {
	//remove each solar system body's trail
	ksw->data()->PC->findByName("Sun")->clearTrail();
	ksw->data()->PC->findByName("Mercury")->clearTrail();
	ksw->data()->PC->findByName("Venus")->clearTrail();
	ksw->data()->Moon->clearTrail();
	ksw->data()->PC->findByName("Mars")->clearTrail();
	ksw->data()->PC->findByName("Jupiter")->clearTrail();
	ksw->data()->PC->findByName("Saturn")->clearTrail();
	ksw->data()->PC->findByName("Uranus")->clearTrail();
	ksw->data()->PC->findByName("Neptune")->clearTrail();
	ksw->data()->PC->findByName("Pluto")->clearTrail();

	for ( KSPlanetBase *ksp = ksw->data()->asteroidList.first(); ksp; ksp = ksw->data()->asteroidList.next() )
		ksp->clearTrail();

	for ( KSPlanetBase *ksp = ksw->data()->cometList.first(); ksp; ksp = ksw->data()->cometList.next() )
		ksp->clearTrail();

	ksw->map()->forceUpdate();
}
