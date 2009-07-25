/***************************************************************************
                          observeradd.cpp  -  description

                             -------------------
    begin                : Sunday July 26, 2009
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

#include "comast/observeradd.h"
#include "ui_observeradd.h"

#include <QFile>

#include <kmessagebox.h>
#include <kstandarddirs.h>

#include "comast/observer.h"

ObserverAdd::ObserverAdd() {
    QWidget *widget = new QWidget;
    ui.setupUi( widget );
    setMainWidget( widget );
    setCaption( i18n( "Add Observer" ) );
    setButtons( KDialog::Close );
    ks = KStars::Instance();
    loadObservers();

    connect( ui.AddObserver, SIGNAL( clicked() ), this, SLOT( slotAddObserver() ) );
}

void ObserverAdd::slotAddObserver() {
    if( ui.Name->text().isEmpty() ) {
        KMessageBox::sorry( 0, i18n("The Name field cannot be empty"), i18n("Invalid Input") );
        return;
    }
    Comast::Observer *o = ks->data()->logObject()->findObserverByName( ui.Name->text() + ui.Surname->text() );
    if( o ) {
        if( KMessageBox::warningYesNo( 0, i18n("Another Observer already exists with the given Name and Surname, Overwrite?"), i18n( "Overwrite" ), KGuiItem( i18n("Overwrite") ), KGuiItem( i18n( "Cancel" ) ) ) == KMessageBox::Yes ) {
            o->setObserver( o->name(), o->surname(), ui.Contact->text() );
        } else
            return; //Do nothing
    } else {
        o = new Comast::Observer( ui.Name->text(), ui.Surname->text(), ui.Contact->text() );
        ks->data()->logObject()->observerList()->append( o );
    }
 
    // Save the new observer list
    saveObservers();

    // Reset the UI for a fresh addition
    ui.Name->clear();
    ui.Surname->clear();
    ui.Contact->clear();
}

void ObserverAdd::saveObservers() {
    QFile f;
    f.setFileName( KStandardDirs::locateLocal( "appdata", "observerlist.xml" ) );   
    if ( ! f.open( QIODevice::WriteOnly ) ) {
        kDebug() << "Cannot write list to  file";
        return;
    }
    QTextStream ostream( &f );
    ostream << ks->data()->logObject()->writeLog( false );
    f.close();
}

void ObserverAdd::loadObservers() {
    QFile f;
    f.setFileName( KStandardDirs::locateLocal( "appdata", "observerlist.xml" ) );   
    if( ! f.open( QIODevice::ReadOnly ) )
        return;
    QTextStream istream( &f );
    ks->data()->logObject()->readBegin( istream.readAll() );
    f.close();
}
    
#include "observeradd.moc"
