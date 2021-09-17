/*
    SPDX-FileCopyrightText: 2021 Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "greatcircle.h"

#include "kstars_debug.h"
#include "Options.h"
#include "skymap.h"
#include "skyqpainter.h"
#include "projections/projector.h"
#include "skypoint.h"
#include "kstars.h"

#include <QStatusBar>

namespace
{

double d2r(double degrees)
{
    return degrees * 2.0 * M_PI / 360.0;
}

double r2d(double radians)
{
    return (radians * 360.0 / (2 * M_PI));
}

}  // namespace

// This implements the equations in:
// https://en.wikipedia.org/wiki/Great-circle_navigation

GreatCircle::GreatCircle(double az1, double alt1, double az2, double alt2) {
    // convert inputs to radians.
    az1 = d2r(az1);
    az2 = d2r(az2);
    alt1 = d2r(alt1);
    alt2 = d2r(alt2);

    const double daz = az2 - az1;
    const double alpha1 = atan2(
                (cos(alt2) * sin(daz)),
                (cos(alt1) * sin(alt2) - (sin(alt1) * cos(alt2) * cos(daz))));

    const double term1 = cos(alt1) * sin(alt2) - (sin(alt1) * cos(alt2) * cos(daz));
    const double term2 = cos(alt2) * sin(daz);
    const double sigma12 = atan2(
                sqrt(  term1 * term1 + term2 * term2),
                (sin(alt1) * sin(alt2) + (cos(alt1) * cos(alt2) * cos(daz))));

    const double term3 = cos(alpha1);
    const double term4 = sin(alpha1);
    const double term5 = sin(alt1);
    const double alpha0 = atan2(
                (sin(alpha1) * cos(alt1)),
                sqrt(term3 * term3 + term4 * term4 * term5 * term5));

    if ((alt1 == 0) && (alpha1 == 0.5 * M_PI))
        sigma01 = 0;
    else
        sigma01 = atan2(tan(alt1), cos(alpha1));

    sigma02 = sigma01 + sigma12;

    const double lambda01 = atan2(sin(alpha0) * sin(sigma01), cos(sigma01));

    lambda0 = az1 - lambda01;

    cosAlpha0 = cos(alpha0);
    sinAlpha0 = sin(alpha0);
}

void GreatCircle::waypoint(double fraction, double *az, double *alt)
{
    double sigma = (1-fraction) * sigma01 + fraction * sigma02;
    double cosSigma = cos(sigma);
    double sinSigma = sin(sigma);

    *alt = r2d(atan2(
                   (cosAlpha0 * sinSigma),
                   (sqrt(cosSigma * cosSigma + sinAlpha0 * sinAlpha0 * sinSigma * sinSigma))));

    double LL0 = atan2((sinAlpha0 * sinSigma), cosSigma);
    
    *az = r2d(LL0 + lambda0);
}
