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

#include "Options.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "skymap.h"
#include "skycomponents/flagcomponent.h"
#include "skycomponents/skymapcomposite.h"


FlagManagerUI::FlagManagerUI( QWidget *p ) : QFrame( p ) {
    setupUi( this );
}


FlagManager::FlagManager( QWidget *ks )
        : KDialog( ks )
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

    m_Ks = KStars::Instance();

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
    imageList = m_Ks->data()->skyComposite()->flags()->imageList();
    flagNames =  m_Ks->data()->skyComposite()->flags()->getNames();

    for ( i=0; i<m_Ks->data()->skyComposite()->flags()->size(); ++i ) {
        QStandardItem* labelItem = new QStandardItem( m_Ks->data()->skyComposite()->flags()->label( i ) );
        labelItem->setForeground( QBrush( m_Ks->data()->skyComposite()->flags()->labelColor( i ) ) );

        itemList << new QStandardItem( m_Ks->data()->skyComposite()->flags()->pointList().at( i )->ra0().toHMSString() ) 
                << new QStandardItem( m_Ks->data()->skyComposite()->flags()->pointList().at( i )->dec0().toDMSString() ) 
                << new QStandardItem( m_Ks->data()->skyComposite()->flags()->epoch( i ) ) 
                << new QStandardItem( QIcon( pixmap->fromImage( m_Ks->data()->skyComposite()->flags()->image( i ) ) ), "" )
                << labelItem;
        m_Model->appendRow( itemList );
        itemList.clear();
    }

    // Fill the combobox
    for ( i=0; i< imageList.size(); ++i ) {
        ui->flagCombobox->addItem( QIcon( pixmap->fromImage( m_Ks->data()->skyComposite()->flags()->imageList( i ) ) ),
                                   flagNames.at( i ),
                                   flagNames.at( i ) );
    }

    // Connect buttons 
    connect( ui->addButton, SIGNAL( clicked() ), this, SLOT( slotValidatePoint() ) );
    connect( ui->delButton, SIGNAL( clicked() ), this, SLOT( slotDeleteFlag() ) );
    connect( ui->CenterButton, SIGNAL( clicked() ), this, SLOT( slotCenterFlag() ) );
    connect( ui->flagList, SIGNAL( clicked(QModelIndex) ), this, SLOT(setShownFlag(QModelIndex) ) );
    connect( ui->flagList, SIGNAL( doubleClicked( const QModelIndex& ) ), this, SLOT( slotCenterFlag() ) );
}

FlagManager::~FlagManager()
{
}

void FlagManager::setRaDec( const dms &ra, const dms &dec )
{
    ui->raBox->show( ra, false );
    ui->decBox->show( dec, true );
}

void FlagManager::showFlag( int flagIdx )
{
    if ( flagIdx < 0 || flagIdx >= m_Model->rowCount() ) {
        return;
    }

    else {
        ui->raBox->setText( m_Model->data( m_Model->index( flagIdx, 0) ).toString() );
        ui->decBox->setText( m_Model->data( m_Model->index( flagIdx, 1) ).toString() );
        ui->epochBox->setText( m_Model->data( m_Model->index( flagIdx, 2) ).toString() );

        ui->flagCombobox->setCurrentItem( m_Model->data( m_Model->index( flagIdx, 3) ).toString() );
        ui->flagLabel->setText( m_Model->data( m_Model->index( flagIdx, 4) ).toString() );

        QColor labelColor = m_Model->item( flagIdx, 4 )->foreground().color();
        ui->labelColorcombo->setColor( labelColor );
    }
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
                    str.setNum( flagPoint->ra0().Degrees() ).toAscii() + ' '
                + str.setNum( flagPoint->dec0().Degrees() ).toAscii() + ' '
                + ui->epochBox->text().toAscii() + ' '
                + ui->flagCombobox->currentText().replace( ' ', '_' ).toAscii() + ' '
                + ui->flagLabel->text().toAscii() + ' '
                + ui->labelColorcombo->color().name().toAscii() + '\n' );

        QFile file( KStandardDirs::locateLocal( "appdata", "flags.dat" ) );
        file.open( QIODevice::Append | QIODevice::Text );
        file.write( line );
        file.close();

        // Add flag in FlagComponent
        m_Ks->data()->skyComposite()->flags()->add( flagPoint, ui->epochBox->text(), ui->flagCombobox->currentText(), ui->flagLabel->text(), ui->labelColorcombo->color() );

        // Add flag in the list
        pixmap = new QPixmap();

        QStandardItem* labelItem = new QStandardItem( ui->flagLabel->text() );
        labelItem->setForeground( QBrush( ui->labelColorcombo->color() ) );

        itemList << new QStandardItem( flagPoint->ra0().toHMSString() )
                << new QStandardItem( flagPoint->dec0().toDMSString() )
                << new QStandardItem( ui->epochBox->text() )
                << new QStandardItem( QIcon( pixmap->fromImage( m_Ks->data()->skyComposite()->flags()->image( m_Ks->data()->skyComposite()->flags()->size()-1 ) ) ),
                                      ui->flagCombobox->currentText() )
                << labelItem;
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
                    str.setNum( m_Ks->data()->skyComposite()->flags()->pointList().at( i )->ra0().Degrees() ).toAscii() + ' '
                + str.setNum( m_Ks->data()->skyComposite()->flags()->pointList().at( i )->dec0().Degrees() ).toAscii() + ' '
                + m_Ks->data()->skyComposite()->flags()->epoch( i ).toAscii() + ' '
                + m_Ks->data()->skyComposite()->flags()->imageName( i ).replace( ' ', '_' ).toAscii() + ' '
                + m_Ks->data()->skyComposite()->flags()->label( i ).toAscii() + ' '
                + m_Ks->data()->skyComposite()->flags()->labelColor( i ).name().toAscii() + '\n' );

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

void FlagManager::slotCenterFlag() {
    if ( ui->flagList->currentIndex().isValid() ) {
        m_Ks->map()->setClickedObject( 0 );
        m_Ks->map()->setClickedPoint( m_Ks->data()->skyComposite()->flags()->pointList().at( ui->flagList->currentIndex().row() ) );
        m_Ks->map()->slotCenter();
    }
}

void FlagManager::setShownFlag( QModelIndex idx ) {
    showFlag ( idx.row() );
}

#include "flagmanager.moc"
