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

#include "opscolors.h"

#include <QFile>
#include <QPixmap>
#include <QTextStream>

#include <kactioncollection.h>
#include <kapplication.h>
#include <klocale.h>
#include <QDoubleSpinBox>
#include <QComboBox.h>
#include <QPushButton>
#include <kcolordialog.h>
#include <kmessagebox.h>
#include <kinputdialog.h>
#include <QStandardPaths>


#include "kstars.h"
#include "kstarsdata.h"
#include "skymap.h"
#include "colorscheme.h"
#include "skyobjects/starobject.h"

static int ItemColorData = Qt::UserRole + 1;

OpsColors::OpsColors( KStars *_ks )
        : QFrame( _ks ), ksw(_ks)
{
    setupUi( this );

    //Populate list of adjustable colors
    for ( unsigned int i=0; i < ksw->data()->colorScheme()->numberOfColors(); ++i ) {
        QPixmap col( 30, 20 );
        QColor itemColor( ksw->data()->colorScheme()->colorAt( i ) );
        col.fill( itemColor );
        QListWidgetItem *item = new QListWidgetItem( ksw->data()->colorScheme()->nameAt( i ), ColorPalette );
        item->setData( Qt::DecorationRole, col );
        item->setData( ItemColorData, itemColor );
    }

    PresetBox->addItem( xi18nc( "use default color scheme", "Default Colors" ) );
    PresetBox->addItem( xi18nc( "use 'star chart' color scheme", "Star Chart" ) );
    PresetBox->addItem( xi18nc( "use 'night vision' color scheme", "Night Vision" ) );
    PresetBox->addItem( xi18nc( "use 'moonless night' color scheme", "Moonless Night" ) );

    PresetFileList.append( "classic.colors" );
    PresetFileList.append( "chart.colors" );
    PresetFileList.append( "night.colors" );
    PresetFileList.append( "moonless-night.colors" );

    QFile file;
    QString line, schemeName, filename;
    file.setFileName( QStandardPaths::locate(QStandardPaths::DataLocation, "colors.dat" ) );
    if ( file.exists() && file.open( QIODevice::ReadOnly ) ) {
        QTextStream stream( &file );

        while ( !stream.atEnd() ) {
            line = stream.readLine();
            schemeName = line.left( line.indexOf( ':' ) );
            filename = line.mid( line.indexOf( ':' ) +1, line.length() );
            PresetBox->addItem( schemeName );
            PresetFileList.append( filename );
        }
        file.close();
    }

    kcfg_StarColorIntensity->setValue( ksw->data()->colorScheme()->starColorIntensity() );
    kcfg_StarColorMode->addItem( xi18nc( "use realistic star colors", "Real Colors" ) );
    kcfg_StarColorMode->addItem( xi18nc( "show stars as red circles", "Solid Red" ) );
    kcfg_StarColorMode->addItem( xi18nc( "show stars as black circles", "Solid Black" ) );
    kcfg_StarColorMode->addItem( xi18nc( "show stars as white circles", "Solid White" ) );
    kcfg_StarColorMode->setCurrentIndex( ksw->data()->colorScheme()->starColorMode() );

    if ( ksw->data()->colorScheme()->starColorMode() != 0 ) //mode is not "Real Colors"
        kcfg_StarColorIntensity->setEnabled( false );
    else
        kcfg_StarColorIntensity->setEnabled( true );

    connect( ColorPalette, SIGNAL( itemClicked( QListWidgetItem* ) ), this, SLOT( newColor( QListWidgetItem* ) ) );
    connect( kcfg_StarColorIntensity, SIGNAL( valueChanged( int ) ), this, SLOT( slotStarColorIntensity( int ) ) );
    connect( kcfg_StarColorMode, SIGNAL( activated( int ) ), this, SLOT( slotStarColorMode( int ) ) );
    connect( PresetBox, SIGNAL( currentRowChanged( int ) ), this, SLOT( slotPreset( int ) ) );
    connect( AddPreset, SIGNAL( clicked() ), this, SLOT( slotAddPreset() ) );
    connect( RemovePreset, SIGNAL( clicked() ), this, SLOT( slotRemovePreset() ) );

    RemovePreset->setEnabled( false );
}

//empty destructor
OpsColors::~OpsColors() {}

void OpsColors::newColor( QListWidgetItem *item ) {
    if ( !item ) return;

    QPixmap pixmap( 30, 20 );
    QColor NewColor;

    int index = ColorPalette->row( item );
    if ( index < 0 || index >= ColorPalette->count() ) return;
    QColor col = item->data( ItemColorData ).value<QColor>();
    if ( KColorDialog::getColor( col ) ) NewColor = col;

    //NewColor will only be valid if the above if statement was found to be true during one of the for loop iterations
    if ( NewColor.isValid() ) {
        pixmap.fill( NewColor );
        item->setData( Qt::DecorationRole, pixmap );
        item->setData( ItemColorData, NewColor );
        ksw->data()->colorScheme()->setColor( ksw->data()->colorScheme()->keyAt( index ), NewColor.name() );
    }

    ksw->map()->forceUpdate();
}

void OpsColors::slotPreset( int index ) {
    QString sPreset = PresetFileList.at( index );
    setColors( sPreset );
}

bool OpsColors::setColors( const QString &filename ) {
    QPixmap temp( 30, 20 );

    //check if colorscheme is removable...
    QFile test;
    test.setFileName( QStandardPaths::writableLocation(QStandardPaths::DataLocation) + QLatin1Char('/') + filename ) ; //try filename in local user KDE directory tree.
    if ( test.exists() ) RemovePreset->setEnabled( true );
    else RemovePreset->setEnabled( false );
    test.close();

		QString actionName = QString("cs_" + filename.left(filename.indexOf(".colors"))).toUtf8();
		QAction *a = ksw->actionCollection()->action( actionName );
		if ( a ) a->setChecked( true );
		kapp->processEvents();

    kcfg_StarColorMode->setCurrentIndex( ksw->data()->colorScheme()->starColorMode() );
    kcfg_StarColorIntensity->setValue( ksw->data()->colorScheme()->starColorIntensity() );

    for ( unsigned int i=0; i < ksw->data()->colorScheme()->numberOfColors(); ++i ) {
        QColor itemColor( ksw->data()->colorScheme()->colorAt( i ) );
        temp.fill( itemColor );
        ColorPalette->item( i )->setData( Qt::DecorationRole, temp );
        ColorPalette->item( i )->setData( ItemColorData, itemColor );
    }

    ksw->map()->forceUpdate();
    return true;
}

void OpsColors::slotAddPreset() {
    bool okPressed = false;
    QString schemename = KInputDialog::getText( xi18n( "New Color Scheme" ),
                         xi18n( "Enter a name for the new color scheme:" ),
                         QString(), &okPressed, 0 );

    if ( okPressed && ! schemename.isEmpty() ) {
        if ( ksw->data()->colorScheme()->save( schemename ) ) {
            QListWidgetItem *item = new QListWidgetItem( schemename, PresetBox );
            QString fname = ksw->data()->colorScheme()->fileName();
            PresetFileList.append( fname );
						QString actionName = QString("cs_" + fname.left(fname.indexOf(".colors"))).toUtf8();
						ksw->addColorMenuItem( schemename, actionName );

						QAction *a = ksw->actionCollection()->action( actionName );
						if ( a ) a->setChecked( true );
            PresetBox->setCurrentItem( item );
        }
    }
}

void OpsColors::slotRemovePreset() {
    QListWidgetItem *current = PresetBox->currentItem();
    if ( !current ) return;
    QString name = current->text();
    QString filename = PresetFileList[ PresetBox->currentRow() ];
    QFile cdatFile;
    cdatFile.setFileName( QStandardPaths::writableLocation(QStandardPaths::DataLocation) + QLatin1Char('/') + "colors.dat" ) ; //determine filename in local user KDE directory tree.

    //Remove action from color-schemes menu
    ksw->removeColorMenuItem( QString("cs_" + filename.left( filename.indexOf(".colors"))).toUtf8() );

    if ( !cdatFile.exists() || !cdatFile.open( QIODevice::ReadWrite ) ) {
        QString message = xi18n( "Local color scheme index file could not be opened.\nScheme cannot be removed." );
        KMessageBox::sorry( 0, message, xi18n( "Could Not Open File" ) );
    } else {
        //Remove entry from the ListBox and from the QStringList holding filenames.
        //There seems to be no way to set no item selected, so select
        // the first item.
        PresetBox->setCurrentRow(0);
        delete current;
        RemovePreset->setEnabled( false );

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
            colorFile.setFileName( QStandardPaths::writableLocation(QStandardPaths::DataLocation) + QLatin1Char('/') + filename ) ; //determine filename in local user KDE directory tree.
            if ( !colorFile.remove() ) {
                QString message = xi18n( "Could not delete the file: %1", colorFile.fileName() );
                KMessageBox::sorry( 0, message, xi18n( "Error Deleting File" ) );
            }

            //remove the old colors.dat file, and rebuild it with the modified string list.
            cdatFile.remove();
            cdatFile.open( QIODevice::ReadWrite );
            QTextStream stream2( &cdatFile );
            for( int i=0; i<slist.count(); ++i )
                stream << slist[i] << endl;
        } else {
            QString message = xi18n( "Could not find an entry named %1 in colors.dat.", name );
            KMessageBox::sorry( 0, message, xi18n( "Scheme Not Found" ) );
        }
        cdatFile.close();
    }
}

void OpsColors::slotStarColorMode( int i ) {
    ksw->data()->colorScheme()->setStarColorMode( i );

    if ( ksw->data()->colorScheme()->starColorMode() != 0 ) //mode is not "Real Colors"
        kcfg_StarColorIntensity->setEnabled( false );
    else
        kcfg_StarColorIntensity->setEnabled( true );
}

void OpsColors::slotStarColorIntensity( int i ) {
    ksw->data()->colorScheme()->setStarColorIntensity( i );
}

#include "opscolors.moc"
