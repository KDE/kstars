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

#include "oal/execute.h"

#include <QFile>

#include <KMessageBox>
#include <QFileDialog>
#include "kstarsdata.h"
#include "oal/observer.h"
#include "oal/site.h"
#include "oal/session.h"
#include "oal/scope.h"
#include "oal/eyepiece.h"
#include "oal/lens.h"
#include "oal/filter.h"
#include "skyobjects/skyobject.h"
#include "dialogs/locationdialog.h"
#include "dialogs/finddialog.h"
#include "skycomponents/skymapcomposite.h"
#include "observinglist.h"

Execute::Execute() {
    QWidget *w = new QWidget;
    ui.setupUi( w );

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(w);
    setLayout(mainLayout);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Close);
    mainLayout->addWidget(buttonBox);
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

    QPushButton *execB = new QPushButton(i18n("End Session"));
    QPushButton *addObs = new QPushButton(i18n("Manage Observers"));
    execB->setToolTip(i18n("Save and End the current session"));
    buttonBox->addButton(execB, QDialogButtonBox::ActionRole);
    buttonBox->addButton(addObs, QDialogButtonBox::ActionRole);
    connect(execB, SIGNAL(clicked()), this, SLOT(slotEndSession()));
    connect(addObs,SIGNAL(clicked()),this,SLOT(slotObserverAdd()));

    setWindowTitle( i18n( "Execute Session" ) );

    currentTarget = NULL;
    currentObserver = NULL;
    currentScope = NULL;
    currentEyepiece = NULL;
    currentLens = NULL;
    currentFilter = NULL;
    currentSession = NULL;
    nextSession = 0;
    nextObservation = 0;
    nextSite = 0;

    //initialize the global logObject
    logObject = KStarsData::Instance()->logObject();

    //initialize the lists and parameters
    init();
    ui.Target->hide();
    ui.AddObject->hide();
    ui.RemoveObject->hide();
    ui.NextButton->hide();
    ui.NextButton->setEnabled( false );
    ui.Slew->setEnabled( false );

    //make connections
    connect( ui.NextButton, SIGNAL( clicked() ),
             this, SLOT( slotNext() ) );
    connect( ui.Slew, SIGNAL( clicked() ),
             this, SLOT( slotSlew() ) );
    connect( ui.Location, SIGNAL( clicked() ),
             this, SLOT( slotLocation() ) );
    connect( ui.Target, SIGNAL( currentTextChanged(const QString) ),
             this, SLOT( slotSetTarget(QString) ) );
    connect( ui.SessionURL, SIGNAL( leftClickedUrl() ),
             this, SLOT( slotShowSession() ) );
    connect( ui.ObservationsURL, SIGNAL( leftClickedUrl() ),
             this, SLOT( slotShowTargets() ) );
    connect( ui.AddObject, SIGNAL( leftClickedUrl() ),
             this, SLOT( slotAddObject() ) );
    connect( ui.RemoveObject, SIGNAL( leftClickedUrl() ),
             this, SLOT( slotRemoveObject() ) );
}

void Execute::init() {
    //initialize geo to current location of the ObservingList
    geo = KStarsData::Instance()->geo();
    ui.Location->setText( geo->fullName() );


    // JM: Aren't we suppose to take KStars time? The one returned by the OL is the time of the LAST object
    // in the list which doesn't make sense.

    /*
    //set the date time to the dateTime from the OL
    ui.Begin->setDateTime( ks->observingList()->dateTime() );
    */
    ui.Begin->setDateTime(KStarsData::Instance()->geo()->UTtoLT(KStarsData::Instance()->clock()->utc()));

    KStarsData::Instance()->logObject()->readAll();

    //load Targets
    loadTargets();

    //load Equipment
    loadEquipment();

    //load Observers
    loadObservers();

    if( logObject->scopeList()->isEmpty() || logObject->observerList()->isEmpty() ) {
        ui.hintLabel->show();
    }
    else {
        ui.hintLabel->hide();
    }

    //set Current Items
    loadCurrentItems();
}
void Execute::loadCurrentItems() {
    //Set the current target, equipments and observer
    if( currentTarget )
        ui.Target->setCurrentRow( findIndexOfTarget( currentTarget->name() ), QItemSelectionModel::SelectCurrent );
    else
        ui.Target->setCurrentRow( 0, QItemSelectionModel::SelectCurrent );

    if( currentObserver )
        ui.Observer->setCurrentIndex( ui.Observer->findText( currentObserver->name() + ' ' + currentObserver->surname() ) );
    if( currentScope )
        ui.Scope->setCurrentIndex( ui.Scope->findText( currentScope->name()) );
    if( currentEyepiece )
        ui.Eyepiece->setCurrentIndex( ui.Eyepiece->findText( currentEyepiece->name()) );
    if( currentLens )
        ui.Lens->setCurrentIndex( ui.Lens->findText( currentLens->name()) );
    if( currentFilter )
        ui.Filter->setCurrentIndex( ui.Filter->findText( currentFilter->name()) );
}

int Execute::findIndexOfTarget( QString name ) {
    for( int i = 0; i < ui.Target->count(); i++ )
        if( ui.Target->item( i )->text() == name )
            return i;
    return -1;
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
                addObservation();
                ui.stackedWidget->setCurrentIndex( 1 );
                ui.NextButton->setText( i18n( "Next Page >" ) );
                QString prevTarget = currentTarget->name();
                loadTargets();
                ui.Target->setCurrentRow( findIndexOfTarget( prevTarget ), QItemSelectionModel::SelectCurrent );
                selectNextTarget();
            break;
        }
    }
}

bool Execute::saveSession() {
    OAL::Site *site = logObject->findSiteByName( geo->fullName() );
    if( ! site ) {
        while( logObject->findSiteById( i18n( "site_" ) + QString::number( nextSite ) ) )
            nextSite++;
        site = new OAL::Site( geo, i18n( "site_" ) + QString::number( nextSite++ ) );
        logObject->siteList()->append( site );
    }
    if( currentSession ){
            currentSession->setSession( currentSession->id(), site->id(), ui.Begin->dateTime(), ui.Begin->dateTime(), ui.Weather->toPlainText(), ui.Equipment->toPlainText(), ui.Comment->toPlainText(), ui.Language->text() );
    } else {
        while( logObject->findSessionByName( i18n( "session_" ) + QString::number( nextSession ) ) )
            nextSession++;
        currentSession = new OAL::Session( i18n( "session_" ) + QString::number( nextSession++ ) , site->id(), ui.Begin->dateTime(), ui.Begin->dateTime(), ui.Weather->toPlainText(), ui.Equipment->toPlainText(), ui.Comment->toPlainText(), ui.Language->text() );
        logObject->sessionList()->append( currentSession );
    }
    ui.stackedWidget->setCurrentIndex( 1 ); //Move to the next page
    return true;
}

void Execute::slotLocation() {
    QPointer<LocationDialog> ld = new LocationDialog( this );
    if ( ld->exec() == QDialog::Accepted ) {
        geo = ld->selectedCity();
        ui.Location->setText( geo -> fullName() );
    }
    delete ld;
}

void Execute::loadTargets() {
    ui.Target->clear();
    sortTargetList();
    foreach( SkyObject *o, KStarsData::Instance()->observingList()->sessionList() ) {
        ui.Target->addItem( getObjectName(o, false) );
    }
}

void Execute::loadEquipment() {
    ui.Scope->clear();
    ui.Eyepiece->clear();
    ui.Lens->clear();
    ui.Filter->clear();
    foreach( OAL::Scope *s, *( logObject->scopeList() ) )
        ui.Scope->addItem( s->name() );
    foreach( OAL::Eyepiece *e, *( logObject->eyepieceList() ) )
        ui.Eyepiece->addItem( e->name() );
    foreach( OAL::Lens *l, *( logObject->lensList() ) )
        ui.Lens->addItem( l->name() );
    foreach( OAL::Filter *f, *( logObject->filterList() ) )
        ui.Filter->addItem( f->name() );
}

void Execute::loadObservers() {
    ui.Observer->clear();
    foreach( OAL::Observer *o,*( logObject->observerList() ) )
        ui.Observer->addItem( o->name() + ' ' + o->surname() );
}

void Execute::sortTargetList() {
    qSort( KStarsData::Instance()->observingList()->sessionList().begin(), KStarsData::Instance()->observingList()->sessionList().end(), Execute::timeLessThan );
}

bool Execute::timeLessThan ( SkyObject *o1, SkyObject *o2 )
{
    QTime t1 = KStarsData::Instance()->observingList()->scheduledTime( o1 );
    QTime t2 = KStarsData::Instance()->observingList()->scheduledTime( o2 );

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
    if( ! ui.Target->count() )
        return;
    SkyObject *o = KStarsData::Instance()->observingList()->findObjectByName( ui.Target->currentItem()->text() );
    if( o ) {
        currentTarget = o;
        o->setNotes( ui.Notes->toPlainText() );
        ui.Notes->clear();
        loadObservationTab();
    }
}

void Execute::loadObservationTab() {
   ui.Time->setTime( KStarsDateTime::currentDateTime().time() );
   ui.stackedWidget->setCurrentIndex( 2 );
   ui.NextButton->setText( i18n( "Next Target >" ) );
}

bool Execute::addObservation() {
    slotSetCurrentObjects();
    while( logObject->findObservationByName( i18n( "observation_" ) + QString::number( nextObservation ) ) )
        nextObservation++;
    KStarsDateTime dt = currentSession->begin();
    dt.setTime( ui.Time->time() );
    OAL::Observation *o = new OAL::Observation( i18n( "observation_" ) + QString::number( nextObservation++ ) , currentObserver, currentSession, currentTarget, dt, ui.FaintestStar->value(), ui.Seeing->value(), currentScope, currentEyepiece, currentLens, currentFilter, ui.Description->toPlainText(), ui.Language->text() );
        logObject->observationList()->append( o );
    ui.Description->clear();
    return true;
}
void Execute::slotEndSession() {
    if( currentSession ) {

        currentSession->setSession( currentSession->id(), currentSession->site(), ui.Begin->dateTime(),
                                    KStarsDateTime::currentDateTime(), ui.Weather->toPlainText(), ui.Equipment->toPlainText(),
                                    ui.Comment->toPlainText(), ui.Language->text() );

        QUrl fileURL = QFileDialog::getSaveFileUrl(0, i18n("Save Session"), QUrl(QDir::homePath()), "*.xml" );

        if( fileURL.isEmpty() ) {
            // Cancel
            return;
        }

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

    }
    hide();
    ui.stackedWidget->setCurrentIndex(0);
    logObject->observationList()->clear();
    logObject->sessionList()->clear();
    delete currentSession;
    currentTarget = NULL;
    currentSession = NULL;
}
void Execute::slotObserverAdd() {
    QPointer<ObserverAdd> m_observerAdd = new ObserverAdd();
    m_observerAdd->exec();
    delete m_observerAdd;
}

void Execute::slotSetTarget( QString name ) {
    currentTarget = KStarsData::Instance()->observingList()->findObjectByName( name );
    if( ! currentTarget ) {
        ui.NextButton->setEnabled( false );
        ui.Slew->setEnabled( false );
        return;
    } else {
        ui.NextButton->setEnabled( true );
        ui.Slew->setEnabled( true );
        KStarsData::Instance()->observingList()->selectObject( currentTarget );
        KStarsData::Instance()->observingList()->slotCenterObject();
        QString smag = "--";
        if (  - 30.0 < currentTarget->mag() && currentTarget->mag() < 90.0 ) smag = QString::number( currentTarget->mag(), 'g', 2 ); // The lower limit to avoid display of unrealistic comet magnitudes
        ui.Mag->setText( smag );
        ui.Type->setText( currentTarget->typeName() );
        ui.SchTime->setText( KStarsData::Instance()->observingList()->scheduledTime(currentTarget).toString( "h:mm:ss AP" ) ) ;
        SkyPoint p = currentTarget->recomputeCoords( KStarsDateTime::currentDateTime() , geo );
        dms lst(geo->GSTtoLST( KStarsDateTime::currentDateTime().gst() ));
        p.EquatorialToHorizontal( &lst, geo->lat() );
        ui.RA->setText( p.ra().toHMSString() ) ;
        ui.Dec->setText( p.dec().toDMSString() );
        ui.Alt->setText( p.alt().toDMSString() );
        ui.Az->setText( p.az().toDMSString() );
        ui.Notes->setText( currentTarget->notes() );
    }
}

void Execute::slotSlew() {
    KStarsData::Instance()->observingList()->slotSlewToObject();
}

void Execute::selectNextTarget() {
    int i = findIndexOfTarget( currentTarget->name() ) + 1;
    if( i < ui.Target->count() ) {
        ui.Target->selectionModel()->clear();
        ui.Target->setCurrentRow( i, QItemSelectionModel::SelectCurrent );
    }
}

void Execute::slotSetCurrentObjects() {
    currentScope = logObject->findScopeByName( ui.Scope->currentText() );
    currentEyepiece = logObject->findEyepieceByName( ui.Eyepiece->currentText() );
    currentLens = logObject->findLensByName( ui.Lens->currentText() );
    currentFilter = logObject->findFilterByName( ui.Filter->currentText() );
    currentObserver = logObject->findObserverByName( ui.Observer->currentText() );
}

void Execute::slotShowSession() {
    ui.Target->hide();
    ui.stackedWidget->setCurrentIndex( 0 );
    ui.NextButton->hide();
    ui.AddObject->hide();
    ui.RemoveObject->hide();
}

void Execute::slotShowTargets() {
    if( saveSession() ) {
        ui.Target->show();
        ui.AddObject->show();
        ui.RemoveObject->show();
        ui.stackedWidget->setCurrentIndex( 1 );
        ui.NextButton->show();
        ui.NextButton->setText( i18n( "Next Page >" ) );
    }
}

void Execute::slotAddObject() {
   QPointer<FindDialog> fd = new FindDialog( KStars::Instance() );
   if ( fd->exec() == QDialog::Accepted ) {
       SkyObject *o = fd->selectedObject();
       if( o != 0 ) {
           KStarsData::Instance()->observingList()->slotAddObject( o, true );
           init();
       }
   }
   delete fd;
}

void Execute::slotRemoveObject()
{
	QModelIndex i = ui.Target->currentIndex();
	SkyObject *obj = 0;
	if( i.isValid() )
	{
		QString ObjName = i.data().toString();
		obj = KStarsData::Instance()->skyComposite()->findByName( ObjName );
	}

	if( obj!=0 )
	{
        KStarsData::Instance()->observingList()->slotRemoveObject( obj, true );
		loadTargets();
	}
}

QString Execute::getObjectName(const SkyObject *o, bool translated)
{
    QString finalObjectName;
    if( o->name() == "star" )
    {
        StarObject *s = (StarObject *)o;

        // JM: Enable HD Index stars to be added to the observing list.
        if( s->getHDIndex() != 0 )
                finalObjectName = QString("HD %1" ).arg( QString::number( s->getHDIndex() ) );
    }
    else
         finalObjectName = translated ? o->translatedName() : o->name();

    return finalObjectName;

}



