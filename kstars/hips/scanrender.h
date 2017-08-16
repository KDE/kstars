/*
  Copyright (C) 2015-2017, Pavel Mraz

  Copyright (C) 2017, Jasem Mutlaq

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#pragma once

#include <QtCore>
#include <QtGui>

#define MAX_BK_SCANLINES      32000

typedef struct
{
  int   scan[2];
  float uv[2][2];
} bkScan_t;


class ScanRender
{
  public:
    explicit ScanRender(void);
    void setBilinearInterpolationEnabled(bool enable);
    bool isBilinearInterpolationEnabled(void);
    void resetScanPoly(int sx, int sy);
    void scanLine(int x1, int y1, int x2, int y2);
    void scanLine(int x1, int y1, int x2, int y2, float u1, float v1, float u2, float v2);
    void renderPolygon(QColor col, QImage *dst);
    void renderPolygon(QImage *dst, QImage *src);
    void renderPolygon(int interpolation, QPointF *pts, QImage *pDest, QImage *pSrc, QPointF *uv);

    void renderPolygonNI(QImage *dst, QImage *src);
    void renderPolygonBI(QImage *dst, QImage *src);

    void renderPolygonAlpha(QImage *dst, QImage *src);
    void renderPolygonAlphaBI(QImage *dst, QImage *src);
    void renderPolygonAlphaNI(QImage *dst, QImage *src);

    void renderPolygonAlpha(QColor col, QImage *dst);
    void setOpacity(float opacity);

private:
    float    m_opacity;
    int      plMinY;
    int      plMaxY;
    int      m_sx;
    int      m_sy;
    bkScan_t scLR[MAX_BK_SCANLINES];
    bool     bBilinear;
};
