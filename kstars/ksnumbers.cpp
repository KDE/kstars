/*
    SPDX-FileCopyrightText: 2002-2005 Jason Harris <kstars@30doradus.org>
    SPDX-FileCopyrightText: 2004-2005 Pablo de Vicente <p.devicente@wanadoo.es>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "ksnumbers.h"

#include "kstarsdatetime.h" //for J2000 define

// 63 elements
const int KSNumbers::arguments[NUTTERMS][5] = {
    { 0, 0, 0, 0, 1 },   { -2, 0, 0, 2, 2 },  { 0, 0, 0, 2, 2 },   { 0, 0, 0, 0, 2 },  { 0, 1, 0, 0, 0 },
    { 0, 0, 1, 0, 0 },   { -2, 1, 0, 2, 2 },  { 0, 0, 0, 2, 1 },   { 0, 0, 1, 2, 2 },  { -2, -1, 0, 2, 2 },
    { -2, 0, 1, 0, 0 },  { -2, 0, 0, 2, 1 },  { 0, 0, -1, 2, 2 },  { 2, 0, 0, 0, 0 },  { 0, 0, 1, 0, 1 },
    { 2, 0, -1, 2, 2 },  { 0, 0, -1, 0, 1 },  { 0, 0, 1, 2, 1 },   { -2, 0, 2, 0, 0 }, { 0, 0, -2, 2, 1 },
    { 2, 0, 0, 2, 2 },   { 0, 0, 2, 2, 2 },   { 0, 0, 2, 0, 0 },   { -2, 0, 1, 2, 2 }, { 0, 0, 0, 2, 0 },
    { -2, 0, 0, 2, 0 },  { 0, 0, -1, 2, 1 },  { 0, 2, 0, 0, 0 },   { 2, 0, -1, 0, 1 }, { -2, 2, 0, 2, 2 },
    { 0, 1, 0, 0, 1 },   { -2, 0, 1, 0, 1 },  { 0, -1, 0, 0, 1 },  { 0, 0, 2, -2, 0 }, { 2, 0, -1, 2, 1 },
    { 2, 0, 1, 2, 2 },   { 0, 1, 0, 2, 2 },   { -2, 1, 1, 0, 0 },  { 0, -1, 0, 2, 2 }, { 2, 0, 0, 2, 1 },
    { 2, 0, 1, 0, 0 },   { -2, 0, 2, 2, 2 },  { -2, 0, 1, 2, 1 },  { 2, 0, -2, 0, 1 }, { 2, 0, 0, 0, 1 },
    { 0, -1, 1, 0, 0 },  { -2, -1, 0, 2, 1 }, { -2, 0, 0, 0, 1 },  { 0, 0, 2, 2, 1 },  { -2, 0, 2, 0, 1 },
    { -2, 1, 0, 2, 1 },  { 0, 0, 1, -2, 0 },  { -1, 0, 1, 0, 0 },  { -2, 1, 0, 0, 0 }, { 1, 0, 0, 0, 0 },
    { 0, 0, 1, 2, 0 },   { 0, 0, -2, 2, 2 },  { -1, -1, 1, 0, 0 }, { 0, 1, 1, 0, 0 },  { 0, -1, 1, 2, 2 },
    { 2, -1, -1, 2, 2 }, { 0, 0, 3, 2, 2 },   { 2, -1, 0, 2, 2 }
};

const int KSNumbers::amp[NUTTERMS][4] = { { -171996, -1742, 92025, 89 },
                                          { -13187, -16, 5736, -31 },
                                          { -2274, -2, 977, -5 },
                                          { 2062, 2, -895, 5 },
                                          { 1426, -34, 54, -1 },
                                          { 712, 1, -7, 0 },
                                          { -517, 12, 224, -6 },
                                          { -386, -4, 200, 0 },
                                          { -301, 0, 129, -1 },
                                          { 217, -5, -95, 3 },
                                          { -158, 0, 0, 0 },
                                          { 129, 1, -70, 0 },
                                          { 123, 0, -53, 0 },
                                          { 63, 0, 0, 0 },
                                          { 63, 1, -33, 0 },
                                          { -59, 0, 26, 0 },
                                          { -58, -1, 32, 0 },
                                          { -51, 0, 27, 0 },
                                          { 48, 0, 0, 0 },
                                          { 46, 0, -24, 0 },
                                          { -38, 0, 16, 0 },
                                          { -31, 0, 13, 0 },
                                          { 29, 0, 0, 0 },
                                          { 29, 0, -12, 0 },
                                          { 26, 0, 0, 0 },
                                          { -22, 0, 0, 0 },
                                          { 21, 0, -10, 0 },
                                          { 17, -1, 0, 0 },
                                          { 16, 0, -8, 0 },
                                          { -16, 1, 7, 0 },
                                          { -15, 0, 9, 0 },
                                          { -13, 0, 7, 0 },
                                          { -12, 0, 6, 0 },
                                          { 11, 0, 0, 0 },
                                          { -10, 0, 5, 0 },
                                          { -8, 0, 3, 0 },
                                          { 7, 0, -3, 0 },
                                          { -7, 0, 0, 0 },
                                          { -7, 0, 3, 0 },
                                          { -7, 0, 3, 0 },
                                          { 6, 0, 0, 0 },
                                          { 6, 0, -3, 0 },
                                          { 6, 0, -3, 0 },
                                          { -6, 0, 3, 0 },
                                          { -6, 0, 3, 0 },
                                          { 5, 0, 0, 0 },
                                          { -5, 0, 3, 0 },
                                          { -5, 0, 3, 0 },
                                          { -5, 0, 3, 0 },
                                          { 4, 0, 0, 0 },
                                          { 4, 0, 0, 0 },
                                          { 4, 0, 0, 0 },
                                          { -4, 0, 0, 0 },
                                          { -4, 0, 0, 0 },
                                          { -4, 0, 0, 0 },
                                          { 3, 0, 0, 0 },
                                          { -3, 0, 0, 0 },
                                          { -3, 0, 0, 0 },
                                          { -3, 0, 0, 0 },
                                          { -3, 0, 0, 0 },
                                          { -3, 0, 0, 0 },
                                          { -3, 0, 0, 0 },
                                          { -3, 0, 0, 0 } };

KSNumbers::KSNumbers(long double jd)
{
    K.setD(20.49552 / 3600.); //set the constant of aberration

    // ecliptic longitude of earth's perihelion, source: https://nssdc.gsfc.nasa.gov/planetary/factsheet/earthfact.html; FIXME: We should correct this, as it changes with time. See the commit log for an order of magnitude estimate of the error.
    // FIXME: FIXME above seems to have been addressed? What is deltaEcLong? -- asimha
    P.setD(102.94719);

    // JM 2017-09-04: Yes the longitude of the perihelion changes with time. The value below is the one for 04-09-2017 obtained from JPL Horizons system.
    // But some Earth orbital elements MUST be updated periodically like any other solar system body.
    // This is an important FIXME!
    // deltaEcLong is used in nutation calculations. Can P be obtained computationally?
    //P.setD(104.8089842092676);

    computeConstantValues();
    updateValues(jd);
}

void KSNumbers::computeConstantValues()
{
    // Compute those numbers that need to be computed only
    // once.
    //
    // Ideally, these should be computed at compile-time. When we
    // upgrade to C++11, we can make this function static and
    // constexpr (maybe?) But even otherwise, the overhead is very
    // negligible.

    //Compute Precession Matrices from B1950 to 1984 using Newcomb formulae
    XB.setD(0.217697);
    YB.setD(0.189274);
    ZB.setD(0.217722);

    XB.SinCos(SXB, CXB);
    YB.SinCos(SYB, CYB);
    ZB.SinCos(SZB, CZB);

    //P1B is used to precess from 1984 to B1950:

    P1B(0, 0) = CXB * CYB * CZB - SXB * SZB;
    P1B(1, 0) = CXB * CYB * SZB + SXB * CZB;
    P1B(2, 0) = CXB * SYB;
    P1B(0, 1) = -1.0 * SXB * CYB * CZB - CXB * SZB;
    P1B(1, 1) = -1.0 * SXB * CYB * SZB + CXB * CZB;
    P1B(2, 1) = -1.0 * SXB * SYB;
    P1B(0, 2) = -1.0 * SYB * CZB;
    P1B(1, 2) = -1.0 * SYB * SZB;
    P1B(2, 2) = CYB;

    //P2 is used to precess from B1950 to 1984 (it is the transpose of P1)
    // FIXME: This can be optimized by taking the transpose of P1 instead of recomputing it from scratch
    P2B(0, 0) = CXB * CYB * CZB - SXB * SZB;
    P2B(1, 0) = -1.0 * SXB * CYB * CZB - CXB * SZB;
    P2B(2, 0) = -1.0 * SYB * CZB;
    P2B(0, 1) = CXB * CYB * SZB + SXB * CZB;
    P2B(1, 1) = -1.0 * SXB * CYB * SZB + CXB * CZB;
    P2B(2, 1) = -1.0 * SYB * SZB;
    P2B(0, 2) = CXB * SYB;
    P2B(1, 2) = -1.0 * SXB * SYB;
    P2B(2, 2) = CYB;
}

void KSNumbers::updateValues(long double jd)
{
    dms arg;
    double args, argc;

    days = jd;

    // FIXME: What is the source for these algorithms / polynomials / numbers? -- asimha

    //Julian Centuries since J2000.0
    T = (jd - J2000) / 36525.;

    // Julian Millenia since J2000.0
    jm = T / 10.0;

    double T2 = T * T;
    double T3 = T2 * T;

    //Sun's Mean Longitude
    L.setD(280.46645 + 36000.76983 * T + 0.0003032 * T2);

    //Mean elongation of the Moon from the Sun
    D.setD(297.85036 + 445267.111480 * T - 0.0019142 * T2 + T3 / 189474.);

    //Sun's Mean Anomaly
    M.setD(357.52910 + 35999.05030 * T - 0.0001559 * T2 - 0.00000048 * T3);

    //Moon's Mean Anomaly
    MM.setD(134.96298 + 477198.867398 * T + 0.0086972 * T2 + T3 / 56250.0);

    //Moon's Mean Longitude
    LM.setD(218.3164591 + 481267.88134236 * T - 0.0013268 * T2 + T3 / 538841. - T * T * T * T / 6519400.);

    //Moon's argument of latitude
    F.setD(93.27191 + 483202.017538 * T - 0.0036825 * T2 + T3 / 327270.);

    //Longitude of Moon's Ascending Node
    O.setD(125.04452 - 1934.136261 * T + 0.0020708 * T2 + T3 / 450000.0);

    //Earth's orbital eccentricity
    e = 0.016708617 - 0.000042037 * T - 0.0000001236 * T2;

    double C = (1.914600 - 0.004817 * T - 0.000014 * T2) * sin(M.radians()) +
               (0.019993 - 0.000101 * T) * sin(2.0 * M.radians()) + 0.000290 * sin(3.0 * M.radians());

    //Sun's True Longitude
    L0.setD(L.Degrees() + C);

    //Sun's True Anomaly
    M0.setD(M.Degrees() + C);

    //Obliquity of the Ecliptic
    // FIXME: This can be optimized by using the fact that U^3 = U^2 * U, so we can reduce the number of multiplications
    double U      = T / 100.0;
    double dObliq = -4680.93 * U - 1.55 * U * U + 1999.25 * U * U * U - 51.38 * U * U * U * U -
                    249.67 * U * U * U * U * U - 39.05 * U * U * U * U * U * U + 7.12 * U * U * U * U * U * U * U +
                    27.87 * U * U * U * U * U * U * U * U + 5.79 * U * U * U * U * U * U * U * U * U +
                    2.45 * U * U * U * U * U * U * U * U * U * U;
    Obliquity.setD(23.43929111 + dObliq / 3600.0);

    //Nutation parameters
    dms L2, M2, O2;
    double sin2L, cos2L, sin2M, cos2M;
    double sinO, cosO, sin2O, cos2O;

    O2.setD(2.0 * O.Degrees());
    L2.setD(2.0 * L.Degrees());  //twice mean ecl. long. of Sun
    M2.setD(2.0 * LM.Degrees()); //twice mean ecl. long. of Moon

    O.SinCos(sinO, cosO);
    O2.SinCos(sin2O, cos2O);
    L2.SinCos(sin2L, cos2L);
    M2.SinCos(sin2M, cos2M);

    //	deltaEcLong = ( -17.2*sinO - 1.32*sin2L - 0.23*sin2M + 0.21*sin2O)/3600.0; //Ecl. long. correction
    //	deltaObliquity = ( 9.2*cosO + 0.57*cos2L + 0.10*cos2M - 0.09*cos2O)/3600.0; //Obliq. correction

    deltaEcLong    = 0.;
    deltaObliquity = 0.;

    for (unsigned int i = 0; i < NUTTERMS; i++)
    {
        arg.setD(arguments[i][0] * D.Degrees() + arguments[i][1] * M.Degrees() + arguments[i][2] * MM.Degrees() +
                 arguments[i][3] * F.Degrees() + arguments[i][4] * O.Degrees());
        arg.SinCos(args, argc);

        deltaEcLong += (amp[i][0] + amp[i][1] / 10. * T) * args * 1e-4;
        deltaObliquity += (amp[i][2] + amp[i][3] / 10. * T) * argc * 1e-4;
    }

    deltaEcLong /= 3600.0;
    deltaObliquity /= 3600.0;

    //Compute Precession Matrices:
    XP.setD(0.6406161 * T + 0.0000839 * T2 + 0.0000050 * T3);
    YP.setD(0.5567530 * T - 0.0001185 * T2 - 0.0000116 * T3);
    ZP.setD(0.6406161 * T + 0.0003041 * T2 + 0.0000051 * T3);

    XP.SinCos(SX, CX);
    YP.SinCos(SY, CY);
    ZP.SinCos(SZ, CZ);

    //P1 is used to precess from any epoch to J2000
    // Note: P1 is a rotation matrix, and P2 is its transpose = inverse (also a rotation matrix)
    P1(0, 0) = CX * CY * CZ - SX * SZ;
    P1(1, 0) = CX * CY * SZ + SX * CZ;
    P1(2, 0) = CX * SY;
    P1(0, 1) = -1.0 * SX * CY * CZ - CX * SZ;
    P1(1, 1) = -1.0 * SX * CY * SZ + CX * CZ;
    P1(2, 1) = -1.0 * SX * SY;
    P1(0, 2) = -1.0 * SY * CZ;
    P1(1, 2) = -1.0 * SY * SZ;
    P1(2, 2) = CY;

    //P2 is used to precess from J2000 to any other epoch (it is the transpose of P1)
    // FIXME: More optimization -- just use P1(j, i) instead of P2(i, j) in code
    P2(0, 0) = P1(0, 0);
    P2(1, 0) = P1(0, 1);
    P2(2, 0) = P1(0, 2);
    P2(0, 1) = P1(1, 0);
    P2(1, 1) = P1(1, 1);
    P2(2, 1) = P1(1, 2);
    P2(0, 2) = P1(2, 0);
    P2(1, 2) = P1(2, 1);
    P2(2, 2) = P1(2, 2);

    // Mean longitudes for the planets. radians
    //

    // TODO Pasar a grados [Google Translate says "Jump to Degrees". --asimha]
    double LVenus   = 3.1761467 + 1021.3285546 * T; // Venus
    double LMars    = 1.7534703 + 628.3075849 * T;  // Mars
    double LEarth   = 6.2034809 + 334.0612431 * T;  // Earth
    double LJupiter = 0.5995465 + 52.9690965 * T;   // Jupiter
    double LSaturn  = 0.8740168 + 21.3299095 * T;   // Saturn
    double LNeptune = 5.3118863 + 3.8133036 * T;    // Neptune
    double LUranus  = 5.4812939 + 7.4781599 * T;    // Uranus

    double LMRad = 3.8103444 + 8399.6847337 * T; // Moon
    double DRad  = 5.1984667 + 7771.3771486 * T;
    double MMRad = 2.3555559 + 8328.6914289 * T; // Moon
    double FRad  = 1.6279052 + 8433.4661601 * T;

    /** Contributions to the velocity of the Earth referred to the barycenter of the solar system
        in the J2000 equatorial system
        Velocities 10^{-8} AU/day
        Ron & Vondrak method
    **/

    double vondrak[36][7] = {
        { LMars, -1719914 - 2 * T, -25, 25 - 13 * T, 1578089 + 156 * T, 10 + 32 * T, 684185 - 358 * T },
        { 2 * LMars, 6434 + 141 * T, 28007 - 107 * T, 25697 - 95 * T, -5904 - 130 * T, 11141 - 48 * T, -2559 - 55 * T },
        { LJupiter, 715, 0, 6, -657, -15, -282 },
        { LMRad, 715, 0, 0, -656, 0, -285 },
        { 3 * LMars, 486 - 5 * T, -236 - 4 * T, -216 - 4 * T, -446 + 5 * T, -94, -193 },
        { LSaturn, 159, 0, 2, -147, -6, -61 },
        { FRad, 0, 0, 0, 26, 0, -59 },
        { LMRad + MMRad, 39, 0, 0, -36, 0, -16 },
        { 2 * LJupiter, 33, -10, -9, -30, -5, -13 },
        { 2 * LMars - LJupiter, 31, 1, 1, -28, 0, -12 },
        { 3 * LMars - 8 * LEarth + 3 * LJupiter, 8, -28, 25, 8, 11, 3 },
        { 5 * LMars - 8 * LEarth + 3 * LJupiter, 8, -28, -25, -8, -11, -3 },
        { 2 * LVenus - LMars, 21, 0, 0, -19, 0, -8 },
        { LVenus, -19, 0, 0, 17, 0, 8 },
        { LNeptune, 17, 0, 0, -16, 0, -7 },
        { LMars - 2 * LJupiter, 16, 0, 0, 15, 1, 7 },
        { LUranus, 16, 0, 1, -15, -3, -6 },
        { LMars + LJupiter, 11, -1, -1, -10, -1, -5 },
        { 2 * LVenus - 2 * LMars, 0, -11, -10, 0, -4, 0 },
        { LMars - LJupiter, -11, -2, -2, 9, -1, 4 },
        { 4 * LMars, -7, -8, -8, 6, -3, 3 },
        { 3 * LMars - 2 * LJupiter, -10, 0, 0, 9, 0, 4 },
        { LVenus - 2 * LMars, -9, 0, 0, -9, 0, -4 },
        { 2 * LVenus - 3 * LMars, -9, 0, 0, -8, 0, -4 },
        { 2 * LSaturn, 0, -9, -8, 0, -3, 0 },
        { 2 * LVenus - 4 * LMars, 0, -9, 8, 0, 3, 0 },
        { 3 * LMars - 2 * LEarth, 8, 0, 0, -8, 0, -3 },
        { LMRad + 2 * DRad - MMRad, 8, 0, 0, -7, 0, -3 },
        { 8 * LVenus - 12 * LMars, -4, -7, -6, 4, -3, 2 },
        { 8 * LVenus - 14 * LMars, -4, -7, 6, -4, 3, -2 },
        { 2 * LEarth, -6, -5, -4, 5, -2, 2 },
        { 3 * LVenus - 4 * LMars, -1, -1, -2, -7, 1, -4 },
        { 2 * LMars - 2 * LJupiter, 4, -6, -5, -4, -2, -2 },
        { 3 * LVenus - 3 * LMars, 0, -7, -6, 0, -3, 0 },
        { 2 * LMars - 2 * LEarth, 5, -5, -4, -5, -2, -2 },
        { LMRad - 2 * DRad, 5, 0, 0, -5, 0, -2 }
    };

    dms anglev;
    double sa, ca;
    // Vearth X component
    vearth[0] = 0.;
    // Vearth Y component
    vearth[1] = 0.;
    // Vearth Z component
    vearth[2] = 0.;

    for (auto &item : vondrak)
    {
        anglev.setRadians(item[0]);
        anglev.SinCos(sa, ca);
        for (unsigned int j = 0; j < 3; j++)
        {
            vearth[j] += item[2 * j + 1] * sa + item[2 * j + 2] * ca;
        }
    }

    const double UA2km = 1.49597870 / 86400.; // 10^{-8}*UA/dia -> km/s

    for (double &item : vearth)
    {
        item *= UA2km;
    }
}
