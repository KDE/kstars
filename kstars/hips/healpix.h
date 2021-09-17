/* -----------------------------------------------------------------------------

    SPDX-FileCopyrightText: 1997-2016 Krzysztof M. Gorski Eric Hivon, Martin Reinecke,
    Benjamin D. Wandelt, Anthony J. Banday,
    Matthias Bartelmann,
    Reza Ansari & Kenneth M. Ganga


    This file is part of HEALPix.

    Based on work by Pavel Mraz from SkyTechX.

    Adapted to KStars by Jasem Mutlaq.

    HEALPix is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    HEALPix is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with HEALPix; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

    For more information about HEALPix see https://healpix.sourceforge.io/

    ---------------------------------------------------------------------------*/

#pragma once

#include "hips.h"

#include <QVector3D>

class SkyPoint;

class HEALPix
{
public:
    HEALPix() = default;

    void getCornerPoints(int level, int pix, SkyPoint *skyCoords);
    void neighbours(int nside, qint32 ipix, int *result);
    int  getPix(int level, double ra, double dec);
    void getPixChilds(int pix, int *childs);

private:
    void nest2xyf(int nside, int pix, int *ix, int *iy, int *face_num);
    QVector3D toVec3(double fx, double fy, int face);
    void boundaries(qint32 nside, qint32 pix, int step, QVector3D *out);
    int ang2pix_nest_z_phi(qint32 nside_, double z, double phi);
    void xyz2sph(const QVector3D &vec, double &l, double &b);
};

