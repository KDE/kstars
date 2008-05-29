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
  KStarsData *kd = KStarsData::Instance();
  long double startJD = dtStart.djd();
  long double stopJD = dtStop.djd();
  dms maxSeparation(1.0); // TODO: Make maxSeparation user-specifiable
  // TODO: Get geoPlace from user.
  //    dms LST( geoPlace->GSTtoLST( dt.gst() ) );
  KSPlanetBase *Object1, *Object2;
  
  if(Obj1ComboBox -> currentIndex() <= 6)
    Object1 = (KSPlanetBase *)(new KSPlanet(kd, I18N_NOOP(Obj1ComboBox -> currentText())));
  else if(Obj1ComboBox -> currentIndex() == 7)
    Object1 = (KSPlanetBase *)(new KSPluto(kd));
  else if(Obj1ComboBox -> currentIndex() == 8)
    Object1 = (KSPlanetBase *)(new KSMoon(kd));
  else if(Obj1ComboBox -> currentIndex() == 9)
    Object1 = (KSPlanetBase *)(new KSSun(kd));

  if(Obj2ComboBox -> currentIndex() <= 6)
    Object2 = (KSPlanetBase *)(new KSPlanet(kd, I18N_NOOP(Obj2ComboBox -> currentText())));
  else if(Obj2ComboBox -> currentIndex() == 7)
    Object2 = (KSPlanetBase *)(new KSPluto(kd));
  else if(Obj2ComboBox -> currentIndex() == 8)
    Object2 = (KSPlanetBase *)(new KSMoon(kd));
  else if(Obj2ComboBox -> currentIndex() == 9)
    Object2 = (KSPlanetBase *)(new KSSun(kd));

  KSConjunct ksc;
  showConjunctions(ksc.findClosestApproach(*Object1, *Object2, startJD, stopJD, maxSeparation));

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

