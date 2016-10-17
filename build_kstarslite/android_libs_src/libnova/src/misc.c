/* $Id: misc.c,v 1.10 2008-04-12 19:13:29 pkubanek Exp $
 **
 * Copyright (C) 1999, 2000 Juan Carlos Remis
 * Copyright (C) 2002 Liam Girdwood
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *     
 */

/*------------------------------------------------------------------------*/
/*                                                                        */
/*  Module:                                                               */
/*                                                                        */
/*  Description:                                                          */
/*                                                                        */
/*                                                                        */
/*  "CAVEAT UTILITOR".                                                    */
/*                                                                        */
/*                   "Non sunt multiplicanda entia praeter necessitatem"  */
/*                                                   Guillermo de Occam.  */
/*------------------------------------------------------------------------*/
/*  Revision History:                                                     */
/*                                                                        */
/*------------------------------------------------------------------------*/

/**/
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <libnova/libnova.h>

#ifndef __APPLE__
#include <malloc.h>
#endif

/* Include unistd.h only if not on a Win32 platform */
#ifndef __WIN32__
#include <unistd.h>
#else

/* local types and macros */
typedef int BOOL;
#define TRUE 1
#define FALSE 0
#define iswhite(c)  ((c)== ' ' || (c)=='\t')

#endif  /* !__WIN32__ */

/*! \fn char *trim(char *x)
 * \param x Character which will be trimmed.
 * \return Trimmed character
 *
 * Trim from character leading whitespaces.
 */
static inline char *trim(char *x)
{
    char *y;

    if(!x)
        return(x);

    y = x + strlen(x)-1;
    while (y >= x && isspace(*y)) 
        *y-- = 0; /* skip white space */

    return x;
}


/*! \fn void skipwhite(char **s)
 * \param s String in which whites will be skipped.
 *
 * Skip white spaces.
 */
static inline void skipwhite(char **s)
{
   while (iswhite(**s))
        (*s)++;
}


/*! \fn double get_dec_location(char * s)
* \param s Location string
* \return angle in degrees
*
* Obtains Latitude, Longitude, RA or Declination from a string.
*
*  If the last char is N/S doesn't accept more than 90 degrees.            
*  If it is E/W doesn't accept more than 180 degrees.                      
*  If they are hours don't accept more than 24:00                          
*                                                                          
*  Any position can be expressed as follows:                               
*  (please use a 8 bits charset if you want                                
*  to view the degrees separator char '0xba')                              
*
*  42.30.35,53                                                             
*  90�0'0,01 W                                                             
*  42�30'35.53 N                                                           
*  42�30'35.53S                                                            
*  42�30'N                                                                 
*  - 42.30.35.53                                                           
*   42:30:35.53 S                                                          
*  + 42.30.35.53                                                           
*  +42�30 35,53                                                            
*   23h36'45,0                                                             
*                                                                          
*                                                                          
*  42:30:35.53 S = -42�30'35.53"                                           
*  + 42 30.35.53 S the same previous position, the plus (+) sign is        
*  considered like an error, the last 'S' has precedence over the sign     
*                                                                          
*  90�0'0,01 N ERROR: +- 90�0'00.00" latitude limit                        
*
*/
double get_dec_location(char *s)
{

	char *ptr, *dec, *hh;
	BOOL negative = FALSE;
	char delim1[] = " :.,;�DdHhMm'\n\t";
	char delim2[] = " NSEWnsew\"\n\t";
	int dghh = 0, minutes = 0;
	double seconds = 0.0, pos;
	short count;

	enum _type {
    	    HOURS, DEGREES, LAT, LONG
	} type;

	if (s == NULL || !*s)
		return(-0.0);
	count = strlen(s) + 1;
	if ((ptr = (char *) alloca(count)) == NULL)
		return (-0.0);
	memcpy(ptr, s, count);
	trim(ptr);
	skipwhite(&ptr);
        
    /* the last letter has precedence over the sign */
	if (strpbrk(ptr,"SsWw") != NULL) 
		negative = TRUE;

	if (*ptr == '+' || *ptr == '-')
		negative = (char) (*ptr++ == '-' ? TRUE : negative);	
	skipwhite(&ptr);
	if ((hh = strpbrk(ptr,"Hh")) != NULL && hh < ptr + 3)
            type = HOURS;
        else 
            if (strpbrk(ptr,"SsNn") != NULL)
		type = LAT;
	    else 
 	        type = DEGREES; /* unspecified, the caller must control it */

	if ((ptr = strtok(ptr,delim1)) != NULL)
		dghh = atoi (ptr);
	else
		return (-0.0);

	if ((ptr = strtok(NULL,delim1)) != NULL) {
		minutes = atoi (ptr);
		if (minutes > 59)
		    return (-0.0);
	}else
		return (-0.0);

	if ((ptr = strtok(NULL,delim2)) != NULL) {
		if ((dec = strchr(ptr,',')) != NULL)
			*dec = '.';
		seconds = strtod (ptr, NULL);
		if (seconds > 59)
			return (-0.0);
	}
	
	if ((ptr = strtok(NULL," \n\t")) != NULL) {
		skipwhite(&ptr);
		if (*ptr == 'S' || *ptr == 'W' || *ptr == 's' || *ptr == 'w')
			    negative = TRUE;
	}
        pos = dghh + minutes /60.0 + seconds / 3600.0;

	if (type == HOURS && pos > 24.0)
		return (-0.0);
	if (type == LAT && pos > 90.0)
		return (-0.0);
	else
	    if (pos > 180.0)
		return (-0.0);

	if (negative)
		pos = 0.0 - pos;

	return (pos);

}


/*! \fn char * get_humanr_location(double location)    
* \param location Location angle in degress
* \return Angle string
*
* Obtains a human readable location in the form: dd�mm'ss.ss"             
*/
char *get_humanr_location(double location)
{
    static char buf[16];
    double deg = 0.0;
    double min = 0.0;
    double sec = 0.0;

    *buf = 0;
    sec = 60.0 * (modf(location, &deg));
    if (sec < 0.0)
        sec *= -1;

    sec = 60.0 * (modf(sec, &min));
    sprintf(buf,"%+d�%d'%.2f\"",(int)deg, (int) min, sec);

    return buf;
}

/*! \fn double interpolate3 (double n, double y1, double y2, double y3)
* \return interpolation value
* \param n Interpolation factor
* \param y1 Argument 1
* \param y2 Argument 2
* \param y3 Argument 3
*
* Calculate an intermediate value of the 3 arguments for the given interpolation
* factor.
*/
double interpolate3 (double n, double y1, double y2, double y3)
{
	double y, a, b, c;
	
	/* equ 3.2 */
	a = y2 - y1;
	b = y3 - y2;
	c = a - b;
	
	/* equ 3.3 */
	y = y2 + n / 2.0 * (a + b + n * c);
	
	return y;
}


/*! \fn double interpolate5 (double n, double y1, double y2, double y3, double y4, double y5)
* \return interpolation value
* \param n Interpolation factor
* \param y1 Argument 1
* \param y2 Argument 2
* \param y3 Argument 3
* \param y4 Argument 4
* \param y5 Argument 5
*
* Calculate an intermediate value of the 5 arguments for the given interpolation
* factor.
*/
double interpolate5 (double n, double y1, double y2, double y3, double y4,
		double y5)
{
	double y, A, B, C, D, E, F, G, H, J, K;
	double n2, n3, n4;
	
	/* equ 3.8 */
	A = y2 - y1;
	B = y3 - y2;
	C = y4 - y3;
	D = y5 - y4;
	E = B - A;
	F = C - B;
	G = D - C;
	H = F - E;
	J = G - F;
	K = J - H;
	
	y = 0.0;
	n2 = n* n;
	n3 = n2 * n;
	n4 = n3 * n;
	
	y += y3;
	y += n * ((B + C ) / 2.0 - (H + J) / 12.0);
	y += n2 * (F / 2.0 - K / 24.0);
	y += n3 * ((H + J) / 12.0);
	y += n4 * (K / 24.0);
	
	return y;
}
