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

#include "healpix.h"

#include <QMatrix4x4>

#include "hipsmanager.h"
#include "skypoint.h"
#include "kstarsdata.h"
#include "geolocation.h"

static const double twothird = 2.0 / 3.0;
static const double twopi = 6.283185307179586476925286766559005768394;
static const double inv_halfpi = 0.6366197723675813430755350534900574;

static const int jrll[] = { 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4 };
static const int jpll[] = { 1, 3, 5, 7, 0, 2, 4, 6, 1, 3, 5, 7 };

static const short ctab[] =
{
#define Z(a) a,a+1,a+256,a+257
#define Y(a) Z(a),Z(a+2),Z(a+512),Z(a+514)
#define X(a) Y(a),Y(a+4),Y(a+1024),Y(a+1028)
    X(0), X(8), X(2048), X(2056)
#undef X
#undef Y
#undef Z
};

static const short utab[] =
{
#define Z(a) 0x##a##0, 0x##a##1, 0x##a##4, 0x##a##5
#define Y(a) Z(a##0), Z(a##1), Z(a##4), Z(a##5)
#define X(a) Y(a##0), Y(a##1), Y(a##4), Y(a##5)
    X(0), X(1), X(4), X(5)
#undef X
#undef Y
#undef Z
};

static short xoffset[] = { -1, -1, 0, 1, 1, 1, 0, -1 };
static short yoffset[] = {  0, 1, 1, 1, 0, -1, -1, -1 };

static short facearray[9][12] =
{
    {  8, 9, 10, 11, -1, -1, -1, -1, 10, 11, 8, 9 }, // S
    {  5, 6, 7, 4, 8, 9, 10, 11, 9, 10, 11, 8 }, // SE
    { -1, -1, -1, -1, 5, 6, 7, 4, -1, -1, -1, -1 }, // E
    {  4, 5, 6, 7, 11, 8, 9, 10, 11, 8, 9, 10 }, // SW
    {  0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 }, // center
    {  1, 2, 3, 0, 0, 1, 2, 3, 5, 6, 7, 4 },   // NE
    { -1, -1, -1, -1, 7, 4, 5, 6, -1, -1, -1, -1 }, // W
    {  3, 0, 1, 2, 3, 0, 1, 2, 4, 5, 6, 7 },   // NW
    {  2, 3, 0, 1, -1, -1, -1, -1, 0, 1, 2, 3 }
}; // N

static uchar swaparray[9][12] =
{
    { 0, 0, 3 }, // S
    { 0, 0, 6 }, // SE
    { 0, 0, 0 }, // E
    { 0, 0, 5 }, // SW
    { 0, 0, 0 }, // center
    { 5, 0, 0 }, // NE
    { 0, 0, 0 }, // W
    { 6, 0, 0 }, // NW
    { 3, 0, 0 }
}; // N

static double fmodulo(double v1, double v2)
{
    if (v1 >= 0)
    {
        return (v1 < v2) ? v1 : fmod(v1, v2);
    }

    double tmp = fmod(v1, v2) + v2;
    return (tmp == v2) ? 0. : tmp;
}


void HEALPix::getCornerPoints(int level, int pix, SkyPoint *skyCoords)
{
    QVector3D v[4];
    // Transform from HealPIX convention to KStars
    QVector3D transformed[4];

    int nside = 1 << level;
    boundaries(nside, pix, 1, v);

    // From rectangular coordinates to Sky coordinates
    for (int i = 0; i < 4; i++)
    {
        transformed[i].setX(v[i].x());
        transformed[i].setY(v[i].y());
        transformed[i].setZ(v[i].z());

        double ra = 0, de = 0;
        xyz2sph(transformed[i], ra, de);
        de /= dms::DegToRad;
        ra /= dms::DegToRad;

        if (HIPSManager::Instance()->getCurrentFrame() == HIPSManager::HIPS_GALACTIC_FRAME)
        {
            dms galacticLong(ra);
            dms galacticLat(de);
            skyCoords[i].GalacticToEquatorial1950(&galacticLong, &galacticLat);
            skyCoords[i].B1950ToJ2000();
            skyCoords[i].setRA0(skyCoords[i].ra());
            skyCoords[i].setDec0(skyCoords[i].dec());
        }
        else
        {
            skyCoords[i].setRA0(ra / 15.0);
            skyCoords[i].setDec0(de);
        }

        skyCoords[i].updateCoords(KStarsData::Instance()->updateNum(), false);
        skyCoords[i].EquatorialToHorizontal(KStarsData::Instance()->lst(), KStarsData::Instance()->geo()->lat());
    }

}

void HEALPix::boundaries(qint32 nside, qint32 pix, int step, QVector3D *out)
{
    int ix, iy, fn;

    nest2xyf(nside, pix, &ix, &iy, &fn);

    double dc = 0.5 / nside;
    double xc = (ix + 0.5) / nside;
    double yc = (iy + 0.5) / nside;
    double d = 1. / (step * nside);

    for (int i = 0; i < step; ++i)
    {
        out[0] = toVec3(xc + dc - i * d, yc + dc, fn);
        out[1] = toVec3(xc - dc, yc + dc - i * d, fn);
        out[2] = toVec3(xc - dc + i * d, yc - dc, fn);
        out[3] = toVec3(xc + dc, yc - dc + i * d, fn);
    }
}

QVector3D HEALPix::toVec3(double fx, double fy, int face)
{
    double jr = jrll[face] - fx - fy;

    double locz;
    double locsth;
    double locphi;
    bool   lochave_sth = false;

    double nr;
    if (jr < 1)
    {
        nr = jr;
        double tmp = nr * nr / 3.;
        locz = 1 - tmp;
        if (locz > 0.99)
        {
            locsth = sqrt(tmp * (2. - tmp));
            lochave_sth = true;
        }
    }
    else if (jr > 3)
    {
        nr = 4 - jr;
        double tmp = nr * nr / 3.;
        locz = tmp - 1;
        if (locz < -0.99)
        {
            locsth = sqrt(tmp * (2. - tmp));
            lochave_sth = true;
        }
    }
    else
    {
        nr = 1;
        locz = (2 - jr) * 2. / 3.;
    }

    double tmp = jpll[face] * nr + fx - fy;
    if (tmp < 0) tmp += 8;
    if (tmp >= 8) tmp -= 8;
    locphi = (nr < 1e-15) ? 0 : (0.5 * dms::PI / 2.0 * tmp) / nr;

    double st = lochave_sth ? locsth : sqrt((1.0 - locz) * (1.0 + locz));

    QVector3D out;

    out.setX(st * cos(locphi));
    out.setY(st * sin(locphi));
    out.setZ(locz);

    return out;
}

void HEALPix::nest2xyf(int nside, int pix, int *ix, int *iy, int *face_num)
{
    int npface_ = nside * nside, raw;
    *face_num = pix / npface_;
    pix &= (npface_ - 1);
    raw = (pix & 0x5555) | ((pix & 0x55550000) >> 15);
    *ix = ctab[raw & 0xff] | (ctab[raw >> 8] << 4);
    pix >>= 1;
    raw = (pix & 0x5555) | ((pix & 0x55550000) >> 15);
    *iy = ctab[raw & 0xff] | (ctab[raw >> 8] << 4);
}

static int64_t spread_bits64 (int v)
{
    return  (int64_t)(utab[ v     & 0xff])
            | ((int64_t)(utab[(v >> 8) & 0xff]) << 16)
            | ((int64_t)(utab[(v >> 16) & 0xff]) << 32)
            | ((int64_t)(utab[(v >> 24) & 0xff]) << 48);
}


static int xyf2nest (int nside, int ix, int iy, int face_num)
{
    return (face_num * nside * nside) +
           (utab[ix & 0xff] | (utab[ix >> 8] << 16)
            | (utab[iy & 0xff] << 1) | (utab[iy >> 8] << 17));
}


/*
int static leadingZeros(qint32 value)
{
  int leadingZeros = 0;
  while(value != 0)
  {
    value = value >> 1;
    leadingZeros++;
  }

  return (32 - leadingZeros);
}
*/

/*
static int ilog2(qint32 arg)
{
  return 32 - leadingZeros(qMax(arg, 1));
}
*/

static int nside2order(qint32 nside)
{
    {
        int i = 0;
        while((nside >> (++i)) > 0);
        return --i;
    }
}

/** Returns the neighboring pixels of ipix.
     This method works in both RING and NEST schemes, but is
     considerably faster in the NEST scheme.
     @param nside defines the size of the neighboring area
     @param ipix the requested pixel number.
     @param result the result array
     @return array with indices of the neighboring pixels.
       The returned array contains (in this order)
       the pixel numbers of the SW, W, NW, N, NE, E, SE and S neighbor
       of ipix. If a neighbor does not exist (this can only happen
       for the W, N, E and S neighbors), its entry is set to -1. */

void HEALPix::neighbours(int nside, qint32 ipix, int *result)
{
    int ix, iy, face_num;

    int order = nside2order(nside);

    nest2xyf(nside, ipix, &ix, &iy, &face_num);

    qint32 nsm1 = nside - 1;
    if ((ix > 0) && (ix < nsm1) && (iy > 0) && (iy < nsm1))
    {
        qint32 fpix = (qint32)(face_num) << (2 * order),
               px0 = spread_bits64(ix  ), py0 = spread_bits64(iy  ) << 1,
               pxp = spread_bits64(ix + 1), pyp = spread_bits64(iy + 1) << 1,
               pxm = spread_bits64(ix - 1), pym = spread_bits64(iy - 1) << 1;

        result[0] = fpix + pxm + py0;
        result[1] = fpix + pxm + pyp;
        result[2] = fpix + px0 + pyp;
        result[3] = fpix + pxp + pyp;
        result[4] = fpix + pxp + py0;
        result[5] = fpix + pxp + pym;
        result[6] = fpix + px0 + pym;
        result[7] = fpix + pxm + pym;
    }
    else
    {
        for (int i = 0; i < 8; ++i)
        {
            int x = ix + xoffset[i];
            int y = iy + yoffset[i];
            int nbnum = 4;
            if (x < 0)
            {
                x += nside;
                nbnum -= 1;
            }
            else if (x >= nside)
            {
                x -= nside;
                nbnum += 1;
            }
            if (y < 0)
            {
                y += nside;
                nbnum -= 3;
            }
            else if (y >= nside)
            {
                y -= nside;
                nbnum += 3;
            }

            int f = facearray[nbnum][face_num];

            if (f >= 0)
            {
                int bits = swaparray[nbnum][face_num >> 2];
                if ((bits & 1) > 0) x = (int)(nside - x - 1);
                if ((bits & 2) > 0) y = (int)(nside - y - 1);
                if ((bits & 4) > 0)
                {
                    int tint = x;
                    x = y;
                    y = tint;
                }
                result[i] =  xyf2nest(nside, x, y, f);
            }
            else
                result[i] = -1;
        }
    }
}

int HEALPix::ang2pix_nest_z_phi (qint32 nside_, double z, double phi)
{
    double za = fabs(z);
    double tt = fmodulo(phi, twopi) * inv_halfpi; /* in [0,4) */
    int face_num, ix, iy;

    if (za <= twothird) /* Equatorial region */
    {
        double temp1 = nside_ * (0.5 + tt);
        double temp2 = nside_ * (z * 0.75);
        int jp = (int)(temp1 - temp2); /* index of  ascending edge line */
        int jm = (int)(temp1 + temp2); /* index of descending edge line */
        int ifp = jp / nside_; /* in {0,4} */
        int ifm = jm / nside_;
        face_num = (ifp == ifm) ? (ifp | 4) : ((ifp < ifm) ? ifp : (ifm + 8));

        ix = jm & (nside_ - 1);
        iy = nside_ - (jp & (nside_ - 1)) - 1;
    }
    else /* polar region, za > 2/3 */
    {
        int ntt = (int)tt, jp, jm;
        double tp, tmp;
        if (ntt >= 4) ntt = 3;
        tp = tt - ntt;
        tmp = nside_ * sqrt(3 * (1 - za));

        jp = (int)(tp * tmp); /* increasing edge line index */
        jm = (int)((1.0 - tp) * tmp); /* decreasing edge line index */
        if (jp >= nside_) jp = nside_ - 1; /* for points too close to the boundary */
        if (jm >= nside_) jm = nside_ - 1;
        if (z >= 0)
        {
            face_num = ntt;  /* in {0,3} */
            ix = nside_ - jm - 1;
            iy = nside_ - jp - 1;
        }
        else
        {
            face_num = ntt + 8; /* in {8,11} */
            ix =  jp;
            iy =  jm;
        }
    }

    return xyf2nest(nside_, ix, iy, face_num);
}

int HEALPix::getPix(int level, double ra, double dec)
{
    int nside = 1 << level;
    double polar[2] = { 0, 0 };

    if (HIPSManager::Instance()->getCurrentFrame() == HIPSManager::HIPS_GALACTIC_FRAME)
    {
        static QMatrix4x4 gl(-0.0548762f, -0.873437f, -0.483835f,  0,
                             0.4941100f, -0.444830f,  0.746982f,  0,
                             -0.8676660f, -0.198076f,  0.455984f,  0,
                             0,           0,          0,          1);

        double rcb = cos(dec);
        QVector3D xyz = QVector3D(rcb * cos(ra), rcb * sin(ra), sin(dec));

        xyz = gl.mapVector(xyz);
        xyz2sph(xyz, polar[1], polar[0]);
    }
    else
    {
        polar[0] = dec;
        polar[1] = ra;
    }

    return ang2pix_nest_z_phi(nside, sin(polar[0]), polar[1]);
}

void HEALPix::getPixChilds(int pix, int *childs)
{
    childs[0] = pix * 4 + 0;
    childs[1] = pix * 4 + 1;
    childs[2] = pix * 4 + 2;
    childs[3] = pix * 4 + 3;
}

void HEALPix::xyz2sph(const QVector3D &vec, double &l, double &b)
{
    double rho = vec.x() * vec.x() + vec.y() * vec.y();

    if (rho > 0)
    {
        l = atan2(vec.y(), vec.x());
        l -= floor(l / (M_PI * 2)) * (M_PI * 2);
        b = atan2(vec.z(), sqrt(rho));
    }
    else
    {
        l = 0.0;
        if (vec.z() == 0.0)
        {
            b = 0.0;
        }
        else
        {
            b = (vec.z() > 0.0) ? M_PI / 2. : -dms::PI / 2.;
        }
    }
}



