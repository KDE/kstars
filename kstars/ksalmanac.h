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
class KSAlmanac {
    public:
        void RiseSetTime( SkyObject *o, double *riseTime, double *setTime, QTime *RiseTime, QTime *SetTime );
        void setDate( KStarsDateTime *newdt );
        void setLocation( GeoLocation *m_geo );
        static KSAlmanac* Instance();
        inline double getSunRise() { return SunRise; }
        inline double getSunSet() { return SunSet; }
        inline double getMoonRise() { return MoonRise; }
        inline double getMoonSet() { return MoonSet; }
        inline QTime sunRise() { return SunRiseT; }
        inline QTime sunSet() { return SunSetT; }
        inline QTime moonRise() { return MoonRiseT; }
        inline QTime moonSet() { return MoonSetT; }
        double getAstroTwilight( bool begin = true );
        double getNauticalTwilight( bool begin = true );
        double getCivilTwilight( bool begin = true );

    private:
        KSAlmanac(); 
        void update();
        static KSAlmanac *pinstance;
        KSSun *m_Sun;
        KSMoon *m_Moon;
        KStars *ks;
        KStarsDateTime dt;
        GeoLocation *geo;
        double SunRise, SunSet, MoonRise, MoonSet, riseRate;
        QTime SunRiseT, SunSetT, MoonRiseT, MoonSetT;
};

#endif
