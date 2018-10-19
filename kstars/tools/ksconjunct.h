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

#pragma once
#include "approachsolver.h"

class GeoLocation;
class KSPlanetBase;
class SkyObject;

/**
 * @class KSConjunct
 * @short Implements algorithms to find close conjunctions of planets in a given time range.
 * A class that implements a method to compute close conjunctions between any two solar system
 * objects excluding planetary moons. Given two such objects, this class has implementations of
 * algorithms required to find the time of closest approach in a given range of time.
 *
 * @author Akarsh Simha
 * @version 2.0
 */
class KSConjunct : public ApproachSolver
{
Q_OBJECT
public:
    /** Constructor. Instantiates a KSNumbers for internal computations. */
    KSConjunct();

    void setObject1(SkyObject_s &obj) { m_object1 = obj; }
    void setObject2(KSPlanetBase_s &obj) { m_object2 = obj; }
    void setOpposition(bool opposition) { m_opposition = opposition; }

signals:
    void madeProgress(int);

protected:
    double findInitialStep(long double startJD, long double stopJD) override;
    void updatePositions(long double jd) override;

private:
    dms findDistance() override;


    SkyObject_s m_object1;
    KSPlanetBase_s m_object2;
    bool m_opposition { false };
};

