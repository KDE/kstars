/***************************************************************************
                          ksconjunct.h  -  K Desktop Planetarium
                             -------------------
    begin                : Sat Mar 15 2008
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


#ifndef KSCONJUNCT_H_
#define KSCONJUNCT_H_

#include <QMap>
#include <QObject>

#include "dms.h"
#include "skyobjects/skyobject.h"
#include "skyobjects/ksplanet.h"
#include "skyobjects/ksplanetbase.h"
#include "ksnumbers.h"

class SkyObject;
class KSNumbers;
class KSPlanetBase;
class KSPlanet; 
class dms;

/**
  *@class KSConjunct
  *A class that implements a method to compute close conjunctions between any two solar system
  *objects excluding planetary moons. Given two such objects, this class has implementations of
  *algorithms required to find the time of closest approach in a given range of time.
  *@short Implements algorithms to find close conjunctions of planets in a given time range.
  *@author Akarsh Simha
  *@version 1.0
  */

class KSConjunct :public QObject {
 Q_OBJECT
 
 public:
  /**
    *Constructor.  Instantiates a KSNumbers for internal computations.
    */
  
  KSConjunct();

  /**
   *Destructor.  (Empty)
   */

  ~KSConjunct() { }

  /**
   *@short Sets the geographic location to compute conjunctions at
   *
   *@param geo  Pointer to the GeoLocation object
   */
  void setGeoLocation( GeoLocation *geo );

  /**
   *@short Compute the closest approach of two planets in the given range
   *
   *@param Object1  A copy of the class corresponding to one of the two bodies
   *@param Object2  A copy of the class corresponding to the other of the two bodies
   *@param startJD  Julian Day corresponding to start of the calculation period
   *@param stopJD   Julian Day corresponding to end of the calculation period
   *@param maxSeparation   Maximum separation between Object1 and Object2 - a measure
   *                       how close the conjunction should be to be output.
   *@param opposition A parameter to see if we are computing conjunction or opposition
   *@return Hash containing julian days of close conjunctions against separation
   */

  QMap<long double, dms> findClosestApproach(SkyObject& Object1, KSPlanetBase& Object2, long double startJD, long double stopJD, dms maxSeparation, bool _opposition=false);
  
 signals:
  void madeProgress( int progress );

 private:

  /**
    *@short Finds the angular distance between two solar system objects.
    *
    *@param jd  Julian Day corresponding to the time of computation
    *@param Object1  A pointer to the first solar system object
    *@param Object2  A pointer to the second solar system object
    *
    *@return The angular distance between the two bodies.
    */

  // TODO: Make pointers to Object1 and Object2 private objects instead of passing them to the methods again and again. 
  //       Should improve performance, at least marginally.

  dms findDistance(long double jd, SkyObject *Object1, KSPlanetBase *Object2);

  /**
    *@short Compute the precise value of the extremum once the extremum has been detected.
    *
    *@param out  A pointer to a QPair that stores the Julian Day and Separation corresponding to the extremum
    *@param Object1  A pointer to the first solar system body
    *@param Object2  A pointer to the second solar system body
    *@param jd  Julian day corresponding to the endpoint of the interval where extremum was detected.
    *@param step  The step in jd taken during computation earlier. (Defines the interval size)
    *@param prevSign The previous sign of increment in moving from jd - step to jd
    *
    *@return true if the extremum is a minimum
    */

  bool findPrecise(QPair<long double, dms> *out, SkyObject *Object1, KSPlanetBase *Object2, long double jd, double step, int prevSign);

  /**
    *@short Return the sign of an angle
    *
    *@param a  The angle whose sign has to be returned
    *
    *@return (-1, 0, 1) if a.radians() is (-ve, zero, +ve)
    */

  int sgn(dms a);

  bool opposition;
  GeoLocation *geoPlace;
};

#endif
  
