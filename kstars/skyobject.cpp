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

//DEBUG
#include <kglobal.h>

#include <qregexp.h>
#include "skyobject.h"
#include "ksutils.h"
#include "dms.h"
#include "geolocation.h"
#include <iostream>

SkyObject::SkyObject( SkyObject &o ) : SkyPoint( o ) {
	setType( o.type() );
	Magnitude = o.mag();
	Name = o.name();
	Name2 = o.name2();
	LongName = o.longname();
	ImageList = o.ImageList;
	ImageTitle = o.ImageTitle;
	InfoList = o.InfoList;
	InfoTitle = o.InfoTitle;
	setLongName(o.longname());
}

SkyObject::SkyObject( int t, dms r, dms d, float m,
						QString n, QString n2, QString lname ) : SkyPoint( r, d) {
	setType( t );
	Magnitude = m;
	Name = n;
	Name2 = n2;
	setLongName( lname );
}

SkyObject::SkyObject( int t, double r, double d, float m,
						QString n, QString n2, QString lname ) : SkyPoint( r, d) {
	setType( t );
	Magnitude = m;
	Name = n;
	Name2 = n2;
	setLongName( lname );
}

void SkyObject::setLongName( const QString &longname ) {
	if ( longname.isEmpty() ) {
		if ( Name.length() )
			LongName = Name;
		else if ( Name2.length() )
			LongName = Name2;
		else
			LongName = i18n( "unnamed object" );
	} else {
		LongName = longname;
	}
}

QTime SkyObject::riseSetTime( long double jd, const GeoLocation *geo, bool rst ) {
	//this object does not rise or set; return an invalid time
	if ( checkCircumpolar(geo->lat()) )
		return QTime( 25, 0, 0 );

	QTime UT = riseSetTimeUT( jd, geo, rst );

	//convert UT to LT;
	return UT.addSecs( int( 3600.0*geo->TZ() ) );
}

QTime SkyObject::riseSetTimeUT( long double jd, const GeoLocation *geo, bool riseT) {
	// First trial to calculate UT
	
	QTime UT = auxRiseSetTimeUT(jd, geo, ra(), dec(), riseT);

	// We iterate once more using the calculated UT to compute again
	// the ra and dec for that time and hence the rise/set time.

	long double jd0 = newJDfromJDandUT( jd, UT );
	SkyPoint sp = computeCoordsForJD( jd0, geo );

	UT = auxRiseSetTimeUT(jd0, geo, sp.ra(), sp.dec(), riseT);

	// We iterate a second time (For the Moon the second iteration changes
	// aprox. 1.5 arcmin the coordinates).
	
	jd0 = newJDfromJDandUT( jd0, UT );
	sp = computeCoordsForJD( jd0, geo );

	UT = auxRiseSetTimeUT(jd0, geo, sp.ra(), sp.dec(), riseT);

	return UT;
}

dms SkyObject::riseSetTimeLST( long double jd, const GeoLocation *geo, bool riseT) {
	QTime UT = riseSetTimeUT(jd, geo, riseT);
	QDateTime utTime = KSUtils::JDtoUT( jd );
	utTime.setTime( UT );
	return KSUtils::UTtoLST( utTime, geo->lng() );
}

QTime SkyObject::auxRiseSetTimeUT( long double jd, const GeoLocation *geo,
			const dms *righta, const dms *decl, bool riseT) {

	// if riseT = true => rise Time, else setTime

	dms LST = auxRiseSetTimeLST(geo->lat(), righta, decl, riseT );
	QDateTime dt = KSUtils::JDtoUT(jd);
	QTime UT = KSUtils::LSTtoUT( LST, dt, geo->lng());

	return UT;
}

dms SkyObject::auxRiseSetTimeLST( const dms *gLat, const dms *righta, const dms *decl, bool riseT ) {

//	double r = -1.0 * tan( gLat.radians() ) * tan( decl.radians() );
//	double H = acos( r )*180./acos(-1.0); //180/Pi converts radians to degrees
	dms h0 = elevationCorrection();
	double H = approxHourAngle (&h0, gLat, decl);

	dms LST;

	// rise Time or setTime

	if (riseT)
		LST.setH( 24.0 + righta->Hours() - H/15.0 );
	else
		LST.setH( righta->Hours() + H/15.0 );

	LST = LST.reduce();

	return LST;
}


dms SkyObject::riseSetTimeAz(long double jd, const GeoLocation *geo, bool riseT) {

	dms Azimuth;
	double AltRad, AzRad;
	double sindec, cosdec, sinlat, coslat, sinHA, cosHA;
	double sinAlt, cosAlt;

	QTime UT = riseSetTimeUT( jd, geo, riseT );
	long double jd0 = newJDfromJDandUT( jd, UT );
	SkyPoint sp = computeCoordsForJD( jd0, geo );
	const dms *ram = sp.ra0();
	const dms *decm = sp.dec0();

	dms LST = auxRiseSetTimeLST( geo->lat(), ram, decm, riseT );
	dms HourAngle = dms( LST.Degrees() - ram->Degrees() );

	geo->lat()->SinCos( sinlat, coslat );
	dec()->SinCos( sindec, cosdec );
	HourAngle.SinCos( sinHA, cosHA );

	sinAlt = sindec*sinlat + cosdec*coslat*cosHA;
	AltRad = asin( sinAlt );
	cosAlt = cos( AltRad );

	AzRad = acos( ( sindec - sinlat*sinAlt )/( coslat*cosAlt ) );
	// AzRad = acos( sindec /( coslat*cosAlt ) );
	if ( sinHA > 0.0 ) AzRad = 2.0*dms::PI - AzRad; // resolve acos() ambiguity
	Azimuth.setRadians( AzRad );

	return Azimuth;
}

QTime SkyObject::transitTimeUT(long double jd, const GeoLocation *geo ) {

	QDateTime utDateTime = KSUtils::JDtoUT( jd );
	dms LST = KSUtils::UTtoLST( utDateTime, geo->lng() );

	//dSec is the number of seconds until the object transits.
	dms HourAngle = dms ( LST.Degrees() - ra()->Degrees() );
	int dSec = int( -3600.*HourAngle.Hours() );

	//UT0 is the first guess at the transit time.
	QTime UT0 = utDateTime.time().addSecs( dSec );
	long double jd0 = newJDfromJDandUT(jd, UT0);

	//recompute object's position at UT0 and then find
	//transit time of this refined position
	SkyPoint sp = computeCoordsForJD( jd0, geo );
	const dms *ram = sp.ra0();

	HourAngle = dms ( LST.Degrees() - ram->Degrees() );
	dSec = int( -3600.*HourAngle.Hours() );

	return utDateTime.time().addSecs( dSec );
}

QTime SkyObject::transitTime( long double jd, const GeoLocation *geo ) {
	QTime UT = transitTimeUT(jd, geo );
	return UT.addSecs( int( 3600.*geo->TZ() ) );
}

dms SkyObject::transitAltitude( long double jd, const GeoLocation *geo ) {
	QTime UT = transitTimeUT( jd, geo );
	long double jd0 = newJDfromJDandUT(jd, UT);
	SkyPoint sp = computeCoordsForJD( jd0, geo );
	const dms *decm = sp.dec0();

	dms delta;
	delta.setRadians( asin ( sin (geo->lat()->radians()) *
				sin ( decm->radians() ) +
				cos (geo->lat()->radians()) *
				cos (decm->radians() ) ) );

	return delta;
}

double SkyObject::approxHourAngle ( const dms *h0, const dms *gLat, const dms *dec ) {

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

long double SkyObject::newJDfromJDandUT(long double jd, QTime UT) {
	QDateTime dt = KSUtils::JDtoUT(jd);
	dt.setTime( UT );
	return KSUtils::UTtoJD( dt );
}

SkyPoint SkyObject::computeCoordsForJD(long double jd, const GeoLocation *geo ) {
	//store current position
	SkyPoint original( ra(), dec() );

	// compute coords for new time jd
	KSNumbers num(jd);
	if ( isSolarSystem() && geo ) {
//		KSPlanetBase *ksp = ksp(,);
		dms LST = KSUtils::UTtoLST( KSUtils::JDtoUT( jd ), geo->lng() );
		updateCoords( &num, true, geo->lat(), &LST );
	} else {
		updateCoords( &num );
	}

	//the coordinates for the date jd:
	SkyPoint sp = SkyPoint( ra(), dec() );

	// restore original coords
	set( original.ra(), original.dec() );

	return sp;
}

bool SkyObject::checkCircumpolar( const dms *gLat ) {
	double r = -1.0 * tan( gLat->radians() ) * tan( dec()->radians() );
	if ( r < -1.0 || r > 1.0 )
		return true;
	else
		return false;
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
	else if ( Type==9 ) return i18n( "Comet" );
	else if ( Type==10 ) return i18n( "Asteroid" );
	else return i18n( "Unknown Type" );
}
