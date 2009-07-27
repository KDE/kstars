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
    slotLoadEquipment();

    //make connections
    connect( this, SIGNAL( closeClicked() ), this, SLOT( slotClose() ) );
    connect( ui.AddScope, SIGNAL( clicked() ), this, SLOT( slotAddScope() ) );
    connect( ui.AddEyepiece, SIGNAL( clicked() ), this, SLOT( slotAddEyepiece() ) );
    connect( ui.AddLens, SIGNAL( clicked() ), this, SLOT( slotAddLens() ) );
    connect( ui.AddFilter, SIGNAL( clicked() ), this, SLOT( slotAddFilter() ) );
}

void EquipmentWriter::slotAddScope() {
    if( ui.Id->text().isEmpty() ) {
        KMessageBox::sorry( 0, i18n("The Id field cannot be empty"), i18n("Invalid Id") );
        return;
    }
    Comast::Scope *s = ks->data()->logObject()->findScopeByName( ui.Id->text() );
    if( s ) {
        if( Comast::warningOverwrite( i18n( "Another Scope already exists with the given Id, Overwrite?" ) ) == KMessageBox::Yes ) {
            s->setScope( ui.Id->text(), ui.Model->text(), ui.Vendor->text(), ui.Type->text(), ui.FocalLength->value() );
        } else
            return; //Do nothing
    } else { // No such scope exists, so create a new scope
        s = new Comast::Scope( ui.Id->text(), ui.Model->text(), ui.Vendor->text(), ui.Type->text(), ui.FocalLength->value() );
        ks->data()->logObject()->scopeList()->append( s );
    }

    slotSaveEquipment(); //Save the new list.
    ui.Id->clear();
    ui.Model->clear();
    ui.Vendor->clear();
    ui.Type->clear();
    ui.FocalLength->setValue(0);
}
 
void EquipmentWriter::slotAddEyepiece() {
    if( ui.e_Id->text().isEmpty() ) {
        KMessageBox::sorry( 0, i18n("The Id field cannot be empty"), i18n("Invalid Id") );
        return;
    }
    Comast::Eyepiece *e = ks->data()->logObject()->findEyepieceByName( ui.e_Id->text() );
    if( e ){
        if( Comast::warningOverwrite( i18n ( "Another Eyepiece already exists with the given Id, Overwrite?" ) ) == KMessageBox::Yes ) {
            e->setEyepiece( ui.e_Id->text(), ui.e_Model->text(), ui.e_Vendor->text(), ui.Fov->value(), ui.FovUnit->text(), ui.e_focalLength->value() );
        } else
            return;
    } else {
        e = new Comast::Eyepiece( ui.e_Id->text(), ui.e_Model->text(), ui.e_Vendor->text(), ui.Fov->value(), ui.FovUnit->text(), ui.e_focalLength->value() );
        ks->data()->logObject()->eyepieceList()->append( e );
    }
    slotSaveEquipment(); //Save the new list.
    ui.e_Id->clear();
    ui.e_Model->clear();
    ui.e_Vendor->clear();
    ui.FovUnit->clear();
    ui.Fov->setValue(0);
    ui.e_focalLength->setValue(0);
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
    slotSaveEquipment(); //Save the new list.
    ui.l_Id->clear();
    ui.l_Model->clear();
    ui.l_Vendor->clear();
    ui.l_Factor->setValue(0);
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
    slotSaveEquipment(); //Save the new list.
    ui.f_Id->clear();
    ui.f_Model->clear();
    ui.f_Vendor->clear();
    ui.f_Type->clear();
    ui.f_Color->clear();
}

void EquipmentWriter::slotSaveEquipment() {
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

void EquipmentWriter::slotLoadEquipment() {
    QFile f;
    f.setFileName( KStandardDirs::locateLocal( "appdata", "equipmentlist.xml" ) );   
    if( ! f.open( QIODevice::ReadOnly ) )
        return;
    QTextStream istream( &f );
    ks->data()->logObject()->readBegin( istream.readAll() );
    f.close();
}

#include "equipmentwriter.moc"
