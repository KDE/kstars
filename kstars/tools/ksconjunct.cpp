/***************************************************************************
                          ksconjunct.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Sat Mar 22 2008
    copyright            : (C) 2008 by Akarsh Simha
    email                : kstar@bas.org.in
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <math.h>

#include "ksconjunct.h"

#include "ksnumbers.h"
#include "ksplanetbase.h"
#include "kstarsdata.h"

KSConjunct::KSConjunct() {

  ksdata = NULL;

}

QMap<long double, dms> KSConjunct::findClosestApproach(KSPlanetBase& Object1, KSPlanetBase& Object2, long double startJD, long double stopJD, dms maxSeparation) {

  QMap<long double, dms> Separations;
  QPair<long double, dms> extremum;
  dms Dist;
  dms prevDist;
  double step;
  int Sign, prevSign;
  long double jd;

  jd = startJD;
  ksdata = KStarsData::Instance();
  prevDist = findDistance(jd, &Object1, &Object2);
  //  kDebug() << "Entered KSConjunct::findClosestApproach() with startJD = " << (double)startJD;
  //  kDebug() << "Initial Positional Information: \n";
  //  kDebug() << Object1.name() << ": RA = " << Object1.ra() -> toHMSString() << "; Dec = " << Object1.dec() -> toDMSString() << "\n";
  //  kDebug() << Object2.name() << ": RA = " << Object2.ra() -> toHMSString() << "; Dec = " << Object2.dec() -> toDMSString() << "\n";
  prevSign = 0;
  
  step = (stopJD - startJD) / 4.0;
  if(Object1.name() == "Mars" || Object2.name() == "Mars")
    if (step > 30.0)
      step = 30.0;
  if(Object1.name() == "Venus" || Object1.name() == "Mercury" || Object2.name() == "Mercury" || Object2.name() == "Venus") 
    if (step > 15.0)
      step = 15.0;
  if(Object1.name() == "Moon" || Object2.name() == "Moon")
    if (step > 5.0)
      step = 5.0;

  //  kDebug() << "Initial Separation between " << Object1.name() << " and " << Object2.name() << " = " << (prevDist.toDMSString());

  for(jd = startJD + step; jd <= stopJD; jd += step) {
    Dist = findDistance(jd, &Object1, &Object2);
    //    kDebug() << "Dist = " << Dist.toDMSString() << "; prevDist = " << prevDist.toDMSString() << "; Difference = " << (Dist - prevDist).toDMSString();
    Sign = sgn(Dist.Degrees() - prevDist.Degrees()); 

    if(Sign != prevSign && prevSign == 1) {   // The prevSign == 1 ensures that we pick up only minima and don't waste time finding maxima
      //      kDebug() << "Sign = " << Sign << " and " << "prevSign = " << prevSign << ": Entering findPrecise()\n";
      if(findPrecise(&extremum, &Object1, &Object2, jd, step, prevSign))
        if(extremum.second.radians() < maxSeparation.radians())
          Separations.insert(extremum.first, extremum.second);
    }

    prevDist = Dist;
    prevSign = Sign;
  }

  return Separations;
}


dms KSConjunct::findDistance(long double jd, KSPlanetBase *Object1, KSPlanetBase *Object2) {

  KStarsDateTime t(jd);
  KSNumbers num(jd);
  dms dist;

  if(ksdata == NULL)
    ksdata = KStarsData::Instance();

  KSPlanet *m_Earth = new KSPlanet( ksdata, I18N_NOOP( "Earth" ), QString(), QColor( "white" ), 12756.28 /*diameter in km*/ );
  m_Earth -> findPosition( &num );
  dms LST(ksdata->geo()->GSTtoLST(t.gst()));

  Object1 -> findPosition(&num, ksdata->geo()->lat(), &LST, (KSPlanetBase *)m_Earth);
  Object2 -> findPosition(&num, ksdata->geo()->lat(), &LST, (KSPlanetBase *)m_Earth);

  dist.setRadians(Object1 -> angularDistanceTo(Object2).radians());
  
  return dist;
}

bool KSConjunct::findPrecise(QPair<long double, dms> *out, KSPlanetBase *Object1, KSPlanetBase *Object2, long double jd, double step, int prevSign) {
  dms prevDist;
  int Sign;
  dms Dist;

  if( out == NULL ) {
    kDebug() << "Argument out to KSConjunct::findPrecise(...) was NULL!";
    return false;
  }

  prevDist = findDistance(jd, Object1, Object2);

  step = -step / 2.0;
  prevSign = -prevSign;

  while(true) {
    jd += step;
    Dist = findDistance(jd, Object1, Object2);

    if(abs(step) < 1.0 / 24.0) {
      out -> first = jd - step / 2.0;
      out -> second = findDistance(jd - step/2.0, Object1, Object2);
      if(out -> second.radians() < findDistance(jd - 5.0, Object1, Object2).radians())
        return true;
      else
        return false;
    }
    Sign = sgn(Dist.Degrees() - prevDist.Degrees());
    if(Sign != prevSign) {
      step = -step / 2.0;
      Sign = -Sign;
    }
    prevSign = Sign;
    prevDist = Dist;
  }
}

int KSConjunct::sgn(dms a) {

  // Auxiliary function used by the KSConjunct::findClosestApproach(...) 
  // method and the KSConjunct::findPrecise(...) method

  return ((a.radians() > 0)?1:((a.radians() < 0)?-1:0));

}
