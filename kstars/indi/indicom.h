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

#ifndef INDICOM_H
#define INDICOM_H

#include <time.h>

#define J2000 2451545.0

enum SexFormat { XXYYZZ = 0 , SXXYYZZ, XXYY, SXXYY, XXYYZ, XXXYY };

extern const char * Direction[];
extern const char * SolarSystem[];

#ifdef __cplusplus
extern "C" {
#endif

double DegToRad( double num );
double RadToDeg( double num );
void SinCos( double Degrees, double *sina, double *cosa );

double obliquity();
/**@returns the constant of aberration (20.49 arcsec). */
double constAberr();
/**@returns the mean solar anomaly. */
double sunMeanAnomaly();
/**@returns the mean solar longitude. */
double sunMeanLongitude();
/**@returns the true solar anomaly. */
double sunTrueAnomaly();
/**@returns the true solar longitude. */
double sunTrueLongitude();
/**@returns the longitude of the Earth's perihelion point. */
double earthPerihelionLongitude();
/**@returns eccentricity of Earth's orbit.*/
double earthEccentricity();
/**@returns the change in obliquity due to the nutation of Earth's orbit. */
double dObliq();
/**@returns the change in Ecliptic Longitude due to nutation. */
double dEcLong();
/**@returns Julian centuries since J2000*/
double julianCenturies();
/**@returns Julian Millenia since J2000*/
/*double julianMillenia(); */
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

int validateSex(const char * str, float *x, float *y, float *z);
void formatSex(double number, char * str, int mode);
int getSex(const char *str, double * value);
int extractISOTime(char *timestr, struct tm *utm);
double UTtoJD(struct tm *utm);

#ifdef __cplusplus
}
#endif


#endif
