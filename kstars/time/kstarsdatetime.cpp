/***************************************************************************
                         kstarsdatetime.cpp  -  K Desktop Planetarium
                            -------------------
   begin                : Tue 05 May 2004
   copyright            : (C) 2004 by Jason Harris
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

#include "kstarsdatetime.h"

#include <QDebug>
#include <KLocalizedString>

#include "ksnumbers.h"
#include "dms.h"

KStarsDateTime::KStarsDateTime() : QDateTime()
{
    setDJD( J2000 );
}

KStarsDateTime::KStarsDateTime( const KStarsDateTime &kdt ) : QDateTime()
{
    setDJD( kdt.djd() );
    setUtcOffset(kdt.utcOffset());
}

/*KStarsDateTime::KStarsDateTime( const QDateTime &kdt ) :
    QDateTime( kdt )
{
    //don't call setDJD() because we don't need to compute the time; just set DJD directly
    QTime _t = kdt.time();
    QDate _d = kdt.date();
    long double jdFrac = ( _t.hour()-12 + ( _t.minute() + ( _t.second() + _t.msec()/1000.)/60.)/60.)/24.;
    DJD = (long double)( _d.toJulianDay() ) + jdFrac;
}*/

KStarsDateTime::KStarsDateTime( const QDateTime &qdt ) :
    QDateTime( qdt )//, QDateTime::Spec::UTC() )
{
    // FIXME: This method might be buggy. Need to write some tests -- asimha (Oct 2016)
    QTime _t = qdt.time();
    QDate _d = qdt.date();
    long double jdFrac = ( _t.hour()-12 + ( _t.minute() + ( _t.second() + _t.msec()/1000.)/60.)/60.)/24.;
    DJD = (long double)( _d.toJulianDay() ) + jdFrac;
    setUtcOffset(qdt.utcOffset());
}

KStarsDateTime::KStarsDateTime( const QDate &_d, const QTime &_t ) :
    //QDateTime( _d, _t, QDateTime::Spec::UTC() )
    QDateTime( _d, _t, Qt::UTC )
{
    //don't call setDJD() because we don't need to compute the time; just set DJD directly
    long double jdFrac = ( _t.hour()-12 + ( _t.minute() + ( _t.second() + _t.msec()/1000.)/60.)/60.)/24.;
    DJD = (long double)( _d.toJulianDay() ) + jdFrac;
}

KStarsDateTime::KStarsDateTime( long double _jd ) : QDateTime() {
    setDJD( _jd );
}

//KStarsDateTime KStarsDateTime::currentDateTime( QDateTime::Spec spec ) {
KStarsDateTime KStarsDateTime::currentDateTime() {
    KStarsDateTime dt( QDateTime::currentDateTime() );
   // if ( dt.time().hour()==0 && dt.time().minute()==0 )        // midnight or right after?
     //   dt.setDate( QDateTime::currentDateTime(spec).date() ); // fetch date again

    return dt;
}

KStarsDateTime KStarsDateTime::currentDateTimeUtc( ) {
    KStarsDateTime dt( QDateTime::currentDateTimeUtc() );
    //if ( dt.time().hour()==0 && dt.time().minute()==0 )        // midnight or right after?
//        dt.setDate( QDateTime::currentDateTime(spec).date() ); // fetch date again

    return dt;
}

KStarsDateTime KStarsDateTime::fromString( const QString &s ) {
    //DEBUG
    qDebug() << "Date string: " << s;

    KStarsDateTime dtResult = QDateTime::fromString( s, Qt::TextDate );
    if ( dtResult.isValid() )
        return dtResult;

    dtResult = QDateTime::fromString( s, Qt::ISODate );
    if ( dtResult.isValid() )
        return dtResult;

    //dtResult = QDateTime::fromString( s, QDateTime::RFCDate );
    dtResult = QDateTime::fromString( s, Qt::RFC2822Date );
    if ( dtResult.isValid() )
        return dtResult;

    qWarning() << i18n( "Could not parse Date/Time string: " ) << s ;
    qWarning() << i18n( "Valid date formats: " ) ;
    qWarning() << "  1950-02-25   ;  1950-02-25T05:30:00" ;
    qWarning() << "  25 Feb 1950  ;  25 Feb 1950 05:30:00" ;
    qWarning() << "  Sat Feb 25 1950  ;  Sat Feb 25 05:30:00 1950";
    return KStarsDateTime( QDateTime() ); //invalid
}

void KStarsDateTime::setDJD( long double _jd ) {
    //QDateTime::setTimeSpec( QDateTime::Spec::UTC() );
    QDateTime::setTimeSpec( Qt::UTC );

    DJD = _jd;
    long int ijd = (long int)_jd;
    double dayfrac = _jd - (double)ijd + 0.5;
    if ( dayfrac > 1.0 ) { ijd++; dayfrac -= 1.0; }

    QDate dd = QDate::fromJulianDay( ijd );
    QDateTime::setDate( dd );

    double hour = 24.*dayfrac;
    int h = int(hour);
    int m = int( 60.*(hour - h) );
    int s = int( 60.*(60.*(hour - h) - m) );
    int ms = int( 1000.*(60.*(60.*(hour - h) - m) - s) );

    QDateTime::setTime( QTime( h, m, s, ms ) );
}

void KStarsDateTime::setDate( const QDate &_d ) {
    //Save the JD fraction
    long double jdFrac = djd() - (long double)( date().toJulianDay() );

    //set the integer portion of the JD and add back the JD fraction:
    setDJD( (long double)_d.toJulianDay() + jdFrac );
}

KStarsDateTime KStarsDateTime::addSecs( double s ) const {
    long double ds = (long double)s/86400.;
    KStarsDateTime kdt( djd() + ds );
    return kdt;
}

void KStarsDateTime::setTime( const QTime &_t ) {
    KStarsDateTime _dt( date(), _t );
    setDJD( _dt.djd() );
}

dms KStarsDateTime::gst() const {
    dms gst0 = GSTat0hUT();

    double hr = double( time().hour() );
    double mn = double( time().minute() );
    double sc = double( time().second() ) + double ( 0.001 * time().msec() );
    double st = (hr + ( mn + sc/60.0)/60.0)*SIDEREALSECOND;

    dms gst = dms( gst0.Degrees() + st*15.0 ).reduce();
    return gst;
}

dms KStarsDateTime::GSTat0hUT() const {
    double sinOb, cosOb;

    // Mean greenwich sidereal time
    KStarsDateTime t0( date(), QTime( 0, 0, 0 ) );
    long double s = t0.djd() - J2000;
    double t = s/36525.0;
    double t1 = 6.697374558 + 2400.051336*t + 0.000025862*t*t +
                0.000000002*t*t*t;

    // To obtain the apparent sidereal time, we have to correct the
    // mean greenwich sidereal time with nutation in longitude multiplied
    // by the cosine of the obliquity of the ecliptic. This correction
    // is called nutation in right ascention, and may amount to 0.3 secs.
    KSNumbers num( t0.djd() );
    num.obliquity()->SinCos( sinOb, cosOb );

    // nutLong has to be in hours of time since t1 is hours of time.
    double nutLong = num.dEcLong()*cosOb/15.0;
    t1 += nutLong;

    dms gst;
    gst.setH( t1 );
    return gst.reduce();
}

QTime KStarsDateTime::GSTtoUT( dms GST ) const {
    dms gst0 = GSTat0hUT();

    //dt is the number of sidereal hours since UT 0h.
    double dt = GST.Hours() - gst0.Hours();
    while ( dt < 0.0 ) dt += 24.0;
    while ( dt >= 24.0 ) dt -= 24.0;

    //convert to solar time.  dt is now the number of hours since 0h UT.
    dt /= SIDEREALSECOND;

    int hr = int( dt );
    int mn = int( 60.0*( dt - double( hr ) ) );
    int sc = int( 60.0*( 60.0*( dt - double( hr ) ) - double( mn ) ) );
    int ms = int( 1000.0*( 60.0*( 60.0*( dt - double(hr) ) - double(mn) ) - double(sc) ) );

    return( QTime( hr, mn, sc, ms ) );
}

void KStarsDateTime::setFromEpoch( double epoch ) {
    if ( epoch == 1950.0 ) // Assume Besselian
        setFromEpoch( epoch, BESSELIAN );
    else
        setFromEpoch( epoch, JULIAN ); // Assume Julian
}

bool KStarsDateTime::setFromEpoch( double epoch, EpochType type ) {
    if ( type != JULIAN && type != BESSELIAN )
        return false;
    else
        setDJD( epochToJd( epoch, type ) );
    return true;
}

bool KStarsDateTime::setFromEpoch( const QString &eName ) {
    bool result;
    double epoch;
    epoch = stringToEpoch( eName, result );

    if( !result )
        return false;
    return setFromEpoch( epoch, JULIAN ); // We've already converted
}


long double KStarsDateTime::epochToJd(double epoch, EpochType type) {
    switch( type ) {
    case BESSELIAN:
        return B1900 + ( epoch - 1900.0 ) * JD_PER_BYEAR;
    case JULIAN:
        return J2000 + ( epoch - 2000.0 ) * 365.25;
    default:
        return NaN::d;
    }
}


double KStarsDateTime::jdToEpoch(long double jd, KStarsDateTime::EpochType type) {
    // Definitions for conversion formulas are from:
    //
    // * http://scienceworld.wolfram.com/astronomy/BesselianEpoch.html
    // * http://scienceworld.wolfram.com/astronomy/JulianEpoch.html
    //

    switch( type ) {
    case KStarsDateTime::BESSELIAN:
        return 1900.0 + ( jd - KStarsDateTime::B1900 )/KStarsDateTime::JD_PER_BYEAR;
    case KStarsDateTime::JULIAN:
        return 2000.0 + ( jd - J2000 )/365.24;
    default:
        return NaN::d;
    }
}


double KStarsDateTime::stringToEpoch(const QString& eName, bool &ok)
{
    double epoch = J2000;
    ok = false;

    if ( eName.isEmpty() ) // By default, assume J2000
        return epoch;

    if ( eName.startsWith( 'J' ) )
        epoch = eName.mid( 1 ).toDouble(&ok);
    else if ( eName.startsWith( 'B' ) )
    {
        epoch = eName.mid( 1 ).toDouble(&ok);
        epoch = jdToEpoch( epochToJd( epoch, BESSELIAN ), JULIAN ); // Convert Besselian epoch to Julian epoch
    }
    // Assume it's Julian
    else
        epoch = eName.toDouble(&ok);

    return epoch;
}
