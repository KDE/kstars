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

#include <kdebug.h>
 
#include "kstarsdatetime.h"
#include "ksnumbers.h"
#include "dms.h"

KStarsDateTime::KStarsDateTime() : ExtDateTime() {
	setDJD( J2000 );
}

KStarsDateTime::KStarsDateTime( long int _jd ) : ExtDateTime(){
	setDJD( (long double)( _jd ) );
}

KStarsDateTime::KStarsDateTime( const KStarsDateTime &kdt ) : ExtDateTime() {
	setDJD( kdt.djd() );
}

KStarsDateTime::KStarsDateTime( const ExtDateTime &edt ) : ExtDateTime( edt ) {
	//don't call setDJD() because we don't need to compute the time; just set DJD directly
	QTime _t = edt.time();
	ExtDate _d = edt.date();
	long double jdFrac = ( _t.hour()-12 + ( _t.minute() + ( _t.second() + _t.msec()/1000.)/60.)/60.)/24.;
	DJD = (long double)( _d.jd() ) + jdFrac;
}

KStarsDateTime::KStarsDateTime( const ExtDate &_d, const QTime &_t ) : ExtDateTime( _d, _t ) {
	//don't call setDJD() because we don't need to compute the time; just set DJD directly
	long double jdFrac = ( _t.hour()-12 + ( _t.minute() + ( _t.second() + _t.msec()/1000.)/60.)/60.)/24.;
	DJD = (long double)( _d.jd() ) + jdFrac;
}

KStarsDateTime::KStarsDateTime( double _jd ) : ExtDateTime() {
	setDJD( (long double)_jd );
}

KStarsDateTime::KStarsDateTime( long double _jd ) : ExtDateTime() {
	setDJD( _jd );
}

KStarsDateTime KStarsDateTime::currentDateTime() {
	KStarsDateTime dt( ExtDate::currentDate(), QTime::currentTime() );
	if ( dt.time().hour()==0 && dt.time().minute()==0 ) // midnight or right after?
			dt.setDate( ExtDate::currentDate() );         // fetch date again
	
	return dt;
}

void KStarsDateTime::setDJD( long double _jd ) {
	DJD = _jd;

	ExtDate dd;
	dd.setJD( (long int)( _jd + 0.5 ) );
	ExtDateTime::setDate( dd );

	double dayfrac = _jd - (double)( date().jd() ) + 0.5;
	if ( dayfrac > 1.0 ) dayfrac -= 1.0;
	double hour = 24.*dayfrac;
	int h = int(hour);
	int m = int( 60.*(hour - h) );
	int s = int( 60.*(60.*(hour - h) - m) );
	int ms = int( 1000.*(60.*(60.*(hour - h) - m) - s) );

	ExtDateTime::setTime( QTime( h, m, s, ms ) );
}

void KStarsDateTime::setDate( const ExtDate &_d ) {
	//Save the JD fraction
	long double jdFrac = djd() - (long double)( date().jd() );
	
	//set the integer portion of the JD and add back the JD fraction:
	setDJD( (long double)_d.jd() + jdFrac );
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
	if (epoch == 1950.0) {
		setDJD( 2433282.4235 );
	} else if ( epoch == 2000.0 ) {
		setDJD( J2000 );
	} else {
		int year = int( epoch );
		KStarsDateTime dt( ExtDate( year, 1, 1 ), QTime( 0, 0, 0 ) );
		double days = (double)(dt.date().daysInYear())*( epoch - (double)year );
		dt = dt.addSecs( days*86400. ); //set date and time based on the number of days into the year
		setDJD( dt.djd() );
	}
}
