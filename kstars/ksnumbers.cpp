/***************************************************************************
                          ksnumbers.cpp  -  description
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

#include "ksnumbers.h"

KSNumbers::KSNumbers( long double jd ){
	K.setD( 20.49552 / 3600. );  //set the constant of aberration
	updateValues( jd );
}

KSNumbers::~KSNumbers(){
}

void KSNumbers::updateValues( long double jd ) {
	days = jd;

	//Julian Centuries since J2000.0
	T = ( jd - J2000 ) / 36525.;
	
	//Julian Centuries since B1950.0
	TB = ( jd - 2433282.4235 ) / 36524.2199;

	// Julian Millenia since J2000.0
	jm = T / 10.0;

	//Sun's Mean Longitude
	L.setD( 280.46645 + 36000.76983*T + 0.0003032*T*T );

	//Sun's Mean Anomaly
	M.setD( 357.52910 + 35999.05030*T - 0.0001559*T*T - 0.00000048*T*T*T );

	//Moon's Mean Longitude
	LM.setD( 218.3164591 + 481267.88134236*T - 0.0013268*T*T + T*T*T/538841. - T*T*T*T/6519400.);

	//Longitude of Moon's Ascending Node
	O.setD( 125.04452 - 1934.136261*T + 0.0020708*T*T + T*T*T/450000.0 );

	//Earth's orbital eccentricity
	e = 0.016708617 - 0.000042037*T - 0.0000001236*T*T;
	
	double C = ( 1.914600 - 0.004817*T - 0.000014*T*T ) * sin( M.radians() )
						+ ( 0.019993 - 0.000101*T ) * sin( 2.0* M.radians() )
						+ 0.000290 * sin( 3.0* M.radians() );

	//Sun's True Longitude
	L0.setD( L.Degrees() + C );

	//Sun's True Anomaly
	M0.setD( M.Degrees() + C );

	//Obliquity of the Ecliptic
	double U = T/100.0;
	double dObliq = -4680.93*U - 1.55*U*U + 1999.25*U*U*U
									- 51.38*U*U*U*U - 249.67*U*U*U*U*U
									- 39.05*U*U*U*U*U*U + 7.12*U*U*U*U*U*U*U
									+ 27.87*U*U*U*U*U*U*U*U + 5.79*U*U*U*U*U*U*U*U*U
									+ 2.45*U*U*U*U*U*U*U*U*U*U;
	Obliquity.setD( 23.43929111 + dObliq/3600.0);

	//Nutation parameters
	dms  L2, M2, O2;
	double sin2L, cos2L, sin2M, cos2M;
	double sinO, cosO, sin2O, cos2O;
	
	O2.setD( 2.0*O.Degrees() );
	L2.setD( 2.0*L.Degrees() ); //twice mean ecl. long. of Sun
	M2.setD( 2.0*LM.Degrees() );  //twice mean ecl. long. of Moon
		
	O.SinCos( sinO, cosO );
	O2.SinCos( sin2O, cos2O );
	L2.SinCos( sin2L, cos2L );
	M2.SinCos( sin2M, cos2M );

	deltaEcLong = ( -17.2*sinO - 1.32*sin2L - 0.23*sin2M + 0.21*sin2O)/3600.0; //Ecl. long. correction
	deltaObliquity = ( 9.2*cosO + 0.57*cos2L + 0.10*cos2M - 0.09*cos2O)/3600.0; //Obliq. correction

	//Compute Precession Matrices:
	XP.setD( 0.6406161*T + 0.0000839*T*T + 0.0000050*T*T*T );
	YP.setD( 0.5567530*T - 0.0001185*T*T - 0.0000116*T*T*T );
	ZP.setD( 0.6406161*T + 0.0003041*T*T + 0.0000051*T*T*T );

	XP.SinCos( SX, CX );
	YP.SinCos( SY, CY );
	ZP.SinCos( SZ, CZ );

//P1 is used to precess from any epoch to J2000
	P1[0][0] = CX*CY*CZ - SX*SZ;
	P1[1][0] = CX*CY*SZ + SX*CZ;
	P1[2][0] = CX*SY;
	P1[0][1] = -1.0*SX*CY*CZ - CX*SZ;
	P1[1][1] = -1.0*SX*CY*SZ + CX*CZ;
	P1[2][1] = -1.0*SX*SY;
	P1[0][2] = -1.0*SY*CZ;
	P1[1][2] = -1.0*SY*SZ;
	P1[2][2] = CY;

//P2 is used to precess from J2000 to any other epoch (it is the transpose of P1)
	P2[0][0] = CX*CY*CZ - SX*SZ;
	P2[1][0] = -1.0*SX*CY*CZ - CX*SZ;
	P2[2][0] = -1.0*SY*CZ;
	P2[0][1] = CX*CY*SZ + SX*CZ;
	P2[1][1] = -1.0*SX*CY*SZ + CX*CZ;
	P2[2][1] = -1.0*SY*SZ;
	P2[0][2] = CX*SY;
	P2[1][2] = -1.0*SX*SY;
	P2[2][2] = CY;

	// eqnCorr.setD ( (0.0775 + 0.0850 * TB)/3600. );

	//Compute Precession Matrices from B1950 to 1984 using Newcomb formulae

	XB.setD( 0.217697 );
	YB.setD( 0.189274 );
	ZB.setD( 0.217722 );

	XB.SinCos( SXB, CXB );
	YB.SinCos( SYB, CYB );
	ZB.SinCos( SZB, CZB );

//P1B is used to precess from 1984 to B1950:

	P1B[0][0] = CXB*CYB*CZB - SXB*SZB;
	P1B[1][0] = CXB*CYB*SZB + SXB*CZB;
	P1B[2][0] = CXB*SYB;
	P1B[0][1] = -1.0*SXB*CYB*CZB - CXB*SZB;
	P1B[1][1] = -1.0*SXB*CYB*SZB + CXB*CZB;
	P1B[2][1] = -1.0*SXB*SYB;
	P1B[0][2] = -1.0*SYB*CZB;
	P1B[1][2] = -1.0*SYB*SZB;
	P1B[2][2] = CYB;

//P2 is used to precess from B1950 to 1984 (it is the transpose of P1)
	P2B[0][0] = CXB*CYB*CZB - SXB*SZB;
	P2B[1][0] = -1.0*SXB*CYB*CZB - CXB*SZB;
	P2B[2][0] = -1.0*SYB*CZB;
	P2B[0][1] = CXB*CYB*SZB + SXB*CZB;
	P2B[1][1] = -1.0*SXB*CYB*SZB + CXB*CZB;
	P2B[2][1] = -1.0*SYB*SZB;
	P2B[0][2] = CXB*SYB;
	P2B[1][2] = -1.0*SXB*SYB;
	P2B[2][2] = CYB;
}
