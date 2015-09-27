/*  Artificial Horizon Manager
    Copyright (C) 2015 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#include "horizonmanager.h"

#include <QStandardItemModel>
#include <QSortFilterProxyModel>
#include <QHeaderView>

#include <KMessageBox>

#include <config-kstars.h>

#include "Options.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "skymap.h"
#include "linelist.h"
#include "skycomponents/artificialhorizoncomponent.h"
#include "skycomponents/skymapcomposite.h"


HorizonManagerUI::HorizonManagerUI( QWidget *p ) : QFrame( p ) {
    setupUi( this );
}


HorizonManager::HorizonManager( QWidget *w )
        : QDialog( w )
{    

    LineListList regionPolygonList;
    QStringList regionNames;

    ui = new HorizonManagerUI( this );

    ui->addRegionB->setIcon(QIcon::fromTheme("list-add"));
    ui->addPointB->setIcon(QIcon::fromTheme("list-add"));
    ui->removeRegionB->setIcon(QIcon::fromTheme("list-remove"));
    ui->removePointB->setIcon(QIcon::fromTheme("list-remove"));
    ui->clearPointsB->setIcon(QIcon::fromTheme("edit-clear"));
    ui->saveB->setIcon(QIcon::fromTheme("document-save"));
    ui->clearPointsB->setIcon(QIcon::fromTheme("draw-path"));
    ui->tipLabel->setPixmap((QIcon::fromTheme("help-hint").pixmap(64,64)));

    setWindowTitle( i18n( "Artificial Horizon Manager" ) );

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(ui);
    setLayout(mainLayout);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Close);
    mainLayout->addWidget(buttonBox);
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

    // Set up List view
    m_RegionsModel = new QStandardItemModel( 0, 3, this );
    m_RegionsModel->setHorizontalHeaderLabels( QStringList() << i18n( "Region") << i18nc( "Azimuth", "Az" ) << i18nc( "Altitude", "Alt" ));

    m_RegionsSortModel = new QSortFilterProxyModel( this );
    m_RegionsSortModel->setSourceModel( m_RegionsModel);
    m_RegionsSortModel->setDynamicSortFilter( true );
    ui->regionsList->setModel( m_RegionsSortModel );
    //ui->regionsList->horizontalHeader()->setStretchLastSection( true );
    //ui->regionsList->horizontalHeader()->setSectionResizeMode( QHeaderView::ResizeToContents );

    //Set up the Table Views
    m_PointsModel = new QStandardItemModel( 0, 2, this );
    m_PointsModel->setHorizontalHeaderLabels( QStringList() << i18nc( "Azimuth", "Az" ) << i18nc( "Altitude", "Alt" ));
    m_PointsSortModel = new QSortFilterProxyModel( this );
    m_PointsSortModel->setSourceModel( m_PointsModel );
    m_PointsSortModel->setDynamicSortFilter( true );

    //ui->pointsList->setModel( m_PointsSortModel );
    ui->pointsList->setModel( m_RegionsModel );
    ui->pointsList->horizontalHeader()->setSectionResizeMode( QHeaderView::Stretch );


    //ui->saveButton->setEnabled( false );

    horizonComponent = KStarsData::Instance()->skyComposite()->artificialHorizon();
    //Fill the list
    regionPolygonList = horizonComponent->polygonsList();
    regionNames       = horizonComponent->regionsList();

    for (int i=0; i< regionPolygonList.size(); ++i )
    {
        QStandardItem * regionItem = new QStandardItem(regionNames[i]);
        m_RegionsModel->appendRow(regionItem);

        SkyList* points = regionPolygonList[i]->points();
        foreach ( SkyPoint* p,  *points)
        {
            QList<QStandardItem*> pointsList;
           pointsList << new QStandardItem( p->az().toDMSString()) << new QStandardItem( p->alt().toDMSString());
            //m_PointsModel->appendRow(pointsList);
           regionItem->appendRow(pointsList);
        }
    }   

    //Connect buttons
    connect( ui->addRegionB, SIGNAL( clicked() ), this, SLOT( slotAddRegion() ) );
    connect( ui->removeRegionB, SIGNAL( clicked() ), this, SLOT( slotRemoveRegion() ) );
    connect( ui->addPointB, SIGNAL(clicked()), this, SLOT(slotAddPoint(SkyPoint*)));
    connect(ui->removePointB, SIGNAL(clicked()), this, SLOT(slotRemovePoint()));

    connect(ui->regionsList, SIGNAL(clicked(QModelIndex)), this, SLOT(slotSetShownRegion(QModelIndex)));

    /*connect( ui->flagList, SIGNAL( clicked(QModelIndex) ), this, SLOT( slotSetShownFlag(QModelIndex) ) );
    connect( ui->flagList, SIGNAL( doubleClicked( const QModelIndex& ) ), this, SLOT( slotCenterFlag() ) );

    connect( ui->saveButton, SIGNAL( clicked() ), this, SLOT( slotSaveChanges() ) );*/
}

HorizonManager::~HorizonManager()
{
}



void HorizonManager::clearFields()
{

    //disable "Save changes" button
   // ui->saveButton->setEnabled( false );

}

void HorizonManager::showRegion( int regionID )
{
    if ( regionID < 0 || regionID >= m_RegionsModel->rowCount() )
        return;
    else
    {
       /*QStandardItem *regionItem = m_RegionsModel->item(regionID);

       for (int i=0; i < regionItem->rowCount(); i++)
       foreach ( SkyPoint* p,  *points)
       {
                pointsList << new QStandardItem( p->az().toDMSString() )
                           << new QStandardItem( p->alt().toDMSString() );

                m_PointsModel->appendRow(pointsList);
       }*/


        /*
        ui->raBox->setText( m_Model->data( m_Model->index( flagIdx, 0) ).toString() );
        ui->decBox->setText( m_Model->data( m_Model->index( flagIdx, 1) ).toString() );
        ui->epochBox->setText( m_Model->data( m_Model->index( flagIdx, 2) ).toString() );

        //ui->flagCombobox->setCurrentItem( m_Model->data( m_Model->index( flagIdx, 3) ).toString() );
        ui->flagCombobox->setCurrentText( m_Model->data( m_Model->index( flagIdx, 3) ).toString() );
        ui->flagLabel->setText( m_Model->data( m_Model->index( flagIdx, 4) ).toString() );

        QColor labelColor = m_Model->item( flagIdx, 4 )->foreground().color();
        ui->labelColorcombo->setColor( labelColor );

        */
    }

    //ui->flagList->selectRow( flagIdx );
    //ui->saveButton->setEnabled( true );
}

bool HorizonManager::validatePoint()
{
/*
    bool raOk(false), decOk(false);
    dms ra( ui->raBox->createDms( false, &raOk ) ); //false means expressed in hours
    dms dec( ui->decBox->createDms( true, &decOk ) );

    QString message;

    //check if ra & dec values were successfully converted
    if ( !raOk || !decOk ) {
        return false;
    }

    //make sure values are in valid range
    if ( ra.Hours() < 0.0 || ra.Hours() > 24.0 )
        message = i18n( "The Right Ascension value must be between 0.0 and 24.0." );
    if ( dec.Degrees() < -90.0 || dec.Degrees() > 90.0 )
        message += '\n' + i18n( "The Declination value must be between -90.0 and 90.0." );
    if ( ! message.isEmpty() ) {
        KMessageBox::sorry( 0, message, i18n( "Invalid Coordinate Data" ) );
        return false;
    }

*/

    //all checks passed
    return true;
}

void HorizonManager::deleteRegion( int regionID )
{
    /*if ( flagIdx < m_Model->rowCount() )
    {
        m_Model->removeRow( flagIdx );
    }*/
}

void HorizonManager::slotAddRegion()
{

    QStandardItem* regionItem = new QStandardItem(i18n("Region"));
    m_RegionsModel->appendRow(regionItem);

    /*
    dms ra( ui->raBox->createDms( false) ); //false means expressed in hours
    dms dec( ui->decBox->createDms( true ) );
    SkyPoint* flagPoint = new SkyPoint( ra, dec );

    insertFlag( true );

    FlagComponent *flags = m_Ks->data()->skyComposite()->flags();
    //Add flag in FlagComponent
    flags->add( flagPoint, ui->epochBox->text(), ui->flagCombobox->currentText(), ui->flagLabel->text(), ui->labelColorcombo->color() );

    ui->flagList->selectRow( m_Model->rowCount() - 1 );
    ui->saveButton->setEnabled( true );

    flags->saveToFile();

    //Redraw map
    m_Ks->map()->forceUpdate(false);
    */
}


void HorizonManager::slotRemoveRegion()
{
/*
    int flag = ui->flagList->currentIndex().row();

    //Remove from FlagComponent
    m_Ks->data()->skyComposite()->flags()->remove( flag );

    //Remove from list
    m_Model->removeRow( flag );

    //Clear form fields
    clearFields();

    //Remove from file
    m_Ks->data()->skyComposite()->flags()->saveToFile();

    //Redraw map
    m_Ks->map()->forceUpdate(false);

    */
}

void HorizonManager::slotSaveChanges()
{
/*    int row = ui->flagList->currentIndex().row();

    validatePoint();

    insertFlag( false, row );

    m_Ks->map()->forceUpdate();

    dms ra( ui->raBox->createDms( false) ); //false means expressed in hours
    dms dec( ui->decBox->createDms( true ) );

    SkyPoint* flagPoint = new SkyPoint( ra, dec );

    //Update FlagComponent
    m_Ks->data()->skyComposite()->flags()->updateFlag( row, flagPoint, ui->epochBox->text(), ui->flagCombobox->currentText(),
                                                       ui->flagLabel->text(), ui->labelColorcombo->color() );

    //Save changes to file
    m_Ks->data()->skyComposite()->flags()->saveToFile();
*/

}

void HorizonManager::slotSetShownRegion( QModelIndex idx )
{
    showRegion ( idx.row() );
}

/*void HorizonManager::insertFlag( bool isNew, int row )
{
    dms ra( ui->raBox->createDms( false) ); //false means expressed in hours
    dms dec( ui->decBox->createDms( true ) );
    SkyPoint *flagPoint = new SkyPoint( ra, dec );

    // Add flag in the list
    QList<QStandardItem*> itemList;

    QStandardItem* labelItem = new QStandardItem( ui->flagLabel->text() );
    labelItem->setForeground( QBrush( ui->labelColorcombo->color() ) );

    FlagComponent *flags = m_Ks->data()->skyComposite()->flags();

    QPixmap pixmap;
    itemList << new QStandardItem( flagPoint->ra0().toHMSString() )
            << new QStandardItem( flagPoint->dec0().toDMSString() )
            << new QStandardItem( ui->epochBox->text() )
            << new QStandardItem( QIcon( pixmap.fromImage( flags->areaPolygonList( ui->flagCombobox->currentIndex() ) ) ),
                                  ui->flagCombobox->currentText() )
            << labelItem;

    if (isNew) {
        m_Model->appendRow(itemList);
    }

    else {
        for( int i = 0; i < m_Model->columnCount(); i++ ) {
            m_Model->setItem( row, i , itemList.at( i ) );
        }
    }
}*/

void HorizonManager::setSkymapPoint(SkyPoint *point)
{
    qDebug() << "Received Az: " << point->az().toDMSString() << " Alt: " << point->alt().toDMSString();
}

void HorizonManager::slotAddPoint(SkyPoint *p)
{

}

void HorizonManager::slotRemovePoint()
{

}
