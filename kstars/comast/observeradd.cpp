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
    // Setting up the widget from the .ui file and adding it to the KDialog
    QWidget *widget = new QWidget;
    ui.setupUi( widget );
    setMainWidget( widget );
    setCaption( i18n( "Add Observer" ) );
    setButtons( KDialog::Close );
    ks = KStars::Instance();

    // Load the observers list from the file
    loadObservers();

    // Make connections
    connect( ui.AddObserver, SIGNAL( clicked() ), this, SLOT( slotAddObserver() ) );
}

void ObserverAdd::slotAddObserver() {
    if( ui.Name->text().isEmpty() ) {
        KMessageBox::sorry( 0, i18n("The Name field cannot be empty"), i18n("Invalid Input") );
        return;
    }
    Comast::Observer *o = ks->data()->logObject()->findObserverByName( ui.Name->text() + ui.Surname->text() ); //The findObserverByName uses the fullName for searching
    if( o ) {
        if( Comast::warningOverwrite( i18n( "Another Observer already exists with the given Name and Surname, Overwrite?" ) ) == KMessageBox::Yes ) {
            o->setObserver( o->name(), o->surname(), ui.Contact->text() );
        } else
            return; //Do nothing
    } else { // No such observer exists, so create a new observer object and append to file
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
        KMessageBox::sorry( 0, i18n( "Could not save the observer list to the file" ), i18n( "Write Error!" ) );
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
