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

QTime SkyObject::setTime( long double jd, GeoLocation *geo ) {

	//this object does not rise or set; return an invalid time
	if ( checkCircumpolar(geo->lat()) )
		return QTime( 25, 0, 0 );  


	dms UT = riseUTTime (jd, geo->lng(), geo->lat(), false); 

	//convert UT to LT;
	dms LT = dms( UT.Degrees() + 15.0*geo->TZ() ).reduce();

	return QTime( LT.hour(), LT.minute(), LT.second() );
}

QTime SkyObject::riseTime( long double jd, GeoLocation *geo ) {

	//this object does not rise or set; return an invalid time
	if ( checkCircumpolar(geo->lat()) )
		return QTime( 25, 0, 0 );  

	dms UT = riseUTTime (jd, geo->lng(), geo->lat(), true); 

	//convert UT to LT;

	dms LT = dms( UT.Degrees() + 15.0*geo->TZ() ).reduce();

	return QTime( LT.hour(), LT.minute(), LT.second() );

}

dms SkyObject::riseUTTime( long double jd, dms gLng, dms gLat, bool riseT) {

	dms UT = auxRiseUTTime (jd, gLng, gLat, ra(), dec(), riseT); 
	// We iterate once more using the calculated UT to compute again 
	// the ra and dec and hence the rise time.
	
	long double jd0 = newJDfromJDandUT(jd, UT);

	SkyPoint sp = getNewCoords(jd, jd0);
	dms ram = sp.ra0();
	dms decm = sp.dec0();
	
	UT = auxRiseUTTime (jd0, gLng, gLat, ram, decm, riseT); 

	return UT;
}

dms SkyObject::auxRiseUTTime( long double jd, dms gLng, dms gLat, dms righta, dms decl, bool riseT) {

	// if riseT = true => rise Time, else setTime
	
	dms LST = auxRiseLSTTime (gLat, righta, decl, riseT ); 

	//convert LST to Greenwich ST
	dms GST = dms( LST.Degrees() - gLng.Degrees() ).reduce();

	//convert GST to UT
	double T = ( jd - J2000 )/36525.0;
	dms T0, dT, UT;
	T0.setH( 6.697374558 + (2400.051336*T) + (0.000025862*T*T) );
	T0 = T0.reduce();
	dT.setH( GST.Hours() - T0.Hours() );
	dT = dT.reduce();

	UT.setH( 0.9972695663 * dT.Hours() );
	UT = UT.reduce();

	return UT;
}

dms SkyObject::riseLSTTime( long double jd, dms gLng, dms gLat, bool riseT) {

	dms UT = riseUTTime (jd, gLng, gLat, riseT);

	QDateTime utTime = DMStoQDateTime(jd, UT);
	QTime lstTime = KSUtils::UTtoLST( utTime, gLng); 
	dms LST = QTimeToDMS(lstTime);

	return LST;
}

dms SkyObject::auxRiseLSTTime( dms gLat, dms righta, dms decl, bool riseT ) {

//	double r = -1.0 * tan( gLat.radians() ) * tan( decl.radians() );
//	double H = acos( r )*180./acos(-1.0); //180/Pi converts radians to degrees
	dms h0 = elevationCorrection();
	double H = approxHourAngle (h0, gLat, decl);

	dms LST;
	
	// rise Time or setTime

	if (riseT) 
		LST.setH( 24.0 + righta.Hours() - H/15.0 );
	else
		LST.setH( righta.Hours() + H/15.0 );

	LST = LST.reduce();

	return LST;
}


dms SkyObject::riseSetTimeAz (long double jd, GeoLocation *geo, bool riseT) {

	dms Azimuth;
	double AltRad, AzRad;
	double sindec, cosdec, sinlat, coslat, sinHA, cosHA;
	double sinAlt, cosAlt;

	dms UT = riseUTTime (jd, geo->lng(), geo->lat(), riseT);
	long double jd0 = newJDfromJDandUT(jd, UT);
	SkyPoint sp = getNewCoords(jd,jd0);
	dms ram = sp.ra0();
	dms decm = sp.dec0();

	dms LST = auxRiseLSTTime( geo->lat(), ram, decm, riseT);

	dms HourAngle = dms( LST.Degrees() - ram.Degrees() );

	geo->lat().SinCos( sinlat, coslat );
	dec().SinCos( sindec, cosdec );
	HourAngle.SinCos( sinHA, cosHA );

	sinAlt = sindec*sinlat + cosdec*coslat*cosHA;
	AltRad = asin( sinAlt );
	cosAlt = cos( AltRad );

	AzRad = acos( ( sindec - sinlat*sinAlt )/( coslat*cosAlt ) );
	// AzRad = acos( sindec /( coslat*cosAlt ) );
	if ( sinHA > 0.0 ) AzRad = 2.0*PI() - AzRad; // resolve acos() ambiguity
	Azimuth.setRadians( AzRad );

	return Azimuth;
}
dms SkyObject::transitUTTime(long double jd, dms gLng ) {

	QDateTime utDateTime;
	utDateTime = KSUtils::JDtoDateTime(jd );
	QTime lstTime;
	lstTime = KSUtils::UTtoLST( utDateTime, gLng );
	dms LST = QTimeToDMS(lstTime);
	
	dms HourAngle = dms ( LST.Degrees() - ra().Degrees() );
	int dSec = int( -3600.*HourAngle.Hours() );
	dms UT0 = QTimeToDMS( utDateTime.time() );

	UT0 = dms (UT0.Degrees() + dSec*15/3600.);
	
 long double jd0 = newJDfromJDandUT(jd, UT0);

	SkyPoint sp = getNewCoords(jd, jd0);
	dms ram = sp.ra0();

	HourAngle = dms ( LST.Degrees() - ram.Degrees() );
	dSec = int( -3600.*HourAngle.Hours() );

	dms UT = QTimeToDMS( utDateTime.time() );
	UT = dms (UT.Degrees() + dSec*15/3600.);

	return UT;
}

QTime SkyObject::transitTime( long double jd, GeoLocation *geo ) {

	dms UT = transitUTTime(jd, geo->lng() );

	dms LT = dms( UT.Degrees() + 15.0*geo->TZ() ).reduce();

	return QTime( LT.hour(), LT.minute(), LT.second() );
}

dms SkyObject::transitAltitude(long double jd, GeoLocation *geo) {

	dms delta;

	dms UT = transitUTTime (jd, geo->lng());
	long double jd0 = newJDfromJDandUT(jd, UT);
	SkyPoint sp = getNewCoords(jd,jd0);
	dms decm = sp.dec0();
	
	delta.setRadians( asin ( sin (geo->lat().radians()) * 
				sin ( decm.radians() ) +
				cos (geo->lat().radians()) * 
				cos (decm.radians() ) ) );

	return delta;
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

double SkyObject::approxHourAngle (dms h0, dms gLat, dms dec) {

	double sh0 = - sin ( h0.radians() );
	double r = (sh0 - sin( gLat.radians() ) * sin(dec.radians() ))
		 / (cos( gLat.radians() ) * cos( dec.radians() ) );

	double H = acos( r )*180./acos(-1.0);

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

long double SkyObject::newJDfromJDandUT(long double jd, dms UT) {

	QDateTime dt;
	dt = DMStoQDateTime(jd, UT);
	long double jd0 = KSUtils::UTtoJulian ( dt );

	return jd0;
}

QDateTime SkyObject::DMStoQDateTime(long double jd, dms UT) {

	QDateTime dt;
	dt = KSUtils::JDtoDateTime(jd);
	return QDateTime(QDate(dt.date().year(), dt.date().month(), dt.date().day()),QTime(UT.hour(), UT.minute(), UT.second())) ;

}

dms SkyObject::QTimeToDMS(QTime qtime) {

	dms tt;
	tt.setH(qtime.hour(), qtime.minute(), qtime.second());
	tt.reduce();

	return tt;
}

SkyPoint SkyObject::getNewCoords(long double jd, long double jd0) {

	// we save the original state of the object
	
	KSNumbers *num = new KSNumbers(jd);

	KSNumbers *num1 = new KSNumbers(jd0);
	updateCoords(num1);
	dms ram = ra();
	dms decm = dec();
	delete num1;

	// we leave the object in its original state

	updateCoords(num);
	delete num;

	return SkyPoint(ram, decm);
}

QString SkyObject::typeName( void ) const {
        if ( Type==0 ) return i18n( "Star" );
	else if ( Type==1 ) return i18n( "Catalog Star" );
	else if ( Type==2 ) return i18n( "Planet" );
	else if ( Type==3 ) return i18n( "Open Cluster" );
	else if ( Type==4 ) return i18n( "Globular Cluster" );
	else if ( Type==5 ) return i18n( "Gaseous Nebula" );
	else if ( Type==6 ) return i18n( "Planetary Nebula" );
	else if ( Type==7 ) return i18n( "Supernova Remnant" );
	else if ( Type==8 ) return i18n( "Galaxy" );
	else return i18n( "Unknown Type" );
}
