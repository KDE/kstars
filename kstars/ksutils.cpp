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
/*
 * @author Mark Hollomon mhh@mindpsring.com
 */

#include <qdatetime.h>

#if (KDE_VERSION <=222)
#include <kstddirs.h>
#else
#include <kstandarddirs.h>
#endif

#include "dms.h"
#include "ksutils.h"

long double KSUtils::UTtoJulian(const QDateTime &t) {
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

QTime KSUtils::UTtoGST( const QDateTime &UT ) {
	
  double t1 = KSUtils::GSTat0hUT( UT );

  double hr = double( UT.time().hour() );
  double mn = double( UT.time().minute() );
  double sc = double( UT.time().second() ) + double ( 0.001 * UT.time().msec() );

  double t2 = (hr + ( mn + sc/60.0)/60.0)*1.002737909;
  double gsth = t1 + t2;

  while (gsth < 0.0) gsth += 24.0;
  while (gsth >24.0) gsth -= 24.0;

  int gh = int(gsth);
  int gm = int((gsth - gh)*60.0);
  int gs = int(((gsth - gh)*60.0 - gm)*60.0);

  QTime gst(gh, gm, gs);

  return gst;
}

QDateTime KSUtils::JDtoDateTime( long double jd ) {
	int year, month, day, seconds, msec;
	int a, b, c, d, e, alpha, z;
	double daywithDecimals, secfloat, f;
	QDateTime dateTime;

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

	dateTime = QDateTime(QDate(year,month,day));
	dateTime = dateTime.addSecs(seconds);
  dateTime.setTime( dateTime.time().addMSecs( msec ) );

	return dateTime;
}

/**Deprecated function.
void KSUtils::nutate( long double JD, double &dEcLong, double &dObliq ) {
	//This uses the pre-1984 theory of nutation.  The results
	//are accurate to 0.5 arcsec, should be fine for KStars.
	//calculations from "Practical Astronomy with Your Calculator"
	//by Peter Duffett-Smith
	//
	//Nutation is a small, quasi-periodic oscillation of the Earth's
	//spin axis (in addition to the more regular precession).  It
	//is manifested as a change in the zeropoint of the Ecliptic
	//Longitude (dEcLong) and a change in the Obliquity angle (dObliq)
	//see the Astronomy Help pages for more info.
	
	dms  L2, M2, O, O2;
	double T;
	double sin2L, cos2L, sin2M, cos2M;
	double sinO, cosO, sin2O, cos2O;
	
	T = ( JD - J2000 )/36525.0; //centuries since J2000.0
  O.setD( 125.04452 - 1934.136261*T + 0.0020708*T*T + T*T*T/450000.0 ); //ecl. long. of Moon's ascending node
	O2.setD( 2.0*O.Degrees() );
	L2.setD( 2.0*( 280.4665 + 36000.7698*T ) ); //twice mean ecl. long. of Sun
	M2.setD( 2.0*( 218.3165 + 481267.8813*T ) );//twice mean ecl. long. of Moon
		
	O.SinCos( sinO, cosO );
	O2.SinCos( sin2O, cos2O );
	L2.SinCos( sin2L, cos2L );
	M2.SinCos( sin2M, cos2M );

	dEcLong = ( -17.2*sinO - 1.32*sin2L - 0.23*sin2M + 0.21*sin2O)/3600.0; //Ecl. long. correction
	dObliq = (   9.2*cosO + 0.57*cos2L + 0.10*cos2M - 0.09*cos2O)/3600.0; //Obliq. correction
}
*/

QTime KSUtils::UTtoLST( QDateTime UT, dms longitude) {
  QTime GST = KSUtils::UTtoGST( UT );
  QTime LST = KSUtils::GSTtoLST( GST, longitude );
  return LST;
}


QTime KSUtils::GSTtoLST( QTime GST, dms longitude) {
  double lsth = double(GST.hour()) + (double(GST.minute()) +
			   double(GST.second())/60.0)/60.0 + longitude.Degrees()/15.0;

  while (lsth < 0.0) lsth += 24.0;
  while (lsth >24.0) lsth -= 24.0;

  int lh = int(lsth);
  int lm = int((lsth - lh)*60.0);
  int ls = int(((lsth - lh)*60.0 - lm)*60.0);

  QTime lst(lh, lm, ls);

  return lst;
}

bool KSUtils::openDataFile( QFile &file, QString s ) {
	bool result;
	KStandardDirs stdDirs;
	stdDirs.addPrefix(".");
	QString FileName= stdDirs.findResource( "data", "kstars/" + s );

	if ( FileName != QString::null ) {
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

double KSUtils::GSTat0hUT( QDateTime DT ) {

	QDateTime t0 = DT;
	t0.setTime( QTime(0,0,0) );
	long double jd0 = KSUtils::UTtoJulian( t0 );
	long double s = jd0 - 2451545.0;
	double t = s/36525.0;
	double t1 = 6.697374558 + 2400.051336*t + 0.000025862*t*t + 
		0.000000002*t*t*t;

	while (t1 >= 24.0) {t1 -= 24.0;}
	while (t1 <   0.0) {t1 += 24.0;}

	return t1;
}


QTime KSUtils::LSTtoUT( QDateTime LST, dms longitude) {

	double t1 = KSUtils::GSTat0hUT( LST );
	double tslocal0h = t1 + longitude.Degrees()/15.0;
	double lsth = double(LST.time().hour()) + (double(LST.time().minute()) +
			   double(LST.time().second())/60.0)/60.0;

	double uth = lsth - tslocal0h;
	while (uth >= 24.0) {uth -= 24.0;}
	while (uth <   0.0) {uth += 24.0;}

	uth = uth/1.00273790935;
	
	int uh = int(uth);
	int um = int((uth - uh)*60.0);
	int us = int(((uth - uh)*60.0 - um)*60.0);

	QTime ut(uh, um, us);

	return ut;
}

long double KSUtils::JdAtZeroUT (long double jd) {

	long double jd0 = int(jd - 0.5) + 0.5;

	return jd0;
}
