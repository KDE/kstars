/*
    SPDX-FileCopyrightText: 2002 Jason Harris <kstars@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "jupitermoons.h"

#include "ksnumbers.h"
#include "ksplanetbase.h"
#include "kssun.h"
#include "trailobject.h"

JupiterMoons::JupiterMoons()
{
    //Initialize the Moon objects.  The magnitude data are from the
    //wikipedia articles for each moon, as of Oct 2007.
    Moon.append(new TrailObject(SkyObject::MOON, 0.0, 0.0, 5.0, i18nc("Jupiter's moon Io", "Io")));
    Moon.append(new TrailObject(SkyObject::MOON, 0.0, 0.0, 5.3, i18nc("Jupiter's moon Europa", "Europa")));
    Moon.append(new TrailObject(SkyObject::MOON, 0.0, 0.0, 4.6, i18nc("Jupiter's moon Ganymede", "Ganymede")));
    Moon.append(new TrailObject(SkyObject::MOON, 0.0, 0.0, 5.7, i18nc("Jupiter's moon Callisto", "Callisto")));

    XP      = QVector<double>(4, 0.0);
    YP      = QVector<double>(4, 0.0);
    ZP      = QVector<double>(4, 0.0);
    InFront = QVector<bool>(4, false);
}

JupiterMoons::~JupiterMoons()
{
}

void JupiterMoons::findPosition(const KSNumbers *num, const KSPlanetBase *Jupiter, const KSSun *Sun)
{
    double Xj, Yj, Zj, Rj;
    double sinJB, cosJB, sinJL, cosJL;
    double sinSB, cosSB, sinSL, cosSL;
    double D, t, tdelay, LAMBDA, ALPHA;
    double T, oj, fj, ij, pa, tb, I, P;

    //Satellite position data:
    //l = mean longitude; Pj = longitude of perijove;
    //w = long. of the node on Jupiter's equatorial plane
    //G = Principal inequality in the longitude of Jupiter (whatever that means :)
    //fl = phase of free libration
    //z = longitude of node of Jupiter's equator on the ecliptic
    //Gj/Gs = mean anomalies of Jupiter and Saturn
    //Pj = Longitude of the perihelion of Jupiter
    double l1, l2, l3, l4, p1, p2, p3, p4, w1, w2, w3, w4, G, fl, z, Gj, Gs, Pj;

    //L/B = true longitude/latitude of satellites
    double S1, S2, S3, S4, L1, L2, L3, L4, b1, b2, b3, b4, R1, R2, R3, R4;
    double X[5], Y[5], Z[5];
    double A1[5], B1[5], C1[5];
    double A2[5], B2[5], C2[5];
    double A3[5], B3[5], C3[5];
    double A4[5], B4[5], C4[5];
    double A5[5], B5[5], C5[5];
    double A6[5], B6[5], C6[5];

    Jupiter->ecLong().SinCos(sinJL, cosJL);
    Jupiter->ecLat().SinCos(sinJB, cosJB);

    Sun->ecLong().SinCos(sinSL, cosSL);
    Sun->ecLat().SinCos(sinSB, cosSB);

    //Geocentric Rectangular coordinates of Jupiter:
    Xj = Jupiter->rsun() * cosJB * cosJL + Sun->rsun() * cosSL;
    Yj = Jupiter->rsun() * cosJB * sinJL + Sun->rsun() * sinSL;
    Zj = Jupiter->rsun() * sinJB;

    //Distance and light-travel delay time:
    //0.0057755183 is the inverse of the speed of light, in days/AU
    Rj     = sqrt(Xj * Xj + Yj * Yj + Zj * Zj);
    tdelay = 0.0057755183 * Rj; //light travel delay, in days

    LAMBDA = atan2(Yj, Xj);
    ALPHA  = atan2(Zj, sqrt(Xj * Xj + Yj * Yj));

    //days since 10 Aug 1976 0h (minus light-travel delay)
    t = num->julianDay() - 2443000.5 - tdelay;

    //Mean longitudes of the satellites:
    l1 = dms(106.07947 + 203.488955432 * t).radians();
    l2 = dms(175.72938 + 101.374724550 * t).radians();
    l3 = dms(120.55434 + 50.317609110 * t).radians();
    l4 = dms(84.44868 + 21.571071314 * t).radians();

    //Longitudes of the satellites' Perijoves (point along orbit nearest to Jupiter)
    p1 = dms(58.3329 + 0.16103936 * t).radians();
    p2 = dms(132.8959 + 0.04647985 * t).radians();
    p3 = dms(187.2887 + 0.00712740 * t).radians();
    p4 = dms(335.3418 + 0.00183998 * t).radians();

    //Longitudes of the satellites' nodes on the equatorial plane of Jupiter
    w1 = dms(311.0793 - 0.13279430 * t).radians();
    w2 = dms(100.5099 - 0.03263047 * t).radians();
    w3 = dms(119.1688 - 0.00717704 * t).radians();
    w4 = dms(322.5729 - 0.00175934 * t).radians();

    //Principal inequality in the longitude of Jupiter
    //	G = 0.33033*sin( 163.679 + 0.0010512*t ) + 0.03439*sin( 34.486 - 0.0161731*t );
    //converted sin args to radians:
    G = dms(0.33033 * sin(2.85674 + 0.0000183469 * t) + 0.03439 * sin(0.601894 - 0.000282274 * t)).radians();

    //phase of free libration
    fl = dms(191.8132 + 0.17390023 * t).radians();

    //longitude of Jupiter's equatorial node on the ecliptic
    z = dms(316.5182 - 0.00000208 * t).radians();

    //Mean anomalies of Jupiter and Saturn
    Gj = dms(30.23756 + 0.0830925701 * t + G / dms::DegToRad).radians();
    Gs = dms(31.97853 + 0.0334597339 * t).radians();

    //Longitude of perihelion of Jupiter
    Pj = dms(13.469942).radians();

    //***Periodic terms in the longitudes of the satellites
    S1 = 0.47259 * sin(2. * (l1 - l2)) - 0.03480 * sin(p3 - p4) - 0.01756 * sin(p1 + p3 - 2. * Pj - 2. * Gj) +
         0.01080 * sin(l2 - 2. * l3 + p3) + 0.00757 * sin(fl) + 0.00663 * sin(l2 - 2. * l3 + p4) +
         0.00453 * sin(l1 - p3) + 0.00453 * sin(l2 - 2. * l3 + p2) - 0.00354 * sin(l1 - l2) -
         0.00317 * sin(2. * z - 2. * Pj) - 0.00269 * sin(l2 - 2. * l3 + p1) + 0.00263 * sin(l1 - p4) +
         0.00186 * sin(l1 - p1) - 0.00186 * sin(Gj) + 0.00167 * sin(p2 - p3) + 0.00158 * sin(4. * (l1 - l2)) -
         0.00155 * sin(l1 - l3) - 0.00142 * sin(z + w3 - 2. * Pj - 2. * Gj) - 0.00115 * sin(2. * (l1 - 2. * l2 + w2)) +
         0.00089 * sin(p2 - p4) + 0.00084 * sin(w2 - w3) + 0.00084 * sin(l1 + p3 - 2. * Pj - 2. * Gj) +
         0.00053 * sin(z - w2);

    S2 = 1.06476 * sin(2. * (l2 - l3)) + 0.04253 * sin(l1 - 2. * l2 + p3) + 0.03579 * sin(l2 - p3) +
         0.02383 * sin(l1 - 2. * l2 + p4) + 0.01977 * sin(l2 - p4) - 0.01843 * sin(fl) + 0.01299 * sin(p3 - p4) -
         0.01142 * sin(l2 - l3) + 0.01078 * sin(l2 - p2) - 0.01058 * sin(Gj) + 0.00870 * sin(l2 - 2. * l3 + p2) -
         0.00775 * sin(2. * (z - Pj)) + 0.00524 * sin(2. * (l1 - l2)) - 0.00460 * sin(l1 - l3) +
         0.00450 * sin(l2 - 2. * l3 + p1) + 0.00327 * sin(z + w3 - 2. * Pj - 2. * Gj) -
         0.00296 * sin(p1 + p3 - 2. * Pj - 2. * Gj) - 0.00151 * sin(2. * Gj) + 0.00146 * sin(z - w3) +
         0.00125 * sin(z - w4) - 0.00117 * sin(l1 - 2. * l3 + p3) - 0.00095 * sin(2. * (l2 - w2)) +
         0.00086 * sin(l1 - 2. * l2 + w2) - 0.00086 * sin(5. * Gs - Gj + 0.911497) - 0.00078 * sin(l2 - l4) -
         0.00064 * sin(l1 - 2. * l3 + p4) - 0.00063 * sin(3. * l3 - 7. * l4 + 4. * p4) + 0.00061 * sin(p1 - p4) +
         0.00058 * sin(2. * (z - Pj - Gj)) + 0.00058 * sin(w3 - w4) + 0.00056 * sin(2. * (l2 - l4)) +
         0.00055 * sin(2. * (l1 - l3)) + 0.00052 * sin(3. * l3 - 7. * l4 + p3 + 3. * p4) - 0.00043 * sin(l1 - p3) +
         0.00042 * sin(p3 - p2) + 0.00041 * sin(5. * (l2 - l3)) + 0.00041 * sin(p4 - Pj) + 0.00038 * sin(l2 - p1) +
         0.00032 * sin(w2 - w3) + 0.00032 * sin(2. * (l3 - Gj - Pj)) + 0.00029 * sin(p1 - p3);

    S3 = 0.16477 * sin(l3 - p3) + 0.09062 * sin(l3 - p4) - 0.06907 * sin(l2 - l3) + 0.03786 * sin(p3 - p4) +
         0.01844 * sin(2. * (l3 - l4)) - 0.01340 * sin(Gj) + 0.00703 * sin(l2 - 2. * l3 + p3) -
         0.00670 * sin(2. * (z - Pj)) - 0.00540 * sin(l3 - l4) + 0.00481 * sin(p1 + p3 - 2. * Pj - 2. * Gj) -
         0.00409 * sin(l2 - 2. * l3 + p2) + 0.00379 * sin(l2 - 2. * l3 + p4) + 0.00235 * sin(z - w3) +
         0.00198 * sin(z - w4) + 0.00180 * sin(fl) + 0.00129 * sin(3. * (l3 - l4)) + 0.00124 * sin(l1 - l3) -
         0.00119 * sin(5. * Gs - 2. * Gj + 0.911497) + 0.00109 * sin(l1 - l2) -
         0.00099 * sin(3. * l3 - 7. * l4 + 4. * p4) + 0.00091 * sin(w3 - w4) +
         0.00081 * sin(3. * l3 - 7. * l4 + p3 + 3. * p4) - 0.00076 * sin(2. * l2 - 3. * l3 + p3) +
         0.00069 * sin(p4 - Pj) - 0.00058 * sin(2. * l3 - 3. * l4 + p4) + 0.00057 * sin(l3 + p3 - 2. * Pj - 2. * Gj) -
         0.00057 * sin(l3 - 2. * l4 + p4) - 0.00052 * sin(p2 - p3) - 0.00052 * sin(l2 - 2. * l3 + p1) +
         0.00048 * sin(l3 - 2. * l4 + p3) - 0.00045 * sin(2. * l2 - 3. * l3 + p4) - 0.00041 * sin(p2 - p4) -
         0.00038 * sin(2. * Gj) - 0.00033 * sin(p3 - p4 + w3 - w4) -
         0.00032 * sin(3. * l3 - 7. * l4 + 2. * p3 + 2. * p4) + 0.00030 * sin(4. * (l3 - l4)) -
         0.00029 * sin(w3 + z - 2. * Pj - 2. * Gj) + 0.00029 * sin(l3 + p4 - 2. * Pj - 2. * Gj) +
         0.00026 * sin(l3 - Pj - Gj) + 0.00024 * sin(l2 - 3. * l3 + 2. * l4) + 0.00021 * sin(2. * (l3 - Pj - Gj)) -
         0.00021 * sin(l3 - p2) + 0.00017 * sin(2. * (l3 - p2));

    S4 = 0.84109 * sin(l4 - p4) + 0.03429 * sin(p4 - p3) - 0.03305 * sin(2. * (z - Pj)) - 0.03211 * sin(Gj) -
         0.01860 * sin(l4 - p3) + 0.01182 * sin(z - w4) + 0.00622 * sin(l4 + p4 - 2. * Gj - 2. * Pj) +
         0.00385 * sin(2. * (l4 - p4)) - 0.00284 * sin(5. * Gs - 2. * Gj + +0.911497) - 0.00233 * sin(2. * (z - p4)) -
         0.00223 * sin(l3 - l4) - 0.00208 * sin(l4 - Pj) + 0.00177 * sin(z + w4 - 2. * p4) + 0.00134 * sin(p4 - Pj) +
         0.00125 * sin(2. * (l4 - Gj - Pj)) - 0.00117 * sin(2. * Gj) - 0.00112 * sin(2. * (l3 - l4)) +
         0.00106 * sin(3. * l3 - 7. * l4 + 4. * p4) + 0.00102 * sin(l4 - Gj - Pj) + 0.00096 * sin(2. * l4 - z - w4) +
         0.00087 * sin(2. * (z - w4)) - 0.00087 * sin(3. * l3 - 7. * l4 + p3 + 3. * p4) +
         0.00085 * sin(l3 - 2. * l4 + p4) - 0.00081 * sin(2. * (l4 - z)) + 0.00071 * sin(l4 + p4 - 2. * Pj - 2. * Gj) +
         0.00060 * sin(l1 - l4) - 0.00056 * sin(z - w3) - 0.00055 * sin(l3 - 2. * l4 + p3) + 0.00051 * sin(l2 - l4) +
         0.00042 * sin(2. * (z - Gj - Pj)) + 0.00039 * sin(2. * (p4 - w4)) + 0.00036 * sin(z + Pj - p4 - w4) +
         0.00035 * sin(2. * Gs - Gj + 3.28767) - 0.00035 * sin(l4 - p4 + 2. * Pj - 2. * z) -
         0.00032 * sin(l4 + p4 - 2. * Pj - Gj) + 0.00030 * sin(3. * l3 - 7. * l4 + 2. * p3 + 2. * p4) +
         0.00030 * sin(2. * Gs - 2. * Gj + 2.60316) + 0.00028 * sin(l4 - p4 + 2. * z - 2. * Pj) -
         0.00028 * sin(2. * (l4 - w4)) - 0.00027 * sin(p3 - p4 + w3 - w4) - 0.00026 * sin(5. * Gs - 3. * Gj + 3.28767) +
         0.00025 * sin(w4 - w3) - 0.00025 * sin(l2 - 3. * l3 + 2. * l4) - 0.00023 * sin(3. * (l3 - l4)) +
         0.00021 * sin(2. * l4 - 2. * Pj - 3. * Gj) - 0.00021 * sin(2. * l3 - 3. * l4 + p4) +
         0.00019 * sin(l4 - p4 - Gj) - 0.00019 * sin(2. * l4 - p4 + Gj) - 0.00018 * sin(l4 - p4 + Gj) -
         0.00016 * sin(l4 + p3 - 2. * Pj - 2. * Gj);

    //Convert Longitude Sums to Radians:
    S1 *= dms::DegToRad;
    S2 *= dms::DegToRad;
    S3 *= dms::DegToRad;
    S4 *= dms::DegToRad;

    L1 = l1 + S1;
    L2 = l2 + S2;
    L3 = l3 + S3;
    L4 = l4 + S4;

    //Periodic terms in the latitudes of the satellites
    tb = 0.0006502 * sin(L1 - w1) + 0.0001835 * sin(L1 - w2) + 0.0000329 * sin(L1 - w3) - 0.0000311 * sin(L1 - z) +
         0.0000093 * sin(L1 - w4) + 0.0000075 * sin(3. * L1 - 4. * l2 - 1.9927 * S1 + w2) +
         0.0000046 * sin(L1 + z - 2. * Pj - 2. * Gj);
    b1 = atan(tb);

    tb = 0.0081275 * sin(L2 - w2) + 0.0004512 * sin(L2 - w3) - 0.0003286 * sin(L2 - z) + 0.0001164 * sin(L2 - w4) +
         0.0000273 * sin(l1 - 2. * l3 + 1.0146 * S2 + w2) + 0.0000143 * sin(L2 + z - 2. * Pj - 2. * Gj) -
         0.0000143 * sin(L2 - w1) + 0.0000035 * sin(L2 - z + Gj) - 0.0000028 * sin(l1 - 2. * l3 + 1.0146 * S2 + w3);
    b2 = atan(tb);

    tb = 0.0032364 * sin(L3 - w3) - 0.0016911 * sin(L3 - z) + 0.0006849 * sin(L3 - w4) - 0.0002806 * sin(L3 - w2) +
         0.0000321 * sin(L3 + z - 2. * Pj - 2. * Gj) + 0.0000051 * sin(L3 - z + Gj) - 0.0000045 * sin(L3 - z - Gj) -
         0.0000045 * sin(L3 + z - 2. * Pj) + 0.0000037 * sin(L3 + z - 2. * Pj - 3. * Gj) +
         0.0000030 * sin(2. * l2 - 3. * L3 + 4.03 * S3 + w2) - 0.0000021 * sin(2. * l2 - 3. * L3 + 4.03 * S3 + w3);
    b3 = atan(tb);

    tb = -0.0076579 * sin(L4 - z) + 0.0044148 * sin(L4 - w4) - 0.0005106 * sin(L4 - w3) +
         0.0000773 * sin(L4 + z - 2. * Pj - 2. * Gj) + 0.0000104 * sin(L4 - z + Gj) - 0.0000102 * sin(L4 - z - Gj) +
         0.0000088 * sin(L4 + z - 2. * Pj - 3. * Gj) - 0.0000038 * sin(L4 + z - 2. * Pj - Gj);
    b4 = atan(tb);

    //Periodic terms in the Radius of the stellites (distance from Jupiter)
    R1 = 5.90730 * (1.0 + -0.0041339 * cos(2. * (l1 - l2)) - 0.0000395 * cos(l1 - p3) - 0.0000214 * cos(l1 - p4) +
                    0.0000170 * cos(l1 - l2) - 0.0000162 * cos(l1 - p1) - 0.0000130 * cos(4. * (l1 - l2)) +
                    0.0000106 * cos(l1 - l3) - 0.0000063 * cos(l1 + p3 - 2. * Pj - 2 * Gj));

    R2 = 9.39912 * (1.0 + 0.0093847 * cos(l1 - l2) - 0.0003114 * cos(l2 - p3) - 0.0001738 * cos(l2 - p4) -
                    0.0000941 * cos(l2 - p2) + 0.0000553 * cos(l2 - l3) + 0.0000523 * cos(l1 - l3) -
                    0.0000290 * cos(2. * (l1 - l2)) + 0.0000166 * cos(2. * (l2 - w2)) +
                    0.0000107 * cos(l1 - 2. * l3 + p3) - 0.0000102 * cos(l2 - p1) - 0.0000091 * cos(2. * (l1 - l3)));

    R3 = 14.99240 * (1.0 + -0.0014377 * cos(l3 - p3) - 0.0007904 * cos(l3 - p4) + 0.0006342 * cos(l2 - l3) -
                     0.0001758 * cos(2. * (l3 - l4)) + 0.0000294 * cos(l3 - l4) - 0.0000156 * cos(3. * (l3 - l4)) +
                     0.0000155 * cos(l1 - l3) - 0.0000153 * cos(l1 - l2) + 0.0000070 * cos(2. * l2 - 3. * l3 + p3) -
                     0.0000051 * cos(l3 + p3 - 2. * Pj - 2. * Gj));

    R4 = 26.36990 * (1.0 + -0.0073391 * cos(l4 - p4) + 0.0001620 * cos(l4 - p3) + 0.0000974 * cos(l3 - l4) -
                     0.0000541 * cos(l4 + p4 - 2. * Pj - 2. * Gj) - 0.0000269 * cos(2. * (l4 - p4)) +
                     0.0000182 * cos(l4 - Pj) + 0.0000177 * cos(2. * (l3 - l4)) - 0.0000167 * cos(2. * l4 - z - w4) +
                     0.0000167 * cos(z - w4) - 0.0000155 * cos(2. * (l4 - Pj - Gj)) + 0.0000142 * cos(2. * (l4 - z)) +
                     0.0000104 * cos(l1 - l4) + 0.0000092 * cos(l2 - l4) - 0.0000089 * cos(l4 - Pj - Gj) -
                     0.0000062 * cos(l4 + p4 - 2. * Pj - 3. * Gj) + 0.0000048 * cos(2. * (l4 - w4)));

    //Inclination of Jupiter's rotational axis since 1900.0
    t = (num->julianDay() - 2415020.50) / 36525.0;
    I = dms(3.120262 + 0.0006 * t).radians();

    //Precession since B1950:
    t = (num->julianDay() - 2433282.423) / 36525.0;
    P = dms(1.3966626 * t + 0.0003088 * t * t).radians();

    L1 += P;
    L2 += P;
    L3 += P;
    L4 += P;
    z += P;

    X[0] = R1 * cos(L1 - z) * cos(b1);
    X[1] = R2 * cos(L2 - z) * cos(b2);
    X[2] = R3 * cos(L3 - z) * cos(b3);
    X[3] = R4 * cos(L4 - z) * cos(b4);
    Y[0] = R1 * sin(L1 - z) * cos(b1);
    Y[1] = R2 * sin(L2 - z) * cos(b2);
    Y[2] = R3 * sin(L3 - z) * cos(b3);
    Y[3] = R4 * sin(L4 - z) * cos(b4);
    Z[0] = R1 * sin(b1);
    Z[1] = R2 * sin(b2);
    Z[2] = R3 * sin(b3);
    Z[3] = R4 * sin(b4);

    //fictional "fifth moon" used later...
    X[4] = 0.0;
    Y[4] = 0.0;
    Z[4] = 1.0;

    T = num->julianCenturies();

    oj = dms(100.464441 + 1.0209550 * T + 0.00040117 * T * T + 0.000000569 * T * T * T).radians();
    fj = z - oj;
    ij = dms(1.303270 - 0.0054966 * T + 0.00000465 * T * T - 0.000000004 * T * T * T).radians();

    for (int i = 0; i < 5; ++i)
    {
        A1[i] = X[i];
        B1[i] = Y[i] * cos(I) - Z[i] * sin(I);
        C1[i] = Y[i] * sin(I) + Z[i] * cos(I);

        A2[i] = A1[i] * cos(fj) - B1[i] * sin(fj);
        B2[i] = A1[i] * sin(fj) + B1[i] * cos(fj);
        C2[i] = C1[i];

        A3[i] = A2[i];
        B3[i] = B2[i] * cos(ij) - C2[i] * sin(ij);
        C3[i] = B2[i] * sin(ij) + C2[i] * cos(ij);

        A4[i] = A3[i] * cos(oj) - B3[i] * sin(oj);
        B4[i] = A3[i] * sin(oj) + B3[i] * cos(oj);
        C4[i] = C3[i];

        A5[i] = A4[i] * sin(LAMBDA) - B4[i] * cos(LAMBDA);
        B5[i] = A4[i] * cos(LAMBDA) + B4[i] * sin(LAMBDA);
        C5[i] = C4[i];

        A6[i] = A5[i];
        B6[i] = C5[i] * sin(ALPHA) + B5[i] * cos(ALPHA);
        C6[i] = C5[i] * cos(ALPHA) - B5[i] * sin(ALPHA);

        /* DEBUG
        qDebug() <<"A: "<<i<<": "<<A1[i]<<": "<<A2[i]<<": "<<A3[i]<<": "<<A4[i]<<": "<<A5[i]<<": "<<A6[i];
        qDebug() <<"B: "<<i<<": "<<B1[i]<<": "<<B2[i]<<": "<<B3[i]<<": "<<B4[i]<<": "<<B5[i]<<": "<<B6[i];
        qDebug() <<"C: "<<i<<": "<<C1[i]<<": "<<C2[i]<<": "<<C3[i]<<": "<<C4[i]<<": "<<C5[i]<<": "<<C6[i];
        */
    }

    D = atan2(A6[4], C6[4]);

    //X and Y are now the rectangular coordinates of each satellite,
    //in units of Jupiter's Equatorial radius.
    //When Z is negative, the planet is nearer to the Sun than Jupiter.

    pa = Jupiter->pa() * dms::PI / 180.0;

    for (int i = 0; i < 4; ++i)
    {
        XP[i] = A6[i] * cos(D) - C6[i] * sin(D);
        YP[i] = A6[i] * sin(D) + C6[i] * cos(D);
        ZP[i] = B6[i];

        Moon[i]->setRA(Jupiter->ra().Hours() - 0.011 * (XP[i] * cos(pa) - YP[i] * sin(pa)) / 15.0);
        Moon[i]->setDec(Jupiter->dec().Degrees() - 0.011 * (XP[i] * sin(pa) + YP[i] * cos(pa)));

        //SkyPoint p = Moon[i]->deprecess(
        //    num); // FIXME: Really, we should also denutate. Actually, we should also be aberrating these above, right?
        SkyPoint p = Moon[i]->catalogueCoord(num->julianDay());
        Moon[i]->setRA0(
            p.ra()); // Just to be sure, in case deprecess doesn't set it already because RA0 was not NaN or something.
        Moon[i]->setDec0(p.dec());

        if (ZP[i] < 0.0)
            InFront[i] = true;
        else
            InFront[i] = false;

        //Update Trails
        if (Moon[i]->hasTrail())
        {
            Moon[i]->addToTrail();
            if (Moon[i]->trail().size() > TrailObject::MaxTrail)
                Moon[i]->clipTrail();
        }
    }
}
