/*
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
 *  Copyright (C) 2000 - 2005 Liam Girdwood <lgirdwood@gmail.com>
 *
 *  The functions in this file use the Lunar Solution ELP 2000-82B by
 *  Michelle Chapront-Touze and Jean Chapront.
 */

#include <math.h>
#include <stdlib.h>
#include <libnova/lunar.h>
#include <libnova/vsop87.h>
#include <libnova/solar.h>
#include <libnova/earth.h>
#include <libnova/transform.h>
#include <libnova/rise_set.h>
#include <libnova/utility.h>
#include "lunar-priv.h"

#ifdef HAVE_LIBsunmath
#include <sunmath.h>
#endif

/* AU in KM */
#define AU			149597870

/* sequence sizes */
#define ELP1_SIZE	1023		/* Main problem. Longitude periodic terms (sine) */
#define ELP2_SIZE	918		/* Main problem. Latitude (sine) */
#define ELP3_SIZE	704		/* Main problem. Distance (cosine) */
#define ELP4_SIZE	347		/* Earth figure perturbations. Longitude */
#define ELP5_SIZE	316		/* Earth figure perturbations. Latitude */
#define ELP6_SIZE	237		/* Earth figure perturbations. Distance */
#define ELP7_SIZE	14		/* Earth figure perturbations. Longitude/t */
#define ELP8_SIZE	11		/* Earth figure perturbations. Latitude/t */
#define ELP9_SIZE	8		/* Earth figure perturbations. Distance/t */
#define ELP10_SIZE	14328		/* Planetary perturbations. Table 1 Longitude */
#define ELP11_SIZE	5233		/* Planetary perturbations. Table 1 Latitude */
#define ELP12_SIZE	6631		/* Planetary perturbations. Table 1 Distance */
#define ELP13_SIZE	4384		/* Planetary perturbations. Table 1 Longitude/t */
#define ELP14_SIZE	833		/* Planetary perturbations. Table 1 Latitude/t */
#define ELP15_SIZE	1715		/* Planetary perturbations. Table 1 Distance/t */
#define ELP16_SIZE	170		/* Planetary perturbations. Table 2 Longitude */
#define ELP17_SIZE	150		/* Planetary perturbations. Table 2 Latitude */
#define ELP18_SIZE	114		/* Planetary perturbations. Table 2 Distance */
#define ELP19_SIZE	226		/* Planetary perturbations. Table 2 Longitude/t */
#define ELP20_SIZE	188		/* Planetary perturbations. Table 2 Latitude/t */
#define ELP21_SIZE	169		/* Planetary perturbations. Table 2 Distance/t */
#define ELP22_SIZE	3		/* Tidal effects. Longitude */
#define ELP23_SIZE	2		/* Tidal effects. Latitude */
#define ELP24_SIZE	2		/* Tidal effects. Distance */
#define ELP25_SIZE	6		/* Tidal effects. Longitude/t */
#define ELP26_SIZE	4		/* Tidal effects. Latitude/t */
#define ELP27_SIZE	5		/* Tidal effects. Distance/t */
#define ELP28_SIZE	20		/* Moon figure perturbations. Longitude */
#define ELP29_SIZE	12		/* Moon figure perturbations. Latitude */
#define ELP30_SIZE	14		/* Moon figure perturbations. Distance */
#define ELP31_SIZE	11		/* Relativistic perturbations. Longitude */
#define ELP32_SIZE	4		/* Relativistic perturbations. Latitude */
#define ELP33_SIZE	10		/* Relativistic perturbations. Distance */
#define ELP34_SIZE	28		/* Planetary perturbations - solar eccentricity. Longitude/t2 */
#define ELP35_SIZE	13		/* Planetary perturbations - solar eccentricity. Latitude/t2 */
#define ELP36_SIZE	19		/* Planetary perturbations - solar eccentricity. Distance/t2 */


/* Chapront theory lunar constants */
#define		RAD		(648000.0 / M_PI)
#define		DEG		(M_PI / 180.0)
#define		M_PI2	(2.0 * M_PI)
#define		PIS2	(M_PI / 2.0)
#define		ATH		384747.9806743165
#define		A0		384747.9806448954
#define		AM		0.074801329518
#define		ALPHA	0.002571881335
#define		DTASM	(2.0 * ALPHA / (3.0 * AM))
#define		W12		(1732559343.73604 / RAD)
#define		PRECES	(5029.0966 / RAD)
#define		C1		60.0
#define		C2		3600.0

/* Corrections of the constants for DE200/LE200 */
#define		DELNU	((0.55604 / RAD) / W12)
#define		DELE	(0.01789 / RAD)
#define		DELG	(-0.08066 / RAD)
#define		DELNP	((-0.06424 / RAD) / W12)
#define		DELEP	(-0.12879 / RAD)

/*     Precession matrix */
#define		P1		0.10180391e-4
#define		P2		0.47020439e-6
#define		P3		-0.5417367e-9
#define		P4		-0.2507948e-11
#define		P5		0.463486e-14
#define		Q1		-0.113469002e-3
#define		Q2		0.12372674e-6
#define		Q3		0.1265417e-8
#define		Q4		-0.1371808e-11
#define		Q5		-0.320334e-14

/* initialise lunar constants */
void init_lunar_constants ();

/* constants with corrections for DE200 / LE200 */
static const double W1[5] =
{
	((218.0 + (18.0 / 60.0) + (59.95571 / 3600.0))) * DEG,
	1732559343.73604 / RAD,
	-5.8883 / RAD,
	0.006604 / RAD,
	-0.00003169 / RAD
};

static const double W2[5] =
{
	((83.0 + (21.0 / 60.0) + (11.67475 / 3600.0))) * DEG,
	14643420.2632 / RAD,
	-38.2776 /  RAD,
	-0.045047 / RAD,
	0.00021301 / RAD
};

static const double W3[5] =
{
	(125.0 + (2.0 / 60.0) + (40.39816 / 3600.0)) * DEG,
	-6967919.3622 / RAD,
	6.3622 / RAD,
	0.007625 / RAD,
	-0.00003586 / RAD
};

static const double earth[5] =
{
	(100.0 + (27.0 / 60.0) + (59.22059 / 3600.0)) * DEG,
	129597742.2758 / RAD,
	-0.0202 / RAD,
	0.000009 / RAD,
	0.00000015 / RAD
};

static const double peri[5] =
{
	(102.0 + (56.0 / 60.0) + (14.42753 / 3600.0)) * DEG,
	1161.2283 / RAD,
	0.5327 / RAD,
	-0.000138 / RAD,
	0.0
};

/* Delaunay's arguments.*/
static const double del[4][5] = {
	{ 5.198466741027443, 7771.377146811758394, -0.000028449351621, 0.000000031973462, -0.000000000154365 },
	{ -0.043125180208125, 628.301955168488007, -0.000002680534843, 0.000000000712676, 0.000000000000727 },
	{ 2.355555898265799, 8328.691426955554562, 0.000157027757616, 0.000000250411114, -0.000000001186339 },
	{ 1.627905233371468, 8433.466158130539043, -0.000059392100004, -0.000000004949948, 0.000000000020217 }
};


static const double zeta[2] =
{
	(218.0 + (18.0 / 60.0) + (59.95571 / 3600.0)) * DEG,
	((1732559343.73604 / RAD) + PRECES)
};


/* Planetary arguments */
static const double p[8][2] =
{
	{(252.0 + 15.0 / C1 + 3.25986 / C2 ) * DEG, 538101628.68898 / RAD },
	{(181.0 + 58.0 / C1 + 47.28305 / C2) * DEG, 210664136.43355 / RAD },
	{(100.0 + (27.0 / 60.0) + (59.22059 / 3600.0)) * DEG, 129597742.2758 / RAD},
	{(355.0 + 25.0 / C1 + 59.78866 / C2) * DEG, 68905077.59284 / RAD },
	{(34.0 + 21.0 / C1 + 5.34212 / C2) * DEG, 10925660.42861 / RAD },
	{(50.0 + 4.0 / C1 + 38.89694 / C2) * DEG, 4399609.65932 / RAD },
	{(314.0 + 3.0 / C1 + 18.01841 / C2) * DEG, 1542481.19393 / RAD },
	{(304.0 + 20.0 / C1 + 55.19575 / C2) * DEG, 786550.32074 / RAD }
};

extern const struct main_problem elp1[];
extern const struct main_problem elp2[];
extern const struct main_problem elp3[];
extern const struct earth_pert elp4[];
extern const struct earth_pert elp5[];
extern const struct earth_pert elp6[];
extern const struct earth_pert elp7[];
extern const struct earth_pert elp8[];
extern const struct earth_pert elp9[];
extern const struct planet_pert elp10[];
extern const struct planet_pert elp11[];
extern const struct planet_pert elp12[];
extern const struct planet_pert elp13[];
extern const struct planet_pert elp14[];
extern const struct planet_pert elp15[];
extern const struct planet_pert elp16[];
extern const struct planet_pert elp17[];
extern const struct planet_pert elp18[];
extern const struct planet_pert elp19[];
extern const struct planet_pert elp20[];
extern const struct planet_pert elp21[];
extern const struct earth_pert elp22[];
extern const struct earth_pert elp23[];
extern const struct earth_pert elp24[];
extern const struct earth_pert elp25[];
extern const struct earth_pert elp26[];
extern const struct earth_pert elp27[];
extern const struct earth_pert elp28[];
extern const struct earth_pert elp29[];
extern const struct earth_pert elp30[];
extern const struct earth_pert elp31[];
extern const struct earth_pert elp32[];
extern const struct earth_pert elp33[];
extern const struct earth_pert elp34[];
extern const struct earth_pert elp35[];
extern const struct earth_pert elp36[];

/* sum lunar elp1 series */
static double sum_series_elp1 (double* t)
{
	double result = 0;
	double x,y;
	double tgv;
	int i,j,k;

	for (j = 0; j < ELP1_SIZE; j++) {
		/* derivatives of A */
		tgv = elp1[j].B[0] + DTASM * elp1[j].B[4];
		x = elp1[j].A + tgv * (DELNP - AM * DELNU) +
			elp1[j].B[1] * DELG + elp1[j].B[2] *
			DELE + elp1[j].B[3] * DELEP;

		y = 0;
		for (k = 0; k < 5; k++) {
			for (i = 0; i < 4; i++)
				y += elp1[j].ilu[i] * del[i][k] * t[k];
		}

		/* y in correct quad */
		y = ln_range_radians2(y);
		result += x * sin(y);
	}
	return result;
}

/* sum lunar elp2 series */
static double sum_series_elp2 (double* t)
{
	double result = 0;
	double x,y;
	double tgv;
	int i,j,k;

	for (j = 0; j < ELP2_SIZE; j++) {
		/* derivatives of A */
		tgv = elp2[j].B[0] + DTASM * elp2[j].B[4];
		x = elp2[j].A + tgv * (DELNP - AM * DELNU) +
			elp2[j].B[1] * DELG + elp2[j].B[2] *
			DELE + elp2[j].B[3] * DELEP;

		y = 0;
		for (k = 0; k < 5; k++) {
			for (i = 0; i < 4; i++)
				y += elp2[j].ilu[i] * del[i][k] * t[k];
		}
		/* y in correct quad */
		y = ln_range_radians2(y);
		result += x * sin(y);
	}
	return result;
}

/* sum lunar elp3 series */
static double sum_series_elp3 (double* t)
{
	double result = 0;
	double x,y;
	double tgv;
	int i,j,k;

	for (j = 0; j < ELP3_SIZE; j++) {
		/* derivatives of A */
		tgv = elp3[j].B[0] + DTASM * elp3[j].B[4];
		x = elp3[j].A + tgv * (DELNP - AM * DELNU) +
			elp3[j].B[1] * DELG + elp3[j].B[2] *
			DELE + elp3[j].B[3] * DELEP;

		y = 0;
		for (k = 0; k < 5; k++) {
			for (i = 0; i < 4; i++)
				y += elp3[j].ilu[i] * del[i][k] * t[k];
		}
		y += (M_PI_2);
		/* y in correct quad */
		y = ln_range_radians2(y);
		result += x * sin(y);
	}
	return result;
}


/* sum lunar elp4 series */
static double sum_series_elp4(double *t)
{
	double result = 0;
	int i,j,k;
	double y;

	for (j = 0; j < ELP4_SIZE; j++) {
		y = elp4[j].O * DEG;
		for (k = 0; k < 2; k++)  {
			y += elp4[j].iz * zeta[k] * t[k];
			for (i = 0; i < 4; i++)
				y += elp4[j].ilu[i] * del[i][k] * t[k];
		}
		/* put y in correct quad */
		y = ln_range_radians2(y);
		result += elp4[j].A * sin(y);
	}
	return result;
}

/* sum lunar elp5 series */
static double sum_series_elp5(double *t)
{
	double result = 0;
	int i,j,k;
	double y;

	for (j = 0; j < ELP5_SIZE; j++) {
		y = elp5[j].O * DEG;
		for (k = 0; k < 2; k++) {
			y += elp5[j].iz * zeta[k] * t[k];
			for (i = 0; i < 4; i++)
				y += elp5[j].ilu[i] * del[i][k] * t[k];
		}
		/* put y in correct quad */
		y = ln_range_radians2(y);
		result += elp5[j].A * sin(y);
	}
	return result;
}


/* sum lunar elp6 series */
static double sum_series_elp6(double *t)
{
	double result = 0;
	int i,j,k;
	double y;

	for (j = 0; j < ELP6_SIZE; j++) {
		y = elp6[j].O * DEG;
		for (k = 0; k < 2; k++) {
			y += elp6[j].iz * zeta[k] * t[k];
			for (i = 0; i < 4; i++)
				y += elp6[j].ilu[i] * del[i][k] * t[k];
		}
		/* put y in correct quad */
		y = ln_range_radians2(y);
		result += elp6[j].A * sin(y);
	}
	return result;
}

/* sum lunar elp7 series */
static double sum_series_elp7(double *t)
{
	double result = 0;
	int i,j,k;
	double y, A;

	for (j = 0; j < ELP7_SIZE; j++) {
		A = elp7[j].A * t[1];
		y = elp7[j].O * DEG;
		for (k = 0; k < 2; k++) {
			y += elp7[j].iz * zeta[k] * t[k];
			for (i = 0; i < 4; i++)
				y += elp7[j].ilu[i] * del[i][k] * t[k];
		}
		/* put y in correct quad */
		y = ln_range_radians2(y);
		result += A * sin(y);
	}
	return result;
}

/* sum lunar elp8 series */
static double sum_series_elp8(double *t)
{
	double result = 0;
	int i,j,k;
	double y, A;

	for (j = 0; j < ELP8_SIZE; j++) {
		y = elp8[j].O * DEG;
		A = elp8[j].A * t[1];
		for (k = 0; k < 2; k++) {
			y += elp8[j].iz * zeta[k] * t[k];
			for (i = 0; i < 4; i++)
				y += elp8[j].ilu[i] * del[i][k] * t[k];
		}
		/* put y in correct quad */
		y = ln_range_radians2(y);
		result += A * sin(y);
	}
	return result;
}

/* sum lunar elp9 series */
static double sum_series_elp9(double *t)
{
	double result = 0;
	int i,j,k;
	double y, A;

	for (j = 0; j < ELP9_SIZE; j++) {
		A = elp9[j].A * t[1];
		y = elp9[j].O * DEG;
		for (k = 0; k < 2; k++) {
			y += elp9[j].iz * zeta[k] * t[k];
			for (i = 0; i < 4; i++)
				y += elp9[j].ilu[i] * del[i][k] * t[k];
		}
		/* put y in correct quad */
		y = ln_range_radians2(y);
		result += A * sin(y);
	}
	return result;
}

/* sum lunar elp10 series */
static double sum_series_elp10(double *t)
{
	double result = 0;
	int i,j,k;
	double y;

	for (j = 0; j < ELP10_SIZE; j++) {
		y = elp10[j].theta * DEG;

		for (k = 0; k < 2; k++) {
			y += (elp10[j].ipla[8] * del[0][k]
			+ elp10[j].ipla[9] * del[2][k]
			+ elp10[j].ipla[10] * del [3][k]) * t[k];
			for (i = 0; i < 8; i++)
				y += elp10[j].ipla[i] * p[i][k] * t[k];
		}

		/* put y in correct quad */
		y = ln_range_radians2(y);
		result += elp10[j].O * sin(y);
	}
	return result;
}

/* sum lunar elp11 series */
static double sum_series_elp11(double *t)
{
	double result = 0;
	int i,j,k;
	double y;

	for (j = 0; j < ELP11_SIZE; j++) {
		y = elp11[j].theta * DEG;
		for (k = 0; k < 2; k++)  {
			y += (elp11[j].ipla[8] * del[0][k]
			+ elp11[j].ipla[9] * del[2][k]
			+ elp11[j].ipla[10] * del [3][k]) * t[k];
			for (i = 0; i < 8; i++)
				y += elp11[j].ipla[i] * p[i][k] * t[k];
		}
		/* put y in correct quad */
		y = ln_range_radians2(y);
		result += elp11[j].O * sin(y);
	}
	return result;
}

/* sum lunar elp12 series */
static double sum_series_elp12(double *t)
{
	double result = 0;
	int i,j,k;
	double y;

	for (j = 0; j < ELP12_SIZE; j++) {
		y = elp12[j].theta * DEG;
		for (k = 0; k < 2; k++) {
			y += (elp12[j].ipla[8] * del[0][k]
			+ elp12[j].ipla[9] * del[2][k]
			+ elp12[j].ipla[10] * del [3][k]) * t[k];
			for (i = 0; i < 8; i++)
				y += elp12[j].ipla[i] * p[i][k] * t[k];
		}
		/* put y in correct quad */
		y = ln_range_radians2(y);
		result += elp12[j].O * sin(y);
	}
	return result;
}

/* sum lunar elp13 series */
static double sum_series_elp13(double *t)
{
	double result = 0;
	int i,j,k;
	double y,x;

	for (j = 0; j < ELP13_SIZE; j++) {
		y = elp13[j].theta * DEG;
		for (k = 0; k < 2; k++) {
			y += (elp13[j].ipla[8] * del[0][k]
			+ elp13[j].ipla[9] * del[2][k]
			+ elp13[j].ipla[10] * del [3][k]) * t[k];
			for (i = 0; i < 8; i++)
				y += elp13[j].ipla[i] * p[i][k] * t[k];
		}
		/* put y in correct quad */
		y = ln_range_radians2(y);
		x = elp13[j].O * t[1];
		result += x * sin(y);
	}
	return result;
}

/* sum lunar elp14 series */
static double sum_series_elp14(double *t)
{
	double result = 0;
	int i,j,k;
	double y,x;

	for (j = 0; j < ELP14_SIZE; j++) {
		y = elp14[j].theta * DEG;
		for (k = 0; k < 2; k++)  {
			y += (elp14[j].ipla[8] * del[0][k]
			+ elp14[j].ipla[9] * del[2][k]
			+ elp14[j].ipla[10] * del [3][k]) * t[k];
			for (i = 0; i < 8; i++)
				y += elp14[j].ipla[i] * p[i][k] * t[k];
		}
		/* put y in correct quad */
		y = ln_range_radians2(y);
		x = elp14[j].O * t[1];
		result += x * sin(y);
	}
	return result;
}


/* sum lunar elp15 series */
static double sum_series_elp15(double *t)
{
	double result = 0;
	int i,j,k;
	double y,x;

	for (j = 0; j < ELP15_SIZE; j++) {
		y = elp15[j].theta * DEG;
		for (k = 0; k < 2; k++) {
			y += (elp15[j].ipla[8] * del[0][k]
			+ elp15[j].ipla[9] * del[2][k]
			+ elp15[j].ipla[10] * del [3][k]) * t[k];
			for (i = 0; i < 8; i++)
				y += elp15[j].ipla[i] * p[i][k] * t[k];
		}
		/* put y in correct quad */
		y = ln_range_radians2(y);
		x = elp15[j].O * t[1];
		result += x * sin(y);
	}
	return result;
}

/* sum lunar elp16 series */
static double sum_series_elp16(double *t)
{
	double result = 0;
	int i,j,k;
	double y;

	for (j = 0; j < ELP16_SIZE; j++) {
		y = elp16[j].theta * DEG;
		for (k = 0; k < 2; k++) {
			for (i = 0; i < 4; i++)
				y += elp16[j].ipla[i + 7] * del[i][k] * t[k];
			for (i = 0; i < 7; i++)
				y += elp16[j].ipla[i] * p[i][k] * t[k];
		}
		/* put y in correct quad */
		y = ln_range_radians2(y);
		result += elp16[j].O * sin(y);
	}
	return result;
}

static double sum_series_elp17(double *t)
{
	double result = 0;
	int i,j,k;
	double y;

	for (j = 0; j < ELP17_SIZE; j++) {
		y = elp17[j].theta * DEG;
		for (k = 0; k < 2; k++) {
			for (i = 0; i < 4; i++)
				y += elp17[j].ipla[i + 7] * del[i][k] * t[k];
			for (i = 0; i < 7; i++)
				y += elp17[j].ipla[i] * p[i][k] * t[k];
		}
		/* put y in correct quad */
		y = ln_range_radians2(y);
		result += elp17[j].O * sin(y);
	}
	return result;
}

static double sum_series_elp18(double *t)
{
	double result = 0;
	int i,j,k;
	double y;

	for (j = 0; j < ELP18_SIZE; j++) {
		y = elp18[j].theta * DEG;
		for (k = 0; k < 2; k++) {
			for (i = 0; i < 4; i++)
				y += elp18[j].ipla[i + 7] * del[i][k] * t[k];
			for (i = 0; i < 7; i++)
				y += elp18[j].ipla[i] * p[i][k] * t[k];
		}
		/* put y in correct quad */
		y = ln_range_radians2(y);
		result += elp18[j].O * sin(y);
	}
	return result;
}

static double sum_series_elp19(double *t)
{
	double result = 0;
	int i,j,k;
	double y,x;

	for (j = 0; j < ELP19_SIZE; j++) {
		y = elp19[j].theta * DEG;
		for (k = 0; k < 2; k++) {
			for (i = 0; i < 4; i++)
				y += elp19[j].ipla[i + 7] * del[i][k] * t[k];
			for (i = 0; i < 7; i++)
				y += elp19[j].ipla[i] * p[i][k] * t[k];
		}
		/* put y in correct quad */
		y = ln_range_radians2(y);
		x = elp19[j].O * t[1];
		result += x * sin(y);
	}
	return result;
}

static double sum_series_elp20(double *t)
{
	double result = 0;
	int i,j,k;
	double y,x;

	for (j = 0; j < ELP20_SIZE; j++) {
		y = elp20[j].theta * DEG;
		for (k = 0; k < 2; k++) {
			for (i = 0; i < 4; i++)
				y += elp20[j].ipla[i + 7] * del[i][k] * t[k];
			for (i = 0; i < 7; i++)
				y += elp20[j].ipla[i] * p[i][k] * t[k];
		}
		/* put y in correct quad */
		y = ln_range_radians2(y);
		x = elp20[j].O * t[1];
		result += x * sin(y);
	}
	return result;
}

static double sum_series_elp21(double *t)
{
	double result = 0;
	int i,j,k;
	double y,x;

	for (j = 0; j < ELP21_SIZE; j++) {
		y = elp21[j].theta * DEG;
		for (k = 0; k < 2; k++) {
			for (i = 0; i < 4; i++)
				y += elp21[j].ipla[i + 7] * del[i][k] * t[k];
			for (i = 0; i < 7; i++)
				y += elp21[j].ipla[i] * p[i][k] * t[k];
		}
		/* put y in correct quad */
		y = ln_range_radians2(y);
		x = elp21[j].O * t[1];
		result += x * sin(y);
	}
	return result;
}

/* sum lunar elp22 series */
static double sum_series_elp22(double *t)
{
	double result = 0;
	int i,j,k;
	double y;

	for (j = 0; j < ELP22_SIZE; j++) {
		y = elp22[j].O * DEG;
		for (k = 0; k < 2; k++) {
			y += elp22[j].iz * zeta[k] * t[k];
			for (i = 0; i < 4; i++)
				y += elp22[j].ilu[i] * del[i][k] * t[k];
		}
		/* put y in correct quad */
		y = ln_range_radians2(y);
		result += elp22[j].A * sin(y);
	}
	return result;
}

/* sum lunar elp23 series */
static double sum_series_elp23(double *t)
{
	double result = 0;
	int i,j,k;
	double y;

	for (j = 0; j < ELP23_SIZE; j++) {
		y = elp23[j].O * DEG;
		for (k = 0; k < 2; k++) {
			y += elp23[j].iz * zeta[k] * t[k];
			for (i = 0; i < 4; i++)
				y += elp23[j].ilu[i] * del[i][k] * t[k];
		}
		/* put y in correct quad */
		y = ln_range_radians2(y);
		result += elp23[j].A * sin(y);
	}
	return result;
}

/* sum lunar elp24 series */
static double sum_series_elp24(double *t)
{
	double result = 0;
	int i,j,k;
	double y;

	for (j = 0; j < ELP24_SIZE; j++) {
		y = elp24[j].O * DEG;
		for (k = 0; k < 2; k++) {
			y += elp24[j].iz * zeta[k] * t[k];
			for (i = 0; i < 4; i++)
				y += elp24[j].ilu[i] * del[i][k] * t[k];
		}
		/* put y in correct quad */
		y = ln_range_radians2(y);
		result += elp24[j].A * sin(y);
	}
	return result;
}

/* sum lunar elp25 series */
static double sum_series_elp25(double *t)
{
	double result = 0;
	int i,j,k;
	double y, A;

	for (j = 0; j < ELP25_SIZE; j++) {
		A = elp25[j].A * t[1];
		y = elp25[j].O * DEG;
		for (k = 0; k < 2; k++) {
			y += elp25[j].iz * zeta[k] * t[k];
			for (i = 0; i < 4; i++)
				y += elp25[j].ilu[i] * del[i][k] * t[k];
		}
		/* put y in correct quad */
		y = ln_range_radians2(y);
		result += A * sin(y);
	}
	return result;
}

/* sum lunar elp26 series */
static double sum_series_elp26(double *t)
{
	double result = 0;
	int i,j,k;
	double y, A;

	for (j = 0; j < ELP26_SIZE; j++) {
		A = elp26[j].A * t[1];
		y = elp26[j].O * DEG;
		for (k = 0; k < 2; k++) {
			y += elp26[j].iz * zeta[k] * t[k];
			for (i = 0; i < 4; i++)
				y += elp26[j].ilu[i] * del[i][k] * t[k];
		}
		/* put y in correct quad */
		y = ln_range_radians2(y);
		result += A * sin(y);
	}
	return result;
}

/* sum lunar elp27 series */
static double sum_series_elp27(double *t)
{
	double result = 0;
	int i,j,k;
	double y, A;

	for (j = 0; j < ELP27_SIZE; j++) {
		A = elp27[j].A * t[1];
		y = elp27[j].O * DEG;
		for (k = 0; k < 2; k++) {
			y += elp27[j].iz * zeta[k] * t[k];
			for (i = 0; i < 4; i++)
				y += elp27[j].ilu[i] * del[i][k] * t[k];
		}
		/* put y in correct quad */
		y = ln_range_radians2(y);
		result += A * sin(y);
	}
	return result;
}

/* sum lunar elp28 series */
static double sum_series_elp28(double *t)
{
	double result = 0;
	int i,j,k;
	double y;

	for (j = 0; j < ELP28_SIZE; j++) {
		y = elp28[j].O * DEG;
		for (k = 0; k < 2; k++) {
			y += elp28[j].iz * zeta[k] * t[k];
			for (i = 0; i < 4; i++)
				y += elp28[j].ilu[i] * del[i][k] * t[k];
		}
		/* put y in correct quad */
		y = ln_range_radians2(y);
		result += elp28[j].A * sin(y);
	}
	return result;
}

/* sum lunar elp29 series */
static double sum_series_elp29(double *t)
{
	double result = 0;
	int i,j,k;
	double y;

	for (j = 0; j < ELP29_SIZE; j++) {
		y = elp29[j].O * DEG;
		for (k = 0; k < 2; k++) {
			y += elp29[j].iz * zeta[k] * t[k];
			for (i = 0; i < 4; i++)
				y += elp29[j].ilu[i] * del[i][k] * t[k];
		}
		/* put y in correct quad */
		y = ln_range_radians2(y);
		result += elp29[j].A * sin(y);
	}
	return result;
}


/* sum lunar elp30 series */
static double sum_series_elp30(double *t)
{
	double result = 0;
	int i,j,k;
	double y;

	for (j = 0; j < ELP30_SIZE; j++) {
		y = elp30[j].O * DEG;
		for (k = 0; k < 2; k++) {
			y += elp30[j].iz * zeta[k] * t[k];
			for (i = 0; i < 4; i++)
				y += elp30[j].ilu[i] * del[i][k] * t[k];
		}
		/* put y in correct quad */
		y = ln_range_radians2(y);
		result += elp30[j].A * sin(y);
	}
	return result;
}


/* sum lunar elp31 series */
static double sum_series_elp31(double *t)
{
	double result = 0;
	int i,j,k;
	double y;

	for (j = 0; j < ELP31_SIZE; j++) {
		y = elp31[j].O * DEG;
		for (k = 0; k < 2; k++) {
			y += elp31[j].iz * zeta[k] * t[k];
			for (i = 0; i < 4; i++)
				y += elp31[j].ilu[i] * del[i][k] * t[k];
		}
		/* put y in correct quad */
		y = ln_range_radians2(y);
		result += elp31[j].A * sin(y);
	}
	return result;
}

/* sum lunar elp32 series */
static double sum_series_elp32(double *t)
{
	double result = 0;
	int i,j,k;
	double y;

	for (j = 0; j < ELP32_SIZE; j++) {
		y = elp32[j].O * DEG;
		for (k = 0; k < 2; k++) {
			y += elp32[j].iz * zeta[k] * t[k];
			for (i = 0; i < 4; i++)
				y += elp32[j].ilu[i] * del[i][k] * t[k];
		}
		/* put y in correct quad */
		y = ln_range_radians2(y);
		result += elp32[j].A * sin(y);
	}
	return result;
}

/* sum lunar elp33 series */
static double sum_series_elp33(double *t)
{
	double result = 0;
	int i,j,k;
	double y;

	for (j = 0; j < ELP33_SIZE; j++) {
		y = elp33[j].O * DEG;
		for (k = 0; k < 2; k++) {
			y += elp33[j].iz * zeta[k] * t[k];
			for (i = 0; i < 4; i++)
				y += elp33[j].ilu[i] * del[i][k] * t[k];
		}
		/* put y in correct quad */
		y = ln_range_radians2(y);
		result += elp33[j].A * sin(y);
	}
	return result;
}

/* sum lunar elp34 series */
static double sum_series_elp34(double *t)
{
	double result = 0;
	int i,j,k;
	double y, A;

	for (j = 0; j < ELP34_SIZE; j++) {
		A = elp34[j].A * t[2];
		y = elp34[j].O * DEG;
		for (k = 0; k < 2; k++) {
			y += elp34[j].iz * zeta[k] * t[k];
			for (i = 0; i < 4; i++)
				y += elp34[j].ilu[i] * del[i][k] * t[k];
		}
		/* put y in correct quad */
		y = ln_range_radians2(y);
		result += A * sin(y);
	}
	return result;
}
/* sum lunar elp35 series */
static double sum_series_elp35(double *t)
{
	double result = 0;
	int i,j,k;
	double y, A;

	for (j = 0; j < ELP35_SIZE; j++) {
		A = elp35[j].A * t[2];
		y = elp35[j].O * DEG;
		for (k = 0; k < 2; k++) {
			y += elp35[j].iz * zeta[k] * t[k];
			for (i = 0; i < 4; i++)
				y += elp35[j].ilu[i] * del[i][k] * t[k];
		}
		/* put y in correct quad */
		y = ln_range_radians2(y);
		result += A * sin(y);
	}
	return result;
}

/* sum lunar elp36 series */
static double sum_series_elp36(double *t)
{
	double result = 0;
	int i,j,k;
	double y, A;

	for (j = 0; j < ELP36_SIZE; j++) {
		A = elp36[j].A * t[2];
		y = elp36[j].O * DEG;
		for (k = 0; k < 2; k++) {
			y += elp36[j].iz * zeta[k] * t[k];
			for (i = 0; i < 4; i++)
				y += elp36[j].ilu[i] * del[i][k] * t[k];
		}
		/* put y in correct quad */
		y = ln_range_radians2(y);
		result += A * sin(y);
	}
	return result;
}

/* internal function used for find_max/find zero lunar phase calculations */
static double lunar_phase(double jd, double *arg)
{
	struct ln_lnlat_posn moon;
	struct ln_helio_posn sol;
	double phase;

	ln_get_lunar_ecl_coords(jd, &moon, 0);
	ln_get_solar_geom_coords(jd, &sol);

	phase = fmod((ln_rad_to_deg(moon.lng - sol.L))
		+ 3.0 * M_PI - arg[0], 2.0 * M_PI) - M_PI;

	return phase;
}

/* internal function used for find_max/find zero lunar phase calculations */
static double lunar_distance(double jd, double *arg)
{
	return ln_get_lunar_earth_dist(jd);
}

/* internal function used for find_max/find zero lunar phase calculations */
static double lunar_neg_distance(double jd, double *arg)
{
	return -ln_get_lunar_earth_dist(jd);
}

/* internal function used for find_max/find zero lunar phase calculations */
static double _lunar_ecl_lat(double jd, double *arg)
{
	struct ln_lnlat_posn pos;

	ln_get_lunar_ecl_coords(jd, &pos, 0);

	return pos.lat;
}

/*! \fn void ln_get_lunar_geo_posn(double JD, struct ln_rect_posn *pos, double precision);
* \param JD Julian day.
* \param pos Pointer to a geocentric position structure to held result.
* \param precision The truncation level of the series in radians for longitude
* and latitude and in km for distance. (Valid range 0 - 0.01, 0 being highest accuracy)
* \ingroup lunar
*
* Calculate the rectangular geocentric lunar coordinates to the inertial mean
* ecliptic and equinox of J2000.
* The geocentric coordinates returned are in units of km.
*
* This function is based upon the Lunar Solution ELP2000-82B by
* Michelle Chapront-Touze and Jean Chapront of the Bureau des Longitudes,
* Paris.
*/
/* ELP 2000-82B theory */
void ln_get_lunar_geo_posn(double JD, struct ln_rect_posn *moon, double precision)
{
	double t[5];
	double elp[36];
	double a,b,c;
	double x,y,z;
	double pw,qw, pwqw, pw2, qw2, ra;

	/* calc julian centuries */
	t[0] = 1.0;
	t[1] =(JD - 2451545.0) / 36525.0;
	t[2] = t[1] * t[1];
	t[3] = t[2] * t[1];
	t[4] = t[3] * t[1];

	/* sum elp series */
	elp[0] = sum_series_elp1(t);
	elp[1] = sum_series_elp2(t);
	elp[2] = sum_series_elp3(t);
	elp[3] = sum_series_elp4(t);
	elp[4] = sum_series_elp5(t);
	elp[5] = sum_series_elp6(t);
	elp[6] = sum_series_elp7(t);
	elp[7] = sum_series_elp8(t);
	elp[8] = sum_series_elp9(t);
	elp[9] = sum_series_elp10(t);
	elp[10] = sum_series_elp11(t);
	elp[11] = sum_series_elp12(t);
	elp[12] = sum_series_elp13(t);
	elp[13] = sum_series_elp14(t);
	elp[14] = sum_series_elp15(t);
	elp[15] = sum_series_elp16(t);
	elp[16] = sum_series_elp17(t);
	elp[17] = sum_series_elp18(t);
	elp[18] = sum_series_elp19(t);
	elp[19] = sum_series_elp20(t);
	elp[20] = sum_series_elp21(t);
	elp[21] = sum_series_elp22(t);
	elp[22] = sum_series_elp23(t);
	elp[23] = sum_series_elp24(t);
	elp[24] = sum_series_elp25(t);
	elp[25] = sum_series_elp26(t);
	elp[26] = sum_series_elp27(t);
	elp[27] = sum_series_elp28(t);
	elp[28] = sum_series_elp29(t);
	elp[29] = sum_series_elp30(t);
	elp[30] = sum_series_elp31(t);
	elp[31] = sum_series_elp32(t);
	elp[32] = sum_series_elp33(t);
	elp[33] = sum_series_elp34(t);
	elp[34] = sum_series_elp35(t);
	elp[35] = sum_series_elp36(t);

	a = elp[0] + elp[3] + elp[6] + elp[9] + elp[12] +
		elp[15] + elp[18] + elp[21] + elp[24] +
		elp[27] + elp[30] + elp[33];
	b = elp[1] + elp[4] + elp[7] + elp[10] + elp[13] +
		elp[16] + elp[19] + elp[22] + elp[25] +
		elp[28] + elp[31] + elp[34];
	c = elp[2] + elp[5] + elp[8] + elp[11] + elp[14] +
		elp[17] + elp[20] + elp[23] + elp[26] +
		elp[29] + elp[32] + elp[35];

	/* calculate geocentric coords */
	a = a / RAD + W1[0] + W1[1] * t[1] + W1[2] * t[2] + W1[3] * t[3]
	    + W1[4] * t[4];
	b = b / RAD;
	c = c * A0 / ATH;

	x = c * cos(b);
	y = x * sin(a);
	x = x * cos(a);
	z = c * sin(b);

	/* Laskars series */
	pw = (P1 + P2 * t[1] + P3 * t[2] + P4 * t[3] + P5 * t[4]) * t[1];
	qw = (Q1 + Q2 * t[1] + Q3 * t[2] + Q4 * t[3] + Q5 * t[4]) * t[1];
	ra = 2.0 * sqrt(1.0 - pw * pw - qw * qw);
	pwqw = 2.0 * pw * qw;
	pw2 = 1.0 - 2.0 * pw * pw;
	qw2 = 1.0 - 2.0 * qw * qw;
	pw = pw * ra;
	qw = qw * ra;
	a = pw2 * x + pwqw * y + pw * z;
	b = pwqw * x + qw2 * y - qw * z;
	c = -pw * x + qw * y + (pw2 + qw2 - 1.0) * z;

	/* save result */
	moon->X = a;
	moon->Y = b;
	moon->Z = c;
}

/*! \fn void ln_get_lunar_equ_coords_prec(double JD, struct ln_equ_posn *position, double precision);
* \param JD Julian Day
* \param position Pointer to a struct ln_lnlat_posn to store result.
* \param precision The truncation level of the series in radians for longitude
* and latitude and in km for distance. (Valid range 0 - 0.01, 0 being highest accuracy)
* \ingroup lunar
*
* Calculate the lunar RA and DEC for Julian day JD.
* Accuracy is better than 10 arcsecs in right ascension and 4 arcsecs in declination.
*/
void ln_get_lunar_equ_coords_prec(double JD, struct ln_equ_posn *position,
	double precision)
{
	struct ln_lnlat_posn ecl;

	ln_get_lunar_ecl_coords(JD, &ecl, precision);
	ln_get_equ_from_ecl(&ecl, JD, position);
}

/*! \fn void ln_get_lunar_equ_coords(double JD, struct ln_equ_posn *position);
* \param JD Julian Day
* \param position Pointer to a struct ln_lnlat_posn to store result.
* \ingroup lunar
*
* Calculate the lunar RA and DEC for Julian day JD.
* Accuracy is better than 10 arcsecs in right ascension and 4 arcsecs in declination.
*/
void ln_get_lunar_equ_coords(double JD, struct ln_equ_posn *position)
{
	ln_get_lunar_equ_coords_prec(JD, position, 0);
}

/*! \fn void ln_get_lunar_ecl_coords(double JD, struct ln_lnlat_posn *position, double precision);
* \param JD Julian Day
* \param position Pointer to a struct ln_lnlat_posn to store result.
* \param precision The truncation level of the series in radians for longitude
* and latitude and in km for distance. (Valid range 0 - 0.01, 0 being highest accuracy)
* \ingroup lunar
*
* Calculate the lunar longitude and latitude for Julian day JD.
* Accuracy is better than 10 arcsecs in longitude and 4 arcsecs in latitude.
*/
void ln_get_lunar_ecl_coords(double JD, struct ln_lnlat_posn *position,
	double precision)
{
	struct ln_rect_posn moon;

	/* get lunar geocentric position */
	ln_get_lunar_geo_posn(JD, &moon, precision);

	/* convert to long and lat */
	position->lng = atan2(moon.Y, moon.X);
	position->lat = atan2(moon.Z,
		(sqrt((moon.X * moon.X) + (moon.Y * moon.Y))));
	position->lng = ln_range_degrees(ln_rad_to_deg(position->lng));
	position->lat = ln_rad_to_deg(position->lat);
}

/*! \fn double ln_get_lunar_earth_dist(double JD);
* \param JD Julian Day
* \return The distance between the Earth and Moon in km.
* \ingroup lunar
*
* Calculates the distance between the centre of the Earth and the
* centre of the Moon in km.
*/
double ln_get_lunar_earth_dist(double JD)
{
	struct ln_rect_posn moon;

	ln_get_lunar_geo_posn(JD, &moon, 0.00001);
	return sqrt((moon.X * moon.X) + (moon.Y * moon.Y) + (moon.Z * moon.Z));
}


/*! \fn double ln_get_lunar_phase(double JD);
* \param JD Julian Day
* \return Phase angle. (Value between 0 and 180)
* \ingroup lunar
*
* Calculates the angle Sun - Moon - Earth.
*/
double ln_get_lunar_phase(double JD)
{
	double phase = 0;
	struct ln_lnlat_posn moon, sunlp;
	double lunar_elong;
	double R, delta;

	/* get lunar and solar long + lat */
	ln_get_lunar_ecl_coords(JD, &moon, 0.0001);
	ln_get_solar_ecl_coords(JD, &sunlp);

	/* calc lunar geocentric elongation equ 48.2 */
	lunar_elong = acos(cos(ln_deg_to_rad(moon.lat)) *
		cos(ln_deg_to_rad(sunlp.lng - moon.lng)));

	/* now calc phase Equ 48.2 */
	R = ln_get_earth_solar_dist(JD);
	delta = ln_get_lunar_earth_dist(JD);
	R = R * AU; /* convert R to km */
	phase = atan2((R * sin(lunar_elong)), (delta - R * cos(lunar_elong)));
	return ln_rad_to_deg(phase);
}

/*! \fn double ln_get_lunar_disk(double JD);
* \param JD Julian Day
* \return Illuminated fraction. (Value between 0 and 1)
* \brief Calculate the illuminated fraction of the moons disk
* \ingroup lunar
*
* Calculates the illuminated fraction of the Moon's disk.
*/
double ln_get_lunar_disk(double JD)
{
	double i;

	/* Equ 48.1 */
	i = ln_deg_to_rad(ln_get_lunar_phase(JD));
	return (1.0 + cos(i)) / 2.0;
}

/*! \fn double ln_get_lunar_bright_limb(double JD);
* \param JD Julian Day
* \return The position angle in degrees.
* \brief Calculate the position angle of the Moon's bright limb.
* \ingroup lunar
*
* Calculates the position angle of the midpoint of the illuminated limb of the
* moon, reckoned eastward from the north point of the disk.
*
* The angle is near 270 deg for first quarter and near 90 deg after a full moon.
* The position angle of the cusps are +90 deg and -90 deg.
*/
double ln_get_lunar_bright_limb(double JD)
{
	double angle;
	double x,y;

	struct ln_equ_posn moon, sunlp;

	/* get lunar and solar long + lat */
	ln_get_lunar_equ_coords(JD, &moon);
	ln_get_solar_equ_coords(JD, &sunlp);

	/* Equ 48.5 */
	x = cos(ln_deg_to_rad(sunlp.dec)) * sin(ln_deg_to_rad(sunlp.ra - moon.ra));
	y = sin((ln_deg_to_rad(sunlp.dec)) * cos(ln_deg_to_rad(moon.dec)))
		- (cos(ln_deg_to_rad(sunlp.dec)) * sin(ln_deg_to_rad(moon.dec))
		* cos(ln_deg_to_rad(sunlp.ra - moon.ra)));
	angle = atan2(x,y);

	angle = ln_range_radians(angle);
	return ln_rad_to_deg(angle);
}


/*! \fn double ln_get_lunar_rst(double JD, struct ln_lnlat_posn *observer, struct ln_rst_time *rst);
* \param JD Julian day
* \param observer Observers position
* \param rst Pointer to store Rise, Set and Transit time in JD
* \return 0 for success, else 1 for circumpolar.
* \todo Improve lunar standard altitude for rst
*
* Calculate the time the rise, set and transit (crosses the local meridian at upper culmination)
* time of the Moon for the given Julian day.
*
* Note: this functions returns 1 if the Moon is circumpolar, that is it remains the whole
* day either above or below the horizon.
*/
int ln_get_lunar_rst(double JD, struct ln_lnlat_posn *observer,
	struct ln_rst_time *rst)
{
	return ln_get_body_rst_horizon(JD, observer, ln_get_lunar_equ_coords,
		LN_LUNAR_STANDART_HORIZON, rst);
}

/*! \fn double ln_get_lunar_sdiam(double JD)
* \param JD Julian day
* \return Semidiameter in arc seconds
* \todo Use Topocentric distance.
*
* Calculate the semidiameter of the Moon in arc seconds for the
* given julian day.
*/
double ln_get_lunar_sdiam(double JD)
{
	double So = 358473400;
	double dist;

	dist = ln_get_lunar_earth_dist(JD);
	return So / dist;
}

/*! \fn double ln_get_lunar_long_asc_node(double JD);
* \param JD Julian Day.
* \return Longitude of ascending node in degrees.
*
* Calculate the mean longitude of the Moons ascening node
* for the given Julian day.
*/
double ln_get_lunar_long_asc_node(double JD)
{
	/* calc julian centuries */
	double T =(JD - 2451545.0) / 36525.0;
	double omega = 125.0445479;
	double T2 = T * T;
	double T3 = T2 * T;
	double T4 = T3 * T;

	/* equ 47.7 */
	omega -= 1934.1362891 * T + 0.0020754 * T2 + T3 / 467441.0 -
		T4 / 60616000.0;
	return omega;
}


/*! \fn double ln_get_lunar_long_perigee(double JD);
* \param JD Julian Day
* \return Longitude of Moons mean perigee in degrees.
*
* Calculate the longitude of the Moon's mean perigee.
*/
double ln_get_lunar_long_perigee(double JD)
{
	/* calc julian centuries */
	double T =(JD - 2451545.0) / 36525.0;
	double per = 83.3532465;
	double T2 = T * T;
	double T3 = T2 * T;
	double T4 = T3 * T;

	/* equ 47.7 */
	per += 4069.0137287 * T - 0.0103200 * T2 -
		T3 / 80053.0 + T4 / 18999000.0;
	return per;
}

/*! \fn double ln_get_lunar_arg_latitude(double JD);
* \param JD Julian Day
* \return Moon's argument of latitude
*
* Calculate the Moon's argument of latitude (mean distance of the Moon from its ascending node)
*/
double ln_get_lunar_arg_latitude(double JD)
{
	/* calc julian centuries */
	double T =(JD - 2451545.0) / 36525.0;
	double arg = 93.2720993;
	double T2 = T * T;
	double T3 = T2 * T;
	double T4 = T3 * T;

	/* equ 45.5 */
	arg += 483202.0175273 * T - 0.0034029 * T2 -
		T3 / 3526000.0 + T4 / 863310000.0;

	return arg;
}

void ln_get_lunar_selenographic_coords(double JD, struct ln_lnlat_posn *moon,
	struct ln_lnlat_posn *position)
{
	/* equ 51.1 */
	static const double I = 0.02692030744861093755; // 1.54242 deg in radians
	double Omega = ln_get_lunar_long_asc_node(JD);
	double W = ln_deg_to_rad(moon->lng - Omega);
	double F = ln_get_lunar_arg_latitude(JD);

	double tan_Ay = sin(W) * cos(ln_deg_to_rad(moon->lat)) * cos(I) -
		sin(ln_deg_to_rad(moon->lat)) * sin(I);
	double tan_Ax = cos(W) * cos(ln_deg_to_rad(moon->lat));

	position->lng =
		ln_range_degrees(ln_rad_to_deg(atan2(tan_Ay, tan_Ax)) - F);
	position->lng =
		(position->lng > 180.0 ? position->lng - 360.0 : position->lng);
	position->lat =
		ln_rad_to_deg(asin(-sin(W) * cos(ln_deg_to_rad(moon->lat)) * sin(I) -
		sin(ln_deg_to_rad(moon->lat))*cos(I)));
}

/*! \fn void ln_get_lunar_opt_libr_coords(double JD, struct ln_lnlat_posn *position)
* \param JD Julian Day
* \param position Pointer to a struct ln_lnlat_posn to store result.
*
* Calculate Lunar optical libration coordinates, also known as selenographic Earth coordinates.
* This is a point on the surface of the Moon where the Earth is in the zenith.
*/
void ln_get_lunar_opt_libr_coords(double JD, struct ln_lnlat_posn *position)
{
	struct ln_lnlat_posn moon;
	ln_get_lunar_ecl_coords(JD, &moon, 0);
	ln_get_lunar_selenographic_coords(JD, &moon, position);
}

/*! \fn void ln_get_lunar_subsolar_coords(double JD, struct ln_lnlat_posn *position)
* \param JD Julian Day
* \param position Pointer to a struct ln_lnlat_posn to store result.
*
* Calculate coordinates of the subsolar point, aslo known as selenographic coordinates of the Sun.
* This is a point on the surface of the Moon where the Sun is in the zenith.
*/
void ln_get_lunar_subsolar_coords(double JD, struct ln_lnlat_posn *position)
{
	struct ln_lnlat_posn moon;
	struct ln_lnlat_posn sun;
	double EM_dist = ln_get_lunar_earth_dist(JD);
	double ES_dist = ln_get_earth_solar_dist(JD) * AU;
	double dist_ratio = EM_dist / ES_dist;

	ln_get_solar_ecl_coords(JD, &sun);
	ln_get_lunar_ecl_coords(JD, &moon, 0);

	moon.lng = sun.lng + 180.0 + 57.296 * dist_ratio *
		cos(ln_deg_to_rad(moon.lat)) *
		sin(ln_deg_to_rad(sun.lng - moon.lng));
	moon.lat = dist_ratio * moon.lat;

	ln_get_lunar_selenographic_coords(JD, &moon, position);
}

/*
 * Notes on phase calculations.
 * In general, exact k cannot be easily computed (Meeus chapter 46 gives
 * approximation formula) and there is no guarantee that you can compute
 * exactly k for *next* (not accidentally previous) phase (or opposite -
 * previous, not accidentally next).

 * Therefore k is firstly approximated and decreased (or increased) by 2
 * and then while loop increase this k until nd (JD of mean phase) is fisrt
 * one below or above given JD. This loop runs several times (1-3) only.
 */

/*! \fn double ln_lunar_next_phase(double jd, double phase)
* \param jd Julian Day
* \param phase 0 for new moon, 0.25 for first quarter, 0.5 for full moon, 0.75 for last quarter
*
* Find next moon phase relative to given time expressed as Julian Day.
*
*/
double ln_lunar_next_phase(double jd, double phase)
{
	double ph, k, angle;

	k = floor((jd - 2451550.09766) / 29.530588861) + phase - 2.0;

	while ((ph = 2451550.09766 + 29.530588861 * k + 0.00015437 *
		(k / 1236.85) * (k / 1236.85)) < jd)
			k += 1.0;

	angle = 2.0 * M_PI * phase;

	while ((ph = ln_find_zero(lunar_phase, ph, ph + 0.01, &angle)) < jd)
		ph += 29.530588861;

	return ph;
}

/*! \fn double ln_lunar_previous_phase(double jd, double phase)
* \param jd Julian Day
* \param phase 0 for new moon, 0.25 for first quarter, 0.5 for full moon, 0.75 for last quarter
*
* Find previous moon phase relative to given time expressed as Julian Day.
*
*/
double ln_lunar_previous_phase(double jd, double phase)
{
	double ph, k, angle;

	k = floor((jd - 2451550.09766) / 29.530588861) + phase + 2.0;

	while ((ph = 2451550.09766 + 29.530588861 * k + 0.00015437 *
		(k / 1236.85) * (k / 1236.85)) > jd)
			k -= 1.0;

	angle = 2.0 * M_PI * phase;

	while ((ph = ln_find_zero(lunar_phase, ph, ph + 0.01, &angle)) > jd)
		ph -= 29.530588861;

	return ph;
}

/*! \fn double ln_lunar_next_apsis(double jd, int mode)
* \param jd Julian Day
* \param apogee 0 for perigee, 1 for apogee
*
* Find next moon apogee or perigee relative to given time expressed as Julian Day.
*
*/
double ln_lunar_next_apsis(double jd, int apogee)
{
	double ap, k;

	k = floor((jd - 2451534.6698) / 27.55454989) + (0.5 * apogee) - 2.0;

	while ((ap = 2451534.6698 + 27.55454989 * k + 0.0006691 *
		(k / 1325.55) * (k / 1325.55)) < jd)
			k += 1.0;

	if (apogee) {
		while ((ap = ln_find_max(lunar_distance, ap - 3.0, ap + 3.0, NULL)) < jd)
			ap += 27.55454989;
	} else {
		while ((ap = ln_find_max(lunar_neg_distance, ap - 3.0, ap + 3.0, NULL)) < jd)
			ap += 27.55454989;
	}

	return ap;
}

/*! \fn double ln_lunar_previous_apsis(double jd, int mode)
* \param jd Julian Day
* \param apogee 0 for perigee, 1 for apogee
*
* Find previous moon apogee or perigee relative to given time expressed as Julian Day.
*
*/
double ln_lunar_previous_apsis(double jd, int apogee)
{
	double ap, k;

	k = floor((jd - 2451534.6698) / 27.55454989) + (0.5 * apogee) + 2.0;

	while ((ap = 2451534.6698 + 27.55454989 * k + 0.0006691 *
		(k / 1325.55) * (k / 1325.55)) > jd)
		k -= 1.0;

	if (apogee) {
		while ((ap = ln_find_max(lunar_distance, ap - 3.0, ap + 3.0, NULL)) > jd)
			ap -= 27.55454989;
	} else {
		while ((ap = ln_find_max(lunar_neg_distance, ap - 3.0, ap + 3.0, NULL)) > jd)
			ap -= 27.55454989;
	}

	return ap;
}

/*! \fn double ln_lunar_next_node(double jd, int mode)
* \param jd Julian Day
* \param mode 0 for ascending, 1 for descending
*
* Find next moon node relative to given time expressed as Julian Day.
*
*/
double ln_lunar_next_node(double jd, int mode)
{
	double nd, k;

	k = floor((jd - 2451565.1619) / 27.212220817) + (0.5 * mode) - 2.0;

	while ((nd = 2451565.1619 + 27.212220817 * k) < jd)
		k += 1.0;

	while ((nd = ln_find_zero(_lunar_ecl_lat, nd - 3.0,
		nd + 3.0, NULL)) < jd)
			nd += 27.212220817;

	return nd;
}

/*! \fn double ln_lunar_previous_node(double jd, int mode)
* \param jd Julian Day
* \param mode 0 for ascending, 1 for descending
*
* Find previous lunar node relative to given time expressed as Julian Day.
*
*/
double ln_lunar_previous_node(double jd, int mode)
{
	double nd, k;

	k = floor((jd - 2451565.1619) / 27.212220817) + (0.5 * mode) + 2.0;

	while ((nd = 2451565.1619 + 27.212220817 * k) > jd)
		k -= 1.0;

	while ((nd = ln_find_zero(_lunar_ecl_lat, nd - 3.0,
		nd + 3.0, NULL)) > jd)
			nd -= 27.212220817 ;

	return nd;
}

/*! \example lunar.c
 *
 * Examples of how to use Lunar functions.
 */
