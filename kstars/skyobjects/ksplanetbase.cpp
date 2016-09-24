/***************************************************************************
                          ksplanetbase.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Sun Jul 22 2001
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

#include "ksplanetbase.h"

#include <cmath>

#include <QFile>
#include <QPoint>
#include <QMatrix>

#include "nan.h"
#include "kstarsdata.h"
#include "ksutils.h"
#include "ksnumbers.h"
#include "Options.h"
#include "skymap.h"
#include "ksasteroid.h"
#include "kscomet.h"
#include "kspluto.h"
#include "ksplanet.h"
#include "kssun.h"
#include "ksmoon.h"
#include "skycomponents/skymapcomposite.h"
#include "texturemanager.h"

QVector<QColor> KSPlanetBase::planetColor = QVector<QColor>() <<
  QColor("slateblue") << //Mercury
  QColor("lightgreen") << //Venus
  QColor("red") << //Mars
  QColor("goldenrod") << //Jupiter
  QColor("khaki") << //Saturn
  QColor("lightseagreen") << //Uranus
  QColor("skyblue") << //Neptune
  QColor("grey") << //Pluto
  QColor("yellow") << //Sun
  QColor("white"); //Moon


const SkyObject::UID KSPlanetBase::UID_SOL_BIGOBJ   = 0;
const SkyObject::UID KSPlanetBase::UID_SOL_ASTEROID = 1;
const SkyObject::UID KSPlanetBase::UID_SOL_COMET    = 2;

KSPlanetBase::KSPlanetBase( const QString &s, const QString &image_file, const QColor &c, double pSize ) :
    TrailObject( 2, 0.0, 0.0, 0.0, s ),
    Rearth( NaN::d )
{
    init( s, image_file, c, pSize );
}

void KSPlanetBase::init( const QString &s, const QString &image_file, const QColor &c, double pSize ) {
    m_image = TextureManager::getImage( image_file );
    PositionAngle = 0.0;
    PhysicalSize = pSize;
    m_Color = c;
    setName( s );
    setLongName( s );
}

KSPlanetBase* KSPlanetBase::createPlanet( int n ) {
    switch ( n ) {
        case KSPlanetBase::MERCURY:
        case KSPlanetBase::VENUS:
        case KSPlanetBase::MARS:
        case KSPlanetBase::JUPITER:
        case KSPlanetBase::SATURN:
        case KSPlanetBase::URANUS:
        case KSPlanetBase::NEPTUNE:
            return new KSPlanet( n );
            break;
        /*case KSPlanetBase::PLUTO:
            return new KSPluto();
            break;*/
        case KSPlanetBase::SUN:
            return new KSSun();
            break;
        case KSPlanetBase::MOON:
            return new KSMoon();
            break;
    }
    return 0;
}

void KSPlanetBase::EquatorialToEcliptic( const dms *Obliquity ) {
    findEcliptic( Obliquity, ep.longitude, ep.latitude );
}

void KSPlanetBase::EclipticToEquatorial( const dms *Obliquity ) {
    setFromEcliptic( Obliquity, ep.longitude, ep.latitude );
}

void KSPlanetBase::updateCoords( const KSNumbers *num, bool includePlanets, const dms *lat, const dms *LST, bool )
{
    KStarsData *kd = KStarsData::Instance();
    if ( includePlanets ) {
        kd->skyComposite()->earth()->findPosition( num ); //since we don't pass lat & LST, localizeCoords will be skipped

        if ( lat && LST ) {
            findPosition( num, lat, LST, kd->skyComposite()->earth() );
            //Don't add to the trail this time
            if( hasTrail() )
                Trail.takeLast();
        } else {
            findGeocentricPosition( num, kd->skyComposite()->earth() );
        }
    }
}

void KSPlanetBase::findPosition( const KSNumbers *num, const dms *lat, const dms *LST, const KSPlanetBase *Earth ) {
    // DEBUG edit
    findGeocentricPosition( num, Earth );  //private function, reimplemented in each subclass
    findPhase();
    setAngularSize( asin(physicalSize()/Rearth/AU_KM)*60.*180./dms::PI ); //angular size in arcmin

    if ( lat && LST )
        localizeCoords( num, lat, LST ); //correct for figure-of-the-Earth

    if ( hasTrail() ) {
        addToTrail( KStarsDateTime( num->getJD() ).toString( "yyyy.MM.dd hh:mm" ) + i18nc("Universal time", "UT") ); // TODO: Localize date/time format?
        if ( Trail.size() > TrailObject::MaxTrail )
            clipTrail();
    }

    findMagnitude(num);

    if ( type() == SkyObject::COMET ) {
        // Compute tail size
        KSComet *me = (KSComet *)this;
        double TailAngSize;
        // Convert the tail size in km to angular tail size (degrees)
        TailAngSize = asin(physicalSize()/Rearth/AU_KM)*60.0*180.0/dms::PI;
        // Find the apparent length as projected on the celestial sphere (the comet's tail points away from the sun)
        me->setTailAngSize( TailAngSize * fabs(sin( phase().radians() )));
    }

}

bool KSPlanetBase::isMajorPlanet() const {
    if ( name() == i18n( "Mercury" ) || name() == i18n( "Venus" ) || name() == i18n( "Mars" ) ||
         name() == i18n( "Jupiter" ) || name() == i18n( "Saturn" ) || name() == i18n( "Uranus" ) ||
         name() == i18n( "Neptune" ) )
        return true;
    return false;
}

void KSPlanetBase::localizeCoords( const KSNumbers *num, const dms *lat, const dms *LST ) {
    //convert geocentric coordinates to local apparent coordinates (topocentric coordinates)
    dms HA, HA2; //Hour Angle, before and after correction
    double rsinp, rcosp, u, sinHA, cosHA, sinDec, cosDec, D;
    double cosHA2;
    double r = Rearth * AU_KM; //distance from Earth, in km
    u = atan( 0.996647*tan( lat->radians() ) );
    rsinp = 0.996647*sin( u );
    rcosp = cos( u );
    HA.setD( LST->Degrees() - ra().Degrees() );
    HA.SinCos( sinHA, cosHA );
    dec().SinCos( sinDec, cosDec );

    D = atan2( rcosp*sinHA, r*cosDec/6378.14 - rcosp*cosHA );
    dms temp;
    temp.setRadians( ra().radians() - D );
    setRA( temp );

    HA2.setD( LST->Degrees() - ra().Degrees() );
    cosHA2 = cos( HA2.radians() );

    //temp.setRadians( atan2( cosHA2*( r*sinDec/6378.14 - rsinp ), r*cosDec*cosHA/6378.14 - rcosp ) );
    // The atan2() version above makes the planets move crazy in the htm branch -jbb
    temp.setRadians( atan( cosHA2*( r*sinDec/6378.14 - rsinp )/( r*cosDec*cosHA/6378.14 - rcosp ) ) );

    setDec( temp );

    //Make sure Dec is between -90 and +90
    if ( dec().Degrees() > 90.0 ) {
        setDec( 180.0 - dec().Degrees() );
        setRA( ra().Hours() + 12.0 );
        ra().reduce();
    }
    if ( dec().Degrees() < -90.0 ) {
        setDec( 180.0 + dec().Degrees() );
        setRA( ra().Hours() + 12.0 );
        ra().reduce();
    }

    EquatorialToEcliptic( num->obliquity() );
}

void KSPlanetBase::setRearth( const KSPlanetBase *Earth ) {
    double sinL, sinB, sinL0, sinB0;
    double cosL, cosB, cosL0, cosB0;
    double x,y,z;

    //The Moon's Rearth is set in its findGeocentricPosition()...
    if ( name() == "Moon" ) {
        return;
    }

    if ( name() == "Earth" ) {
        Rearth = 0.0;
        return;
    }

    if ( ! Earth  ) {
        qDebug() << "KSPlanetBase::setRearth():  Error: Need an Earth pointer.  (" << name() << ")";
        Rearth = 1.0;
        return;
    }

    Earth->ecLong().SinCos( sinL0, cosL0 );
    Earth->ecLat().SinCos( sinB0, cosB0 );
    double eX = Earth->rsun()*cosB0*cosL0;
    double eY = Earth->rsun()*cosB0*sinL0;
    double eZ = Earth->rsun()*sinB0;

    helEcLong().SinCos( sinL, cosL );
    helEcLat().SinCos( sinB, cosB );
    x = rsun()*cosB*cosL - eX;
    y = rsun()*cosB*sinL - eY;
    z = rsun()*sinB - eZ;

    Rearth = sqrt(x*x + y*y + z*z);

    //Set angular size, in arcmin
    AngularSize = asin(PhysicalSize/Rearth/AU_KM)*60.*180./dms::PI;
}

void KSPlanetBase::findPA( const KSNumbers *num ) {
    //Determine position angle of planet (assuming that it is aligned with
    //the Ecliptic, which is only roughly correct).
    //Displace a point along +Ecliptic Latitude by 1 degree
    SkyPoint test;
    dms newELat( ecLat().Degrees() + 1.0 );
    test.setFromEcliptic( num->obliquity(), ecLong(), newELat );
    double dx = ra().Degrees() - test.ra().Degrees();
    double dy = test.dec().Degrees() - dec().Degrees();
    double pa;
    if ( dy ) {
        pa = atan2( dx, dy )*180.0/dms::PI;
    } else {
        pa = dx < 0 ? 90.0 : -90.0;
    }
    setPA( pa );
}

double KSPlanetBase::labelOffset() const {
    double size = angSize() * dms::PI * Options::zoomFactor()/10800.0;

    //Determine minimum size for offset
    double minsize = 4.;
    if ( type() == SkyObject::ASTEROID || type() == SkyObject::COMET )
        minsize = 2.;
    if ( name() == "Sun" || name() == "Moon" )
        minsize = 8.;
    if ( size < minsize )
        size = minsize;

    //Inflate offset for Saturn
    if ( name() == i18n( "Saturn" ) )
        size = int(2.5*size);

    return 0.5*size + 4.;
}

void KSPlanetBase::findPhase() {
    /* Compute the phase of the planet in degrees */
    double earthSun = KStarsData::Instance()->skyComposite()->earth()->rsun();
    double cosPhase = (rsun()*rsun() + rearth()*rearth() - earthSun*earthSun)
        / (2 * rsun() * rearth() );
    Phase = acos ( cosPhase ) * 180.0 / dms::PI;
    /* More elegant way of doing it, but requires the Sun.
       TODO: Switch to this if and when we make KSSun a singleton */
    //    Phase = ecLong()->Degrees() - Sun->ecLong()->Degrees();
}
