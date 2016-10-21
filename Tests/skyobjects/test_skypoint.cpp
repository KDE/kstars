/***************************************************************************
                   test_skypoint.cpp  -  KStars Planetarium
                             -------------------
    begin                : Tue 27 Sep 2016 20:54:28 CDT
    copyright            : (c) 2016 by Akarsh Simha
    email                : akarsh.simha@kdemail.net
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/


/* Project Includes */
#include "test_skypoint.h"
#include "ksnumbers.h"
#include "time/kstarsdatetime.h"
#include "auxiliary/dms.h"


void TestSkyPoint::testPrecession() {
    /*
     * NOTE: These test cases were picked randomly and generated using
     * https://ned.ipac.caltech.edu/forms/calculator.html
     */

    auto verify = []( const SkyPoint &p, const double targetRA, const double targetDec, const double tolerance ) {
        qDebug() << p.ra().toHMSString() << " " << p.dec().toDMSString();
        QVERIFY( fabs( p.ra().Degrees() - targetRA ) < tolerance * 15. );
        QVERIFY( fabs( p.dec().Degrees() - targetDec ) < tolerance );
    };

    /* Precession of equinoxes within FK5 system */

    // Check precession from J2000.0 to J1992.5

    // NOTE: (FIXME?) I set 1950.0 as the observation epoch in the
    // calculator. Changing it to 1992.5 made no difference; probably
    // required only for FK5 to FK4 conversion. The help is not
    // extremely clear on this. -- asimha

    constexpr double arcsecPrecision = 3.e-4;

    long double jd = KStarsDateTime::epochToJd( 1992.5, KStarsDateTime::JULIAN );
    KSNumbers num( jd );
    SkyPoint p( dms( "18:22:54", false ), dms( "34:23:19", true ) ); // J2000.0 coordinates
    p.precess( &num );
    verify( p, 275.65734620, 34.38447020, arcsecPrecision );

    // Check precession from J2000.0 to J2016.8
    jd = KStarsDateTime::epochToJd( 2016.8 );
    num.updateValues( jd );
    p.set( dms( "23:24:25", false ), dms( "-08:07:06", true ) );
    p.precess( &num );
    verify( p,  351.32145112, ( -8.02590004 ) , arcsecPrecision );

    // Check precession from J2000.0 to J2109.8
    jd = KStarsDateTime::epochToJd( 2109.8 );
    num.updateValues( jd );
    p.precess( &num );
    verify( p, 352.52338626, ( -7.51340424 ) , arcsecPrecision );

    /* Precession of equinoxes + conversion from FK4 to FK5 */

    // Conversion from B1950.0 to J2000.0
    p.set( dms( "11:22:33", false ), dms( "44:55:66", true ) ); // Sets all of RA0,Dec0,RA,Dec
    p.B1950ToJ2000(); // Assumes (RA, Dec) are referenced to FK4 @ B1950 and stores the FK5 @ J2000 result in (RA, Dec)
    verify( p, 171.32145647, 44.66010982, arcsecPrecision );

    // Conversion from B1950.0 to J2016.8
    // Current of B1950ToJ2000 is to NOT alter (RA0,Dec0), so we shouldn't have to reset it
    jd = KStarsDateTime::epochToJd( 2016.8 );
    p.precessFromAnyEpoch( B1950, jd ); // Takes (RA0, Dec0) and precesses to (RA, Dec) applying FK4 <--> FK5 conversions as necessary
    verify( p, 171.55045618, 44.56762158, arcsecPrecision );

    /* Precession of equinoxes + conversion from FK5 to FK4 */
    p.set( dms( "11:22:33", false ), dms( "44:55:66", true ) ); // Sets all of RA0,Dec0,RA,Dec
    p.J2000ToB1950(); // Assumes (RA, Dec) are referenced to FK5 @ J2000 and stores the FK4 @ B1950 result in (RA, Dec)
    // NOTE: I set observation epoch = 2000.0 in NED's calculator. This time, it made a difference whether I set 2000.0 or 1950.0, but the difference was very small.
    verify( p,169.94978888, 45.20928652, arcsecPrecision );

    // Conversion from J2016.8 to B1950.0
    // Current behavior of J2000ToB1950 is to NOT alter (RA0,Dec0), so we shouldn't have to reset it
    jd = KStarsDateTime::epochToJd( 2016.8 );
    p.precessFromAnyEpoch( jd, B1950 ); // Takes (RA0, Dec0) and precesses to (RA, Dec) applying FK4 <--> FK5 conversions as necessary
    // NOTE: I set observation epoch = 2016.8 in NED's calculator. It made a difference whether I set 2000.0 or 2016.8, but the difference was very small.
    verify( p, 169.71785991, 45.30132855, arcsecPrecision );



}

QTEST_GUILESS_MAIN( TestSkyPoint )
