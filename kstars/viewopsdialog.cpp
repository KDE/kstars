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


#include <klocale.h>
#include <knuminput.h>

#include <qlayout.h>
#include <qtabwidget.h>
#include <qwidget.h>
#include <qgroupbox.h>
#include <qbuttongroup.h>
#include <qradiobutton.h>
#include <qcheckbox.h>
#include <qframe.h>
#include <qbutton.h>
#include <qcolor.h>
#include <qfont.h>
#include <qcolordialog.h>
#include <qlabel.h>

#include "kstars.h"
#include "magnitudespinbox.h"
#include "viewopsdialog.h"

ViewOpsDialog::ViewOpsDialog( QWidget *parent )
	: KDialogBase( KDialogBase::Plain, i18n( "Display Options" ), Ok|Cancel, Ok, parent ) {
	QFrame *page = plainPage();
	KStars *ksw = (KStars *)parent;

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

	CoordsGroup = new QButtonGroup( 1, Qt::Vertical, i18n( "Coordinate system" ), DisplayBox );
	
	EquatRadio = new QRadioButton( i18n( "Equatorial" ), CoordsGroup );
	EquatRadio->setChecked( !ksw->GetOptions()->useAltAz );

	AltAzRadio = new QRadioButton( i18n( "Altitude/Azimuth" ), CoordsGroup );
	AltAzRadio->setChecked( ksw->GetOptions()->useAltAz );

	DisplayBoxLayout->addSpacing( 10 );
	DisplayBoxLayout->addWidget( DisplayTabs );
	DisplayBoxLayout->addWidget( CoordsGroup );

	// construct stars tab	
	StarsTab = new  QWidget( DisplayTabs, "StarsTab" );
	vlayStarsTab = new QVBoxLayout( StarsTab );
	vlayStarsTab->setSpacing( 6 );
	vlayStarsTab->setMargin( 11 );

	showBSC = new QCheckBox( i18n( "SAO Star Catalog" ), StarsTab );
	QFont stdFont(  showBSC->font() );

	showBSC->setFont( stdFont );
	showBSC->setChecked( ksw->GetOptions()->drawBSC );

	// Spin box : show stars brighter than magnitude limit
	int intMagLimitDrawStars = 10*((int)(ksw->GetOptions()->magLimitDrawStar));

	int faintLimit = int( 10.*ksw->GetData()->starList.at(ksw->GetData()->starList.count()-1)->mag );
	magSpinBoxDrawStars = new MagnitudeSpinBox( 0, faintLimit, StarsTab );
	magSpinBoxDrawStars->setFont( stdFont );	
	magSpinBoxDrawStars->setValue( intMagLimitDrawStars );

	QLabel *textLabelMagStars = new QLabel( StarsTab, "LabelMagStars" );
	textLabelMagStars->setText( i18n( "Show stars brighter than" ) );
	textLabelMagStars->setFont( stdFont );	

	// Spin box : show stars names for stars brighter than magnitude limit
	int intMagLimitDrawStarInfo = 10*((int)(ksw->GetOptions()->magLimitDrawStarInfo));
	//	float magLimitDrawStarName;
	magSpinBoxDrawStarInfo = new MagnitudeSpinBox( 0, 90, StarsTab );
	magSpinBoxDrawStarInfo->setFont( stdFont );	
	magSpinBoxDrawStarInfo->setValue( intMagLimitDrawStarInfo );

	QLabel *textLabelMagStarInfo = new QLabel( StarsTab, "LabelMagStarNames" );
	textLabelMagStarInfo->setText( i18n( "for stars brighter than" ) );
	textLabelMagStarInfo->setFont( stdFont );	

	showStarNames = new QCheckBox( i18n( "show name" ), StarsTab );
	showStarNames->setFont( stdFont );
	showStarNames->setChecked( ksw->GetOptions()->drawStarName );

	showStarMagnitude = new QCheckBox( i18n( "show magnitude" ), StarsTab );
	showStarMagnitude->setFont( stdFont );
	showStarMagnitude->setChecked( ksw->GetOptions()->drawStarMagnitude );

	// MagnitudeSpinBox* magSpinBoxDrawStarNames;
	QSpacerItem *spacerStarsTab = new QSpacerItem( 20, 20, QSizePolicy::Minimum, QSizePolicy::Expanding );

	
	vlayStarsTab->addWidget( showBSC );
	vlayStarsTab->addWidget( textLabelMagStars );
	vlayStarsTab->addWidget( magSpinBoxDrawStars );
	vlayStarsTab->addSpacing( 20 );
	vlayStarsTab->addWidget( textLabelMagStarInfo );
	vlayStarsTab->addWidget( magSpinBoxDrawStarInfo );
	vlayStarsTab->addWidget( showStarNames );
	vlayStarsTab->addWidget( showStarMagnitude );
	vlayStarsTab->addItem( spacerStarsTab );

	DisplayTabs->insertTab( StarsTab, i18n( "Stars" ) );

//Construct Catalogs tab:
	CatalogTab = new QWidget( DisplayTabs, "CatalogTab" );
	vlayCatTab = new QVBoxLayout( CatalogTab );
	vlayCatTab->setSpacing( 6 );
	vlayCatTab->setMargin( 11 );

	showMessier = new QCheckBox( i18n( "Messier Objects (symbols)" ), CatalogTab );
	showMessier->setFont( stdFont );
	showMessier->setChecked( ksw->GetOptions()->drawMessier );

	showMessImages = new QCheckBox( i18n( "Messier Objects (images)" ), CatalogTab );
	showMessImages->setFont( stdFont );
	showMessImages->setChecked( ksw->GetOptions()->drawMessImages );

	showNGC = new QCheckBox( i18n( "NGC Catalog" ), CatalogTab );
	showNGC->setFont( stdFont );
	showNGC->setChecked( ksw->GetOptions()->drawNGC );

	showIC = new QCheckBox( i18n( "IC Catalog" ), CatalogTab );
	showIC->setFont( stdFont );
	showIC->setChecked( ksw->GetOptions()->drawIC );

	QSpacerItem *spacerCatTab = new QSpacerItem( 20, 20, QSizePolicy::Minimum, QSizePolicy::Expanding );

	vlayCatTab->addWidget( showMessier );
	vlayCatTab->addWidget( showMessImages );
	vlayCatTab->addWidget( showNGC );
	vlayCatTab->addWidget( showIC );
	vlayCatTab->addItem( spacerCatTab );

	DisplayTabs->insertTab( CatalogTab, i18n( "Catalogs" ) );

//Construct Guides tab:
	GuideTab = new QWidget( DisplayTabs, "GuideTab" );
	vlayGuideTab = new QVBoxLayout( GuideTab );
	vlayGuideTab->setSpacing( 6 );
	vlayGuideTab->setMargin( 11 );

	showConstellLines = new QCheckBox( i18n( "Constellation Lines" ), GuideTab );
	showConstellLines->setFont( stdFont );
	showConstellLines->setChecked( ksw->GetOptions()->drawConstellLines );

	showConstellNames = new QCheckBox( i18n( "Constellation Names" ), GuideTab );
	showConstellNames->setFont( stdFont );
	showConstellNames->setChecked( ksw->GetOptions()->drawConstellNames );

	useLatinConstellNames = new QCheckBox( i18n( "Use Latin Constellation Names" ), GuideTab );
	useLatinConstellNames->setFont ( stdFont );
	useLatinConstellNames->setChecked( ksw->GetOptions()->useLatinConstellNames );
	
	showMilkyWay = new QCheckBox( i18n( "Milky Way" ), GuideTab );
	showMilkyWay->setFont( stdFont );
	showMilkyWay->setChecked( ksw->GetOptions()->drawMilkyWay );

	showEquator = new QCheckBox( i18n( "Celestial Equator" ), GuideTab );
	showEquator->setFont( stdFont );
	showEquator->setChecked( ksw->GetOptions()->drawEquator );

	showEcliptic = new QCheckBox( i18n( "Ecliptic" ), GuideTab );
	showEcliptic->setFont( stdFont );
	showEcliptic->setChecked( ksw->GetOptions()->drawEcliptic );

	showHorizon = new QCheckBox( i18n( "Horizon (line)" ), GuideTab );
	showHorizon->setFont( stdFont );
	showHorizon->setChecked( ksw->GetOptions()->drawHorizon );

	showGround = new QCheckBox( i18n( "Opaque Ground" ), GuideTab );
	showGround->setFont( stdFont );
	showGround->setChecked( ksw->GetOptions()->drawGround );
	if ( !ksw->GetOptions()->useAltAz ) showGround->setEnabled( false );
	
	QSpacerItem *spacerGuideTab = new QSpacerItem( 20, 20, QSizePolicy::Minimum, QSizePolicy::Expanding );

	vlayGuideTab->addWidget( showConstellLines );
	vlayGuideTab->addWidget( showConstellNames );
	vlayGuideTab->addWidget( useLatinConstellNames );
	vlayGuideTab->addWidget( showMilkyWay );
	vlayGuideTab->addWidget( showEquator );
	vlayGuideTab->addWidget( showEcliptic );
	vlayGuideTab->addWidget( showHorizon );
	vlayGuideTab->addWidget( showGround );
	vlayGuideTab->addItem( spacerGuideTab );

	DisplayTabs->insertTab( GuideTab, i18n( "Guides" ) );

//Construct Planets tab:
	PlanetTab = new QWidget( DisplayTabs, "PlanetTab" );
	vlayPlanetTab = new QVBoxLayout( PlanetTab );
	vlayPlanetTab->setSpacing( 6 );
	vlayPlanetTab->setMargin( 11 );
	glayPlanetTab = new QGridLayout( 5, 2 );
	
	showSun = new QCheckBox( i18n( "The Sun" ), PlanetTab );
	showSun->setFont( stdFont );
	showSun->setChecked( ksw->GetOptions()->drawSun );

	showMoon = new QCheckBox( i18n( "The Moon" ), PlanetTab );
  showMoon->setFont( stdFont );
  showMoon->setChecked( ksw->GetOptions()->drawMoon );
  showMoon->setChecked( ksw->GetOptions()->drawMoon );
		
	showMercury = new QCheckBox( i18n( "Mercury" ), PlanetTab );
	showMercury->setFont( stdFont );
	showMercury->setChecked( ksw->GetOptions()->drawMercury );

	showVenus = new QCheckBox( i18n( "Venus" ), PlanetTab );
	showVenus->setFont( stdFont );
	showVenus->setChecked( ksw->GetOptions()->drawVenus );

	showMars = new QCheckBox( i18n( "Mars" ), PlanetTab );
	showMars->setFont( stdFont );
	showMars->setChecked( ksw->GetOptions()->drawMars );

	showJupiter = new QCheckBox( i18n( "Jupiter" ), PlanetTab );
	showJupiter->setFont( stdFont );
	showJupiter->setChecked( ksw->GetOptions()->drawJupiter );

	showSaturn = new QCheckBox( i18n( "Saturn" ), PlanetTab );
	showSaturn->setFont( stdFont );
	showSaturn->setChecked( ksw->GetOptions()->drawSaturn );

	showUranus = new QCheckBox( i18n( "Uranus" ), PlanetTab );
	showUranus->setFont( stdFont );
	showUranus->setChecked( ksw->GetOptions()->drawUranus );

	showNeptune = new QCheckBox( i18n( "Neptune" ), PlanetTab );
	showNeptune->setFont( stdFont );
	showNeptune->setChecked( ksw->GetOptions()->drawNeptune );

	showPluto = new QCheckBox( i18n( "Pluto" ), PlanetTab );
	showPluto->setFont( stdFont );
	showPluto->setChecked( ksw->GetOptions()->drawPluto );
	
	QSpacerItem *spacerPlanetTab = new QSpacerItem( 20, 20, QSizePolicy::Minimum, QSizePolicy::Expanding );

	glayPlanetTab->addWidget( showSun, 0, 0 );
	glayPlanetTab->addWidget( showMoon, 1, 0 );
	glayPlanetTab->addWidget( showMercury, 2, 0 );
	glayPlanetTab->addWidget( showVenus, 3, 0 );
	glayPlanetTab->addWidget( showMars, 4, 0 );
	glayPlanetTab->addWidget( showJupiter, 0, 1 );
	glayPlanetTab->addWidget( showSaturn, 1, 1 );
	glayPlanetTab->addWidget( showUranus, 2, 1 );
	glayPlanetTab->addWidget( showNeptune, 3, 1 );
	glayPlanetTab->addWidget( showPluto, 4, 1 );

	vlayPlanetTab->addLayout( glayPlanetTab );
  vlayPlanetTab->addItem( spacerPlanetTab );

	DisplayTabs->insertTab( PlanetTab, i18n( "Solar System" ) );

//Construct the Color Tab
	ColorTab = new QWidget( DisplayTabs, "ColorTab" );
	vlayColorTab = new QVBoxLayout( ColorTab );
	glayColorTab = new QGridLayout( 9, 2 );
	vlayColorTab->setSpacing( 6 );
	vlayColorTab->setMargin( 15 );
	
	ChangeCSky = new QPushButton( i18n( "Sky" ), ColorTab );
	ChangeCSky->setPalette( QPalette( QColor( ksw->GetOptions()->colorSky ) ) );
	ChangeCMess = new QPushButton( i18n( "Messier Object" ), ColorTab );
	ChangeCMess->setPalette( QPalette( QColor( ksw->GetOptions()->colorMess ) ) );
	ChangeCNGC = new QPushButton( i18n( "NGC Object" ), ColorTab );
	ChangeCNGC->setPalette( QPalette( QColor( ksw->GetOptions()->colorNGC ) ) );
	ChangeCIC = new QPushButton( i18n( "IC Object" ), ColorTab );
	ChangeCIC->setPalette( QPalette( QColor( ksw->GetOptions()->colorIC ) ) );
	ChangeCHST = new QPushButton( i18n( "Object w/ Links" ), ColorTab );
	ChangeCHST->setPalette( QPalette( QColor( ksw->GetOptions()->colorHST ) ) );
	ChangeCStarText = new QPushButton( i18n( "Star Name" ), ColorTab );
	ChangeCStarText->setPalette( QPalette( QColor( ksw->GetOptions()->colorSName ) ) );
	ChangeCEquator = new QPushButton( i18n( "Equator" ), ColorTab );
	ChangeCEquator->setPalette( QPalette( QColor( ksw->GetOptions()->colorEq ) ) );
	ChangeCEcliptic = new QPushButton( i18n( "Ecliptic" ), ColorTab );
	ChangeCEcliptic->setPalette( QPalette( QColor( ksw->GetOptions()->colorEcl ) ) );
	ChangeCHorizon = new QPushButton( i18n( "Horizon" ), ColorTab );
	ChangeCHorizon->setPalette( QPalette( QColor( ksw->GetOptions()->colorHorz ) ) );
	ChangeCMilkyWay = new QPushButton( i18n( "Milky Way" ), ColorTab );
	ChangeCMilkyWay->setPalette( QPalette( QColor( ksw->GetOptions()->colorMW ) ) );
	ChangeCConstLine = new QPushButton( i18n( "Constell. Line" ), ColorTab );
	ChangeCConstLine->setPalette( QPalette( QColor( ksw->GetOptions()->colorCLine ) ) );
	ChangeCConstText = new QPushButton( i18n( "Constell. Name" ), ColorTab );
	ChangeCConstText->setPalette( QPalette( QColor( ksw->GetOptions()->colorCName ) ) );

	NightColors = new QPushButton( i18n( "Night Vision"), ColorTab );
	ResetColors = new QPushButton( i18n( "Default Colors"), ColorTab );
	
// the spinbox for colorintensity of stars
	IntensityBox = new KIntSpinBox (0, 10, 1, ksw->GetOptions()->starColorIntensity, 10, ColorTab);
	QLabel *intensity = new QLabel (IntensityBox, i18n ("Color &Intensity of stars: "), ColorTab);
	intensity->setAlignment ( AlignRight | AlignVCenter );
	
// Blank Widgets
	QWidget *Blank1 = new QWidget( ColorTab );
	QWidget *Blank2 = new QWidget( ColorTab );
	Blank1->setMinimumHeight( 10 );
	Blank2->setMinimumHeight( 10 );

//Pack the widgets into the layouts
	glayColorTab->addWidget( ChangeCSky, 0, 0 );
	glayColorTab->addWidget( ChangeCMess, 1, 0 );
	glayColorTab->addWidget( ChangeCNGC, 2, 0 );
	glayColorTab->addWidget( ChangeCIC, 3, 0 );
	glayColorTab->addWidget( ChangeCHST, 4, 0 );
	glayColorTab->addWidget( ChangeCStarText, 5, 0 );
	glayColorTab->addWidget( ChangeCEquator, 0, 1 );
	glayColorTab->addWidget( ChangeCEcliptic, 1, 1 );
	glayColorTab->addWidget( ChangeCHorizon, 2, 1 );
	glayColorTab->addWidget( ChangeCMilkyWay, 3, 1 );
	glayColorTab->addWidget( ChangeCConstLine, 4, 1 );
	glayColorTab->addWidget( ChangeCConstText, 5, 1 );
	glayColorTab->addWidget( Blank1, 6, 0 );
	glayColorTab->addWidget( Blank2, 6, 1 );
	glayColorTab->addWidget( NightColors, 7, 0 );
	glayColorTab->addWidget( ResetColors, 7, 1 );
	glayColorTab->addWidget (intensity, 8, 0);
	glayColorTab->addWidget (IntensityBox, 8, 1);
	
	QSpacerItem *spacerColorTab = new QSpacerItem( 20, 20, QSizePolicy::Minimum, QSizePolicy::Expanding );

	vlayColorTab->addLayout( glayColorTab );
	vlayColorTab->addItem( spacerColorTab );
	
	DisplayTabs->insertTab( ColorTab, i18n( "Colors" ) );

	hlay->addWidget( DisplayBox );
	hlay->activate();

  connect( this, SIGNAL( okClicked() ), this, SLOT( accept() ) ) ;
  connect( this, SIGNAL( cancelClicked() ), this, SLOT( reject() ) );
  connect( ChangeCSky, SIGNAL( clicked() ), this, SLOT( newSkyColor() ) );
//  connect( ChangeCStar, SIGNAL( clicked() ), this, SLOT( newStarColor() ) );
  connect( ChangeCMess, SIGNAL( clicked() ), this, SLOT( newMessColor() ) );
  connect( ChangeCNGC, SIGNAL( clicked() ), this, SLOT( newNGCColor() ) );
  connect( ChangeCIC, SIGNAL( clicked() ), this, SLOT( newICColor() ) );
  connect( ChangeCHST, SIGNAL( clicked() ), this, SLOT( newHSTColor() ) );
  connect( ChangeCMilkyWay, SIGNAL( clicked() ), this, SLOT( newMWColor() ) );
  connect( ChangeCEquator, SIGNAL( clicked() ), this, SLOT( newEqColor() ) );
  connect( ChangeCEcliptic, SIGNAL( clicked() ), this, SLOT( newEclColor() ) );
  connect( ChangeCHorizon, SIGNAL( clicked() ), this, SLOT( newHorzColor() ) );
  connect( ChangeCConstLine, SIGNAL( clicked() ), this, SLOT( newCLineColor() ) );
  connect( ChangeCConstText, SIGNAL( clicked() ), this, SLOT( newCNameColor() ) );
  connect( ChangeCStarText, SIGNAL( clicked() ), this, SLOT( newSNameColor() ) );
	connect( NightColors, SIGNAL( clicked() ), this, SLOT( redColors() ) );
	connect( ResetColors, SIGNAL( clicked() ), this, SLOT( defaultColors() ) );
	connect( showBSC, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( showMessier, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( showMessImages, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( showNGC, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( showIC, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( showConstellLines, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( showConstellNames, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( useLatinConstellNames, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( showMilkyWay, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( showEquator, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( showEcliptic, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( showHorizon, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( showGround, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
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
	connect( EquatRadio, SIGNAL( clicked() ), this, SLOT( changeCoordSys() ) );
	connect( AltAzRadio, SIGNAL( clicked() ), this, SLOT( changeCoordSys() ) );
	connect( magSpinBoxDrawStars, SIGNAL( valueChanged( int ) ), SLOT( changeMagDrawStars( int) ) );
	connect( magSpinBoxDrawStarInfo, SIGNAL( valueChanged( int ) ), SLOT( changeMagDrawInfo( int) ) );
	connect( showStarNames, 		SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( showStarMagnitude, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect ( IntensityBox, SIGNAL (valueChanged (int)), SLOT (changeStarColorIntensity(int)) );
}

ViewOpsDialog::~ViewOpsDialog(){
}

void ViewOpsDialog::changeMagDrawStars( int newValue )
{
	KStars *ksw = (KStars *)parent();
	//debug( "magnitude limit draw star %d", newValue );
	float fNewValue = ( newValue * 1.0) / 10.0;
	ksw->GetOptions()->magLimitDrawStar = fNewValue;
	// force redraw
	ksw->skymap->Update();
}

void ViewOpsDialog::changeMagDrawInfo( int newValue )
{
	KStars *ksw = (KStars *)parent();
	debug( "magnitude limit draw star info %d", newValue );
	float fNewValue = ( newValue * 1.0) / 10.0;
	ksw->GetOptions()->magLimitDrawStarInfo = fNewValue;
	// force redraw
	ksw->skymap->Update();
}

void ViewOpsDialog::newSkyColor( void ) {
	KStars *ksw = (KStars *)parent();
	ksw->GetOptions()->colorSky = QColorDialog::getColor( QColor( ksw->GetOptions()->colorSky ) ).name();
	ChangeCSky->setPalette( QPalette( QColor( ksw->GetOptions()->colorSky ) ) );
	ksw->skymap->Update();
}	

//void ViewOpsDialog::newStarColor( void ) {
//	KStars *ksw = (KStars *)parent();
//	ksw->GetOptions()->colorStar = QColorDialog::getColor( QColor( ksw->GetOptions()->colorStar ) ).name();
//	ChangeCStar->setPalette( QPalette( QColor( ksw->GetOptions()->colorStar ) ) );
//	ksw->skymap->Update();
//}	

void ViewOpsDialog::newMessColor( void ) {
	KStars *ksw = (KStars *)parent();
	ksw->GetOptions()->colorMess = QColorDialog::getColor( QColor( ksw->GetOptions()->colorMess ) ).name();
	ChangeCMess->setPalette( QPalette( QColor( ksw->GetOptions()->colorMess ) ) );
	ksw->skymap->Update();
}	

void ViewOpsDialog::newNGCColor( void ) {
	KStars *ksw = (KStars *)parent();
	ksw->GetOptions()->colorNGC = QColorDialog::getColor( QColor( ksw->GetOptions()->colorNGC ) ).name();
	ChangeCNGC->setPalette( QPalette( QColor( ksw->GetOptions()->colorNGC ) ) );
	ksw->skymap->Update();
}	

void ViewOpsDialog::newICColor( void ) {
	KStars *ksw = (KStars *)parent();
	ksw->GetOptions()->colorIC = QColorDialog::getColor( QColor( ksw->GetOptions()->colorIC ) ).name();
	ChangeCIC->setPalette( QPalette( QColor( ksw->GetOptions()->colorIC ) ) );
	ksw->skymap->Update();
}	

void ViewOpsDialog::newHSTColor( void ) {
	KStars *ksw = (KStars *)parent();
	ksw->GetOptions()->colorHST = QColorDialog::getColor( QColor( ksw->GetOptions()->colorHST ) ).name();
	ChangeCHST->setPalette( QPalette( QColor( ksw->GetOptions()->colorHST ) ) );
	ksw->skymap->Update();
}	

void ViewOpsDialog::newMWColor( void ) {
	KStars *ksw = (KStars *)parent();
	ksw->GetOptions()->colorMW = QColorDialog::getColor( QColor( ksw->GetOptions()->colorMW ) ).name();
	ChangeCMilkyWay->setPalette( QPalette( QColor( ksw->GetOptions()->colorMW ) ) );
	ksw->skymap->Update();
}	

void ViewOpsDialog::newEqColor( void ) {
	KStars *ksw = (KStars *)parent();
	ksw->GetOptions()->colorEq = QColorDialog::getColor( QColor( ksw->GetOptions()->colorEq ) ).name();
	ChangeCEquator->setPalette( QPalette( QColor( ksw->GetOptions()->colorEq ) ) );
	ksw->skymap->Update();
}	

void ViewOpsDialog::newEclColor( void ) {
	KStars *ksw = (KStars *)parent();
	ksw->GetOptions()->colorEcl = QColorDialog::getColor( QColor( ksw->GetOptions()->colorEcl ) ).name();
	ChangeCEcliptic->setPalette( QPalette( QColor( ksw->GetOptions()->colorEcl ) ) );
	ksw->skymap->Update();
}	

void ViewOpsDialog::newHorzColor( void ) {
	KStars *ksw = (KStars *)parent();
	ksw->GetOptions()->colorHorz = QColorDialog::getColor( QColor( ksw->GetOptions()->colorHorz ) ).name();
	ChangeCHorizon->setPalette( QPalette( QColor( ksw->GetOptions()->colorHorz ) ) );
	ksw->skymap->Update();
}	

void ViewOpsDialog::newCLineColor( void ) {
	KStars *ksw = (KStars *)parent();
	ksw->GetOptions()->colorCLine = QColorDialog::getColor( QColor( ksw->GetOptions()->colorCLine ) ).name();
	ChangeCConstLine->setPalette( QPalette( QColor( ksw->GetOptions()->colorCLine ) ) );
	ksw->skymap->Update();
}	

void ViewOpsDialog::newCNameColor( void ) {
	KStars *ksw = (KStars *)parent();
	ksw->GetOptions()->colorCName = QColorDialog::getColor( QColor( ksw->GetOptions()->colorCName ) ).name();
	ChangeCConstText->setPalette( QPalette( QColor( ksw->GetOptions()->colorCName ) ) );
	ksw->skymap->Update();
}	

void ViewOpsDialog::newSNameColor( void ) {
	KStars *ksw = (KStars *)parent();
	ksw->GetOptions()->colorSName = QColorDialog::getColor( QColor( ksw->GetOptions()->colorSName ) ).name();
	ChangeCStarText->setPalette( QPalette( QColor( ksw->GetOptions()->colorSName ) ) );
	ksw->skymap->Update();
}	

void ViewOpsDialog::defaultColors( void ) {
	KStars *ksw = (KStars *)parent();
	ksw->GetOptions()->useNightColors = false;
	ksw->GetOptions()->colorSky 	= "#002";
//	ksw->GetOptions()->colorStar 	= "#FFF";
	ksw->GetOptions()->colorMW 		= "#345";
	ksw->GetOptions()->colorEq 		= "#FFF";
	ksw->GetOptions()->colorEcl		= "#663";
	ksw->GetOptions()->colorHorz 	= "#5A3";
	ksw->GetOptions()->colorMess 	= "#0F0";
	ksw->GetOptions()->colorNGC 	= "#066";
	ksw->GetOptions()->colorIC 		= "#439";
	ksw->GetOptions()->colorCLine = "#555";
	ksw->GetOptions()->colorCName = "#AA7";
	ksw->GetOptions()->colorSName = "#7AA";
	ksw->GetOptions()->colorHST 	= "#A00";
	
	ChangeCSky->setPalette( QPalette( QColor( ksw->GetOptions()->colorSky ) ) );
//	ChangeCStar->setPalette( QPalette( QColor( ksw->GetOptions()->colorStar ) ) );
	ChangeCMess->setPalette( QPalette( QColor( ksw->GetOptions()->colorMess ) ) );
	ChangeCNGC->setPalette( QPalette( QColor( ksw->GetOptions()->colorNGC ) ) );
	ChangeCIC->setPalette( QPalette( QColor( ksw->GetOptions()->colorIC ) ) );
	ChangeCHST->setPalette( QPalette( QColor( ksw->GetOptions()->colorHST ) ) );
	ChangeCMilkyWay->setPalette( QPalette( QColor( ksw->GetOptions()->colorMW ) ) );
	ChangeCEquator->setPalette( QPalette( QColor( ksw->GetOptions()->colorEq ) ) );
	ChangeCEcliptic->setPalette( QPalette( QColor( ksw->GetOptions()->colorEcl ) ) );
	ChangeCHorizon->setPalette( QPalette( QColor( ksw->GetOptions()->colorHorz ) ) );
	ChangeCConstLine->setPalette( QPalette( QColor( ksw->GetOptions()->colorCLine ) ) );
	ChangeCConstText->setPalette( QPalette( QColor( ksw->GetOptions()->colorCName ) ) );
	ChangeCStarText->setPalette( QPalette( QColor( ksw->GetOptions()->colorSName ) ) );

// set default colors if not set yet
	if (ksw->skymap->starpix->useNightColors() != ksw->GetOptions()->useNightColors)
		ksw->skymap->starpix->setDefaultColors();
	
	IntensityBox->setValue (4);
	ksw->skymap->Update();	// It's needed although IntensityBox->setValue(4) calls Update() too! But why?
}

void ViewOpsDialog::redColors( void ) {
	KStars *ksw = (KStars *)parent();
	
	ksw->GetOptions()->useNightColors = true;	 // reload starpixmaps with red colors
	ksw->GetOptions()->colorSky 	= "#000";
//	ksw->GetOptions()->colorStar 	= "#F00";
	ksw->GetOptions()->colorMW		= "#900";
	ksw->GetOptions()->colorEq		= "#A00";
	ksw->GetOptions()->colorEcl	= "#A00";
	ksw->GetOptions()->colorHorz 	= "#933";
	ksw->GetOptions()->colorMess 	= "#C09";
	ksw->GetOptions()->colorNGC	 	= "#C09";
	ksw->GetOptions()->colorIC 		= "#C09";
	ksw->GetOptions()->colorHST 	= "#A00";
	ksw->GetOptions()->colorCLine = "#800";
	ksw->GetOptions()->colorCName = "#900";
	ksw->GetOptions()->colorSName = "#900";
	
	ChangeCSky->setPalette( QPalette( QColor( ksw->GetOptions()->colorSky ) ) );
//	ChangeCStar->setPalette( QPalette( QColor( ksw->GetOptions()->colorStar ) ) );
	ChangeCMess->setPalette( QPalette( QColor( ksw->GetOptions()->colorMess ) ) );
	ChangeCNGC->setPalette( QPalette( QColor( ksw->GetOptions()->colorNGC ) ) );
	ChangeCIC->setPalette( QPalette( QColor( ksw->GetOptions()->colorIC ) ) );
	ChangeCHST->setPalette( QPalette( QColor( ksw->GetOptions()->colorHST ) ) );
	ChangeCMilkyWay->setPalette( QPalette( QColor( ksw->GetOptions()->colorMW ) ) );
	ChangeCEquator->setPalette( QPalette( QColor( ksw->GetOptions()->colorEq ) ) );
	ChangeCEcliptic->setPalette( QPalette( QColor( ksw->GetOptions()->colorEcl ) ) );
	ChangeCHorizon->setPalette( QPalette( QColor( ksw->GetOptions()->colorHorz ) ) );
	ChangeCConstLine->setPalette( QPalette( QColor( ksw->GetOptions()->colorCLine ) ) );
	ChangeCConstText->setPalette( QPalette( QColor( ksw->GetOptions()->colorCName ) ) );
	ChangeCStarText->setPalette( QPalette( QColor( ksw->GetOptions()->colorSName ) ) );
	
// set night colors if not set yet
	if (ksw->skymap->starpix->useNightColors() != ksw->GetOptions()->useNightColors)
		ksw->skymap->starpix->setNightColors();
	
	ksw->skymap->Update();
}

void ViewOpsDialog::updateDisplay( void ) {
	KStars *ksw = (KStars *)parent();
	ksw->GetOptions()->drawBSC = showBSC->isChecked();
	ksw->GetOptions()->drawMessier = showMessier->isChecked();
	ksw->GetOptions()->drawMessImages = showMessImages->isChecked();
	ksw->GetOptions()->drawNGC = showNGC->isChecked();
	ksw->GetOptions()->drawIC = showIC->isChecked();
	ksw->GetOptions()->drawSun = showSun->isChecked();
	ksw->GetOptions()->drawMoon = showMoon->isChecked();
	ksw->GetOptions()->drawMercury = showMercury->isChecked();
	ksw->GetOptions()->drawVenus = showVenus->isChecked();
	ksw->GetOptions()->drawMars = showMars->isChecked();
	ksw->GetOptions()->drawJupiter = showJupiter->isChecked();
	ksw->GetOptions()->drawSaturn = showSaturn->isChecked();
	ksw->GetOptions()->drawUranus = showUranus->isChecked();
	ksw->GetOptions()->drawNeptune = showNeptune->isChecked();
	ksw->GetOptions()->drawPluto = showPluto->isChecked();
	ksw->GetOptions()->drawConstellLines = showConstellLines->isChecked();
	ksw->GetOptions()->drawConstellNames = showConstellNames->isChecked();
	ksw->GetOptions()->useLatinConstellNames = useLatinConstellNames->isChecked();
	ksw->GetOptions()->drawMilkyWay = showMilkyWay->isChecked();
	ksw->GetOptions()->drawEquator = showEquator->isChecked();
	ksw->GetOptions()->drawEcliptic = showEcliptic->isChecked();
	ksw->GetOptions()->drawHorizon = showHorizon->isChecked();
	ksw->GetOptions()->drawGround = showGround->isChecked();
	ksw->GetOptions()->drawStarName 		 = showStarNames->isChecked();
	ksw->GetOptions()->drawStarMagnitude = showStarMagnitude->isChecked();

	ksw->skymap->Update();
}

void ViewOpsDialog::changeCoordSys( void ) {
	KStars *ksw = (KStars *)parent();
	ksw->GetOptions()->useAltAz = AltAzRadio->isChecked();
	showGround->setEnabled( AltAzRadio->isChecked() );

	if ( EquatRadio->isChecked() ) {
		showGround->setChecked( false );
	}
	ksw->GetOptions()->drawGround = showGround->isChecked();
	ksw->skymap->Update();
}

void ViewOpsDialog::changeStarColorIntensity( int newValue ) {
	KStars *ksw = (KStars *)parent();
	ksw->skymap->starpix->setIntensity (newValue);
	ksw->GetOptions()->starColorIntensity = ksw->skymap->starpix->Intensity();
	ksw->skymap->Update();
}
