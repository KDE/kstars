/***************************************************************************
                          dms.cpp  -  K Desktop Planetarium
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

// needed for sincos() in math.h
//#define _GNU_SOURCE

#include <stdlib.h>
#include <qstring.h>

#include "skypoint.h"
#include "dms.h"

void dms::setD( double x ) {
  D = x;
}

void dms::setD(int d, int m, int s, int ms) {
  D = (double)abs(d) + ((double)m + ((double)s + (double)ms/1000.)/60.)/60.;
  if (d<0) {D = -1.0*D;}
}

void dms::setH( double x ) {
  D = x*15.0;
}

void dms::setH(int h, int m, int s, int ms) {
  D = 15.0*((double)abs(h) + ((double)m + ((double)s + (double)ms/1000.)/60.)/60.);
  if (h<0) {D = -1.0*D;}
}

int dms::getArcMin( void ) const {
	int am = int( 60.0*( fabs(D) - abs( degree() ) ) );
	if ( D<0.0 && D>-1.0 ) { //angle less than zero, but greater than -1.0
		am = -1*am; //make minute negative
	}
	return am;
}

int dms::getArcSec( void ) const {
	int as = int( 60.0*( 60.0*( fabs(D) - abs( degree() ) ) - abs( getArcMin() ) ) );
	//If the angle is slightly less than 0.0, give ArcSec a neg. sgn.
	if ( degree()==0 && getArcMin()==0 && D<0.0 ) {
		as = -1*as;
	}
	return as;
}

int dms::getmArcSec( void ) const {
	int as = int( 1000.0*(60.0*( 60.0*( fabs(D) - abs( degree() ) ) - abs( getArcMin() ) ) - abs( getArcSec() ) ) );
	//If the angle is slightly less than 0.0, give ArcSec a neg. sgn.
	if ( degree()==0 && getArcMin()==0 && getArcSec()== 0 && D<0.0 ) {
		as = -1*as;
	}
	return as;
}

int dms::minute( void ) const {
	int hm = int( 60.0*( fabs( Hours() ) - abs( hour() ) ) );
	if ( Hours()<0.0 && Hours()>-1.0 ) { //angle less than zero, but greater than -1.0
		hm = -1*hm; //make minute negative
	}
	return hm;
}

int dms::second( void ) const {
	int hs = int( 60.0*( 60.0*( fabs( Hours() ) - abs( hour() ) ) - abs( minute() ) ) );
	if ( hour()==0 && minute()==0 && Hours()<0.0 ) {
		hs = -1*hs;
	}
	return hs;
}

int dms::msecond( void ) const {
	int hs = int( 1000.0*(60.0*( 60.0*( fabs( Hours() ) - abs( hour() ) ) - abs( minute() ) ) - abs( second() ) ) );
	if ( hour()==0 && minute()==0 && second()==0 && Hours()<0.0 ) {
		hs = -1*hs;
	}
	return hs;
}

/*
dms dms::operator+ (dms angle) {
	return Degrees() + angle.Degrees();
}

dms dms::operator- (dms angle)
{
	return Degrees() - angle.Degrees();
}
*/
//---------------------------------------------------------------------------

void dms::SinCos( double &sina, double &cosa ) {
  /**We have two versions of this function.  One is ANSI standard, but 
   *slower.  The other is faster, but is GNU only.
   *For now, leaving the GNU-only solution commented out, until
   *we figure out a way to tell if the user has the GNU math library.
   *(perhaps with a "#if defined ( SOMETHING )" directive. )
   */

  // This is the old implementation of sincos which is standard C compliant
  //#if defined(__FreeBSD__) 
  // almost certainly this needs more checks for other non-glibc platforms
  // but I am currently unable to check those
  register double rad = radians();
  sina = sin( rad );
  cosa = cos( rad );
  
  //#else
  //This is the faster GNU version.  Commented out for now.
  //sincos(radians(), &sina, &cosa);
  //#endif
}
//---------------------------------------------------------------------------

double dms::radians( void ) {
	return (D*PI/180.0);
}
//---------------------------------------------------------------------------

void dms::setRadians( double Rad ) {
  setD( Rad*180.0/PI );
}
//---------------------------------------------------------------------------

dms dms::reduce( void ) const {
	double a = D;
	while (a<0.0) {a += 360.0;}
	while (a>=360.0) {a -= 360.0;}
	return dms( a );
}

QString dms::toDMSString(bool forceSign) const {
	QString dummy;
	char pm(' ');
	int dd = abs(degree());
	int dm = abs(getArcMin());
	int ds = abs(getArcSec());

	if ( Degrees() < 0.0 ) pm = '-';
	else if (forceSign && Degrees() > 0.0 ) pm = '+';

	QString format( "%c%3d%c %02d\' %02d\"" );
	if ( dd < 100 ) format = "%c%2d%c %02d\' %02d\"";
	if ( dd < 10  ) format = "%c%1d%c %02d\' %02d\"";

//	return dummy.sprintf("%c%3d%c %02d\' %02d\"", pm, dd, 176, dm, ds);
	return dummy.sprintf(format.local8Bit(), pm, dd, 176, dm, ds);
}

QString dms::toHMSString() const {
	QString dummy;

	return dummy.sprintf("%02dh %02dm %02ds", hour(), minute(), second());
}

	

//---------------------------------------------------------------------------

// const double dms::PI = acos(-1.0); 
// M_PI is defined in math.h
const double dms::PI = M_PI; 
