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


#include <stdlib.h>
#include <qstring.h>
#include "skypoint.h"
#include "dms.h"

void dms::setD( double x ) {
  D = x;
}

void dms::setD(int d, int m, int s) {
  D = (double)abs(d) + ((double)m + (double)s/60.)/60.;
  if (d<0) {D = -1.0*D;}
}

void dms::setH( double x ) {
  D = x*15.0;
}

void dms::setH(int h, int m, int s) {
  D = 15.0*((double)abs(h) + ((double)m + (double)s/60.)/60.);
  if (h<0) {D = -1.0*D;}
}

int dms::getArcMin( void ) const {
	int am = int( 60.0*( fabs(D) - abs( getDeg() ) ) );
	if ( D<0.0 && D>-1.0 ) { //angle less than zero, but greater than -1.0
		am = -1*am; //make minute negative
	}
	return am;
}

int dms::getArcSec( void ) const {
	int as = int( 60.0*( 60.0*( fabs(D) - abs( getDeg() ) ) - abs( getArcMin() ) ) );
	//If the angle is slightly less than 0.0, give ArcSec a neg. sgn.
	if ( getDeg()==0 && getArcMin()==0 && D<0.0 ) {
		as = -1*as;
	}
	return as;
}

int dms::getHMin( void ) const {
	int hm = int( 60.0*( fabs( getH() ) - abs( getHour() ) ) );
	if ( getH()<0.0 && getH()>-1.0 ) { //angle less than zero, but greater than -1.0
		hm = -1*hm; //make minute negative
	}
	return hm;
}

int dms::getHSec( void ) const {
	int hs = int( 60.0*( 60.0*( fabs( getH() ) - abs( getHour() ) ) - abs( getHMin() ) ) );
	if ( getHour()==0 && getHMin()==0 && getH()<0.0 ) {
		hs = -1*hs;
	}
	return hs;
}

/*
dms dms::operator+ (dms angle) {
	return getD() + angle.getD();
}

dms dms::operator- (dms angle)
{
	return getD() - angle.getD();
}
*/
//---------------------------------------------------------------------------

void dms::SinCos(double &sina, double &cosa) {
  sina = sin( radians() );
	cosa = cos( radians() );
}
//---------------------------------------------------------------------------

double dms::radians( void ) {
  double Rad = getD()*PI()/180.0;
  return Rad;
}
//---------------------------------------------------------------------------

void dms::setRadians( double Rad ) {
  setD( Rad*180.0/PI() );
}
//---------------------------------------------------------------------------

dms dms::reduce( void ) const {
	double a = D;
  while (a<0.0) {a += 360.0;}
  while (a>=360.0) {a -= 360.0;}
	return dms( a );
}
//---------------------------------------------------------------------------

double PI() {
  double PI = acos(-1.0);
  return PI;
}
//---------------------------------------------------------------------------

