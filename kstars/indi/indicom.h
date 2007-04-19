/*
    INDI LIB
    Common routines used by all drivers
    Copyright (C) 2003 by Jason Harris (jharris@30doradus.org)
    			  Elwood C. Downey
			  Jasem Mutlaq

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

/** \file indicom.h
    \brief Implementations for common driver routines.

    The INDI Common Routine Library provides astronomical, mathematical, and formatting routines employed by many INDI drivers. Currently, the library is composed of the following sections:

    <ul>
    <li>Math Functions</li>
    <li>Ephemeris Functions</li>
    <li>Formatting Functions</li>
    <li>Conversion Functions</li>
    <li>TTY Functions</li>


    </ul>
    
    \author Jason Harris
    \author Elwood C. Downey
    \author Jasem Mutlaq
*/

#ifndef INDICOM_H
#define INDICOM_H

#include <time.h>

#include "config-kstars.h"


#define J2000 2451545.0
#define ERRMSG_SIZE 1024
#define INDI_DEBUG


extern const char * Direction[];
extern const char * SolarSystem[];

/* TTY Error Codes */
enum TTY_ERROR { TTY_OK=0, TTY_READ_ERROR=-1, TTY_WRITE_ERROR=-2, TTY_SELECT_ERROR=-3, TTY_TIME_OUT=-4, TTY_PORT_FAILURE=-5, TTY_PARAM_ERROR=-6, TTY_ERRNO = -7};

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \defgroup mathFunctions Math Functions: Functions to perform common math routines.
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
 * \defgroup ttyFunctions TTY Functions: Functions to perform common terminal access routines.
*/

/*@{*/

/** \brief read buffer from terminal
    \param fd file descriptor
    \param buf pointer to store data. Must be initilized and big enough to hold data.
    \param nbytes number of bytes to read.
    \param timeout number of seconds to wait for terminal before a timeout error is issued.
    \param nbytes_read the number of bytes read.
    \return On success, it returns TTY_OK, otherwise, a TTY_ERROR code.
*/
int tty_read(int fd, char *buf, int nbytes, int timeout, int *nbytes_read);

/** \brief read buffer from terminal with a delimiter
    \param fd file descriptor
    \param buf pointer to store data. Must be initilized and big enough to hold data.
    \param stop_char if the function encounters \e stop_char then it stops reading and returns the buffer.
    \param timeout number of seconds to wait for terminal before a timeout error is issued.
    \param nbytes_read the number of bytes read.
    \return On success, it returns TTY_OK, otherwise, a TTY_ERROR code.
*/

int tty_read_section(int fd, char *buf, char stop_char, int timeout, int *nbytes_read);


/** \brief Writes a buffer to fd.
    \param fd file descriptor
    \param buffer a null-terminated buffer to write to fd.
    \param nbytes number of bytes to write from \e buffer
    \param nbytes_written the number of bytes written
    \return On success, it returns TTY_OK, otherwise, a TTY_ERROR code.
*/
int tty_write(int fd, const char * buffer, int nbytes, int *nbytes_written);

/** \brief Writes a null terminated string to fd.
    \param fd file descriptor
    \param buffer the buffer to write to fd.
    \param nbytes_written the number of bytes written
    \return On success, it returns TTY_OK, otherwise, a TTY_ERROR code.
*/
int tty_write_string(int fd, const char * buffer, int *nbytes_written);


/** \brief Establishes a tty connection to a terminal device.
    \param device the device node. e.g. /dev/ttyS0
    \param bit_rate bit rate
    \param word_size number of data bits, 7 or 8, USE 8 DATA BITS with modbus
    \param parity 0=no parity, 1=parity EVEN, 2=parity ODD
    \param stop_bits number of stop bits : 1 or 2
    \param fd \e fd is set to the file descriptor value on success.
    \return On success, it returns TTY_OK, otherwise, a TTY_ERROR code.
    \author Wildi Markus
*/

int tty_connect(const char *device, int bit_rate, int word_size, int parity, int stop_bits, int *fd);

/** \brief Closes a tty connection and flushes the bus.
    \param fd the file descriptor to close.
    \return On success, it returns TTY_OK, otherwise, a TTY_ERROR code.
*/
int tty_disconnect(int fd);

/** \brief Retrieve the tty error message
    \param err_code the error code return by any TTY function.
    \param err_msg an initialized buffer to hold the error message.
    \param err_msg_len length in bytes of \e err_msg
*/
void tty_error_msg(int err_code, char *err_msg, int err_msg_len);

int tty_timeout(int fd, int timeout);
/*@}*/

/**
 * \defgroup ephemerisFunctions Ephemeris Functions: Functions to perform common ephemeris routines.
   
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
    \return Angular separation in degrees.
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
 * \defgroup convertFunctions Formatting Functions: Functions to perform handy formatting and conversion routines.
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

/** \brief Create an ISO 8601 formatted time stamp. The format is YYYY-MM-DDTHH:MM:SS
    \return The formatted time stamp.
*/
const char *timestamp (void);

/*@}*/

#ifdef __cplusplus
}
#endif


#endif
