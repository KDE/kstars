/* -----------------------------------------------------------------------------

    SPDX-FileCopyrightText: 1997-2016 Martin Reinecke,
    SPDX-FileCopyrightText: 1997-2016 Krzysztof M. Gorski
    SPDX-FileCopyrightText: 1997-2016 Eric Hivon
    SPDX-FileCopyrightText: Benjamin D. Wandelt
    SPDX-FileCopyrightText: Anthony J. Banday,
    SPDX-FileCopyrightText: Matthias Bartelmann,
    SPDX-FileCopyrightText: Reza Ansari
    SPDX-FileCopyrightText: Kenneth M. Ganga

    This file is part of HEALPix.

    Based on work by Pavel Mraz from SkyTechX.

    Modified by Jasem Mutlaq for KStars:
    SPDX-FileCopyrightText: Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later

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

