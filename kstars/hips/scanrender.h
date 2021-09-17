/*
    SPDX-FileCopyrightText: 2015-2017 Pavel Mraz

    SPDX-FileCopyrightText: 2017 Jasem Mutlaq

    SPDX-License-Identifier: GPL-2.0-or-later
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
    float    m_opacity { 1.0f };
    int      plMinY { 0 };
    int      plMaxY { 0 };
    int      m_sx { 0 };
    int      m_sy { 0 };
    bkScan_t scLR[MAX_BK_SCANLINES];
    bool     bBilinear { false };
};
