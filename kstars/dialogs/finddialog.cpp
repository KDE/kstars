/***************************************************************************
                          finddialog.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Wed Jul 4 2001
    copyright            : (C) 2001 by Jason Harris
    email                : jharris@30doradus.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "finddialog.h"

#include "kstarsdata.h"
#include "Options.h"
#include "detaildialog.h"
#include "skyobjects/skyobject.h"
#include "skycomponents/starcomponent.h"
#include "skycomponents/skymapcomposite.h"

#include <kmessagebox.h>

#include <QSortFilterProxyModel>
#include <QStringListModel>
#include <QTimer>

FindDialogUI::FindDialogUI( QWidget *parent ) : QFrame( parent ) {
    setupUi( this );

    FilterType->addItem( xi18n ("Any") );
    FilterType->addItem( xi18n ("Stars") );
    FilterType->addItem( xi18n ("Solar System") );
    FilterType->addItem( xi18n ("Open Clusters") );
    FilterType->addItem( xi18n ("Globular Clusters") );
    FilterType->addItem( xi18n ("Gaseous Nebulae") );
    FilterType->addItem( xi18n ("Planetary Nebulae") );
    FilterType->addItem( xi18n ("Galaxies") );
    FilterType->addItem( xi18n ("Comets") );
    FilterType->addItem( xi18n ("Asteroids") );
    FilterType->addItem( xi18n ("Constellations") );
    FilterType->addItem( xi18n ("Supernovae") );

    SearchList->setMinimumWidth( 256 );
    SearchList->setMinimumHeight( 320 );
}

FindDialog::FindDialog( QWidget* parent ) :
    QDialog( parent ),
    timer(0)
{
    ui = new FindDialogUI( this );

    setWindowTitle( xi18n( "Find Object" ) );

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(ui);
    setLayout(mainLayout);

    //FIXME Need porting to KF5
    //setMainWidget( ui );
    //setButtons( QDialog::Ok|QDialog::User1|QDialog::Cancel );

    ui->FilterType->setCurrentIndex(0);  // show all types of objects

    fModel = new QStringListModel( this );
    sortModel = new QSortFilterProxyModel( ui->SearchList );
    sortModel->setFilterCaseSensitivity( Qt::CaseInsensitive );
    ui->SearchList->setModel( sortModel );
    sortModel->setSourceModel( fModel );
    ui->SearchList->setModel( sortModel );

    //FIXME Need porting to KF5
    //setButtonText(QDialog::User1, xi18n("Details..."));

    // Connect signals to slots
    connect( this, SIGNAL( okClicked() ), this, SLOT( slotOk() ) );
    connect( this, SIGNAL( cancelClicked() ), this, SLOT( reject() ) );
    connect(this, SIGNAL(user1Clicked()), this, SLOT(slotDetails()));
    connect( ui->SearchBox, SIGNAL( textChanged( const QString & ) ), SLOT( enqueueSearch() ) );
    connect( ui->SearchBox, SIGNAL( returnPressed() ), SLOT( slotOk() ) );
    connect( ui->FilterType, SIGNAL( activated( int ) ), this, SLOT( enqueueSearch() ) );
    connect( ui->SearchList, SIGNAL( doubleClicked( const QModelIndex & ) ), SLOT( slotOk() ) );

    // Set focus to object name edit
    ui->SearchBox->setFocus();

    // First create and paint dialog and then load list
    QTimer::singleShot(0, this, SLOT( init() ));

    listFiltered = false;
}

FindDialog::~FindDialog() { }

void FindDialog::init() {
    ui->SearchBox->clear();
    filterByType();
    sortModel->sort( 0 );
    initSelection();
}

void FindDialog::initSelection() {
    //FIXME Need porting to KF5
    /*
    if ( sortModel->rowCount() <= 0 ) {
        button( Ok )->setEnabled( false );
        return;
    }*/

    if ( ui->SearchBox->text().isEmpty() ) {
        //Pre-select the first item
        QModelIndex selectItem = sortModel->index( 0, sortModel->filterKeyColumn(), QModelIndex() );
        switch ( ui->FilterType->currentIndex() ) {
        case 0: //All objects, choose Andromeda galaxy
            {
                QModelIndex qmi = fModel->index( fModel->stringList().indexOf( xi18n("Andromeda Galaxy") ) );
                selectItem = sortModel->mapFromSource( qmi );
                break;
            }
        case 1: //Stars, choose Aldebaran
            {
                QModelIndex qmi = fModel->index( fModel->stringList().indexOf( xi18n("Aldebaran") ) );
                selectItem = sortModel->mapFromSource( qmi );
                break;
            }
        case 2: //Solar system or Asteroids, choose Aaltje
        case 9:
            {
                QModelIndex qmi = fModel->index( fModel->stringList().indexOf( xi18n("Aaltje") ) );
                selectItem = sortModel->mapFromSource( qmi );
                break;
            }
        case 8: //Comets, choose 'Aarseth-Brewington (1989 W1)'
            {
                QModelIndex qmi = fModel->index( fModel->stringList().indexOf( xi18n("Aarseth-Brewington (1989 W1)") ) );
                selectItem = sortModel->mapFromSource( qmi );
                break;
            }

        }

        if ( selectItem.isValid() ) {
            ui->SearchList->selectionModel()->select( selectItem, QItemSelectionModel::ClearAndSelect );
            ui->SearchList->scrollTo( selectItem );
            ui->SearchList->setCurrentIndex( selectItem );

            //FIXME Need porting to KF5
            //button( Ok )->setEnabled( true );
        }
    }

    listFiltered = true;
}

void FindDialog::filterByType() {
    KStarsData *data = KStarsData::Instance();

    switch ( ui->FilterType->currentIndex() ) {
    case 0: // All object types
        {
            QStringList allObjects;
            foreach( int type, data->skyComposite()->objectNames().keys() )
                allObjects += data->skyComposite()->objectNames( type );
            fModel->setStringList( allObjects );
            break;
        }
    case 1: //Stars
        {
            QStringList starObjects;
            starObjects += data->skyComposite()->objectNames( SkyObject::STAR );
            starObjects += data->skyComposite()->objectNames( SkyObject::CATALOG_STAR );
            fModel->setStringList( starObjects );
            break;
        }
    case 2: //Solar system
        {
            QStringList ssObjects;
            ssObjects += data->skyComposite()->objectNames( SkyObject::PLANET );
            ssObjects += data->skyComposite()->objectNames( SkyObject::COMET );
            ssObjects += data->skyComposite()->objectNames( SkyObject::ASTEROID );
            ssObjects += data->skyComposite()->objectNames( SkyObject::MOON );
            ssObjects += xi18n("Sun");
            fModel->setStringList( ssObjects );
            break;
        }
    case 3: //Open Clusters
        fModel->setStringList( data->skyComposite()->objectNames( SkyObject::OPEN_CLUSTER ) );
        break;
    case 4: //Open Clusters
        fModel->setStringList( data->skyComposite()->objectNames( SkyObject::GLOBULAR_CLUSTER ) );
        break;
    case 5: //Gaseous nebulae
        fModel->setStringList( data->skyComposite()->objectNames( SkyObject::GASEOUS_NEBULA ) );
        break;
    case 6: //Planetary nebula
        fModel->setStringList( data->skyComposite()->objectNames( SkyObject::PLANETARY_NEBULA ) );
        break;
    case 7: //Galaxies
        fModel->setStringList( data->skyComposite()->objectNames( SkyObject::GALAXY ) );
        break;
    case 8: //Comets
        fModel->setStringList( data->skyComposite()->objectNames( SkyObject::COMET ) );
        break;
    case 9: //Asteroids
        fModel->setStringList( data->skyComposite()->objectNames( SkyObject::ASTEROID ) );
        break;
    case 10: //Constellations
        fModel->setStringList( data->skyComposite()->objectNames( SkyObject::CONSTELLATION ) );
        break;
    case 11: //Supernovae
        fModel->setStringList( data->skyComposite()->objectNames( SkyObject::SUPERNOVA ) );
        break;
    }
}

void FindDialog::filterList() {  
    QString SearchText;
    SearchText = processSearchText();
    sortModel->setFilterFixedString( SearchText );
    filterByType();
    initSelection();

    //Select the first item in the list that begins with the filter string
    if ( !SearchText.isEmpty() ) {
        QStringList mItems = fModel->stringList().filter( QRegExp( '^'+SearchText, Qt::CaseInsensitive ) );
        mItems.sort();
    
        if ( mItems.size() ) {
            QModelIndex qmi = fModel->index( fModel->stringList().indexOf( mItems[0] ) );
            QModelIndex selectItem = sortModel->mapFromSource( qmi );
    
            if ( selectItem.isValid() ) {
                ui->SearchList->selectionModel()->select( selectItem, QItemSelectionModel::ClearAndSelect );
                ui->SearchList->scrollTo( selectItem );
                ui->SearchList->setCurrentIndex( selectItem );

                //FIXME Need porting to KF5
                //button( Ok )->setEnabled( true );
            }
        }
    }

    listFiltered = true;
}

SkyObject* FindDialog::selectedObject() const {
    QModelIndex i = ui->SearchList->currentIndex();
    SkyObject *obj = 0;
    if ( i.isValid() ) {
        QString ObjName = i.data().toString();
        obj = KStarsData::Instance()->skyComposite()->findByName( ObjName );
    }
    if( !obj ) {
        QString stext = ui->SearchBox->text();
        if( stext.startsWith( QLatin1String( "HD" ) ) ) {
            stext.remove( "HD" );
            bool ok;
            int HD = stext.toInt( &ok );
            // Looks like the user is looking for a HD star
            if( ok ) {
                obj = StarComponent::Instance()->findByHDIndex( HD );
            }
        }
    }
    return obj;
}

void FindDialog::enqueueSearch() {
    listFiltered = false;
    if ( timer ) {
        timer->stop();
    } else {
        timer = new QTimer( this );
        timer->setSingleShot( true );
        connect( timer, SIGNAL( timeout() ), this, SLOT( filterList() ) );
    }
    timer->start( 500 );
}

// Process the search box text to replace equivalent names like "m93" with "m 93"
QString FindDialog::processSearchText() {
    QRegExp re;
    QString searchtext = ui->SearchBox->text();

    re.setCaseSensitivity( Qt::CaseInsensitive ); 

    // If it is an NGC/IC/M catalog number, as in "M 76" or "NGC 5139", check for absence of the space
    re.setPattern("^(m|ngc|ic)\\s*\\d*$");
    if(ui->SearchBox->text().contains(re)) {
        re.setPattern("\\s*(\\d+)");
        searchtext.replace( re, " \\1" );
        re.setPattern("\\s*$");
        searchtext.remove( re );
        re.setPattern("^\\s*");
        searchtext.remove( re );
    }

    // TODO after KDE 4.1 release:
    // If it is a IAU standard three letter abbreviation for a constellation, then go to that constellation
    // Check for genetive names of stars. Example: alp CMa must go to alpha Canis Majoris

    return searchtext;
}

void FindDialog::slotOk() {
    //If no valid object selected, show a sorry-box.  Otherwise, emit accept()
    SkyObject *selObj;
    if(!listFiltered) {
        filterList();
    }
    selObj = selectedObject();
    if ( selObj == 0 ) {
        QString message = xi18n( "No object named %1 found.", ui->SearchBox->text() );
        KMessageBox::sorry( 0, message, xi18n( "Bad object name" ) );
    } else {
        accept();
    }
}

void FindDialog::keyPressEvent( QKeyEvent *e ) {
    switch( e->key() ) {
    case Qt::Key_Escape :
        reject();
        break;
    case Qt::Key_Up :
    {
        int currentRow = ui->SearchList->currentIndex().row();
        if ( currentRow > 0 ) {
            QModelIndex selectItem = sortModel->index( currentRow-1, sortModel->filterKeyColumn(), QModelIndex() );
            ui->SearchList->selectionModel()->setCurrentIndex( selectItem, QItemSelectionModel::SelectCurrent );
        }
        break;
    }
    case Qt::Key_Down :
    {
        int currentRow = ui->SearchList->currentIndex().row();
        if ( currentRow < sortModel->rowCount()-1 ) {
            QModelIndex selectItem = sortModel->index( currentRow+1, sortModel->filterKeyColumn(), QModelIndex() );
            ui->SearchList->selectionModel()->setCurrentIndex( selectItem, QItemSelectionModel::SelectCurrent );
        }
        break;
    }
    }
}

void FindDialog::slotDetails()
{
    if ( selectedObject() ) {
        QPointer<DetailDialog> dd = new DetailDialog( selectedObject(), KStarsData::Instance()->lt(), KStarsData::Instance()->geo(), KStars::Instance());
        dd->exec();
        delete dd;
    }

}

#include "finddialog.moc"
