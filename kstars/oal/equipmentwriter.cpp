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

#include <kstandarddirs.h>

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
    ks = KStars::Instance();
    nextScope = 0;
    nextEyepiece = 0;
    nextFilter = 0;
    nextLens = 0;
    loadEquipment();
    newScope = true;
    newEyepiece = true;
    newLens = true;
    newFilter = true;

    #ifdef HAVE_INDI_H
    ui.driverComboBox->insertItems(1, KStars::Instance()->indiDriver()->driversList.keys());
    #endif

    //make connections
    connect( this, SIGNAL( closeClicked() ), this, SLOT( slotClose() ) );
    connect( ui.NewScope, SIGNAL( clicked() ), this, SLOT( slotNewScope() ) );
    connect( ui.NewEyepiece, SIGNAL( clicked() ), this, SLOT( slotNewEyepiece() ) );
    connect( ui.NewLens, SIGNAL( clicked() ), this, SLOT( slotNewLens() ) );
    connect( ui.NewFilter, SIGNAL( clicked() ), this, SLOT( slotNewFilter() ) );
    connect( ui.AddScope, SIGNAL( clicked() ), this, SLOT( slotSave() ) );
    connect( ui.AddEyepiece, SIGNAL( clicked() ), this, SLOT( slotSave() ) );
    connect( ui.AddLens, SIGNAL( clicked() ), this, SLOT( slotSave() ) );
    connect( ui.AddFilter, SIGNAL( clicked() ), this, SLOT( slotSave() ) );
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
    
    ks->data()->userdb()->AddScope(ui.Model->text(), ui.Vendor->text(), ui.driverComboBox->currentText(), 
                                   ui.Type->currentText(), ui.FocalLength->value(), ui.Aperture->value());
    loadEquipment();
    ui.Model->clear();
    ui.Vendor->clear();
    ui.FocalLength->setValue(0);
    ui.Aperture->setValue(0);
    ui.driverComboBox->setCurrentIndex(0);
}

void EquipmentWriter::slotRemoveScope() {
    ks->data()->userdb()->EraseEquipment("telescope",ui.Id->text().toInt());
    ui.Model->clear();
    ui.Vendor->clear();
    ui.FocalLength->setValue(0);
    ui.Aperture->setValue(0);
    loadEquipment();
}

void EquipmentWriter::slotSaveScope() {
    ks->data()->userdb()->AddScope(ui.Model->text(), ui.Vendor->text(), ui.driverComboBox->currentText(), 
                                   ui.Type->currentText(), ui.FocalLength->value(), ui.Aperture->value(), 
                                   ui.Id->text());

    loadEquipment();
}
 
void EquipmentWriter::slotSetScope( QString name) {
    //TODO: maybe this should also use the DB? ~~spacetime
    OAL::Scope *s = ks->data()->logObject()->findScopeByName( name );
    if ( s ) {
        ui.Id->setText( s->id() ) ;
        ui.Model->setText( s->model() );
        ui.Vendor->setText( s->vendor() );
        ui.Type->setCurrentIndex( ui.Type->findText( s->type() ) );
        ui.FocalLength->setValue( s->focalLength() );
        ui.Aperture->setValue( s->aperture() );
        ui.driverComboBox->setCurrentIndex(ui.driverComboBox->findText(s->driver()));
        newScope = false;
    }
}
void EquipmentWriter::slotNewScope() {
    ui.Id->clear();
    ui.Model->clear();
    ui.Vendor->clear();
    ui.FocalLength->setValue(0);
    ui.driverComboBox->setCurrentIndex(0);
    ui.ScopeList->selectionModel()->clear();
    newScope = true;
}

void EquipmentWriter::slotAddEyepiece() {
    ks->data()->userdb()->addEyepiece(ui.e_Vendor->text(), ui.e_Model->text(), 
                                      ui.e_focalLength->value(),  ui.Fov->value(), 
                                      ui.FovUnit->currentText());
    loadEquipment();
    ui.e_Id->clear();
    ui.e_Model->clear();
    ui.e_Vendor->clear();
    ui.Fov->setValue(0);
    ui.e_focalLength->setValue(0);
}

void EquipmentWriter::slotRemoveEyepiece() {
    ks->data()->userdb()->EraseEquipment("eyepiece",ui.e_Id->text().toInt());
    loadEquipment();
    ui.e_Id->clear();
    ui.e_Model->clear();
    ui.e_Vendor->clear();
    ui.Fov->setValue(0);
    ui.e_focalLength->setValue(0);
    ui.EyepieceList->clear();
    foreach( OAL::Eyepiece *e, *( ks->data()->logObject()->eyepieceList() ) )
        ui.EyepieceList->addItem( e->name() );
}
void EquipmentWriter::slotSaveEyepiece() {
    ks->data()->userdb()->addEyepiece(ui.e_Vendor->text(), ui.e_Model->text(), 
                                      ui.e_focalLength->value(),  ui.Fov->value(),
                                      ui.FovUnit->currentText(), ui.e_Id->text());
    loadEquipment();
}

void EquipmentWriter::slotSetEyepiece( QString name ) {
    //TODO: maybe this should also use the DB? ~~spacetime
    OAL::Eyepiece *e;
    e = ks->data()->logObject()->findEyepieceByName( name ); 
    if( e ) {
        ui.e_Id->setText( e->id() );
        ui.e_Model->setText( e->model() );
        ui.e_Vendor->setText( e->vendor() );
        ui.Fov->setValue( e->appFov() );
        ui.e_focalLength->setValue( e->focalLength() );
        newEyepiece = false;
    }
}

void EquipmentWriter::slotNewEyepiece() {
    ui.e_Id->clear();
    ui.e_Model->clear();
    ui.e_Vendor->clear();
    ui.Fov->setValue(0);
    ui.e_focalLength->setValue(0);
    ui.EyepieceList->selectionModel()->clear();
    newEyepiece = true;
}

void EquipmentWriter::slotAddLens() {
    ks->data()->userdb()->addLens(ui.l_Vendor->text(), ui.l_Model->text(), ui.l_Factor->value() );
    loadEquipment();
    ui.l_Id->clear();
    ui.l_Model->clear();
    ui.l_Vendor->clear();
    ui.l_Factor->setValue(0);
}

void EquipmentWriter::slotRemoveLens() {
    ks->data()->userdb()->EraseEquipment("lens",ui.e_Id->text().toInt());
    loadEquipment();
    ui.l_Id->clear();
    ui.l_Model->clear();
    ui.l_Vendor->clear();
    ui.l_Factor->setValue(0);
    ui.LensList->clear();
    foreach( OAL::Lens *l, *( ks->data()->logObject()->lensList() ) )
        ui.LensList->addItem( l->name() );
}
void EquipmentWriter::slotSaveLens() {
    ks->data()->userdb()->addLens(ui.l_Vendor->text(), ui.l_Model->text(), 
                                      ui.l_Factor->value(), ui.l_Id->text());
    loadEquipment();
}

void EquipmentWriter::slotSetLens( QString name ) {
    OAL::Lens *l;
    l = ks->data()->logObject()->findLensByName( name );
    if( l ) {
        ui.l_Id->setText( l->id() );
        ui.l_Model->setText( l->model() );
        ui.l_Vendor->setText( l->vendor() );
        ui.l_Factor->setValue( l->factor() );
        newLens = false;
    }
}

void EquipmentWriter::slotNewLens() {
    ui.l_Id->clear();
    ui.l_Model->clear();
    ui.l_Vendor->clear();
    ui.l_Factor->setValue(0);
    ui.LensList->selectionModel()->clear();
    newLens = true;
}

void EquipmentWriter::slotAddFilter() {
    ks->data()->userdb()->addFilter( ui.f_Vendor->text(), ui.f_Model->text(), 
                                     ui.f_Type->text(), ui.f_Color->text());
    loadEquipment();
    ui.f_Id->clear();
    ui.f_Model->clear();
    ui.f_Vendor->clear();
    ui.f_Type->clear();
    ui.f_Color->clear();
}

void EquipmentWriter::slotRemoveFilter() {
    ks->data()->userdb()->EraseEquipment("filter",ui.f_Id->text().toInt());
    loadEquipment();
    ui.f_Id->clear();
    ui.f_Model->clear();
    ui.f_Vendor->clear();
    ui.f_Type->clear();
    ui.f_Color->clear();
    ui.FilterList->clear();
    foreach( OAL::Filter *f, *( ks->data()->logObject()->filterList() ) )
        ui.FilterList->addItem( f->name() );
}

void EquipmentWriter::slotSaveFilter() {
    ks->data()->userdb()->addFilter( ui.f_Vendor->text(), ui.f_Model->text(), 
                                     ui.f_Type->text(), ui.f_Color->text(), 
                                     ui.f_Id->text());
    loadEquipment();
}

void EquipmentWriter::slotSetFilter( QString name ) {
    OAL::Filter *f;
    f = ks->data()->logObject()->findFilterByName( name ); 
    if( f ) {
        ui.f_Id->setText( f->id() );
        ui.f_Model->setText( f->model() );
        ui.f_Vendor->setText( f->vendor() );
        ui.f_Type->setText( f->type() );
        ui.f_Color->setText( f->color() );
        newFilter = false;
    }
}

void EquipmentWriter::slotNewFilter() {
    ui.f_Id->clear();
    ui.f_Model->clear();
    ui.f_Vendor->clear();
    ui.f_Type->clear();
    ui.f_Color->clear();
    ui.FilterList->selectionModel()->clear();
    newFilter = true;
}

void EquipmentWriter::loadEquipment() {
    
    ks->data()->logObject()->readScopes();
    ks->data()->logObject()->readEyepieces();
    ks->data()->logObject()->readLenses();
    ks->data()->logObject()->readFilters();
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
    
    //TODO: confirm this is correct ~~spacetime
    #ifdef HAVE_INDI_H
    KStars::Instance()->indiDriver()->updateCustomDrivers();
    #endif
}

void EquipmentWriter::slotSave() {
    switch( ui.tabWidget->currentIndex() ) {
        case 0: {
            if( newScope )
                slotAddScope();
            else
                slotSaveScope();
            ui.ScopeList->clear();
            foreach( OAL::Scope *s, *( ks->data()->logObject()->scopeList() ) )
                ui.ScopeList->addItem( s->name() );
            break;
        }
        case 1: {
            if( newEyepiece )
                slotAddEyepiece();
            else
                slotSaveEyepiece();
            ui.EyepieceList->clear();
            foreach( OAL::Eyepiece *e, *( ks->data()->logObject()->eyepieceList() ) )
                ui.EyepieceList->addItem( e->name() );
            break;
        }
        case 2: {
            if( newLens )
                slotAddLens();
            else
                slotSaveLens();
            ui.LensList->clear();
            foreach( OAL::Lens *l, *( ks->data()->logObject()->lensList() ) )
                ui.LensList->addItem( l->name() );
            break;
        }
        case 3: {
            if( newFilter )
                slotAddFilter();
            else
                slotSaveFilter();
            ui.FilterList->clear();
            foreach( OAL::Filter *f, *( ks->data()->logObject()->filterList() ) )
                ui.FilterList->addItem( f->name() );
            break;
        }
    }
}

void EquipmentWriter::slotClose()
{
   hide();
}

#include "equipmentwriter.moc"
