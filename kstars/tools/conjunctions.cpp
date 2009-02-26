/***************************************************************************
                          conjunctions.cpp  -  Conjunctions Tool
                             -------------------
    begin                : Sun 20th Apr 2008
    copyright            : (C) 2008 Akarsh Simha
    email                : akarshsimha@gmail.com
***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 * Much of the code here is taken from Pablo de Vicente's                  *
 * modcalcplanets.cpp                                                      *
 *                                                                         *
 ***************************************************************************/

#include "conjunctions.h"

#include <KGlobal>
#include <KLocale>
#include <kfiledialog.h>
#include <kmessagebox.h>

#include "ksconjunct.h"
#include "geolocation.h"
#include "dialogs/locationdialog.h"
#include "dms.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "ksnumbers.h"
#include "skyobjects/kssun.h"
#include "skyobjects/ksplanet.h"
#include "skyobjects/ksplanetbase.h"
#include "skyobjects/ksmoon.h"
#include "skyobjects/kspluto.h"
#include "widgets/dmsbox.h"
#include "dialogs/finddialog.h"
#include "skyobjects/kscomet.h"
#include "skyobjects/ksasteroid.h"
#include "skymap.h"
#include "infoboxes.h"

ConjunctionsTool::ConjunctionsTool(QWidget *parentSplit)
    : QFrame(parentSplit), Object1( 0 ), Object2( 0 ) {

    setupUi(this);

    /*
      OutputView -> setTableFlags( Tbl_autoScrollBars|Tbl_clipCellPainting );
      OutputView -> setAutoUpdate( TRUE );
      OutputView -> setNumRows( 0 );
      OutputView -> setNumCols( 2 );
      OutputView -> selected.setX( -1 );
      OutputView -> selected.setY( -1 );
    */

    KStarsData *kd = KStarsData::Instance();
    KStarsDateTime dtStart ( KStarsDateTime::currentDateTime() );
    KStarsDateTime dtStop ( dtStart.djd() + 365.24 ); // TODO: Refine

    startDate -> setDateTime( dtStart.dateTime() );
    stopDate -> setDateTime( dtStop.dateTime() );

    geoPlace = kd -> geo();
    LocationButton -> setText( geoPlace -> fullName() );

    pNames[KSPlanetBase::MERCURY] = i18n("Mercury");
    pNames[KSPlanetBase::VENUS] = i18n("Venus");
    pNames[KSPlanetBase::MARS] = i18n("Mars");
    pNames[KSPlanetBase::JUPITER] = i18n("Jupiter");
    pNames[KSPlanetBase::SATURN] = i18n("Saturn");
    pNames[KSPlanetBase::URANUS] = i18n("Uranus");
    pNames[KSPlanetBase::NEPTUNE] = i18n("Neptune");
    pNames[KSPlanetBase::PLUTO] = i18n("Pluto");
    pNames[KSPlanetBase::SUN] = "Sun";
    pNames[KSPlanetBase::MOON] = "Moon";

    for ( int i=0; i<KSPlanetBase::UNKNOWN_PLANET; ++i ) {
        //      Obj1ComboBox->insertItem( i, pNames[i] );
        Obj2ComboBox->insertItem( i, pNames[i] );
    }

    // Initialize the Maximum Separation box to 1 degree
    maxSeparationBox->setDegType( true );
    maxSeparationBox->setDMS( "01 00 00.0" );

    // signals and slots connections
    connect(LocationButton, SIGNAL(clicked()), this, SLOT(slotLocation()));
    connect(Obj1FindButton, SIGNAL(clicked()), this, SLOT(slotFindObject()));
    connect(ComputeButton, SIGNAL(clicked()), this, SLOT(slotCompute()));
    connect( OutputView, SIGNAL( itemDoubleClicked( QListWidgetItem * ) ), this, SLOT( slotGoto() ) );

    show();
}

ConjunctionsTool::~ConjunctionsTool(){
    if( Object1 )
        delete Object1;
    if( Object2 )
        delete Object2;
}

void ConjunctionsTool::slotGoto() {
    int index = OutputView->currentRow();
    long double jd = outputJDList.value( index );
    KStarsDateTime dt;
    KStars *ks= (KStars *) topLevelWidget()->parent();
    KStarsData *data = KStarsData::Instance();
    SkyMap *map = ks->map();

    data->setLocation( *geoPlace );
    ks->infoBoxes()->geoChanged( geoPlace );
    dt.setDJD( jd );
    data->changeDateTime( dt );
    map->setClickedObject( data->skyComposite()->findByName( Object1->name() ) ); // This is required, because the Object1 we have is a copy
    map->setClickedPoint( map->clickedObject() );
    map->slotCenter();
}

void ConjunctionsTool::slotFindObject() {
    FindDialog fd( (KStars*) topLevelWidget()->parent() );
    if ( fd.exec() == QDialog::Accepted ) {
        if( Object1 )
            delete Object1;
        if( !fd.selectedObject() )
            return;
        if( !fd.selectedObject()->isSolarSystem() ) {
            Object1 = new SkyObject( *fd.selectedObject() );
        }
        else {
            switch( fd.selectedObject()->type() ) {
            case 9: {
                Object1 =  new KSComet( (KSComet &) *fd.selectedObject() );
                break;
            }
            case 10: {
                Object1 =  new KSAsteroid( (KSAsteroid &) *fd.selectedObject() );
                break;
            }
            case 2: 
            default: {
                Object1 = KSPlanetBase::createPlanet( pNames.key( fd.selectedObject()->name() ) ); // TODO: Fix i18n issues.
                break;
            }
            }
        }
        if( Object1 )
            Obj1FindButton->setText( Object1->name() );
    }
}

void ConjunctionsTool::slotLocation()
{
    LocationDialog ld( (KStars*) topLevelWidget()->parent() );

    if ( ld.exec() == QDialog::Accepted ) {
        geoPlace = ld.selectedCity();
        LocationButton -> setText( geoPlace -> fullName() );
    }
}

void ConjunctionsTool::slotCompute (void)
{

    KStarsDateTime dtStart = startDate -> dateTime();
    KStarsDateTime dtStop = stopDate -> dateTime();
    long double startJD = dtStart.djd();
    long double stopJD = dtStop.djd();
    dms maxSeparation( 0.0 );
    bool ok;
    maxSeparation = maxSeparationBox->createDms( true, &ok );
    if( !ok ) {
        KMessageBox::sorry( 0, i18n("Maximum separation entered is not a valid angle. Use the What's this help feature for information on how to enter a valid angle") );
        return;
    }
    if( !Object1 ) {
        KMessageBox::sorry( 0, i18n("Please select an object to check conjunctions with, by clicking on the \'Find Object\' button.") );
        return;
    }
    Object2 = KSPlanetBase::createPlanet( Obj2ComboBox->currentIndex() );
    if( Object1->name() == Object2->name() ) {
    	KMessageBox::sorry( 0 , i18n("Please select two different objects to check conjunctions with.") );
    	return;
    }
    QApplication::setOverrideCursor( QCursor(Qt::WaitCursor) );
    KSConjunct ksc;
    ComputeStack->setCurrentIndex( 1 );
    connect( &ksc, SIGNAL(madeProgress(int)), this, SLOT(showProgress(int)) );

    showConjunctions( ksc.findClosestApproach(*Object1, *Object2, startJD, stopJD, maxSeparation) );

    ComputeStack->setCurrentIndex( 0 );
    QApplication::restoreOverrideCursor();

    delete Object2;
    Object2 = NULL;

}

void ConjunctionsTool::showProgress(int n) {
    progress->setValue( n );
}

void ConjunctionsTool::showConjunctions(const QMap<long double, dms> &conjunctionlist) {

    KStarsDateTime dt;
    QMap<long double, dms>::ConstIterator it;
    int i;

    OutputView->clear();
    outputJDList.clear();
    i = 0;

    for(it = conjunctionlist.constBegin(); it != conjunctionlist.constEnd(); ++it) {
        dt.setDJD( it.key() );
        OutputView -> addItem( i18n("Conjunction on %1 UT: Separation is %2", 
                                    KGlobal::locale()->formatDateTime( dt, KLocale::LongDate ), 
                                    it.value().toDMSString()) );
        outputJDList.insert( i, it.key() );
        ++i;
    }
}

