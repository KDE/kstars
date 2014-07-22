/***************************************************************************
                          ksalmanac.cpp  -  description

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

#include "ksalmanac.h"

#include <cmath>

#include "geolocation.h"
#include "kstarsdata.h"
#include "kstarsdatetime.h"
#include "ksnumbers.h"
#include "dms.h"
#include "skyobjects/kssun.h"
#include "skyobjects/ksmoon.h"
#include "skyobjects/skyobject.h"


KSAlmanac::KSAlmanac() :
    SunRise(0),
    SunSet(0),
    MoonRise(0),
    MoonSet(0)
{
    KStarsData* data = KStarsData::Instance();

    dt  = KStarsDateTime::currentDateTime();
    geo = data->geo();
    dt.setTime(QTime());
    dt  = geo->LTtoUT(dt);
    update();
}

void KSAlmanac::update() {
    RiseSetTime( &m_Sun, &SunRise, &SunSet, &SunRiseT, &SunSetT );
    RiseSetTime( &m_Moon, &MoonRise, &MoonSet, &MoonRiseT, &MoonSetT );
    findDawnDusk();
}

void KSAlmanac::RiseSetTime( SkyObject *o, double *riseTime, double *setTime, QTime *RiseTime, QTime *SetTime ) {
    // Compute object rise and set times
    const KStarsDateTime today = dt;
    const GeoLocation* _geo = geo;
    *RiseTime = o->riseSetTime( today.addDays(1), _geo, true ); // The addDays(1) gives the future rise time rather than past
    *SetTime = o->riseSetTime( today, _geo, false );
    *riseTime = -1.0 * RiseTime->secsTo(QTime(0,0,0,0)) / 86400.0;
    *setTime = -1.0 * SetTime->secsTo(QTime(0,0,0,0)) / 86400.0;

    // Check to see if the object is circumpolar
    // NOTE: Since we are working on a local copy of the Sun / Moon,
    //       we freely change the geolocation / time without setting
    //       them back.

    KSNumbers num( dt.djd() );
    dms LST = geo->GSTtoLST( dt.gst() );
    o->updateCoords( &num, true, geo->lat(), &LST );
    if ( o->checkCircumpolar( geo->lat() ) ) {
        if ( o->alt().Degrees() > 0.0 ) {
            //Circumpolar, signal it this way:
            *riseTime = 0.0;
            *setTime = 1.0;
        } else {
            //never rises, signal it this way:
            *riseTime = 0.0;
            *setTime = -1.0;
        }
    }
}

// FIXME: This is utter code duplication. All this should be handled
// in the KStars engine. Forgive me for adding to the nonsense, but I
// want to get the Observation Planner functional first. -- asimha

void KSAlmanac::findDawnDusk() {

    KStarsDateTime today = dt;
    KSNumbers num( today.djd() );
    dms LST = geo->GSTtoLST( today.gst() );

    m_Sun.updateCoords( &num, true, geo->lat(), &LST ); // We can abuse our own copy of the sun
    double dawn, da, dusk, du, max_alt, min_alt;
    double last_h = -12.0;
    double last_alt = findAltitude( &m_Sun, last_h );
    dawn = dusk = -13.0;
    max_alt = -100.0;
    min_alt = 100.0;
    for ( double h=-11.95; h<=12.0; h+=0.05 ) {
        double alt = findAltitude( &m_Sun, h );
        bool   asc = alt - last_alt > 0;
        if ( alt > max_alt )
            max_alt = alt;
        if ( alt < min_alt )
            min_alt = alt;

        if ( asc && last_alt <= -18.0 && alt >= -18.0 )
            dawn = h;
        if ( !asc && last_alt >= -18.0 && alt <= -18.0 )
            dusk = h;

        last_h   = h;
        last_alt = alt;
    }

    if ( dawn < -12.0 || dusk < -12.0 ) {
        da = -1.0;
        du = -1.0;
    } else {
        da = dawn / 24.0;
        du = ( dusk + 24.0 ) / 24.0;
    }

    DawnAstronomicalTwilight = da;
    DuskAstronomicalTwilight = du;
}

void KSAlmanac::setDate( KStarsDateTime *newdt ) {
    dt = *newdt; 
    update();
}

void KSAlmanac::setLocation( GeoLocation *m_geo ) {
    geo = m_geo;
    update();
}


double KSAlmanac::sunZenithAngleToTime( double z ) {
    // TODO: Correct for movement of the sun
    double HA = acos( ( cos( z * dms::DegToRad ) - m_Sun.dec().sin() * geo->lat()->sin() ) / (m_Sun.dec().cos() * geo->lat()->cos()) );
    double HASunset = acos( ( -m_Sun.dec().sin() * geo->lat()->sin() ) / (m_Sun.dec().cos() * geo->lat()->cos()) );
    return SunSet + ( HA - HASunset ) / 24.0;
}

    
double KSAlmanac::findAltitude( SkyPoint *p, double hour ) {
    KStarsDateTime ut = dt;
    ut.setTime( QTime() );
    ut = geo->LTtoUT( ut );
    ut= ut.addSecs( hour*3600.0 );
    dms LST = geo->GSTtoLST( ut.gst() );
    p->EquatorialToHorizontal( &LST, geo->lat() );
    return p->alt().Degrees();
}

