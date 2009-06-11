/***************************************************************************
                          ksmoon.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Sun Aug 26 2001
    copyright            : (C) 2001 by Jason Harris
    email                : kstars@30doradus.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "ksmoon.h"

#include <stdlib.h>
#include <math.h>

#include <QFile>
#include <QTextStream>
#include <kglobal.h>

#include "ksnumbers.h"
#include "ksutils.h"
#include "kssun.h"
#include "kstarsdata.h"

KSMoon::KSMoon()
        : KSPlanetBase( I18N_NOOP( "Moon" ), QString(), QColor("white"), 3474.8 /*diameter in km*/ )
{
    instance_count++;
    //Reset object type
    setType( SkyObject::MOON );
}

KSMoon::KSMoon(const KSMoon& o) :
    KSPlanetBase(o)
{
    instance_count++;
}

KSMoon* KSMoon::clone() const
{
    return new KSMoon(*this);
}

KSMoon::~KSMoon() {
    instance_count--;
    if(instance_count <= 0) {
        while ( ! LRData.isEmpty() ) delete LRData.takeFirst();
        while ( !  BData.isEmpty() ) delete  BData.takeFirst();
	data_loaded = false;
    }
}

bool KSMoon::data_loaded = false;
int KSMoon::instance_count = 0;
QList<KSMoon::MoonLRData*> KSMoon::LRData;
QList<KSMoon::MoonBData*> KSMoon::BData;

const double KSMoon::MagArray[19] = {-12.7,-12.4,-12.1,-11.8,-11.5,-11.2,
                               -11.0,-10.8,-10.4,-10.0,-9.6,-9.2,
                               -8.7,-8.2,-7.6,-6.7,-3.4,0,0};


bool KSMoon::loadData() {
    if (data_loaded) return true;

    QStringList fields;
    QFile f;
    int nd, nm, nm1, nf;
    double Li, Ri, Bi; //coefficients of the sums

    if ( KSUtils::openDataFile( f, "moonLR.dat" ) ) {
        QTextStream stream( &f );
        while ( !stream.atEnd() ) {
            fields = stream.readLine().split( ' ', QString::SkipEmptyParts );

            if ( fields.size() == 6 ) {
                nd = fields[0].toInt();
                nm = fields[1].toInt();
                nm1= fields[2].toInt();
                nf = fields[3].toInt();
                Li = fields[4].toDouble();
                Ri = fields[5].toDouble();
                LRData.append(new MoonLRData(nd, nm, nm1, nf, Li, Ri));
            }
        }
        f.close();
    } else
        return false;


    if ( KSUtils::openDataFile( f, "moonB.dat" ) ) {
        QTextStream stream( &f );
        while ( !stream.atEnd() ) {
            fields = stream.readLine().split( ' ', QString::SkipEmptyParts );
            
            if ( fields.size() == 5 ) {
                nd = fields[0].toInt();
                nm = fields[1].toInt();
                nm1= fields[2].toInt();
                nf = fields[3].toInt();
                Bi = fields[4].toDouble();
                BData.append(new MoonBData(nd, nm, nm1, nf, Bi));
            }
        }
        f.close();
    }

    data_loaded = true;
    return true;
}

bool KSMoon::findGeocentricPosition( const KSNumbers *num, const KSPlanetBase* ) {
    //Algorithms in this subroutine are taken from Chapter 45 of "Astronomical Algorithms"
    //by Jean Meeus (1991, Willmann-Bell, Inc. ISBN 0-943396-35-2.  http://www.willbell.com/math/mc1.htm)
    QString fname, snum, line;
    QFile f;
    double DegtoRad;
    double T, L, D, M, M1, F, E, A1, A2, A3;
    double sumL, sumR, sumB;

    DegtoRad = acos( -1.0 )/180.0;

    //Julian centuries since J2000
    T = num->julianCenturies();

    double Et = 1.0 - 0.002516*T - 0.0000074*T*T;

    //Moon's mean longitude
    L = 218.3164591 + 481267.88134236*T - 0.0013268*T*T + T*T*T/538841.0 - T*T*T*T/65194000.0;
    while ( L > 360.0 ) L -= 360.0;
    while ( L < 0.0 ) L += 360.0;
    L *= DegtoRad;
    //Moon's mean elongation
    D = 297.8502042 + 445267.1115168*T - 0.0016300*T*T + T*T*T/545868.0 - T*T*T*T/113065000.0;
    while ( D > 360.0 ) D -= 360.0;
    while ( D < 0.0 ) D += 360.0;
    D *= DegtoRad;
    //Sun's mean anomaly
    M = 357.5291092 + 35999.0502909*T - 0.0001536*T*T + T*T*T/24490000.0;
    while ( M > 360.0 ) M -= 360.0;
    while ( M < 0.0 ) M += 360.0;
    M *= DegtoRad;
    //Moon's mean anomaly
    M1= 134.9634114 + 477198.8676313*T + 0.0089970*T*T + T*T*T/69699.0 - T*T*T*T/14712000.0;
    while ( M1 > 360.0 ) M1 -= 360.0;
    while ( M1 < 0.0 ) M1 += 360.0;
    M1 *= DegtoRad;
    //Moon's argument of latitude (angle from ascending node)
    F = 93.2720993 + 483202.0175273*T - 0.0034029*T*T - T*T*T/3526000.0 + T*T*T*T/863310000.0;
    while ( F > 360.0 ) F -= 360.0;
    while ( F < 0.0 ) F += 360.0;
    F *= DegtoRad;

    A1 = 119.75 +    131.849*T;
    A2 =  53.09 + 479264.290*T;
    A3 = 313.45 + 481226.484*T;
    while ( A1 > 360.0 ) A1 -= 360.0;
    while ( A1 < 0.0 ) A1 += 360.0;
    while ( A2 > 360.0 ) A2 -= 360.0;
    while ( A2 < 0.0 ) A2 += 360.0;
    while ( A3 > 360.0 ) A3 -= 360.0;
    while ( A3 < 0.0 ) A3 += 360.0;
    A1 *= DegtoRad;
    A2 *= DegtoRad;
    A3 *= DegtoRad;

    //Calculate the series expansions stored in moonLR.txt and moonB.txt.
    //
    sumL = 0.0;
    sumR = 0.0;

    if (!loadData()) return false;

    for ( int i=0; i < LRData.size(); ++i ) {
        MoonLRData *mlrd = LRData[i];

        E = 1.0;
        if ( mlrd->nm ) { //if M != 0, include changing eccentricity of Earth's orbit
            E = Et;
            if ( abs( mlrd->nm )==2 ) E = E*E; //use E^2
        }
        sumL += E*mlrd->Li*sin( mlrd->nd*D + mlrd->nm*M + mlrd->nm1*M1 + mlrd->nf*F );
        sumR += E*mlrd->Ri*cos( mlrd->nd*D + mlrd->nm*M + mlrd->nm1*M1 + mlrd->nf*F );
    }

    sumB = 0.0;
    for ( int i=0; i < BData.size(); ++i ) {
        MoonBData *mbd = BData[i];

        E = 1.0;
        if ( mbd->nm ) { //if M != 0, include changing eccentricity of Earth's orbit
            E = Et;
            if ( abs( mbd->nm )==2 ) E = E*E; //use E^2
        }
        sumB += E*mbd->Bi*sin( mbd->nd*D + mbd->nm*M + mbd->nm1*M1 + mbd->nf*F );
    }

    //Additive terms for sumL and sumB
    sumL += ( 3958.0*sin( A1 ) + 1962.0*sin( L-F ) + 318.0*sin( A2 ) );
    sumB += ( -2235.0*sin( L ) + 382.0*sin( A3 ) + 175.0*sin( A1-F ) + 175.0*sin( A1+F ) + 127.0*sin( L-M1 ) - 115.0*sin( L+M1 ) );

    //Geocentric coordinates
    setEcLong( ( L + DegtoRad*sumL/1000000.0 ) * 180./dms::PI );  //convert radians to degrees
    setEcLat( ( DegtoRad*sumB/1000000.0 ) * 180./dms::PI );
    Rearth = ( 385000.56 + sumR/1000.0 )/AU_KM; //distance from Earth, in AU

    EclipticToEquatorial( num->obliquity() );

    //Determine position angle
    findPA( num );

    return true;
}

void KSMoon::findPhase() {
    KSSun *Sun = (KSSun*)KStarsData::Instance()->skyComposite()->findByName( "Sun" ); // TODO: Get rid of this ugly thing by making KSSun singleton.
    Phase = ecLong()->Degrees() - Sun->ecLong()->Degrees();
    double DegPhase = dms( Phase ).reduce().Degrees();
    int iPhase = int( 0.1*DegPhase+0.5 );
    if (iPhase==36) iPhase = 0;
    QString sPhase;
    sPhase = sPhase.sprintf( "%02d", iPhase );
    QString imName = "moon" + sPhase + ".png";

    QFile imFile;
    if ( KSUtils::openDataFile( imFile, imName ) ) {
        imFile.close();
        image0()->load( imFile.fileName() );
        image()->load( imFile.fileName() );
    }
}

QString KSMoon::phaseName() const {
    double f = illum();
    double p = Phase;

    //First, handle the major phases
    if ( f > 0.99 ) return i18nc( "moon phase, 100 percent illuminated", "Full moon" );
    if ( f < 0.01 ) return i18nc( "moon phase, 0 percent illuminated", "New moon" );
    if ( fabs( f - 0.50 ) < 0.01 ) {
        if ( p < 180.0 ) return i18nc( "moon phase, half-illuminated and growing", "First quarter" );
        else return i18nc( "moon phase, half-illuminated and shrinking", "Third quarter" );
    }

    //Next, handle the more general cases
    if ( p < 90.0 ) return i18nc( "moon phase between new moon and 1st quarter", "Waxing crescent" );
    else if ( p < 180.0 ) return i18nc( "moon phase between 1st quarter and full moon", "Waxing gibbous" );
    else if ( p < 270.0 ) return i18nc( "moon phase between full moon and 3rd quarter", "Waning gibbous" );
    else if ( p < 360.0 ) return i18nc( "moon phase between 3rd quarter and new moon", "Waning crescent" );

    else return i18n( "unknown" );
}
