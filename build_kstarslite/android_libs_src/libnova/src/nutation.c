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
 */

#include <math.h>
#include <libnova/nutation.h>
#include <libnova/dynamical_time.h>
#include <libnova/utility.h>

#define TERMS 63
#define LN_NUTATION_EPOCH_THRESHOLD 0.1

struct nutation_arguments {
    double D;
    double M;
    double MM;
    double F;
    double O;
};

struct nutation_coefficients {
    double longitude1;
    double longitude2;
    double obliquity1;
    double obliquity2;
};

/* arguments and coefficients taken from table 21A on page 133 */

const static struct nutation_arguments arguments[TERMS] = {
    {0.0,	0.0,	0.0,	0.0,	1.0},
    {-2.0,	0.0,	0.0,	2.0,	2.0},
    {0.0,	0.0,	0.0,	2.0,	2.0},
    {0.0,	0.0,	0.0,	0.0,	2.0},
    {0.0,	1.0,	0.0,	0.0,	0.0},
    {0.0,	0.0,	1.0,	0.0,	0.0},
    {-2.0,	1.0,	0.0,	2.0,	2.0},
    {0.0,	0.0,	0.0,	2.0,	1.0},
    {0.0,	0.0,	1.0,	2.0,	2.0},
    {-2.0,	-1.0,	0.0,	2.0,	2.0},
    {-2.0,	0.0,	1.0,	0.0,	0.0},
    {-2.0,	0.0,	0.0,	2.0,	1.0},
    {0.0,	0.0,	-1.0,	2.0,	2.0},
    {2.0,	0.0,	0.0,	0.0,	0.0},
    {0.0,	0.0,	1.0,	0.0,	1.0},
    {2.0,	0.0,	-1.0,	2.0,	2.0},
    {0.0,	0.0,	-1.0,	0.0,	1.0},
    {0.0,	0.0,	1.0,	2.0,	1.0},
    {-2.0,	0.0,	2.0,	0.0,	0.0},
    {0.0,	0.0,	-2.0,	2.0,	1.0},
    {2.0,	0.0,	0.0,	2.0,	2.0},
    {0.0,	0.0,	2.0,	2.0,	2.0},
    {0.0,	0.0,	2.0,	0.0,	0.0},
    {-2.0,	0.0,	1.0,	2.0,	2.0},
    {0.0,	0.0,	0.0,	2.0,	0.0},
    {-2.0,	0.0,	0.0,	2.0,	0.0},
    {0.0,	0.0,	-1.0,	2.0,	1.0},
    {0.0,	2.0,	0.0,	0.0,	0.0},
    {2.0,	0.0,	-1.0,	0.0,	1.0},
    {-2.0,	2.0,	0.0,	2.0,	2.0},
    {0.0,	1.0,	0.0,	0.0,	1.0},
    {-2.0,	0.0,	1.0,	0.0,	1.0},
    {0.0,	-1.0,	0.0,	0.0,	1.0},
    {0.0,	0.0,	2.0,	-2.0,	0.0},
    {2.0,	0.0,	-1.0,	2.0,	1.0},
    {2.0,	0.0,	1.0,	2.0,	2.0},
    {0.0,	1.0,	0.0,	2.0,	2.0},
    {-2.0,	1.0,	1.0,	0.0,	0.0},
    {0.0,	-1.0,	0.0,	2.0,	2.0},
    {2.0,	0.0,	0.0,	2.0,	1.0},
    {2.0,	0.0,	1.0,	0.0,	0.0},
    {-2.0,	0.0,	2.0,	2.0,	2.0},
    {-2.0,	0.0,	1.0,	2.0,	1.0},
    {2.0,	0.0,	-2.0,	0.0,	1.0},
    {2.0,	0.0,	0.0,	0.0,	1.0},
    {0.0,	-1.0,	1.0,	0.0,	0.0},
    {-2.0,	-1.0,	0.0,	2.0,	1.0},
    {-2.0,	0.0,	0.0,	0.0,	1.0},
    {0.0,	0.0,	2.0,	2.0,	1.0},
    {-2.0,	0.0,	2.0,	0.0,	1.0},
    {-2.0,	1.0,	0.0,	2.0,	1.0},
    {0.0,	0.0,	1.0,	-2.0,	0.0},
    {-1.0,	0.0,	1.0,	0.0,	0.0},
    {-2.0,	1.0,	0.0,	0.0,	0.0},
    {1.0,	0.0,	0.0,	0.0,	0.0},
    {0.0,	0.0,	1.0,	2.0,	0.0},
    {0.0,	0.0,	-2.0,	2.0,	2.0},
    {-1.0,	-1.0,	1.0,	0.0,	0.0},
    {0.0,	1.0,	1.0,	0.0,	0.0},
    {0.0,	-1.0,	1.0,	2.0,	2.0},
    {2.0,	-1.0,	-1.0,	2.0,	2.0},
    {0.0,	0.0,	3.0,	2.0,	2.0},
    {2.0,	-1.0,	0.0,	2.0,	2.0}};

const static struct nutation_coefficients coefficients[TERMS] = {
    {-171996.0,	-174.2,	92025.0,8.9},
    {-13187.0,	-1.6,  	5736.0,	-3.1},
    {-2274.0, 	-0.2,  	977.0,	-0.5},
    {2062.0,   	0.2,    -895.0,    0.5},
    {1426.0,    -3.4,    54.0,    -0.1},
    {712.0,    0.1,    -7.0,    0.0},
    {-517.0,    1.2,    224.0,    -0.6},
    {-386.0,    -0.4,    200.0,    0.0},
    {-301.0,    0.0,    129.0,    -0.1},
    {217.0,    -0.5,    -95.0,    0.3},
    {-158.0,    0.0,    0.0,    0.0},
    {129.0,	0.1,	-70.0,	0.0},
    {123.0,	0.0,	-53.0,	0.0},
    {63.0,	0.0,	0.0,	0.0},
    {63.0,	0.1,	-33.0,	0.0},
    {-59.0,	0.0,	26.0,	0.0},
    {-58.0,	-0.1,	32.0,	0.0},
    {-51.0,	0.0,	27.0,	0.0},
    {48.0,	0.0,	0.0,	0.0},
    {46.0,	0.0,	-24.0,	0.0},
    {-38.0,	0.0,	16.0,	0.0},
    {-31.0,	0.0,	13.0,	0.0},
    {29.0,	0.0,	0.0,	0.0},
    {29.0,	0.0,	-12.0,	0.0},
    {26.0,	0.0,	0.0,	0.0},
    {-22.0,	0.0,	0.0,	0.0},
    {21.0,	0.0,	-10.0,	0.0},
    {17.0,	-0.1,	0.0,	0.0},
    {16.0,	0.0,	-8.0,	0.0},
    {-16.0,	0.1,	7.0,	0.0},
    {-15.0,	0.0,	9.0,	0.0},
    {-13.0,	0.0,	7.0,	0.0},
    {-12.0,	0.0,	6.0,	0.0},
    {11.0,	0.0,	0.0,	0.0},
    {-10.0,	0.0,	5.0,	0.0},
    {-8.0,	0.0,	3.0,	0.0},
    {7.0,	0.0,	-3.0,	0.0},
    {-7.0,	0.0,	0.0,	0.0},
    {-7.0,	0.0,	3.0,	0.0},
    {-7.0,	0.0,	3.0,	0.0},
    {6.0,	0.0,	0.0,	0.0},
    {6.0,	0.0,	-3.0,	0.0},
    {6.0,	0.0,	-3.0,	0.0},
    {-6.0,	0.0,	3.0,	0.0},
    {-6.0,	0.0,	3.0,	0.0},
    {5.0,	0.0,	0.0,	0.0},
    {-5.0,	0.0,	3.0,	0.0},
    {-5.0,	0.0,	3.0,	0.0},
    {-5.0,	0.0,	3.0,	0.0},
    {4.0,	0.0,	0.0,	0.0},
    {4.0,	0.0,	0.0,	0.0},
    {4.0,	0.0,	0.0,	0.0},
    {-4.0,	0.0,	0.0,	0.0},
    {-4.0,	0.0,	0.0,	0.0},
    {-4.0,	0.0,	0.0,	0.0},
    {3.0,	0.0,	0.0,	0.0},
    {-3.0,	0.0,	0.0,	0.0},
    {-3.0,	0.0,	0.0,	0.0},
    {-3.0,	0.0,	0.0,	0.0},
    {-3.0,	0.0,	0.0,	0.0},
    {-3.0,	0.0,	0.0,	0.0},
    {-3.0,	0.0,	0.0,	0.0},
    {-3.0,	0.0,	0.0,	0.0}};

/* cache values */
static long double c_JD = 0.0, c_longitude = 0.0, c_obliquity = 0.0,
	c_ecliptic = 0.0;

	
/*! \fn void ln_get_nutation(double JD, struct ln_nutation *nutation)
* \param JD Julian Day.
* \param nutation Pointer to store nutation
*
* Calculate nutation of longitude and obliquity in degrees from Julian Ephemeris Day 
*/
/* Chapter 21 pg 131-134 Using Table 21A 
*/
/* TODO: add argument to specify this */
/* TODO: use JD or JDE. confirm */
void ln_get_nutation(double JD, struct ln_nutation *nutation)
{
	
	long double D, M, MM, F, O, T, T2, T3, JDE;
	long double coeff_sine, coeff_cos;
	long double argument;
	int i;

	/* should we bother recalculating nutation */
	if (fabs(JD - c_JD) > LN_NUTATION_EPOCH_THRESHOLD) {
		/* set the new epoch */
		c_JD = JD;
		c_longitude = 0;
		c_obliquity = 0;

		/* get julian ephemeris day */
		JDE = (long double)ln_get_jde(JD);
		
		/* calc T */
		T = (JDE - 2451545.0) / 36525.0;
		T2 = T * T;
		T3 = T2 * T;

		/* calculate D,M,M',F and Omega */
		D = 297.85036 + 445267.111480 * T - 0.0019142 * T2 + T3 / 189474.0;
		M = 357.52772 + 35999.050340 * T - 0.0001603 * T2 - T3 / 300000.0;
		MM = 134.96298 + 477198.867398 * T + 0.0086972 * T2 + T3 / 56250.0;
		F = 93.2719100 + 483202.017538 * T - 0.0036825 * T2 + T3 / 327270.0;
		O = 125.04452 - 1934.136261 * T + 0.0020708 * T2 + T3 / 450000.0;

		/* convert to radians */
		D = ln_deg_to_rad(D);
		M = ln_deg_to_rad(M);
		MM = ln_deg_to_rad(MM);
		F = ln_deg_to_rad(F);
		O = ln_deg_to_rad(O);

		/* calc sum of terms in table 21A */
		for (i = 0; i < TERMS; i++) {
			/* calc coefficients of sine and cosine */
			coeff_sine = (coefficients[i].longitude1 +
				(coefficients[i].longitude2 * T));
			coeff_cos = (coefficients[i].obliquity1 +
				(coefficients[i].obliquity2 * T));

			argument = arguments[i].D * D 
				+ arguments[i].M * M 
				+ arguments[i].MM * MM 
				+ arguments[i].F * F
				+ arguments[i].O * O;
            
			c_longitude += coeff_sine * sinl(argument);
			c_obliquity += coeff_cos * cosl(argument);
		}

		/* change to arcsecs */
		c_longitude /= 10000.0;
		c_obliquity /= 10000.0;

		/* change to degrees */
		c_longitude /= (60.0 * 60.0);
		c_obliquity /= (60.0 * 60.0);
		
		/* calculate mean ecliptic - Meeus 2nd edition, eq. 22.2 */
		c_ecliptic = 23.0 + 26.0 / 60.0 + 21.448 / 3600.0
                   - 46.8150 / 3600.0 * T
                   - 0.00059 / 3600.0 * T2
                   + 0.001813 / 3600.0 * T3;

		/* c_ecliptic += c_obliquity; * Uncomment this if function should 
                                         return true obliquity rather than
                                         mean obliquity */
	}

	/* return results */
	nutation->longitude = c_longitude;
	nutation->obliquity = c_obliquity;
	nutation->ecliptic = c_ecliptic;
}

/*! \fn void ln_get_equ_nut(struct ln_equ_posn *mean_position, double JD, struct ln_equ_posn *position)
* \param mean_position Mean position of object
* \param JD Julian Day.
* \param position Pointer to store new object position.
*
* Calculate a stars equatorial coordinates from it's mean equatorial coordinates
* with the effects of nutation for a given Julian Day.
*/
/* Equ 22.1
*/
void ln_get_equ_nut(struct ln_equ_posn *mean_position, double JD,
	struct ln_equ_posn *position)
{
	struct ln_nutation nut;
	ln_get_nutation (JD, &nut);

	long double mean_ra, mean_dec, delta_ra, delta_dec;

	mean_ra = ln_deg_to_rad(mean_position->ra);
	mean_dec = ln_deg_to_rad(mean_position->dec);

	// Equ 22.1

	long double nut_ecliptic = ln_deg_to_rad(nut.ecliptic + nut.obliquity);
	long double sin_ecliptic = sin(nut_ecliptic);

	long double sin_ra = sin(mean_ra);
	long double cos_ra = cos(mean_ra);

	long double tan_dec = tan(mean_dec);

	delta_ra = (cos (nut_ecliptic) + sin_ecliptic * sin_ra * tan_dec) * nut.longitude - cos_ra * tan_dec * nut.obliquity;
	delta_dec = (sin_ecliptic * cos_ra) * nut.longitude + sin_ra * nut.obliquity;

	position->ra = mean_position->ra + delta_ra;
	position->dec = mean_position->dec + delta_dec;
}
