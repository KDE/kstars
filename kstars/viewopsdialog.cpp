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
#include <kcombobox.h>

#include "kstars.h"
#include "magnitudespinbox.h"
#include "timestepbox.h"
#include "ksutils.h"
#include "addcatdialog.h"

#include "viewopsdialog.h"

#include "viewopsdialog.moc"

ViewOpsDialog::ViewOpsDialog( QWidget *parent )
	: KDialogBase( KDialogBase::IconList, i18n( "Display Options" ), Ok|Cancel, Ok, parent ) {
	ksw = (KStars *)parent;

	QFile imFile;
	QPixmap im = QPixmap();

	//Catalog tab
	if ( KSUtils::openDataFile( imFile, "opscatalog.png" ) ) {
		imFile.close(); //Just need the filename...
		im.load( imFile.name() );
	}
	QFrame *catpage = addPage( i18n( "Catalogs" ), QString::null, im );
	QVBoxLayout *catlay = new QVBoxLayout( catpage, 0, 6 );
	cat = new OpsCatalog( catpage );

	cat->showSAO->setChecked( Options::showSAO );
	cat->magSpinBoxDrawStars->setMinValue( 0.0 );
	cat->magSpinBoxDrawStars->setMaxValue( 12.0 );
	cat->magSpinBoxDrawStars->setValue( Options::magLimitDrawStar );
	cat->magSpinBoxDrawStarZoomOut->setMinValue( 0.0 );
	cat->magSpinBoxDrawStarZoomOut->setMaxValue( cat->magSpinBoxDrawStars->value() );
	cat->magSpinBoxDrawStarZoomOut->setValue( Options::magLimitDrawStarZoomOut );
	cat->magSpinBoxDrawStarInfo->setMinValue( 0.0 );
	cat->magSpinBoxDrawStarInfo->setMaxValue( 9.0 );
	cat->magSpinBoxDrawStarInfo->setValue( Options::magLimitDrawStarInfo );
	cat->showStarNames->setChecked( Options::showStarName );
	cat->showStarMagnitude->setChecked( Options::showStarMagnitude );

	//star options enabled only if showSAO is checked...
	cat->showStarNames->setEnabled( cat->showSAO->isChecked() );
	cat->showStarMagnitude->setEnabled( cat->showSAO->isChecked() );
	cat->magSpinBoxDrawStars->setEnabled( cat->showSAO->isChecked() );
	cat->magSpinBoxDrawStarZoomOut->setEnabled( cat->showSAO->isChecked() );
	cat->magSpinBoxDrawStarInfo->setEnabled( cat->showSAO->isChecked() );
	cat->textLabelMagStars->setEnabled( cat->showSAO->isChecked() );
	cat->textLabelMagStarsZoomOut->setEnabled( cat->showSAO->isChecked() );
	cat->textLabelMagStarInfo->setEnabled( cat->showSAO->isChecked() );
	cat->textLabelMag1->setEnabled( cat->showSAO->isChecked() );
	cat->textLabelMag2->setEnabled( cat->showSAO->isChecked() );
	cat->textLabelMag3->setEnabled( cat->showSAO->isChecked() );

	//Populate CatalogList
	showIC = new QCheckListItem( cat->CatalogList, i18n( "Index Catalog (IC)" ), QCheckListItem::CheckBox );
	showIC->setOn( Options::showIC );

	showNGC = new QCheckListItem( cat->CatalogList, i18n( "New General Catalog (NGC)" ), QCheckListItem::CheckBox );
	showNGC->setOn( Options::showNGC );

	showMessImages = new QCheckListItem( cat->CatalogList, i18n( "Messier Catalog (images)" ), QCheckListItem::CheckBox );
	showMessImages->setOn( Options::showMessImages );

	showMessier = new QCheckListItem( cat->CatalogList, i18n( "Messier Catalog (symbols)" ), QCheckListItem::CheckBox );
	showMessier->setOn( Options::showMessier );

	//Add custom catalogs, if necessary
	for ( unsigned int i=0; i<Options::catalogCount(); ++i ) { //loop over custom catalogs
		QCheckListItem *newItem = new QCheckListItem( cat->CatalogList, Options::CatalogName[i], QCheckListItem::CheckBox );
		newItem->setOn( Options::showCatalog[i] );
	}

	//Magnitude limits for deep-sky objects
	cat->magSpinBoxDrawDeepSky->setMinValue( 0.0 );
	cat->magSpinBoxDrawDeepSky->setMaxValue( 16.0 );
	cat->magSpinBoxDrawDeepSky->setValue( Options::magLimitDrawDeepSky );
	cat->magSpinBoxDrawDeepSkyZoomOut->setMinValue( 0.0 );
	cat->magSpinBoxDrawDeepSkyZoomOut->setMaxValue( cat->magSpinBoxDrawDeepSky->value() );
	cat->magSpinBoxDrawDeepSkyZoomOut->setValue( Options::magLimitDrawDeepSkyZoomOut );

	catlay->addWidget( cat );

	//Solar System tab
	if ( KSUtils::openDataFile( imFile, "opssolarsystem.png" ) ) {
		imFile.close(); //Just need the filename...
		im.load( imFile.name() );
	}
	QFrame *sspage = addPage( i18n( "Solar System" ), QString::null, im );
	QVBoxLayout *sslay = new QVBoxLayout( sspage, 0, 6 );
	ss = new OpsSolarSystem( sspage );

	ss->showSun->setChecked( Options::showSun );
	ss->showMoon->setChecked( Options::showMoon );
	ss->showMercury->setChecked( Options::showMercury );
	ss->showVenus->setChecked( Options::showVenus );
	ss->showMars->setChecked( Options::showMars );
	ss->showJupiter->setChecked( Options::showJupiter );
	ss->showSaturn->setChecked( Options::showSaturn );
	ss->showUranus->setChecked( Options::showUranus );
	ss->showNeptune->setChecked( Options::showNeptune );
	ss->showPluto->setChecked( Options::showPluto );
	ss->showPlanetNames->setChecked( Options::showPlanetName );
	ss->showPlanetImages->setChecked( Options::showPlanetImage );
	ss->showAsteroids->setChecked( Options::showAsteroids );
	ss->astDrawSpinBox->setMinValue( 2.5 );
	ss->astDrawSpinBox->setMaxValue( 12.0 );
	ss->astDrawSpinBox->setValue( Options::magLimitAsteroid );
	ss->astDrawSpinBox->setEnabled( Options::showAsteroids );
	ss->showAsteroidNames->setChecked( Options::showAsteroidName );
	ss->showAsteroidNames->setEnabled( Options::showAsteroids );
	ss->astNameSpinBox->setMinValue( 2.5 );
	ss->astNameSpinBox->setMaxValue( 12.0 );
	ss->astNameSpinBox->setValue( Options::magLimitAsteroidName );
	ss->astNameSpinBox->setEnabled( Options::showAsteroids && Options::showAsteroidName );
	ss->showComets->setChecked( Options::showComets );
	ss->showCometNames->setChecked( Options::showCometName );
	ss->showCometNames->setEnabled( Options::showComets );
	ss->comNameSpinBox->setValue( Options::maxRadCometName );
	ss->comNameSpinBox->setEnabled( Options::showComets && Options::showCometName );
	ss->autoTrail->setChecked( Options::useAutoTrail );
	ss->fadePlanetTrails->setChecked( Options::fadePlanetTrails );

	sslay->addWidget( ss );

	//Guides tab
	if ( KSUtils::openDataFile( imFile, "opsguides.png" ) ) {
		imFile.close(); //Just need the filename...
		im.load( imFile.name() );
	}
	QFrame *guidepage = addPage( i18n( "Guides" ), QString::null, im );
	QVBoxLayout *guidelay = new QVBoxLayout( guidepage, 0, 6 );
	guide = new OpsGuides( guidepage );

	guide->showConstellLines->setChecked( Options::showConstellLines );
	guide->showConstellBounds->setChecked( Options::showConstellBounds );
	guide->showConstellNames->setChecked( Options::showConstellNames );
	guide->ConstellOptions::setEnabled( Options::showConstellNames );
	guide->useLatinConstellNames->setChecked( Options::useLatinConstellNames );
	guide->useLocalConstellNames->setChecked( Options::useLocalConstellNames );
	guide->useAbbrevConstellNames->setChecked( Options::useAbbrevConstellNames );
	guide->showMilkyWay->setChecked( Options::showMilkyWay );
	guide->showMilkyWayFilled->setChecked( Options::fillMilkyWay );
	guide->showMilkyWayFilled->setEnabled( Options::showMilkyWay );
	guide->showGrid->setChecked( Options::showGrid );
	guide->showEquator->setChecked( Options::showEquator );
	guide->showEcliptic->setChecked( Options::showEcliptic );
	guide->showHorizon->setChecked( Options::showHorizon );
	guide->showGround->setChecked( Options::showGround );

	guidelay->addWidget( guide );

	//Colors tab
	if ( KSUtils::openDataFile( imFile, "opscolors.png" ) ) {
		imFile.close(); //Just need the filename...
		im.load( imFile.name() );
	}
	QFrame *colorpage = addPage( i18n( "Colors" ), QString::null, im );
	QVBoxLayout *colorlay = new QVBoxLayout( colorpage, 0, 6 );

	color = new OpsColors( colorpage );

	//Initialize Colors tab
	color->ColorPalette->setSelectionMode( QListBox::Single );
	color->PresetBox->setSelectionMode( QListBox::Single );

	for ( unsigned int i=0; i < Options::colorScheme()->numberOfColors(); ++i ) {
		QPixmap col( 30, 20 );
		col.fill( QColor( Options::colorScheme()->colorAt( i ) ) );
		color->ColorPalette->insertItem( col, Options::colorScheme()->nameAt( i ) );
	}

	color->PresetBox->insertItem( i18n( "use default color scheme", "Default Colors" ) );
	color->PresetBox->insertItem( i18n( "use 'star chart' color scheme", "Star Chart" ) );
	color->PresetBox->insertItem( i18n( "use 'night vision' color scheme", "Night Vision" ) );
	color->PresetBox->insertItem( i18n( "use 'moonless night' color scheme", "Moonless Night" ) );

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
			color->PresetBox->insertItem( schemeName );
			PresetFileList.append( filename );
	  }
		file.close();
	}

	color->IntensityBox->setValue( Options::colorScheme()->starColorIntensity() );
	color->StarColorMode->insertItem( i18n( "use realistic star colors", "Real Colors" ) );
	color->StarColorMode->insertItem( i18n( "show stars as red circles", "Solid Red" ) );
	color->StarColorMode->insertItem( i18n( "show stars as black circles", "Solid Black" ) );
	color->StarColorMode->insertItem( i18n( "show stars as white circles", "Solid White" ) );
	color->StarColorMode->setCurrentItem( Options::colorScheme()->starColorMode() );

	colorlay->addWidget( color );

	//Advanced tab
	if ( KSUtils::openDataFile( imFile, "opsadvanced.png" ) ) {
		imFile.close(); //Just need the filename...
		im.load( imFile.name() );
	}
	QFrame *advpage = addPage( i18n( "Advanced" ), QString::null, im );
	QVBoxLayout *advlay = new QVBoxLayout( advpage, 0, 6 );
	adv = new OpsAdvanced( advpage );

	adv->useRefraction->setChecked( Options::useRefraction );
	adv->animateSlewing->setChecked( Options::useAnimatedSlewing );
	adv->autoLabel->setChecked( Options::useAutoLabel );
	adv->hoverLabel->setChecked( Options::useHoverLabel );

	adv->hideObjects->setChecked( Options::hideOnSlew );
	adv->hideSpinBox->tsbox()->changeScale( (float)Options::slewTimeScale );
	adv->hideStars->setChecked( Options::hideStars );
	adv->hidePlanets->setChecked( Options::hidePlanets );
	adv->hideMess->setChecked( Options::hideMess );
	adv->hideNGC->setChecked( Options::hideNGC );
	adv->hideIC->setChecked( Options::hideIC );
	adv->hideMW->setChecked( Options::hideMW );
	adv->hideCNames->setChecked( Options::hideCNames );
	adv->hideCLines->setChecked( Options::hideCLines );
	adv->hideCBounds->setChecked( Options::hideCBounds );
	adv->hideGrid->setChecked( Options::hideGrid );
	adv->magSpinBoxHideStars->setValue( Options::magLimitHideStar );

	//HideBox widgets enabled only if hideObjects is checked...
	adv->hideSpinBox->setEnabled( adv->hideObjects->isChecked() );
	adv->textLabelHideTimeStep->setEnabled( adv->hideObjects->isChecked() );
	adv->HideBox->setEnabled( adv->hideObjects->isChecked() );

	advlay->addWidget( adv );

	//Connect Signals to Slots.  Each modifiable option widget shoudl be connected to updateDisplay()
	//Dialog buttons
	connect( this, SIGNAL( okClicked() ), this, SLOT( accept() ) ) ;
	connect( this, SIGNAL( cancelClicked() ), this, SLOT( reject() ) );

	//Catalog Tab
	connect( cat->showSAO, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( cat->CatalogList, SIGNAL( clicked( QListViewItem* ) ), this, SLOT( updateDisplay() ) );
	connect( cat->CatalogList, SIGNAL( selectionChanged() ), this, SLOT( selectCatalog() ) );
	connect( cat->showStarNames, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( cat->showStarMagnitude, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( cat->magSpinBoxDrawStars, SIGNAL( valueChanged( double ) ), this, SLOT( changeMagDrawStars( double ) ) );
	connect( cat->magSpinBoxDrawStarZoomOut, SIGNAL( valueChanged( double ) ), this, SLOT( changeMagDrawStarZoomOut( double ) ) );
	connect( cat->magSpinBoxDrawStarInfo, SIGNAL( valueChanged( double ) ), this, SLOT( changeMagDrawInfo( double ) ) );
	connect( cat->magSpinBoxDrawDeepSky, SIGNAL( valueChanged( double ) ), this, SLOT( changeMagDrawDeepSky( double ) ) );
	connect( cat->magSpinBoxDrawDeepSkyZoomOut, SIGNAL( valueChanged( double ) ), this, SLOT( changeMagDrawDeepSkyZoomOut( double ) ) );
	connect( cat->AddCatalog, SIGNAL( clicked() ), this, SLOT( slotAddCatalog() ) );
	connect( cat->RemoveCatalog, SIGNAL( clicked() ), this, SLOT( slotRemoveCatalog() ) );

	//Solar System Tab
	connect( ss->showSun, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( ss->showMoon, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( ss->showMercury, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( ss->showVenus, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( ss->showMars, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( ss->showJupiter, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( ss->showSaturn, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( ss->showUranus, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( ss->showNeptune, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( ss->showPluto, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( ss->showAllPlanets, SIGNAL( clicked() ), this, SLOT( markPlanets() ) );
	connect( ss->showNonePlanets, SIGNAL( clicked() ), this, SLOT( unMarkPlanets() ) );
	connect( ss->showPlanetNames, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( ss->showPlanetImages, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( ss->showAsteroids, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( ss->showAsteroidNames, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( ss->showComets, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( ss->showCometNames, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( ss->astDrawSpinBox, SIGNAL( valueChanged( double ) ), this, SLOT( changeAstDrawMagLimit( double ) ) );
	connect( ss->astNameSpinBox, SIGNAL( valueChanged( double ) ), this, SLOT( changeAstNameMagLimit( double ) ) );
	connect( ss->comNameSpinBox, SIGNAL( valueChanged( double ) ), this, SLOT( changeComNameMaxRad( double ) ) );
	connect( ss->autoTrail, SIGNAL( clicked() ), this, SLOT( changeAutoTrail() ) );
	connect( ss->fadePlanetTrails, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( ss->ClearAllTrails, SIGNAL( clicked() ), this, SLOT( clearPlanetTrails() ) );

	//Guide Tab
	connect( guide->showConstellLines, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( guide->showConstellBounds, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( guide->showConstellNames, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( guide->useLatinConstellNames, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( guide->useLatinConstellNames, SIGNAL( clicked() ), this, SLOT( sendClearCache() ) );
	connect( guide->useLocalConstellNames, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( guide->useLocalConstellNames, SIGNAL( clicked() ), this, SLOT( sendClearCache() ) );
	connect( guide->useAbbrevConstellNames, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( guide->useAbbrevConstellNames, SIGNAL( clicked() ), this, SLOT( sendClearCache() ) );
	connect( guide->showMilkyWay, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( guide->showMilkyWayFilled, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( guide->showGrid, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( guide->showEquator, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( guide->showEcliptic, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( guide->showHorizon, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( guide->showGround, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );

	//Color Tab
	connect( color->ColorPalette, SIGNAL( clicked( QListBoxItem* ) ), this, SLOT( newColor( QListBoxItem* ) ) );
	connect( color->PresetBox, SIGNAL( highlighted( int ) ), this, SLOT( slotPreset( int ) ) );
	connect( color->AddPreset, SIGNAL( clicked() ), this, SLOT( slotAddPreset() ) );
	connect( color->RemovePreset, SIGNAL( clicked() ), this, SLOT( slotRemovePreset() ) );
	connect( color->IntensityBox, SIGNAL (valueChanged (int)), SLOT (changeStarColorIntensity(int)) );
	connect( color->StarColorMode, SIGNAL( activated( int ) ), this, SLOT( changeStarColorMode( int ) ) );

	//Advanced Tab
	connect( adv->useRefraction, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( adv->animateSlewing, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( adv->autoLabel, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( adv->hoverLabel, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( adv->hideSpinBox, SIGNAL( scaleChanged( float ) ), this, SLOT( changeSlewTimeScale( float ) ) );
	connect( adv->hideObjects, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( adv->hideStars, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( adv->hidePlanets, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( adv->hideMess, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( adv->hideNGC, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( adv->hideIC, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( adv->hideMW, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( adv->hideCNames, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( adv->hideCLines, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( adv->hideCBounds, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( adv->hideGrid, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( adv->magSpinBoxHideStars, SIGNAL( valueChanged( double ) ), SLOT( changeMagHideStars( double ) ) );
        color->RemovePreset->setEnabled( false );
        }

ViewOpsDialog::~ViewOpsDialog(){
}

void ViewOpsDialog::changeMagDrawStars( double newValue )
{
	ksw->data()->setMagnitude( newValue );
	cat->magSpinBoxDrawStarZoomOut->setMaxValue( Options::magLimitDrawStar );

	// force redraw
	ksw->map()->forceUpdate();
}

void ViewOpsDialog::changeMagDrawStarZoomOut( double newValue ) {
	Options::magLimitDrawStarZoomOut = newValue;

	// force redraw
	ksw->map()->forceUpdate();
}

void ViewOpsDialog::changeMagDrawDeepSky( double newValue )
{
	Options::magLimitDrawDeepSky = newValue;
	cat->magSpinBoxDrawDeepSkyZoomOut->setMaxValue( Options::magLimitDrawDeepSky );

	// force redraw
	ksw->map()->forceUpdate();
}

void ViewOpsDialog::changeMagDrawDeepSkyZoomOut( double newValue ) {
	Options::magLimitDrawDeepSkyZoomOut = newValue;

	// force redraw
	ksw->map()->forceUpdate();
}

void ViewOpsDialog::changeMagHideStars( double newValue )
{
	Options::magLimitHideStar = newValue;
}

void ViewOpsDialog::changeMagDrawInfo( double newValue )
{
	Options::magLimitDrawStarInfo = newValue;
	// force redraw
	ksw->map()->forceUpdate();
}

void ViewOpsDialog::newColor( QListBoxItem *item ) {
	QPixmap temp( 30, 20 );
	QColor newColor;
	unsigned int i;

	for ( i=0; i < Options::colorScheme()->numberOfColors(); ++i ) {
		if ( item->text() == Options::colorScheme()->nameAt( i ) ) {
			QColor col( Options::colorScheme()->colorAt( i ) );

			if(KColorDialog::getColor( col )) newColor = col;
			break;
		}
	}

	//newColor will only be valid if the above if statement was found to be true during one of the for loop iterations
	if ( newColor.isValid() ) {
		temp.fill( newColor );
		color->ColorPalette->changeItem( temp, item->text(), color->ColorPalette->index( item ) );
		Options::colorScheme()->setColor( Options::colorScheme()->keyAt( i ), newColor.name() );
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

//old KDE2-compatible version
//	KLineEditDlg schemeDlg( i18n( "Enter a name for the new color scheme:" ),
//											QString::null, 0 );

//KDE3-only version (incompatible w/KDE2 because of different arguments)
	bool okPressed = false;
  QString schemename = KLineEditDlg::getText( i18n( "New Color Scheme" ),
											i18n( "Enter a name for the new color scheme:" ),
											QString::null, &okPressed, 0 );

	if ( okPressed ) {
		if ( !schemename.isEmpty() ) {
			if ( Options::colorScheme()->save( schemename ) ) {
				color->PresetBox->insertItem( schemename );
				PresetFileList.append( Options::colorScheme()->fileName() );
				ksw->addColorMenuItem( schemename, "cs_" + Options::colorScheme()->fileName().utf8() );
			}
		}
	}
}

void ViewOpsDialog::slotRemovePreset( void ) {
	QString name = color->PresetBox->currentText();
	QString filename = PresetFileList[ color->PresetBox->currentItem() ];
	QFile cdatFile;
	cdatFile.setName( locateLocal( "appdata", "colors.dat" ) ); //determine filename in local user KDE directory tree.

	//Remove entry from the ListBox and from the QStringList holding filenames.
	//We don't want another color scheme to be selected, so first
	//temporarily disconnect the "highlighted" signal.
	disconnect( color->PresetBox, SIGNAL( highlighted( int ) ), this, SLOT( slotPreset( int ) ) );

	PresetFileList.remove( PresetFileList.at( color->PresetBox->currentItem() ) );
	color->PresetBox->removeItem( color->PresetBox->currentItem() );

	//Reconnect the "highlighted" signal
	connect( color->PresetBox, SIGNAL( highlighted( int ) ), this, SLOT( slotPreset( int ) ) );

	//Remove action from color-schemes menu
	ksw->removeColorMenuItem( QString("cs_" + filename.left( filename.find(".colors"))).utf8() );

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
	if ( test.exists() ) color->RemovePreset->setEnabled( true );
	else color->RemovePreset->setEnabled( false );
	test.close();

	if ( Options::colorScheme()->load( filename ) ) {
	  if ( ksw->map()->starColorMode() != Options::colorScheme()->starColorMode() )
	    ksw->map()->setStarColorMode( Options::colorScheme()->starColorMode() );

		for ( unsigned int i=0; i < Options::colorScheme()->numberOfColors(); ++i ) {
			temp->fill( QColor( Options::colorScheme()->colorAt( i ) ) );
			color->ColorPalette->changeItem( *temp, Options::colorScheme()->nameAt( i ), i );
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
	if ( sender()->name() == QString( "CatalogList" ) )
		Options::showDeepSky = true;

	if ( sender()->name() == QString( "showSun" ) ||
			sender()->name() == QString( "showMoon" ) ||
			sender()->name() == QString( "showMercury" ) ||
			sender()->name() == QString( "showVenus" ) ||
			sender()->name() == QString( "showMars" ) ||
			sender()->name() == QString( "showJupiter" ) ||
			sender()->name() == QString( "showSaturn" ) ||
			sender()->name() == QString( "showUranus" ) ||
			sender()->name() == QString( "showNeptune" ) ||
			sender()->name() == QString( "showPluto" ) ||
			sender()->name() == QString( "showAsteroids" ) ||
			sender()->name() == QString( "showAsteroidNames" ) ||
			sender()->name() == QString( "showComets" ) ||
			sender()->name() == QString( "showCometNames" ) ||
			sender()->name() == QString( "showPlanetNames" ) ||
			sender()->name() == QString( "showPlanetImages" ) ||
			sender()->name() == QString( "showAllPlanets" ) ||
			sender()->name() == QString( "showNonePlanets" ) )
		Options::showPlanets = true;

//Set options according to current settings
//Catalogs Tab
	Options::showSAO = cat->showSAO->isChecked();
	Options::showMessier = showMessier->isOn();
	Options::showMessImages = showMessImages->isOn();
	Options::showNGC = showNGC->isOn();
	Options::showIC = showIC->isOn();
	for ( unsigned int i=0; i<Options::catalogCount(); ++i ) {
		QCheckListItem *item = (QCheckListItem*)(cat->CatalogList->findItem( Options::CatalogName[i], 0 ));
		Options::showCatalog[i] = item->isOn();
	}

	Options::showStarName = cat->showStarNames->isChecked();
	Options::showStarMagnitude = cat->showStarMagnitude->isChecked();
	//star options enabled only if showSAO is checked...
	cat->showStarNames->setEnabled( cat->showSAO->isChecked() );
	cat->showStarMagnitude->setEnabled( cat->showSAO->isChecked() );
	cat->magSpinBoxDrawStars->setEnabled( cat->showSAO->isChecked() );
	cat->magSpinBoxDrawStarZoomOut->setEnabled( cat->showSAO->isChecked() );
	cat->magSpinBoxDrawStarInfo->setEnabled( cat->showSAO->isChecked() );
	cat->textLabelMagStars->setEnabled( cat->showSAO->isChecked() );
	cat->textLabelMagStarsZoomOut->setEnabled( cat->showSAO->isChecked() );
	cat->textLabelMagStarInfo->setEnabled( cat->showSAO->isChecked() );
	cat->textLabelMag1->setEnabled( cat->showSAO->isChecked() );
	cat->textLabelMag2->setEnabled( cat->showSAO->isChecked() );
	cat->textLabelMag3->setEnabled( cat->showSAO->isChecked() );

//Planets Tab
	Options::showSun = ss->showSun->isChecked();
	Options::showMoon = ss->showMoon->isChecked();
	Options::showMercury = ss->showMercury->isChecked();
	Options::showVenus = ss->showVenus->isChecked();
	Options::showMars = ss->showMars->isChecked();
	Options::showJupiter = ss->showJupiter->isChecked();
	Options::showSaturn = ss->showSaturn->isChecked();
	Options::showUranus = ss->showUranus->isChecked();
	Options::showNeptune = ss->showNeptune->isChecked();
	Options::showPluto = ss->showPluto->isChecked();
	Options::showAsteroids = ss->showAsteroids->isChecked();
	Options::showComets = ss->showComets->isChecked();
	Options::showAsteroidName = ss->showAsteroidNames->isChecked();
	Options::showCometName = ss->showCometNames->isChecked();
	Options::showPlanetName = ss->showPlanetNames->isChecked();
	Options::showPlanetImage = ss->showPlanetImages->isChecked();

	ss->showAsteroidNames->setEnabled( ss->showAsteroids->isChecked() );
	ss->showCometNames->setEnabled( ss->showComets->isChecked() );
	ss->astDrawSpinBox->setEnabled( ss->showAsteroids->isChecked() );
	ss->astNameSpinBox->setEnabled( ss->showAsteroids->isChecked() && ss->showAsteroidNames->isChecked() );
	ss->comNameSpinBox->setEnabled( ss->showComets->isChecked() && ss->showCometNames->isChecked() );

	Options::fadePlanetTrails = ss->fadePlanetTrails->isChecked();
	Options::useAutoTrail = ss->autoTrail->isChecked();

//Guides Tab
	Options::showConstellLines = guide->showConstellLines->isChecked();
	Options::showConstellBounds = guide->showConstellBounds->isChecked();
	Options::showConstellNames = guide->showConstellNames->isChecked();
	Options::useLatinConstellNames = guide->useLatinConstellNames->isChecked();
	Options::useLocalConstellNames = guide->useLocalConstellNames->isChecked();
	Options::useAbbrevConstellNames = guide->useAbbrevConstellNames->isChecked();
	Options::showMilkyWay = guide->showMilkyWay->isChecked();
	Options::fillMilkyWay = guide->showMilkyWayFilled->isChecked();
	Options::showGrid = guide->showGrid->isChecked();
	Options::showEquator = guide->showEquator->isChecked();
	Options::showEcliptic = guide->showEcliptic->isChecked();
	Options::showHorizon = guide->showHorizon->isChecked();
	Options::showGround = guide->showGround->isChecked();
	//constellation name options enabled only if showConstellationNames is checked...
	guide->useLatinConstellNames->setEnabled( guide->showConstellNames->isChecked() );
	guide->useLocalConstellNames->setEnabled( guide->showConstellNames->isChecked() );
	guide->useAbbrevConstellNames->setEnabled( guide->showConstellNames->isChecked() );
	//fill MW checkbox enabled only if showMilkyWay is checked...
	guide->showMilkyWayFilled->setEnabled( guide->showMilkyWay->isChecked() );

	//Advanced Tab
	Options::useRefraction = adv->useRefraction->isChecked();
	Options::useAnimatedSlewing = adv->animateSlewing->isChecked();
	Options::useAutoLabel = adv->autoLabel->isChecked();
	Options::useHoverLabel = adv->hoverLabel->isChecked();
	Options::hideOnSlew = adv->hideObjects->isChecked();
	Options::hideStars = adv->hideStars->isChecked();
	Options::hidePlanets = adv->hidePlanets->isChecked();
	Options::hideMess = adv->hideMess->isChecked();
	Options::hideNGC = adv->hideNGC->isChecked();
	Options::hideIC = adv->hideIC->isChecked();
	Options::hideMW = adv->hideMW->isChecked();
	Options::hideCNames = adv->hideCNames->isChecked();
	Options::hideCLines = adv->hideCLines->isChecked();
	Options::hideCBounds = adv->hideCBounds->isChecked();
	Options::hideGrid = adv->hideGrid->isChecked();

	//HideBox widgets enabled only if hideObjects is checked...
	adv->hideSpinBox->setEnabled( adv->hideObjects->isChecked() );
	adv->textLabelHideTimeStep->setEnabled( adv->hideObjects->isChecked() );
	adv->HideBox->setEnabled( adv->hideObjects->isChecked() );

	// update time for all objects because they might be not initialized
	// it's needed when using horizontal coordinates
	ksw->data()->setFullTimeUpdate();
	ksw->updateTime();

	ksw->map()->forceUpdate();
}

void ViewOpsDialog::changeStarColorIntensity( int newValue ) {
	ksw->map()->setStarColorIntensity( newValue );
//	Options::starColorIntensity = ksw->map()->starColorIntensity();
	Options::colorScheme()->setStarColorIntensity( newValue );
	ksw->map()->forceUpdate();
}

void ViewOpsDialog::changeStarColorMode( int newValue ) {
	ksw->map()->setStarColorMode( newValue );
	Options::colorScheme()->setStarColorMode( newValue );
	if (newValue) color->IntensityBox->setEnabled( false );
	else color->IntensityBox->setEnabled( true );
	ksw->map()->forceUpdate();
}

void ViewOpsDialog::changeAstDrawMagLimit( double newValue ) {
	Options::magLimitAsteroid = newValue;
	// force redraw
	ksw->map()->forceUpdate();
}

void ViewOpsDialog::changeAstNameMagLimit( double newValue ) {
	Options::magLimitAsteroidName = newValue;
	// force redraw
	ksw->map()->forceUpdate();
}

void ViewOpsDialog::changeComNameMaxRad( double newValue ) {
	Options::maxRadCometName = newValue;
	// force redraw
	ksw->map()->forceUpdate();
}

void ViewOpsDialog::markPlanets( void ) {
	ss->showSun->setChecked( true );
	ss->showMoon->setChecked( true );
	ss->showMercury->setChecked( true );
	ss->showVenus->setChecked( true );
	ss->showMars->setChecked( true );
	ss->showJupiter->setChecked( true );
	ss->showSaturn->setChecked( true );
	ss->showUranus->setChecked( true );
	ss->showNeptune->setChecked( true );
	ss->showPluto->setChecked( true );
	updateDisplay();
}

void ViewOpsDialog::unMarkPlanets( void ) {
	ss->showSun->setChecked( false );
	ss->showMoon->setChecked( false );
	ss->showMercury->setChecked( false );
	ss->showVenus->setChecked( false );
	ss->showMars->setChecked( false );
	ss->showJupiter->setChecked( false );
	ss->showSaturn->setChecked( false );
	ss->showUranus->setChecked( false );
	ss->showNeptune->setChecked( false );
	ss->showPluto->setChecked( false );
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
		QCheckListItem *newCat = new QCheckListItem( cat->CatalogList, ac.name(), QCheckListItem::CheckBox );
		newCat->setOn( true );
		cat->CatalogList->insertItem( newCat );

		Options::catalogCount() = ++Options::catalogCount();
		Options::CatalogName.append( ac.name() );
		Options::CatalogFile.append( ac.filename() );
		Options::showCatalog.append( true );
//		kdWarning() << "CatalogCount: " << Options::catalogCount() << endl;
//		kdWarning() << "CatalogName.count(): " << Options::CatalogName.count() << endl;
		ksw->map()->forceUpdate();
	}
}

void ViewOpsDialog::slotRemoveCatalog() {
//Remove CatalogName, CatalogFile, and drawCatalog entries, and decrement CatalogCount
	for ( unsigned int i=0; i < Options::CatalogName.count(); ++i ) {
		if ( cat->CatalogList->currentItem()->text( 0 ) == Options::CatalogName[i] ) {
			Options::CatalogName.remove( Options::CatalogName.at(i) );
			Options::CatalogFile.remove( Options::CatalogFile.at(i) );
			Options::showCatalog.remove( Options::showCatalog.at(i) );
			--Options::catalogCount();
			break;
		}
	}

//Remove entry in the QListView
//	int i = CatalogList->itemPos( CatalogList->currentItem() );
	cat->CatalogList->takeItem( cat->CatalogList->currentItem() );

	ksw->map()->forceUpdate();
}

void ViewOpsDialog::selectCatalog() {
//If selected item is a custom catalog, enable the remove button (otherwise, disable it)
	cat->RemoveCatalog->setEnabled( false );
	for ( unsigned int i=0; i < Options::CatalogName.count(); ++i ) {
		if ( cat->CatalogList->currentItem()->text( 0 ) == Options::CatalogName[i] ) {
			cat->RemoveCatalog->setEnabled( true );
			break;
		}
	}
}

void ViewOpsDialog::changeSlewTimeScale( float f ) {
	if ( f < 0.0 ) {
		f = 0.0;
		adv->hideSpinBox->tsbox()->changeScale( 0.0 );
	}
	Options::slewTimeScale = f;
}

void ViewOpsDialog::changeAutoTrail( void ) {
	if ( ksw->map()->focusObject() && ksw->map()->focusObject()->isSolarSystem() ) {
		if ( ss->autoTrail->isChecked() ) {
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
	ksw->data()->PCat->findByName("Sun")->clearTrail();
	ksw->data()->PCat->findByName("Mercury")->clearTrail();
	ksw->data()->PCat->findByName("Venus")->clearTrail();
	ksw->data()->Moon->clearTrail();
	ksw->data()->PCat->findByName("Mars")->clearTrail();
	ksw->data()->PCat->findByName("Jupiter")->clearTrail();
	ksw->data()->PCat->findByName("Saturn")->clearTrail();
	ksw->data()->PCat->findByName("Uranus")->clearTrail();
	ksw->data()->PCat->findByName("Neptune")->clearTrail();
	ksw->data()->PCat->findByName("Pluto")->clearTrail();

	for ( KSPlanetBase *ksp = ksw->data()->asteroidList.first(); ksp; ksp = ksw->data()->asteroidList.next() )
		ksp->clearTrail();

	for ( KSPlanetBase *ksp = ksw->data()->cometList.first(); ksp; ksp = ksw->data()->cometList.next() )
		ksp->clearTrail();

	ksw->map()->forceUpdate();
}
