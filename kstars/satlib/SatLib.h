/*
** SatLib.h
*/

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
    #define EXPORT __declspec(dllexport)
    #define declare_func
  #else
/* the exe imports */
    #define EXPORT __declspec(dllimport)
    #define declare_func
  #endif
#else 
  #ifdef BUILD_DLL
/* no export necessary */
    #undef declare_func
  #else
/* the exe imports */
    #define EXPORT
    #define declare_func
  #endif 
#endif

#ifdef declare_func
/* function to be imported/exported */
   EXPORT int SatInit(char *ObsName, double ObsLat, double ObsLong, double ObsAlt, char *TLE_file);
   EXPORT int SatPassList(double jd_start, double jd_end, SPositionSat *pPositionSat[]);	
   EXPORT int SatNextPass(char *satname,  double jd_start, SPositionSat *pPositionSat[]);
   EXPORT int SatFindPosition(char *satname, double jd_start, double step, long number_pos, SPositionSat *pPositionSat[]);
   EXPORT double Julian_Date(struct tm *cdate);
   EXPORT void Date_Time(double julian_date,struct  tm *cdate);
#endif


#endif
