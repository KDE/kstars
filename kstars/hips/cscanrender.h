#ifndef CSCANRENDER_H
#define CSCANRENDER_H

#include <QtCore>
#include <QtGui>

#include "vecmath.h"

#define MAX_BK_SCANLINES      32000

typedef struct
{
  int   scan[2];
  float uv[2][2];
} bkScan_t;


class CScanRender
{
  public:
    explicit CScanRender(void);
    void enableBillinearInt(bool enable);
    bool isBillinearInt(void);
    void resetScanPoly(int sx, int sy);
    void scanLine(int x1, int y1, int x2, int y2);
    void scanLine(int x1, int y1, int x2, int y2, float u1, float v1, float u2, float v2);
    void renderPolygon(QColor col, QImage *dst);
    void renderPolygon(QImage *dst, QImage *src);
    void renderPolygon(QPainter *p, int interpolation, SKPOINT *pts, QImage *pDest, QImage *pSrc, QPointF *uv);

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

extern CScanRender scanRender;

#endif // CSCANRENDER_H
