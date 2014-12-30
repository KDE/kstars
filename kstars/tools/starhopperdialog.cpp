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
    lw = ui->listWidget;
    sh = new StarHopper();
    connect( ui->NextButton, SIGNAL( clicked() ), this, SLOT( slotNext() ) );
    connect( ui->GotoButton, SIGNAL( clicked() ), this, SLOT( slotGoto() ) );
    connect( ui->DetailsButton, SIGNAL( clicked() ), this, SLOT( slotDetails() ) );
    connect(m_lw, SIGNAL( doubleClicked(const QModelIndex& ) ), this, SLOT( slotGoto() ));
    connect(this, SIGNAL(finished(int)), this, SLOT(deleteLater()));
}

StarHopperDialog::~StarHopperDialog() {
    TargetListComponent *t = getTargetListComponent();
    t->list->clear();
    SkyMap::Instance()->forceUpdate( true );
    delete sh;
}

void StarHopperDialog::starHop( const SkyPoint &startHop, const SkyPoint &stopHop, float fov, float maglim ) {
    QList<StarObject *> *starList = sh->computePath( startHop, stopHop, fov, maglim );
    foreach( StarObject *so, *starList ) {
        setData( so );
    }
    m_skyObjList = KSUtils::castStarObjListToSkyObjList( starList );
    starList->clear();
    delete starList;
    TargetListComponent *t = getTargetListComponent();
    delete t->list;
    t->list = m_skyObjList;
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
    lw->addItem( item );
}

void StarHopperDialog::slotNext() {
    lw->setCurrentRow( lw->currentRow()+1 );
}

void StarHopperDialog::slotGoto() {
    SkyObject *skyobj = getStarData( lw->currentItem() );
    if( skyobj ) {
        KStars *ks = KStars::Instance();
        ks->map()->setClickedObject( skyobj );
        ks->map()->setClickedPoint( skyobj );
        ks->map()->slotCenter();
    }
}

void StarHopperDialog::slotDetails() {
    SkyObject *skyobj = getStarData( lw->currentItem() );
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
