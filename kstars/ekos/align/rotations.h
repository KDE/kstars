/*
    SPDX-FileCopyrightText: 2021 Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QPointF>

namespace Rotations
{

// Like QVector3D but double precision.
class V3
{
    public:
        V3(double x, double y, double z) : X(x), Y(y), Z(z) {};
        V3() : X(0.0), Y(0.0), Z(0.0) {};
        double x() const
        {
            return X;
        }
        double y() const
        {
            return Y;
        }
        double z() const
        {
            return Z;
        }
        static V3 normal(const V3 &v1, const V3 &v2, const V3 &v3);
        double length();
    private:
        double X, Y, Z;
};

// Convert degrees to radians and vica versa.
double d2r(double degrees);
double r2d(double radians);

// With 2 args, get the axis of rotation defined by 2 points on a sphere
// (the 3rd point is assumed to be the center of the sphere)
// So the rotation is along the geodesic defined by the two points and the center.
// With all three args, find the axis defined by those 3 points.
V3 getAxis(const V3 &p1, const V3 &p2, const V3 &p3 = V3(0.0, 0.0, 0.0));

// Gets the angle between 2 points on a sphere from the point of view of the
// center of the sphere.
double getAngle(const V3 &p1, const V3 &p2);

// Rotates the point around the unit vector axis (must be a unit vector to work)
// by degrees.
V3 rotateAroundAxis(const V3 &point, const V3 &axis, double degrees);
V3 rotateAroundY(const V3 &point, double degrees);

// Converts the AzAlt (azimuth is in .x(), altitude in .y())
// to an xyz point, and vica versa.
V3 azAlt2xyz(const QPointF &azAlt);
QPointF xyz2azAlt(const V3 &xyz);

// Converts an xyz point to HA and DEC (ha is in .x(), dec in .y())
// and vica versa.
QPointF xyz2haDec(const V3 &xyz, double latitude);
V3 haDec2xyz(const QPointF &haDec, double latitude);

// Rotate the az/alt point. Used when assisting the user to correct a polar alignment error.
// Input is the point to be rotated, Azimuth = azAltPoint.x(), Altitude = azAltPoint.y().
// azAltRotation: the rotation angles, which correspond to the error in polar alignment
// that we would like to correct at the pole. The returned QPointF is the rotated azimuth
// and altitude coordinates.
QPointF rotateRaAxis(const QPointF &azAltPoint, const QPointF &azAltRotation);

}  // namespace rotations
