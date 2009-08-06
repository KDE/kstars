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

#include "comast/equipmentwriter.h"
#include "ui_equipmentwriter.h"

#include <QFile>

#include <kstandarddirs.h>

#include "comast/comast.h"
#include "comast/scope.h"
#include "comast/eyepiece.h"
#include "comast/lens.h"
#include "comast/filter.h"

EquipmentWriter::EquipmentWriter() {
    QWidget *widget = new QWidget;
    ui.setupUi( widget );
    setMainWidget( widget );
    setCaption( i18n( "Equipment Writer" ) );
    setButtons( KDialog::Close );
    ks = KStars::Instance();
    loadEquipment();
    newScope = true;
    newEyepiece = true;
    newLens = true;
    newFilter = true;

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
}

void EquipmentWriter::slotAddScope() {
    if( ui.Id->text().isEmpty() ) {
        KMessageBox::sorry( 0, i18n("The Id field cannot be empty"), i18n("Invalid Id") );
        return;
    }
    Comast::Scope *s = ks->data()->logObject()->findScopeByName( ui.Id->text() );
    if( s ) {
        if( Comast::warningOverwrite( i18n( "Another Scope already exists with the given Id, Overwrite?" ) ) == KMessageBox::Yes ) {
            s->setScope( ui.Id->text(), ui.Model->text(), ui.Vendor->text(), ui.Type->text(), ui.FocalLength->value(), ui.Aperture->value() );
        } else
            return; //Do nothing
    } else { // No such scope exists, so create a new scope
        s = new Comast::Scope( ui.Id->text(), ui.Model->text(), ui.Vendor->text(), ui.Type->text(), ui.FocalLength->value(), ui.Aperture->value() );
        ks->data()->logObject()->scopeList()->append( s );
    }

    saveEquipment(); //Save the new list.
    ui.Id->clear();
    ui.Model->clear();
    ui.Vendor->clear();
    ui.Type->clear();
    ui.FocalLength->setValue(0);
}
void EquipmentWriter::slotSaveScope() {
    if( ui.Id->text().isEmpty() ) {
        KMessageBox::sorry( 0, i18n("The Id field cannot be empty"), i18n("Invalid Id") );
        return;
    }
    Comast::Scope *s = ks->data()->logObject()->findScopeByName( ui.Id->text() );
    if( s ) {
        s->setScope( ui.Id->text(), ui.Model->text(), ui.Vendor->text(), ui.Type->text(), ui.FocalLength->value(), ui.Aperture->value() );
    }
    saveEquipment(); //Save the new list.
}
 
void EquipmentWriter::slotSetScope( QString id) {
    Comast::Scope *s = ks->data()->logObject()->findScopeByName( id );
    if ( s ) {
        ui.Id->setText( s->id() ) ;
        ui.Model->setText( s->model() );
        ui.Vendor->setText( s->vendor() );
        ui.Type->setText( s->type() );
        ui.FocalLength->setValue( s->focalLength() );
        newScope = false;
    }
}
void EquipmentWriter::slotNewScope() {
    ui.Id->clear();
    ui.Model->clear();
    ui.Vendor->clear();
    ui.Type->clear();
    ui.FocalLength->setValue(0);
    ui.ScopeList->selectionModel()->clear();
    newScope = true;
}

void EquipmentWriter::slotAddEyepiece() {
    if( ui.e_Id->text().isEmpty() ) {
        KMessageBox::sorry( 0, i18n("The Id field cannot be empty"), i18n("Invalid Id") );
        return;
    }
    Comast::Eyepiece *e = ks->data()->logObject()->findEyepieceByName( ui.e_Id->text() );
    if( e ){
        if( Comast::warningOverwrite( i18n ( "Another Eyepiece already exists with the given Id, Overwrite?" ) ) == KMessageBox::Yes ) {
            e->setEyepiece( ui.e_Id->text(), ui.e_Model->text(), ui.e_Vendor->text(), ui.Fov->value(), ui.FovUnit->currentText(), ui.e_focalLength->value() );
        } else
            return;
    } else {
        e = new Comast::Eyepiece( ui.e_Id->text(), ui.e_Model->text(), ui.e_Vendor->text(), ui.Fov->value(), ui.FovUnit->currentText(), ui.e_focalLength->value() );
        ks->data()->logObject()->eyepieceList()->append( e );
    }
    saveEquipment(); //Save the new list.
    ui.e_Id->clear();
    ui.e_Model->clear();
    ui.e_Vendor->clear();
    ui.Fov->setValue(0);
    ui.e_focalLength->setValue(0);
}

void EquipmentWriter::slotSaveEyepiece() {
    if( ui.e_Id->text().isEmpty() ) {
        KMessageBox::sorry( 0, i18n("The Id field cannot be empty"), i18n("Invalid Id") );
        return;
    }
    Comast::Eyepiece *e = ks->data()->logObject()->findEyepieceByName( ui.e_Id->text() );
    if( e ){
        e->setEyepiece( ui.e_Id->text(), ui.e_Model->text(), ui.e_Vendor->text(), ui.Fov->value(), ui.FovUnit->currentText(), ui.e_focalLength->value() );
    } 
    saveEquipment(); //Save the new list.
}

void EquipmentWriter::slotSetEyepiece( QString id ) {
    Comast::Eyepiece *e;
    e = ks->data()->logObject()->findEyepieceByName( id ); 
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
    if( ui.l_Id->text().isEmpty() ) {
        KMessageBox::sorry( 0, i18n("The Id field cannot be empty"), i18n("Invalid Id") );
        return;
    }
    Comast::Lens *l = ks->data()->logObject()->findLensByName( ui.l_Id->text() );
    if( l ){
        if( Comast::warningOverwrite( ( "Another Lens already exists with the given Id, Overwrite?" ) ) == KMessageBox::Yes ) {
            l->setLens( ui.l_Id->text(), ui.l_Model->text(), ui.l_Vendor->text(), ui.l_Factor->value() );
        } else
            return;
    } else {
        l = new Comast::Lens( ui.l_Id->text(), ui.l_Model->text(), ui.l_Vendor->text(), ui.l_Factor->value() );
        ks->data()->logObject()->lensList()->append( l );
    }
    saveEquipment(); //Save the new list.
    ui.l_Id->clear();
    ui.l_Model->clear();
    ui.l_Vendor->clear();
    ui.l_Factor->setValue(0);
}

void EquipmentWriter::slotSaveLens() {
    if( ui.l_Id->text().isEmpty() ) {
        KMessageBox::sorry( 0, i18n("The Id field cannot be empty"), i18n("Invalid Id") );
        return;
    }
    Comast::Lens *l = ks->data()->logObject()->findLensByName( ui.l_Id->text() );
    if( l ){
        l->setLens( ui.l_Id->text(), ui.l_Model->text(), ui.l_Vendor->text(), ui.l_Factor->value() );
    }
    saveEquipment(); //Save the new list.
}

void EquipmentWriter::slotSetLens( QString id ) {
    Comast::Lens *l;
    l = ks->data()->logObject()->findLensByName( id );
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
    if( ui.f_Id->text().isEmpty() ) {
        KMessageBox::sorry( 0, i18n("The Id field cannot be empty"), i18n("Invalid Id") );
        return;
    }
    Comast::Filter *f = ks->data()->logObject()->findFilterByName( ui.f_Id->text() );
    if( f ){
        if( Comast::warningOverwrite( ( "Another Filter already exists with the given Id, Overwrite?" ) ) == KMessageBox::Yes ) {
            f->setFilter( ui.f_Id->text(), ui.f_Model->text(), ui.f_Vendor->text(), ui.f_Type->text(), ui.f_Color->text() );
        } else
            return;
    } else {
        f = new Comast::Filter( ui.f_Id->text(), ui.f_Model->text(), ui.f_Vendor->text(), ui.f_Type->text(), ui.f_Color->text() );
        ks->data()->logObject()->filterList()->append( f );
    }
    saveEquipment(); //Save the new list.
    ui.f_Id->clear();
    ui.f_Model->clear();
    ui.f_Vendor->clear();
    ui.f_Type->clear();
    ui.f_Color->clear();
}

void EquipmentWriter::slotSaveFilter() {
    if( ui.f_Id->text().isEmpty() ) {
        KMessageBox::sorry( 0, i18n("The Id field cannot be empty"), i18n("Invalid Id") );
        return;
    }
    Comast::Filter *f = ks->data()->logObject()->findFilterByName( ui.f_Id->text() );
    if( f ){
        f->setFilter( ui.f_Id->text(), ui.f_Model->text(), ui.f_Vendor->text(), ui.f_Type->text(), ui.f_Color->text() );
    } 
    saveEquipment(); //Save the new list.
}

void EquipmentWriter::slotSetFilter( QString id ) {
    Comast::Filter *f;
    f = ks->data()->logObject()->findFilterByName( id ); 
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

void EquipmentWriter::saveEquipment() {
    QFile f;
    f.setFileName( KStandardDirs::locateLocal( "appdata", "equipmentlist.xml" ) );   
    if ( ! f.open( QIODevice::WriteOnly ) ) {
        kDebug() << "Cannot write list to  file";
        return;
    }
    QTextStream ostream( &f );
    ks->data()->logObject()->writeBegin();
    ks->data()->logObject()->writeScopes();
    ks->data()->logObject()->writeEyepieces();
    ks->data()->logObject()->writeLenses();
    ks->data()->logObject()->writeFilters();
    ks->data()->logObject()->writeEnd();
    ostream << ks->data()->logObject()->writtenOutput();
    f.close();
}

void EquipmentWriter::loadEquipment() {
    QFile f;
    f.setFileName( KStandardDirs::locateLocal( "appdata", "equipmentlist.xml" ) );   
    if( ! f.open( QIODevice::ReadOnly ) )
        return;
    QTextStream istream( &f );
    ks->data()->logObject()->readBegin( istream.readAll() );
    f.close();
    ui.ScopeList->clear();
    ui.EyepieceList->clear();
    ui.LensList->clear();
    ui.FilterList->clear();
    foreach( Comast::Scope *s, *( ks->data()->logObject()->scopeList() ) )
        ui.ScopeList->addItem( s->id() );
    foreach( Comast::Eyepiece *e, *( ks->data()->logObject()->eyepieceList() ) )
        ui.EyepieceList->addItem( e->id() );
    foreach( Comast::Lens *l, *( ks->data()->logObject()->lensList() ) )
        ui.LensList->addItem( l->id() );
    foreach( Comast::Filter *f, *( ks->data()->logObject()->filterList() ) )
        ui.FilterList->addItem( f->id() );
}

void EquipmentWriter::slotSave() {
    switch( ui.tabWidget->currentIndex() ) {
        case 0: {
            if( newScope )
                slotAddScope();
            else
                slotSaveScope();
            ui.ScopeList->clear();
            foreach( Comast::Scope *s, *( ks->data()->logObject()->scopeList() ) )
                ui.ScopeList->addItem( s->id() );
            break;
        }
        case 1: {
            if( newEyepiece )
                slotAddEyepiece();
            else
                slotSaveEyepiece();
            ui.EyepieceList->clear();
            foreach( Comast::Eyepiece *e, *( ks->data()->logObject()->eyepieceList() ) )
                ui.EyepieceList->addItem( e->id() );
            break;
        }
        case 2: {
            if( newLens )
                slotAddLens();
            else
                slotSaveLens();
            ui.LensList->clear();
            foreach( Comast::Lens *l, *( ks->data()->logObject()->lensList() ) )
                ui.LensList->addItem( l->id() );
            break;
        }
        case 3: {
            if( newFilter )
                slotAddFilter();
            else
                slotSaveFilter();
            ui.FilterList->clear();
            foreach( Comast::Filter *f, *( ks->data()->logObject()->filterList() ) )
                ui.FilterList->addItem( f->id() );
            break;
        }
    }
}
#include "equipmentwriter.moc"
