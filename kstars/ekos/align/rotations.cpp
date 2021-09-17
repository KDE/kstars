/*
    SPDX-FileCopyrightText: 2021 Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "rotations.h"

#include <cmath>

// In the coordinate system used in this file:
// x points forward
// y points to the left
// z points up.
// In this system, theta is the angle between x & y and is related
// to our azimuth: theta = 90-azimuth.
// Phi is the angle away from z, and is related to our altitude as 90-altitude.

namespace Rotations
{

double d2r(double degrees)
{
    return 2 * M_PI * degrees / 360.0;
}

double r2d(double radians)
{
    return 360.0 * radians / (2 * M_PI);
}

V3 V3::normal(const V3 &v1, const V3 &v2, const V3 &v3)
{
    // First subtract d21 = V2-V1; d32 = V3-V2
    const V3 d21 = V3(v2.x() - v1.x(), v2.y() - v1.y(), v2.z() - v1.z());
    const V3 d32 = V3(v3.x() - v2.x(), v3.y() - v2.y(), v3.z() - v2.z());
    // Now take the cross-product of d21 and d31
    const V3 cross = V3(d21.y() * d32.z() - d21.z() * d32.y(),
                        d21.z() * d32.x() - d21.x() * d32.z(),
                        d21.x() * d32.y() - d21.y() * d32.x());
    // Finally normalize cross so that it is a unit vector.
    const double lenSq = cross.x() * cross.x() + cross.y() * cross.y() + cross.z() * cross.z();
    if (lenSq == 0.0) return V3();
    const double len = sqrt(lenSq);
    // Should we also fail if len < e.g. 5e-8 ??
    return V3(cross.x() / len, cross.y() / len, cross.z() / len);
}

double V3::length()
{
    return sqrt(X * X + Y * Y + Z * Z);
}

V3 azAlt2xyz(const QPointF &azAlt)
{
    // Convert the new point to xyz
    // See https://mathworld.wolfram.com/SphericalCoordinates.html
    const double azRadians = d2r(azAlt.x());
    const double altRadians = d2r(azAlt.y());

    const double theta = -azRadians;
    const double phi = (M_PI / 2.0) - altRadians;
    const double x = cos(theta) * sin(phi);
    const double y = sin(theta) * sin (phi);
    const double z = cos(phi);
    return V3(x, y, z);

}

QPointF xyz2azAlt(const V3 &xyz)
{
    // Deal with degenerate values for the atan along the meridian (y == 0).
    if (xyz.y() == 0.0 && xyz.x() == 0.0)
    {
        // Straight overhead
        return QPointF(0.0, 90.0);
    }

    const double azRadians = (xyz.y() == 0) ? 0.0 : -atan2(xyz.y(), xyz.x());
    const double altRadians = (M_PI / 2.0) - acos(xyz.z());

    return QPointF(r2d(azRadians), r2d(altRadians));
}

V3 haDec2xyz(const QPointF &haDec, double latitude)
{
    const double haRadians = d2r(haDec.x());
    // Since dec=90 points at the pole.
    const double decRadians = d2r(haDec.y() - 90);

    const double x = cos(decRadians);
    const double y = sin(haRadians) * sin(decRadians);
    const double z = cos(haRadians) * sin(decRadians);
    return rotateAroundY(V3(x, y, z), -fabs(latitude));
}

QPointF xyz2haDec(const V3 &xyz, double latitude)
{

    const V3 pt = rotateAroundY(xyz, latitude);
    const double ha = r2d(atan2(pt.y(), pt.z()));

    V3 pole = Rotations::azAlt2xyz(QPointF(0, fabs(latitude)));
    const double dec = 90 - getAngle(xyz, pole);
    return QPointF(ha, dec);
}

V3 getAxis(const V3 &p1, const V3 &p2, const V3 &p3)
{
    return V3::normal(p1, p2, p3);
}

// Returns the angle between two points on a sphere using the spherical law of
// cosines: https://en.wikipedia.org/wiki/Great-circle_distance
double getAngle(const V3 &p1, const V3 &p2)
{
    QPointF a1 = xyz2azAlt(p1);
    QPointF a2 = xyz2azAlt(p2);
    return r2d(acos(sin(d2r(a1.y())) * sin(d2r(a2.y())) +
                    cos(d2r(a1.y())) * cos(d2r(a2.y())) * cos(d2r(a1.x()) - d2r(a2.x()))));
}

// Using the equations in
// https://sites.google.com/site/glennmurray/Home/rotation-matrices-and-formulas/rotation-about-an-arbitrary-axis-in-3-dimensions
// This rotates point around axis (which should be a unit vector) by degrees.
V3 rotateAroundAxis(const V3 &point, const V3 &axis, double degrees)
{
    const double cosAngle = cos(d2r(degrees));
    const double sinAngle = sin(d2r(degrees));
    const double pointDotAxis = (point.x() * axis.x() + point.y() * axis.y() + point.z() * axis.z());
    const double x = axis.x() * pointDotAxis * (1.0 - cosAngle) + point.x() * cosAngle + (-axis.z() * point.y() + axis.y() *
                     point.z()) * sinAngle;
    const double y = axis.y() * pointDotAxis * (1.0 - cosAngle) + point.y() * cosAngle + (axis.z() * point.x() + -axis.x() *
                     point.z()) * sinAngle;
    const double z = axis.z() * pointDotAxis * (1.0 - cosAngle) + point.z() * cosAngle + (-axis.y() * point.x() + axis.x() *
                     point.y()) * sinAngle;
    return V3(x, y, z);
}

// Simpler version of above for altitude rotations, our main case.
// Multiply [x,y,z] by the rotate-Y by "angle" rotation matrix
// as in https://en.wikipedia.org/wiki/Rotation_matrix
//   cos(angle)  0   sin(angle)
//       0       1       0
//  -sin(angle)  0   cos(angle)
V3 rotateAroundY(const V3 &point, double degrees)
{
    const double radians = d2r(degrees);
    const double cosAngle = cos(radians);
    const double sinAngle = sin(radians);
    return V3( point.x() * cosAngle + point.z() * sinAngle,
               point.y(),
               -point.x() * sinAngle + point.z() * cosAngle);
}

// Rotates in altitude then azimuth, as is done to correct for polar alignment.
// Note, NOT a single rotation along a great circle, but rather two separate
// rotations.
QPointF rotateRaAxis(const QPointF &azAltPoint, const QPointF &azAltRotation)
{
    const V3 point = azAlt2xyz(azAltPoint);
    const V3 altRotatedPoint = rotateAroundY(point, -azAltRotation.y());

    // Az rotation is simply adding in the az angle.
    const QPointF altAz = xyz2azAlt(altRotatedPoint);
    return QPointF(altAz.x() + azAltRotation.x(), altAz.y());
}

}  // namespace rotations
