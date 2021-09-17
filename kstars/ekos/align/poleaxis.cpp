/*
    SPDX-FileCopyrightText: 2020 Chris Rowland <chris.rowland@cherryfield.me.uk>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "poleaxis.h"

#include <cmath>


/******************************************************************
poleAxis(sp1, sp2, sp3) finds the mount's RA axis of rotation determined
by three points sampled by fixing the mount's DEC and sampling a point
a three different RA position.

For each SkyPoint sp, it finds its corresponding x,y,z coordinates,
which are points on a unit sphere.  Those 3 coordinates define a plane.
That plane intesects the sphere, and the intersection of a plane and a
sphere is a circle. The center of that circle would be the axis of rotation
defined by the origial 3 points.  So, finding the normal to the plane,
and pointing in that (normal) direction from the center of the sphere
(the origin) gives the axis of rotation of the mount.

poleAxis returns the normal. The way to interpret this vector
is a unit vector (direction) in the x,y,z space. To convert this back to an
angle useful for polar alignment, the ra and dec angles can be
retrieved with the primary(V) and secondary(V) functions.
These can then be turned into an altitude and azimuth angles using methods
in the SkyPoint class.

Further detail

Definitions: SkyPoint sp contains an hour angle ha and declication dec,
with the usual convention that dec=0 is the equator, dec=90 is the north pole
about which the axis spins, and ha is the spin angle
(positive is counter clockwise when looking north from the center of the sphere)
where 0-degrees would be pointing "up", e.g. toward the zenith when
looking at the north pole. The sphere has radius 1.0.

dirCos(sp) will return a 3D vector V whose components x,y,z
are 3D cartesean coordinate positions given these ha & dec angles
for points on the surface of a unit sphere.
The z direction points towards the north pole.
The y direction points left when looking at the north pole
The x direction points "up".

primary(V) where V contains the above x,y,z coordinates,
will return the ha angle pointing to V.

secondary(V) where V contains the above x,y,z coordinates,
will return the dec angle pointing to V.
******************************************************************/

Rotations::V3 PoleAxis::dirCos(const dms primary, const dms secondary)
{
    return Rotations::V3(
               static_cast<float>(secondary.cos() * primary.cos()),
               static_cast<float>(secondary.cos() * primary.sin()),
               static_cast<float>(secondary.sin()));
}

Rotations::V3 PoleAxis::dirCos(const SkyPoint sp)
{
    return dirCos(sp.ra(), sp.dec());
}

dms PoleAxis::primary(Rotations::V3 dirCos)
{
    dms p;
    p.setRadians(static_cast<double>(std::atan2(dirCos.y(), dirCos.x())));
    return p;
}

dms PoleAxis::secondary(Rotations::V3 dirCos)
{
    dms p;
    p.setRadians(static_cast<double>(std::asin(dirCos.z())));
    return p;
}

SkyPoint PoleAxis::skyPoint(Rotations::V3 dc)
{
    return SkyPoint(primary(dc), secondary(dc));
}

Rotations::V3 PoleAxis::poleAxis(SkyPoint p1, SkyPoint p2, SkyPoint p3)
{
    // convert the three positions to vectors, these define the plane
    // of the Ha axis rotation
    Rotations::V3 v1 = PoleAxis::dirCos(p1);
    Rotations::V3 v2 = PoleAxis::dirCos(p2);
    Rotations::V3 v3 = PoleAxis::dirCos(p3);

    // the Ha axis direction is the normal to the plane
    Rotations::V3 p = Rotations::V3::normal(v1, v2, v3);

    // p points to the north or south pole depending on the rotation of the points
    // the other pole position can be determined by reversing the sign of the Dec and
    // adding 12hrs to the Ha value.
    // if we want only the north then this would do it
    //if (p.z() < 0)
    //    p = -p;
    return p;
}
