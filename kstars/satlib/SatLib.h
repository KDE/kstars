/***************************************************************************\
*          SATLIB : A satellite position/orbital prediction library         *
*         Made by Patrick Chevalley, Vincent Suc & Jean-Baptiste Butet.     *
*                         Copyright (C) 2005                                *
*                         http://www.sf.net/predictsatlib                   *
*****************************************************************************
* This Library is based on : PREDICT, Made by John A. Magliacan             *
*****************************************************************************
*   SGP4/SDP4 code was derived from Pascal routines originally written by   *
*       Dr. TS Kelso, and converted to C by Neoklis Kyriazis, 5B4AZ         *
*****************************************************************************
*                                                                           *
*   This program is free software; you can redistribute it and/or modify    *
*   it under the terms of the GNU General Public License as published by    *
*   the Free Software Foundation; either version 2 of the License, or       *
*   (at your option) any later version.                                     *
*                                                                           *
*   This program is distributed in the hope that it will be useful,         *
*   but WITHOUT ANY WARRANTY; without even the implied warranty of          *
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the           *
*   GNU General Public License for more details.                            *
*                                                                           *
*   You should have received a copy of the GNU General Public License       *
*   along with this program; if not, write to the                           *
*   Free Software Foundation, Inc.,                                         *
*   Foundation, Inc., 51 Franklin Street, Fifth Floor,                      *
*   Boston, MA  02110-1301  USA						    *
*                                                                           *
*   Licensed under the GNU GPL                                              *
*                                                                           *
*****************************************************************************
*Changelog :                                                                *
*                                                                           *
*09/10/2005 :SatLib, Project (re)started, v0.1                              *
*26/05/1991 :Predict, Project started                                       *
\***************************************************************************/

#ifndef SATLIB_H_
#define SATLIB_H_

#include <time.h>

/* Result structure                                */
typedef struct 
{
    double sat_ele; 	/* elevation */
    double sat_azi;	/* azimuth */
    double jd;		/* Unix time of position */
    long catnum;	/* satellite catalog number */
    long range;		/* satellite distance range */
    long rev_num;	/* revolution number */
    long phase;		/* phase angle */
    long sat_lat;	/* satellite latitude */
    long sat_long;	/* satellite longitude */
    char visibility; 	/* + = in sunlight, night time, visible above horizon
			   * = in sunlight
			   blank  = in earth shadow */
    char name[25];  	/* satellite name */
    char designator[10];/* International designator */
} SPositionSat;
	
#ifdef _WIN32 
  #ifdef BUILD_DLL
/* the dll exports */
    #define SATLIB_EXPORT __declspec(dllexport)
  #else
/* the exe imports */
    #define SATLIB_EXPORT __declspec(dllimport)
  #endif
#else 
  #ifndef BUILD_DLL
/* no declarations necessary for import */
    #define SATLIB_EXPORT
  #else
/* the exe imports */
    #if __GNUC__- 0 >= 4
      #define SATLIB_EXPORT __attribute__ ((visibility("default")))
    #else
      #define SATLIB_EXPORT
    #endif
  #endif 
#endif

/* function to be imported/exported */
SATLIB_EXPORT int SatInit(char *ObsName, double ObsLat, double ObsLong, double ObsAlt, char *TLE_file);
SATLIB_EXPORT int SatPassList(double jd_start, double jd_end, SPositionSat *pPositionSat[]);	
SATLIB_EXPORT int SatNextPass(char *satname,  double jd_start, SPositionSat *pPositionSat[]);
SATLIB_EXPORT int SatFindPosition(char *satname, double jd_start, double step, long number_pos, SPositionSat *pPositionSat[]);
SATLIB_EXPORT double Julian_Date(struct tm *cdate);
SATLIB_EXPORT void Date_Time(double julian_date,struct  tm *cdate);


#endif
