/***************************************************************************
                          ksutils.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Mon Jan  7 10:48:09 EST 2002
    copyright            : (C) 2002 by Mark Hollomon
    email                : mhh@mindspring.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <kstandarddirs.h>

#include <qfile.h>

#include "dms.h"
#include "ksutils.h"
#include "ksnumbers.h"
#include "libkdeedu/extdate/extdatetime.h"

long double KSUtils::UTtoJD(const ExtDateTime &t) {
  int year = t.date().year();
  int month = t.date().month();
  int day = t.date().day();
  int hour = t.time().hour();
  int minute = t.time().minute();
  double second = t.time().second() + 0.001*t.time().msec();
  int m, y, A, B, C, D;

  if (month > 2) {
	  m = month;
	  y = year;
  } else {
	  y = year - 1;
	  m = month + 12;
  }

 /*  If the date is after 10/15/1582, then take Pope Gregory's modification
	 to the Julian calendar into account */

	 if (( year  >1582 ) ||
		 ( year ==1582 && month  >9 ) ||
		 ( year ==1582 && month ==9 && day >15 ))
	 {
		 A = int(y/100);
		 B = 2 - A + int(A/4);
	 } else {
		 B = 0;
	 }

  if (y < 0) {
	  C = int((365.25*y) - 0.75);
  } else {
	  C = int(365.25*y);
  }

  D = int(30.6001*(m+1));

  long double d = double(day) + (double(hour) + (double(minute) + second/60.0)/60.0)/24.0;
  long double jd = B + C + D + d + 1720994.5;

  return jd;
}

ExtDateTime KSUtils::JDtoUT( long double jd ) {
	int year, month, day, seconds, msec;
	int a, b, c, d, e, alpha, z;
	double daywithDecimals, secfloat, f;
	ExtDateTime dateTime;

	jd+=0.5;
	z = int(jd);
	f = jd - z;
	if (z<2299161) {
		a = z;
	} else {
		alpha = int ((z-1867216.25)/ 36524.25);
		a = z + 1 + alpha - int(alpha / 4.0);
	}
	b = a + 1524;
	c = int ((b-122.1)/ 365.25);
	d = int (365.25*c);
	e = int ((b-d)/ 30.6001);
	daywithDecimals = b-d-int(30.6001*e)+f;

	month = (e<14) ? e-1 : e-13;
	year  = (month>2)  ? c-4716 : c-4715;
	day   =  int(daywithDecimals);
	secfloat = (daywithDecimals - day) * 86400.;
	seconds = int(secfloat);
	msec = int((secfloat - seconds) * 1000.);

	dateTime = ExtDateTime(ExtDate(year,month,day));
	dateTime = dateTime.addSecs(seconds);

	dateTime.setTime( dateTime.time().addMSecs( msec ) );

//	//DEBUG
//	kdDebug() << "seconds=" << seconds << "   msec=" << msec << "   daywithDecimals=" << daywithDecimals << endl;

	return dateTime;
}

dms KSUtils::UTtoGST( const ExtDateTime &UT ) {
	dms gst0 = KSUtils::GSTat0hUT( UT );

	double hr = double( UT.time().hour() );
	double mn = double( UT.time().minute() );
	double sc = double( UT.time().second() ) + double ( 0.001 * UT.time().msec() );
	double t = (hr + ( mn + sc/60.0)/60.0)*1.002737909;

	dms gst = dms( gst0.Degrees() + t*15.0 ).reduce();

	return gst;
}

QTime KSUtils::GSTtoUT( const dms &GST, const ExtDateTime &UT ) {
	dms gst0 = KSUtils::GSTat0hUT( UT );

	//dt is the number of sidereal hours since UT 0h.
	double dt = GST.Hours() - gst0.Hours();
	while ( dt < 0.0 ) dt += 24.0;
	while ( dt >= 24.0 ) dt -= 24.0;

	//convert to solar time.  dt is now the number of hours since 0h UT.
	dt /= 1.002737909;

	int hr = int( dt );
	int mn = int( 60.0*( dt - double( hr ) ) );
	int sc = int( 60.0*( 60.0*( dt - double( hr ) ) - double( mn ) ) );

	return QTime( hr, mn, sc );
}

dms KSUtils::GSTtoLST( const dms &GST, const dms *longitude) {
	return dms( GST.Degrees() + longitude->Degrees() );
}

dms KSUtils::LSTtoGST( const dms &LST, const dms *longitude) {
	return dms( LST.Degrees() - longitude->Degrees() );
}

dms KSUtils::UTtoLST( const ExtDateTime &UT, const dms *longitude) {
	return KSUtils::GSTtoLST( KSUtils::UTtoGST( UT ), longitude );
}

QTime KSUtils::LSTtoUT( const dms &LST, const ExtDateTime &UT, const dms *longitude) {
	dms GST = KSUtils::LSTtoGST( LST, longitude );
	return KSUtils::GSTtoUT( GST, UT );
}

dms KSUtils::GSTat0hUT( const ExtDateTime &td ) {
	double sinOb, cosOb;
	
	// Mean greenwich sidereal time
	
	ExtDateTime t0( td.date(), QTime( 0, 0, 0 ) );
	long double jd0 = KSUtils::UTtoJD( t0 );
	long double s = jd0 - J2000;
	double t = s/36525.0;
	double t1 = 6.697374558 + 2400.051336*t + 0.000025862*t*t +
		0.000000002*t*t*t;

	// To obtain the apparent sidereal time, we have to correct the
	// mean greenwich sidereal time with nutation in longitude multiplied
	// by the cosine of the obliquity of the ecliptic. This correction
	// is called nutation in right ascention, and may ammount 0.3 secs. 
	
	KSNumbers *num = new KSNumbers(jd0);
	num->obliquity()->SinCos( sinOb, cosOb );
	
	// nutLong has to be in hours of time since t1 is hours of time.
	double nutLong = num->dEcLong()*cosOb/15.0;

	t1 += nutLong;

	dms gst;
	gst.setH( t1 );

	delete num;

	return gst.reduce();
}

long double KSUtils::JDat0hUT( const ExtDateTime &UT ) {
	long double jd =  UTtoJD( UT );
	return JDat0hUT( jd );
}

long double KSUtils::JDat0hUT( long double jd ) {
	return int( jd - 0.5 ) + 0.5;
}

bool KSUtils::openDataFile( QFile &file, const QString &s ) {
	bool result;
	KStandardDirs stdDirs;
	stdDirs.addPrefix(".");
	QString FileName= stdDirs.findResource( "data", "kstars/" + s );

	if ( !FileName.isNull() ) {
		file.setName( FileName );
		if ( !file.open( IO_ReadOnly ) ) {
			result = false;
		} else {
			result = true;
		}
	} else {
		result = false;
	}

	return result;
}

double KSUtils::JdToEpoch (long double jd) {

	long double Jd2000 = 2451545.00;
	return ( 2000.0 - ( Jd2000 - jd ) / 365.2425);
}

long double KSUtils::epochToJd (double epoch) {

	double yearsTo2000 = 2000.0 - epoch;

	if (epoch == 1950.0) {
		return 2433282.4235;
	} else if ( epoch == 2000.0 ) {
		return J2000;
	} else {
		return ( J2000 - yearsTo2000 * 365.2425 );
	}

}

double KSUtils::lagrangeInterpolation( const double x[], const double v[], int n, double xval) {
	
	double value = 0;
	for (int i=1; i<n; ++i) {
		double c = 1;
		for (int j = 1; j<n;++j) 
			if (i != j)
				c *= (xval - x[j]) / (x[i] - x[j]);
		value += c *v[i];
	}
	
	return value;
					
}
