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
