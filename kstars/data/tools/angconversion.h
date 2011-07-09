/***************************************************************************
         angconversion.h -- Functions for converting angle formats
                             -------------------
    begin                : Sat Jul 9 2011
    copyright            : (C) 2011 by Akarsh Simha
    email                : akarshsimha@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef ANGCONVERSION_H
#define ANGCONVERSION_H

#include <math.h> /* For M_PI */

/* Angle conversion macros */
#define RADPERDEG (M_PI/180.0) /* Number of radians in a degree */

/* Basic conversions between Degrees, Hours and Radians */
inline double deg2rad(x) { return (x * RADPERDEG); }
inline double rad2deg(x) { return (x / RADPERDEG); }
inline double hour2deg(x) { return (x * 15.0); }
inline double deg2hour(x) { return (x / 15.0); }
inline double hour2rad(x) { return deg2rad( hour2deg(x) ); }
inline double rad2hour(x) { return deg2hour( rad2deg(x) ); }

/* Convert degrees to arcminutes or arcseconds and back */
inline double deg2arcsec(x) { return (x * 3600.0); }
inline double arcsec2deg(x) { return (x / 3600.0); }
inline double deg2arcmin(x) { return (x * 60.0); }
inline double arcmin2deg(x) { return (x / 60.0); }

/* The following are redundant, but anyway */
inline double hour2sec(x) { return (x * 3600.0); }
inline double sec2hour(x) { return (x / 3600.0); }
inline double hour2min(x) { return (x * 60.0); }
inline double min2hour(x) { return (x / 60.0); }

/* Convert DMS / HMS to Degrees / Hours */
inline double dms2deg(double d, double m, double s) { return (d + m / 60.0 + s/3600.0); }
inline double hms2hour(double h, double m, double s) { return (h + m / 60.0 + s / 3600.0); }

void deg2dms(double D, int *d, int *m, float *s) {
    *d = (int)D;
    *m = (int)(( D - *d ) * 60);
    *s = (int)(( D - *d ) * 3600 - (*m * 60));
}

void hour2hms(double H, int *h, int *m, float *s) { /* Another redundant function */
    *h = (int)H;
    *m = (int)(( H - *d ) * 60);
    *s = (int)(( H - *d ) * 3600 - (*m * 60));
}

#endif
