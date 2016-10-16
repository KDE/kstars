/*
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Library General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA. 

Copyright (C) 2003 Liam Girdwood <lgirdwood@gmail.com>


A simple example showing some comet calculations.

Comet Encke

*/

#include <stdio.h>
#include <libnova/comet.h>
#include <libnova/julian_day.h>
#include <libnova/rise_set.h>
#include <libnova/transform.h>
#include <libnova/elliptic_motion.h>

/* use the orbital example from Meeus book */
#define MEEUS 0

static void print_date(const char *title, struct ln_zonedate *date)
{
	fprintf(stdout, "\n%s\n",title);
	fprintf(stdout, " Year    : %d\n", date->years);
	fprintf(stdout, " Month   : %d\n", date->months);
	fprintf(stdout, " Day     : %d\n", date->days);
	fprintf(stdout, " Hours   : %d\n", date->hours);
	fprintf(stdout, " Minutes : %d\n", date->minutes);
	fprintf(stdout, " Seconds : %f\n", date->seconds);
	fprintf(stdout, " GMT offset %ld\n", date->gmtoff);
}

int main (int argc, const char *argv[])
{
	struct ln_equ_posn equ;
	struct ln_rst_time rst;
	struct ln_zonedate rise, set, transit;
#if MEEUS
	struct ln_date epoch_date, date;
#endif
	struct ln_lnlat_posn observer;
	struct ln_ell_orbit orbit;
	struct ln_rect_posn posn;
	double JD, e_JD;
	double E, v, V, r, l, dist, M;
	
	/* observers location (Edinburgh), used to calc rst */
	observer.lat = 55.92; /* 55.92 N */
	observer.lng = -3.18; /* 3.18 W */

#if MEEUS
	date.years = 1990;
	date.months = 10;
	date.days = 6;
	date.hours = 0;
	date.minutes = 0;
	date.seconds = 0;
	JD = ln_get_julian_day(&date);
#else
	/* get Julian day from local time */
	JD = ln_get_julian_from_sys();	
	fprintf(stdout, "JD %f\n", JD);
#endif
	
	/* calc epoch JD */
#if MEEUS
	epoch_date.years = 1990;
	epoch_date.months = 10;
	epoch_date.days = 28;
	epoch_date.hours = 12;
	epoch_date.minutes = 30;
	epoch_date.seconds = 0;
	e_JD = ln_get_julian_day(&epoch_date);
#else
	e_JD = 2456617.5;
#endif

	fprintf(stdout, "Epoch JD %f diff %f\n", e_JD, JD - e_JD);

	/* Encke orbital elements */
#if MEEUS
	orbit.JD = e_JD;
	orbit.a = 2.2091404;
	orbit.e = 0.8502196;
	orbit.i = 11.94525;
	orbit.omega = 334.75006;
	orbit.w = 186.23352;
	orbit.n = 0;
#else
	orbit.JD = e_JD;
	orbit.a = 2.214743;
	orbit.e = 0.848232;
	orbit.i = 11.7790;
	orbit.omega = 334.5731;
	orbit.w = 186.5356;
	orbit.n = 0;//0.2990330;
#endif
	/* get mean anomaly */
	if (orbit.n == 0.0)
		orbit.n = ln_get_ell_mean_motion(orbit.a);
	M = ln_get_ell_mean_anomaly(orbit.n, JD - orbit.JD);
	fprintf(stdout,
			"(Mean Anomaly) M when n is %f and JD diff is %f = %f\n",
			orbit.n, JD - orbit.JD, M);

	/* solve kepler for orbit */
	E = ln_solve_kepler(orbit.e, M);
	fprintf(stdout,
		"(Equation of kepler) E when e is %f and M is %f = %f\n",
		orbit.e, M, E);
	
	/* true anomaly */
	v = ln_get_ell_true_anomaly(orbit.e, E);
	fprintf(stdout,
		"(True Anomaly) v when e is %f and E is %f = %f\n", orbit.e, E, v);
	
	/* radius vector */
	r = ln_get_ell_radius_vector(M, orbit.e, E);
	fprintf(stdout,
		"(Radius Vector) r when e is %f and E is %f = %f\n", orbit.e, E, r);
	
	/* geocentric rect coords */
	ln_get_ell_geo_rect_posn(&orbit, JD, &posn);
	fprintf(stdout,
		"(Geocentric Rect Coords X) for comet Encke  %f\n", posn.X);
	fprintf(stdout,
		"(Geocentric Rect Coords Y) for comet Encke  %f\n", posn.Y);
	fprintf(stdout,
		"(Geocentric Rect Coords Z) for comet Encke  %f\n", posn.Z);
	
	/* rectangular coords */
	ln_get_ell_helio_rect_posn(&orbit, JD, &posn);
	fprintf(stdout,
		"(Heliocentric Rect Coords X) for comet Encke  %f\n", posn.X);
	fprintf(stdout,
		"(Heliocentric Rect Coords Y) for comet Encke  %f\n", posn.Y);
	fprintf(stdout,
		"(Heliocentric Rect Coords Z) for comet Encke  %f\n", posn.Z);
	
	/* ra, dec */
	ln_get_ell_body_equ_coords(JD, &orbit, &equ);
	fprintf(stdout, "(RA) for comet Encke  %f\n", equ.ra);
	fprintf(stdout, "(Dec) for comet Encke  %f\n", equ.dec);
	
	/* orbit length */
	l = ln_get_ell_orbit_len(&orbit);
	fprintf(stdout, "(Orbit Length) for comet Encke in AU  %f\n", l);
	
	/* orbital velocity at perihelion */
	V = ln_get_ell_orbit_pvel(&orbit);
	fprintf(stdout,
		"(Orbit Perihelion Vel) for comet Encke in kms  %f\n", V);
	
	/* orbital velocity at aphelion */
	V = ln_get_ell_orbit_avel(&orbit);
	fprintf(stdout, "(Orbit Aphelion Vel) for comet Encke in kms  %f\n", V);
	
	/* average orbital velocity */
	V = ln_get_ell_orbit_vel(JD, &orbit);
	fprintf(stdout, "(Orbit Vel JD) for comet Encke in kms  %f\n", V);
	
	/* comet sun distance */
	dist = ln_get_ell_body_solar_dist(JD, &orbit);
	fprintf(stdout, "(Body Solar Dist) for comet Encke in AU  %f\n", dist);
	
	/* comet earth distance */
	dist = ln_get_ell_body_earth_dist(JD, &orbit);
	fprintf(stdout, "(Body Earth Dist) for comet Encke in AU  %f\n", dist);
	
	/* rise, set and transit */
	if (ln_get_ell_body_rst(JD, &observer, &orbit, &rst) != 0)
		fprintf(stdout, "Comet is circumpolar\n");
	else {
		ln_get_local_date(rst.rise, &rise);
		ln_get_local_date(rst.transit, &transit);
		ln_get_local_date(rst.set, &set);
		print_date("Rise", &rise);
		print_date("Transit", &transit);
		print_date("Set", &set);
	}
	
	return 0;
}
