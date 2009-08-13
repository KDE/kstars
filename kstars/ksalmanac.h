/***************************************************************************
                          ksalmanac.h  -  description

                             -------------------
    begin                : Friday May 8, 2009
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

#ifndef KSALMANAC_H_
#define KSALMANAC_H_

#include "skyobjects/kssun.h"
#include "skyobjects/ksmoon.h"
#include "geolocation.h"
#include "kstars.h"
#include "kstarsdatetime.h"
#include "kstarsdata.h"


class KStars;

/**
 *@class KSAlmanac
 *
 *A class that implements methods to find sun rise, sun set, twilight
 *begin / end times, moon rise and moon set times.
 *
 *@short Implement methods to find important times in a day
 *@author Prakash Mohan
 *@version 1.0
 */

// TODO: Fix the documentation for these methods

class KSAlmanac {
public:
    /**
     * This function computes the rise and set time for the given SkyObject. This is done in order to 
     * have a common function for the computation of the Sun and Moon rise and set times.
     */
    void RiseSetTime( SkyObject *o, double *riseTime, double *setTime, QTime *RiseTime, QTime *SetTime );

    /**
     *@short Set the date for computations to the given date
     *@param The new date to set as a KStarsDateTime
     */
    void setDate( KStarsDateTime *newdt );

    /**
     *@short Set the location for computations to the given location
     *@param The location to set for computations
     */
    void setLocation( GeoLocation *m_geo );

    /**
     *@short Returns an instance of this class
     * TODO: Re-implement as a multiple-instance class
     */
    static KSAlmanac* Instance();

    /**
     *All the functions returns the fraction of the day
     *as their return value
     */
    inline double getSunRise() { return SunRise; }
    inline double getSunSet() { return SunSet; }
    inline double getMoonRise() { return MoonRise; }
    inline double getMoonSet() { return MoonSet; }
    
    inline QTime sunRise() { return SunRiseT; }
    inline QTime sunSet() { return SunSetT; }
    inline QTime moonRise() { return MoonRiseT; }
    inline QTime moonSet() { return MoonSetT; }
    
 private:
    // TODO: Add documentation
    KSAlmanac(); 
    void update();
    
    static KSAlmanac *pinstance;
    KSSun m_Sun;
    KSMoon m_Moon;
    KStars *ks;
    KStarsDateTime dt;
    
    GeoLocation *geo;
    double SunRise, SunSet, MoonRise, MoonSet;
    QTime SunRiseT, SunSetT, MoonRiseT, MoonSetT;
};

#endif
