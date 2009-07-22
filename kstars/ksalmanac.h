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
     * ??? I have absolutely no clue what this method does and why it takes a SkyObject at all - Akarsh
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
     *??? in what units does this return?
     */
    inline double getSunRise() { return SunRise; }
    
    /**
     *??? in what units does this return?
     */
    inline double getSunSet() { return SunSet; }
    
    /**
     *??? in what units does this return?
     */
    inline double getMoonRise() { return MoonRise; }
    
    /**
     *??? in what units does this return?
     */
    inline double getMoonSet() { return MoonSet; }
    
    inline QTime sunRise() { return SunRiseT; }
    inline QTime sunSet() { return SunSetT; }
    inline QTime moonRise() { return MoonRiseT; }
    inline QTime moonSet() { return MoonSetT; }
    

    double getAstroTwilight( bool begin = true );
    double getNauticalTwilight( bool begin = true );
    double getCivilTwilight( bool begin = true );

 private:
    // TODO: Add documentation
    KSAlmanac(); 
    void update();
    
    static KSAlmanac *pinstance;
    KSSun *m_Sun;
    KSMoon *m_Moon;
    KStars *ks;
    KStarsDateTime dt;
    
    GeoLocation *geo;
    double SunRise, SunSet, MoonRise, MoonSet;
    QTime SunRiseT, SunSetT, MoonRiseT, MoonSetT;
};

#endif
