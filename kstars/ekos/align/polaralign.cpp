/*  polaralign.cpp  determines the mount polar axis position

    Copyright (C) 2020 Chris Rowland <chris.rowland@cherryfield.me.uk>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include "polaralign.h"

#include <cmath>

QVector3D PolarAlign::dirCos(const dms primary, const dms secondary)
{
    return QVector3D(
                static_cast<float>(secondary.cos() * primary.cos()),
                static_cast<float>(secondary.cos() * primary.sin()),
                static_cast<float>(secondary.sin()));
}

QVector3D PolarAlign::dirCos(const SkyPoint sp)
{
    return dirCos(sp.ra(), sp.dec());
}

dms PolarAlign::primary(QVector3D dirCos)
{
    dms p;
    p.setRadians(static_cast<double>(std::atan2(dirCos.y(), dirCos.x())));
    return p;
}

dms PolarAlign::secondary(QVector3D dirCos)
{
    dms p;
    p.setRadians(static_cast<double>(std::asin(dirCos.z())));
    return p;
}

SkyPoint PolarAlign::skyPoint(QVector3D dc)
{
    return SkyPoint(primary(dc), secondary(dc));
}

QVector3D PolarAlign::poleAxis(SkyPoint p1, SkyPoint p2, SkyPoint p3)
{
    // convert the three positions to vectors, these define the plane
    // of the Ha axis rotation
    QVector3D v1 = PolarAlign::dirCos(p1);
    QVector3D v2 = PolarAlign::dirCos(p2);
    QVector3D v3 = PolarAlign::dirCos(p3);

    // the Ha axis direction is the normal to the plane
    QVector3D p = QVector3D::normal(v1, v2, v3);

    // p points to the north or south pole depending on the rotation of the points
    // the other pole position can be determined by reversing the sign of the Dec and
    // adding 12hrs to the Ha value.
    // if we want only the north then this would do it
    //if (p.z() < 0)
    //    p = -p;
    return p;
}
