/***************************************************************************
                          ksnumbers.h  -  description
                             -------------------
    begin                : Sun Jan 13 2002
    copyright            : (C) 2002 by Jason Harris
    email                : kstars@30doradus.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef KSNUMBERS_H
#define KSNUMBERS_H

#include "dms.h"

/**
  *@author Jason Harris
  */

class KSNumbers {
public: 
	KSNumbers( long double jd );
	~KSNumbers();

	dms obliquity() const { return Obliquity; }
	dms constAberr() const { return K; }
	dms sunMeanAnomaly() const { return M; }
	dms sunMeanLongitude() const { return L; }
	dms sunTrueAnomaly() const { return M0; }
	dms sunTrueLongitude() const { return L0; }
	dms earthPerihelionLongitude() const { return P; }
	double dObliq() const { return deltaObliquity; }
	double dEcLong() const { return deltaEcLong; }
	double julianCenturies() const { return T; }
	double earthEccentricity() const { return e; }
	long double julianDate() const { return days; }
	double julianMillenia() const { return jm; }

	double p1( int i1, int i2 ) const { return P1[i1][i2]; }
	double p2( int i1, int i2 ) const { return P2[i1][i2]; }

	void updateValues( long double jd );

private:
	dms Obliquity, K, L, L0, LM, M, M0, O, P;
	dms XP, YP, ZP;
	double CX, SX, CY, SY, CZ, SZ;
	double P1[3][3], P2[3][3];
	double deltaObliquity, deltaEcLong;
	double e, T;
	long double days;
	double jm;

};

#endif
