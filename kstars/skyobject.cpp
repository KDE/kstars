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

#include <iostream>

#include <kglobal.h>
#include <kstandarddirs.h>
#include <qpoint.h>
#include <qregexp.h>
#include <qfile.h>
#include <qtextstream.h>

#include "skyobject.h"
#include "starobject.h" //needed in saveUserLog()
#include "ksnumbers.h"
#include "dms.h"
#include "geolocation.h"
#include "kstarsdatetime.h"

QString SkyObject::emptyString = QString("");
QString SkyObject::unnamedString = QString(i18n("unnamed"));
QString SkyObject::unnamedObjectString = QString(i18n("unnamed object"));
QString SkyObject::starString = QString("star");

SkyObject::SkyObject( SkyObject &o ) : SkyPoint( o ) {
	setType( o.type() );
	Magnitude = o.mag();
	setName(o.name());
	setName2(o.name2());
	setLongName(o.longname());
	ImageList = o.ImageList;
	ImageTitle = o.ImageTitle;
	InfoList = o.InfoList;
	InfoTitle = o.InfoTitle;
}

SkyObject::SkyObject( int t, dms r, dms d, float m,
						QString n, QString n2, QString lname ) : SkyPoint( r, d) {
	setType( t );
	Magnitude = m;
	Name = 0;
	setName(n);
	Name2 = 0;
	setName2(n2);
	LongName = 0;
	setLongName(lname);
}

SkyObject::SkyObject( int t, double r, double d, float m,
						QString n, QString n2, QString lname ) : SkyPoint( r, d) {
	setType( t );
	Magnitude = m;
	Name = 0;
	setName(n);
	Name2 = 0;
	setName2(n2);
	LongName = 0;
	setLongName(lname);
}

SkyObject::~SkyObject() {
	delete Name;
	delete Name2;
	delete LongName;
}

void SkyObject::setLongName( const QString &longname ) {
	delete LongName;
	if ( longname.isEmpty() ) {
		if ( hasName() )
			LongName = new QString(translatedName());
		else if ( hasName2() )
			LongName = new QString(*Name2);
		else
			LongName = 0;
	} else {
		LongName = new QString(longname);
	}
}

QTime SkyObject::riseSetTime( const KStarsDateTime &dt, const GeoLocation *geo, bool rst ) {
	//this object does not rise or set; return an invalid time
	if ( checkCircumpolar(geo->lat()) )
		return QTime( 25, 0, 0 );

	//First of all, if the object is below the horizon at date/time dt, adjust the time 
	//to bring it above the horizon
	KStarsDateTime dt2 = dt;
	SkyPoint p = recomputeCoords( dt, geo );
	p.EquatorialToHorizontal( &(geo->GSTtoLST( dt.gst() )), geo->lat() );
	if ( p.alt()->Degrees() < 0.0 ) {
		if ( p.az()->Degrees() < 180.0 ) { //object has not risen yet
			dt2 = dt.addSecs( 12.*3600. );
		} else { //object has already set
			dt2 = dt.addSecs( -12.*3600. );
		}
	}
	
	return geo->UTtoLT( KStarsDateTime( dt2.date(), riseSetTimeUT( dt2, geo, rst ) ) ).time();
}

QTime SkyObject::riseSetTimeUT( const KStarsDateTime &dt, const GeoLocation *geo, bool riseT ) {
	// First trial to calculate UT
	QTime UT = auxRiseSetTimeUT( dt, geo, ra(), dec(), riseT );
	
	// We iterate once more using the calculated UT to compute again
	// the ra and dec for that time and hence the rise/set time.
	// Also, adjust the date by +/- 1 day, if necessary
	KStarsDateTime dt0 = dt;
	dt0.setTime( UT );
	if ( riseT && dt0 > dt ) {
		dt0 = dt0.addDays( -1 );
	} else if ( ! riseT && dt0 < dt ) {
		dt0 = dt0.addDays( 1 );
	}
	
	SkyPoint sp = recomputeCoords( dt0, geo );
	UT = auxRiseSetTimeUT( dt0, geo, sp.ra(), sp.dec(), riseT );

	// We iterate a second time (For the Moon the second iteration changes
	// aprox. 1.5 arcmin the coordinates).
	dt0.setTime( UT );
	sp = recomputeCoords( dt0, geo );
	UT = auxRiseSetTimeUT( dt0, geo, sp.ra(), sp.dec(), riseT );
	return UT;
}

dms SkyObject::riseSetTimeLST( const KStarsDateTime &dt, const GeoLocation *geo, bool riseT ) {
	KStarsDateTime rst( dt.date(), riseSetTimeUT( dt, geo, riseT) );
	return geo->GSTtoLST( rst.gst() );
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
	if ( sinHA > 0.0 ) AzRad = 2.0*dms::PI - AzRad; // resolve acos() ambiguity
	Azimuth.setRadians( AzRad );

	return Azimuth;
}

QTime SkyObject::transitTimeUT( const KStarsDateTime &dt, const GeoLocation *geo ) {
	dms LST = geo->GSTtoLST( dt.gst() );

	//dSec is the number of seconds until the object transits.
	dms HourAngle = dms( LST.Degrees() - ra()->Degrees() );
	int dSec = int( -3600.*HourAngle.Hours() );

	//dt0 is the first guess at the transit time.
	KStarsDateTime dt0 = dt.addSecs( dSec );

	//recompute object's position at UT0 and then find
	//transit time of this refined position
	SkyPoint sp = recomputeCoords( dt0, geo );
	const dms *ram = sp.ra0();

	HourAngle = dms ( LST.Degrees() - ram->Degrees() );
	dSec = int( -3600.*HourAngle.Hours() );

	return dt.addSecs( dSec ).time();
}

QTime SkyObject::transitTime( const KStarsDateTime &dt, const GeoLocation *geo ) {
	return geo->UTtoLT( KStarsDateTime( dt.date(), transitTimeUT( dt, geo ) ) ).time();
}

dms SkyObject::transitAltitude( const KStarsDateTime &dt, const GeoLocation *geo ) {
	KStarsDateTime dt0 = dt;
	QTime UT = transitTimeUT( dt, geo );
	dt0.setTime( UT );
	SkyPoint sp = recomputeCoords( dt0, geo );
	const dms *decm = sp.dec0();

	dms delta;
	delta.setRadians( asin ( sin (geo->lat()->radians()) *
				sin ( decm->radians() ) +
				cos (geo->lat()->radians()) *
				cos (decm->radians() ) ) );

	return delta;
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
	SkyPoint original( ra(), dec() );

	// compute coords for new time jd
	KSNumbers num( dt.djd() );
	if ( isSolarSystem() && geo ) {
		dms LST = geo->GSTtoLST( dt.gst() );
		updateCoords( &num, true, geo->lat(), &LST );
	} else {
		updateCoords( &num );
	}

	//the coordinates for the date dt:
	SkyPoint sp = SkyPoint( ra(), dec() );

	// restore original coords
	setRA( original.ra()->Hours() );
	setDec( original.dec()->Degrees() );

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
void SkyObject::setName( const QString &name ) {
//	if (name == "star" ) kdDebug() << "name == star" << endl;
	delete Name;
	if (!name.isEmpty())
		Name = new QString(name);
	else
		{ Name = 0; /*kdDebug() << "name saved" << endl;*/ }
}

void SkyObject::setName2( const QString &name2 ) {
	delete Name2;
	if (!name2.isEmpty())
		Name2 = new QString(name2);
	else
		{ Name2 = 0; /*kdDebug() << "name2 saved" << endl;*/ }
}

QString SkyObject::messageFromTitle( const QString &imageTitle ) {
	QString message = imageTitle;

	//HST Image
	if ( imageTitle == i18n( "Show HST Image" ) || imageTitle.contains("HST") ) {
		message = i18n( "%1: Hubble Space Telescope, operated by STScI for NASA [public domain]" ).arg( longname() );  

	//Spitzer Image
	} else if ( imageTitle.contains( i18n( "Show Spitzer Image" ) ) ) {
		message = i18n( "%1: Spitzer Space Telescope, courtesy NASA/JPL-Caltech [public domain]" ).arg( longname() );  

	//SEDS Image
	} else if ( imageTitle == i18n( "Show SEDS Image" ) ) {
		message = i18n( "%1: SEDS, http://www.seds.org [free for non-commercial use]" ).arg( longname() );

	//Kitt Peak AOP Image
	} else if ( imageTitle == i18n( "Show KPNO AOP Image" ) ) {
		message = i18n( "%1: Advanced Observing Program at Kitt Peak National Observatory [free for non-commercial use; no physical reproductions]" ).arg( longname() );

	//NOAO Image
	} else if ( imageTitle.contains( i18n( "Show NOAO Image" ) ) ) {
		message = i18n( "%1: National Optical Astronomy Observatories and AURA [free for non-commercial use]" ).arg( longname() );

	//VLT Image
	} else if ( imageTitle.contains( "VLT" ) ) {
		message = i18n( "%1: Very Large Telescope, operated by the European Southern Observatory [free for non-commercial use; no reproductions]" ).arg( longname() );

	//All others
	} else if ( imageTitle.startsWith( i18n( "Show" ) ) ) {
		message = imageTitle.mid( imageTitle.find( " " ) + 1 ); //eat first word, "Show"
		message = longname() + ": " + message;
	}
	
	return message;
}

//New saveUserLog, moved from DetailDialog.  
//Should create a special UserLog widget that encapsulates the "default"
//message in the widget when no log exists (much like we do with dmsBox now)
void SkyObject::saveUserLog( const QString &newLog ) {
	QFile file;
	QString logs; //existing logs
	
	//Do nothing if new log is the "default" message
	//(keep going if new log is empty; we'll want to delete its current entry)
	if ( newLog == (i18n("Record here observation logs and/or data on %1.").arg(name())) || newLog.isEmpty() )
		return;

	// header label
	QString KSLabel ="[KSLABEL:" + name() + "]";
	//However, we can't accept a star name if it has a greek letter in it:
	if ( type() == STAR ) {
		StarObject *star = (StarObject*)this;
		if ( name() == star->gname() ) 
			KSLabel = "[KSLABEL:" + star->gname( false ) + "]"; //"false": spell out greek letter
	}
	
	file.setName( locateLocal( "appdata", "userlog.dat" ) ); //determine filename in local user KDE directory tree.
	if ( file.open( IO_ReadOnly)) {
		QTextStream instream(&file);
		// read all data into memory
		logs = instream.read();
		file.close();
	}
	
	//Remove old log entry from the logs text
	if ( ! userLog.isEmpty() ) {
		int startIndex, endIndex;
		QString sub;
	
		startIndex = logs.find(KSLabel);
		sub = logs.mid (startIndex);
		endIndex = sub.find("[KSLogEnd]");
	
		logs.remove(startIndex, endIndex + 11);
	}
	
	//append the new log entry to the end of the logs text,
	//but only if the log is not empty
	if ( ! newLog.stripWhiteSpace().isEmpty() )
		logs.append( KSLabel + "\n" + newLog + "\n[KSLogEnd]\n" );
	
	//Open file for writing
	//FIXME: change error message to "cannot write to user log file"
	if ( !file.open( IO_WriteOnly ) ) {
		kdDebug() << i18n( "user log file could not be opened." ) << endl;
		return;
	}
	
	//Write new logs text
	QTextStream outstream(&file);
	outstream << logs;
	
	//Set the log text in the object itself.
	userLog = newLog;
	
	file.close();
}
