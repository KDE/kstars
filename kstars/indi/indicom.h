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
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

*/

/** \file indicom.h
    \brief Implementations for common astronomical routines.
    
    \author Jason Harris
    \author Elwood C. Downey
    \author Jasem Mutlaq
*/

#ifndef INDICOM_H
#define INDICOM_H

#include <time.h>

#define J2000 2451545.0
#define TRACKING_THRESHOLD	0.05		/* 3' for tracking */
#define ERRMSG_SIZE 1024

extern const char * Direction[];
extern const char * SolarSystem[];

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \defgroup mathFunctions Handy math routines.
 */
/*@{*/

/** \brief Convert degrees to radians.
    \param num number in degrees.
    \return number in radians.
*/
double DegToRad( double num );

/** \brief Convert radians to degrees.
    \param num number in radians.
    \return number in degrees.
*/
double RadToDeg( double num );

/** \brief Get the sin and cosine of a number.

    The function attempts to use GNU sincos extension when possible. The sin and cosine values for \e Degrees are stored in the \e sina and \e cosa parameters.
    \param Degrees the number in degrees to return its sin and cosine.
    \param sina pointer to a double to hold the sin value.
    \param cosa pointer to a double to hold the cosine value.
*/
void SinCos( double Degrees, double *sina, double *cosa );

/*@}*/

/**
 * \defgroup ephemerisFunctions Common ephemeris functions.
   
   The ephemeris functions are date-dependent. Call updateAstroValues() to update orbital values used in many algorithms before using any ephemeris function. You only need to call updateAstroValues() again if you need to update the orbital values for a different date.
 */
/*@{*/

/** \brief Returns the obliquity of orbit.
*/
double obliquity();

/** \brief  Returns the constant of aberration (20.49 arcsec). */
double constAberr();

/** \brief Returns the mean solar anomaly. */
double sunMeanAnomaly();

/** \brief Returns the mean solar longitude. */
double sunMeanLongitude();

/** \brief Returns the true solar anomaly. */
double sunTrueAnomaly();

/** \brief Returns the true solar longitude. */
double sunTrueLongitude();

/** \brief Returns the longitude of the Earth's perihelion point. */
double earthPerihelionLongitude();

/** \brief Returns eccentricity of Earth's orbit.*/
double earthEccentricity();

/** \brief Returns the change in obliquity due to the nutation of Earth's orbit. */
double dObliq();

/** \brief Returns the change in Ecliptic Longitude due to nutation. */
double dEcLong();

/** \brief Returns Julian centuries since J2000*/
double julianCenturies();

/** \brief Returns element of P1 precession array at position [i1][i2] */
double p1( int i1, int i2 );

/** \brief Returns element of P2 precession array at position [i1][i2] */
double p2( int i1, int i2 );

/** \brief Update all orbital values for the given date as an argument. Any subsecquent functions will use values affected by this date until changed.
    \param jd Julian date
 */
void updateAstroValues( double jd );

/** \brief Calculates the declination on the celestial sphere at 0 degrees altitude given the siderial time and latitude.
    \param latitude Current latitude.
    \param SDTime Current sideral time.
    \return Returns declinatation at 0 degrees altitude for the given parameters.
 */
double calculateDec(double latitude, double SDTime);

/** \brief Calculates the right ascension on the celestial sphere at 0 degrees azimuth given the siderial time.
    \param SDTime Current sidereal time.
    \return Returns right ascension at 0 degrees azimith for the given parameters.
 */
double calculateRA(double SDTime);

/** \brief Calculates the angular distance between two points on the celestial sphere. 
    \param fromRA Right ascension of starting point in degrees.
    \param fromDEC Declination of starting point in degrees.
    \param toRA Right ascension of ending point in degrees.
    \param toDEC Declination of ending point in degrees.
    \return Angular seperation in degrees.
*/
double angularDistance(double fromRA, double fromDEC, double toRA, double toDEC);

/** \brief Nutate a given RA and Dec.
    \param RA a pointer to a double containing the Right ascension in degrees to nutate. The function stores the processed Right ascension back in this variable.
    \param Dec a pointer to a double containing the delination in degrees to nutate. The function stores the processed delination back in this variable.
*/
void nutate(double *RA, double *Dec);

/** \brief Aberrate a given RA and Dec.
    \param RA a pointer to a double containing the Right ascension in degrees to aberrate. The function stores the processed Right ascension back in this variable.
    \param Dec a pointer to a double containing the delination in degrees to aberrate. The function stores the processed delination back in this variable.
*/
void aberrate(double *RA, double *Dec);

/** \brief Precess the given RA and Dec from any epoch to any epoch.
    \param jd0 starting epoch.
    \param jdf final epoch.
    \param RA a pointer to a double containing the Right ascension in degrees to precess. The function stores the processed Right ascension back in this variable.
    \param Dec a pointer to a double containing the delination in degrees to precess. The function stores the processed delination back in this variable.
*/
void precessFromAnyEpoch(double jd0, double jdf, double *RA, double *Dec);

/** \brief Calculate the apparent coordiantes for RA and Dec from any epoch to any epoch.
    \param jd0 starting epoch.
    \param jdf final epoch.
    \param RA a pointer to a double containing the Right ascension in hours. The function stores the processed Right ascension back in this variable.
    \param Dec a pointer to a double containing the delination in degrees. The function stores the processed delination back in this variable.
*/
void apparentCoord(double jd0, double jdf, double *RA, double *Dec);

/*@}*/


/**
 * \defgroup convertFunctions Handy formatting and conversion routines.
 */
/*@{*/

/** \brief Converts a sexagesimal number to a string.
 
   sprint the variable a in sexagesimal format into out[].
	
  \param out a pointer to store the sexagesimal number.
  \param a the sexagesimal number to convert.
  \param w the number of spaces in the whole part.
  \param fracbase is the number of pieces a whole is to broken into; valid options:\n
          \li 360000:	\<w\>:mm:ss.ss
	  \li 36000:	\<w\>:mm:ss.s
 	  \li 3600:	\<w\>:mm:ss
 	  \li 600:	\<w\>:mm.m
 	  \li 60:	\<w\>:mm
  
  \return number of characters written to out, not counting final null terminator.
 */
int fs_sexa (char *out, double a, int w, int fracbase);

/** \brief convert sexagesimal string str AxBxC to double.

    x can be anything non-numeric. Any missing A, B or C will be assumed 0. Optional - and + can be anywhere.
    
    \param str0 string containing sexagesimal number.
    \param dp pointer to a double to store the sexagesimal number.
    \return return 0 if ok, -1 if can't find a thing.
 */
int f_scansexa (const char *str0, double *dp);

/** \brief Extract ISO 8601 time and store it in a tm struct.
    \param timestr a string containing date and time in ISO 8601 format.
    \param utm a pointer to a \e tm structure to store the extracted time and date.
    \return 0 on success, -1 on failure.
*/
int extractISOTime(char *timestr, struct tm *utm);

/** \brief Converts Universal Time to Julian Date.
    \param utm a pointer to a structure holding the universal time and date.
    \return The Julian date fot the passed universal time.
*/
double UTtoJD(struct tm *utm);

/** \brief Return the degree, minute, and second components of a sexagesimal number.
    \param value sexagesimal number to decompose.
    \param d a pointer to double to store the degrees field.
    \param m a pointer to a double to store the minutes field.
    \param s a pointer to a double to store the seconds field.
*/

/** \brief Converts Julian Date to Greenwich Sidereal Time.
    \param jd The Julian date 
    \return GMST in degrees
*/
double JDtoGMST( double jd );

void getSexComponents(double value, int *d, int *m, int *s);

/** \brief Fill buffer with properly formatted INumber string.
    \param buf to store the formatted string.
    \param format format in sprintf style.
    \param value the number to format.
    \return length of string.
*/
int numberFormat (char *buf, const char *format, double value);

/** \brief Fill buffer with properly formatted INumber string.
    \param buf to store the formatted string.
    \param format format in sprintf style.
    \param value the number to format.
    \return length of string.
*/
int numberFormat (char *buf, const char *format, double value);

/** \brief Create an ISO 8601 formatted time stamp. The format is YYYY-MM-DDTHH:MM:SS
    \return The formatted time stamp.
*/
const char *timestamp (void);

/*@}*/

#ifdef __cplusplus
}
#endif


#endif
