/***************************************************************************
                          execute.cpp  -  description

                             -------------------
    begin                : Friday July 21, 2009
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

#include "comast/execute.h"

#include <QFile>

#include <kmessagebox.h>
#include <kfiledialog.h>
#include "comast/observer.h"
#include "comast/site.h"
#include "comast/session.h"
#include "comast/scope.h"
#include "comast/eyepiece.h"
#include "comast/lens.h"
#include "comast/filter.h"
#include "skyobjects/skyobject.h"
#include "dialogs/locationdialog.h"

Execute::Execute() {
    QWidget *w = new QWidget;
    ui.setupUi( w );
    setMainWidget( w );
    setCaption( i18n( "Execute Session" ) );
    setButtons( KDialog::User1|KDialog::Close );
    setButtonGuiItem( KDialog::User1, KGuiItem( i18n("End Session"), QString(), i18n("Save and End the current session") ) );
    ks = KStars::Instance();
    currentTarget = NULL;
    currentObserver = NULL;
    currentScope = NULL;
    currentEyepiece = NULL;
    currentLens = NULL;
    currentFilter = NULL;

    //initialize the global logObject
    logObject = ks->data()->logObject();

    //initialize the lists and parameters
    init();

    //make connections
    connect( this, SIGNAL( user1Clicked() ), 
             this, SLOT( slotEndSession() ) );
    connect( ui.NextButton, SIGNAL( clicked() ),
             this, SLOT( slotNext() ) );
    connect( ui.Location, SIGNAL( clicked() ),
             this, SLOT( slotLocation() ) );
    connect( ui.TargetList, SIGNAL( currentIndexChanged(const QString) ),
             this, SLOT( slotSetTarget(QString) ) );
}

void Execute::init() {
    //intialize geo to current location of the ObservingList
    geo = ks->observingList()->geoLocation();
    ui.Location->setText( geo->fullName() );

    //set the date time to the dateTime from the OL
    ui.Begin->setDateTime( ks->observingList()->dateTime().dateTime() );

    //load Targets
    loadTargets();

    //load Equipment
    loadEquipment();

    //load Observers
    loadObservers();

    //set Current Items
    loadCurrentItems();
}
void Execute::loadCurrentItems() {
    //Set the current target, equipments and observer
    if( currentTarget )
        ui.TargetList->setCurrentIndex( ui.TargetList->findText( currentTarget->name() ) );
    if( currentObserver )
        ui.Observer->setCurrentIndex( ui.Observer->findText( currentObserver->name() + " " + currentObserver->surname() ) );
    if( currentScope )
        ui.Scope->setCurrentIndex( ui.Scope->findText( currentScope->id()) );
    if( currentEyepiece )
        ui.Eyepiece->setCurrentIndex( ui.Eyepiece->findText( currentEyepiece->id()) );
    if( currentLens )
        ui.Lens->setCurrentIndex( ui.Lens->findText( currentLens->id()) );
    if( currentFilter )
        ui.Filter->setCurrentIndex( ui.Filter->findText( currentFilter->id()) );
}
void Execute::slotNext() {
    switch( ui.stackedWidget->currentIndex() ) {
        case 0: {
            saveSession();
            break;
        }
        case 1: {
            addTargetNotes();
            break;
        }
        case 2: {
            if ( addObservation() )
                ui.stackedWidget->setCurrentIndex( 1 );
                QString prevTarget = currentTarget->name();
                loadTargets();
                ui.TargetList->setCurrentIndex( ui.TargetList->findText( prevTarget ) );
                selectNextTarget();
            break;
        }
    }
}

void Execute::saveSession() {
    if( ui.Id->text().isEmpty() ) {
        KMessageBox::sorry( 0, i18n("The Id field cannot be empty"), i18n("Invalid Id") );
        return;
    }
    currentSession = logObject->findSessionByName( ui.Id->text() );
    if( currentSession ){
        if( Comast::warningOverwrite( i18n("Another session already exists with the given Id, Overwrite?") ) == KMessageBox::Yes ) {
            currentSession->setSession( ui.Id->text(), geo->fullName(), ui.Begin->dateTime(), ui.Begin->dateTime(), ui.Weather->toPlainText(), ui.Equipment->toPlainText(), ui.Comment->toPlainText(), ui.Language->text() );
        } else
            return;
    } else {
        currentSession = new Comast::Session( ui.Id->text(), geo->fullName(), ui.Begin->dateTime(), ui.Begin->dateTime(), ui.Weather->toPlainText(), ui.Equipment->toPlainText(), ui.Comment->toPlainText(), ui.Language->text() );
        logObject->sessionList()->append( currentSession );
    } 
    if( ! logObject->findSiteByName( geo->fullName() ) ) {
        Comast::Site *newSite = new Comast::Site( geo );
        logObject->siteList()->append( newSite );
    }
    ui.stackedWidget->setCurrentIndex( 1 ); //Move to the next page
}

void Execute::slotLocation() {
    QPointer<LocationDialog> ld = new LocationDialog( ks );
    if ( ld->exec() == QDialog::Accepted ) {
        geo = ld->selectedCity();
        ui.Location->setText( geo -> fullName() );
    }
    delete ld;
}

void Execute::loadTargets() {
    ui.TargetList->clear();
    sortTargetList();
    foreach( SkyObject *o, ks->observingList()->sessionList() )
        ui.TargetList->addItem( o->name() );
}

void Execute::loadEquipment() {
    ui.Scope->clear();
    ui.Eyepiece->clear();
    ui.Lens->clear();
    ui.Filter->clear();
    foreach( Comast::Scope *s, *( logObject->scopeList() ) )
        ui.Scope->addItem( s->id() );
    foreach( Comast::Eyepiece *e, *( logObject->eyepieceList() ) )
        ui.Eyepiece->addItem( e->id() );
    foreach( Comast::Lens *l, *( logObject->lensList() ) )
        ui.Lens->addItem( l->id() );
    foreach( Comast::Filter *f, *( logObject->filterList() ) )
        ui.Filter->addItem( f->id() );
}

void Execute::loadObservers() {
    ui.Observer->clear();
    foreach( Comast::Observer *o,*( logObject->observerList() ) )
        ui.Observer->addItem( o->name() + " " + o->surname() );
}

void Execute::sortTargetList() {
    qSort( ks->observingList()->sessionList().begin(), ks->observingList()->sessionList().end(), Execute::timeLessThan );
}

 bool Execute::timeLessThan ( SkyObject *o1, SkyObject *o2 ) {
    QTime t1 = KStars::Instance()->observingList()->scheduledTime( o1 ), t2 = KStars::Instance()->observingList()->scheduledTime( o2 );
    if( t1 < QTime(12,0,0) )
        t1.setHMS( t1.hour()+12, t1.minute(), t1.second() );
    else
        t1.setHMS( t1.hour()-12, t1.minute(), t1.second() );
    if( t2 < QTime(12,0,0) )
        t2.setHMS( t2.hour()+12, t2.minute(), t2.second() );
    else
        t2.setHMS( t2.hour()-12, t2.minute(), t2.second() );
    return ( t1 < t2 ) ;
}

void Execute::addTargetNotes() {
    SkyObject *o = ks->observingList()->findObjectByName( ui.TargetList->currentText() );
    currentTarget = o;
    if( o ) {
        o->setNotes( ui.Notes->toPlainText() );
        loadObservationTab();
    }
}

void Execute::loadObservationTab() {
   ui.Time->setTime( KStarsDateTime::currentDateTime().time() );
   ui.stackedWidget->setCurrentIndex( 2 );
}

bool Execute::addObservation() {
    if( ui.Id->text().isEmpty() ) {
        KMessageBox::sorry( 0, i18n("The Id field cannot be empty"), i18n("Invalid Id") );
        return false;
    }
    Comast::Observation *o = logObject->findObservationByName( ui.o_Id->text() );
    KStarsDateTime dt = currentSession->begin();
    dt.setTime( ui.Time->time() );
    if( o ){
        if( Comast::warningOverwrite( i18n("Another observation already exists with the given Id, Overwrite?") ) == KMessageBox::Yes ) {
            o->setObservation( ui.o_Id->text(), ui.Observer->currentText(), geo->fullName(), currentSession->id(), currentTarget->name(), dt, ui.FaintestStar->value(), ui.Seeing->value(), ui.Scope->currentText(), ui.Eyepiece->currentText(), ui.Lens->currentText(), ui.Filter->currentText(), ui.Description->toPlainText(), ui.Language->text() );
        } else
            return false;
    } else {
        o = new Comast::Observation( ui.o_Id->text(), ui.Observer->currentText(), geo->fullName(), currentSession->id(), currentTarget->name(), dt, ui.FaintestStar->value(), ui.Seeing->value(), ui.Scope->currentText(), ui.Eyepiece->currentText(), ui.Lens->currentText(), ui.Filter->currentText(), ui.Description->toPlainText(), ui.Language->text() );
        logObject->observationList()->append( o );
    }
    slotSetCurrentObjects();
    return true;
}
void Execute::slotEndSession() {
    currentSession->setSession( ui.Id->text(), geo->fullName(), ui.Begin->dateTime(), KStarsDateTime::currentDateTime(), ui.Weather->toPlainText(), ui.Equipment->toPlainText(), ui.Comment->toPlainText(), ui.Language->text() );
    KUrl fileURL = KFileDialog::getSaveUrl( QDir::homePath(), "*.xml" );
    if( fileURL.isValid() ) {
        QFile f( fileURL.path() );
        if( ! f.open( QIODevice::WriteOnly ) ) {
            QString message = i18n( "Could not open file %1", f.fileName() );
            KMessageBox::sorry( 0, message, i18n( "Could Not Open File" ) );
            return;
        }
        QTextStream ostream( &f );
        ostream<< logObject->writeLog( false );
        f.close();
    }
    hide();
}

void Execute::slotSetTarget( QString name ) { 
    currentTarget = ks->observingList()->findObjectByName( name );
    if( ! currentTarget ) {
        ui.NextButton->setEnabled( false );
        return;
    } else {
        ui.NextButton->setEnabled( true );
        ks->observingList()->selectObject( currentTarget );
        ks->observingList()->slotCenterObject();
        QString smag = "--";
        if (  - 30.0 < currentTarget->mag() && currentTarget->mag() < 90.0 ) smag = QString::number( currentTarget->mag(), 'g', 2 ); // The lower limit to avoid display of unrealistic comet magnitudes
        ui.Mag->setText( smag );
        ui.Type->setText( currentTarget->typeName() );
        ui.SchTime->setText( ks->observingList()->scheduledTime(currentTarget).toString( "h:mm:ss AP" ) ) ;
        SkyPoint p = currentTarget->recomputeCoords( KStarsDateTime::currentDateTime() , geo );
        dms lst(geo->GSTtoLST( KStarsDateTime::currentDateTime().gst() ));
        p.EquatorialToHorizontal( &lst, geo->lat() );
        ui.RA->setText( p.ra()->toHMSString() ) ;
        ui.Dec->setText( p.dec()->toDMSString() );
        ui.Alt->setText( p.alt()->toDMSString() );
        ui.Az->setText( p.az()->toDMSString() );
    }
}

void Execute::selectNextTarget() {
    int i = ui.TargetList->findText( currentTarget->name() ) + 1;
    if( i < ui.TargetList->count() ) {
        ui.TargetList->setCurrentIndex( i );
        slotSetTarget( ui.TargetList->currentText() );
    }
}

void Execute::slotSetCurrentObjects() {
    currentScope = logObject->findScopeByName( ui.Scope->currentText() );
    currentEyepiece = logObject->findEyepieceByName( ui.Eyepiece->currentText() );
    currentLens = logObject->findLensByName( ui.Lens->currentText() );
    currentFilter = logObject->findFilterByName( ui.Filter->currentText() );
    currentObserver = logObject->findObserverByName( ui.Observer->currentText() );
}

#include "execute.moc"
