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

#include "skyobject.h"
#include "dms.h"

SkyObject::SkyObject(){
	type = 0;
	Position.set( 0.0, 0.0 );
  mag = 0.0;
  name = "unnamed";
  name2 = "";
  longname = "";
}

SkyObject::SkyObject( SkyObject &o ) {
	type = o.type;
	Position = *o.pos();
  mag = o.mag;
  name = o.name;
	name2 = o.name2;
	longname = o.longname;
	ImageList = o.ImageList;
	ImageTitle = o.ImageTitle;
	InfoList = o.InfoList;
	InfoTitle = o.InfoTitle;
}

SkyObject::SkyObject( int t, dms r, dms d, double m, QString n,
                      QString n2, QString lname ) {
  type = t;
	Position.setRA0( r );
	Position.setDec0( d );
	Position.setRA( r );
	Position.setDec( d );
  mag = m;
  name = n;
  name2 = n2;
  longname = lname;
}

SkyObject::SkyObject( int t, double r, double d, double m, QString n,
                      QString n2, QString lname ) {
  type = t;
	Position.setRA0( r );
	Position.setDec0( d );
	Position.setRA( r );
	Position.setDec( d );
  mag = m;
  name = n;
  name2 = n2;
  longname = lname;
}

QTime SkyObject::riseTime( long double jd, GeoLocation *geo ) {
	double r = -1.0 * tan( geo->lat().radians() ) * tan( getDec().radians() );
	if ( r < -1.0 || r > 1.0 )
		return QTime( 25, 0, 0 );  //this object does not rise or set; return an invalid time
		
	double H = acos( r )*180./acos(-1.0); //180/Pi converts radians to degrees
	dms LST;
	LST.setH( 24.0 + getRA().getH() - H/15.0 );
	LST = LST.reduce();

	//convert LST to Greenwich ST
	dms GST = dms( LST.getD() - geo->lng().getD() ).reduce();

	//convert GST to UT
	double T = ( jd - J2000 )/36525.0;
	dms T0, dT, UT;
	T0.setH( 6.697374558 + (2400.051336*T) + (0.000025862*T*T) );
	T0 = T0.reduce();
	dT.setH( GST.getH() - T0.getH() );
	dT = dT.reduce();
	UT.setH( 0.9972695663 * dT.getH() );
	UT = UT.reduce();

	//convert UT to LT;
	dms LT = dms( UT.getD() - 15.0*geo->TZ() ).reduce();

	return QTime( LT.getHour(), LT.getHMin(), LT.getHSec() );
}

QTime SkyObject::setTime( long double jd, GeoLocation *geo ) {
	double r = -1.0 * tan( geo->lat().radians() ) * tan( getDec().radians() );
	if ( r < -1.0 || r > 1.0 )
		return QTime( 25, 0, 0 );  //this object does not rise or set; return an invalid time
		
	double H = acos( r )*180./acos(-1.0);
	dms LST;

	//the following line is the only difference between riseTime() and setTime()
	LST.setH( getRA().getH() + H/15.0 );
	LST = LST.reduce();

	//convert LST to Greenwich ST
	dms GST = dms( LST.getD() - geo->lng().getD() ).reduce();

	//convert GST to UT
	double T = ( jd - J2000 )/36525.0;
	dms T0, dT, UT;
	T0.setH( 6.697374558 + (2400.051336*T) + (0.000025862*T*T) );
	T0 = T0.reduce();
	dT.setH( GST.getH() - T0.getH() );
	dT = dT.reduce();
	UT.setH( 0.9972695663 * dT.getH() );
  UT = UT.reduce();

	//convert UT to LT;
	dms LT = dms( UT.getD() - 15.0*geo->TZ() ).reduce();
	return QTime( LT.getHour(), LT.getHMin(), LT.getHSec() );
}
