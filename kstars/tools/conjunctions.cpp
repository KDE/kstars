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

#include <kfiledialog.h>
#include <kglobal.h>
#include <kmessagebox.h>

#include "ksconjunct.h"
#include "geolocation.h"
#include "locationdialog.h"
#include "dms.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "ksnumbers.h"
#include "kssun.h"
#include "ksplanet.h"
#include "ksmoon.h"
#include "kspluto.h"
#include "widgets/dmsbox.h"

ConjunctionsTool::ConjunctionsTool(QWidget *parentSplit) 
  : QFrame(parentSplit) {

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
  
  // signals and slots connections
  connect(LocationButton, SIGNAL(clicked()), this, SLOT(slotLocation()));
  connect(ComputeButton, SIGNAL(clicked()), this, SLOT(slotCompute()));
  
  show();
}

ConjunctionsTool::~ConjunctionsTool(){
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
  dms maxSeparation(1.0); // TODO: Make maxSeparation user-specifiable
  // TODO: Get geoPlace from user.
  //    dms LST( geoPlace->GSTtoLST( dt.gst() ) );
  KSPlanetBase *Object1, *Object2;
  
  Object1 = createPlanetFromIndex( Obj1ComboBox -> currentIndex() );
  Object2 = createPlanetFromIndex( Obj2ComboBox -> currentIndex() );
  
  KSConjunct ksc;
  showConjunctions(ksc.findClosestApproach(*Object1, *Object2, startJD, stopJD, maxSeparation));

  delete Object1;
  delete Object2;

}

KSPlanetBase* ConjunctionsTool::createPlanetFromIndex( int i ) {
    KSPlanetBase *Object;
    KStarsData *kd = KStarsData::Instance();

    switch ( i ) {
        case 0:
            Object = (KSPlanetBase *)(new KSPlanet( kd, "Mercury" ));
            break;
        case 1:
            Object = (KSPlanetBase *)(new KSPlanet( kd, "Venus" ));
            break;
        case 2:
            Object = (KSPlanetBase *)(new KSPlanet( kd, "Mars" ));
            break;
        case 3:
            Object = (KSPlanetBase *)(new KSPlanet( kd, "Jupiter" ));
            break;
        case 4:
            Object = (KSPlanetBase *)(new KSPlanet( kd, "Saturn" ));
            break;
        case 5:
            Object = (KSPlanetBase *)(new KSPlanet( kd, "Uranus" ));
            break;
        case 6:
            Object = (KSPlanetBase *)(new KSPlanet( kd, "Neptune" ));
            break;
        case 7:
            Object = (KSPlanetBase *)(new KSPluto( kd ));
            break;
        case 8:
            Object = (KSPlanetBase *)(new KSSun( kd ));
            break;
        case 9:
            Object = (KSPlanetBase *)(new KSMoon( kd ));
            break;
    }

    return Object;
}

void ConjunctionsTool::showConjunctions(QMap<long double, dms> conjunctionlist) {

  KStarsDateTime dt;
  QMap<long double, dms>::Iterator it;

  OutputView->clear();

  for(it = conjunctionlist.begin(); it != conjunctionlist.end(); ++it) {
    dt.setDJD( it.key() );
    OutputView -> addItem( i18n("Conjunction on %1 UT: Separation is %2", dt.toString("%a, %d %b %Y %H:%M"), it.data().toDMSString()) );
  }
}

