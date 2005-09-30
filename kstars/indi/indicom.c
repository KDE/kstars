/*
    INDI LIB
    Common routines used by all drivers
    Copyright (C) 2003 by Jason Harris (jharris@30doradus.org)
                          Elwood C. Downey

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
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

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

/* make it compile on solaris */
#ifndef M_PI
#define M_PI 3.14159265358979323846264338327950288419716939937510582097494459
#endif

/******** Prototypes ***********/
double DegToRad( double num );
double RadToDeg( double num );
void SinCos( double Degrees, double *sina, double *cosa );

double obliquity(void);
double constAberr(void);
double sunMeanAnomaly(void);
double sunMeanLongitude(void);
double sunTrueAnomaly(void);
double sunTrueLongitude(void);
double earthPerihelionLongitude(void);
double earthEccentricity(void);
double dObliq(void);
double dEcLong(void);
double julianCenturies(void);
double p1( int i1, int i2 );
double p2( int i1, int i2 );
void updateAstroValues( double jd );
void nutate(double *RA, double *Dec);
void aberrate(double *RA, double *Dec);
void precessFromAnyEpoch(double jd0, double jdf, double *RA, double *Dec);
void apparentCoord(double jd0, double jdf, double *RA, double *Dec);
void getSexComponents(double value, int *d, int *m, int *s);

double K = ( 20.49552 / 3600. );
double Obliquity, K, L, L0, LM, M, M0, O, P;
double XP, YP, ZP;
double CX, SX, CY, SY, CZ, SZ;
double P1[3][3], P2[3][3];
double deltaObliquity, deltaEcLong;
double e, T;

double obliquity() { return Obliquity; }
double constAberr() { return K; }
double sunMeanAnomaly() { return M; }
double sunMeanLongitude() { return L; }
double sunTrueAnomaly() { return M0; }
double sunTrueLongitude() { return L0; }
double earthPerihelionLongitude() { return P; }
double earthEccentricity() { return e; }
double dObliq() { return deltaObliquity; }
double dEcLong() { return deltaEcLong; }
double julianCenturies() { return T; }
double p1( int i1, int i2 ) { return P1[i1][i2]; }
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
                #if ( __GLIBC__ >= 2 && __GLIBC_MINOR__ >=1  && !defined(__UCLIBC__))
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

double calculateDec(double latitude, double SDTime)
{
  if (SDTime > 12) SDTime -= 12;
  
  return RadToDeg ( atan ( (cos (DegToRad (latitude)) / sin (DegToRad (latitude))) * cos (DegToRad (SDTime))) );

}

double calculateRA(double SDTime)
{
  double ra;
  
  ra = SDTime + 12;
  
  if (ra < 24)
    return ra;
  else
    return (ra - 24);
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

double UTtoJD(struct tm *utm)
{
  int year, month, day, hour, minute, second;
  int m, y, A, B, C, D;
  double d, jd;

  /* Note: The tm_year was modified by adding +1900 to it since tm_year refers
  to the number of years after 1900. The month field was also modified by adding 1 to it
  since the tm_mon range is from 0 to 11 */
  year   = utm->tm_year + 1900;
  month  = utm->tm_mon  + 1;
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

double JDtoGMST( double jd )
{
	double Gmst;

	/* Julian Centuries since J2000.0 */
	T = ( jd - J2000 ) / 36525.;

	/* Greewich Mean Sidereal Time */

	Gmst = 280.46061837 
		+ 360.98564736629*(jd-J2000) 
		+ 0.000387933*T*T 
		- T*T*T/38710000.0;

	return Gmst;
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

/* sprint the variable a in sexagesimal format into out[].
 * w is the number of spaces for the whole part.
 * fracbase is the number of pieces a whole is to broken into; valid options:
 *	360000:	<w>:mm:ss.ss
 *	36000:	<w>:mm:ss.s
 *	3600:	<w>:mm:ss
 *	600:	<w>:mm.m
 *	60:	<w>:mm
 * return number of characters written to out, not counting final '\0'.
 */
int
fs_sexa (char *out, double a, int w, int fracbase)
{
	char *out0 = out;
	unsigned long n;
	int d;
	int f;
	int m;
	int s;
	int isneg;

	/* save whether it's negative but do all the rest with a positive */
	isneg = (a < 0);
	if (isneg)
	    a = -a;

	/* convert to an integral number of whole portions */
	n = (unsigned long)(a * fracbase + 0.5);
	d = n/fracbase;
	f = n%fracbase;

	/* form the whole part; "negative 0" is a special case */
	if (isneg && d == 0)
	    out += sprintf (out, "%*s-0", w-2, "");
	else
	    out += sprintf (out, "%*d", w, isneg ? -d : d);

	/* do the rest */
	switch (fracbase) {
	case 60:	/* dd:mm */
	    m = f/(fracbase/60);
	    out += sprintf (out, ":%02d", m);
	    break;
	case 600:	/* dd:mm.m */
	    out += sprintf (out, ":%02d.%1d", f/10, f%10);
	    break;
	case 3600:	/* dd:mm:ss */
	    m = f/(fracbase/60);
	    s = f%(fracbase/60);
	    out += sprintf (out, ":%02d:%02d", m, s);
	    break;
	case 36000:	/* dd:mm:ss.s*/
	    m = f/(fracbase/60);
	    s = f%(fracbase/60);
	    out += sprintf (out, ":%02d:%02d.%1d", m, s/10, s%10);
	    break;
	case 360000:	/* dd:mm:ss.ss */
	    m = f/(fracbase/60);
	    s = f%(fracbase/60);
	    out += sprintf (out, ":%02d:%02d.%02d", m, s/100, s%100);
	    break;
	default:
	    printf ("fs_sexa: unknown fracbase: %d\n", fracbase);
	    exit(1);
	}

	return (out - out0);
}

/* convert sexagesimal string str AxBxC to double.
 *   x can be anything non-numeric. Any missing A, B or C will be assumed 0.
 *   optional - and + can be anywhere.
 * return 0 if ok, -1 if can't find a thing.
 */
int
f_scansexa (
const char *str0,	/* input string */
double *dp)		/* cracked value, if return 0 */
{
	double a = 0, b = 0, c = 0;
	char str[128];
	char *neg;
	int r;

	/* copy str0 so we can play with it */
	strncpy (str, str0, sizeof(str)-1);
	str[sizeof(str)-1] = '\0';

	neg = strchr(str, '-');
	if (neg)
	    *neg = ' ';
	r = sscanf (str, "%lf%*[^0-9]%lf%*[^0-9]%lf", &a, &b, &c);
	if (r < 1)
	    return (-1);
	*dp = a + b/60 + c/3600;
	if (neg)
	    *dp *= -1;
	return (0);
}

void getSexComponents(double value, int *d, int *m, int *s)
{

  *d = (int) fabs(value);
  *m = (int) ((fabs(value) - *d) * 60.0);
  *s = (int) rint(((fabs(value) - *d) * 60.0 - *m) *60.0);

  if (value < 0)
   *d *= -1;
}

/* fill buf with properly formatted INumber string. return length */
int
numberFormat (char *buf, const char *format, double value)
{
        int w, f, s;
        char m;

        if (sscanf (format, "%%%d.%d%c", &w, &f, &m) == 3 && m == 'm') {
            /* INDI sexi format */
            switch (f) {
            case 9:  s = 360000; break;
            case 8:  s = 36000;  break;
            case 6:  s = 3600;   break;
            case 5:  s = 600;    break;
            default: s = 60;     break;
            }
            return (fs_sexa (buf, value, w-f, s));
        } else {
            /* normal printf format */
            return (sprintf (buf, format, value));
        }
}

double angularDistance(double fromRA, double fromDEC, double toRA, double toDEC)
{

	double dalpha = DegToRad(fromRA) - DegToRad(toRA);
	double ddelta = DegToRad(fromDEC) - DegToRad(toDEC);

	double sa = sin(dalpha/2.);
	double sd = sin(ddelta/2.);
	
	double hava = sa*sa;
	double havd = sd*sd;

	double aux = havd + cos (DegToRad(fromDEC)) * cos(DegToRad(toDEC)) * hava;
	
	return (RadToDeg ( 2 * fabs(asin( sqrt(aux) ))));
}

/* return current system time in message format */
const char *
timestamp()
{
	static char ts[32];
	struct tm *tp;
	time_t t;

	time (&t);
	tp = gmtime (&t);
	strftime (ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S", tp);
	return (ts);
}

