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

	cat->showSAO->setChecked( ksw->options()->drawSAO );
	cat->magSpinBoxDrawStars->setMinValue( 0.0 );
	cat->magSpinBoxDrawStars->setMaxValue( 13.0 );
	cat->magSpinBoxDrawStars->setValue( ksw->options()->magLimitDrawStar );
	cat->magSpinBoxDrawStars->setMinValue( 0.0 );
	cat->magSpinBoxDrawStars->setMaxValue( 9.0 );
	cat->magSpinBoxDrawStarInfo->setValue( ksw->options()->magLimitDrawStarInfo );
	cat->showStarNames->setChecked( ksw->options()->drawStarName );
	cat->showStarMagnitude->setChecked( ksw->options()->drawStarMagnitude );

	//Populate CatalogList
	showIC = new QCheckListItem( cat->CatalogList, i18n( "Index Catalog (IC)" ), QCheckListItem::CheckBox );
	showIC->setOn( ksw->options()->drawIC );

	showNGC = new QCheckListItem( cat->CatalogList, i18n( "New General Catalog (NGC)" ), QCheckListItem::CheckBox );
	showNGC->setOn( ksw->options()->drawNGC );

	showMessImages = new QCheckListItem( cat->CatalogList, i18n( "Messier Catalog (images)" ), QCheckListItem::CheckBox );
	showMessImages->setOn( ksw->options()->drawMessImages );

	showMessier = new QCheckListItem( cat->CatalogList, i18n( "Messier Catalog (symbols)" ), QCheckListItem::CheckBox );
	showMessier->setOn( ksw->options()->drawMessier );

	//Add custom catalogs, if necessary
	for ( unsigned int i=0; i<ksw->options()->CatalogCount; ++i ) { //loop over custom catalogs
		QCheckListItem *newItem = new QCheckListItem( cat->CatalogList, ksw->options()->CatalogName[i], QCheckListItem::CheckBox );
		newItem->setOn( ksw->options()->drawCatalog[i] );
	}

	catlay->addWidget( cat );

	//Solar System tab
	if ( KSUtils::openDataFile( imFile, "opssolarsystem.png" ) ) {
		imFile.close(); //Just need the filename...
		im.load( imFile.name() );
	}
	QFrame *sspage = addPage( i18n( "Solar System" ), QString::null, im );
	QVBoxLayout *sslay = new QVBoxLayout( sspage, 0, 6 );
	ss = new OpsSolarSystem( sspage );

	ss->showSun->setChecked( ksw->options()->drawSun );
	ss->showMoon->setChecked( ksw->options()->drawMoon );
	ss->showMercury->setChecked( ksw->options()->drawMercury );
	ss->showVenus->setChecked( ksw->options()->drawVenus );
	ss->showMars->setChecked( ksw->options()->drawMars );
	ss->showJupiter->setChecked( ksw->options()->drawJupiter );
	ss->showSaturn->setChecked( ksw->options()->drawSaturn );
	ss->showUranus->setChecked( ksw->options()->drawUranus );
	ss->showNeptune->setChecked( ksw->options()->drawNeptune );
	ss->showPluto->setChecked( ksw->options()->drawPluto );
	ss->showPlanetNames->setChecked( ksw->options()->drawPlanetName );
	ss->showPlanetImages->setChecked( ksw->options()->drawPlanetImage );
	ss->showAsteroids->setChecked( ksw->options()->drawAsteroids );
	ss->astDrawSpinBox->setMinValue( 2.5 );
	ss->astDrawSpinBox->setMaxValue( 12.0 );
	ss->astDrawSpinBox->setValue( ksw->options()->magLimitAsteroid );
	ss->astDrawSpinBox->setEnabled( ksw->options()->drawAsteroids );
	ss->showAsteroidNames->setChecked( ksw->options()->drawAsteroidName );
	ss->showAsteroidNames->setEnabled( ksw->options()->drawAsteroids );
	ss->astNameSpinBox->setMinValue( 2.5 );
	ss->astNameSpinBox->setMaxValue( 12.0 );
	ss->astNameSpinBox->setValue( ksw->options()->magLimitAsteroidName );
	ss->astNameSpinBox->setEnabled( ksw->options()->drawAsteroids && ksw->options()->drawAsteroidName );
	ss->showComets->setChecked( ksw->options()->drawComets );
	ss->showCometNames->setChecked( ksw->options()->drawCometName );
	ss->showCometNames->setEnabled( ksw->options()->drawComets );
	ss->comNameSpinBox->setValue( ksw->options()->maxRadCometName );
	ss->comNameSpinBox->setEnabled( ksw->options()->drawComets && ksw->options()->drawCometName );
	ss->autoTrail->setChecked( ksw->options()->useAutoTrail );
	ss->fadePlanetTrails->setChecked( ksw->options()->fadePlanetTrails );

	sslay->addWidget( ss );

	//Guides tab
	if ( KSUtils::openDataFile( imFile, "opsguides.png" ) ) {
		imFile.close(); //Just need the filename...
		im.load( imFile.name() );
	}
	QFrame *guidepage = addPage( i18n( "Guides" ), QString::null, im );
	QVBoxLayout *guidelay = new QVBoxLayout( guidepage, 0, 6 );
	guide = new OpsGuides( guidepage );

	guide->showConstellLines->setChecked( ksw->options()->drawConstellLines );
	guide->showConstellNames->setChecked( ksw->options()->drawConstellNames );
	guide->ConstellOptions->setEnabled( ksw->options()->drawConstellNames );
	guide->useLatinConstellNames->setChecked( ksw->options()->useLatinConstellNames );
	guide->useLocalConstellNames->setChecked( ksw->options()->useLocalConstellNames );
	guide->useAbbrevConstellNames->setChecked( ksw->options()->useAbbrevConstellNames );
	guide->showMilkyWay->setChecked( ksw->options()->drawMilkyWay );
	guide->showMilkyWayFilled->setChecked( ksw->options()->fillMilkyWay );
	guide->showMilkyWayFilled->setEnabled( ksw->options()->drawMilkyWay );
	guide->showGrid->setChecked( ksw->options()->drawGrid );
	guide->showEquator->setChecked( ksw->options()->drawEquator );
	guide->showEcliptic->setChecked( ksw->options()->drawEcliptic );
	guide->showHorizon->setChecked( ksw->options()->drawHorizon );
	guide->showGround->setChecked( ksw->options()->drawGround );

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

	for ( unsigned int i=0; i < ksw->options()->colorScheme()->numberOfColors(); ++i ) {
		QPixmap col( 30, 20 );
		col.fill( QColor( ksw->options()->colorScheme()->colorAt( i ) ) );
		color->ColorPalette->insertItem( col, ksw->options()->colorScheme()->nameAt( i ) );
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

	color->IntensityBox->setValue( ksw->options()->colorScheme()->starColorIntensity() );
	color->StarColorMode->insertItem( i18n( "use realistic star colors", "Real Colors" ) );
	color->StarColorMode->insertItem( i18n( "show stars as red circles", "Solid Red" ) );
	color->StarColorMode->insertItem( i18n( "show stars as black circles", "Solid Black" ) );
	color->StarColorMode->insertItem( i18n( "show stars as white circles", "Solid White" ) );
	color->StarColorMode->setCurrentItem( ksw->options()->colorScheme()->starColorMode() );

	colorlay->addWidget( color );

	//Advanced tab
	if ( KSUtils::openDataFile( imFile, "opsadvanced.png" ) ) {
		imFile.close(); //Just need the filename...
		im.load( imFile.name() );
	}
	QFrame *advpage = addPage( i18n( "Advanced" ), QString::null, im );
	QVBoxLayout *advlay = new QVBoxLayout( advpage, 0, 6 );
	adv = new OpsAdvanced( advpage );

	adv->useRefraction->setChecked( ksw->options()->useRefraction );
	adv->animateSlewing->setChecked( ksw->options()->useAnimatedSlewing );
	adv->autoLabel->setChecked( ksw->options()->useAutoLabel );
	adv->hideSpinBox->tsbox()->changeScale( (float)ksw->options()->slewTimeScale );
	adv->hideObjects->setChecked( ksw->options()->hideOnSlew );
	adv->hideStars->setChecked( ksw->options()->hideStars );
	adv->hidePlanets->setChecked( ksw->options()->hidePlanets );
	adv->hideMess->setChecked( ksw->options()->hideMess );
	adv->hideNGC->setChecked( ksw->options()->hideNGC );
	adv->hideIC->setChecked( ksw->options()->hideIC );
	adv->hideMW->setChecked( ksw->options()->hideMW );
	adv->hideCNames->setChecked( ksw->options()->hideCNames );
	adv->hideCLines->setChecked( ksw->options()->hideCLines );
	adv->hideGrid->setChecked( ksw->options()->hideGrid );
	adv->magSpinBoxHideStars->setValue( ksw->options()->magLimitHideStar );

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
	connect( cat->magSpinBoxDrawStarInfo, SIGNAL( valueChanged( double ) ), this, SLOT( changeMagDrawInfo( double ) ) );
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
	connect( adv->hideGrid, SIGNAL( clicked() ), this, SLOT( updateDisplay() ) );
	connect( adv->magSpinBoxHideStars, SIGNAL( valueChanged( double ) ), SLOT( changeMagHideStars( double ) ) );
}

ViewOpsDialog::~ViewOpsDialog(){
}

void ViewOpsDialog::changeMagDrawStars( double newValue )
{
	ksw->data()->setMagnitude( newValue );

	// force redraw
	ksw->map()->forceUpdate();
}

void ViewOpsDialog::changeMagHideStars( double newValue )
{
	ksw->options()->magLimitHideStar = newValue;
}

void ViewOpsDialog::changeMagDrawInfo( double newValue )
{
	ksw->options()->magLimitDrawStarInfo = newValue;
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

			if(KColorDialog::getColor( col )) newColor = col;
			break;
		}
	}

	//newColor will only be valid if the above if statement was found to be true during one of the for loop iterations
	if ( newColor.isValid() ) {
		temp.fill( newColor );
		color->ColorPalette->changeItem( temp, item->text(), color->ColorPalette->index( item ) );
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
			if ( ksw->options()->colorScheme()->save( schemename ) ) {
				color->PresetBox->insertItem( schemename );
				PresetFileList.append( ksw->options()->colorScheme()->fileName() );
				ksw->addColorMenuItem( schemename, "cs_" + ksw->options()->colorScheme()->fileName().utf8() );
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

	if ( ksw->options()->colorScheme()->load( filename ) ) {
	  if ( ksw->map()->starColorMode() != ksw->options()->colorScheme()->starColorMode() )
	    ksw->map()->setStarColorMode( ksw->options()->colorScheme()->starColorMode() );

		for ( unsigned int i=0; i < ksw->options()->colorScheme()->numberOfColors(); ++i ) {
			temp->fill( QColor( ksw->options()->colorScheme()->colorAt( i ) ) );
			color->ColorPalette->changeItem( *temp, ksw->options()->colorScheme()->nameAt( i ), i );
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
		ksw->options()->drawDeepSky = true;

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
		ksw->options()->drawPlanets = true;

//Set options according to current settings
//Catalogs Tab
	ksw->options()->drawSAO = cat->showSAO->isChecked();
	ksw->options()->drawMessier = showMessier->isOn();
	ksw->options()->drawMessImages = showMessImages->isOn();
	ksw->options()->drawNGC = showNGC->isOn();
	ksw->options()->drawIC = showIC->isOn();
	for ( unsigned int i=0; i<ksw->options()->CatalogCount; ++i ) {
		QCheckListItem *item = (QCheckListItem*)(cat->CatalogList->findItem( ksw->options()->CatalogName[i], 0 ));
		ksw->options()->drawCatalog[i] = item->isOn();
	}

	ksw->options()->drawStarName = cat->showStarNames->isChecked();
	ksw->options()->drawStarMagnitude = cat->showStarMagnitude->isChecked();
	//star options enabled only if showSAO is checked...
	cat->showStarNames->setEnabled( cat->showSAO->isChecked() );
	cat->showStarMagnitude->setEnabled( cat->showSAO->isChecked() );
	cat->magSpinBoxDrawStars->setEnabled( cat->showSAO->isChecked() );
	cat->magSpinBoxDrawStarInfo->setEnabled( cat->showSAO->isChecked() );
	cat->textLabelMagStars->setEnabled( cat->showSAO->isChecked() );
	cat->textLabelMagStarInfo->setEnabled( cat->showSAO->isChecked() );
	cat->textLabelMag1->setEnabled( cat->showSAO->isChecked() );
	cat->textLabelMag2->setEnabled( cat->showSAO->isChecked() );

//Planets Tab
	ksw->options()->drawSun = ss->showSun->isChecked();
	ksw->options()->drawMoon = ss->showMoon->isChecked();
	ksw->options()->drawMercury = ss->showMercury->isChecked();
	ksw->options()->drawVenus = ss->showVenus->isChecked();
	ksw->options()->drawMars = ss->showMars->isChecked();
	ksw->options()->drawJupiter = ss->showJupiter->isChecked();
	ksw->options()->drawSaturn = ss->showSaturn->isChecked();
	ksw->options()->drawUranus = ss->showUranus->isChecked();
	ksw->options()->drawNeptune = ss->showNeptune->isChecked();
	ksw->options()->drawPluto = ss->showPluto->isChecked();
	ksw->options()->drawAsteroids = ss->showAsteroids->isChecked();
	ksw->options()->drawComets = ss->showComets->isChecked();
	ksw->options()->drawAsteroidName = ss->showAsteroidNames->isChecked();
	ksw->options()->drawCometName = ss->showCometNames->isChecked();
	ksw->options()->drawPlanetName = ss->showPlanetNames->isChecked();
	ksw->options()->drawPlanetImage = ss->showPlanetImages->isChecked();

	ss->showAsteroidNames->setEnabled( ss->showAsteroids->isChecked() );
	ss->showCometNames->setEnabled( ss->showComets->isChecked() );
	ss->astDrawSpinBox->setEnabled( ss->showAsteroids->isChecked() );
	ss->astNameSpinBox->setEnabled( ss->showAsteroids->isChecked() && ss->showAsteroidNames->isChecked() );
	ss->comNameSpinBox->setEnabled( ss->showComets->isChecked() && ss->showCometNames->isChecked() );

	ksw->options()->fadePlanetTrails = ss->fadePlanetTrails->isChecked();
	ksw->options()->useAutoTrail = ss->autoTrail->isChecked();

//Guides Tab
	ksw->options()->drawConstellLines = guide->showConstellLines->isChecked();
	ksw->options()->drawConstellNames = guide->showConstellNames->isChecked();
	ksw->options()->useLatinConstellNames = guide->useLatinConstellNames->isChecked();
	ksw->options()->useLocalConstellNames = guide->useLocalConstellNames->isChecked();
	ksw->options()->useAbbrevConstellNames = guide->useAbbrevConstellNames->isChecked();
	ksw->options()->drawMilkyWay = guide->showMilkyWay->isChecked();
	ksw->options()->fillMilkyWay = guide->showMilkyWayFilled->isChecked();
	ksw->options()->drawGrid = guide->showGrid->isChecked();
	ksw->options()->drawEquator = guide->showEquator->isChecked();
	ksw->options()->drawEcliptic = guide->showEcliptic->isChecked();
	ksw->options()->drawHorizon = guide->showHorizon->isChecked();
	ksw->options()->drawGround = guide->showGround->isChecked();
	//constellation name options enabled only if showConstellationNames is checked...
	guide->useLatinConstellNames->setEnabled( guide->showConstellNames->isChecked() );
	guide->useLocalConstellNames->setEnabled( guide->showConstellNames->isChecked() );
	guide->useAbbrevConstellNames->setEnabled( guide->showConstellNames->isChecked() );
	//fill MW checkbox enabled only if showMilkyWay is checked...
	guide->showMilkyWayFilled->setEnabled( guide->showMilkyWay->isChecked() );

	//Advanced Tab
	ksw->options()->useRefraction = adv->useRefraction->isChecked();
	ksw->options()->useAnimatedSlewing = adv->animateSlewing->isChecked();
	ksw->options()->useAutoLabel = adv->autoLabel->isChecked();
	ksw->options()->hideOnSlew = adv->hideObjects->isChecked();
	ksw->options()->hideStars = adv->hideStars->isChecked();
	ksw->options()->hidePlanets = adv->hidePlanets->isChecked();
	ksw->options()->hideMess = adv->hideMess->isChecked();
	ksw->options()->hideNGC = adv->hideNGC->isChecked();
	ksw->options()->hideIC = adv->hideIC->isChecked();
	ksw->options()->hideMW = adv->hideMW->isChecked();
	ksw->options()->hideCNames = adv->hideCNames->isChecked();
	ksw->options()->hideCLines = adv->hideCLines->isChecked();
	ksw->options()->hideGrid = adv->hideGrid->isChecked();
	//HideBox widgets enabled only if hideObjects is checked...
	adv->hideSpinBox->setEnabled( adv->hideObjects->isChecked() );
	adv->hideStars->setEnabled( adv->hideObjects->isChecked() );
	adv->magSpinBoxHideStars->setEnabled( adv->hideObjects->isChecked() );
	adv->hidePlanets->setEnabled( adv->hideObjects->isChecked() );
	adv->hideMess->setEnabled( adv->hideObjects->isChecked() );
	adv->hideNGC->setEnabled( adv->hideObjects->isChecked() );
	adv->hideIC->setEnabled( adv->hideObjects->isChecked() );
	adv->hideMW->setEnabled( adv->hideObjects->isChecked() );
	adv->hideCNames->setEnabled( adv->hideObjects->isChecked() );
	adv->hideCLines->setEnabled( adv->hideObjects->isChecked() );
	adv->hideGrid->setEnabled( adv->hideObjects->isChecked() );

	// update time for all objects because they might be not initialized
	// it's needed when using horizontal coordinates
	ksw->data()->setFullTimeUpdate();
	ksw->updateTime();

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
	if (newValue) color->IntensityBox->setEnabled( false );
	else color->IntensityBox->setEnabled( true );
	ksw->map()->forceUpdate();
}

void ViewOpsDialog::changeAstDrawMagLimit( double newValue ) {
	ksw->options()->magLimitAsteroid = newValue;
	// force redraw
	ksw->map()->forceUpdate();
}

void ViewOpsDialog::changeAstNameMagLimit( double newValue ) {
	ksw->options()->magLimitAsteroidName = newValue;
	// force redraw
	ksw->map()->forceUpdate();
}

void ViewOpsDialog::changeComNameMaxRad( double newValue ) {
	ksw->options()->maxRadCometName = newValue;
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
		if ( cat->CatalogList->currentItem()->text( 0 ) == ksw->options()->CatalogName[i] ) {
			ksw->options()->CatalogName.remove( ksw->options()->CatalogName.at(i) );
			ksw->options()->CatalogFile.remove( ksw->options()->CatalogFile.at(i) );
			ksw->options()->drawCatalog.remove( ksw->options()->drawCatalog.at(i) );
			--ksw->options()->CatalogCount;
			break;
		}
	}

//Remove entry in the QListView
//	int i = CatalogList->itemPos( CatalogList->currentItem() );
	cat->CatalogList->takeItem( cat->CatalogList->currentItem() );
}

void ViewOpsDialog::selectCatalog() {
//If selected item is a custom catalog, enable the remove button (otherwise, disable it)
	cat->RemoveCatalog->setEnabled( false );
	for ( unsigned int i=0; i < ksw->options()->CatalogName.count(); ++i ) {
		if ( cat->CatalogList->currentItem()->text( 0 ) == ksw->options()->CatalogName[i] ) {
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
	ksw->options()->slewTimeScale = f;
}

void ViewOpsDialog::changeAutoTrail( void ) {
	if ( ksw->data()->isSolarSystem( ksw->map()->focusObject() ) ) {
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
