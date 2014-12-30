/***************************************************************************
               starhopperdialog.cpp  -  UI of Star Hopping Guide for KStars
                             -------------------
    begin                : Sat 15th Nov 2014
    copyright            : (C) 2014 Utkarsh Simha
    email                : utkarshsimha@gmail.com
***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "starhopperdialog.h"

StarHopperDialog::StarHopperDialog( QWidget *parent ) : QDialog( parent ), ui( new Ui::StarHopperDialog ) {
    ui->setupUi( this );
    m_lw = ui->listWidget;
    m_Metadata = new QStringList();
    ui->directionsLabel->setWordWrap( true );
    m_sh = new StarHopper();
    connect( ui->NextButton, SIGNAL( clicked() ), this, SLOT( slotNext() ) );
    connect( ui->GotoButton, SIGNAL( clicked() ), this, SLOT( slotGoto() ) );
    connect( ui->DetailsButton, SIGNAL( clicked() ), this, SLOT( slotDetails() ) );
    connect(m_lw, SIGNAL( doubleClicked(const QModelIndex& ) ), this, SLOT( slotGoto() ));
    connect(m_lw, SIGNAL( itemSelectionChanged() ), this, SLOT( slotRefreshMetadata() ));
    connect(this, SIGNAL(finished(int)), this, SLOT(deleteLater()));
}

StarHopperDialog::~StarHopperDialog() {
    TargetListComponent *t = getTargetListComponent();
    t->list->clear();
    SkyMap::Instance()->forceUpdate( true );
    delete m_sh;
}

void StarHopperDialog::starHop( const SkyPoint &startHop, const SkyPoint &stopHop, float fov, float maglim ) {
    QList<StarObject *> *starList = m_sh->computePath( startHop, stopHop, fov, maglim, m_Metadata );
    foreach( StarObject *so, *starList ) {
        setData( so );
    }
    slotRefreshMetadata();
    m_skyObjList = KSUtils::castStarObjListToSkyObjList( starList );
    starList->clear();
    delete starList;
    TargetListComponent *t = getTargetListComponent();
    delete t->list;
    t->list = m_skyObjList;
    SkyMap::Instance()->forceUpdate( true );
}

void StarHopperDialog::setData( StarObject * sobj ) {
    QListWidgetItem *item = new QListWidgetItem();
    QString starName;
    if( sobj->name() != "star" ) {
        starName = sobj->translatedLongName();
    }
    else if( sobj->getHDIndex() ) {
        starName = QString( "HD%1" ).arg( QString::number( sobj->getHDIndex() ) );
    }
    else {
        starName = "";
        starName+= sobj->spchar();
        starName+= QString( " Star of mag %2" ).arg( QString::number( sobj->mag() ) );
    }
    item->setText( starName );
    QVariant qv;
    qv.setValue( sobj );
    item->setData( Qt::UserRole, qv );
    m_lw->addItem( item );
}

void StarHopperDialog::slotNext() {
    m_lw->setCurrentRow( m_lw->currentRow()+1 );
    slotGoto();
}

void StarHopperDialog::slotGoto() {
    slotRefreshMetadata();
    SkyObject *skyobj = getStarData( m_lw->currentItem() );
    if( skyobj ) {
        KStars *ks = KStars::Instance();
        ks->map()->setClickedObject( skyobj );
        ks->map()->setClickedPoint( skyobj );
        ks->map()->slotCenter();
    }
}

void StarHopperDialog::slotDetails() {
    SkyObject *skyobj = getStarData( m_lw->currentItem() );
    if ( skyobj ) {
        DetailDialog *detailDialog = new DetailDialog( skyobj, KStarsData::Instance()->lt(), KStarsData::Instance()->geo(), KStars::Instance());
        detailDialog->exec();
        delete detailDialog;
    }
}

SkyObject * StarHopperDialog::getStarData(QListWidgetItem *item) {
    if( !item )
        return 0;
    else {
        QVariant v = item->data( Qt::UserRole );
        StarObject *starobj = v.value<StarObject *>();
        return starobj;
    }
}

inline TargetListComponent * StarHopperDialog::getTargetListComponent() {
    return KStarsData::Instance()->skyComposite()->getStarHopRouteList();
}

void StarHopperDialog::slotRefreshMetadata() {
    qDebug() << "Slot RefreshMetadata";
    int row = m_lw->currentRow();
    if( row >=0 ) {
        ui->directionsLabel->setText( m_Metadata->at( row ) );
    }
    else {
        ui->directionsLabel->setText( m_Metadata->at( 0 ) );
    }
    qDebug() << "Slot RefreshMetadata";
}
