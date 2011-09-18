/***************************************************************************
                          equipmentwriter.cpp  -  description

                             -------------------
    begin                : Friday July 19, 2009
    copyright            : (C) 2009 by Prakash Mohan
    email                : prakash.mohan@kdemail.net
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "oal/equipmentwriter.h"
#include "ui_equipmentwriter.h"

#include <QFile>

#include "kstarsdata.h"
#include "oal/oal.h"
#include "oal/scope.h"
#include "oal/eyepiece.h"
#include "oal/lens.h"
#include "oal/filter.h"

#include <config-kstars.h>

#ifdef HAVE_INDI_H
#include "indi/indidriver.h"
#endif

EquipmentWriter::EquipmentWriter() {
    QWidget *widget = new QWidget;
    ui.setupUi( widget );
    ui.tabWidget->setCurrentIndex(0);
    setMainWidget( widget );
    setCaption( i18n( "Define Equipment" ) );
    setButtons( KDialog::Close );
    setupFilterTab();

    ks = KStars::Instance();
    nextScope = 0;
    nextEyepiece = 0;
    nextFilter = 0;
    nextLens = 0;

    loadEquipment();

    #ifdef HAVE_INDI_H
    ui.driverComboBox->insertItems(1, KStars::Instance()->indiDriver()->driversList.keys());
    #endif

    //make connections
    connect( this, SIGNAL( closeClicked() ), this, SLOT( slotClose() ) );
    connect( ui.NewScope, SIGNAL( clicked() ), this, SLOT( slotAddScope() ) );
    connect( ui.NewEyepiece, SIGNAL( clicked() ), this, SLOT( slotAddEyepiece() ) );
    connect( ui.NewLens, SIGNAL( clicked() ), this, SLOT( slotAddLens() ) );
    connect( ui.NewFilter, SIGNAL( clicked() ), this, SLOT( slotAddFilter() ) );
    connect( ui.SaveScope, SIGNAL( clicked() ), this, SLOT( slotSaveScope() ) );
    connect( ui.SaveEyepiece, SIGNAL( clicked() ), this, SLOT( slotSaveEyepiece() ) );
    connect( ui.SaveLens, SIGNAL( clicked() ), this, SLOT( slotSaveLens() ) );
    connect( ui.SaveFilter, SIGNAL( clicked() ), this, SLOT( slotSaveFilter() ) );
    connect( ui.ScopeList, SIGNAL( currentTextChanged(const QString) ),
             this, SLOT( slotSetScope(QString) ) );
    connect( ui.EyepieceList, SIGNAL( currentTextChanged(const QString) ),
             this, SLOT( slotSetEyepiece(QString) ) );
    connect( ui.LensList, SIGNAL( currentTextChanged(const QString) ),
             this, SLOT( slotSetLens(QString) ) );
    connect( ui.FilterList, SIGNAL( currentTextChanged(const QString) ),
             this, SLOT( slotSetFilter(QString) ) );
    connect( ui.RemoveScope, SIGNAL( clicked() ), this, SLOT( slotRemoveScope() ) );
    connect( ui.RemoveEyepiece, SIGNAL( clicked() ), this, SLOT( slotRemoveEyepiece() ) );
    connect( ui.RemoveLens, SIGNAL( clicked() ), this, SLOT( slotRemoveLens() ) );
    connect( ui.RemoveFilter, SIGNAL( clicked() ), this, SLOT( slotRemoveFilter() ) );
}

void EquipmentWriter::slotAddScope() {
    while ( ks->data()->logObject()->findScopeById( "telescope" + '_' + QString::number( nextScope ) ) )
        nextScope++;
    OAL::Scope *s = new OAL::Scope( "telescope" + '_' + QString::number( nextScope++ ), ui.Model->text(),
                                    ui.Vendor->text(), ui.Type->currentText(), ui.FocalLength->value(), ui.Aperture->value() );
    ks->data()->logObject()->scopeList()->append( s );
    s->setINDIDriver( ui.driverComboBox->currentText() );

    ui.Model->clear();
    ui.Vendor->clear();
    ui.FocalLength->setValue(0);
    ui.driverComboBox->setCurrentIndex(0);

    saveEquipment(); //Save the new list.
}

void EquipmentWriter::slotRemoveScope() {
    OAL::Scope *s = ks->data()->logObject()->findScopeById( ui.Id->text() );
    int idx = ks->data()->logObject()->scopeList()->indexOf( s );
    if ( idx < 0 )
        return;
    ks->data()->logObject()->scopeList()->removeAt( idx );
    if( s )
        delete s;

    ui.Id->clear();
    ui.Model->clear();
    ui.Vendor->clear();
    ui.FocalLength->setValue( 0 );

    QListWidgetItem *item = ui.ScopeList->takeItem( ui.ScopeList->currentRow() );
    if( item )
        delete item;

    saveEquipment(); //Save the new list.
}

void EquipmentWriter::slotSaveScope() {
    OAL::Scope *s = ks->data()->logObject()->findScopeById( ui.Id->text() );
    if( s ) {
        s->setScope( ui.Id->text(), ui.Model->text(), ui.Vendor->text(), ui.Type->currentText(), ui.FocalLength->value(), ui.Aperture->value() );
        s->setINDIDriver(ui.driverComboBox->currentText());
    }
    saveEquipment(); //Save the new list.
}
 
void EquipmentWriter::slotSetScope( QString name) {
    OAL::Scope *s = ks->data()->logObject()->findScopeByName( name );
    if ( s ) {
        ui.Id->setText( s->id() ) ;
        ui.Model->setText( s->model() );
        ui.Vendor->setText( s->vendor() );
        ui.Type->setCurrentIndex( ui.Type->findText( s->type() ) );
        ui.FocalLength->setValue( s->focalLength() );
        ui.Aperture->setValue( s->aperture() );
        ui.driverComboBox->setCurrentIndex(ui.driverComboBox->findText(s->driver()));
    }
}

void EquipmentWriter::slotAddEyepiece() {
    while ( ks->data()->logObject()->findEyepieceById( "eyepiece" + '_' + QString::number( nextEyepiece ) ) )
        nextEyepiece++;
    OAL::Eyepiece *e = new OAL::Eyepiece( "eyepiece" + '_' + QString::number( nextEyepiece++ ), ui.e_Model->text(),
                                          ui.e_Vendor->text(), ui.Fov->value(), ui.FovUnit->currentText(), ui.e_focalLength->value() );
    ks->data()->logObject()->eyepieceList()->append( e );

    ui.e_Id->clear();
    ui.e_Model->clear();
    ui.e_Vendor->clear();
    ui.Fov->setValue(0);
    ui.e_focalLength->setValue(0);

    saveEquipment(); //Save the new list.
}

void EquipmentWriter::slotRemoveEyepiece() {
    OAL::Eyepiece *e = ks->data()->logObject()->findEyepieceById( ui.e_Id->text() );
    int idx = ks->data()->logObject()->eyepieceList()->indexOf( e );
    if ( idx < 0 )
        return;
    ks->data()->logObject()->eyepieceList()->removeAt( idx );
    if( e )
        delete e;

    ui.e_Id->clear();
    ui.e_Model->clear();
    ui.e_Vendor->clear();
    ui.Fov->setValue( 0 );
    ui.e_focalLength->setValue( 0 );

    QListWidgetItem *item = ui.EyepieceList->takeItem( ui.EyepieceList->currentRow() );
    if( item )
        delete item;

    saveEquipment(); //Save the new list.
}
void EquipmentWriter::slotSaveEyepiece() {
    OAL::Eyepiece *e = ks->data()->logObject()->findEyepieceByName( ui.e_Id->text() );
    if( e ){
        e->setEyepiece( ui.e_Id->text(), ui.e_Model->text(), ui.e_Vendor->text(), ui.Fov->value(), ui.FovUnit->currentText(), ui.e_focalLength->value() );
    } 
    saveEquipment(); //Save the new list.
}

void EquipmentWriter::slotSetEyepiece( QString name ) {
    OAL::Eyepiece *e;
    e = ks->data()->logObject()->findEyepieceByName( name ); 
    if( e ) {
        ui.e_Id->setText( e->id() );
        ui.e_Model->setText( e->model() );
        ui.e_Vendor->setText( e->vendor() );
        ui.Fov->setValue( e->appFov() );
        ui.FovUnit->setCurrentIndex( ui.FovUnit->findText( e->fovUnit() ) );
        ui.e_focalLength->setValue( e->focalLength() );
    }
}

void EquipmentWriter::slotAddLens() {
    while ( ks->data()->logObject()->findLensById( "lens" + '_' + QString::number( nextLens ) ) )
        nextLens++;
    OAL::Lens *l = new OAL::Lens( "lens" + '_' + QString::number( nextLens++ ), ui.l_Model->text(), ui.l_Vendor->text(), ui.l_Factor->value() ) ;
    ks->data()->logObject()->lensList()->append( l );

    ui.l_Id->clear();
    ui.l_Model->clear();
    ui.l_Vendor->clear();
    ui.l_Factor->setValue( 0 );

    saveEquipment(); //Save the new list.
}

void EquipmentWriter::slotRemoveLens() {
    OAL::Lens *l = ks->data()->logObject()->findLensById( ui.l_Id->text() );
    int idx = ks->data()->logObject()->lensList()->indexOf( l );
    if ( idx < 0 )
        return;
    ks->data()->logObject()->lensList()->removeAt( idx );
    if( l )
        delete l;

    ui.l_Id->clear();
    ui.l_Model->clear();
    ui.l_Vendor->clear();
    ui.l_Factor->setValue( 0 );
    ui.f_Color->setCurrentIndex( 0 );

    QListWidgetItem *item = ui.LensList->takeItem( ui.LensList->currentRow() );
    if( item )
        delete item;

    saveEquipment(); //Save the new list.
}

void EquipmentWriter::slotSaveLens() {
    OAL::Lens *l = ks->data()->logObject()->findLensByName( ui.l_Id->text() );
    if( l ){
        l->setLens( ui.l_Id->text(), ui.l_Model->text(), ui.l_Vendor->text(), ui.l_Factor->value() );
    }
    saveEquipment(); //Save the new list.
}

void EquipmentWriter::slotSetLens( QString name ) {
    OAL::Lens *l;
    l = ks->data()->logObject()->findLensByName( name );
    if( l ) {
        ui.l_Id->setText( l->id() );
        ui.l_Model->setText( l->model() );
        ui.l_Vendor->setText( l->vendor() );
        ui.l_Factor->setValue( l->factor() );
    }
}

void EquipmentWriter::slotAddFilter() {
    OAL::Filter *f = new OAL::Filter( "filter" + '_' + QString::number( ks->data()->logObject()->filterList()->size() ),
                                      ui.f_Model->text(), ui.f_Vendor->text(), static_cast<OAL::Filter::FILTER_TYPE>( ui.f_Type->currentIndex() ),
                                      static_cast<OAL::Filter::FILTER_COLOR>( ui.f_Color->currentIndex() ) );
    ks->data()->logObject()->filterList()->append( f );

    ui.f_Id->clear();
    ui.f_Model->clear();
    ui.f_Vendor->clear();
    ui.f_Type->setCurrentIndex( 0 );
    ui.f_Color->setCurrentIndex( 0 );

    ui.FilterList->addItem( f->name() );

    saveEquipment(); //Save the new list.
}

void EquipmentWriter::slotRemoveFilter() {
    OAL::Filter *f = ks->data()->logObject()->findFilterById( ui.f_Id->text() );
    int idx = ks->data()->logObject()->filterList()->indexOf( f );
    if ( idx < 0 )
        return;
    ks->data()->logObject()->filterList()->removeAt( idx );
    if( f )
        delete f;

    ui.f_Id->clear();
    ui.f_Model->clear();
    ui.f_Vendor->clear();
    ui.f_Type->setCurrentIndex( 0 );
    ui.f_Color->setCurrentIndex( 0 );

    QListWidgetItem *item = ui.FilterList->takeItem( ui.FilterList->currentRow() );
    if( item )
        delete item;

    saveEquipment(); //Save the new list.
}

void EquipmentWriter::slotSaveFilter() {
    OAL::Filter *f = ks->data()->logObject()->findFilterById( ui.f_Id->text() );
    if( f ){
        f->setFilter( ui.f_Id->text(), ui.f_Model->text(), ui.f_Vendor->text(),
                      static_cast<OAL::Filter::FILTER_TYPE>( ui.f_Type->currentIndex() ), static_cast<OAL::Filter::FILTER_COLOR>( ui.f_Color->currentIndex() ) );

        ui.FilterList->currentItem()->setText( f->name() );
        saveEquipment(); //Save the new list.
    } 
}

void EquipmentWriter::slotSetFilter( QString name ) {
    OAL::Filter *f;
    f = ks->data()->logObject()->findFilterByName( name ); 
    if( f ) {
        ui.f_Id->setText( f->id() );
        ui.f_Model->setText( f->model() );
        ui.f_Vendor->setText( f->vendor() );
        ui.f_Type->setCurrentIndex( f->type() );
        ui.f_Color->setCurrentIndex( f->color() );
    }
}

void EquipmentWriter::saveEquipment() {
    ks->data()->logObject()->saveEquipmentToFile();

#ifdef HAVE_INDI_H
    ks->indiDriver()->updateCustomDrivers();
#endif
}

void EquipmentWriter::loadEquipment() {
    ks->data()->logObject()->loadEquipmentFromFile();

    ui.ScopeList->clear();
    ui.EyepieceList->clear();
    ui.LensList->clear();
    ui.FilterList->clear();
    foreach( OAL::Scope *s, *( ks->data()->logObject()->scopeList() ) )
        ui.ScopeList->addItem( s->name() );
    foreach( OAL::Eyepiece *e, *( ks->data()->logObject()->eyepieceList() ) )
        ui.EyepieceList->addItem( e->name() );
    foreach( OAL::Lens *l, *( ks->data()->logObject()->lensList() ) )
        ui.LensList->addItem( l->name() );
    foreach( OAL::Filter *f, *( ks->data()->logObject()->filterList() ) )
        ui.FilterList->addItem( f->name() );
}

void EquipmentWriter::slotClose()
{
   hide();
}

void EquipmentWriter::setupFilterTab()
{
    ui.f_Type->addItems( OAL::Filter::filterTypes() );
    ui.f_Color->addItems( OAL::Filter::filterColors() );
}

#include "equipmentwriter.moc"
