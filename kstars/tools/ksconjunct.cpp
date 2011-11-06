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

#include "ksconjunct.h"

#include <math.h>

#include "ksnumbers.h"
#include "skyobjects/ksplanetbase.h"
#include "skyobjects/ksplanet.h"
#include "skyobjects/ksasteroid.h"
#include "skyobjects/kscomet.h"
#include "kstarsdata.h"

KSConjunct::KSConjunct() {
    geoPlace = KStarsData::Instance()->geo();
}

void KSConjunct::setGeoLocation( GeoLocation *geo ) {
    if( geo != NULL )
        geoPlace = geo;
    else
        geoPlace = KStarsData::Instance()->geo();
}

QMap<long double, dms> KSConjunct::findClosestApproach(SkyObject& Object1, KSPlanetBase& Object2, long double startJD, long double stopJD, dms maxSeparation,bool _opposition) {

  QMap<long double, dms> Separations;
  QPair<long double, dms> extremum;
  dms Dist;
  dms prevDist;
  double step, step0;
  int Sign, prevSign;
  opposition=_opposition;
  //  kDebug() << "Entered KSConjunct::findClosestApproach() with startJD = " << (double)startJD;
  //  kDebug() << "Initial Positional Information: \n";
  //  kDebug() << Object1.name() << ": RA = " << Object1.ra() -> toHMSString() << "; Dec = " << Object1.dec() -> toDMSString() << "\n";
  //  kDebug() << Object2.name() << ": RA = " << Object2.ra() -> toHMSString() << "; Dec = " << Object2.dec() -> toDMSString() << "\n";
  prevSign = 0;
  
  step0 = (stopJD - startJD) / 4.0;  // I'm an idiot for having done this without having the lines that follow -- asimha

  // TODO: Work out a solid footing on which one can decide step0. -- asimha
  if( step0 > 24.8 * 365.25 ) // Sample pluto's orbit (248.09 years) at least 10 times.
      step0 = 24.8 * 365.25;

  // FIXME: This can be done better, but for now, I'm doing it the dumb way -- asimha
  if( Object1.name() == i18n( "Neptune" ) || Object2.name() == i18n( "Neptune" ) || Object1.name() == i18n( "Uranus" ) || Object2.name() == i18n( "Uranus" ) )
      if( step0 > 3652.5 )
          step0 = 3652.5;
  if( Object1.name() == i18n( "Jupiter" ) || Object2.name() == i18n( "Jupiter" ) || Object1.name() == i18n( "Saturn" ) || Object2.name() == i18n( "Saturn" ) )
      if( step0 > 365.25 )
          step0 = 365;
  if(Object1.name() == i18n( "Mars" ) || Object2.name() == i18n( "Mars" ))
    if (step0 > 10.0)
      step0 = 10.0;
  if(Object1.name() == i18n( "Venus" ) || Object1.name() == i18n( "Mercury" ) || Object2.name() == i18n( "Mercury" ) || Object2.name() == i18n( "Venus" )) 
    if (step0 > 5.0)
      step0 = 5.0;
  if(Object1.name() == "Moon" || Object2.name() == "Moon")
    if (step0 > 0.25)
      step0 = 0.25;

  step = step0;
  
  //	kDebug() << "Initial Separation between " << Object1.name() << " and " << Object2.name() << " = " << (prevDist.toDMSString());

  long double jd = startJD;
  prevDist = findDistance(jd, &Object1, &Object2);
  jd += step;
  while ( jd <= stopJD ) {
    int progress = int( 100.0*(jd - startJD)/(stopJD - startJD) );
    emit madeProgress( progress );
    
    Dist = findDistance(jd, &Object1, &Object2);
    Sign = sgn(Dist - prevDist); 
    //	kDebug() << "Dist = " << Dist.toDMSString() << "; prevDist = " << prevDist.toDMSString() << "; Difference = " << (Dist.Degrees() - prevDist.Degrees()) << "; Step = " << step;

    //How close are we to a conjunction, and how fast are we approaching one?
    double factor = fabs( (Dist.Degrees() - prevDist.Degrees()) / Dist.Degrees());
    if ( factor > 10.0 ) { //let's go faster!
        step = step0 * factor/10.0;
    } else { //slow down, we're getting close!
        step = step0;
    }
    
    if( Sign != prevSign && prevSign == -1) { //all right, we may have just passed a conjunction
        if ( step > step0 ) { //mini-loop to back up and make sure we're close enough
            //            kDebug() << "Entering slow loop: " << endl;
            jd -= step;
            step = step0;
            Sign = prevSign;
            while ( jd <= stopJD ) {
                Dist = findDistance(jd, &Object1, &Object2);
                Sign = sgn(Dist - prevDist); 
                //	kDebug() << "Dist=" << Dist.toDMSString() << "; prevDist=" << prevDist.toDMSString() << "; Diff=" << (Dist.Degrees() - prevDist.Degrees()) << "djd=" << (int)(jd - startJD);
                if ( Sign != prevSign ) break;
                
                prevDist = Dist;
                prevSign = Sign;
                jd += step;
            }
        }
        
        //	kDebug() << "Sign = " << Sign << " and " << "prevSign = " << prevSign << ": Entering findPrecise()\n";
        if(findPrecise(&extremum, &Object1, &Object2, jd, step, Sign))
            if(extremum.second.radians() < maxSeparation.radians())
                Separations.insert(extremum.first, extremum.second);
    }

    prevDist = Dist;
    prevSign = Sign;
    jd += step;
  }

  return Separations;
}


dms KSConjunct::findDistance(long double jd, SkyObject *Object1, KSPlanetBase *Object2)
{
  KStarsDateTime t(jd);
  KSNumbers num(jd);
  dms dist;

  KSPlanet *m_Earth = new KSPlanet( I18N_NOOP( "Earth" ), QString(), QColor( "white" ), 12756.28 /*diameter in km*/ );
  m_Earth -> findPosition( &num );
  dms LST(geoPlace->GSTtoLST(t.gst()));

  KSPlanetBase* p = dynamic_cast<KSPlanetBase*>(Object1);
  if( p )
      p->findPosition(&num, geoPlace->lat(), &LST, m_Earth);
  else
      Object1->updateCoords( &num );

  Object2->findPosition(&num, geoPlace->lat(), &LST, m_Earth);
  dist.setRadians(Object1 -> angularDistanceTo(Object2).radians());
  if( opposition ) {
      dist.setD( 180 - dist.Degrees() );
  }
  return dist;
}

bool KSConjunct::findPrecise(QPair<long double, dms> *out, SkyObject *Object1, KSPlanetBase *Object2, long double jd, double step, int prevSign) {
  dms prevDist;
  int Sign;
  dms Dist;

  if( out == NULL ) {
    kDebug() << "ERROR: Argument out to KSConjunct::findPrecise(...) was NULL!";
    return false;
  }

  prevDist = findDistance(jd, Object1, Object2);

  step = -step / 2.0;
  prevSign = -prevSign;

  while(true) {
    jd += step;
    Dist = findDistance(jd, Object1, Object2);
    //    kDebug() << "Dist=" << Dist.toDMSString() << "; prevDist=" << prevDist.toDMSString() << "; Diff=" << (Dist.Degrees() - prevDist.Degrees()) << "step=" << step;

    if(fabs(step) < 1.0 / (24.0*60.0) ) {
      out -> first = jd - step / 2.0;
      out -> second = findDistance(jd - step/2.0, Object1, Object2);
      if(out -> second.radians() < findDistance(jd - 5.0, Object1, Object2).radians())
        return true;
      else
        return false;
    }
    Sign = sgn(Dist - prevDist);
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
#include "ksconjunct.moc"
