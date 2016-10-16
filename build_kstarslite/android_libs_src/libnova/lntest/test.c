/* 
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA. 

Copyright 2000 Liam Girdwood 
Copyright 2008-2009 Petr Kubanek*/

#define _GNU_SOURCE

#if defined(__MINGW__) || defined(__MINGW32__) || defined(__MINGW64__)
#define _USE_32BIT_TIME_T
#endif	//__MINGW__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libnova/libnova.h>
#include <sys/time.h>
#ifndef __WIN32__
    #include <unistd.h>
#endif 

#ifdef __WIN32__
// the usleep function does not exist in visual studio
void usleep(int miliseconds)
{
    clock_t endwait;
    endwait = clock () + miliseconds * CLOCKS_PER_SEC / 1000;
    while (clock() < endwait) {}
}

#endif

/*
 * Define DATE or SYS_DATE for testing VSOP87 theory with other JD
 */
//#define DATE
//#define SYS_DATE

// holds number of tests
static int test_number = 0;

static struct timeval start, end;

static void start_timer(void)
{
	gettimeofday(&start, NULL);
}

static void end_timer(void)
{
	double secs;

	gettimeofday(&end, NULL);
	secs = ((end.tv_sec * 1000000 + end.tv_usec) -
		(start.tv_sec * 1000000 + start.tv_usec)) / 1000000.0;

	fprintf(stdout, "   Time %3.1f msecs\n", secs * 1000.0);
}

static double compare_results(double calc, double expect, double tolerance)
{
	if (calc - expect > tolerance || calc - expect < (tolerance * -1.0))
		return calc - expect;
	else
		return 0;
}

static int test_result(char *test, double calc, double expect, double tolerance)
{
	double diff;
	
	fprintf(stdout, "TEST %s....", test);

	test_number++;
	
	diff = compare_results(calc, expect, tolerance);
	if (diff) {
		fprintf(stdout, "[FAILED]\n");
		fprintf(stdout,
			"	Expected %8.8f but calculated %8.8f %0.12f error.\n\n",
			expect, calc, diff);
		return 1;
	} else {
		fprintf(stdout, "[PASSED]\n");
		fprintf(stdout, "	Expected and calculated %8.8f.\n\n", calc);
		return 0;
	}
}

static int test_str_result(char *test, const char *calc, const char *expect)
{
	int diff;
	
	fprintf(stdout, "TEST %s....", test);

	test_number++;
	
	diff = strcmp(calc, expect);
	if (diff) {
		fprintf(stdout, "[FAILED]\n");
		fprintf(stdout,
			"	Expected %s but calculated %s.\n\n",
			expect, calc);
		return 1;
	} else {
		fprintf(stdout, "[PASSED]\n");
		fprintf(stdout, "	Expected and calculated %s.\n\n", calc);
		return 0;
	}
}


/* test julian day calculations */
static int julian_test(void)
{ 
	double JD, JD2;
	int wday, failed = 0;
	struct ln_date date, pdate;
	struct ln_zonedate zonedate;

	time_t now;
	time_t now_jd;

	/* Get julian day for 04/10/1957 19:00:00 */
	date.years = 1957;
	date.months = 10;
	date.days = 4; 
	date.hours = 19;
	date.minutes = 0;
	date.seconds = 0;
	JD = ln_get_julian_day(&date);
	failed += test_result("(Julian Day) JD for 4/10/1957 19:00:00",
		JD, 2436116.29166667, 0.00001);

	/* Get julian day for 27/01/333 12:00:00 */
	date.years = 333;
	date.months = 1;
	date.days = 27;
	date.hours = 12;
	JD = ln_get_julian_day(&date);
	failed += test_result("(Julian Day) JD for 27/01/333 12:00:00",
		JD, 1842713.0, 0.1);

	/* Get julian day for 30/06/1954 00:00:00 */
	date.years = 1954;
	date.months = 6;
	date.days = 30;
	date.hours = 0;
	JD = ln_get_julian_day(&date);
	failed += test_result("(Julian Day) JD for 30/06/1954 00:00:00",
		JD, 2434923.5, 0.1);

	wday = ln_get_day_of_week(&date);
	failed += test_result("(Julian Day) Weekday No", wday, 3, 0.1);

	/* Test ln_date_to_zonedate and back */

	ln_date_to_zonedate(&date, &zonedate, 7200);
	ln_zonedate_to_date(&zonedate, &date);

	JD = ln_get_julian_day(&date);

	failed += test_result("(Julian Day) ln_date_to_zonedate and "
		"ln_zonedate_to_date check - JD for 30/06/1954 00:00:00",
		JD, 2434923.5, 0.1);

	ln_get_date(JD, &pdate);
	failed += test_result("(Julian Day) Day from JD for 30/06/1954 00:00:00",
		pdate.days, 30, 0.1);

	failed += test_result("(Julian Day) Month from JD for 30/06/1954 00:00:00",
		pdate.months, 6, 0.1);

	failed += test_result("(Julian Day) Year from JD for 30/06/1954 00:00:00",
		pdate.years, 1954, 0.1);

	failed += test_result("(Julian Day) Hour from JD for 30/06/1954 00:00:00",
		pdate.hours, 0, 0.1);

	failed += test_result("(Julian Day) Minute from JD for 30/06/1954 00:00:00",
		pdate.minutes, 0, 0.1);

	failed += test_result("(Julian Day) Second from JD for 30/06/1954 00:00:00",
		pdate.seconds, 0, 0.001);

	JD = ln_get_julian_from_sys();

	usleep (10000);

	JD2 = ln_get_julian_from_sys();

	failed += test_result("(Julian Day) Diferrence between two successive "
		"ln_get_julian_from_sys() calls (it shall never be zero)",
		JD2 - JD, 1e-2/86400.0, .99e-1);

	/* Test that we get from JD same value as is in time_t
	 * struct..was problem before introduction of ln_zonedate (on
	 * machines which doesn't run in UTC).
	 */

	time(&now);
	ln_get_timet_from_julian(ln_get_julian_from_timet(&now), &now_jd);

	failed += test_result("(Julian Day) Difference between time_t from system"
		"and from JD", difftime(now, now_jd), 0, 0);

	return failed;
}

static int dynamical_test(void)
{
	struct ln_date date;
	double TD,JD;
	int failed = 0;
 
	/* Dynamical Time test for 01/01/2000 00:00:00 */
	date.years = 2000;
	date.months = 1;
	date.days = 1;
	date.hours = 0;
	date.minutes = 0;
	date.seconds = 0.0;

	JD = ln_get_julian_day(&date);
	TD = ln_get_jde(JD);
	failed += test_result("(Dynamical Time) TD for 01/01/2000 00:00:00",
		TD, 2451544.50073877, 0.000001);
	return failed;
}

static int heliocentric_test(void)
{
	struct ln_equ_posn object;
	struct ln_date date;
	double JD;
	double diff;
	int failed = 0;

	object.ra = 0.0;
	object.dec = 60.0;

	date.years = 2000;
	date.months = 1;
	date.days = 1;
	date.hours = 0;
	date.minutes = 0;
	date.seconds = 0.0;

	JD = ln_get_julian_day(&date);

	diff = ln_get_heliocentric_time_diff(JD, &object);

	failed += test_result("(Heliocentric time) TD for 01/01, object on 0h +60",
		diff, 15.0 * 0.0001, 0.0001);

	object.ra = 270.0;
	object.dec = 50.0;

	diff = ln_get_heliocentric_time_diff(JD, &object);

	failed += test_result("(Heliocentric time) TD for 01/01, object on 18h +50",
		diff, -16.0 * 0.0001, 0.0001);

	date.months = 8;
	date.days = 8;

	JD = ln_get_julian_day(&date);

	diff = ln_get_heliocentric_time_diff(JD, &object);

	failed += test_result("(Heliocentric time) TD for 08/08, object on 18h +50",
		diff, 12.0 * 0.0001, 0.0001);

	return failed;
}

static int nutation_test(void)
{
	double JD;
	struct ln_nutation nutation;
	struct ln_date date;

	int failed = 0;
		
	JD = 2446895.5;

	ln_get_nutation(JD, &nutation);
	failed += test_result("(Nutation) longitude (deg) for JD 2446895.5",
		nutation.longitude, -3.788 / 3600.0, 0.0000001);
	
	failed += test_result("(Nutation) obliquity (deg) for JD 2446895.5",
		nutation.obliquity, 9.443 / 3600.0, 0.000001);
	
	failed += test_result("(Nutation) ecliptic (deg) for JD 2446895.5",
		nutation.ecliptic, 23 + 26 / 60.0 + 27.407 / 3600.0, 0.000001);

	date.years = 2028;
	date.months = 11;
	date.days = 13;
	date.hours = 4;
	date.minutes = 33;
	date.seconds = 36;

	JD = ln_get_julian_day(&date);

	ln_get_nutation(JD, &nutation);

	failed += test_result("(Nutation) longitude (deg) for JD 2446895.5",
		nutation.longitude, 14.861 / 3600.0, 0.0000001);

	failed += test_result("(Nutation) obliquity (deg) for JD 2446895.5",
		nutation.obliquity, 2.705 / 3600.0, 0.000001);

	failed += test_result("(Nutation) ecliptic (deg) for JD 2446895.5",
		nutation.ecliptic, 23.436, 0.001);

	return failed;
}

static int aber_prec_nut_test()
{
	double JD;
	struct ln_date date;
	struct ln_equ_posn mean_position, nutated, precessed, aberated;
	struct lnh_equ_posn hmean_position;

	int failed = 0;

	date.years = 2028;
	date.months = 11;
	date.days = 13;
	date.hours = 4;
	date.minutes = 33;
	date.seconds = 36;

	JD = ln_get_julian_day(&date);

	hmean_position.ra.hours = 2;
	hmean_position.ra.minutes = 44;
	hmean_position.ra.seconds = 12.9747;

	hmean_position.dec.neg = 0;
	hmean_position.dec.degrees = 49;
	hmean_position.dec.minutes = 13;
	hmean_position.dec.seconds = 39.896;

	ln_hequ_to_equ(&hmean_position, &mean_position);

	failed += test_result("(Nutation) Theta Persei RA",
		mean_position.ra, 41.0540613, 0.00001);
	failed += test_result("(Nutation) Theta Persei DEC",
		mean_position.dec, 49.2277489, 0.00001);

	ln_get_equ_aber(&mean_position, JD, &aberated);

	failed += test_result ("(Aberation) Theta Persei position on 13th November 2028 RA",
		aberated.ra, 41.0623836, 0.0001);
	failed += test_result ("(Aberation) Theta Persei position on 13th November 2028 DEC",
		aberated.dec, 49.2296238, 0.00001);

	ln_get_equ_prec(&aberated, JD, &precessed);

	failed += test_result ("(Aberation + Precession) Theta Persei position on 13th November 2028 RA",
		precessed.ra, 41.5555635, 0.0001);
	failed += test_result ("(Aberation + Precession) Theta Persei position on 13th November 2028 DEC",
		precessed.dec, 49.3503415, 0.00001);

	ln_get_equ_nut(&precessed, JD, &nutated);

	failed += test_result ("(Aberation + Precession + Nutation) Theta Persei position on 13th November 2028 RA",
		nutated.ra, 41.5599646, 0.0001);
	failed += test_result ("(Aberation + Precession + Nutation) Theta Persei position on 13th November 2028 DEC",
		nutated.dec, 49.3520685, 0.00001);

	return failed;
}

static int transform_test(void)
{
	struct lnh_equ_posn hobject, hpollux;
	struct lnh_lnlat_posn hobserver, hecl;
	struct ln_equ_posn object, pollux, equ;
	struct ln_hrz_posn hrz;
	struct ln_lnlat_posn observer, ecl;
	struct ln_gal_posn gal;
	double JD;
	struct ln_date date;
	int failed = 0;

	/* observers position */
	hobserver.lng.neg = 0;
	hobserver.lng.degrees = 282;
	hobserver.lng.minutes = 56;
	hobserver.lng.seconds = 4.0;
	hobserver.lat.neg = 0;
	hobserver.lat.degrees = 38;
	hobserver.lat.minutes = 55;
	hobserver.lat.seconds = 17.0;

	/* object position */
	hobject.ra.hours = 23;
	hobject.ra.minutes = 9;
	hobject.ra.seconds = 16.641;
	hobject.dec.neg = 1;
	hobject.dec.degrees = 6;
	hobject.dec.minutes = 43;
	hobject.dec.seconds = 11.61;

	/* date and time */
	date.years = 1987;
	date.months = 4;
	date.days = 10;
	date.hours = 19;
	date.minutes = 21;
	date.seconds = 0.0;

	JD = ln_get_julian_day(&date);
	ln_hequ_to_equ(&hobject, &object);
	ln_hlnlat_to_lnlat(&hobserver, &observer);
	
	ln_get_hrz_from_equ(&object, &observer, JD, &hrz);
	failed += test_result("(Transforms) Equ to Horiz ALT ",
		hrz.alt, 15.12426274, 0.00000001);
	failed += test_result("(Transforms) Equ to Horiz AZ ",
		hrz.az, 68.03429264, 0.00000001);

	/* try something close to the pole */
	object.dec = 90.0;

	ln_get_hrz_from_equ(&object, &observer, JD, &hrz);
	failed += test_result("(Transforms) Equ to Horiz ALT ",
		hrz.alt, 38.9213888888, 0.00000001);
	failed += test_result("(Transforms) Equ to Horiz AZ ",
		hrz.az, 180.0, 0.00000001);
	
	object.dec = -90.0;

	ln_get_hrz_from_equ(&object, &observer, JD, &hrz);
	failed += test_result("(Transforms) Equ to Horiz ALT ",
		hrz.alt, -38.9213888888, 0.00000001);
	failed += test_result("(Transforms) Equ to Horiz AZ ",
		hrz.az, 0.0, 0.00000001);

	observer.lat *= -1.0;

	ln_get_hrz_from_equ(&object, &observer, JD, &hrz);
	failed += test_result("(Transforms) Equ to Horiz ALT ",
		hrz.alt, 38.9213888888, 0.00000001);
	failed += test_result("(Transforms) Equ to Horiz AZ ",
		hrz.az, 0.0, 0.00000001);
	
	object.dec = 90.0;

	ln_get_hrz_from_equ(&object, &observer, JD, &hrz);
	failed += test_result("(Transforms) Equ to Horiz ALT ",
		hrz.alt, -38.9213888888, 0.00000001);
	failed += test_result("(Transforms) Equ to Horiz AZ ",
		hrz.az, 180.0, 0.00000001);

	/* Equ position of Pollux */
	hpollux.ra.hours = 7;
	hpollux.ra.minutes = 45;
	hpollux.ra.seconds = 18.946;
	hpollux.dec.neg = 0;
	hpollux.dec.degrees = 28;
	hpollux.dec.minutes = 1;
	hpollux.dec.seconds = 34.26;

	ln_hequ_to_equ(&hpollux, &pollux);
	ln_get_ecl_from_equ(&pollux, JD, &ecl);
	
	ln_lnlat_to_hlnlat(&ecl, &hecl);
	failed += test_result("(Transforms) Equ to Ecl longitude ",
		ecl.lng, 113.21542105, 0.00000001);
	failed += test_result("(Transforms) Equ to Ecl latitude",
		ecl.lat, 6.68002727, 0.00000001);

	ln_get_equ_from_ecl(&ecl, JD, &equ);
	failed += test_result ("(Transforms) Ecl to Equ RA ",
		equ.ra, 116.32894167, 0.00000001);
	failed += test_result ("(Transforms) Ecl to Equ DEC",
		equ.dec, 28.02618333, 0.00000001);

	/* Gal pole */
	gal.l = 0.0;
	gal.b = 90.0;
	
	ln_get_equ_from_gal(&gal, &equ);
	failed += test_result("(Transforms) Gal to Equ RA",
		equ.ra, 192.25, 0.00000001);
	failed += test_result("(Transforms) Gal to Equ DEC",
		equ.dec, 27.4, 0.00000001);

	ln_get_gal_from_equ(&equ, &gal);
	failed += test_result("(Transforms) Equ to Gal b",
		gal.b, 90, 0.00000001);

	// Swift triger 174738
	
	equ.ra = 125.2401;
	equ.dec = +31.9260;

	ln_get_gal_from_equ2000(&equ, &gal);
	failed += test_result("(Transforms) Equ J2000 to Gal l",
		gal.l, 190.54, 0.005);
	failed += test_result("(Transforms) Equ J2000 to Gal b",
		gal.b, 31.92, 0.005);

	return failed;
}    

static int sidereal_test(void)
{
	struct ln_date date;
	double sd;
	double JD;
	int failed = 0;
	
	/* 10/04/1987 19:21:00 */
	date.years = 1987;
	date.months = 4;
	date.days = 10;
	date.hours = 19;
	date.minutes = 21;
	date.seconds = 0.0;

	JD = ln_get_julian_day(&date);
	sd = ln_get_mean_sidereal_time(JD);

	failed += test_result("(Sidereal) mean hours on 10/04/1987 19:21:00 ",
		sd, 8.58252488, 0.000001);
	sd = ln_get_apparent_sidereal_time(JD);
	failed += test_result("(Sidereal) apparent hours on 10/04/1987 19:21:00 ",
		sd, 8.58245327, 0.000001);
	return failed;
}

static int solar_coord_test(void)
{
	struct ln_helio_posn pos;
	int failed = 0;
	
	ln_get_solar_geom_coords(2448908.5, &pos);
	failed += test_result("(Solar Coords) longitude (deg) on JD 2448908.5  ",
		pos.L, 200.00810889, 0.00000001);
	failed += test_result("(Solar Coords) latitude (deg) on JD 2448908.5  ",
		pos.B, 0.00018690, 0.00000001);
	failed += test_result("(Solar Coords) radius vector (AU) on JD 2448908.5  ",
		pos.R, 0.99760852, 0.00000001);
	return failed;
}

static int aberration_test(void)
{
	struct lnh_equ_posn hobject;
	struct ln_equ_posn object, pos;
	struct ln_date date;
	double JD;
	int failed = 0;
	
	/* object position */
	hobject.ra.hours = 2;
	hobject.ra.minutes = 44;
	hobject.ra.seconds = 12.9747;
	hobject.dec.neg = 0;
	hobject.dec.degrees = 49;
	hobject.dec.minutes = 13;
	hobject.dec.seconds = 39.896;

	/* date */
	date.years = 2028;
	date.months = 11;
	date.days = 13;
	date.hours = 4;
	date.minutes = 31;
	date.seconds = 0;

	JD = ln_get_julian_day(&date);

	ln_hequ_to_equ(&hobject, &object);
	ln_get_equ_aber(&object, JD, &pos);
	failed += test_result("(Aberration) RA  ", pos.ra,
		41.06238352, 0.00000001);
	failed += test_result("(Aberration) DEC  ", pos.dec,
		49.22962359, 0.00000001);
	return failed;
}

static int precession_test(void)
{
	double JD;
	struct ln_equ_posn object, pos, pos2, pm;
	struct lnh_equ_posn hobject;
	struct ln_date grb_date;
	int failed = 0;
	
	/* object position */
	hobject.ra.hours = 2;
	hobject.ra.minutes = 44;
	hobject.ra.seconds = 11.986;
	hobject.dec.neg = 0;
	hobject.dec.degrees = 49;
	hobject.dec.minutes = 13;
	hobject.dec.seconds = 42.48;

	JD = 2462088.69;
	ln_hequ_to_equ(&hobject, &object);

	pm.ra = 0.03425 * (15.0 / 3600.0);
	pm.dec = -0.0895 / 3600.0;
	
	ln_get_equ_pm(&object, &pm, JD, &object);

	failed += test_result("(Proper motion) RA on JD 2462088.69  ",
		object.ra, 41.054063, 0.00001);
	failed += test_result("(Proper motion) DEC on JD 2462088.69  ",
		object.dec, 49.227750, 0.00001);

	ln_get_equ_prec(&object, JD, &pos);
	failed += test_result("(Precession) RA on JD 2462088.69  ",
		pos.ra, 41.547212, 0.00003);
	failed += test_result("(Precession) DEC on JD 2462088.69  ",
		pos.dec, 49.348483, 0.00001);

	ln_get_equ_prec2(&object, JD2000, JD, &pos);

	failed += test_result("(Precession 2) RA on JD 2462088.69  ",
		pos.ra, 41.547212, 0.00001);
	failed += test_result("(Precession 2) DEC on JD 2462088.69  ",
		pos.dec, 49.348483, 0.00001);

	ln_get_equ_prec2(&pos, JD, JD2000, &pos2);

	failed += test_result("(Precession 2) RA on JD 2451545.0  ",
		pos2.ra, object.ra, 0.00001);
	failed += test_result("(Precession 2) DEC on JD 2451545.0  ",
		pos2.dec, object.dec, 0.00001);

	// INTEGRAL GRB050922A coordinates lead to RA not in <0-360> range
	pos.ra = 271.2473;
	pos.dec = -32.0227;

	grb_date.years = 2005;
	grb_date.months = 9;
	grb_date.days = 22;
	grb_date.hours = 13;
	grb_date.minutes = 43;
	grb_date.seconds = 18.0;

	JD = ln_get_julian_day(&grb_date);

	ln_get_equ_prec2 (&pos, JD, JD2000, &pos2);

	failed += test_result("(Precession 2) RA on JD 2451545.0  ",
		pos2.ra, 271.1541, 0.0002);
	failed += test_result("(Precession 2) DEC on JD 2451545.0  ",
		pos2.dec, -32.0235, 0.0002);

	// second test from AA, p. 128
	hobject.ra.hours = 2;
	hobject.ra.minutes = 31;
	hobject.ra.seconds = 48.704;
	hobject.dec.neg = 0;
	hobject.dec.degrees = 89;
	hobject.dec.minutes = 15;
	hobject.dec.seconds = 50.72;

	ln_hequ_to_equ (&hobject, &object);

	// proper motions
	pm.ra = ((long double) 0.19877) * (15.0 / 3600.0);
	pm.dec = ((long double) -0.0152) / 3600.0;

	ln_get_equ_pm(&object, &pm, B1900, &pos);
	
	ln_get_equ_prec2(&pos, JD2000, B1900, &pos2);

	// the position is so close to pole, that it depends a lot on how precise
	// functions we will use. So we get such big errors compared to Meeus.
	// I checked results agains SLAlib on-line calculator and SLAlib performs
	// even worse then we
	
	failed += test_result ("(Precession 2) RA on B1900  ", pos2.ra,
		20.6412499980, 0.002);
	failed += test_result ("(Precession 2) DEC on B1900  ", pos2.dec,
		88.7739388888, 0.0001);

	ln_get_equ_pm(&object, &pm, JD2050, &pos);

	ln_get_equ_prec2(&pos, JD2000, JD2050, &pos2);

	failed += test_result("(Precession 2) RA on J2050  ",
		pos2.ra, 57.0684583320, 0.003);
	failed += test_result("(Precession 2) DEC on J2050  ",
		pos2.dec, 89.4542722222, 0.0001);

	return failed;
}

static int apparent_position_test(void)
{
	double JD;
	struct lnh_equ_posn hobject, hpm;
	struct ln_equ_posn object, pm, pos;	
	int failed = 0;
	
	/* objects position */
	hobject.ra.hours = 2;
	hobject.ra.minutes = 44;
	hobject.ra.seconds = 12.9747;
	hobject.dec.neg = 0;
	hobject.dec.degrees = 49;
	hobject.dec.minutes = 13;
	hobject.dec.seconds = 39.896;

	/* proper motion of object */
	hpm.ra.hours = 0;
	hpm.ra.minutes = 0;
	hpm.ra.seconds = 0.03425;
	hpm.dec.neg = 1;
	hpm.dec.degrees = 0;
	hpm.dec.minutes = 0;
	hpm.dec.seconds = 0.0895;
    
	JD = 2462088.69;
	ln_hequ_to_equ(&hobject, &object);
	ln_hequ_to_equ(&hpm, &pm);
	ln_get_apparent_posn(&object, &pm, JD, &pos);

	failed += test_result("(Apparent Position) RA on JD 2462088.69  ",
		pos.ra, 41.56406641, 0.00000001);
	failed += test_result("(Apparent Position) DEC on JD 2462088.69  ",
		pos.dec, 49.35135029, 0.0000001);
	return failed;
}

static int vsop87_test(void)
{
	struct ln_helio_posn pos;
	struct lnh_equ_posn hequ;
	struct ln_equ_posn equ;
	double JD = 2448976.5;
	double au;
	int failed = 0;
	
#ifdef DATE
	struct ln_date date;
	date.years = 2003;
	date.months = 1;
	date.days = 29; 
	date.hours = 0;
	date.minutes = 0;
	date.seconds = 0;

	JD = ln_get_julian_day(&date);
#endif
#ifdef SYS_TIME
	JD = ln_get_julian_from_sys();	
#endif
	
	ln_get_solar_equ_coords(JD, &equ);
	failed += test_result("(Solar Position) RA on JD 2448976.5  ",
		equ.ra, 268.32146893, 0.00000001);
	failed += test_result("(Solar Position) DEC on JD 2448976.5  ",
		equ.dec, -23.43026873, 0.00000001);
	
	ln_get_mercury_helio_coords(JD, &pos);
	printf("Mercury L %f B %f R %f\n", pos.L, pos.B, pos.R);
	ln_get_mercury_equ_coords(JD, &equ);
	ln_equ_to_hequ (&equ, &hequ);
	printf("Mercury RA %d:%d:%f Dec %d:%d:%f\n", hequ.ra.hours, hequ.ra.minutes, hequ.ra.seconds, hequ.dec.degrees, hequ.dec.minutes, hequ.dec.seconds);
	au = ln_get_mercury_earth_dist(JD);
	fprintf(stdout, "mercury -> Earth dist (AU) %f\n",au);
	au = ln_get_mercury_solar_dist(JD);
	fprintf(stdout, "mercury -> Sun dist (AU) %f\n",au);
	au = ln_get_mercury_disk(JD);
	fprintf(stdout, "mercury -> illuminated disk %f\n",au);
	au = ln_get_mercury_magnitude(JD);
	fprintf(stdout, "mercury -> magnitude %f\n",au);
	au = ln_get_mercury_phase(JD);
	fprintf(stdout, "mercury -> phase %f\n",au);
	au = ln_get_mercury_sdiam(JD);
	fprintf(stdout, "mercury -> sdiam %f\n",au);
	
	ln_get_venus_helio_coords(JD, &pos);
	printf("Venus L %f B %f R %f\n", pos.L, pos.B, pos.R);
	ln_get_venus_equ_coords(JD, &equ);
	ln_equ_to_hequ (&equ, &hequ);
	printf("Venus RA %d:%d:%f Dec %d:%d:%f\n", hequ.ra.hours, hequ.ra.minutes, hequ.ra.seconds, hequ.dec.degrees, hequ.dec.minutes, hequ.dec.seconds);
	au = ln_get_venus_earth_dist(JD);
	fprintf(stdout, "venus -> Earth dist (AU) %f\n",au);
	au = ln_get_venus_solar_dist(JD);
	fprintf(stdout, "venus -> Sun dist (AU) %f\n",au);
	au = ln_get_venus_disk(JD);
	fprintf(stdout, "venus -> illuminated disk %f\n",au);
	au = ln_get_venus_magnitude(JD);
	fprintf(stdout, "venus -> magnitude %f\n",au);
	au = ln_get_venus_phase(JD);
	fprintf(stdout, "venus -> phase %f\n",au);
	au = ln_get_venus_sdiam(JD);
	fprintf(stdout, "venus -> sdiam %f\n",au);
	
	ln_get_earth_helio_coords(JD, &pos);
	printf("Earth L %f B %f R %f\n", pos.L, pos.B, pos.R);
	au = ln_get_earth_solar_dist(JD);
	fprintf(stdout, "earth -> Sun dist (AU) %f\n",au);

	ln_get_mars_helio_coords(JD, &pos);	
	printf("Mars L %f B %f R %f\n", pos.L, pos.B, pos.R);
	ln_get_mars_equ_coords(JD, &equ);
	ln_equ_to_hequ (&equ, &hequ);
	printf("Mars RA %d:%d:%f Dec %d:%d:%f\n", hequ.ra.hours, hequ.ra.minutes, hequ.ra.seconds, hequ.dec.degrees, hequ.dec.minutes, hequ.dec.seconds);
	au = ln_get_mars_earth_dist(JD);
	fprintf(stdout, "mars -> Earth dist (AU) %f\n",au);
	au = ln_get_mars_solar_dist(JD);
	fprintf(stdout, "mars -> Sun dist (AU) %f\n",au);
	au = ln_get_mars_disk(JD);
	fprintf(stdout, "mars -> illuminated disk %f\n",au);
	au = ln_get_mars_magnitude(JD);
	fprintf(stdout, "mars -> magnitude %f\n",au);
	au = ln_get_mars_phase(JD);
	fprintf(stdout, "mars -> phase %f\n",au);
	au = ln_get_mars_sdiam(JD);
	fprintf(stdout, "mars -> sdiam %f\n",au);
	
	ln_get_jupiter_helio_coords(JD, &pos);
	printf("Jupiter L %f B %f R %f\n", pos.L, pos.B, pos.R);
	ln_get_jupiter_equ_coords(JD, &equ);
	ln_equ_to_hequ (&equ, &hequ);
	printf("Jupiter RA %d:%d:%f Dec %d:%d:%f\n", hequ.ra.hours, hequ.ra.minutes, hequ.ra.seconds, hequ.dec.degrees, hequ.dec.minutes, hequ.dec.seconds);
	au = ln_get_jupiter_earth_dist(JD);
	fprintf(stdout, "jupiter -> Earth dist (AU) %f\n",au);
	au = ln_get_jupiter_solar_dist(JD);
	fprintf(stdout, "jupiter -> Sun dist (AU) %f\n",au);
	au = ln_get_jupiter_disk(JD);
	fprintf(stdout, "jupiter -> illuminated disk %f\n",au);
	au = ln_get_jupiter_magnitude(JD);
	fprintf(stdout, "jupiter -> magnitude %f\n",au);
	au = ln_get_jupiter_phase(JD);
	fprintf(stdout, "jupiter -> phase %f\n",au);
	au = ln_get_jupiter_pol_sdiam(JD);
	fprintf(stdout, "jupiter -> polar sdiam %f\n",au);
	au = ln_get_jupiter_equ_sdiam(JD);
	fprintf(stdout, "jupiter -> equ sdiam %f\n",au);
	
	ln_get_saturn_helio_coords(JD, &pos);
	printf("Saturn L %f B %f R %f\n", pos.L, pos.B, pos.R);
	ln_get_saturn_equ_coords(JD, &equ);
	ln_equ_to_hequ (&equ, &hequ);
	printf("Saturn RA %d:%d:%f Dec %d:%d:%f\n", hequ.ra.hours, hequ.ra.minutes, hequ.ra.seconds, hequ.dec.degrees, hequ.dec.minutes, hequ.dec.seconds);
	au = ln_get_saturn_earth_dist(JD);
	fprintf(stdout, "saturn -> Earth dist (AU) %f\n",au);
	au = ln_get_saturn_solar_dist(JD);
	fprintf(stdout, "saturn -> Sun dist (AU) %f\n",au);
	au = ln_get_saturn_disk(JD);
	fprintf(stdout, "saturn -> illuminated disk %f\n",au);
	au = ln_get_saturn_magnitude(JD);
	fprintf(stdout, "saturn -> magnitude %f\n",au);
	au = ln_get_saturn_phase(JD);
	fprintf(stdout, "saturn -> phase %f\n",au);
	au = ln_get_saturn_pol_sdiam(JD);
	fprintf(stdout, "saturn -> polar sdiam %f\n",au);
	au = ln_get_saturn_equ_sdiam(JD);
	fprintf(stdout, "saturn -> equ sdiam %f\n",au);
	
	ln_get_uranus_helio_coords(JD, &pos);	
	printf("Uranus L %f B %f R %f\n", pos.L, pos.B, pos.R);
	ln_get_uranus_equ_coords(JD, &equ);
	ln_equ_to_hequ (&equ, &hequ);
	printf("Uranus RA %d:%d:%f Dec %d:%d:%f\n", hequ.ra.hours, hequ.ra.minutes, hequ.ra.seconds, hequ.dec.degrees, hequ.dec.minutes, hequ.dec.seconds);
	au = ln_get_uranus_earth_dist(JD);
	fprintf(stdout, "uranus -> Earth dist (AU) %f\n",au);
	au = ln_get_uranus_solar_dist(JD);
	fprintf(stdout, "uranus -> Sun dist (AU) %f\n",au);
	au = ln_get_uranus_disk(JD);
	fprintf(stdout, "uranus -> illuminated disk %f\n",au);
	au = ln_get_uranus_magnitude(JD);
	fprintf(stdout, "uranus -> magnitude %f\n",au);
	au = ln_get_uranus_phase(JD);
	fprintf(stdout, "uranus -> phase %f\n",au);
	au = ln_get_uranus_sdiam(JD);
	fprintf(stdout, "uranus -> sdiam %f\n",au);
	
	ln_get_neptune_helio_coords(JD, &pos);
	printf("Neptune L %f B %f R %f\n", pos.L, pos.B, pos.R);
	ln_get_neptune_equ_coords(JD, &equ);
	ln_equ_to_hequ (&equ, &hequ);
	printf("Neptune RA %d:%d:%f Dec %d:%d:%f\n", hequ.ra.hours, hequ.ra.minutes, hequ.ra.seconds, hequ.dec.degrees, hequ.dec.minutes, hequ.dec.seconds);
	au = ln_get_neptune_earth_dist(JD);
	fprintf(stdout, "neptune -> Earth dist (AU) %f\n",au);
	au = ln_get_neptune_solar_dist(JD);
	fprintf(stdout, "neptune -> Sun dist (AU) %f\n",au);
	au = ln_get_neptune_disk(JD);
	fprintf(stdout, "neptune -> illuminated disk %f\n",au);
	au = ln_get_neptune_magnitude(JD);
	fprintf(stdout, "neptune -> magnitude %f\n",au);
	au = ln_get_neptune_phase(JD);
	fprintf(stdout, "neptune -> phase %f\n",au);
	au = ln_get_neptune_sdiam(JD);
	fprintf(stdout, "neptune -> sdiam %f\n",au);
	
	ln_get_pluto_helio_coords(JD, &pos);
	printf("Pluto L %f B %f R %f\n", pos.L, pos.B, pos.R);
	ln_get_pluto_equ_coords(JD, &equ);
	ln_equ_to_hequ (&equ, &hequ);
	printf("Pluto RA %d:%d:%f Dec %d:%d:%f\n", hequ.ra.hours, hequ.ra.minutes, hequ.ra.seconds, hequ.dec.degrees, hequ.dec.minutes, hequ.dec.seconds);
	au = ln_get_pluto_earth_dist(JD);
	fprintf(stdout, "pluto -> Earth dist (AU) %f\n",au);
	au = ln_get_pluto_solar_dist(JD);
	fprintf(stdout, "pluto -> Sun dist (AU) %f\n",au);
	au = ln_get_pluto_disk(JD);
	fprintf(stdout, "pluto -> illuminated disk %f\n",au);
	au = ln_get_pluto_magnitude(JD);
	fprintf(stdout, "pluto -> magnitude %f\n",au);
	au = ln_get_pluto_phase(JD);
	fprintf(stdout, "pluto -> phase %f\n",au);
	au = ln_get_pluto_sdiam(JD);
	fprintf(stdout, "pluto -> sdiam %f\n",au);
	
	return failed;
}

int lunar_test ()
{
	double JD = 2448724.5;
	
	struct ln_rect_posn moon;
	struct ln_equ_posn equ;
	struct ln_lnlat_posn ecl;
	int failed = 0;
	
	/*	JD = get_julian_from_sys();*/
	/*JD=2448724.5;*/
	ln_get_lunar_geo_posn(JD, &moon, 0);
	fprintf(stdout, "lunar x %f  y %f  z %f\n",moon.X, moon.Y, moon.Z);
	ln_get_lunar_ecl_coords(JD, &ecl, 0);
	fprintf(stdout, "lunar long %f  lat %f\n",ecl.lng, ecl.lat);
	ln_get_lunar_equ_coords_prec(JD, &equ, 0);
	fprintf(stdout, "lunar RA %f  Dec %f\n",equ.ra, equ.dec);
	fprintf(stdout, "lunar distance km %f\n", ln_get_lunar_earth_dist(JD));
	fprintf(stdout, "lunar disk %f\n", ln_get_lunar_disk(JD));
	fprintf(stdout, "lunar phase %f\n", ln_get_lunar_phase(JD));
	fprintf(stdout, "lunar bright limb %f\n", ln_get_lunar_bright_limb(JD));
	return failed;
}

int elliptic_motion_test ()
{
	double r,v,l,V,dist;
	double E, e_JD, o_JD;
	struct ln_ell_orbit orbit;
	struct ln_rect_posn posn;
	struct ln_date epoch_date, obs_date;
	struct ln_equ_posn equ_posn;
	int failed = 0;
		
	obs_date.years = 1990;
	obs_date.months = 10;
	obs_date.days = 6;
	obs_date.hours = 0;
	obs_date.minutes = 0;
	obs_date.seconds = 0;
		
	epoch_date.years = 1990;
	epoch_date.months = 10;
	epoch_date.days = 28;
	epoch_date.hours = 12;
	epoch_date.minutes = 30;
	epoch_date.seconds = 0;
	
	e_JD = ln_get_julian_day (&epoch_date);
	o_JD = ln_get_julian_day (&obs_date);
	
	orbit.JD = e_JD;
	orbit.a = 2.2091404;
	orbit.e = 0.8502196;
	orbit.i = 11.94525;
	orbit.omega = 334.75006;
	orbit.w = 186.23352;
	orbit.n = 0;
	
	E = ln_solve_kepler (0.1, 5.0);
	failed += test_result ("(Equation of kepler) E when e is 0.1 and M is 5.0   ", E, 5.554589253872320, 0.000000000001);
	
	v = ln_get_ell_true_anomaly (0.1, E);
	failed += test_result ("(True Anomaly) v when e is 0.1 and E is 5.5545   ", v, 6.13976152, 0.00000001);
	
	r = ln_get_ell_radius_vector (0.5, 0.1, E);
	failed += test_result ("(Radius Vector) r when v is , e is 0.1 and E is 5.5545   ", r, 0.45023478, 0.00000001);
	
	ln_get_ell_geo_rect_posn (&orbit, o_JD, &posn);
	failed += test_result ("(Geocentric Rect Coords X) for comet Enckle   ", posn.X, 0.72549850, 0.00000001);
	failed += test_result ("(Geocentric Rect Coords Y) for comet Enckle   ", posn.Y, -0.28443537, 0.00000001);
	failed += test_result ("(Geocentric Rect Coords Z) for comet Enckle   ", posn.Z, -0.27031656, 0.00000001);
	
	ln_get_ell_helio_rect_posn (&orbit, o_JD, &posn);
	failed += test_result ("(Heliocentric Rect Coords X) for comet Enckle   ", posn.X, 0.25017473, 0.00000001);
	failed += test_result ("(Heliocentric Rect Coords Y) for comet Enckle   ", posn.Y, 0.48476422, 0.00000001);
	failed += test_result ("(Heliocentric Rect Coords Z) for comet Enckle   ", posn.Z, 0.35716517, 0.00000001);
	
	ln_get_ell_body_equ_coords (o_JD, &orbit, &equ_posn);
	failed += test_result ("(RA) for comet Enckle   ", equ_posn.ra, 158.58242653, 0.00000001);
	failed += test_result ("(Dec) for comet Enckle   ", equ_posn.dec, 19.13924815, 0.00000001);
	
	l = ln_get_ell_orbit_len (&orbit);
	failed += test_result ("(Orbit Length) for comet Enckle in AU   ", l, 10.85028112, 0.00000001);
	
	V = ln_get_ell_orbit_pvel (&orbit);
	failed += test_result ("(Orbit Perihelion Vel) for comet Enckle in kms   ", V, 70.43130198, 0.00000001);
	
	V = ln_get_ell_orbit_avel (&orbit);
	failed += test_result ("(Orbit Aphelion Vel) for comet Enckle in kms   ", V, 5.70160892, 0.00000001);
	
	V = ln_get_ell_orbit_vel (o_JD, &orbit);
	failed += test_result ("(Orbit Vel JD) for comet Enckle in kms   ", V, 48.16148331, 0.00000001);
	
	dist = ln_get_ell_body_solar_dist (o_JD, &orbit);
	failed += test_result ("(Body Solar Dist) for comet Enckle in AU   ", dist, 0.65203581, 0.00000001);
	
	dist = ln_get_ell_body_earth_dist (o_JD, &orbit);
	failed += test_result ("(Body Earth Dist) for comet Enckle in AU   ", dist, 0.82481670, 0.00000001);

	// TNO http://www.cfa.harvard.edu/mpec/K05/K05O42.html

	obs_date.years = 2006;
	obs_date.months = 5;
	obs_date.days = 5;
	obs_date.hours = 0;
	obs_date.minutes = 0;
	obs_date.seconds = 0;
		
	epoch_date.years = 2006;
	epoch_date.months = 3;
	epoch_date.days = 6;
	epoch_date.hours = 0;
	epoch_date.minutes = 0;
	epoch_date.seconds = 0;
	
	e_JD = ln_get_julian_day (&epoch_date);
	o_JD = ln_get_julian_day (&obs_date);
	
	orbit.JD = e_JD;
	orbit.a = 45.7082927;
	orbit.e = 0.1550125;
	orbit.i = 28.99870;
	orbit.omega = 79.55499;
	orbit.w = 296.40937;
	orbit.n = 0.00318942;

	// MPO refers to Mean anomaly & epoch, we hence need to convert epoch
	// to perihelion pass

	orbit.JD -= 147.09926 / orbit.n;

	ln_get_ell_body_equ_coords (o_JD, &orbit, &equ_posn);
	failed += test_result ("(RA) for TNO K05F09Y   ", equ_posn.ra, 184.3699999995, 0.001);
	failed += test_result ("(Dec) for TNO K05F09Y  ", equ_posn.dec, 30.3316666666, 0.001);

	return failed;
}

/* need a proper parabolic orbit to properly test */
int parabolic_motion_test ()
{ 
	double r,v,dist;
	double e_JD, o_JD;
	struct ln_par_orbit orbit;
	struct ln_rect_posn posn;
	struct ln_date epoch_date, obs_date;
	struct ln_equ_posn equ_posn;
	int failed = 0;
		
	obs_date.years = 2003;
	obs_date.months = 1;
	obs_date.days = 11;
	obs_date.hours = 0;
	obs_date.minutes = 0;
	obs_date.seconds = 0;
		
	epoch_date.years = 2003;
	epoch_date.months = 1;
	epoch_date.days = 29;
	epoch_date.hours = 0;
	epoch_date.minutes = 6;
	epoch_date.seconds = 37.44;
	
	e_JD = ln_get_julian_day (&epoch_date);
	o_JD = ln_get_julian_day (&obs_date);
	
	orbit.q = 0.190082; 
	orbit.i = 94.1511;
	orbit.w = 187.5613; 
	orbit.omega = 119.0676; 
	orbit.JD = e_JD;
	
	v = ln_get_par_true_anomaly (orbit.q, o_JD - e_JD);
	failed += test_result ("(True Anomaly) v when e is 0.1 and E is 5.5545   ", v, 247.18968605, 0.00000001);
	
	r = ln_get_par_radius_vector (orbit.q, o_JD - e_JD);
	failed += test_result ("(Radius Vector) r when v is , e is 0.1 and E is 5.5545   ", r, 0.62085992, 0.00000001);
	
	ln_get_par_geo_rect_posn (&orbit, o_JD, &posn);
	failed += test_result ("(Geocentric Rect Coords X) for comet C/2002 X5 (Kudo-Fujikawa)   ", posn.X, 0.29972461, 0.00000001);
	failed += test_result ("(Geocentric Rect Coords Y) for comet C/2002 X5 (Kudo-Fujikawa)   ", posn.Y, -0.93359772, 0.00000001);
	failed += test_result ("(Geocentric Rect Coords Z) for comet C/2002 X5 (Kudo-Fujikawa)   ", posn.Z, 0.24639194, 0.00000001);
	
	ln_get_par_helio_rect_posn (&orbit, o_JD, &posn);
	failed += test_result ("(Heliocentric Rect Coords X) for comet C/2002 X5 (Kudo-Fujikawa)   ", posn.X, -0.04143700, 0.00000001);
	failed += test_result ("(Heliocentric Rect Coords Y) for comet C/2002 X5 (Kudo-Fujikawa)   ", posn.Y, -0.08736588, 0.00000001);
	failed += test_result ("(Heliocentric Rect Coords Z) for comet C/2002 X5 (Kudo-Fujikawa)   ", posn.Z, 0.61328397, 0.00000001);
	
	ln_get_par_body_equ_coords (o_JD, &orbit, &equ_posn);
	failed += test_result ("(RA) for comet C/2002 X5 (Kudo-Fujikawa)   ", equ_posn.ra, 287.79617309, 0.00000001);
	failed += test_result ("(Dec) for comet C/2002 X5 (Kudo-Fujikawa)   ", equ_posn.dec, 14.11800859, 0.00000001);
	
	dist = ln_get_par_body_solar_dist (o_JD, &orbit);
	failed += test_result ("(Body Solar Dist) for comet C/2002 X5 (Kudo-Fujikawa) in AU   ", dist, 0.62085992, 0.00001);
	
	dist = ln_get_par_body_earth_dist (o_JD, &orbit);
	failed += test_result ("(Body Earth Dist) for comet C/2002 X5 (Kudo-Fujikawa) in AU   ", dist, 1.01101362, 0.00001);
	return failed;
}

/* data from Meeus, chapter 35 */
int hyperbolic_motion_test ()
{ 
	double r,v,dist;
	double e_JD, o_JD;
	struct ln_hyp_orbit orbit;
	struct ln_date epoch_date, obs_date;
	struct ln_equ_posn equ_posn;
	int failed = 0;
		
	orbit.q = 3.363943; 
	orbit.e = 1.05731;

	// the one from Meeus..
	v = ln_get_hyp_true_anomaly (orbit.q, orbit.e, 1237.1);
	failed += test_result ("(True Anomaly) v when q is 3.363943 and e is 1.05731   ", v, 109.40598, 0.00001);
	
	r = ln_get_hyp_radius_vector (orbit.q, orbit.e, 1237.1);
	failed += test_result ("(Radius Vector) r when q is 3.363943 and e is 1.05731  ", r, 10.668551, 0.00001);

	// and now something real.. C/2001 Q4 (NEAT)
	obs_date.years = 2004;
	obs_date.months = 5;
	obs_date.days = 15;
	obs_date.hours = 0;
	obs_date.minutes = 0;
	obs_date.seconds = 0;
		
	epoch_date.years = 2004;
	epoch_date.months = 5;
	epoch_date.days = 15;
	epoch_date.hours = 23;
	epoch_date.minutes = 12;
	epoch_date.seconds = 37.44;
	
	e_JD = ln_get_julian_day (&epoch_date);
	o_JD = ln_get_julian_day (&obs_date);

	orbit.q = 0.961957;
	orbit.e = 1.000744;
	orbit.i = 99.6426;
	orbit.w = 1.2065;
	orbit.omega = 210.2785;
	orbit.JD = e_JD;

	r = ln_get_hyp_radius_vector (orbit.q, orbit.e, o_JD - e_JD);
	failed += test_result ("(Radius Vector) r for C/2001 Q4 (NEAT)   ", r, 0.962, 0.001);

	ln_get_hyp_body_equ_coords (o_JD, &orbit, &equ_posn);
	failed += test_result ("(RA) for comet C/2001 Q4 (NEAT)   ", equ_posn.ra, 128.01, 0.01);
	failed += test_result ("(Dec) for comet C/2001 Q4 (NEAT)   ", equ_posn.dec, 18.3266666666, 0.03);

	dist = ln_get_hyp_body_solar_dist (o_JD, &orbit);
	failed += test_result ("(Body Solar Dist) for comet C/2001 Q4 (NEAT) in AU   ", dist, 0.962, 0.001);

	obs_date.years = 2005;
	obs_date.months = 1;

	o_JD = ln_get_julian_day (&obs_date);

	r = ln_get_hyp_radius_vector (orbit.q, orbit.e, o_JD - e_JD);
	failed += test_result ("(Radius Vector) r for C/2001 Q4 (NEAT)   ", r, 3.581, 0.001);

	ln_get_hyp_body_equ_coords (o_JD, &orbit, &equ_posn);
	failed += test_result ("(RA) for comet C/2001 Q4 (NEAT)   ", equ_posn.ra, 332.9025, 0.01);
	failed += test_result ("(Dec) for comet C/2001 Q4 (NEAT)   ", equ_posn.dec, 58.6116666666, 0.001);
	
	return failed;
}


int rst_test ()
{
	struct ln_lnlat_posn observer;
	struct ln_rst_time rst;
	struct ln_hms hms;
	struct ln_dms dms;
	struct ln_date date;
	struct ln_equ_posn object;
	double JD, JD_next;
	int ret;
	int failed = 0;

	// Arcturus
	hms.hours = 14;
	hms.minutes = 15;
	hms.seconds = 39.67;

	dms.neg = 0;
	dms.degrees = 19;
	dms.minutes = 10;
	dms.seconds = 56.7;

	object.ra = ln_hms_to_deg (&hms);
	object.dec = ln_dms_to_deg (&dms);

	date.years = 2006;
	date.months = 1;
	date.days = 17;

	date.hours = 0;
	date.minutes = 0;
	date.seconds = 0;

	JD = ln_get_julian_day (&date);

	observer.lng = 15;
	observer.lat = 51;

	ret = ln_get_object_rst(JD, &observer, &object, &rst);
	failed += test_result ("Arcturus sometimes rise at 15 E, 51 N", ret, 0, 0);

	if (!ret)
	{
		ln_get_date (rst.rise, &date);
		failed += test_result ("Arcturus rise hour on 2006/01/17 at (15 E,51 N)", date.hours, 21, 0);
		failed += test_result ("Arcturus rise minute on 2006/01/17 at (15 E,51 N)", date.minutes, 40, 0);

		ln_get_date (rst.transit, &date);
		failed += test_result ("Arcturus transit hour on 2006/01/17 at (15 E,51 N)", date.hours, 5, 0);
		failed += test_result ("Arcturus transit minute on 2006/01/17 at (15 E,51 N)", date.minutes, 29, 0);

		ln_get_date (rst.set, &date);
		failed += test_result ("Arcturus set hour on 2006/01/17 at (15 E,51 N)", date.hours, 13, 0);
		failed += test_result ("Arcturus set minute on 2006/01/17 at (15 E,51 N)", date.minutes, 14, 0);
	}

	JD_next = rst.transit - 0.001;
	ret = ln_get_object_next_rst(JD_next, &observer, &object, &rst);
	failed += test_result ("Arcturus sometimes rise at 15 E, 51 N", ret, 0, 0);

	if (!ret)
	{
		failed += test_result ("Arcturus next date is less then transit time",(JD_next < rst.transit), 1, 0);
		failed += test_result ("Arcturus next transit time is less then set time", (rst.transit < rst.set), 1, 0);
		failed += test_result ("Arcturus next set time is less then rise time", (rst.set < rst.rise), 1, 0);

		ln_get_date (rst.rise, &date);
		failed += test_result ("Arcturus next rise hour on 2006/01/17 at (15 E,51 N)", date.hours, 21, 0);
		failed += test_result ("Arcturus next rise minute on 2006/01/17 at (15 E,51 N)", date.minutes, 40, 0);

		ln_get_date (rst.transit, &date);
		failed += test_result ("Arcturus next transit hour on 2006/01/17 at (15 E,51 N)", date.hours, 5, 0);
		failed += test_result ("Arcturus next transit minute on 2006/01/17 at (15 E,51 N)", date.minutes, 29, 0);

		ln_get_date (rst.set, &date);
		failed += test_result ("Arcturus next set hour on 2006/01/17 at (15 E,51 N)", date.hours, 13, 0);
		failed += test_result ("Arcturus next set minute on 2006/01/17 at (15 E,51 N)", date.minutes, 14, 0);
	}

	JD_next = rst.set - 0.001;
	ret = ln_get_object_next_rst(JD_next, &observer, &object, &rst);
	failed += test_result ("Arcturus sometimes rise at 15 E, 51 N", ret, 0, 0);

	if (!ret)
	{
		failed += test_result ("Arcturus next date is less then set time",(JD_next < rst.set), 1, 0);
		failed += test_result ("Arcturus next set time is less then rise time", (rst.set < rst.rise), 1, 0);
		failed += test_result ("Arcturus next rise time is less then transit time", (rst.rise < rst.transit), 1, 0);

		ln_get_date (rst.rise, &date);
		failed += test_result ("Arcturus next rise hour on 2006/01/17 at (15 E,51 N)", date.hours, 21, 0);
		failed += test_result ("Arcturus next rise minute on 2006/01/17 at (15 E,51 N)", date.minutes, 40, 0);

		ln_get_date (rst.transit, &date);
		failed += test_result ("Arcturus next transit hour on 2006/01/18 at (15 E,51 N)", date.hours, 5, 0);
		failed += test_result ("Arcturus next transit minute on 2006/01/18 at (15 E,51 N)", date.minutes, 25, 0);

		ln_get_date (rst.set, &date);
		failed += test_result ("Arcturus next set hour on 2006/01/17 at (15 E,51 N)", date.hours, 13, 0);
		failed += test_result ("Arcturus next set minute on 2006/01/17 at (15 E,51 N)", date.minutes, 14, 0);
	}

	JD_next = rst.rise - 0.001;
	ret = ln_get_object_next_rst(JD_next, &observer, &object, &rst);
	failed += test_result ("Arcturus sometimes rise at 15 E, 51 N", ret, 0, 0);

	if (!ret)
	{
		failed += test_result ("Arcturus next date is less then rise time",(JD_next < rst.rise), 1, 0);
		failed += test_result ("Arcturus next rise time is less then transit time", (rst.rise < rst.transit), 1, 0);
		failed += test_result ("Arcturus next transit time is less then set time", (rst.transit < rst.set), 1, 0);

		ln_get_date (rst.rise, &date);
		failed += test_result ("Arcturus next rise hour on 2006/01/17 at (15 E,51 N)", date.hours, 21, 0);
		failed += test_result ("Arcturus next rise minute on 2006/01/17 at (15 E,51 N)", date.minutes, 40, 0);

		ln_get_date (rst.transit, &date);
		failed += test_result ("Arcturus next transit hour on 2006/01/18 at (15 E,51 N)", date.hours, 5, 0);
		failed += test_result ("Arcturus next transit minute on 2006/01/18 at (15 E,51 N)", date.minutes, 25, 0);

		ln_get_date (rst.set, &date);
		failed += test_result ("Arcturus next set hour on 2006/01/18 at (15 E,51 N)", date.hours, 13, 0);
		failed += test_result ("Arcturus next set minute on 2006/01/18 at (15 E,51 N)", date.minutes, 10, 0);
	}

	JD_next = rst.rise + 0.001;
	ret = ln_get_object_next_rst(JD_next, &observer, &object, &rst);
	failed += test_result ("Arcturus sometimes rise at 15 E, 51 N", ret, 0, 0);

	if (!ret)
	{
		failed += test_result ("Arcturus next date is less then transit time",(JD_next < rst.transit), 1, 0);
		failed += test_result ("Arcturus next transit time is less then set time", (rst.transit < rst.set), 1, 0);
		failed += test_result ("Arcturus next set time is less then rise time", (rst.set < rst.rise), 1, 0);

		ln_get_date (rst.rise, &date);
		failed += test_result ("Arcturus next rise hour on 2006/01/18 at (15 E,51 N)", date.hours, 21, 0);
		failed += test_result ("Arcturus next rise minute on 2006/01/18 at (15 E,51 N)", date.minutes, 37, 0);

		ln_get_date (rst.transit, &date);
		failed += test_result ("Arcturus next transit hour on 2006/01/18 at (15 E,51 N)", date.hours, 5, 0);
		failed += test_result ("Arcturus next transit minute on 2006/01/18 at (15 E,51 N)", date.minutes, 25, 0);

		ln_get_date (rst.set, &date);
		failed += test_result ("Arcturus next set hour on 2006/01/18 at (15 E,51 N)", date.hours, 13, 0);
		failed += test_result ("Arcturus next set minute on 2006/01/18 at (15 E,51 N)", date.minutes, 10, 0);
	}

	ret = ln_get_object_rst_horizon(JD, &observer, &object, 20, &rst);
	failed += test_result ("Arcturus sometimes rise above 20 deg at 15 E, 51 N", ret, 0, 0);

	if (!ret)
	{
		ln_get_date (rst.rise, &date);
		failed += test_result ("Arcturus rise above 20 deg hour on 2006/01/17 at (15 E,51 N)", date.hours, 0, 0);
		failed += test_result ("Arcturus rise above 20 deg minute on 2006/01/17 at (15 E,51 N)", date.minutes, 6, 0);

		ln_get_date (rst.transit, &date);
		failed += test_result ("Arcturus transit hour on 2006/01/17 at (15 E,51 N)", date.hours, 5, 0);
		failed += test_result ("Arcturus transit minute on 2006/01/17 at (15 E,51 N)", date.minutes, 29, 0);

		ln_get_date (rst.set, &date);
		failed += test_result ("Arcturus set bellow 20 deg hour on 2006/01/17 at (15 E,51 N)", date.hours, 10, 0);
		failed += test_result ("Arcturus set bellow 20 deg minute on 2006/01/17 at (15 E,51 N)", date.minutes, 52, 0);
	}

	JD_next = rst.rise - 0.002;
	ret = ln_get_object_next_rst_horizon(JD_next, &observer, &object, 20, &rst);
	failed += test_result ("Arcturus sometimes rise above 20 deg at 15 E, 51 N", ret, 0, 0);

	if (!ret)
	{
		failed += test_result ("Arcturus next date is less then rise time",(JD_next < rst.rise), 1, 0);
		failed += test_result ("Arcturus next rise time is less then transit time", (rst.rise < rst.transit), 1, 0);
		failed += test_result ("Arcturus next transit time is less then set time", (rst.transit < rst.set), 1, 0);

		ln_get_date (rst.rise, &date);
		failed += test_result ("Arcturus next rise above 20 deg hour on 2006/01/17 at (15 E,51 N)", date.hours, 0, 0);
		failed += test_result ("Arcturus next rise above 20 deg minute on 2006/01/17 at (15 E,51 N)", date.minutes, 6, 0);

		ln_get_date (rst.transit, &date);
		failed += test_result ("Arcturus next transit hour on 2006/01/17 at (15 E,51 N)", date.hours, 5, 0);
		failed += test_result ("Arcturus next transit minute on 2006/01/17 at (15 E,51 N)", date.minutes, 29, 0);

		ln_get_date (rst.set, &date);
		failed += test_result ("Arcturus next set bellow 20 deg hour on 2006/01/17 at (15 E,51 N)", date.hours, 10, 0);
		failed += test_result ("Arcturus next set bellow 20 deg minute on 2006/01/17 at (15 E,51 N)", date.minutes, 52, 0);
	}

	JD_next = rst.transit - 0.001;
	ret = ln_get_object_next_rst_horizon(JD_next, &observer, &object, 20, &rst);
	failed += test_result ("Arcturus sometimes rise above 20 deg at 15 E, 51 N", ret, 0, 0);

	if (!ret)
	{
		failed += test_result ("Arcturus next date is less then transit time",(JD_next < rst.transit), 1, 0);
		failed += test_result ("Arcturus next transit time is less then set time", (rst.transit < rst.set), 1, 0);
		failed += test_result ("Arcturus next set time is less then rise time", (rst.set < rst.rise), 1, 0);

		ln_get_date (rst.rise, &date);
		failed += test_result ("Arcturus next rise above 20 deg hour on 2006/01/18 at (15 E,51 N)", date.hours, 0, 0);
		failed += test_result ("Arcturus next rise above 20 deg minute on 2006/01/18 at (15 E,51 N)", date.minutes, 2, 0);

		ln_get_date (rst.transit, &date);
		failed += test_result ("Arcturus next transit hour on 2006/01/17 at (15 E,51 N)", date.hours, 5, 0);
		failed += test_result ("Arcturus next transit minute on 2006/01/17 at (15 E,51 N)", date.minutes, 29, 0);

		ln_get_date (rst.set, &date);
		failed += test_result ("Arcturus next set bellow 20 deg hour on 2006/01/17 at (15 E,51 N)", date.hours, 10, 0);
		failed += test_result ("Arcturus next set bellow 20 deg minute on 2006/01/17 at (15 E,51 N)", date.minutes, 52, 0);
	}

	JD_next = rst.set - 0.001;
	ret = ln_get_object_next_rst_horizon(JD_next, &observer, &object, 20, &rst);
	failed += test_result ("Arcturus sometimes rise above 20 deg at 15 E, 51 N", ret, 0, 0);

	if (!ret)
	{
		failed += test_result ("Arcturus next date is less then set time",(JD_next < rst.set), 1, 0);
		failed += test_result ("Arcturus next set time is less then rise time", (rst.set < rst.rise), 1, 0);
		failed += test_result ("Arcturus next rise time is less then transit time", (rst.rise < rst.transit), 1, 0);

		ln_get_date (rst.rise, &date);
		failed += test_result ("Arcturus next rise above 20 deg hour on 2006/01/18 at (15 E,51 N)", date.hours, 0, 0);
		failed += test_result ("Arcturus next rise above 20 deg minute on 2006/01/18 at (15 E,51 N)", date.minutes, 2, 0);

		ln_get_date (rst.transit, &date);
		failed += test_result ("Arcturus next transit hour on 2006/01/18 at (15 E,51 N)", date.hours, 5, 0);
		failed += test_result ("Arcturus next transit minute on 2006/01/18 at (15 E,51 N)", date.minutes, 25, 0);

		ln_get_date (rst.set, &date);
		failed += test_result ("Arcturus next set bellow 20 deg hour on 2006/01/17 at (15 E,51 N)", date.hours, 10, 0);
		failed += test_result ("Arcturus next set bellow 20 deg minute on 2006/01/17 at (15 E,51 N)", date.minutes, 52, 0);
	}

	JD_next = rst.set + 0.001;
	ret = ln_get_object_next_rst_horizon(JD_next, &observer, &object, 20, &rst);
	failed += test_result ("Arcturus sometimes rise above 20 deg at 15 E, 51 N", ret, 0, 0);

	if (!ret)
	{
		failed += test_result ("Arcturus next date is less then rise time",(JD_next < rst.rise), 1, 0);
		failed += test_result ("Arcturus next rise time is less then transit time", (rst.rise < rst.transit), 1, 0);
		failed += test_result ("Arcturus next transit time is less then set time", (rst.transit < rst.set), 1, 0);

		ln_get_date (rst.rise, &date);
		failed += test_result ("Arcturus next rise above 20 deg hour on 2006/01/18 at (15 E,51 N)", date.hours, 0, 0);
		failed += test_result ("Arcturus next rise above 20 deg minute on 2006/01/18 at (15 E,51 N)", date.minutes, 2, 0);

		ln_get_date (rst.transit, &date);
		failed += test_result ("Arcturus next transit hour on 2006/01/18 at (15 E,51 N)", date.hours, 5, 0);
		failed += test_result ("Arcturus next transit minute on 2006/01/18 at (15 E,51 N)", date.minutes, 25, 0);

		ln_get_date (rst.set, &date);
		failed += test_result ("Arcturus next set bellow 20 deg hour on 2006/01/18 at (15 E,51 N)", date.hours, 10, 0);
		failed += test_result ("Arcturus next set bellow 20 deg minute on 2006/01/18 at (15 E,51 N)", date.minutes, 48, 0);
	}

	observer.lat = -51;

	ret = ln_get_object_rst(JD, &observer, &object, &rst);
	failed += test_result ("Arcturus sometimes rise at 15 E, 51 S", ret, 0, 0);

	if (!ret)
	{
		ln_get_date (rst.rise, &date);
		failed += test_result ("Arcturus rise hour on 2006/01/17 at (15 E,51 S)", date.hours, 1, 0);
		failed += test_result ("Arcturus rise minute on 2006/01/17 at (15 E,51 S)", date.minutes, 7, 0);

		ln_get_date (rst.transit, &date);
		failed += test_result ("Arcturus transit hour on 2006/01/17 at (15 E,51 S)", date.hours, 5, 0);
		failed += test_result ("Arcturus transit minute on 2006/01/17 at (15 E,51 S)", date.minutes, 29, 0);

		ln_get_date (rst.set, &date);
		failed += test_result ("Arcturus set hour on 2006/01/17 at (15 E,51 S)", date.hours, 9, 0);
		failed += test_result ("Arcturus set minute on 2006/01/17 at (15 E,51 S)", date.minutes, 51, 0);
	}

	ret = ln_get_object_rst_horizon(JD, &observer, &object, -20, &rst);
	failed += test_result ("Arcturus sometimes rise above -20 deg at 15 E, 51 S", ret, 0, 0);

	if (!ret)
	{
		ln_get_date (rst.rise, &date);
		failed += test_result ("Arcturus rise above -20 deg hour on 2006/01/17 at (15 E,51 S)", date.hours, 22, 0);
		failed += test_result ("Arcturus rise above -20 deg minute on 2006/01/17 at (15 E,51 S)", date.minutes, 50, 0);

		ln_get_date (rst.transit, &date);
		failed += test_result ("Arcturus transit hour on 2006/01/17 at (15 E,51 S)", date.hours, 5, 0);
		failed += test_result ("Arcturus transit minute on 2006/01/17 at (15 E,51 S)", date.minutes, 29, 0);

		ln_get_date (rst.set, &date);
		failed += test_result ("Arcturus set bellow -20 deg hour on 2006/01/17 at (15 E,51 S)", date.hours, 12, 0);
		failed += test_result ("Arcturus set bellow -20 deg minute on 2006/01/17 at (15 E,51 S)", date.minutes, 4, 0);
	}
	
	ret = ln_get_solar_rst(JD, &observer, &rst);
	failed += test_result ("Sun sometimes rise on 2006/01/17 at 15 E, 51 S", ret, 0, 0);

	if (!ret)
	{
		ln_get_date (rst.rise, &date);
		failed += test_result ("Sun rise hour on 2006/01/17 at (15 E,51 S)", date.hours, 3, 0);
		failed += test_result ("Sun rise minute on 2006/01/17 at (15 E,51 S)", date.minutes, 11, 0);

		ln_get_date (rst.transit, &date);
		failed += test_result ("Sun transit hour on 2006/01/17 at (15 E,51 S)", date.hours, 11, 0);
		failed += test_result ("Sun transit minute on 2006/01/17 at (15 E,51 S)", date.minutes, 9, 0);

		ln_get_date (rst.set, &date);
		failed += test_result ("Sun set hour on 2006/01/17 at (15 E,51 S)", date.hours, 19, 0);
		failed += test_result ("Sun set minute on 2006/01/17 at (15 E,51 S)", date.minutes, 7, 0);
	}

	observer.lat = 37;

	object.dec = -54;
	failed += test_result ("Object at dec -54 never rise at 37 N", ln_get_object_rst(JD, &observer, &object, &rst), -1, 0);

	object.dec = -52;
	failed += test_result ("Object at dec -52 rise at 37 N", ln_get_object_rst(JD, &observer, &object, &rst), 0, 0);

	object.dec = 54;
	failed += test_result ("Object at dec 54 is always above the horizon at 37 N", ln_get_object_rst(JD, &observer, &object, &rst), 1, 0);

	object.dec = 52;
	failed += test_result ("Object at dec 52 rise at 37 N", ln_get_object_rst(JD, &observer, &object, &rst), 0, 0);

	observer.lat = -37;

	object.dec = 54;
	failed += test_result ("Object at dec 54 never rise at 37 S", ln_get_object_rst(JD, &observer, &object, &rst), -1, 0);

	object.dec = 52;
	failed += test_result ("Object at dec 52 rise at 37 S", ln_get_object_rst(JD, &observer, &object, &rst), 0, 0);

	object.dec = -54;
	failed += test_result ("Object at dec -54 is always above the horizon at 37 S", ln_get_object_rst(JD, &observer, &object, &rst), 1, 0);

	object.dec = -52;
	failed += test_result ("Object at dec -52 rise at 37 S", ln_get_object_rst(JD, &observer, &object, &rst), 0, 0);

	/* Venus on 1988 March 20 at Boston */
	date.years = 1988;
	date.months = 3;
	date.days = 20;

	date.hours = 0;
	date.minutes = 0;
	date.seconds = 0;

	JD = ln_get_julian_day (&date);

	observer.lng = -71.0833;
	observer.lat = 42.3333;

	ret = ln_get_venus_rst(JD, &observer, &rst);
	failed += test_result ("Venus sometime rise on 1988/03/20 at Boston", ret, 0, 0);
	
	if (!ret)
	{
		ln_get_date (rst.rise, &date);
		failed += test_result ("Venus rise hour on 1988/03/20 at Boston", date.hours, 12, 0);
		failed += test_result ("Venus rise minute on 1988/03/20 at Boston", date.minutes, 25, 0);

		ln_get_date (rst.transit, &date);
		failed += test_result ("Venus transit hour on 1988/03/20 at Boston", date.hours, 19, 0);
		failed += test_result ("Venus transit minute on 1988/03/20 at Boston", date.minutes, 41, 0);

		ln_get_date (rst.set, &date);
		failed += test_result ("Venus set hour on 1988/03/20 at Boston", date.hours, 2, 0);
		failed += test_result ("Venus set minute on 1988/03/20 at Boston", date.minutes, 55, 0);
	}

	return failed;
}

static int ell_rst_test(void)
{
  	struct ln_lnlat_posn observer;
	struct ln_ell_orbit orbit;
	struct ln_date date;
	struct ln_rst_time rst;
	struct ln_equ_posn pos;
	double JD;
	int ret;
	int failed = 0;

	/* Comment C/1996 B2 (Hyakutake) somewhere at Japan */

	observer.lng = 135.0;
	observer.lat = 35.0;
	
	date.years = 1996;
	date.months = 5;
	date.days = 1;
	
	date.hours = 0;
	date.minutes = 0;
	date.seconds = 0.0;

	orbit.JD = ln_get_julian_day(&date);
	orbit.JD += 0.39481;
	orbit.a = 1014.2022026431;
	orbit.e = 0.9997730;
	orbit.i = 124.92379;
	orbit.omega = 188.04546;
	orbit.w = 130.17654;
	orbit.n = 0.0;

	date.years = 1996;
	date.months = 3;
	date.days = 24;

	date.hours = date.minutes = 0;
	date.seconds = 0.0;

	JD = ln_get_julian_day(&date);

	ln_get_ell_body_equ_coords(JD, &orbit, &pos);
	failed += test_result("(RA) for Hyakutake 1996/03/28 00:00",
		pos.ra, 220.8554, 0.001);
	failed += test_result("(Dec) for Hyakutake 1996/03/28 00:00",
		pos.dec, 36.5341, 0.001);

	date.days = 28;

	date.hours = 10;
	date.minutes = 42;

	JD = ln_get_julian_day(&date);

	ln_get_ell_body_equ_coords(JD, &orbit, &pos);
	failed += test_result("(RA) for Hyakutake 1996/03/28 10:42",
		pos.ra, 56.2140, 0.001);
	failed += test_result("(Dec) for Hyakutake 1996/03/28 10:42",
		pos.dec, 74.4302, 0.001);

	date.days = 23;

	date.hours = 17;
	date.minutes = 38;
	date.seconds = 0.0;

	JD = ln_get_julian_day (&date);

	ln_get_ell_body_equ_coords(JD, &orbit, &pos);
	failed += test_result("(RA) for Hyakutake 1996/03/23 17:38",
		pos.ra,  221.2153, 0.001);
	failed += test_result("(Dec) for Hyakutake 1996/03/23 17:38",
		pos.dec, 32.4803, 0.001);

	JD = ln_get_julian_day(&date);
	
	ret = ln_get_ell_body_rst(JD, &observer, &orbit, &rst);
	failed += test_result("Hyakutake sometime rise on 1996/03/23 at 135 E, 35 N",
		ret, 0, 0);

	if (!ret) {
		ln_get_date(rst.rise, &date);
		failed += test_result("Hyakutake rise hour on 1996/03/23 at 135 E, 35 N",
			date.hours, 9, 0);
		failed += test_result("Hyakutake rise minute on 1996/03/23 at 135 E, 35 N",
			date.minutes, 31, 0);

		ln_get_date(rst.transit, &date);
		failed += test_result("Hyakutake transit hour on 1996/03/23 at 135 E, 35 N",
			date.hours, 17, 0);
		failed += test_result("Hyakutake transit minute on 1996/03/23 at 135 E, 35 N",
			date.minutes, 27, 0);

		ln_get_date(rst.set, &date);
		failed += test_result("Hyakutake set hour on 1996/03/23 at 135 E, 35 N",
			date.hours, 1, 0);
		failed += test_result("Hyakutake set minute on 1996/03/23 at 135 E, 35 N",
			date.minutes, 49, 0);
	}

	ret = ln_get_ell_body_next_rst(JD, &observer, &orbit, &rst);
	failed += test_result ("Hyakutake sometime rise on 1996/03/23 at 135 E, 35 N", ret, 0, 0);

	if (!ret) {
		ln_get_date(rst.rise, &date);
		failed += test_result("Hyakutake next rise hour on 1996/03/23 at 135 E, 35 N",
			date.hours, 9, 0);
		failed += test_result("Hyakutake next rise minute on 1996/03/23 at 135 E, 35 N",
			date.minutes, 31, 0);

		ln_get_date(rst.transit, &date);
		failed += test_result("Hyakutake next transit hour on 1996/03/24 at 135 E, 35 N",
			date.hours, 17, 0);
		failed += test_result("Hyakutake next transit minute on 1996/03/24 at 135 E, 35 N",
			date.minutes, 4, 0);

		ln_get_date(rst.set, &date);
		failed += test_result ("Hyakutake next set hour on 1996/03/23 at 135 E, 35 N",
			date.hours, 1, 0);
		failed += test_result ("Hyakutake next set minute on 1996/03/23 at 135 E, 35 N",
			date.minutes, 49, 0);
	}

	return failed;
}

static int hyp_future_rst_test(void)
{
	struct ln_lnlat_posn observer;
	struct ln_hyp_orbit orbit;
	struct ln_date date;
	struct ln_rst_time rst;
	double JD;
	int ret;
	int failed = 0;

	observer.lng = 15.0;
	observer.lat = 50.0;

	/* C/2006 P1 (McNaught) */

	orbit.q = 0.170742005109787;
	orbit.e = 1.00001895427704;
	orbit.i = 77.8348999023438;
	orbit.w = 155.977096557617;
	orbit.omega = 267.414398193359;
	orbit.JD = 2454113.251;

	date.years = 2007;
	date.months = 1;
	date.days = 17;

	date.hours = 12;
	date.minutes = 0;
	date.seconds = 0.0;

	JD = ln_get_julian_day(&date);

	ret = ln_get_hyp_body_next_rst_horizon(JD, &observer, &orbit, 0, &rst);
	failed += test_result("McNaught rise on 2997/01/18 at 15 E, 50 N",
		ret, 0, 0);

	if (!ret) {
		ln_get_date(rst.rise, &date);
		failed += test_result("McNaught rise hour on 2007/01/18 at 15 E, 50 N",
			date.hours, 9, 0);
		failed += test_result("McNaught rise minute on 2007/01/18 at 15 E, 50 N",
			date.minutes, 6, 0);

		ln_get_date(rst.transit, &date);
		failed += test_result("McNaught transit hour on 2007/01/18 at 15 E, 50 N",
			date.hours, 11, 0);
		failed += test_result("McNaught transit minute on 2007/01/18 at 15 E, 50 N",
			date.minutes, 38, 0);

		ln_get_date(rst.set, &date);
		failed += test_result("McNaught set hour on 2007/01/17 at 15 E, 50 N",
			date.hours, 14, 0);
		failed += test_result("McNaught set minute on 2007/01/17 at 15 E, 50 N",
			date.minutes, 37, 0);
	}

	ret = ln_get_hyp_body_next_rst_horizon(JD, &observer, &orbit, 15, &rst);
	failed += test_result("McNaught does not rise above 15 degrees on"
		"2007/01/17 at 15 E, 50 N", ret, -1, 0);
	
	return failed;
}

static int body_future_rst_test(void)
{
	struct ln_lnlat_posn observer;
	struct ln_date date;
	struct ln_rst_time rst;
	double JD;
	int ret;
	int failed = 0;

	observer.lng = 0;
	observer.lat = 85;

	date.years = 2006;
	date.months = 1;
	date.days = 1;

	date.hours = date.minutes = 0;
	date.seconds = 0.0;

	JD = ln_get_julian_day(&date);

	ret = ln_get_body_next_rst_horizon_future(JD, &observer,
		ln_get_solar_equ_coords, LN_SOLAR_STANDART_HORIZON, 300, &rst);

	failed += test_result("Sun is above horizon sometimes at 0, 85 N",
		ret, 0, 0);

	if (!ret) {
		ln_get_date(rst.rise, &date);
		failed += test_result("Solar next rise years at 0, 85 N",
			date.years, 2006, 0);
		failed += test_result("Solar next rise months at 0, 85 N",
			date.months, 3, 0);
		failed += test_result("Solar next rise days at 0, 85 N",
			date.days, 7, 0);
		failed += test_result("Solar next rise hour at 0, 85 N",
			date.hours, 10, 0);
		failed += test_result("Solar next rise minute at 0, 85 N",
			date.minutes, 19, 0);

		ln_get_date(rst.transit, &date);
		failed += test_result("Solar next transit years at 0, 85 N",
			date.years, 2006, 0);
		failed += test_result("Solar next transit months at 0, 85 N",
			date.months, 3, 0);
		failed += test_result("Solar next transit days at 0, 85 N",
			date.days, 7, 0);
		failed += test_result("Solar next transit hour at 0 E, 85 N",
			date.hours, 12, 0);
		failed += test_result("Solar next transit minute at 0 E, 85 N",
			date.minutes, 10, 0);

		ln_get_date(rst.set, &date);
		failed += test_result("Solar next set years at 0, 85 N",
			date.years, 2006, 0);
		failed += test_result("Solar next set months at 0, 85 N",
			date.months, 3, 0);
		failed += test_result("Solar next set days at 0, 85 N",
			date.days, 7, 0);
		failed += test_result("Solar next set hour at 0 E, 85 N",
			date.hours, 14, 0);
		failed += test_result("Solar next set minute at 0, 85 N",
			date.minutes, 8, 0);
	}

	ret = ln_get_body_next_rst_horizon_future(JD, &observer, ln_get_solar_equ_coords, 0, 300, &rst);
	failed += test_result ("Sun is above 0 horizon sometimes at 0, 85 N", ret, 0, 0);

	if (!ret) {
		ln_get_date(rst.rise, &date);
		failed += test_result("Solar next rise years at 0, 85 N with 0 horizon",
			date.years, 2006, 0);
		failed += test_result("Solar next rise months at 0, 85 N with 0 horizon",
			date.months, 3, 0);
		failed += test_result("Solar next rise days at 0, 85 N with 0 horizon",
			date.days, 9, 0);
		failed += test_result("Solar next rise hour at 0, 85 N with 0 horizon",
			date.hours, 10, 0);
		failed += test_result("Solar next rise minute at 0, 85 N with 0 horizon",
			date.minutes, 23, 0);

		ln_get_date(rst.transit, &date);
		failed += test_result("Solar next transit years at 0, 85 N with 0 horizon",
			date.years, 2006, 0);
		failed += test_result("Solar next transit months at 0, 85 N with 0 horizon",
			date.months, 3, 0);
		failed += test_result("Solar next transit days at 0, 85 N with 0 horizon",
			date.days, 9, 0);
		failed += test_result("Solar next transit hour at 0 E, 85 N with 0 horizon",
			date.hours, 12, 0);
		failed += test_result("Solar next transit minute at 0 E, 85 N with 0 horizon",
			date.minutes, 10, 0);

		ln_get_date(rst.set, &date);
		failed += test_result("Solar next set years at 0, 85 N with 0 horizon",
			date.years, 2006, 0);
		failed += test_result("Solar next set months at 0, 85 N with 0 horizon",
			date.months, 3, 0);
		failed += test_result("Solar next set days at 0, 85 N with 0 horizon",
			date.days, 9, 0);
		failed += test_result("Solar next set hour at 0 E, 85 N with 0 horizon",
			date.hours, 14, 0);
		failed += test_result("Solar next set minute at 0, 85 N with 0 horizon",
			date.minutes, 2, 0);
	}

	return failed;
}

static int parallax_test(void)
{
	struct ln_equ_posn mars, parallax;
  	struct ln_lnlat_posn observer;
	struct ln_dms dms;
	struct ln_date date;
	double jd;
	int failed = 0;

	dms.neg = 0;
	dms.degrees = 33;
	dms.minutes = 21;
	dms.seconds = 22.0;
	
	observer.lat = ln_dms_to_deg(&dms);

	dms.neg = 1;
	dms.degrees = 116;
	dms.minutes = 51;
	dms.seconds = 47.0;
	
	observer.lng = ln_dms_to_deg(&dms);

	date.years = 2003;
	date.months = 8;
	date.days = 28;

	date.hours = 3;
	date.minutes = 17;
	date.seconds = 0.0;

	jd = ln_get_julian_day(&date);

	ln_get_mars_equ_coords(jd, &mars);

	ln_get_parallax(&mars, ln_get_mars_earth_dist(jd),
			&observer, 1706, jd, &parallax);

	/* parallax is hard to calculate, so we allow relatively big error */
	failed += test_result("Mars RA parallax for Palomar observatory at "
		"2003/08/28 3:17 UT  ", parallax.ra, 0.003552, 0.00001);
	failed += test_result("Mars DEC parallax for Palomar observatory at "
		"2003/08/28 3:17 UT  ", parallax.dec, -20.01 / 3600.0, 0.00002);

	return failed;
}

static int angular_test(void)
{
	int failed = 0;
	double d;
	struct ln_equ_posn posn1, posn2;
		
	/* alpha Bootes (Arcturus) */
	posn1.ra = 213.9154;
	posn1.dec = 19.1825;
	
	/* alpha Virgo (Spica) */
	posn2.ra = 201.2983;
	posn2.dec = -11.1614;
	
	d = ln_get_angular_separation(&posn1, &posn2);
	failed += test_result("(Angular) Separation of Arcturus and Spica   ",
		d, 32.79302684, 0.00001);
	
	d = ln_get_rel_posn_angle(&posn1, &posn2);
	failed += test_result("(Angular) Position Angle of Arcturus and Spica   ",
		d, 22.39042787, 0.00001);
	
	return failed;
}

static int utility_test(void)
{
	struct ln_dms dms;
	double deg = -1.23, deg2 = 1.23, deg3 = -0.5;
	
	ln_deg_to_dms(deg, &dms);
	printf("TEST deg %f ==> deg %c%d min %d sec %f\n",
		deg, dms.neg ? '-' : '+', dms.degrees, dms.minutes, dms.seconds);
	ln_deg_to_dms(deg2, &dms);
	printf("TEST deg %f ==> deg %c%d min %d sec %f\n",
		deg2, dms.neg ? '-' : '+', dms.degrees, dms.minutes, dms.seconds);
	ln_deg_to_dms(deg3, &dms);
	printf("TEST deg %f ==> deg %c%d min %d sec %f\n",
		deg3, dms.neg ? '-' : '+', dms.degrees, dms.minutes, dms.seconds);
	return 0;
}

static int airmass_test(void)
{
	int failed = 0;
	double x;

	double X = ln_get_airmass(90, 750.0);
	failed += test_result("(Airmass) Airmass at Zenith", X, 1, 0);
 
	X = ln_get_airmass(10, 750.0);
	failed += test_result("(Airmass) Airmass at 10 degrees altitude",
		X, 5.64, 0.1);
	
	X = ln_get_alt_from_airmass(1, 750.0);
	failed += test_result("(Airmass) Altitude at airmass 1", X, 90, 0);

	for (x = -10; x < 90; x += 10.54546456) {
		X = ln_get_alt_from_airmass(ln_get_airmass(x, 750.0), 750.0);
		failed += test_result("(Airmass) Altitude->Airmass->Altitude at"
			"10 degrees", X, x, 0.000000001);
	}

	return failed;
}

static int constellation_test(void)
{  
	int i;

	int failed = 0;

        struct {
		double ra;
		double dec;
		const char *constel;
	} test_c[] = {
		{ 127.1810, 14.2177, "Cnc" },
		{ 139.0884, 11.5273, "Leo" }, 
		{ 139.0884, 11.5273, "Leo" },
		{ 150.7003, 8.3804, "Leo" },
		{ 162.0989, 4.9088, "Leo" },
		{ 173.3865, 1.2455, "Vir" },
		{ 184.6787, -2.4756, "Vir" },
		{ 219.8070, -12.6071, "Lib" },
		{ 232.3073, -15.1551, "Lib" },
		{ 245.3311, -17.0311, "Oph" },
		{ 258.8877, -18.0809, "Oph" },
		{ 272.9237, -18.1687, "Sgr" },
		{ 287.3266, -17.2004, "Sgr" },
		{ 301.9481, -15.1508, "Cap" },
		{ 316.6420, -12.0870, "Aqr" },
		{ 331.3001, -8.1780, "Aqr" },
		{ 345.8704, -3.6867, "Psc" },
		{ 0.3515, 1.0575, "Psc" },
		{ 14.7686, 5.6999, "Psc" },
		{ 29.1425, 9.9069, "Psc" },
		{ 43.4634, 13.4057, "Ari" },
		{ 57.6785, 16.0084, "Tau" },
		{ 71.6973, 17.6196, "Tau" },
		{ 85.4141, 18.2277, "Tau" },
		{ 98.7371, 17.8859, "Gem" },
		{ 111.6113, 16.6902, "Gem" },
		{ 124.0301, 14.7589, "Cnc" },
		{ 136.0329, 12.2183, "Cnc" },
		{ 147.6959, 9.1950, "Leo" },
		{ 0, 0, NULL }
	};

	for(i = 0; ; i++) {
		if (test_c[i].constel == NULL)
			break;

		struct ln_equ_posn equ;
		const char *constel;
		char test_r[200];

		equ.ra = test_c[i].ra;
		equ.dec = test_c[i].dec;
		constel = ln_get_constellation(&equ);
		sprintf(test_r, "Constellation at %.04f %+.04f", equ.ra, equ.dec);
		failed += test_str_result(test_r, constel, test_c[i].constel);
	}

	return failed;
}

int main(int argc, const char *argv[])
{
	int failed = 0;
	
	start_timer();

	failed += julian_test();
	failed += dynamical_test();
	failed += heliocentric_test ();
	failed += sidereal_test();
	failed += nutation_test();
	failed += aber_prec_nut_test();
	failed += transform_test();
	failed += solar_coord_test ();
	failed += aberration_test();
	failed += precession_test();
	failed += apparent_position_test ();
	failed += vsop87_test();
	failed += lunar_test ();
	failed += elliptic_motion_test();
	failed += parabolic_motion_test ();
	failed += hyperbolic_motion_test ();
	failed += rst_test ();
	failed += ell_rst_test ();
	failed += hyp_future_rst_test ();
	failed += body_future_rst_test ();
	failed += parallax_test ();
	failed += angular_test();
	failed += utility_test();
	failed += airmass_test ();
        failed += constellation_test ();
	
	end_timer();
	fprintf(stdout, "Test completed: %d tests, %d errors.\n",
		test_number, failed);
		
	return 0;
}
