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

#include <config-kstars.h>

#include <QFile>
#include <QPixmap>
#include <QTextStream>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QPushButton>
#include <QStandardPaths>
#include <QColorDialog>
#include <QInputDialog>

#include <KActionCollection>
#include <KLocalizedString>
#include <KMessageBox>

#include "kspaths.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "skymap.h"
#include "colorscheme.h"

#ifdef HAVE_CFITSIO
#include "fitsviewer/fitsviewer.h"
#include "fitsviewer/fitsview.h"
#endif

#include "skyobjects/starobject.h"

static int ItemColorData = Qt::UserRole + 1;

OpsColors::OpsColors()
        : QFrame(KStars::Instance())
{
    setupUi( this );

    //Populate list of adjustable colors
    for ( unsigned int i=0; i < KStarsData::Instance()->colorScheme()->numberOfColors(); ++i ) {
        QPixmap col( 30, 20 );
        QColor itemColor( KStarsData::Instance()->colorScheme()->colorAt( i ) );
        col.fill( itemColor );
        QListWidgetItem *item = new QListWidgetItem( KStarsData::Instance()->colorScheme()->nameAt( i ), ColorPalette );
        item->setData( Qt::DecorationRole, col );
        item->setData( ItemColorData, itemColor );
    }

    PresetBox->addItem( i18nc( "use default color scheme", "Default Colors" ) );
    PresetBox->addItem( i18nc( "use 'star chart' color scheme", "Star Chart" ) );
    PresetBox->addItem( i18nc( "use 'night vision' color scheme", "Night Vision" ) );
    PresetBox->addItem( i18nc( "use 'moonless night' color scheme", "Moonless Night" ) );

    PresetFileList.append( "classic.colors" );
    PresetFileList.append( "chart.colors" );
    PresetFileList.append( "night.colors" );
    PresetFileList.append( "moonless-night.colors" );

    QFile file;
    QString line, schemeName, filename;
    file.setFileName( KSPaths::locate(QStandardPaths::GenericDataLocation, "colors.dat" ) );
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

    kcfg_StarColorIntensity->setValue( KStarsData::Instance()->colorScheme()->starColorIntensity() );
    kcfg_StarColorMode->addItem( i18nc( "use realistic star colors", "Real Colors" ) );
    kcfg_StarColorMode->addItem( i18nc( "show stars as red circles", "Solid Red" ) );
    kcfg_StarColorMode->addItem( i18nc( "show stars as black circles", "Solid Black" ) );
    kcfg_StarColorMode->addItem( i18nc( "show stars as white circles", "Solid White" ) );
    kcfg_StarColorMode->addItem( i18nc( "show stars as colored circles", "Solid Colors" ) );
    kcfg_StarColorMode->setCurrentIndex( KStarsData::Instance()->colorScheme()->starColorMode() );

    if ( KStarsData::Instance()->colorScheme()->starColorMode() != 0 ) //mode is not "Real Colors"
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
    NewColor = QColorDialog::getColor( col );

    //NewColor will only be valid if the above if statement was found to be true during one of the for loop iterations
    if ( NewColor.isValid() ) {
        pixmap.fill( NewColor );
        item->setData( Qt::DecorationRole, pixmap );
        item->setData( ItemColorData, NewColor );
        KStarsData::Instance()->colorScheme()->setColor( KStarsData::Instance()->colorScheme()->keyAt( index ), NewColor.name() );
    }

    KStars::Instance()->map()->forceUpdate();

    #ifdef HAVE_CFITSIO
    QList<FITSViewer *> viewers = KStars::Instance()->findChildren<FITSViewer *>();
    foreach(FITSViewer *viewer, viewers){
        viewer->getCurrentView()->updateFrame();
    }
    #endif
}

void OpsColors::slotPreset( int index ) {
    QString sPreset = PresetFileList.at( index );
    setColors( sPreset );
}

bool OpsColors::setColors( const QString &filename ) {
    QPixmap temp( 30, 20 );

    //check if colorscheme is removable...
    QFile test;
    test.setFileName( KSPaths::writableLocation(QStandardPaths::GenericDataLocation) + filename ) ; //try filename in local user KDE directory tree.
    if ( test.exists() ) RemovePreset->setEnabled( true );
    else RemovePreset->setEnabled( false );
    test.close();

		QString actionName = QString("cs_" + filename.left(filename.indexOf(".colors"))).toUtf8();
        QAction *a = KStars::Instance()->actionCollection()->action( actionName );
		if ( a ) a->setChecked( true );
        qApp->processEvents();

    kcfg_StarColorMode->setCurrentIndex( KStarsData::Instance()->colorScheme()->starColorMode() );
    kcfg_StarColorIntensity->setValue( KStarsData::Instance()->colorScheme()->starColorIntensity() );

    for ( unsigned int i=0; i < KStarsData::Instance()->colorScheme()->numberOfColors(); ++i ) {
        QColor itemColor( KStarsData::Instance()->colorScheme()->colorAt( i ) );
        temp.fill( itemColor );
        ColorPalette->item( i )->setData( Qt::DecorationRole, temp );
        ColorPalette->item( i )->setData( ItemColorData, itemColor );
    }

    KStars::Instance()->map()->forceUpdate();
    #ifdef HAVE_CFITSIO
    QList<FITSViewer *> viewers = KStars::Instance()->findChildren<FITSViewer *>();
    foreach(FITSViewer *viewer, viewers){
        viewer->getCurrentView()->updateFrame();
    #endif
    }
    return true;
}

void OpsColors::slotAddPreset() {


    bool okPressed = false;
    QString schemename = QInputDialog::getText(0, i18n( "New Color Scheme" ),
                         i18n( "Enter a name for the new color scheme:" ), QLineEdit::Normal,
                         QString(), &okPressed, 0 );

    if ( okPressed && ! schemename.isEmpty() ) {
        if ( KStarsData::Instance()->colorScheme()->save( schemename ) ) {
            QListWidgetItem *item = new QListWidgetItem( schemename, PresetBox );
            QString fname = KStarsData::Instance()->colorScheme()->fileName();
            PresetFileList.append( fname );
						QString actionName = QString("cs_" + fname.left(fname.indexOf(".colors"))).toUtf8();
                        KStars::Instance()->addColorMenuItem( schemename, actionName );

                        QAction *a = KStars::Instance()->actionCollection()->action( actionName );
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
    cdatFile.setFileName( KSPaths::writableLocation(QStandardPaths::GenericDataLocation) + "colors.dat" ) ; //determine filename in local user KDE directory tree.

    //Remove action from color-schemes menu
    KStars::Instance()->removeColorMenuItem( QString("cs_" + filename.left( filename.indexOf(".colors"))).toUtf8() );

    if ( !cdatFile.exists() || !cdatFile.open( QIODevice::ReadWrite ) ) {
        QString message = i18n( "Local color scheme index file could not be opened.\nScheme cannot be removed." );
        KMessageBox::sorry( 0, message, i18n( "Could Not Open File" ) );
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
            colorFile.setFileName( KSPaths::writableLocation(QStandardPaths::GenericDataLocation) + filename ) ; //determine filename in local user KDE directory tree.
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
    KStarsData::Instance()->colorScheme()->setStarColorMode( i );

    if ( KStarsData::Instance()->colorScheme()->starColorMode() != 0 ) //mode is not "Real Colors"
        kcfg_StarColorIntensity->setEnabled( false );
    else
        kcfg_StarColorIntensity->setEnabled( true );
}

void OpsColors::slotStarColorIntensity( int i ) {
    KStarsData::Instance()->colorScheme()->setStarColorIntensity( i );
}


