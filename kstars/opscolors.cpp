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

#include <QFile>
#include <QPixmap>
#include <QTextStream>

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

OpsColors::OpsColors( KStars *_ks ) 
	: QFrame( _ks ), ksw(_ks)
{
	setupUi( this );

	//Populate list of adjustable colors
	for ( unsigned int i=0; i < ksw->data()->colorScheme()->numberOfColors(); ++i ) {
		QPixmap col( 30, 20 );
		col.fill( QColor( ksw->data()->colorScheme()->colorAt( i ) ) );
		ColorPalette->insertItem( col, ksw->data()->colorScheme()->nameAt( i ) );
	}

	PresetBox->insertItem( i18nc( "use default color scheme", "Default Colors" ) );
	PresetBox->insertItem( i18nc( "use 'star chart' color scheme", "Star Chart" ) );
	PresetBox->insertItem( i18nc( "use 'night vision' color scheme", "Night Vision" ) );
	PresetBox->insertItem( i18nc( "use 'moonless night' color scheme", "Moonless Night" ) );

	PresetFileList.append( "default.colors" );
	PresetFileList.append( "chart.colors" );
	PresetFileList.append( "night.colors" );
	PresetFileList.append( "moonless-night.colors" );

	QFile file;
	QString line, schemeName, filename;
	file.setFileName( KStandardDirs::locate( "appdata", "colors.dat" ) );
	if ( file.exists() && file.open( QIODevice::ReadOnly ) ) {
		QTextStream stream( &file );

  	while ( !stream.atEnd() ) {
			line = stream.readLine();
			schemeName = line.left( line.indexOf( ':' ) );
			filename = line.mid( line.indexOf( ':' ) +1, line.length() );
			PresetBox->insertItem( schemeName );
			PresetFileList.append( filename );
	  }
		file.close();
	}

	kcfg_StarColorIntensity->setValue( ksw->data()->colorScheme()->starColorIntensity() );
	kcfg_StarColorMode->addItem( i18nc( "use realistic star colors", "Real Colors" ) );
	kcfg_StarColorMode->addItem( i18nc( "show stars as red circles", "Solid Red" ) );
	kcfg_StarColorMode->addItem( i18nc( "show stars as black circles", "Solid Black" ) );
	kcfg_StarColorMode->addItem( i18nc( "show stars as white circles", "Solid White" ) );
	kcfg_StarColorMode->setCurrentIndex( ksw->data()->colorScheme()->starColorMode() );

	if ( ksw->data()->colorScheme()->starColorMode() != 0 ) //mode is not "Real Colors"
		kcfg_StarColorIntensity->setEnabled( false );
	else
		kcfg_StarColorIntensity->setEnabled( true );
		
	connect( ColorPalette, SIGNAL( clicked( Q3ListBoxItem* ) ), this, SLOT( newColor( Q3ListBoxItem* ) ) );
	connect( kcfg_StarColorIntensity, SIGNAL( valueChanged( int ) ), this, SLOT( slotStarColorIntensity( int ) ) );
	connect( kcfg_StarColorMode, SIGNAL( activated( int ) ), this, SLOT( slotStarColorMode( int ) ) );
	connect( PresetBox, SIGNAL( highlighted( int ) ), this, SLOT( slotPreset( int ) ) );
	connect( AddPreset, SIGNAL( clicked() ), this, SLOT( slotAddPreset() ) );
	connect( RemovePreset, SIGNAL( clicked() ), this, SLOT( slotRemovePreset() ) );

	RemovePreset->setEnabled( false );
}

//empty destructor
OpsColors::~OpsColors() {}

void OpsColors::newColor( Q3ListBoxItem *item ) {
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
	QString sPreset = PresetFileList.at( index );
	bool result = setColors( sPreset );
	if (!result) {
		QString message = i18n( "The specified color scheme file (%1) could not be found, or was corrupt.", sPreset );
		KMessageBox::sorry( 0, message, i18n( "Could Not Set Color Scheme" ) );
	}
}

bool OpsColors::setColors( QString filename ) {
	QPixmap temp( 30, 20 );

	//just checking if colorscheme is removable...
	QFile test;
	test.setFileName( KStandardDirs::locateLocal( "appdata", filename ) ); //try filename in local user KDE directory tree.
	if ( test.exists() ) RemovePreset->setEnabled( true );
	else RemovePreset->setEnabled( false );
	test.close();

	ksw->loadColorScheme( filename );
	kcfg_StarColorMode->setCurrentIndex( ksw->data()->colorScheme()->starColorMode() );
	kcfg_StarColorIntensity->setValue( ksw->data()->colorScheme()->starColorIntensity() );

	if ( ksw->data()->skyComposite()->starColorMode() != ksw->data()->colorScheme()->starColorMode() )
		ksw->data()->skyComposite()->setStarColorMode( ksw->data()->colorScheme()->starColorMode() );

	if ( ksw->data()->skyComposite()->starColorIntensity() != ksw->data()->colorScheme()->starColorIntensity() )
		ksw->data()->skyComposite()->setStarColorIntensity( ksw->data()->colorScheme()->starColorIntensity() );

	for ( unsigned int i=0; i < ksw->data()->colorScheme()->numberOfColors(); ++i ) {
		temp.fill( QColor( ksw->data()->colorScheme()->colorAt( i ) ) );
		ColorPalette->changeItem( temp, ksw->data()->colorScheme()->nameAt( i ), i );
	}

	ksw->map()->forceUpdate();
	return true;
}

void OpsColors::slotAddPreset() {
	bool okPressed = false;
	QString schemename = KInputDialog::getText( i18n( "New Color Scheme" ),
						    i18n( "Enter a name for the new color scheme:" ),
						    QString(), &okPressed, 0 );

	if ( okPressed && ! schemename.isEmpty() ) {
		if ( ksw->data()->colorScheme()->save( schemename ) ) {
			PresetBox->insertItem( schemename );
			PresetBox->setCurrentItem( PresetBox->findItem( schemename ) );
			QString fname = ksw->data()->colorScheme()->fileName();
			PresetFileList.append( fname );
			ksw->addColorMenuItem( schemename, QString("cs_" + fname.left(fname.indexOf(".colors"))).toUtf8() );
		}
	}
}

void OpsColors::slotRemovePreset() {
	QString name = PresetBox->currentText();
	QString filename = PresetFileList[ PresetBox->currentItem() ];
	QFile cdatFile;
	cdatFile.setFileName( KStandardDirs::locateLocal( "appdata", "colors.dat" ) ); //determine filename in local user KDE directory tree.

	//Remove action from color-schemes menu
	ksw->removeColorMenuItem( QString("cs_" + filename.left( filename.indexOf(".colors"))).toUtf8() );

	if ( !cdatFile.exists() || !cdatFile.open( QIODevice::ReadWrite ) ) {
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

		while ( !stream.atEnd() ) {
			QString line = stream.readLine();
			if ( line.left( line.indexOf(':') ) != name ) slist.append( line );
			else removed = true;
		}

		if ( removed ) { //Entry was removed; delete the corresponding .colors file.
			QFile colorFile;
			colorFile.setFileName( locateLocal( "appdata", filename ) ); //determine filename in local user KDE directory tree.
			if ( !colorFile.remove() ) {
				QString message = i18n( "Could not delete the file: %1", colorFile.fileName() );
				KMessageBox::sorry( 0, message, i18n( "Error Deleting File" ) );
			}

			//remove the old colors.dat file, and rebuild it with the modified string list.
			cdatFile.remove();
			cdatFile.open( QIODevice::ReadWrite );
			QTextStream stream2( &cdatFile );
			for( int i=0; i<slist.count(); ++i )
				stream << slist[i] << endl;
		} else {
			QString message = i18n( "Could not find an entry named %1 in colors.dat.", name );
			KMessageBox::sorry( 0, message, i18n( "Scheme Not Found" ) );
		}
		cdatFile.close();
	}
}

void OpsColors::slotStarColorMode( int i ) {
	ksw->data()->colorScheme()->setStarColorMode( i );
	if ( ksw->data()->skyComposite()->starColorMode() != ksw->data()->colorScheme()->starColorMode() )
		ksw->data()->skyComposite()->setStarColorMode( ksw->data()->colorScheme()->starColorMode() );

	if ( ksw->data()->colorScheme()->starColorMode() != 0 ) //mode is not "Real Colors"
		kcfg_StarColorIntensity->setEnabled( false );
	else
		kcfg_StarColorIntensity->setEnabled( true );
}

void OpsColors::slotStarColorIntensity( int i ) {
	ksw->data()->colorScheme()->setStarColorIntensity( i );
	if ( ksw->data()->skyComposite()->starColorIntensity() != ksw->data()->colorScheme()->starColorIntensity() )
		ksw->data()->skyComposite()->setStarColorIntensity( ksw->data()->colorScheme()->starColorIntensity() );

}

#include "opscolors.moc"
