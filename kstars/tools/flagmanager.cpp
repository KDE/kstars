/***************************************************************************
                          flagmanager.cpp  -  Flags manager
                             -------------------
    begin                : Mon Feb 01 2009
    copyright            : (C) 2009 by Jerome SONRIER
    email                : jsid@emor3j.fr.eu.org
 ***************************************************************************/
/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "flagmanager.h"

#include <QStandardItemModel>
#include <QSortFilterProxyModel>
#include <QHeaderView>

#include <kmessagebox.h>
#include <kstandarddirs.h>

#include "kstars.h"
#include "kstarsdata.h"
#include "skymap.h"


FlagManagerUI::FlagManagerUI( QWidget *p ) : QFrame( p ) {
    setupUi( this );
}


FlagManager::FlagManager( KStars *ks )
        : KDialog( (QWidget*)ks )
{
    QList<QStandardItem*> itemList;
    QList<QImage> imageList;
    QStringList flagNames;
    QPixmap *pixmap = new QPixmap();
    int i;

    ui = new FlagManagerUI( this );
    setMainWidget( ui );
    setCaption( i18n( "Flag manager" ) );
    setButtons( KDialog::Close );

    m_Ks = ks;

    //Set up the Table Views
    m_Model = new QStandardItemModel( 0, 5, this );
    m_Model->setHorizontalHeaderLabels( QStringList() << i18nc( "Right Ascension", "RA" ) 
            << i18nc( "Declination", "Dec" ) << i18n( "Epoch" ) 
            << i18n( "Icon" ) << i18n( "Label" ) );
    m_SortModel = new QSortFilterProxyModel( this );
    m_SortModel->setSourceModel( m_Model );
    m_SortModel->setDynamicSortFilter( true );
    ui->flagList->setModel( m_SortModel );
    ui->flagList->horizontalHeader()->setStretchLastSection( true );
    ui->flagList->horizontalHeader()->setResizeMode( QHeaderView::ResizeToContents );

    // Fill the list
    imageList = ks->data()->skyComposite()->flags()->imageList();
    flagNames =  ks->data()->skyComposite()->flags()->getNames();
    for ( i=0; i<ks->data()->skyComposite()->flags()->size(); ++i ) {
        itemList << new QStandardItem( ks->data()->skyComposite()->flags()->pointList().at( i )->ra0()->toHMSString() ) 
                << new QStandardItem( ks->data()->skyComposite()->flags()->pointList().at( i )->dec0()->toDMSString() ) 
                << new QStandardItem( ks->data()->skyComposite()->flags()->epoch( i ) ) 
                << new QStandardItem( QIcon( pixmap->fromImage( ks->data()->skyComposite()->flags()->image( i ) ) ), "" ) 
                << new QStandardItem( ks->data()->skyComposite()->flags()->label( i ) );
        m_Model->appendRow( itemList );
        itemList.clear();
    }

    // Fill the combobox
    for ( i=0; i< imageList.size(); ++i ) {
        ui->flagCombobox->addItem( QIcon( pixmap->fromImage( ks->data()->skyComposite()->flags()->imageList( i ) ) ),
                                   flagNames.at( i ),
                                   flagNames.at( i ) );
    }

    // Connect "Add" and "Delete" buttons
    connect( ui->addButton, SIGNAL( clicked() ), this, SLOT( slotValidatePoint() ) );
    connect( ui->delButton, SIGNAL( clicked() ), this, SLOT( slotDeleteFlag() ) );
}

FlagManager::~FlagManager()
{
}

void FlagManager::slotValidatePoint() {
    bool raOk(false), decOk(false);
    dms ra( ui->raBox->createDms( false, &raOk ) ); //false means expressed in hours
    dms dec( ui->decBox->createDms( true, &decOk ) );
    QString message, str;
    QByteArray line;
    SkyPoint* flagPoint;
    QList<QStandardItem*> itemList;
    QPixmap *pixmap;

    if ( raOk && decOk ) {
        //make sure values are in valid range
        if ( ra.Hours() < 0.0 || ra.Hours() > 24.0 )
            message = i18n( "The Right Ascension value must be between 0.0 and 24.0." );
        if ( dec.Degrees() < -90.0 || dec.Degrees() > 90.0 )
            message += '\n' + i18n( "The Declination value must be between -90.0 and 90.0." );
        if ( ! message.isEmpty() ) {
            KMessageBox::sorry( 0, message, i18n( "Invalid Coordinate Data" ) );
            return;
        }

        flagPoint = new SkyPoint( ra, dec );
        line.append(
                    str.setNum( flagPoint->ra0()->Degrees() ).toAscii() + ' '
                + str.setNum( flagPoint->dec0()->Degrees() ).toAscii() + ' '
                + ui->epochBox->text().toAscii() + ' '
                + ui->flagCombobox->currentText().replace( ' ', '_' ).toAscii() + ' '
                + ui->flagLabel->text().toAscii() + '\n' );

        QFile file( KStandardDirs::locateLocal( "appdata", "flags.dat" ) );
        file.open( QIODevice::Append | QIODevice::Text );
        file.write( line );
        file.close();

        // Add flag in FlagComponent
        m_Ks->data()->skyComposite()->flags()->add( flagPoint, ui->epochBox->text(), ui->flagCombobox->currentText(), ui->flagLabel->text() );

        // Add flag in the list 
        pixmap = new QPixmap();

        itemList << new QStandardItem( flagPoint->ra0()->toHMSString() ) 
                << new QStandardItem( flagPoint->dec0()->toDMSString() ) 
                << new QStandardItem( ui->epochBox->text() ) 
                << new QStandardItem( QIcon( pixmap->fromImage( m_Ks->data()->skyComposite()->flags()->image( m_Ks->data()->skyComposite()->flags()->size()-1 ) ) ), "" )
                << new QStandardItem( ui->flagLabel->text() );
        m_Model->appendRow( itemList );

        // Redraw map
        m_Ks->map()->forceUpdate(false);
    }
}

void FlagManager::slotDeleteFlag() {
    int flag = ui->flagList->currentIndex().row();
    int i;
    QString str;
    QByteArray line;

    //Remove from FlagComponent
    m_Ks->data()->skyComposite()->flags()->remove( flag );

    //Remove from file
    QFile file( KStandardDirs::locateLocal( "appdata", "flags.dat.tmp" ) );
    file.open( QIODevice::WriteOnly | QIODevice::Text );

    for ( i=0; i<m_Ks->data()->skyComposite()->flags()->size(); ++i ) {
        line.append(
                    str.setNum( m_Ks->data()->skyComposite()->flags()->pointList().at( i )->ra0()->Degrees() ).toAscii() + ' '
                + str.setNum( m_Ks->data()->skyComposite()->flags()->pointList().at( i )->dec0()->Degrees() ).toAscii() + ' '
                + m_Ks->data()->skyComposite()->flags()->epoch( i ).toAscii() + ' '
                + m_Ks->data()->skyComposite()->flags()->imageName( i ).replace( ' ', '_' ).toAscii() + ' '
                + m_Ks->data()->skyComposite()->flags()->label( i ).toAscii() + '\n' );

        file.write( line );
        line.clear();
    }

    QFile::remove( KStandardDirs::locateLocal( "appdata", "flags.dat" ) );
    file.rename( KStandardDirs::locateLocal( "appdata", "flags.dat" ) );
    file.close();

    //Remove from list
    m_Model->removeRow( flag );

    // Redraw map
    m_Ks->map()->forceUpdate(false);
}

#include "flagmanager.moc"
