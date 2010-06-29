/***************************************************************************
                          skypoint.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Sun Feb 11 2001
    copyright            : (C) 2001-2005 by Jason Harris
    email                : jharris@30doradus.org
    copyright            : (C) 2004-2005 by Pablo de Vicente
    email                : p.devicente@wanadoo.es
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "skypoint.h"

#include <kdebug.h>
#include <klocale.h>

#include "skyobject.h"
#include "dms.h"
#include "ksnumbers.h"
#include "kstarsdatetime.h" //for J2000 define
#include "kssun.h"
#include "kstarsdata.h"
#include "Options.h"
#include "skycomponents/skymapcomposite.h"

void SkyPoint::set( const dms& r, const dms& d ) {
    RA  = RA0  = r;
    Dec = Dec0 = d;
    //Quaternion
    syncQuaternion();
}

void SkyPoint::set( double r, double d ) {
    RA0.setH( r );
    Dec0.setD( d );
    RA.setH( r );
    Dec.setD( d );
    //Quaternion
    syncQuaternion();
}

SkyPoint::~SkyPoint(){
}

//Quaternion
void SkyPoint::syncQuaternion() {
    m_q = Quaternion( RA.radians(), Dec.radians() );
}

void SkyPoint::EquatorialToHorizontal( const dms *LST, const dms *lat ) {
    //Uncomment for spherical trig version
    double AltRad, AzRad;
    double sindec, cosdec, sinlat, coslat, sinHA, cosHA;
    double sinAlt, cosAlt;

    dms HourAngle = (*LST) - ra();

    lat->SinCos( sinlat, coslat );
    dec().SinCos( sindec, cosdec );
    HourAngle.SinCos( sinHA, cosHA );

    sinAlt = sindec*sinlat + cosdec*coslat*cosHA;
    AltRad = asin( sinAlt );
    cosAlt = cos( AltRad );

    double arg = ( sindec - sinlat*sinAlt )/( coslat*cosAlt );
    if ( arg <= -1.0 ) AzRad = dms::PI;
    else if ( arg >= 1.0 ) AzRad = 0.0;
    else AzRad = acos( arg );

    if ( sinHA > 0.0 ) AzRad = 2.0*dms::PI - AzRad; // resolve acos() ambiguity

    Alt.setRadians( AltRad );
    Az.setRadians( AzRad );

    // //Uncomment for XYZ version
    //  	double xr, yr, zr, xr1, zr1, sa, ca;
    // 	//Z-axis rotation by -LST
    // 	dms a = dms( -1.0*LST->Degrees() );
    // 	a.SinCos( sa, ca );
    // 	xr1 = m_X*ca + m_Y*sa;
    // 	yr  = -1.0*m_X*sa + m_Y*ca;
    // 	zr1 = m_Z;
    //
    // 	//Y-axis rotation by lat - 90.
    // 	a = dms( lat->Degrees() - 90.0 );
    // 	a.SinCos( sa, ca );
    // 	xr = xr1*ca - zr1*sa;
    // 	zr = xr1*sa + zr1*ca;
    //
    // 	//FIXME: eventually, we will work with XYZ directly
    // 	Alt.setRadians( asin( zr ) );
    // 	Az.setRadians( atan2( yr, xr ) );

}

void SkyPoint::HorizontalToEquatorial( const dms *LST, const dms *lat ) {
    double HARad, DecRad;
    double sinlat, coslat, sinAlt, cosAlt, sinAz, cosAz;
    double sinDec, cosDec;

    lat->SinCos( sinlat, coslat );
    alt().SinCos( sinAlt, cosAlt );
    Az.SinCos( sinAz,  cosAz );

    sinDec = sinAlt*sinlat + cosAlt*coslat*cosAz;
    DecRad = asin( sinDec );
    cosDec = cos( DecRad );
    Dec.setRadians( DecRad );

    double x = ( sinAlt - sinlat*sinDec )/( coslat*cosDec );

    //Under certain circumstances, x can be very slightly less than -1.0000, or slightly
    //greater than 1.0000, leading to a crash on acos(x).  However, the value isn't
    //*really* out of range; it's a kind of roundoff error.
    if ( x < -1.0 && x > -1.000001 ) HARad = dms::PI;
    else if ( x > 1.0 && x < 1.000001 ) HARad = 0.0;
    else if ( x < -1.0 ) {
        kWarning() << i18n( "Coordinate out of range." ) << endl;
        HARad = dms::PI;
    } else if ( x > 1.0 ) {
        kWarning() << i18n( "Coordinate out of range." ) << endl;
        HARad = 0.0;
    } else HARad = acos( x );

    if ( sinAz > 0.0 ) HARad = 2.0*dms::PI - HARad; // resolve acos() ambiguity

    RA.setRadians( LST->radians() - HARad );
    RA.setD( RA.reduce().Degrees() );  // 0 <= RA < 24

    syncQuaternion();
}

void SkyPoint::findEcliptic( const dms *Obliquity, dms &EcLong, dms &EcLat ) {
    double sinRA, cosRA, sinOb, cosOb, sinDec, cosDec, tanDec;
    ra().SinCos( sinRA, cosRA );
    dec().SinCos( sinDec, cosDec );
    Obliquity->SinCos( sinOb, cosOb );

    tanDec = sinDec/cosDec;                    // FIXME: -jbb div by zero?
    double y = sinRA*cosOb + tanDec*sinOb;
    double ELongRad = atan2( y, cosRA );
    EcLong.setRadians( ELongRad );
    EcLong.reduce();
    EcLat.setRadians( asin( sinDec*cosOb - cosDec*sinOb*sinRA ) );
}

void SkyPoint::setFromEcliptic( const dms *Obliquity, const dms *EcLong, const dms *EcLat ) {
    double sinLong, cosLong, sinLat, cosLat, sinObliq, cosObliq;
    EcLong->SinCos( sinLong, cosLong );
    EcLat->SinCos( sinLat, cosLat );
    Obliquity->SinCos( sinObliq, cosObliq );

    double sinDec = sinLat*cosObliq + cosLat*sinObliq*sinLong;

    double y = sinLong*cosObliq - (sinLat/cosLat)*sinObliq;
    double RARad =  atan2( y, cosLong );
    RA.setRadians( RARad );
    RA.reduce();
    Dec.setRadians( asin(sinDec) );
}

void SkyPoint::precess( const KSNumbers *num) {
    double cosRA0, sinRA0, cosDec0, sinDec0;
    double v[3], s[3];

    RA0.SinCos( sinRA0, cosRA0 );
    Dec0.SinCos( sinDec0, cosDec0 );

    s[0] = cosRA0*cosDec0;
    s[1] = sinRA0*cosDec0;
    s[2] = sinDec0;
    //Multiply P2 and s to get v, the vector representing the new coords.
    for ( unsigned int i=0; i<3; ++i ) {
        v[i] = 0.0;
        for (uint j=0; j< 3; ++j) {
            v[i] += num->p2( j, i )*s[j];
        }
    }

    //Extract RA, Dec from the vector:
    RA.setRadians( atan2( v[1], v[0] ) );
    RA.reduce();
    Dec.setRadians( asin( v[2] ) );
}

void SkyPoint::nutate(const KSNumbers *num) {
    double cosRA, sinRA, cosDec, sinDec, tanDec;
    double cosOb, sinOb;

    RA.SinCos( sinRA, cosRA );
    Dec.SinCos( sinDec, cosDec );

    num->obliquity()->SinCos( sinOb, cosOb );

    //Step 2: Nutation
    if ( fabs( Dec.Degrees() ) < 80.0 ) { //approximate method
        tanDec = sinDec/cosDec;

        double dRA  = num->dEcLong()*( cosOb + sinOb*sinRA*tanDec ) - num->dObliq()*cosRA*tanDec;
        double dDec = num->dEcLong()*( sinOb*cosRA ) + num->dObliq()*sinRA;

        RA.setD( RA.Degrees() + dRA );
        Dec.setD( Dec.Degrees() + dDec );
    } else { //exact method
        dms EcLong, EcLat;
        findEcliptic( num->obliquity(), EcLong, EcLat );

        //Add dEcLong to the Ecliptic Longitude
        dms newLong( EcLong.Degrees() + num->dEcLong() );
        setFromEcliptic( num->obliquity(), &newLong, &EcLat );
    }
}

SkyPoint SkyPoint::moveAway( SkyPoint &from, double dist ){
    dms lat1, dtheta;

    if( dist == 0.0 ) {
        kDebug() << "moveThrough called with zero distance!";
        return *this;
    }

    double dst = fabs( dist * dms::DegToRad / 3600.0 ); // In radian
    
    // Compute the bearing angle w.r.t. the RA axis ("latitude")
    dms dRA( ra().Degrees() - from.ra().Degrees() );
    dms dDec( dec().Degrees() - from.dec().Degrees() );
    double bearing = atan2( dRA.sin() / dRA.cos(), dDec.sin() ); // Do not use dRA = PI / 2!!
    //double bearing = atan2( dDec.radians() , dRA.radians() );
    
    double dir0 = (dist >= 0 ) ? bearing : bearing + dms::PI; // in radian
    dist = fabs( dist ); // in radian


    lat1.setRadians( asin( dec().sin() * cos( dst ) +
                           dec().cos() * sin( dst ) * cos( dir0 ) ) );
    dtheta.setRadians( atan2( sin( dir0 ) * sin( dst ) * dec().cos(),
                              cos( dst ) - dec().sin() * lat1.sin() ) );
    
    dms finalRA( ra().Degrees() + dtheta.Degrees() );
    return SkyPoint( finalRA, lat1 );
}

bool SkyPoint::bendlight() {

    // NOTE: This should be applied before aberration

    // First see if we are close enough to the sun to bother about the
    // effect. We correct for the effect at least till b = 10 solar
    // radii, where the effect is only about 0.06".  Assuming
    // min. sun-earth distance is 200 solar radii.

    static KSSun* thesun;
    const dms maxAngle( 1.75 * ( 30.0 / 200.0) / dms::DegToRad );

    if( !thesun ) // Hope we don't destroy the sun before the program ends!
        thesun = (KSSun*) KStarsData::Instance()->skyComposite()->findByName( "Sun" );
    //    kDebug() << "bendlight says maximum correctable angle is " << maxAngle.Degrees();

    // TODO: We can make this code more efficient
    SkyPoint sp = *thesun;
    //    kDebug() << "The unaberrated sun is " << sp.angularDistanceTo( sun ).Degrees() << "deg away";
    if( fabs( angularDistanceTo( &sp ).radians() ) <= maxAngle.radians() ) {
        // We correct for GR effects
        double corr_sec = 1.75 * thesun->physicalSize() / ( thesun->rearth() * AU_KM * angularDistanceTo( &sp ).sin() );
        Q_ASSERT( corr_sec > 0 );
        
        sp = moveAway( sp, corr_sec );
        setRA(  sp.ra() );
        setDec( sp.dec() );
        return true;
    }
    return false;
}

void SkyPoint::aberrate(const KSNumbers *num) {
    double cosRA, sinRA, cosDec, sinDec;
    double cosOb, sinOb, cosL, sinL, cosP, sinP;

    double K = num->constAberr().Degrees();  //constant of aberration
    double e = num->earthEccentricity();

    RA.SinCos( sinRA, cosRA );
    Dec.SinCos( sinDec, cosDec );

    num->obliquity()->SinCos( sinOb, cosOb );
    double tanOb = sinOb/cosOb;

    num->sunTrueLongitude().SinCos( sinL, cosL );
    num->earthPerihelionLongitude().SinCos( sinP, cosP );


    //Step 3: Aberration
    double dRA = -1.0 * K * ( cosRA * cosL * cosOb + sinRA * sinL )/cosDec
                  + e * K * ( cosRA * cosP * cosOb + sinRA * sinP )/cosDec;

    double dDec = -1.0 * K * ( cosL * cosOb * ( tanOb * cosDec - sinRA * sinDec ) + cosRA * sinDec * sinL )
                   + e * K * ( cosP * cosOb * ( tanOb * cosDec - sinRA * sinDec ) + cosRA * sinDec * sinP );

    RA.setD( RA.Degrees() + dRA );
    Dec.setD( Dec.Degrees() + dDec );
}

void SkyPoint::updateCoords( KSNumbers *num, bool /*includePlanets*/, const dms *lat, const dms *LST ) {
    //Correct the catalog coordinates for the time-dependent effects
    //of precession, nutation and aberration
    precess(num);
    nutate(num);
    if(Options::useRelativistic())
        bendlight();
    aberrate(num);
    if ( lat || LST )
        kWarning() << i18n( "lat and LST parameters should only be used in KSPlanetBase objects." ) ;
}

void SkyPoint::precessFromAnyEpoch(long double jd0, long double jdf){

    double cosRA, sinRA, cosDec, sinDec;
    double v[3], s[3];

    RA.setD( RA0.Degrees() );
    Dec.setD( Dec0.Degrees() );

    RA.SinCos( sinRA, cosRA );
    Dec.SinCos( sinDec, cosDec );

    if (jd0 == jdf)
        return;

    if ( jd0 == B1950) {
        B1950ToJ2000();
        jd0 = J2000;
        RA.SinCos( sinRA, cosRA );
        Dec.SinCos( sinDec, cosDec );

    }

    if (jd0 !=jdf) {
        // The original coordinate is referred to the FK5 system and
        // is NOT J2000.
        if ( jd0 != J2000 ) {

            //v is a column vector representing input coordinates.
            v[0] = cosRA*cosDec;
            v[1] = sinRA*cosDec;
            v[2] = sinDec;

            //Need to first precess to J2000.0 coords
            //s is the product of P1 and v; s represents the
            //coordinates precessed to J2000
            KSNumbers num(jd0);
            for ( unsigned int i=0; i<3; ++i ) {
                s[i] = num.p1( 0, i )*v[0] +
                       num.p1( 1, i )*v[1] +
                       num.p1( 2, i )*v[2];
            }

            //Input coords already in J2000, set s accordingly.
        } else {

            s[0] = cosRA*cosDec;
            s[1] = sinRA*cosDec;
            s[2] = sinDec;
        }

        if ( jdf == B1950) {

            RA.setRadians( atan2( s[1],s[0] ) );
            Dec.setRadians( asin( s[2] ) );
            J2000ToB1950();

            return;
        }

        KSNumbers num(jdf);
        for ( unsigned int i=0; i<3; ++i ) {
            v[i] = num.p2( 0, i )*s[0] +
                   num.p2( 1, i )*s[1] +
                   num.p2( 2, i )*s[2];
        }

        RA.setRadians( atan2( v[1],v[0] ) );
        Dec.setRadians( asin( v[2] ) );

        if (RA.Degrees() < 0.0 )
            RA.setD( RA.Degrees() + 360.0 );

        return;
    }

}

void SkyPoint::apparentCoord(long double jd0, long double jdf){
    precessFromAnyEpoch(jd0,jdf);
    KSNumbers *num = new KSNumbers (jdf);
    nutate(num);
    if(Options::useRelativistic())
        bendlight();
    aberrate(num);
    delete num;
}

void SkyPoint::Equatorial1950ToGalactic(dms &galLong, dms &galLat) {

    double a = 192.25;
    double sinb, cosb, sina_RA, cosa_RA, sinDEC, cosDEC, tanDEC;

    dms c(303.0);
    dms b(27.4);
    tanDEC = tan( Dec.radians() );

    b.SinCos(sinb,cosb);
    dms( a - RA.Degrees() ).SinCos(sina_RA,cosa_RA);
    Dec.SinCos(sinDEC,cosDEC);

    galLong.setRadians( c.radians() - atan2( sina_RA, cosa_RA*sinb-tanDEC*cosb) );
    galLong = galLong.reduce();

    galLat.setRadians( asin(sinDEC*sinb+cosDEC*cosb*cosa_RA) );
}

void SkyPoint::GalacticToEquatorial1950(const dms* galLong, const dms* galLat) {

    double a = 123.0;
    double sinb, cosb, singLat, cosgLat, tangLat, singLong_a, cosgLong_a;

    dms c(12.25);
    dms b(27.4);
    tangLat = tan( galLat->radians() );
    galLat->SinCos(singLat,cosgLat);

    dms( galLong->Degrees() - a ).SinCos(singLong_a,cosgLong_a);
    b.SinCos(sinb,cosb);

    RA.setRadians(c.radians() + atan2(singLong_a,cosgLong_a*sinb-tangLat*cosb) );
    RA = RA.reduce();

    Dec.setRadians( asin(singLat*sinb+cosgLat*cosb*cosgLong_a) );
}

void SkyPoint::B1950ToJ2000(void) {
    double cosRA, sinRA, cosDec, sinDec;
    //	double cosRA0, sinRA0, cosDec0, sinDec0;
    double v[3], s[3];

    // 1984 January 1 0h
    KSNumbers *num = new KSNumbers (2445700.5);

    // Eterms due to aberration
    addEterms();
    RA.SinCos( sinRA, cosRA );
    Dec.SinCos( sinDec, cosDec );

    // Precession from B1950 to J1984
    s[0] = cosRA*cosDec;
    s[1] = sinRA*cosDec;
    s[2] = sinDec;
    for ( unsigned int i=0; i<3; ++i ) {
        v[i] = num->p2b( 0, i )*s[0] + num->p2b( 1, i )*s[1] +
               num->p2b( 2, i )*s[2];
    }

    // RA zero-point correction at 1984 day 1, 0h.
    RA.setRadians( atan2( v[1],v[0] ) );
    Dec.setRadians( asin( v[2] ) );

    RA.setH( RA.Hours() + 0.06390/3600. );
    RA.SinCos( sinRA, cosRA );
    Dec.SinCos( sinDec, cosDec );

    s[0] = cosRA*cosDec;
    s[1] = sinRA*cosDec;
    s[2] = sinDec;

    // Precession from 1984 to J2000.

    for ( unsigned int i=0; i<3; ++i ) {
        v[i] = num->p1( 0, i )*s[0] +
               num->p1( 1, i )*s[1] +
               num->p1( 2, i )*s[2];
    }

    RA.setRadians( atan2( v[1],v[0] ) );
    Dec.setRadians( asin( v[2] ) );

    delete num;
}

void SkyPoint::J2000ToB1950(void) {
    double cosRA, sinRA, cosDec, sinDec;
    //	double cosRA0, sinRA0, cosDec0, sinDec0;
    double v[3], s[3];

    // 1984 January 1 0h
    KSNumbers *num = new KSNumbers (2445700.5);

    RA.SinCos( sinRA, cosRA );
    Dec.SinCos( sinDec, cosDec );

    s[0] = cosRA*cosDec;
    s[1] = sinRA*cosDec;
    s[2] = sinDec;

    // Precession from J2000 to 1984 day, 0h.

    for ( unsigned int i=0; i<3; ++i ) {
        v[i] = num->p2( 0, i )*s[0] +
               num->p2( 1, i )*s[1] +
               num->p2( 2, i )*s[2];
    }

    RA.setRadians( atan2( v[1],v[0] ) );
    Dec.setRadians( asin( v[2] ) );

    // RA zero-point correction at 1984 day 1, 0h.

    RA.setH( RA.Hours() - 0.06390/3600. );
    RA.SinCos( sinRA, cosRA );
    Dec.SinCos( sinDec, cosDec );

    // Precession from B1950 to J1984

    s[0] = cosRA*cosDec;
    s[1] = sinRA*cosDec;
    s[2] = sinDec;
    for ( unsigned int i=0; i<3; ++i ) {
        v[i] = num->p1b( 0, i )*s[0] + num->p1b( 1, i )*s[1] +
               num->p1b( 2, i )*s[2];
    }

    RA.setRadians( atan2( v[1],v[0] ) );
    Dec.setRadians( asin( v[2] ) );

    // Eterms due to aberration
    subtractEterms();

    delete num;
}

SkyPoint SkyPoint::Eterms(void) {

    double sd, cd, sinEterm, cosEterm;
    dms raTemp, raDelta, decDelta;

    Dec.SinCos(sd,cd);
    raTemp.setH( RA.Hours() + 11.25);
    raTemp.SinCos(sinEterm,cosEterm);

    raDelta.setH( 0.0227*sinEterm/(3600.*cd) );
    decDelta.setD( 0.341*cosEterm*sd/3600. + 0.029*cd/3600. );

    SkyPoint spDelta = SkyPoint (raDelta, decDelta);

    return spDelta;
}

void SkyPoint::addEterms(void) {

    SkyPoint spd = Eterms();

    RA.setD( RA.Degrees() + spd.ra().Degrees() );
    Dec.setD( Dec.Degrees() + spd.dec().Degrees() );

}

void SkyPoint::subtractEterms(void) {

    SkyPoint spd = Eterms();

    RA.setD( RA.Degrees() - spd.ra().Degrees() );
    Dec.setD( Dec.Degrees() - spd.dec().Degrees() );
}

dms SkyPoint::angularDistanceTo(const SkyPoint *sp) {

    double dalpha = ra().radians() - sp->ra().radians() ;
    double ddelta = dec().radians() - sp->dec().radians() ;

    double sa = sin(dalpha/2.);
    double sd = sin(ddelta/2.);

    double hava = sa*sa;
    double havd = sd*sd;

    double aux = havd + cos (dec().radians()) * cos(sp->dec().radians())
                 * hava;

    dms angDist;
    angDist.setRadians( 2 * fabs(asin( sqrt(aux) )) );

    return angDist;
}

double SkyPoint::vRSun(long double jd0) {

    double ca, sa, cd, sd, vsun;
    double cosRA, sinRA, cosDec, sinDec;

    /* Sun apex (where the sun goes) coordinates */

    dms asun(270.9592);	// Right ascention: 18h 3m 50.2s [J2000]
    dms dsun(30.00467);	// Declination: 30^o 0' 16.8'' [J2000]
    vsun=20.;		// [km/s]

    asun.SinCos(sa,ca);
    dsun.SinCos(sd,cd);

    /* We need an auxiliary SkyPoint since we need the
    * source referred to the J2000 equinox and we do not want to ovewrite
    * the current values
    */

    SkyPoint aux;
    aux.set(RA0,Dec0);

    aux.precessFromAnyEpoch(jd0, J2000);

    aux.ra().SinCos( sinRA, cosRA );
    aux.dec().SinCos( sinDec, cosDec );

    /* Computation is done performing the scalar product of a unitary vector
    in the direction of the source with the vector velocity of Sun, both being in the
    LSR reference system:
    Vlsr	= Vhel + Vsun.u_radial =>
    Vlsr 	= Vhel + vsun(cos D cos A,cos D sen A,sen D).(cos d cos a,cos d sen a,sen d)
    Vhel 	= Vlsr - Vsun.u_radial
    */

    return vsun *(cd*cosDec*(cosRA*ca + sa*sinRA) + sd*sinDec);

}

double SkyPoint::vHeliocentric(double vlsr, long double jd0) {

    return vlsr - vRSun(jd0);
}

double SkyPoint::vHelioToVlsr(double vhelio, long double jd0) {

    return vhelio + vRSun(jd0);
}

double SkyPoint::vREarth(long double jd0)
{
    double sinRA, sinDec, cosRA, cosDec;

    /* u_radial = unitary vector in the direction of the source
    	Vlsr 	= Vhel + Vsun.u_radial
    		= Vgeo + VEarth.u_radial + Vsun.u_radial  =>

    	Vgeo 	= (Vlsr -Vsun.u_radial) - VEarth.u_radial
    		=  Vhel - VEarth.u_radial
    		=  Vhel - (vx, vy, vz).(cos d cos a,cos d sen a,sen d)
    */

    /* We need an auxiliary SkyPoint since we need the
    * source referred to the J2000 equinox and we do not want to ovewrite
    * the current values
    */

    SkyPoint aux;
    aux.set(RA0,Dec0);

    aux.precessFromAnyEpoch(jd0, J2000);

    aux.ra().SinCos( sinRA, cosRA );
    aux.dec().SinCos( sinDec, cosDec );

    /* vEarth is referred to the J2000 equinox, hence we need that
    the source coordinates are also in the same reference system.
    */

    KSNumbers num(jd0);
    return num.vEarth(0) * cosDec * cosRA +
           num.vEarth(1) * cosDec * sinRA +
           num.vEarth(2) * sinDec;
}


double SkyPoint::vGeocentric(double vhelio, long double jd0)
{
    return vhelio - vREarth(jd0);
}

double SkyPoint::vGeoToVHelio(double vgeo, long double jd0)
{
    return vgeo + vREarth(jd0);
}

double SkyPoint::vRSite(double vsite[3])
{

    double sinRA, sinDec, cosRA, cosDec;

    RA.SinCos( sinRA, cosRA );
    Dec.SinCos( sinDec, cosDec );

    return vsite[0]*cosDec*cosRA + vsite[1]*cosDec*sinRA + vsite[2]*sinDec;
}

double SkyPoint::vTopoToVGeo(double vtopo, double vsite[3])
{
    return vtopo + vRSite(vsite);
}

double SkyPoint::vTopocentric(double vgeo, double vsite[3])
{
    return vgeo - vRSite(vsite);
}

bool SkyPoint::checkCircumpolar( const dms *gLat ) {
    return fabs(dec().Degrees())  >  (90 - fabs(gLat->Degrees()));
}

dms SkyPoint::altRefracted() const {
    if( Options::useRefraction() )
        return refract(Alt);
    else
        return Alt;
}

// Calculate refraction correction. Parameter and return value are in degrees
static double refractionCorr(double alt) {
    return 1.02 / tan(dms::DegToRad * ( alt + 10.3/(alt + 5.11) )) / 60;
}
// Critical height. Below this height formula produce meaningless
// results and correction value is just interpolated
static const double altCrit  = -1;
static const double corrCrit = refractionCorr( altCrit );

dms SkyPoint::refract(dms h) {
    const double alt = h.Degrees();
    if( alt > altCrit )
        return dms( alt + refractionCorr(alt) );
    else
        return dms( alt + corrCrit * (alt + 90) / (altCrit + 90) );
}

// Found uncorrected value by solving equation. This is OK since
// unrefract is never called in loops.
//
// Convergence is quite fast just a few iterations.
dms SkyPoint::unrefract(dms h) {
    const double alt = h.Degrees();

    double h0 = alt;
    double h1 = alt - refractionCorr(h0);
    while( fabs(h1 - h0) > 1e-4 ) {
        h0 = h1;
        h1 = alt - refractionCorr(h0);
    }
    return dms(h1);
}
