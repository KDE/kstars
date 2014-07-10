/***************************************************************************
                          skyobject.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Sun Feb 11 2001
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

#include "skyobject.h"

#include <iostream>

#include <kglobal.h>
#include <kstandarddirs.h>
#include <QPainter>
#include <QTextStream>
#include <QFile>
#include <QFontMetricsF>

#include "starobject.h" //needed in saveUserLog()
#include "ksnumbers.h"
#include "kspopupmenu.h"
#include "dms.h"
#include "geolocation.h"
#include "kstarsdatetime.h"
#include "kstarsdata.h"
#include "Options.h"
#include "skycomponents/skylabeler.h"

QString SkyObject::emptyString;
QString SkyObject::unnamedString = QString(I18N_NOOP("unnamed"));
QString SkyObject::unnamedObjectString = QString(I18N_NOOP("unnamed object"));
QString SkyObject::starString = QString("star");

const SkyObject::UID SkyObject::invalidUID   = ~0;
const SkyObject::UID SkyObject::UID_STAR     = 0;
const SkyObject::UID SkyObject::UID_GALAXY   = 1;
const SkyObject::UID SkyObject::UID_DEEPSKY  = 2;
const SkyObject::UID SkyObject::UID_SOLARSYS = 3;

SkyObject::SkyObject( int t, dms r, dms d, float m,
                      const QString &n, const QString &n2,
                      const QString &lname )
    : SkyPoint( r, d),
      info()
{
    setType( t );
    sortMagnitude = m;
    setName(n);
    setName2(n2);
    setLongName(lname);
}

SkyObject::SkyObject( int t, double r, double d, float m,
                      const QString &n, const QString &n2,
                      const QString &lname )
    : SkyPoint( r, d),
      info()
{
    setType( t );
    sortMagnitude = m;
    setName(n);
    setName2(n2);
    setLongName(lname);
}

SkyObject* SkyObject::clone() const
{
    return new SkyObject(*this);
}

SkyObject::~SkyObject() {}

void SkyObject::showPopupMenu( KSPopupMenu *pmenu, const QPoint &pos ) {
    initPopupMenu( pmenu );
    pmenu->popup( pos );
}

void SkyObject::initPopupMenu( KSPopupMenu *pmenu ) {
    pmenu->createEmptyMenu( this );
}

void SkyObject::setLongName( const QString &longname ) {
    if ( longname.isEmpty() ) {
        if ( hasName() )
            LongName = name();
        else if ( hasName2() )
            LongName = name2();
        else
            LongName.clear();
    } else {
        LongName = longname;
    }
}

QTime SkyObject::riseSetTime( const KStarsDateTime &dt, const GeoLocation *geo, bool rst, bool exact ) {

    // If this object does not rise or set, return an invalid time
    SkyPoint p = recomputeCoords( dt, geo );
    if( p.checkCircumpolar( geo->lat() ) )
        return QTime();

    //First of all, if the object is below the horizon at date/time dt, adjust the time
    //to bring it above the horizon
    KStarsDateTime dt2 = dt;
    dms lst(geo->GSTtoLST( dt.gst() ));
    p.EquatorialToHorizontal( &lst, geo->lat() );
    if ( p.alt().Degrees() < 0.0 ) {
        if ( p.az().Degrees() < 180.0 ) { //object has not risen yet
            dt2 = dt.addSecs( 12.*3600. ); // Move forward 12 hours, to a time when it has already risen
        } else { //object has already set
            dt2 = dt.addSecs( -12.*3600. ); // Move backward 12 hours, to a time when it has not yet set
        }
    }
    // The addition / subtraction of 12 hours ensures that we always
    // compute the _closest_ rise time and the _closest_ set time to
    // the current time.

    QTime rstUt = riseSetTimeUT( dt2, geo, rst, exact );
    if ( ! rstUt.isValid() )
        return QTime();

    return geo->UTtoLT( KStarsDateTime( dt2.date(), rstUt ) ).time();
}

QTime SkyObject::riseSetTimeUT( const KStarsDateTime &dt, const GeoLocation *geo, bool riseT, bool exact ) {
    // First trial to calculate UT
    QTime UT = auxRiseSetTimeUT( dt, geo, &ra(), &dec(), riseT );

    // We iterate once more using the calculated UT to compute again
    // the ra and dec for that time and hence the rise/set time.
    // Also, adjust the date by +/- 1 day, if necessary

    // By adding this +/- 1 day, we are double-checking that the
    // reported rise-time is the _already_ (last) risen time, and that
    // the reported set-time is the _future_ (next) set time
    //
    // However, issues with this are taken care of in
    // SkyObject::riseSetTime()

    KStarsDateTime dt0 = dt;
    dt0.setTime( UT );
    if ( riseT && dt0 > dt ) {
        dt0 = dt0.addDays( -1 );
    } else if ( ! riseT && dt0 < dt ) {
        dt0 = dt0.addDays( 1 );
    }

    SkyPoint sp = recomputeCoords( dt0, geo );
    UT = auxRiseSetTimeUT( dt0, geo, &sp.ra(), &sp.dec(), riseT );

    if ( exact ) {
        // We iterate a second time (For the Moon the second iteration changes
        // aprox. 1.5 arcmin the coordinates).
        dt0.setTime( UT );
        sp = recomputeCoords( dt0, geo );
        UT = auxRiseSetTimeUT( dt0, geo, &sp.ra(), &sp.dec(), riseT );
    }

    return UT;
}

QTime SkyObject::auxRiseSetTimeUT( const KStarsDateTime &dt, const GeoLocation *geo,
                                   const dms *righta, const dms *decl, bool riseT) {
    dms LST = auxRiseSetTimeLST( geo->lat(), righta, decl, riseT );
    return dt.GSTtoUT( geo->LSTtoGST( LST ) );
}

dms SkyObject::auxRiseSetTimeLST( const dms *gLat, const dms *righta, const dms *decl, bool riseT ) {
    dms h0 = elevationCorrection();
    double H = approxHourAngle ( &h0, gLat, decl );
    dms LST;

    if ( riseT )
        LST.setH( 24.0 + righta->Hours() - H/15.0 );
    else
        LST.setH( righta->Hours() + H/15.0 );

    return LST.reduce();
}


dms SkyObject::riseSetTimeAz( const KStarsDateTime &dt, const GeoLocation *geo, bool riseT ) {
    dms Azimuth;
    double AltRad, AzRad;
    double sindec, cosdec, sinlat, coslat, sinHA, cosHA;
    double sinAlt, cosAlt;

    QTime UT = riseSetTimeUT( dt, geo, riseT );
    KStarsDateTime dt0 = dt;
    dt0.setTime( UT );
    SkyPoint sp = recomputeCoords( dt0, geo );

    dms LST = auxRiseSetTimeLST( geo->lat(), &sp.ra0(), &sp.dec0(), riseT );
    dms HourAngle = dms( LST.Degrees() - sp.ra0().Degrees() );

    geo->lat()->SinCos( sinlat, coslat );
    dec().SinCos( sindec, cosdec );
    HourAngle.SinCos( sinHA, cosHA );

    sinAlt = sindec*sinlat + cosdec*coslat*cosHA;
    AltRad = asin( sinAlt );
    cosAlt = cos( AltRad );

    AzRad = acos( ( sindec - sinlat*sinAlt )/( coslat*cosAlt ) );
    if ( sinHA > 0.0 ) AzRad = 2.0*dms::PI - AzRad; // resolve acos() ambiguity
    Azimuth.setRadians( AzRad );

    return Azimuth;
}

QTime SkyObject::transitTimeUT( const KStarsDateTime &dt, const GeoLocation *geo ) {
    dms LST = geo->GSTtoLST( dt.gst() );

    //dSec is the number of seconds until the object transits.
    dms HourAngle = dms( LST.Degrees() - ra().Degrees() );
    int dSec = int( -3600.*HourAngle.Hours() );

    //dt0 is the first guess at the transit time.
    KStarsDateTime dt0 = dt.addSecs( dSec );

    //recompute object's position at UT0 and then find
    //transit time of this refined position
    SkyPoint sp = recomputeCoords( dt0, geo );

    HourAngle = dms ( LST.Degrees() - sp.ra().Degrees() );
    dSec = int( -3600.*HourAngle.Hours() );

    return dt.addSecs( dSec ).time();
}

QTime SkyObject::transitTime( const KStarsDateTime &dt, const GeoLocation *geo ) {
    return geo->UTtoLT( KStarsDateTime( dt.date(), transitTimeUT( dt, geo ) ) ).time();
}

dms SkyObject::transitAltitude( const KStarsDateTime &dt, const GeoLocation *geo ) {
    KStarsDateTime dt0 = dt;
    dt0.setTime( transitTimeUT( dt, geo ) );
    SkyPoint sp = recomputeCoords( dt0, geo );

    double delta = 90 - geo->lat()->Degrees() + sp.dec().Degrees();
    if( delta > 90 )
        delta = 180 - delta;
    return dms(delta);
}

double SkyObject::approxHourAngle( const dms *h0, const dms *gLat, const dms *dec ) {

    double sh0 = sin ( h0->radians() );
    double r = (sh0 - sin( gLat->radians() ) * sin(dec->radians() ))
               / (cos( gLat->radians() ) * cos( dec->radians() ) );

    double H = acos( r )/dms::DegToRad;

    return H;
}

dms SkyObject::elevationCorrection(void) {

    /* The atmospheric refraction at the horizon shifts altitude by
     * - 34 arcmin = 0.5667 degrees. This value changes if the observer
     * is above the horizon, or if the weather conditions change much.
     *
     * For the sun we have to add half the angular sie of the body, since
     * the sunset is the time the upper limb of the sun disappears below
     * the horizon, and dawn, when the upper part of the limb appears
     * over the horizon. The angular size of the sun = angular size of the
     * moon = 31' 59''.
     *
     * So for the sun the correction is = -34 - 16 = 50 arcmin = -0.8333
     *
     * This same correction should be applied to the moon however parallax
     * is important here. Meeus states that the correction should be
     * 0.7275 P - 34 arcmin, where P is the moon's horizontal parallax.
     * He proposes a mean value of 0.125 degrees if no great accuracy
     * is needed.
     */

    if ( name() == "Sun" || name() == "Moon" )
        return dms(-0.8333);
    //	else if ( name() == "Moon" )
    //		return dms(0.125);
    else                             // All sources point-like.
        return dms(-0.5667);
}

SkyPoint SkyObject::recomputeCoords( const KStarsDateTime &dt, const GeoLocation *geo ) {
    //store current position
    SkyPoint original = *this;

    // compute coords for new time jd
    KSNumbers num( dt.djd() );
    if ( isSolarSystem() && geo ) {
        dms LST = geo->GSTtoLST( dt.gst() );
        updateCoords( &num, true, geo->lat(), &LST );
    } else {
        updateCoords( &num );
    }

    //the coordinates for the date dt:
    SkyPoint sp = *this;

    // restore original coords
    setRA( original.ra().Hours() );
    setDec( original.dec().Degrees() );

    return sp;
}

QString SkyObject::typeName( int t ) {

    switch( t ) {
    case STAR:
        return i18n( "Star" );
    case CATALOG_STAR:
        return i18n( "Catalog Star" );
    case PLANET:
        return i18n( "Planet" );
    case OPEN_CLUSTER:
        return i18n( "Open Cluster" );
    case GLOBULAR_CLUSTER:
        return i18n( "Globular Cluster" );
    case GASEOUS_NEBULA:
        return i18n( "Gaseous Nebula" );
    case PLANETARY_NEBULA:
        return i18n( "Planetary Nebula" );
    case SUPERNOVA_REMNANT:
        return i18n( "Supernova Remnant" );
    case GALAXY:
        return i18n( "Galaxy" );
    case COMET:
        return i18n( "Comet" );
    case ASTEROID:
        return i18n( "Asteroid" );
    case CONSTELLATION:
        return i18n( "Constellation" );
    case MOON:
        return i18n( "Moon" );
    case GALAXY_CLUSTER:
        return i18n( "Galaxy Cluster" );
    case SATELLITE:
        return i18n( "Satellite" );
    case SUPERNOVA:
        return i18n( "Supernova" );
    case RADIO_SOURCE:
        return i18n("Radio Source");
    case ASTERISM:
        return i18n( "Asterism" );
    case DARK_NEBULA:
        return i18n( "Dark Nebula" );
    case QUASAR:
        return i18n( "Quasar" );
    case MULT_STAR:
        return i18n( "Multiple Star" );
    default:
        return i18n( "Unknown Type" );
    }

}

QString SkyObject::typeName() const {
    return typeName( Type );
}

QString SkyObject::messageFromTitle( const QString &imageTitle ) {
    QString message = imageTitle;

    //HST Image
    if ( imageTitle == i18n( "Show HST Image" ) || imageTitle.contains("HST") ) {
        message = i18n( "%1: Hubble Space Telescope, operated by STScI for NASA [public domain]", longname() );

        //Spitzer Image
    } else if ( imageTitle.contains( i18n( "Show Spitzer Image" ) ) ) {
        message = i18n( "%1: Spitzer Space Telescope, courtesy NASA/JPL-Caltech [public domain]", longname() );

        //SEDS Image
    } else if ( imageTitle == i18n( "Show SEDS Image" ) ) {
        message = i18n( "%1: SEDS, http://www.seds.org [free for non-commercial use]", longname() );

        //Kitt Peak AOP Image
    } else if ( imageTitle == i18n( "Show KPNO AOP Image" ) ) {
        message = i18n( "%1: Advanced Observing Program at Kitt Peak National Observatory [free for non-commercial use; no physical reproductions]", longname() );

        //NOAO Image
    } else if ( imageTitle.contains( i18n( "Show NOAO Image" ) ) ) {
        message = i18n( "%1: National Optical Astronomy Observatories and AURA [free for non-commercial use]", longname() );

        //VLT Image
    } else if ( imageTitle.contains( "VLT" ) ) {
        message = i18n( "%1: Very Large Telescope, operated by the European Southern Observatory [free for non-commercial use; no reproductions]", longname() );

        //All others
    } else if ( imageTitle.startsWith( i18n( "Show" ) ) ) {
        message = imageTitle.mid( imageTitle.indexOf( " " ) + 1 ); //eat first word, "Show"
        message = longname() + ": " + message;
    }

    return message;
}

//TODO: Should create a special UserLog widget that encapsulates the "default"
//message in the widget when no log exists (much like we do with dmsBox now)
void SkyObject::saveUserLog( const QString &newLog ) {
    QFile file;
    QString logs; //existing logs

    //Do nothing if:
    //+ new log is the "default" message
    //+ new log is empty
    if ( newLog == (i18n("Record here observation logs and/or data on %1.", name())) || newLog.isEmpty() )
        return;

    // header label
    QString KSLabel ="[KSLABEL:" + name() + ']';
    //However, we can't accept a star name if it has a greek letter in it:
    if ( type() == STAR ) {
        StarObject *star = (StarObject*)this;
        if ( name() == star->gname() )
            KSLabel = "[KSLABEL:" + star->gname( false ) + ']'; //"false": spell out greek letter
    }

    file.setFileName( KStandardDirs::locateLocal( "appdata", "userlog.dat" ) ); //determine filename in local user KDE directory tree.
    if ( file.open( QIODevice::ReadOnly)) {
        QTextStream instream(&file);
        // read all data into memory
        logs = instream.readAll();
        file.close();
    }

    //Remove old log entry from the logs text
    if ( ! userLog().isEmpty() ) {
        int startIndex, endIndex;
        QString sub;

        startIndex = logs.indexOf(KSLabel);
        sub = logs.mid (startIndex);
        endIndex = sub.indexOf("[KSLogEnd]");

        logs.remove(startIndex, endIndex + 11);
    }

    //append the new log entry to the end of the logs text
    logs.append( KSLabel + '\n' + newLog + "\n[KSLogEnd]\n" );

    //Open file for writing
    if ( !file.open( QIODevice::WriteOnly ) ) {
        qDebug() << i18n( "Cannot write to user log file" );
        return;
    }

    //Write new logs text
    QTextStream outstream(&file);
    outstream << logs;

    //Set the log text in the object itself.
    userLog() = newLog;

    file.close();
}

QString SkyObject::labelString() const {
    return translatedName();
}

double SkyObject::labelOffset() const {
    return SkyLabeler::ZoomOffset();
}

AuxInfo *SkyObject::getAuxInfo() {
    if( !info )
        info = new AuxInfo;
    return &(*info);
}

SkyObject::UID SkyObject::getUID() const
{
    return invalidUID;
}
