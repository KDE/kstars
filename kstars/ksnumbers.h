/*
    SPDX-FileCopyrightText: 2002-2005 Jason Harris <kstars@30doradus.org>
    SPDX-FileCopyrightText: 2004-2005 Pablo de Vicente <p.devicente@wanadoo.es>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "cachingdms.h"

#if __GNUC__ > 5
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wignored-attributes"
#endif
#if __GNUC__ > 6
#pragma GCC diagnostic ignored "-Wint-in-bool-context"
#endif
#include <Eigen/Core>
#if __GNUC__ > 5
#pragma GCC diagnostic pop
#endif

#define NUTTERMS 63

/** @class KSNumbers
	*
	*There are several time-dependent values used in position calculations,
	*that are not specific to an object.  This class provides
	*storage for these values, and methods for calculating them for a given date.
	*The numbers include solar data like the true/mean solar anomalies
	*and longitudes, the longitude of the Earth's perihelion, the
	*eccentricity of Earth's orbit, the
	*constant of aberration, the obliquity of the Ecliptic, the effects of
	*Nutation (delta Obliquity and delta Ecliptic longitude),
	*the Julian Day/Century/Millenium, and arrays for computing the precession.
	*@short Store several time-dependent astronomical quantities.
	*@author Jason Harris
	*@version 1.0
	*/

class KSNumbers
{
  public:
    /**
     * Constructor.
     * @param jd  Julian Day for which the new instance is initialized
     */
    explicit KSNumbers(long double jd);
    ~KSNumbers() = default;

    /**
     * @return the current Obliquity (the angle of inclination between
     * the celestial equator and the ecliptic)
     */
    inline const CachingDms *obliquity() const { return &Obliquity; }

    /** @return the constant of aberration (20.49 arcsec). */
    inline dms constAberr() const { return K; }

    /** @return the mean solar anomaly. */
    inline dms sunMeanAnomaly() const { return M; }

    /** @return the mean solar longitude. */
    inline dms sunMeanLongitude() const { return L; }

    /** @return the true solar anomaly. */
    inline dms sunTrueAnomaly() const { return M0; }

    /** @return the true solar longitude. */
    inline CachingDms sunTrueLongitude() const { return L0; }

    /** @return the longitude of the Earth's perihelion point. */
    inline CachingDms earthPerihelionLongitude() const { return P; }

    /** @return eccentricity of Earth's orbit.*/
    inline double earthEccentricity() const { return e; }

    /** @return the change in obliquity due to the nutation of
         * Earth's orbit. Value is in degrees */
    inline double dObliq() const { return deltaObliquity; }

    /** @return the change in Ecliptic Longitude due to nutation.
         * Value is in degrees. */
    inline double dEcLong() const { return deltaEcLong; }

    /** @return Julian centuries since J2000*/
    inline double julianCenturies() const { return T; }

    /** @return Julian Day*/
    inline long double julianDay() const { return days; }

    /** @return Julian Millenia since J2000*/
    inline double julianMillenia() const { return jm; }

    /** @return element of P1 precession array at position (i1, i2) */
    inline double p1(int i1, int i2) const { return P1(i1, i2); }

    /** @return element of P2 precession array at position (i1, i2) */
    inline double p2(int i1, int i2) const { return P2(i1, i2); }

    /** @return element of P1B precession array at position (i1, i2) */
    inline double p1b(int i1, int i2) const { return P1B(i1, i2); }

    /** @return element of P2B precession array at position (i1, i2) */
    inline double p2b(int i1, int i2) const { return P2B(i1, i2); }

    /** @return the precession matrix directly **/
    inline const Eigen::Matrix3d &p2() const { return P1; }
    inline const Eigen::Matrix3d &p1() const { return P2; }
    inline const Eigen::Matrix3d &p1b() const { return P1B; }
    inline const Eigen::Matrix3d &p2b() const { return P2B; }

    /**
     * @short compute constant values that need to be computed only once per instance of the application
     */
    void computeConstantValues();

    /**
     * @short update all values for the date given as an argument.
     * @param jd the Julian date for which to compute values
     */
    void updateValues(long double jd);

    /**
     * @return the JD for which these values hold (i.e. the last updated JD)
     */
    inline long double getJD() const { return days; }

    inline double vEarth(int i) const { return vearth[i]; }

  private:
    CachingDms Obliquity, L0, P;
    dms K, L, LM, M, M0, O, D, MM, F;
    dms XP, YP, ZP, XB, YB, ZB;
    double CX, SX, CY, SY, CZ, SZ;
    double CXB, SXB, CYB, SYB, CZB, SZB;
    Eigen::Matrix3d P1, P2, P1B, P2B;
    double deltaObliquity, deltaEcLong;
    double e, T;
    long double days; // JD for which the last update was called
    double jm;
    static const int arguments[NUTTERMS][5];
    static const int amp[NUTTERMS][4];
    double vearth[3];
};
