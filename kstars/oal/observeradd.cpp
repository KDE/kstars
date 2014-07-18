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

#include <QFile>

#include <kmessagebox.h>


#include "kstarsdata.h"
#include "observeradd.h"
#include "ui_observeradd.h"
#include "observer.h"

ObserverAdd::ObserverAdd() {
    // Setting up the widget from the .ui file and adding it to the QDialog
    QWidget *widget = new QWidget;
    ui.setupUi( widget );

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(widget);
    setLayout(mainLayout);

    setWindowTitle( xi18n( "Add Observer" ) );

    //FIXME Need porting to KF5
    //setMainWidget( widget );
    //setButtons( QDialog::Close );
    ks = KStars::Instance();
    nextObserver = 0;

    // Load the observers list from the file
    loadObservers();

    // Make connections
    connect( ui.AddObserver, SIGNAL( clicked() ), this, SLOT( slotAddObserver() ) );
}

void ObserverAdd::slotAddObserver() {
    if( ui.Name->text().isEmpty() ) {
        KMessageBox::sorry( 0, xi18n("The Name field cannot be empty"), xi18n("Invalid Input") );
        return;
    }

    if (KStarsData::Instance()->userdb()->FindObserver(ui.Name->text(),ui.Surname->text())){
        if( OAL::warningOverwrite( xi18n( "Another Observer already exists with the given Name and Surname, Overwrite?" ) ) == KMessageBox::No ) return;
    }

    KStarsData::Instance()->userdb()->AddObserver(ui.Name->text(),ui.Surname->text(),ui.Contact->text());

    //Reload observers into OAL::m_observers
    loadObservers();

    // Reset the UI for a fresh addition
    ui.Name->clear();
    ui.Surname->clear();
    ui.Contact->clear();
}

void ObserverAdd::loadObservers() {
    ks->data()->logObject()->readObservers();
}


