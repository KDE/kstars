/***************************************************************************
                          kscomet.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Wed 19 Feb 2003
    copyright            : (C) 2001 by Jason Harris
    email                : jharris@30doradus.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "kscomet.h"

#include <QRegExp>
#include <kdebug.h>
#include <QMap>

#include "kstarsdata.h"
#include "kstarsdatetime.h"
#include "ksnumbers.h"
#include "dms.h"

namespace {
    int letterToNum(QChar c)
    {
        char l = c.toAscii();
        if( l < 'A' || l > 'Z' || l == 'I')
            return 0;
        if( l > 'I' )
            return l - 'A';
        return l - 'A' + 1;
    }
    // Convert letter designation like "AZ" to number.
    int letterDesigToN(QString s) {
        int n = 0;
        foreach(QChar c, s) {
            int nl = letterToNum(c);
            if( nl == 0 )
                return 0;
            n = n*25 + nl;
        }
        return n;
    }

    QMap<QChar,qint64> cometType;
}

KSComet::KSComet( const QString &_s, const QString &imfile,
                  long double _JD, double _q, double _e, dms _i, dms _w, dms _Node, double Tp, float _M1,
                  float _M2, float _K1, float _K2)
    : KSPlanetBase(_s, imfile),
      JD(_JD), q(_q), e(_e), M1(_M1), M2(_M2), K1(_K1), K2(_K2),  i(_i), w(_w), N(_Node)
{
    setType( SkyObject::COMET );

    //Find the Julian Day of Perihelion from Tp
    //Tp is a double which encodes a date like: YYYYMMDD.DDDDD (e.g., 19730521.33333
    int year = int( Tp/10000.0 );
    int month = int( (int(Tp) % 10000)/100.0 );
    int day = int( int(Tp) % 100 );
    double Hour = 24.0 * ( Tp - int(Tp) );
    int h = int( Hour );
    int m = int( 60.0 * ( Hour - h ) );
    int s = int( 60.0 * ( 60.0 * ( Hour - h) - m ) );

    JDp = KStarsDateTime( QDate( year, month, day ), QTime( h, m, s ) ).djd();

    //compute the semi-major axis, a:
    a = q/(1.0-e);

    //Compute the orbital Period from Kepler's 3rd law:
    P = 365.2568984 * pow(a, 1.5); //period in days

    //If the name contains a "/", make this name2 and make name a truncated version without the leading "P/" or "C/"
    if ( name().contains( "/" ) ) {
        setLongName( name() );
        setName( name().mid( name().indexOf("/") + 1 ) );
    }

    // Try to calculate UID for comets. It's derived from comet designation.
    // To parge name string regular exressions are used. Not really readable.
    // And its make strong assumtions about format of comets' names.
    // Probably come better algotrithm should be used. 

    // Periodic comet. Designation like: 12P or 12P-C
    QRegExp rePer("^(\\d+)[PD](-([A-Z]+))?");
    if( rePer.indexIn(_s) != -1 ) {
        // Comet number
        qint64 num = rePer.cap(1).toInt();
        // Fragment number
        qint64 fragmentN = letterDesigToN( rePer.cap(2) );
        // Set UID
        uidPart = num << 16 | fragmentN;
        return;
    }

    // Non periodic comet or comets with single approach
    // Designations like C/(2006 A7)
    QRegExp rePro("^([PCXDA])/.*\\((\\d{4}) ([A-Z])(\\d+)(-([A-Z]+))?\\)");
    if( rePro.indexIn(_s) != -1 ) {
        // Fill comet type
        if( cometType.empty() ) {
            cometType.insert('P',0);
            cometType.insert('C',1);
            cometType.insert('X',2);
            cometType.insert('D',3);
            cometType.insert('A',4);
        }
        qint64 type       = cometType[ rePro.cap(1)[0] ]; // Type of comet
        qint64 year       = rePro.cap(2).toInt();         // Year of discovery
        qint64 halfMonth  = letterToNum( rePro.cap(3)[0] );
        qint64 nHalfMonth = rePro.cap(4).toInt();
        qint64 fragment   = letterDesigToN( rePro.cap(6) );

        uidPart =
            1          << 43 |
            type       << 40 |  // Bits 40-42 (3)
            halfMonth  << 33 |  // Bits 33-39 (7) Hope this is enough 
            nHalfMonth << 28 |  // Bits 28-32 (5)
            year       << 16 |  // Bits 16-27 (12)
            fragment;           // Bits 0-15  (16)
        return;
    }
    uidPart = 0;
    // kDebug() << "Didn't get it: " << _s;
}

KSComet* KSComet::clone() const
{
    return new KSComet(*this);
}

void KSComet::findPhysicalParameters() {
    // Compute and store the estimated Physical size of the comet's coma, tail and nucleus
    // References:
    // * http://www.projectpluto.com/update7b.htm#comet_tail_formula [Project Pluto / GUIDE]
    // * http://articles.adsabs.harvard.edu//full/1978BAICz..29..103K/0000113.000.html [Kresak, 1978a, "Passages of comets and asteroids near the earth"]
    NuclearSize = pow( 10, 2.1 - 0.2 * M1 );
    double mHelio = M1 + K1 * log10( rsun() );
    double L0, D0, L, D;
    L0 = pow( 10, -0.0075 * mHelio * mHelio - 0.19 * mHelio + 2.10 );
    D0 = pow( 10, -0.0033 * mHelio * mHelio - 0.07 * mHelio + 3.25 );
    L = L0 * ( 1 - pow( 10, -4 * rsun() ) ) * ( 1 - pow( 10, -2 * rsun() ) );
    D = D0 * ( 1 - pow( 10, -2 * rsun() ) ) * ( 1 - pow( 10, -rsun() ) );
    TailSize = L * 1e6;
    ComaSize = D * 1e3;
    setPhysicalSize( ComaSize );
}

bool KSComet::findGeocentricPosition( const KSNumbers *num, const KSPlanetBase *Earth ) {
    double v(0.0), r(0.0);

    //Precess the longitude of the Ascending Node to the desired epoch:
    dms n = dms( double(N.Degrees() - 3.82394E-5 * ( num->julianDay() - J2000 )) ).reduce();

    if ( e > 0.98 ) {
        //Use near-parabolic approximation
        double k = 0.01720209895; //Gauss gravitational constant
        double a = 0.75 * ( num->julianDay() - JDp ) * k * sqrt( (1+e)/(q*q*q) );
        double b = sqrt( 1.0 + a*a );
        double W = pow((b+a),1.0/3.0) - pow((b-a),1.0/3.0);
        double c = 1.0 + 1.0/(W*W);
        double f = (1.0-e)/(1.0+e);
        double g = f/(c*c);

        double a1 = (2.0/3.0) + (2.0*W*W/5.0);
        double a2 = (7.0/5.0) + (33.0*W*W/35.0) + (37.0*W*W*W*W/175.0);
        double a3 = W*W*( (432.0/175.0) + (956.0*W*W/1125.0) + (84.0*W*W*W*W/1575.0) );
        double w = W*(1.0 + g*c*( a1 + a2*g + a3*g*g ));

        v = 2.0*atan(w) / dms::DegToRad;
        r = q*( 1.0 + w*w )/( 1.0 + w*w*f );
    } else {
        //Use normal ellipse method
        //Determine Mean anomaly for desired date:
        dms m = dms( double(360.0*( num->julianDay() - JDp )/P) ).reduce();
        double sinm, cosm;
        m.SinCos( sinm, cosm );

        //compute eccentric anomaly:
        double E = m.Degrees() + e*180.0/dms::PI * sinm * ( 1.0 + e*cosm );

        if ( e > 0.05 ) { //need more accurate approximation, iterate...
            double E0;
            int iter(0);
            do {
                E0 = E;
                iter++;
                E = E0 - ( E0 - e*180.0/dms::PI *sin( E0*dms::DegToRad ) - m.Degrees() )/(1 - e*cos( E0*dms::DegToRad ) );
            } while ( fabs( E - E0 ) > 0.001 && iter < 1000 );
        }

        double sinE, cosE;
        dms E1( E );
        E1.SinCos( sinE, cosE );

        double xv = a * ( cosE - e );
        double yv = a * sqrt( 1.0 - e*e ) * sinE;

        //v is the true anomaly; r is the distance from the Sun
        v = atan2( yv, xv ) / dms::DegToRad;
        r = sqrt( xv*xv + yv*yv );
    }

    //vw is the sum of the true anomaly and the argument of perihelion
    dms vw( v + w.Degrees() );
    double sinN, cosN, sinvw, cosvw, sini, cosi;

    n.SinCos( sinN, cosN );
    vw.SinCos( sinvw, cosvw );
    i.SinCos( sini, cosi );

    //xh, yh, zh are the heliocentric cartesian coords with the ecliptic plane congruent with zh=0.
    double xh = r * ( cosN * cosvw - sinN * sinvw * cosi );
    double yh = r * ( sinN * cosvw + cosN * sinvw * cosi );
    double zh = r * ( sinvw * sini );

    //the spherical ecliptic coordinates:
    double ELongRad = atan2( yh, xh );
    double ELatRad = atan2( zh, r );

    helEcPos.longitude.setRadians( ELongRad );
    helEcPos.latitude.setRadians( ELatRad );
    setRsun( r );

    //xe, ye, ze are the Earth's heliocentric cartesian coords
    double cosBe, sinBe, cosLe, sinLe;
    Earth->ecLong().SinCos( sinLe, cosLe );
    Earth->ecLat().SinCos( sinBe, cosBe );

    double xe = Earth->rsun() * cosBe * cosLe;
    double ye = Earth->rsun() * cosBe * sinLe;
    double ze = Earth->rsun() * sinBe;

    //convert to geocentric ecliptic coordinates by subtracting Earth's coords:
    xh -= xe;
    yh -= ye;
    zh -= ze;

    //Finally, the spherical ecliptic coordinates:
    ELongRad = atan2( yh, xh );
    double rr = sqrt( xh*xh + yh*yh );
    ELatRad = atan2( zh, rr );

    ep.longitude.setRadians( ELongRad );
    ep.latitude.setRadians( ELatRad );
    setRearth( Earth );

    EclipticToEquatorial( num->obliquity() );
    nutate( num );
    aberrate( num );

    findPhysicalParameters();

    return true;
}

//T-mag =  M1 + 5*log10(delta) + k1*log10(r)
void KSComet::findMagnitude(const KSNumbers*)
{
    setMag( M1 + 5.0 * log10( rearth() ) + K1 * log10( rsun() ) );
}

void KSComet::setEarthMOID( double earth_moid )
{
    EarthMOID = earth_moid;
}

void KSComet::setAlbedo( float albedo )
{
    Albedo = albedo;
}

void KSComet::setDiameter( float diam )
{
    Diameter = diam;
}
 
void KSComet::setDimensions( QString dim )
{
    Dimensions = dim;
}

void KSComet::setNEO( bool neo )
{
    NEO = neo;
}

void KSComet::setOrbitClass( QString orbit_class )
{
    OrbitClass = orbit_class;
}

void KSComet::setOrbitID( QString orbit_id )
{
    OrbitID = orbit_id;
}

void KSComet::setPeriod( float per )
{
    Period = per;
}

void KSComet::setRotationPeriod(float rot_per)
{
    RotationPeriod = rot_per;
}

//Unused virtual function from KSPlanetBase
bool KSComet::loadData() { return false; }

SkyObject::UID KSComet::getUID() const
{
    return solarsysUID(UID_SOL_COMET) | uidPart;
}
