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

#include <qregexp.h>
#include <qfile.h>
#include "skyobject.h"
#include "ksutils.h"
#include "dms.h"
#include "geolocation.h"

SkyObject::SkyObject() : SkyPoint(0.0, 0.0) {
	Type = 0;
	Magnitude = 0.0;
	MajorAxis = 1.0;
	MinorAxis = 0.0;
	PositionAngle = 0;
	UGC = 0;
	PGC = 0;
	Name = "unnamed";
	Name2 = "";
	LongName = "";
	Catalog = "";
	Image = 0;
	ra1 = dms(0.);
	ra2 = dms(0.);
	ra3 = dms(0.);
	dec1 = dms(0.);
	dec2 = dms(0.);
	dec3 = dms(0.);
}

SkyObject::SkyObject( SkyObject &o ) : SkyPoint( o) {
	Type = o.type();
	Magnitude = o.mag();
	MajorAxis = o.a();
	MinorAxis = o.b();
	PositionAngle = o.pa();
	UGC = o.ugc();
	PGC = o.pgc();
	Name = o.name();
	Name2 = o.name2();
	LongName = o.longname();
	Catalog = o.catalog();
	ImageList = o.ImageList;
	ImageTitle = o.ImageTitle;
	InfoList = o.InfoList;
	InfoTitle = o.InfoTitle;
	Image = o.image();
	ra1 = o.ra1;
	ra2 = o.ra2;
	ra3 = o.ra3;
	dec1 = o.dec1;
	dec2 = o.dec2;
	dec3 = o.dec3;
}

SkyObject::SkyObject( int t, dms r, dms d, double m,
						QString n, QString n2, QString lname, QString cat,
						double a, double b, int pa, int pgc, int ugc ) : SkyPoint( r, d) {
	Type = t;
	Magnitude = m;
	MajorAxis = a;
	MinorAxis = b;
	PositionAngle = pa;
	PGC = pgc;
	UGC = ugc;
	Name = n;
	Name2 = n2;
	LongName = lname;
	Catalog = cat;
	Image = 0;
	ra1 = dms(0.);
	ra2 = dms(0.);
	ra3 = dms(0.);
	dec1 = dms(0.);
	dec2 = dms(0.);
	dec3 = dms(0.);
}

SkyObject::SkyObject( int t, double r, double d, double m,
						QString n, QString n2, QString lname, QString cat,
						double a, double b, int pa, int pgc, int ugc ) : SkyPoint( r, d) {
	Type = t;
	Magnitude = m;
	MajorAxis = a;
	MinorAxis = b;
	PositionAngle = pa;
	PGC = pgc;
	UGC = ugc;
	Name = n;
	Name2 = n2;
	LongName = lname;
	Catalog = cat;
	Image = 0;
}

QTime SkyObject::riseTime( long double jd, GeoLocation *geo ) {
	double r = -1.0 * tan( geo->lat().radians() ) * tan( dec().radians() );
	if ( r < -1.0 || r > 1.0 )
		return QTime( 25, 0, 0 );  //this object does not rise or set; return an invalid time
		
	double H = acos( r )*180./acos(-1.0); //180/Pi converts radians to degrees
	dms LST;
	LST.setH( 24.0 + ra().Hours() - H/15.0 );
	LST = LST.reduce();

	//convert LST to Greenwich ST
	dms GST = dms( LST.Degrees() - geo->lng().Degrees() ).reduce();

	//convert GST to UT
	double T = ( jd - J2000 )/36525.0;
	dms T0, dT, UT;
	T0.setH( 6.697374558 + (2400.051336*T) + (0.000025862*T*T) );
	T0 = T0.reduce();
	dT.setH( GST.Hours() - T0.Hours() );
	dT = dT.reduce();
	UT.setH( 0.9972695663 * dT.Hours() );
	UT = UT.reduce();

	//convert UT to LT;
	dms LT = dms( UT.Degrees() + 15.0*geo->TZ() ).reduce();

	return QTime( LT.hour(), LT.minute(), LT.second() );
}

QTime SkyObject::setTime( long double jd, GeoLocation *geo ) {
	double r = -1.0 * tan( geo->lat().radians() ) * tan( dec().radians() );
	if ( r < -1.0 || r > 1.0 )
		return QTime( 25, 0, 0 );  //this object does not rise or set; return an invalid time
		
	double H = acos( r )*180./acos(-1.0);
	dms LST;

	//the following line is the only difference between riseTime() and setTime()
	LST.setH( ra().Hours() + H/15.0 );
	LST = LST.reduce();

	//convert LST to Greenwich ST
	dms GST = dms( LST.Degrees() - geo->lng().Degrees() ).reduce();

	//convert GST to UT
	double T = ( jd - J2000 )/36525.0;
	dms T0, dT, UT;
	T0.setH( 6.697374558 + (2400.051336*T) + (0.000025862*T*T) );
	T0 = T0.reduce();
	dT.setH( GST.Hours() - T0.Hours() );
	dT = dT.reduce();
	UT.setH( 0.9972695663 * dT.Hours() );
  UT = UT.reduce();

	//convert UT to LT;
	dms LT = dms( UT.Degrees() + 15.0*geo->TZ() ).reduce();
	return QTime( LT.hour(), LT.minute(), LT.second() );
}

QTime SkyObject::transitTime( long double jd, GeoLocation *geo ) {

	if ( checkCircumpolar(geo->lat() ) )
		return QTime( 25, 0, 0 );

	double UT = transitUTTime (jd, geo->lng(), geo->lat());

	dms LT = dms( UT + 15.0*geo->TZ() ).reduce();

	return QTime( LT.hour(), LT.minute(), LT.second() );
}

double SkyObject::transitUTTime( long double jd, dms gLng, dms gLat ) {

	setThreeCoords (jd);
	dms h0 = elevationCorrection();
//  No need for transitTime, but necessary for riseTime and setTime.
//	double H = approxHourAngle( h0, gLat );
	dms ts0 = gstAtCeroUT(jd);

	double m0 = ra2.Degrees()+ gLng.Degrees() - ts0.Degrees();

	m0 = m0 / 360. ;
	reduceToOne(m0);
	double ts_0 = ts0.Degrees() + 360.985647 * m0;
	double ram = Interpolate ( ra1.Degrees(), ra2.Degrees(), ra3.Degrees(), m0);
	double HourAngle = ts_0 - gLng.Degrees() - ram;
	reduceTo180(HourAngle);

	double UT = (m0 - HourAngle/360.) * 24.;

	return UT;
}

dms SkyObject::transitAltitude(GeoLocation *geo) {

	double delta = asin ( sin (geo->lat().radians()) * 
				sin ( dec().radians() ) +
				cos (geo->lat().radians()) * 
				cos (dec().radians() ) );

	return dms(delta);
}

float SkyObject::e( void ) const {
	if ( MajorAxis==0.0 || MinorAxis==0.0 ) return 1.0; //assume circular
	return MinorAxis / MajorAxis;
}

QImage* SkyObject::readImage( void ) {
	QFile file;
	if ( Image==0 ) { //Image not currently set; try to load it from disk.
		QString fname = Name.lower().replace( QRegExp(" "), "" ) + ".png";

		if ( KSUtils::openDataFile( file, fname ) ) {
			file.close();
			Image = new QImage( file.name(), "PNG" );
		}
	}

	return Image;
}

dms SkyObject::gstAtCeroUT (long double jd) {

	long double jd0 = KSUtils::JdAtZeroUT (jd) ;
	long double s = jd0 - 2451545.0;
	double t = s/36525.0;
	dms T0;
	T0.setH (6.697374558 + 2400.051336*t + 0.000025862*t*t + 
			0.000000002*t*t*t);

	return T0;
}

double SkyObject::approxHourAngle (dms h0, dms gLat) {

	double sh0 = sin ( h0.radians() );
	double r = (sh0 - sin( gLat.radians() ) * sin(dec2.radians() ))
		 / (cos( gLat.radians() ) * cos( dec2.radians() ) );

	double H = acos( r )*180./acos(-1.0);

	return H;
}

void SkyObject::setThreeCoords (long double jd) {

	// We save the original state of the object

	KSNumbers *num = new KSNumbers(jd);

	long double jd2 = KSUtils::JdAtZeroUT(jd);


	KSNumbers *num1 = new KSNumbers(jd2-1);
	updateCoords(num1);
	dms ra1 = ra();
	dms dec1 = dec();
	delete num1;

	KSNumbers *num2 = new KSNumbers(jd2);
	updateCoords(num2);
	dms ra2 = ra();
	dms dec2 = dec();
	delete num2;

	KSNumbers *num3 = new KSNumbers(jd2+1);
	updateCoords(num3);
	dms ra3 = ra();
	dms dec3 = dec();
	delete num3;

	// we leave the object in its original state
	
	updateCoords(num);
	delete num;
}

double SkyObject::Interpolate (double y1, double y2, double y3, double n) {

	double a = y2 - y1;
	double b = y3 - y2;
	double c = y1 + y3 - 2*y2;

	return (y2 + (a + b + n*c) * n /2. );
}

double SkyObject::reduceToOne(double m) {

	while (m<0.0) {m+= 1.0;}
	while (m>=1.0) {m -= 1.0;}

	return m;
}

double SkyObject::reduceTo180(double H) {

	while (H<-180) {H+= 360.0;}
	while (H>=180.0) {H -= 360.0;}

	return H;
}

bool SkyObject::checkCircumpolar(dms gLat) {
	double r = -1.0 * tan( gLat.radians() ) * tan( dec().radians() );
	if ( r < -1.0 || r > 1.0 )
		return true;
	else
		return false;
}

dms SkyObject::elevationCorrection(void) {
	if ( name() == "Sun"  )
		return dms(0.5667);
	else if ( name() == "Moon" )
		return dms(0.125); // a rough approximation
	else
		return dms(0.8333);
}
