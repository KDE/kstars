/***************************************************************************
                          deepskyobject.cpp  -  K Desktop Planetarium
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
#include "deepskyobject.h"
#include "ksutils.h"
#include "dms.h"
#include "geolocation.h"

DeepSkyObject::DeepSkyObject( DeepSkyObject &o )
	: SkyObject( o ) {
	MajorAxis = o.a();
	MinorAxis = o.b();
	PositionAngle = o.pa();
	UGC = o.ugc();
	PGC = o.pgc();
	setCatalog( o.catalog() );
	Image = o.image();
}

DeepSkyObject::DeepSkyObject( int t, dms r, dms d, float m,
			QString n, QString n2, QString lname, QString cat,
			float a, float b, short int pa, int pgc, int ugc )
	: SkyObject( t, r, d, m, n, n2, lname ) {
	MajorAxis = a;
	MinorAxis = b;
	PositionAngle = pa;
	PGC = pgc;
	UGC = ugc;
	setCatalog( cat );
	Image = 0;
}

DeepSkyObject::DeepSkyObject( int t, double r, double d, float m,
			QString n, QString n2, QString lname, QString cat,
			float a, float b, short int pa, int pgc, int ugc )
	: SkyObject( t, r, d, m, n, n2, lname ) {
	MajorAxis = a;
	MinorAxis = b;
	PositionAngle = pa;
	PGC = pgc;
	UGC = ugc;
	setCatalog( cat );
	Image = 0;
}

float DeepSkyObject::e( void ) const {
	if ( MajorAxis==0.0 || MinorAxis==0.0 ) return 1.0; //assume circular
	return MinorAxis / MajorAxis;
}

QString DeepSkyObject::catalog() const {
	if ( isCatalogM() ) return QString("M");
	if ( isCatalogNGC() ) return QString("NGC");
	if ( isCatalogIC() ) return QString("IC");
	return QString("");
}

void DeepSkyObject::setCatalog( const QString &cat ) {
	if ( cat.upper() == "M" ) Catalog = (unsigned char)CAT_MESSIER;
	else if ( cat.upper() == "NGC" ) Catalog = (unsigned char)CAT_NGC;
	else if ( cat.upper() == "IC"  ) Catalog = (unsigned char)CAT_NGC;
	else Catalog = (unsigned char)CAT_UNKNOWN;
}

QImage* DeepSkyObject::readImage( void ) {
	QFile file;
	if ( Image==0 ) { //Image not currently set; try to load it from disk.
		QString fname = name().lower().replace( QRegExp(" "), "" ) + ".png";

		if ( KSUtils::openDataFile( file, fname ) ) {
			file.close();
			Image = new QImage( file.name(), "PNG" );
		}
	}

	return Image;
}

