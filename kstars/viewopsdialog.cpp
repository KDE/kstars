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
#include <klineeditdlg.h>
#include <kmessagebox.h>

#include <qtextstream.h>
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
#include <qlistbox.h>
#include <qpushbutton.h>
#include <kdebug.h>

#include "kstars.h"
#include "magnitudespinbox.h"
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

	CoordsGroup = new QButtonGroup( 1, Qt::Vertical, i18n( "Coordinate system" ), DisplayBox );

	EquatRadio = new QRadioButton( i18n( "Equatorial" ), CoordsGroup );
	EquatRadio->setChecked( !ksw->GetOptions()->useAltAz );

	AltAzRadio = new QRadioButton( i18n( "Horizontal" ), CoordsGroup );
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
	int intMagLimitDrawStars = int ( 10.0 * ksw->GetOptions()->magLimitDrawStar );

// faintLimit doesn't work with reloading star data
//	int faintLimit = int( 10.*ksw->GetData()->starList.at(ksw->GetData()->starList.count()-1)->mag );
//	magSpinBoxDrawStars = new MagnitudeSpinBox( 0, faintLimit, StarsTab );
	magSpinBoxDrawStars = new MagnitudeSpinBox( 0, 79, StarsTab );	// max mag = 7.9
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
	ConstellOptions = new QButtonGroup( 1, Qt::Horizontal, i18n( "Constellation Name Options" ), GuideTab );
	useLatinConstellNames = new QRadioButton( i18n( "Latin" ), ConstellOptions );
	useLatinConstellNames->setFont ( stdFont );
	useLatinConstellNames->setChecked( ksw->GetOptions()->useLatinConstellNames );
  useLocalConstellNames = new QRadioButton( i18n( "Localized" ), ConstellOptions );
	useLocalConstellNames->setFont ( stdFont );
	useLocalConstellNames->setChecked( ksw->GetOptions()->useLocalConstellNames );
  useAbbrevConstellNames = new QRadioButton( i18n( "Abbreviated" ), ConstellOptions );
	useAbbrevConstellNames->setFont ( stdFont );
	useAbbrevConstellNames->setChecked( ksw->GetOptions()->useAbbrevConstellNames );
	ConstellOptions->setEnabled( showConstellNames->isChecked() );

	showMilkyWay = new QCheckBox( i18n( "Milky Way" ), GuideTab );
	showMilkyWay->setFont( stdFont );
	showMilkyWay->setChecked( ksw->GetOptions()->drawMilkyWay );

	showMilkyWayFilled = new QCheckBox( i18n( "Fill Milky Way" ), GuideTab );
	showMilkyWayFilled->setFont( stdFont );
	showMilkyWayFilled->setChecked( ksw->GetOptions()->fillMilkyWay );
  showMilkyWayFilled->setEnabled( showMilkyWay->isChecked() );

	showGrid = new QCheckBox( i18n( "Coordinate Grid" ), GuideTab );
	showGrid->setFont( stdFont );
	showGrid->setChecked( ksw->GetOptions()->drawGrid );

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

	DisplayTabs->insertTab( GuideTab, i18n( "Guides" ) );

//Construct Planets tab:
	PlanetTab = new QWidget( DisplayTabs, "PlanetTab" );
	vlayPlanetTab = new QVBoxLayout( PlanetTab );
	vlayPlanetTab->setSpacing( 6 );
	vlayPlanetTab->setMargin( 11 );
	glayPlanetTab = new QGridLayout( 7, 2 );

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

	showAll = new QPushButton( i18n( "Show All" ), PlanetTab );
	showAll->setFont( stdFont );

	showNone = new QPushButton( i18n( "Show None" ), PlanetTab );
	showNone->setFont( stdFont );

	QSpacerItem *smallSpacerLeft  = new QSpacerItem( 20, 20, QSizePolicy::Minimum, QSizePolicy::Minimum );
	QSpacerItem *smallSpacerRight = new QSpacerItem( 20, 20, QSizePolicy::Minimum, QSizePolicy::Minimum );

	QSpacerItem *spacerPlanetTab  = new QSpacerItem( 20, 20, QSizePolicy::Minimum, QSizePolicy::Expanding );

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
	glayPlanetTab->addItem( smallSpacerLeft, 5, 0 );
	glayPlanetTab->addItem( smallSpacerRight, 5, 1 );
	glayPlanetTab->addWidget( showAll, 6, 0 );
	glayPlanetTab->addWidget( showNone, 6, 1 );

	vlayPlanetTab->addLayout( glayPlanetTab );
  vlayPlanetTab->addItem( spacerPlanetTab );

	DisplayTabs->insertTab( PlanetTab, i18n( "Solar System" ) );

//Construct the Color Tab
	ColorTab = new QWidget( DisplayTabs, "ColorTab" );
	LeftBox = new QGroupBox( ColorTab, "LeftBox" );
	LeftBox->setTitle( i18n( "Current Colors" ) );

	RightBox = new QGroupBox( ColorTab, "RightBox" );
	RightBox->setTitle( i18n( "Preset Color Schemes" ) );

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

	SkyColor = new QPixmap( 30, 20 );
	SkyColor->fill( QColor( ksw->GetOptions()->colorSky ) );
	ColorPalette->insertItem( *SkyColor, i18n( "Sky" ) );

	MessColor = new QPixmap( 30, 20 );
	MessColor->fill( QColor( ksw->GetOptions()->colorMess ) );
	ColorPalette->insertItem( *MessColor, i18n( "Messier Object" ) );

	NGCColor = new QPixmap( 30, 20 );
	NGCColor->fill( QColor( ksw->GetOptions()->colorNGC ) );
	ColorPalette->insertItem( *NGCColor, i18n( "NGC Object" ) );

	ICColor = new QPixmap( 30, 20 );
	ICColor->fill( QColor( ksw->GetOptions()->colorIC ) );
	ColorPalette->insertItem( *ICColor, i18n( "IC Object" ) );

	LinksColor = new QPixmap( 30, 20 );
	LinksColor->fill( QColor( ksw->GetOptions()->colorHST ) );
	ColorPalette->insertItem( *LinksColor, i18n( "Object w/ Links" ) );

	SNameColor = new QPixmap( 30, 20 );
	SNameColor->fill( QColor( ksw->GetOptions()->colorSName ) );
	ColorPalette->insertItem( *SNameColor, i18n( "Star Name" ) );

	CNameColor = new QPixmap( 30, 20 );
	CNameColor->fill( QColor( ksw->GetOptions()->colorCName ) );
	ColorPalette->insertItem( *CNameColor, i18n( "Constell. Name" ) );

	CLineColor = new QPixmap( 30, 20 );
	CLineColor->fill( QColor( ksw->GetOptions()->colorCLine ) );
	ColorPalette->insertItem( *CLineColor, i18n( "Constell. Line" ) );

	MWColor = new QPixmap( 30, 20 );
	MWColor->fill( QColor( ksw->GetOptions()->colorMW ) );
	ColorPalette->insertItem( *MWColor, i18n( "Milky Way" ) );

	EqColor = new QPixmap( 30, 20 );
	EqColor->fill( QColor( ksw->GetOptions()->colorEq ) );
	ColorPalette->insertItem( *EqColor, i18n( "Equator" ) );

	EclColor = new QPixmap( 30, 20 );
	EclColor->fill( QColor( ksw->GetOptions()->colorEcl ) );
	ColorPalette->insertItem( *EclColor, i18n( "Ecliptic" ) );

	HorzColor = new QPixmap( 30, 20 );
	HorzColor->fill( QColor( ksw->GetOptions()->colorHorz ) );
	ColorPalette->insertItem( *HorzColor, i18n( "Horizon" ) );

	GridColor = new QPixmap( 30, 20 );
	GridColor->fill( QColor( ksw->GetOptions()->colorGrid ) );
	ColorPalette->insertItem( *GridColor, i18n( "Coordinate Grid" ) );

// the spinbox for colorintensity of stars
	IntensityBox = new KIntSpinBox (0, 10, 1, ksw->GetOptions()->starColorIntensity, 10, LeftBox);
	QLabel *IntensityLabel = new QLabel (IntensityBox, i18n ("Star Color &Intensity"), LeftBox);
	IntensityLabel->setAlignment ( AlignRight | AlignVCenter );

// the QComboBox for the starcolor mode
	StarColorMode = new QComboBox( LeftBox );
	QLabel *ColorModeLabel = new QLabel( StarColorMode, i18n( "Star Color &Mode" ), LeftBox );
	ColorModeLabel->setAlignment ( AlignRight | AlignVCenter );
	StarColorMode->insertItem( i18n( "Color by temperature" ) );
	StarColorMode->insertItem( i18n( "Solid Red" ) );
	StarColorMode->insertItem( i18n( "Solid Black" ) );
	StarColorMode->insertItem( i18n( "Solid White" ) );

//The list of preset color schemes
	PresetBox = new QListBox( RightBox );
  PresetBox->setSelectionMode( QListBox::Single );
	PresetBox->insertItem( i18n( "Default Colors" ) );
	PresetBox->insertItem( i18n( "Star Chart" ) );
	PresetBox->insertItem( i18n( "Night Vision" ) );

	PresetFileList.append( "default.colors" );
	PresetFileList.append( "chart.colors" );
	PresetFileList.append( "night.colors" );

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

	hlayColorTab->addWidget ( LeftBox );
	hlayColorTab->addWidget ( RightBox );

	DisplayTabs->insertTab( ColorTab, i18n( "Colors" ) );

	hlay->addWidget( DisplayBox );
	hlay->activate();

  connect( this, SIGNAL( okClicked() ), this, SLOT( accept() ) ) ;
  connect( this, SIGNAL( cancelClicked() ), this, SLOT( reject() ) );
	connect( ColorPalette, SIGNAL( clicked( QListBoxItem* ) ), this, SLOT( newColor( QListBoxItem* ) ) );
	connect( PresetBox, SIGNAL( highlighted( int ) ), this, SLOT( slotPreset( int ) ) );
	connect( AddPreset, SIGNAL( clicked() ), this, SLOT( slotAddPreset() ) );
	connect( showBSC, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( showMessier, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( showMessImages, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( showNGC, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( showIC, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( showConstellLines, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( showConstellNames, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( useLatinConstellNames, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( useLocalConstellNames, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( useAbbrevConstellNames, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( showMilkyWay, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( showMilkyWayFilled, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( showGrid, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
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
	connect( showAll, SIGNAL( clicked() ), this, SLOT( markPlanets() ) );
	connect( showNone, SIGNAL( clicked() ), this, SLOT( unMarkPlanets() ) );
	connect( EquatRadio, SIGNAL( clicked() ), this, SLOT( changeCoordSys() ) );
	connect( AltAzRadio, SIGNAL( clicked() ), this, SLOT( changeCoordSys() ) );
	connect( magSpinBoxDrawStars, SIGNAL( valueChanged( int ) ), SLOT( changeMagDrawStars( int) ) );
	connect( magSpinBoxDrawStarInfo, SIGNAL( valueChanged( int ) ), SLOT( changeMagDrawInfo( int) ) );
	connect( showStarNames, 		SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( showStarMagnitude, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( IntensityBox, SIGNAL (valueChanged (int)), SLOT (changeStarColorIntensity(int)) );
	connect( StarColorMode, SIGNAL( activated( int ) ), this, SLOT( changeStarColorMode( int ) ) );
}

ViewOpsDialog::~ViewOpsDialog(){
	delete SkyColor;  delete MessColor;  delete NGCColor;  delete ICColor;
	delete LinksColor;  delete SNameColor;  delete CLineColor;  delete CNameColor;
	delete EqColor;  delete EclColor;  delete HorzColor;  delete GridColor;
	delete MWColor;
}

void ViewOpsDialog::changeMagDrawStars( int newValue )
{
	float fNewValue = ( newValue * 1.0) / 10.0;

	ksw->GetData()->setMagnitude( fNewValue );

	// force redraw
	ksw->skymap->Update();
}

void ViewOpsDialog::changeMagDrawInfo( int newValue )
{
    kdDebug()<<"magnitude limit draw star info "<<newValue<<endl;
	float fNewValue = ( newValue * 1.0) / 10.0;
	ksw->GetOptions()->magLimitDrawStarInfo = fNewValue;
	// force redraw
	ksw->skymap->Update();
}

void ViewOpsDialog::newColor( QListBoxItem *item ) {
	QPixmap *temp = new QPixmap( 30, 20 );
  QColor newColor;
	if ( item->text() == i18n( "Sky" ) ) {
		newColor = QColorDialog::getColor( QColor( ksw->GetOptions()->colorSky ) );
		if ( newColor.isValid() ) {
			ksw->GetOptions()->colorSky = newColor.name();
			temp->fill( QColor( ksw->GetOptions()->colorSky ) );
		}
  } else if ( item->text() == i18n( "Messier Object" ) ) {
		newColor = QColorDialog::getColor( QColor( ksw->GetOptions()->colorMess ) );
		if ( newColor.isValid() ) {
			ksw->GetOptions()->colorMess = newColor.name();
			temp->fill( QColor( ksw->GetOptions()->colorMess ) );
		}
  } else if ( item->text() == i18n( "NGC Object" ) ) {
		newColor = QColorDialog::getColor( QColor( ksw->GetOptions()->colorNGC ) );
		if ( newColor.isValid() ) {
			ksw->GetOptions()->colorNGC = newColor.name();
			temp->fill( QColor( ksw->GetOptions()->colorNGC ) );
		}
  } else if ( item->text() == i18n( "IC Object" ) ) {
		newColor = QColorDialog::getColor( QColor( ksw->GetOptions()->colorIC ) );
		if ( newColor.isValid() ) {
			ksw->GetOptions()->colorIC = newColor.name();
			temp->fill( QColor( ksw->GetOptions()->colorIC ) );
		}
  } else if ( item->text() == i18n( "Object w/ Links" ) ) {
		newColor = QColorDialog::getColor( QColor( ksw->GetOptions()->colorHST ) );
		if ( newColor.isValid() ) {
			ksw->GetOptions()->colorHST = newColor.name();
			temp->fill( QColor( ksw->GetOptions()->colorHST ) );
		}
  } else if ( item->text() == i18n( "Milky Way" ) ) {
		newColor = QColorDialog::getColor( QColor( ksw->GetOptions()->colorMW ) );
		if ( newColor.isValid() ) {
			ksw->GetOptions()->colorMW = newColor.name();
			temp->fill( QColor( ksw->GetOptions()->colorMW ) );
		}
  } else if ( item->text() == i18n( "Equator" ) ) {
		newColor = QColorDialog::getColor( QColor( ksw->GetOptions()->colorEq ) );
		if ( newColor.isValid() ) {
			ksw->GetOptions()->colorEq = newColor.name();
			temp->fill( QColor( ksw->GetOptions()->colorEq ) );
		}
  } else if ( item->text() == i18n( "Ecliptic" ) ) {
		newColor = QColorDialog::getColor( QColor( ksw->GetOptions()->colorEcl ) );
		if ( newColor.isValid() ) {
			ksw->GetOptions()->colorEcl = newColor.name();
			temp->fill( QColor( ksw->GetOptions()->colorEcl ) );
		}
  } else if ( item->text() == i18n( "Horizon" ) ) {
		newColor = QColorDialog::getColor( QColor( ksw->GetOptions()->colorHorz ) );
		if ( newColor.isValid() ) {
			ksw->GetOptions()->colorHorz = newColor.name();
			temp->fill( QColor( ksw->GetOptions()->colorHorz ) );
		}
  } else if ( item->text() == i18n( "Coordinate Grid" ) ) {
		newColor = QColorDialog::getColor( QColor( ksw->GetOptions()->colorGrid ) );
		if ( newColor.isValid() ) {
			ksw->GetOptions()->colorGrid = newColor.name();
			temp->fill( QColor( ksw->GetOptions()->colorGrid ) );
		}
  } else if ( item->text() == i18n( "Star Name" ) ) {
		newColor = QColorDialog::getColor( QColor( ksw->GetOptions()->colorSName ) );
		if ( newColor.isValid() ) {
			ksw->GetOptions()->colorSName = newColor.name();
			temp->fill( QColor( ksw->GetOptions()->colorSName ) );
		}
  } else if ( item->text() == i18n( "Constell. Line" ) ) {
		newColor = QColorDialog::getColor( QColor( ksw->GetOptions()->colorCLine ) );
		if ( newColor.isValid() ) {
			ksw->GetOptions()->colorCLine = newColor.name();
			temp->fill( QColor( ksw->GetOptions()->colorCLine ) );
		}
  } else if ( item->text() == i18n( "Constell. Name" ) ) {
		newColor = QColorDialog::getColor( QColor( ksw->GetOptions()->colorCName ) );
		if ( newColor.isValid() ) {
			ksw->GetOptions()->colorCName = newColor.name();
			temp->fill( QColor( ksw->GetOptions()->colorCName ) );
		}
	}

	if ( newColor.isValid() )
		ColorPalette->changeItem( *temp, item->text(), ColorPalette->index( item ) );

	delete temp;
	ksw->skymap->Update();
}

void ViewOpsDialog::defaultColors( void ) {
	bool result = setColors( "default.colors" );
	if (!result) {
		QString message = i18n( "The specified color-scheme file could not be found." );
		KMessageBox::sorry( 0, message, i18n( "Could not set color scheme" ) );
	}
}

void ViewOpsDialog::redColors( void ) {
	bool result = setColors( "night.colors" );
	if (!result) {
		QString message = i18n( "The specified color-scheme file could not be found." );
		KMessageBox::sorry( 0, message, i18n( "Could not set color scheme" ) );
	}
}

void ViewOpsDialog::chartColors( void ) {
	bool result = setColors( "chart.colors" );
	if (!result) {
		QString message = i18n( "The specified color-scheme file could not be found." );
		KMessageBox::sorry( 0, message, i18n( "Could not set color scheme" ) );
	}
}

void ViewOpsDialog::slotPreset( int index ) {
	QStringList::Iterator it = PresetFileList.at( index );
	bool result = setColors( *it );
	if (!result) {
		QString message = i18n( "The specified color-scheme file could not be found." );
		KMessageBox::sorry( 0, message, i18n( "Could not set color scheme" ) );
	}
}

void ViewOpsDialog::slotAddPreset( void ) {
	QFile file;
	QString filename;

	KLineEditDlg getName( i18n( "Enter a name for the new Scheme:" ), "", this );

	if ( getName.exec() == QDialog::Accepted ) {
		filename = getName.text();
		for( int i=0; i<8; ++i)
			if ( filename.at(i)==' ' ) filename.replace( i, 1, "-" );

		filename = filename.append( ".colors" );
		file.setName( locateLocal( "appdata", filename ) ); //determine filename in local user KDE directory tree.

		if ( file.exists() || !file.open( IO_ReadWrite | IO_Append ) ) {
			QString message = i18n( "Local color scheme file could not be opened.\nScheme cannot be recorded." );
			KMessageBox::sorry( 0, message, i18n( "Could not open file" ) );
		} else {
			QTextStream stream( &file );
			stream << ksw->GetOptions()->starColorMode << endl;
			stream << ksw->GetOptions()->colorSky << endl;
			stream << ksw->GetOptions()->colorMess << endl;
			stream << ksw->GetOptions()->colorNGC << endl;
			stream << ksw->GetOptions()->colorIC << endl;
			stream << ksw->GetOptions()->colorHST << endl;
			stream << ksw->GetOptions()->colorSName << endl;
			stream << ksw->GetOptions()->colorCName << endl;
			stream << ksw->GetOptions()->colorCLine << endl;
			stream << ksw->GetOptions()->colorMW << endl;
			stream << ksw->GetOptions()->colorEq << endl;
			stream << ksw->GetOptions()->colorEcl << endl;
			stream << ksw->GetOptions()->colorHorz << endl;
			stream << ksw->GetOptions()->colorGrid << endl;
			file.close();
		}

		file.setName( locateLocal( "appdata", "colors.dat" ) ); //determine filename in local user KDE directory tree.

		if ( !file.open( IO_ReadWrite | IO_Append ) ) {
			QString message = i18n( "Local color scheme index file could not be opened.\nScheme cannot be recorded." );
			KMessageBox::sorry( 0, message, i18n( "Could not open file" ) );
		} else {
			QTextStream stream( &file );
			stream << getName.text() << ":" << filename << endl;
			file.close();
		}

		PresetBox->insertItem( getName.text() );
		PresetFileList.append( filename );
	}
}

bool ViewOpsDialog::setColors( QString filename ) {
	QPixmap *temp = new QPixmap( 30, 20 );
	QFile file;
	int i=0;

	if ( !KStarsData::openDataFile( file, filename ) ) {
		file.setName( locateLocal( "appdata", filename ) ); //try filename in local user KDE directory tree.
		if ( !file.open( IO_ReadOnly ) ) {
			return false;
    }
	}

	QTextStream stream( &file );
	QString line;

	//first line is the star-color mode
  line = stream.readLine();
	int newmode = line.left(1).toInt();
	ksw->GetOptions()->starColorMode = newmode;
	if ( ksw->skymap->starpix->mode() != newmode )
		ksw->skymap->starpix->setColorMode( newmode );
	StarColorMode->setCurrentItem( newmode );

	//sky
  line = stream.readLine();
	ksw->GetOptions()->colorSky = line.left( 7 );
	temp->fill( QColor( ksw->GetOptions()->colorSky ) );
	ColorPalette->changeItem( *temp, ColorPalette->item(i)->text(), i );
	i++;

  //Messier
  line = stream.readLine();
	ksw->GetOptions()->colorMess = line.left( 7 );
	temp->fill( QColor( ksw->GetOptions()->colorMess ) );
	ColorPalette->changeItem( *temp, ColorPalette->item(i)->text(), i );
	i++;

  //NGC
  line = stream.readLine();
	ksw->GetOptions()->colorNGC = line.left( 7 );
	temp->fill( QColor( ksw->GetOptions()->colorNGC ) );
	ColorPalette->changeItem( *temp, ColorPalette->item(i)->text(), i );
	i++;

  //IC
  line = stream.readLine();
	ksw->GetOptions()->colorIC = line.left( 7 );
	temp->fill( QColor( ksw->GetOptions()->colorIC ) );
	ColorPalette->changeItem( *temp, ColorPalette->item(i)->text(), i );
	i++;

  //Links
  line = stream.readLine();
	ksw->GetOptions()->colorHST = line.left( 7 );
	temp->fill( QColor( ksw->GetOptions()->colorHST ) );
	ColorPalette->changeItem( *temp, ColorPalette->item(i)->text(), i );
	i++;

  //Star Names
  line = stream.readLine();
	ksw->GetOptions()->colorSName = line.left( 7 );
	temp->fill( QColor( ksw->GetOptions()->colorSName ) );
	ColorPalette->changeItem( *temp, ColorPalette->item(i)->text(), i );
	i++;

  //Constell Names
  line = stream.readLine();
	ksw->GetOptions()->colorCName = line.left( 7 );
	temp->fill( QColor( ksw->GetOptions()->colorCName ) );
	ColorPalette->changeItem( *temp, ColorPalette->item(i)->text(), i );
	i++;

  //Constell Lines
  line = stream.readLine();
	ksw->GetOptions()->colorCLine = line.left( 7 );
	temp->fill( QColor( ksw->GetOptions()->colorCLine ) );
	ColorPalette->changeItem( *temp, ColorPalette->item(i)->text(), i );
	i++;

  //Milky Way
  line = stream.readLine();
	ksw->GetOptions()->colorMW = line.left( 7 );
	temp->fill( QColor( ksw->GetOptions()->colorMW ) );
	ColorPalette->changeItem( *temp, ColorPalette->item(i)->text(), i );
	i++;

  //Equator
  line = stream.readLine();
	ksw->GetOptions()->colorEq = line.left( 7 );
	temp->fill( QColor( ksw->GetOptions()->colorEq ) );
	ColorPalette->changeItem( *temp, ColorPalette->item(i)->text(), i );
	i++;

  //Ecliptic
  line = stream.readLine();
	ksw->GetOptions()->colorEcl = line.left( 7 );
	temp->fill( QColor( ksw->GetOptions()->colorEcl ) );
	ColorPalette->changeItem( *temp, ColorPalette->item(i)->text(), i );
	i++;

  //Horizon
  line = stream.readLine();
	ksw->GetOptions()->colorHorz = line.left( 7 );
	temp->fill( QColor( ksw->GetOptions()->colorHorz ) );
	ColorPalette->changeItem( *temp, ColorPalette->item(i)->text(), i );
	i++;

  //Coordinate Grid
  line = stream.readLine();
	ksw->GetOptions()->colorGrid = line.left( 7 );
	temp->fill( QColor( ksw->GetOptions()->colorGrid ) );
	ColorPalette->changeItem( *temp, ColorPalette->item(i)->text(), i );
  i++;

	ksw->skymap->Update();
	return true;
}

void ViewOpsDialog::updateDisplay( void ) {
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
	ksw->GetOptions()->useLocalConstellNames = useLocalConstellNames->isChecked();
	ksw->GetOptions()->useAbbrevConstellNames = useAbbrevConstellNames->isChecked();
	ConstellOptions->setEnabled( showConstellNames->isChecked() );
	ksw->GetOptions()->drawMilkyWay = showMilkyWay->isChecked();
	ksw->GetOptions()->fillMilkyWay = showMilkyWayFilled->isChecked();
	showMilkyWayFilled->setEnabled( showMilkyWay->isChecked() );
	ksw->GetOptions()->drawGrid = showGrid->isChecked();
	ksw->GetOptions()->drawEquator = showEquator->isChecked();
	ksw->GetOptions()->drawEcliptic = showEcliptic->isChecked();
	ksw->GetOptions()->drawHorizon = showHorizon->isChecked();
	ksw->GetOptions()->drawGround = showGround->isChecked();
	ksw->GetOptions()->drawStarName 		 = showStarNames->isChecked();
	ksw->GetOptions()->drawStarMagnitude = showStarMagnitude->isChecked();

	ksw->skymap->Update();
}

void ViewOpsDialog::changeCoordSys( void ) {
	ksw->GetOptions()->useAltAz = AltAzRadio->isChecked();
	showGround->setEnabled( AltAzRadio->isChecked() );

	if ( EquatRadio->isChecked() ) {
		showGround->setChecked( false );
	}
	ksw->GetOptions()->drawGround = showGround->isChecked();
	ksw->skymap->Update();
}

void ViewOpsDialog::changeStarColorIntensity( int newValue ) {
	ksw->skymap->starpix->setIntensity (newValue);
	ksw->GetOptions()->starColorIntensity = ksw->skymap->starpix->intensity();
	ksw->skymap->Update();
}

void ViewOpsDialog::changeStarColorMode( int newValue ) {
	ksw->skymap->starpix->setColorMode( newValue );
	ksw->GetOptions()->starColorMode = ksw->skymap->starpix->mode();
	if (newValue) IntensityBox->setEnabled( false );
	else IntensityBox->setEnabled( true );
	ksw->skymap->Update();
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
