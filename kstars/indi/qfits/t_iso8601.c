/*----------------------------------------------------------------------------*/
/**
  @file		t_iso8601.c
  @author	N. Devillard
  @date		Aug 1999
  @version	$Revision$
  @brief	Get date/time, possibly in ISO8601 format.

  This module contains various utilities to get the current date/time, 
  and possibly format it according to the ISO 8601 format.
*/
/*----------------------------------------------------------------------------*/

/*
	$Id$
	$Author$
	$Date$
	$Revision$
*/

/*-----------------------------------------------------------------------------
   								Includes
 -----------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>
#include "t_iso8601.h"

/*-----------------------------------------------------------------------------
   								Macros
 -----------------------------------------------------------------------------*/
/** Get century from a date in long format */
#define GET_CENTURY(d)      (int) ( (d) / 1000000L)
/** Get century year from a date in long format */
#define GET_CCYEAR(d)       (int) ( (d) / 10000L)
/** Get year from a date in long format */
#define GET_YEAR(d)         (int) (((d) % 1000000L) / 10000L)
/** Get month from a date in long format */
#define GET_MONTH(d)        (int) (((d) % 10000L) / 100)
/** Get day from a date in long format */
#define GET_DAY(d)          (int) ( (d) % 100)

/** Get hours from a date in long format */
#define GET_HOUR(t)         (int) ( (t) / 1000000L)
/** Get minutes from a date in long format */
#define GET_MINUTE(t)       (int) (((t) % 1000000L) / 10000L)
/** Get seconds from a date in long format */
#define GET_SECOND(t)       (int) (((t) % 10000L) / 100)
/** Get centi-seconds from a date in long format */
#define GET_CENTI(t)        (int) ( (t) % 100)

/** Make date in long format from its components */
#define MAKE_DATE(c,y,m,d)  (long) (c) * 1000000L +                          \
                            (long) (y) * 10000L +                            \
                            (long) (m) * 100 + (d)
/** Make time in long format from its components */
#define MAKE_TIME(h,m,s,c)  (long) (h) * 1000000L +                          \
                            (long) (m) * 10000L +                            \
                            (long) (s) * 100 + (c)

/**  Interval values, specified in centiseconds */

#define INTERVAL_CENTI      1
#define INTERVAL_SEC        100
#define INTERVAL_MIN        6000
#define INTERVAL_HOUR       360000L
#define INTERVAL_DAY        8640000L

/*-----------------------------------------------------------------------------
							Private to this module
 -----------------------------------------------------------------------------*/

static long timer_to_date(time_t time_secs) ;
static long timer_to_time(time_t time_secs) ;

/*-----------------------------------------------------------------------------
						    Static Function code
 -----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/**
  @brief	Returns the current date as a long (CCYYMMDD).
  @return	The current date as a long number.

  Returns the current date as a long value (CCYYMMDD). Since most
  system clocks do not return a century, this function assumes that
  all years 80 and above are in the 20th century, and all years 00 to
  79 are in the 21st century.  For best results, consume before 1 Jan
  2080.
  Example:	19 Oct 2000 is returned as 20001019
 */
/*----------------------------------------------------------------------------*/
long date_now (void)
{
    return (timer_to_date (time (NULL)));
}

/*----------------------------------------------------------------------------*/
/**
  @brief	Returns the current time as a long (HHMMSSCC).
  @return	The current time as a long number.

  Returns the current time as a long value (HHMMSSCC). If the system
  clock does not return centiseconds, these are set to zero.

  Example: 15:36:12.84 is returned as 15361284
 */
/*----------------------------------------------------------------------------*/
long time_now(void)
{
    struct timeval time_struct;

    gettimeofday (&time_struct, 0);
    return (timer_to_time (time_struct.tv_sec)
                         + time_struct.tv_usec / 10000);
}

/*----------------------------------------------------------------------------*/
/**
  @brief	Converts a timer value to a date.
  @param	time_secs	Current time definition in seconds.
  @return	Current date as a long (CCYYMMDD).

  Converts the supplied timer value into a long date value. Dates are
  stored as long values: CCYYMMDD. If the supplied value is zero,
  returns zero.  If the supplied value is out of range, returns 1
  January, 1970 (19700101). The timer value is assumed to be UTC
  (GMT).
 */
/*----------------------------------------------------------------------------*/
static long timer_to_date(time_t time_secs)
{
    struct tm *time_struct;

	if (time_secs == 0) {
		return 0;
	} else {
		/*  Convert into a long value CCYYMMDD */
		time_struct = localtime (&time_secs);
		if (time_struct) {
			time_struct-> tm_year += 1900;
			return (MAKE_DATE (	time_struct-> tm_year / 100,
								time_struct-> tm_year % 100,
								time_struct-> tm_mon + 1,
								time_struct-> tm_mday));
		} else {
			return (19700101);
		}
	}
}

/*----------------------------------------------------------------------------*/
/**
  @brief	Convert a timer value to a time.
  @param	time_secs	Current time definition in seconds.
  @return	Current time as a long.

  Converts the supplied timer value into a long time value.  Times are
  stored as long values: HHMMSS00.  Since the timer value does not
  hold centiseconds, these are set to zero.  If the supplied value was
  zero or invalid, returns zero.  The timer value is assumed to be UTC
  (GMT).
 */
/*----------------------------------------------------------------------------*/
static long timer_to_time(time_t time_secs)
{
    struct tm *time_struct;

    if (time_secs == 0) {
		return 0;
	} else {
        /*  Convert into a long value HHMMSS00 */
        time_struct = localtime (&time_secs);
        if (time_struct) {
            return (MAKE_TIME (time_struct-> tm_hour,
                               time_struct-> tm_min,
                               time_struct-> tm_sec,
                               0));
		} else {
            return 0;
		}
	}
}

/*-----------------------------------------------------------------------------
  							Function codes
 -----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/**
  @brief    Returns the current date as a static string.
  @return   Pointer to statically allocated string.
 
  Build and return a string containing the date of today in ISO8601
  format. The returned pointer points to a statically allocated string
  in the function, so no need to free it.
  */
/*----------------------------------------------------------------------------*/
char * get_date_iso8601(void)
{
    static char date_iso8601[20] ;
    long        curdate ;

    curdate  = date_now() ;
    sprintf(date_iso8601, "%04d-%02d-%02d",
            GET_CCYEAR(curdate),
            GET_MONTH(curdate),
            GET_DAY(curdate));
    return date_iso8601 ;
}

/*----------------------------------------------------------------------------*/
/**
  @brief    Returns the current date and time as a static string.
  @return   Pointer to statically allocated string
 
  Build and return a string containing the date of today and the
  current time in ISO8601 format. The returned pointer points to a
  statically allocated string in the function, so no need to free it.
 */
/*----------------------------------------------------------------------------*/
char * get_datetime_iso8601(void)
{
    static char date_iso8601[20] ;
    long        curdate ;
    long        curtime ;

    curdate  = date_now() ;
    curtime  = time_now() ;

    sprintf(date_iso8601, "%04d-%02d-%02dT%02d:%02d:%02d",
            GET_CCYEAR(curdate),
            GET_MONTH(curdate),
            GET_DAY(curdate),
            GET_HOUR(curtime),
            GET_MINUTE(curtime),
            GET_SECOND(curtime));
    return date_iso8601 ;
}

#ifdef TEST
int main(int argc, char *argv[])
{
	printf( "date now           %ld\n"
			"time now           %ld\n"
			"date iso8601       %s\n"
			"date/time iso 8601 %s\n",
			date_now(),
			time_now(),
			get_date_iso8601(),
			get_datetime_iso8601());
		
	return 0 ;
}
#endif
/* vim: set ts=4 et sw=4 tw=75 */
