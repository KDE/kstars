/*
    INDI LIB
    Common routines used by all drivers
    Copyright (C) 2003 by Jason Harris (jharris@30doradus.org)

    This is the C version of the astronomical library in KStars
    modified by Jasem Mutlaq (mutlaqja@ikarustech.com)

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

*/

/* needed for sincos() in math.h */
#define _GNU_SOURCE

#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "indicom.h"

const char * Direction[] = { "North", "West", "East", "South", "All"};
const char * SolarSystem[] = { "Mercury", "Venus", "Moon", "Mars", "Jupiter", "Saturn", "Uranus", "Neptune", "Pluto"};

/******** Prototypes ***********/
double DegToRad( double num );
double RadToDeg( double num );
void SinCos( double Degrees, double *sina, double *cosa );

double obliquity(void);
/**@returns the constant of aberration (20.49 arcsec). */
double constAberr(void);
/**@returns the mean solar anomaly. */
double sunMeanAnomaly(void);
/**@returns the mean solar longitude. */
double sunMeanLongitude(void);
/**@returns the true solar anomaly. */
double sunTrueAnomaly(void);
/**@returns the true solar longitude. */
double sunTrueLongitude(void);
/**@returns the longitude of the Earth's perihelion point. */
double earthPerihelionLongitude(void);
/**@returns eccentricity of Earth's orbit.*/
double earthEccentricity(void);
/**@returns the change in obliquity due to the nutation of Earth's orbit. */
double dObliq(void);
/**@returns the change in Ecliptic Longitude due to nutation. */
double dEcLong(void);
/**@returns Julian centuries since J2000*/
double julianCenturies(void);
/**@returns Julian Millenia since J2000*/
/* double julianMillenia(void);*/
/**@returns element of P1 precession array at position [i1][i2] */
double p1( int i1, int i2 );
/**@returns element of P2 precession array at position [i1][i2] */
double p2( int i1, int i2 );
/**@short update all values for the date given as an argument. */
void updateAstroValues( double jd );
void nutate(double *RA, double *Dec);
void aberrate(double *RA, double *Dec);
void precessFromAnyEpoch(double jd0, double jdf, double *RA, double *Dec);
void apparentCoord(double jd0, double jdf, double *RA, double *Dec);

double K = ( 20.49552 / 3600. );
double Obliquity, K, L, L0, LM, M, M0, O, P;
double XP, YP, ZP;
double CX, SX, CY, SY, CZ, SZ;
double P1[3][3], P2[3][3];
double deltaObliquity, deltaEcLong;
double e, T;

double obliquity() { return Obliquity; }
/**@returns the constant of aberration (20.49 arcsec). */
double constAberr() { return K; }
/**@returns the mean solar anomaly. */
double sunMeanAnomaly() { return M; }
/**@returns the mean solar longitude. */
double sunMeanLongitude() { return L; }
/**@returns the true solar anomaly. */
double sunTrueAnomaly() { return M0; }
/**@returns the true solar longitude. */
double sunTrueLongitude() { return L0; }
/**@returns the longitude of the Earth's perihelion point. */
double earthPerihelionLongitude() { return P; }
/**@returns eccentricity of Earth's orbit.*/
double earthEccentricity() { return e; }
/**@returns the change in obliquity due to the nutation of Earth's orbit. */
double dObliq() { return deltaObliquity; }
/**@returns the change in Ecliptic Longitude due to nutation. */
double dEcLong() { return deltaEcLong; }
/**@returns Julian centuries since J2000*/
double julianCenturies() { return T; }
/**@returns Julian Millenia since J2000*/
/* double julianMillenia() { return jm; }*/
/**@returns element of P1 precession array at position [i1][i2] */
double p1( int i1, int i2 ) { return P1[i1][i2]; }
/**@returns element of P2 precession array at position [i1][i2] */
double p2( int i1, int i2 ) { return P2[i1][i2]; }

void updateAstroValues( double jd )
{
	static double days = 0;
	double jm;
	double C, U, dObliq2;
	/* Nutation parameters */
	double  L2, M2, O2;
	double sin2L, cos2L, sin2M, cos2M;
	double sinO, cosO, sin2O, cos2O;

	/* no need to recalculate if same as previous JD */
	if (days == jd)
	 return;

	days = jd;

	/* Julian Centuries since J2000.0 */
	T = ( jd - J2000 ) / 36525.;

	/* Julian Millenia since J2000.0 */
	jm = T / 10.0;

	/* Sun's Mean Longitude */
	L = ( 280.46645 + 36000.76983*T + 0.0003032*T*T );

	/* Sun's Mean Anomaly */
	M = ( 357.52910 + 35999.05030*T - 0.0001559*T*T - 0.00000048*T*T*T );

	/* Moon's Mean Longitude */
	LM = ( 218.3164591 + 481267.88134236*T - 0.0013268*T*T + T*T*T/538841. - T*T*T*T/6519400.);

	/* Longitude of Moon's Ascending Node */
	O = ( 125.04452 - 1934.136261*T + 0.0020708*T*T + T*T*T/450000.0 );

	/* Earth's orbital eccentricity */
	e = 0.016708617 - 0.000042037*T - 0.0000001236*T*T;

	C = ( 1.914600 - 0.004817*T - 0.000014*T*T ) * sin( DegToRad(M) )
						+ ( 0.019993 - 0.000101*T ) * sin( 2.0* DegToRad(M) )
						+ 0.000290 * sin( 3.0* DegToRad(M));

	/* Sun's True Longitude */
	L0 = ( L + C );

	/* Sun's True Anomaly */
	M0 = ( M + C );

	/* Obliquity of the Ecliptic */
	U = T/100.0;
	dObliq2 = -4680.93*U - 1.55*U*U + 1999.25*U*U*U
									- 51.38*U*U*U*U - 249.67*U*U*U*U*U
									- 39.05*U*U*U*U*U*U + 7.12*U*U*U*U*U*U*U
									+ 27.87*U*U*U*U*U*U*U*U + 5.79*U*U*U*U*U*U*U*U*U
									+ 2.45*U*U*U*U*U*U*U*U*U*U;
	Obliquity = ( 23.43929111 + dObliq2/3600.0);

	O2 = ( 2.0 * O );
	L2 = ( 2.0 * L );  /* twice mean ecl. long. of Sun */
	M2 = ( 2.0 * LM);  /* twice mean ecl. long. of Moon */

	SinCos( O, &sinO, &cosO );
	SinCos( O2, &sin2O, &cos2O );
	SinCos( L2, &sin2L, &cos2L );
	SinCos( M2, &sin2M, &cos2M );

	deltaEcLong = ( -17.2*sinO - 1.32*sin2L - 0.23*sin2M + 0.21*sin2O)/3600.0;  /* Ecl. long. correction */
	deltaObliquity = ( 9.2*cosO + 0.57*cos2L + 0.10*cos2M - 0.09*cos2O)/3600.0; /* Obliq. correction */

	/* Compute Precession Matrices: */
	XP = ( 0.6406161*T + 0.0000839*T*T + 0.0000050*T*T*T );
	YP = ( 0.5567530*T - 0.0001185*T*T - 0.0000116*T*T*T );
	ZP = ( 0.6406161*T + 0.0003041*T*T + 0.0000051*T*T*T );

	SinCos(XP, &SX, &CX );
	SinCos(YP, &SY, &CY );
	SinCos(ZP, &SZ, &CZ );

        /* P1 is used to precess from any epoch to J2000 */
	P1[0][0] = CX*CY*CZ - SX*SZ;
	P1[1][0] = CX*CY*SZ + SX*CZ;
	P1[2][0] = CX*SY;
	P1[0][1] = -1.0*SX*CY*CZ - CX*SZ;
	P1[1][1] = -1.0*SX*CY*SZ + CX*CZ;
	P1[2][1] = -1.0*SX*SY;
	P1[0][2] = -1.0*SY*CZ;
	P1[1][2] = -1.0*SY*SZ;
	P1[2][2] = CY;

        /* P2 is used to precess from J2000 to any other epoch (it is the transpose of P1) */
	P2[0][0] = CX*CY*CZ - SX*SZ;
	P2[1][0] = -1.0*SX*CY*CZ - CX*SZ;
	P2[2][0] = -1.0*SY*CZ;
	P2[0][1] = CX*CY*SZ + SX*CZ;
	P2[1][1] = -1.0*SX*CY*SZ + CX*CZ;
	P2[2][1] = -1.0*SY*SZ;
	P2[0][2] = CX*SY;
	P2[1][2] = -1.0*SX*SY;
	P2[2][2] = CY;

}

double DegToRad( double num )
{
  /*return (num * deg_rad);*/
  return (num * (M_PI / 180.0));
}

double RadToDeg( double num )
{
  return (num / (M_PI / 180.0));
}

void SinCos( double Degrees, double *sina, double *cosa )
{
	/**We have two versions of this function.  One is ANSI standard, but
		*slower.  The other is faster, but is GNU only.
		*Using the __GLIBC_ and __GLIBC_MINOR_ constants to determine if the
		* GNU extension sincos() is defined.
		*/
		static int rDirty = 1;
		double Sin, Cos;


		if (rDirty)
		{
                #ifdef __GLIBC__
                #if ( __GLIBC__ >= 2 && __GLIBC_MINOR__ >=1 )
		/* GNU version */
		sincos( DegToRad(Degrees), &Sin, &Cos );
                #else
		/* For older GLIBC versions */
	        Sin = ::sin( DegToRad(Degrees) );
		Cos = ::cos( DegToRad(Degrees) );
		#endif
		#else
		/* ANSI-compliant version */
		Sin = sin( DegToRad(Degrees) );
		Cos = cos( DegToRad(Degrees) );
		rDirty = 0;
		#endif
	        }
		else
		{
		 Sin = sin( DegToRad(Degrees) );
		 Cos = cos( DegToRad(Degrees) );
		}

  		*sina = Sin;
		*cosa = Cos;
}

void nutate(double *RA, double *Dec)
{
	double cosRA, sinRA, cosDec, sinDec, tanDec;
	double dRA1, dDec1;
	double cosOb, sinOb;

	SinCos( *RA, &sinRA, &cosRA );
	SinCos( *Dec, &sinDec, &cosDec );

	SinCos( obliquity(), &sinOb, &cosOb );

	/* Step 2: Nutation */
	/* approximate method */
	tanDec = sinDec/cosDec;

	dRA1  = ( dEcLong()*( cosOb + sinOb*sinRA*tanDec ) - dObliq()*cosRA*tanDec );
	dDec1 = ( dEcLong()*( sinOb*cosRA ) + dObliq()*sinRA );

	*RA  = ( *RA + dRA1 );
	*Dec = ( *Dec+ dDec1);
}

void aberrate(double *RA, double *Dec)
{
	double cosRA, sinRA, cosDec, sinDec;
	double dRA2, dDec2;
	double cosOb, sinOb, cosL, sinL, cosP, sinP;
	double K2, e2, tanOb;

	K2 = constAberr();  /* constant of aberration */
	e2 = earthEccentricity();

	SinCos( *RA, &sinRA, &cosRA );
	SinCos( *Dec, &sinDec, &cosDec );

	SinCos( obliquity(), &sinOb, &cosOb );
	tanOb = sinOb/cosOb;

	SinCos( sunTrueLongitude(), &sinL, &cosL );
	SinCos( earthPerihelionLongitude(), &sinP, &cosP );

	/* Step 3: Aberration */
	dRA2 = ( -1.0 * K2 * ( cosRA * cosL * cosOb + sinRA * sinL )/cosDec
		+ e2 * K2 * ( cosRA * cosP * cosOb + sinRA * sinP )/cosDec );

	dDec2 = ( -1.0 * K2 * ( cosL * cosOb * ( tanOb * cosDec - sinRA * sinDec ) + cosRA * sinDec * sinL )
		+ e2 * K2 * ( cosP * cosOb * ( tanOb * cosDec - sinRA * sinDec ) + cosRA * sinDec * sinP ) );

	*RA  = ( *RA  + dRA2 );
	*Dec = ( *Dec + dDec2);
}

void precessFromAnyEpoch(double jd0, double jdf, double *RA, double *Dec)
{
 	double cosRA0, sinRA0, cosDec0, sinDec0;
	double v[3], s[3];
	unsigned int i=0;

	SinCos( *RA, &sinRA0, &cosRA0 );
	SinCos( *Dec, &sinDec0, &cosDec0 );

	/* Need first to precess to J2000.0 coords */

	if ( jd0 != J2000 )
	{

	/* v is a column vector representing input coordinates. */

		updateAstroValues(jd0);

		v[0] = cosRA0*cosDec0;
		v[1] = sinRA0*cosDec0;
		v[2] = sinDec0;


	/*s is the product of P1 and v; s represents the
	coordinates precessed to J2000 */
		for ( i=0; i<3; ++i ) {
			s[i] = p1( 0, i )*v[0] + p1( 1, i )*v[1] +
				p1( 2, i )*v[2];
		}

	} else
	{

	/* Input coords already in J2000, set s accordingly. */
		s[0] = cosRA0*cosDec0;
		s[1] = sinRA0*cosDec0;
		s[2] = sinDec0;
       }

       updateAstroValues(jdf);

	/* Multiply P2 and s to get v, the vector representing the new coords. */

	for ( i=0; i<3; ++i ) {
		v[i] = p2( 0, i )*s[0] + p2( 1, i )*s[1] +
		p2( 2, i )*s[2];
	}

	/*Extract RA, Dec from the vector:
	RA.setRadians( atan( v[1]/v[0] ) ); */

	*RA  = RadToDeg( atan2( v[1],v[0] ) );
	*Dec = RadToDeg( asin( v[2] ) );

	if (*RA < 0.0 )
	    *RA = ( *RA + 360.0 );
}

void apparentCoord(double jd0, double jdf, double *RA, double *Dec)
{
        *RA = *RA * 15.0;

	precessFromAnyEpoch(jd0,jdf, RA, Dec);

	updateAstroValues(jdf);

	nutate(RA, Dec);
	aberrate(RA, Dec);

	*RA = *RA / 15.0;
}

int validateSex(const char * str, float *x, float *y, float *z)
{
  int ret;

  ret = sscanf(str, "%f%*c%f%*c%f", x, y, z);
  if (ret < 1)
   return -1;

  if (ret == 1)
  {
   *y = 0;
   *z = 0;
  }
  else if (ret == 2)
   *z = 0;


  return (0);

}

int getSex(const char *str, double * value)
{
  float x,y,z;

  if (validateSex(str, &x, &y, &z))
   return (-1);

  if (x > 0)
   *value = x + (y / 60.0) + (z / (3600.00));
  else
   *value = x - (y / 60.0) - (z / (3600.00));

   return (0);
}

/* Mode 0   XX:YY:ZZ
   Mode 1 +-XX:YY:ZZ
   Mode 2   XX:YY
   Mode 3 +-XX:YY
   Mode 3   XX:YY.Z
   Mode 4   XXX:YY */
void formatSex(double number, char * str, int mode)
{
  int x;
  double y, z;

  x = (int) number;

  y = ((number - x) * 60.00);

  if (y < 0) y *= -1;

  z = (y - (int) y) * 60.00;

  if (z < 0) z *= -1;

  switch (mode)
  {
    case XXYYZZ:
      	sprintf(str, "%02d:%02d:%02.0f", x, (int) y, z);
	break;
    case SXXYYZZ:
        sprintf(str, "%+03d:%02d:%02.0f", x, (int) y, z);
	break;
    case XXYY:
        sprintf(str, "%02d:%02d", x, (int) y);
	break;
    case SXXYY:
    sprintf(str, "%+03d:%02d", x, (int) y);
	break;
    case XXYYZ:
        sprintf(str, "%02d:%02.1f", x, (float) y);
	break;
    case XXXYY:
    	sprintf(str, "%03d:%02d", x, (int) y);
	break;
  }

}

double UTtoJD(struct tm *utm)
{
  int year, month, day, hour, minute, second;
  int m, y, A, B, C, D;
  double d, jd;

  /* Note: The tm_year was modified by adding +1900 to it since tm_year refers
  to the number of years after 1900. The month field was also modified by adding 1 to it
  since the tm_mon range is from 0 to 11 */
  year   = utm->tm_year;
  month  = utm->tm_mon;
  day    = utm->tm_mday;
  hour   = utm->tm_hour;
  minute = utm->tm_min;
  second = utm->tm_sec;


  if (month > 2)
  {
	  m = month;
	  y = year;
  } else
  {
	  y = year - 1;
	  m = month + 12;
  }

 /*  If the date is after 10/15/1582, then take Pope Gregory's modification
	 to the Julian calendar into account */

	 if (( year  >1582 ) ||
		 ( year ==1582 && month  >9 ) ||
		 ( year ==1582 && month ==9 && day >15 ))
	 {
		 A = (int) y/100;
		 B = 2 - A + (int) A/4;
	 } else {
		 B = 0;
	 }

  if (y < 0) {
	  C = (int) ((365.25*y) - 0.75);
  } else {
	  C = (int) (365.25*y);
  }

  D = (int) (30.6001*(m+1));

  d = (double) day + ( (double) hour + ( (double) minute + (double) second/60.0)/60.0)/24.0;
  jd = B + C + D + d + 1720994.5;

  return jd;
}

int extractISOTime(char *timestr, struct tm *utm)
{

  if (strptime(timestr, "%Y-%m-%dT%H:%M:%S", utm))
   return (0);
  if (strptime(timestr, "%Y/%m/%dT%H:%M:%S", utm))
   return (0);
  if (strptime(timestr, "%Y:%m:%dT%H:%M:%S", utm))
   return (0);
  if (strptime(timestr, "%Y-%m-%dT%H-%M-%S", utm))
   return (0);

   return (-1);

}

