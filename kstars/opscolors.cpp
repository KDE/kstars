/***************************************************************************
                          opscolors.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Sun Feb 29  2004
    copyright            : (C) 2004 by Jason Harris
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

#include <qfile.h>

#include <klocale.h>
#include <knuminput.h>
#include <kcombobox.h>
#include <kpushbutton.h>
#include <kcolordialog.h>
#include <kmessagebox.h>
#include <kinputdialog.h>
#include <kstandarddirs.h>

#include "opscolors.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "skymap.h"
#include "colorscheme.h"

OpsColors::OpsColors( QWidget *p, const char *name, WFlags fl ) 
	: OpsColorsUI( p, name, fl ) 
{
	ksw = (KStars *)p;

	//Populate list of adjustable colors
	for ( unsigned int i=0; i < ksw->data()->colorScheme()->numberOfColors(); ++i ) {
		QPixmap col( 30, 20 );
		col.fill( QColor( ksw->data()->colorScheme()->colorAt( i ) ) );
		ColorPalette->insertItem( col, ksw->data()->colorScheme()->nameAt( i ) );
	}

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
	file.setName( locate( "appdata", "colors.dat" ) );
	if ( file.exists() && file.open( IO_ReadOnly ) ) {
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

	kcfg_StarColorIntensity->setValue( ksw->data()->colorScheme()->starColorIntensity() );
	kcfg_StarColorMode->insertItem( i18n( "use realistic star colors", "Real Colors" ) );
	kcfg_StarColorMode->insertItem( i18n( "show stars as red circles", "Solid Red" ) );
	kcfg_StarColorMode->insertItem( i18n( "show stars as black circles", "Solid Black" ) );
	kcfg_StarColorMode->insertItem( i18n( "show stars as white circles", "Solid White" ) );
	kcfg_StarColorMode->setCurrentItem( ksw->data()->colorScheme()->starColorMode() );

	if ( ksw->data()->colorScheme()->starColorMode() != 0 ) //mode is not "Real Colors"
		kcfg_StarColorIntensity->setEnabled( false );
	else
		kcfg_StarColorIntensity->setEnabled( true );
		
	connect( ColorPalette, SIGNAL( clicked( QListBoxItem* ) ), this, SLOT( newColor( QListBoxItem* ) ) );
	connect( kcfg_StarColorIntensity, SIGNAL( valueChanged( int ) ), this, SLOT( slotStarColorIntensity( int ) ) );
	connect( kcfg_StarColorMode, SIGNAL( activated( int ) ), this, SLOT( slotStarColorMode( int ) ) );
	connect( PresetBox, SIGNAL( highlighted( int ) ), this, SLOT( slotPreset( int ) ) );
	connect( AddPreset, SIGNAL( clicked() ), this, SLOT( slotAddPreset() ) );
	connect( RemovePreset, SIGNAL( clicked() ), this, SLOT( slotRemovePreset() ) );

	RemovePreset->setEnabled( false );
}

//empty destructor
OpsColors::~OpsColors() {}

void OpsColors::newColor( QListBoxItem *item ) {
	QPixmap pixmap( 30, 20 );
	QColor NewColor;
	unsigned int i;

	for ( i=0; i < ksw->data()->colorScheme()->numberOfColors(); ++i ) {
		if ( item->text() == ksw->data()->colorScheme()->nameAt( i ) ) {
			QColor col( ksw->data()->colorScheme()->colorAt( i ) );

			if(KColorDialog::getColor( col )) NewColor = col;
			break;
		}
	}

	//NewColor will only be valid if the above if statement was found to be true during one of the for loop iterations
	if ( NewColor.isValid() ) {
		pixmap.fill( NewColor );
		ColorPalette->changeItem( pixmap, item->text(), ColorPalette->index( item ) );
		ksw->data()->colorScheme()->setColor( ksw->data()->colorScheme()->keyAt( i ), NewColor.name() );
	}

	ksw->map()->forceUpdate();
}

void OpsColors::slotPreset( int index ) {
	QStringList::Iterator it = PresetFileList.at( index );
	bool result = setColors( *it );
	if (!result) {
		QString message = i18n( "The specified color scheme file (%1) could not be found, or was corrupt." ).arg( QString(*it) );
		KMessageBox::sorry( 0, message, i18n( "Could Not Set Color Scheme" ) );
	}
}

bool OpsColors::setColors( QString filename ) {
	QPixmap *temp = new QPixmap( 30, 20 );

	//just checking if colorscheme is removable...
	QFile test;
	test.setName( locateLocal( "appdata", filename ) ); //try filename in local user KDE directory tree.
	if ( test.exists() ) RemovePreset->setEnabled( true );
	else RemovePreset->setEnabled( false );
	test.close();

	ksw->loadColorScheme( filename );
	kcfg_StarColorMode->setCurrentItem( ksw->data()->colorScheme()->starColorMode() );
	kcfg_StarColorIntensity->setValue( ksw->data()->colorScheme()->starColorIntensity() );

	if ( ksw->map()->starColorMode() != ksw->data()->colorScheme()->starColorMode() )
		ksw->map()->setStarColorMode( ksw->data()->colorScheme()->starColorMode() );

	if ( ksw->map()->starColorIntensity() != ksw->data()->colorScheme()->starColorIntensity() )
		ksw->map()->setStarColorIntensity( ksw->data()->colorScheme()->starColorIntensity() );

	for ( unsigned int i=0; i < ksw->data()->colorScheme()->numberOfColors(); ++i ) {
		temp->fill( QColor( ksw->data()->colorScheme()->colorAt( i ) ) );
		ColorPalette->changeItem( *temp, ksw->data()->colorScheme()->nameAt( i ), i );
	}

	ksw->map()->forceUpdate();
	return true;
}

void OpsColors::slotAddPreset() {
	bool okPressed = false;
	QString schemename = KInputDialog::getText( i18n( "New Color Scheme" ),
						    i18n( "Enter a name for the new color scheme:" ),
						    QString::null, &okPressed, 0 );

	if ( okPressed && ! schemename.isEmpty() ) {
		if ( ksw->data()->colorScheme()->save( schemename ) ) {
			PresetBox->insertItem( schemename );
			PresetBox->setCurrentItem( PresetBox->findItem( schemename ) );
			QString fname = ksw->data()->colorScheme()->fileName();
			PresetFileList.append( fname );
			ksw->addColorMenuItem( schemename, QString("cs_" + fname.left(fname.find(".colors"))).utf8() );
		}
	}
}

void OpsColors::slotRemovePreset() {
	QString name = PresetBox->currentText();
	QString filename = PresetFileList[ PresetBox->currentItem() ];
	QFile cdatFile;
	cdatFile.setName( locateLocal( "appdata", "colors.dat" ) ); //determine filename in local user KDE directory tree.

	//Remove action from color-schemes menu
	ksw->removeColorMenuItem( QString("cs_" + filename.left( filename.find(".colors"))).utf8() );

	if ( !cdatFile.exists() || !cdatFile.open( IO_ReadWrite ) ) {
		QString message = i18n( "Local color scheme index file could not be opened.\nScheme cannot be removed." );
		KMessageBox::sorry( 0, message, i18n( "Could Not Open File" ) );
	} else {
		//Remove entry from the ListBox and from the QStringList holding filenames.
		//We don't want another color scheme to be selected, so first
		//temporarily disconnect the "highlighted" signal.
		disconnect( PresetBox, SIGNAL( highlighted( int ) ), this, SLOT( slotPreset( int ) ) );
		PresetBox->removeItem( PresetBox->currentItem() );
		PresetBox->setCurrentItem( -1 );
		RemovePreset->setEnabled( false );
		
		//Reconnect the "highlighted" signal
		connect( PresetBox, SIGNAL( highlighted( int ) ), this, SLOT( slotPreset( int ) ) );

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
			QString message = i18n( "Could not find an entry named %1 in colors.dat." ).arg( name );
			KMessageBox::sorry( 0, message, i18n( "Scheme Not Found" ) );
		}
		cdatFile.close();
	}
}

void OpsColors::slotStarColorMode( int i ) {
	ksw->data()->colorScheme()->setStarColorMode( i );
	if ( ksw->map()->starColorMode() != ksw->data()->colorScheme()->starColorMode() )
		ksw->map()->setStarColorMode( ksw->data()->colorScheme()->starColorMode() );

	if ( ksw->data()->colorScheme()->starColorMode() != 0 ) //mode is not "Real Colors"
		kcfg_StarColorIntensity->setEnabled( false );
	else
		kcfg_StarColorIntensity->setEnabled( true );
}

void OpsColors::slotStarColorIntensity( int i ) {
	ksw->data()->colorScheme()->setStarColorIntensity( i );
	if ( ksw->map()->starColorIntensity() != ksw->data()->colorScheme()->starColorIntensity() )
		ksw->map()->setStarColorIntensity( ksw->data()->colorScheme()->starColorIntensity() );

}

#include "opscolors.moc"
