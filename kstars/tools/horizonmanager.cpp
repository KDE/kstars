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
#include "projections/projector.h"
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

    ui->setStyleSheet("QPushButton:checked { background-color: red; }");

    ui->addRegionB->setIcon(QIcon::fromTheme("list-add"));
    ui->addPointB->setIcon(QIcon::fromTheme("list-add"));
    ui->removeRegionB->setIcon(QIcon::fromTheme("list-remove"));
    ui->removePointB->setIcon(QIcon::fromTheme("list-remove"));
    ui->clearPointsB->setIcon(QIcon::fromTheme("edit-clear"));
    ui->saveB->setIcon(QIcon::fromTheme("document-save"));
    ui->selectPointsB->setIcon(QIcon::fromTheme("snap-orthogonal"));

    ui->tipLabel->setPixmap((QIcon::fromTheme("help-hint").pixmap(64,64)));

    ui->polygonValidatoin->setPixmap(QIcon::fromTheme("dialog-ok").pixmap(32,32));
    ui->polygonValidatoin->setToolTip(i18n("Region is invalid. The polygon must be closed"));

    setWindowTitle( i18n( "Artificial Horizon Manager" ) );

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(ui);
    setLayout(mainLayout);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Close);
    mainLayout->addWidget(buttonBox);
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

    selectPoints = false;

    // Set up List view
    m_RegionsModel = new QStandardItemModel( 0, 3, this );
    m_RegionsModel->setHorizontalHeaderLabels( QStringList() << i18n( "Region") << i18nc( "Azimuth", "Az" ) << i18nc( "Altitude", "Alt" ));

    m_RegionsSortModel = new QSortFilterProxyModel( this );
    m_RegionsSortModel->setSourceModel( m_RegionsModel);
    m_RegionsSortModel->setDynamicSortFilter( true );

    ui->regionsList->setModel( m_RegionsSortModel );

    ui->pointsList->setModel( m_RegionsModel );
    ui->pointsList->horizontalHeader()->setSectionResizeMode( QHeaderView::Stretch );
    ui->pointsList->setColumnHidden(0, true);

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
           pointsList << new QStandardItem("") << new QStandardItem( p->az().toDMSString()) << new QStandardItem( p->alt().toDMSString());
            //m_PointsModel->appendRow(pointsList);
           regionItem->appendRow(pointsList);
        }
    }   

    ui->removeRegionB->setEnabled(true);

    //Connect buttons
    connect( ui->addRegionB, SIGNAL( clicked() ), this, SLOT( slotAddRegion() ) );
    connect( ui->removeRegionB, SIGNAL( clicked() ), this, SLOT( slotRemoveRegion() ) );
    //connect( ui->addPointB, SIGNAL(clicked()), this, SLOT(slotAddPoint(SkyPoint*)));
    connect(ui->removeRegionB, SIGNAL(clicked()), this, SLOT(slotRemovePoint()));

    connect(ui->regionsList, SIGNAL(clicked(QModelIndex)), this, SLOT(slotSetShownRegion(QModelIndex)));

    connect(ui->addPointB, SIGNAL(clicked()), this, SLOT(slotAddPoint()));
    connect(ui->removePointB, SIGNAL(clicked()), this, SLOT(slotRemovePoint()));
    connect(ui->clearPointsB, SIGNAL(clicked()), this, SLOT(clearPoints()));
    connect(ui->selectPointsB, SIGNAL(clicked(bool)), this, SLOT(setSelectPoints(bool)));
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
       ui->pointsList->setRootIndex(m_RegionsModel->index(regionID, 0));
       ui->pointsList->setColumnHidden(0, true);

       ui->addPointB->setEnabled(true);
       ui->removePointB->setEnabled(true);
       ui->selectPointsB->setEnabled(true);
       ui->clearPointsB->setEnabled(true);
    }

    ui->saveB->setEnabled( true );
}

bool HorizonManager::validatePolygon()
{
    QStandardItem *regionItem = m_RegionsModel->item(ui->regionsList->currentIndex().row(), 0);

    const Projector *proj = SkyMap::Instance()->projector();

    if (regionItem == NULL)
        return false;

    QPolygonF poly;
    dms az, alt;
    for (int i=0; i < regionItem->rowCount(); i++)
    {
        QVariant azData  = regionItem->child(i, 1)->data();
        QVariant altData = regionItem->child(i, 2)->data();

        if (azData.isValid() == false || altData.isValid() == false)
            return false;

        az.setFromString( azData.toString());
        alt.setFromString( altData.toString());
        SkyPoint p;
        p.setAz(az);
        p.setAlt(alt);
        p.HorizontalToEquatorial(KStarsData::Instance()->lst(), KStarsData::Instance()->geo()->lat());

        QPointF point = proj->toScreen(&p);
        poly << point;
    }

    return poly.isClosed();
}

void HorizonManager::slotAddRegion()
{

    QStandardItem* regionItem = new QStandardItem(i18n("Region"));
    m_RegionsModel->appendRow(regionItem);

    QModelIndex index = regionItem->index();
    ui->regionsList->setCurrentIndex(index);
    ui->regionsList->selectionModel()->setCurrentIndex(index, QItemSelectionModel::ClearAndSelect);


    //ui->regionsList->selectR
    //slotAddPoint();

    ui->pointsList->setColumnHidden(0, true);

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

    int regionID = ui->regionsList->currentIndex().row();

    //Remove from HorizonComponent
    //m_Ks->data()->skyComposite()->flags()->remove( flag );

    //Remove from list
    m_RegionsModel->removeRow( regionID );

    //Clear form fields
    //clearFields();

    //Remove from file
    //m_Ks->data()->skyComposite()->flags()->saveToFile();

    //Redraw map
    //m_Ks->map()->forceUpdate(false);

}

void HorizonManager::deleteRegion( int regionID )
{
    if ( regionID < m_RegionsModel->rowCount() )
    {
        m_RegionsModel->removeRow( regionID );
    }
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

void HorizonManager::addSkyPoint(SkyPoint *point)
{
   if (selectPoints == false)
       return;

    qDebug() << "Received Az: " << point->az().toDMSString() << " Alt: " << point->alt().toDMSString();

    QStandardItem *regionItem = m_RegionsModel->item(ui->regionsList->currentIndex().row(), 0);

    if (regionItem)
    {
        QList<QStandardItem*> pointsList;
        pointsList << new QStandardItem("") << new QStandardItem(point->az().toDMSString()) << new QStandardItem(point->alt().Degrees() < 0 ? "0" : point->alt().toDMSString());
        regionItem->appendRow(pointsList);

        if (validatePolygon())
        {
            ui->polygonValidatoin->setEnabled(true);
            ui->polygonValidatoin->setToolTip(i18n("Region is valid"));
        }
        else
        {
            ui->polygonValidatoin->setEnabled(false);
            ui->polygonValidatoin->setToolTip(i18n("Region is invalid. The polygon must be closed"));
        }
    }
}

void HorizonManager::slotAddPoint()
{
    QStandardItem *regionItem = m_RegionsModel->item(ui->regionsList->currentIndex().row(), 0);

    if (regionItem)
    {
        QList<QStandardItem*> pointsList;
        pointsList << new QStandardItem("") << new QStandardItem("") << new QStandardItem("");
        regionItem->appendRow(pointsList);

        //ui->pointsList->setRootIndex(ui->regionsList->currentIndex());
        //ui->pointsList->setColumnHidden(0, true);
    }

}

void HorizonManager::slotRemovePoint()
{
    QStandardItem *regionItem = m_RegionsModel->item(ui->regionsList->currentIndex().row(), 0);

    if (regionItem)
    {
        int row = ui->pointsList->currentIndex().row();
        regionItem->removeRow(row);
    }
}

void HorizonManager::clearPoints()
{
    QStandardItem *regionItem = m_RegionsModel->item(ui->regionsList->currentIndex().row(), 0);

    if (regionItem)
        regionItem->removeRows(0, regionItem->rowCount());
}

void HorizonManager::setSelectPoints(bool enable)
{
    selectPoints = enable;
    ui->selectPointsB->clearFocus();

}
