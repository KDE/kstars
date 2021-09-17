/*
    SPDX-FileCopyrightText: 2015-2017 Pavel Mraz

    SPDX-FileCopyrightText: 2017 Jasem Mutlaq

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "scanrender.h"

//#include <omp.h>
//#define PARALLEL_OMP

#define FRAC(f, from, to)      ((((f) - (from)) / (double)((to) - (from))))
#define LERP(f, mi, ma)        ((mi) + (f) * ((ma) - (mi)))
#define CLAMP(v, mi, ma)       (((v) < (mi)) ? (mi) : ((v) > (ma)) ? (ma) : (v))
#define SIGN(x)                ((x) >= 0 ? 1.0 : -1.0)

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-align"

//////////////////////////////
ScanRender::ScanRender(void)
//////////////////////////////
{
}

/////////////////////////////////////////////////
void ScanRender::setBilinearInterpolationEnabled(bool enable)
/////////////////////////////////////////////////
{
  bBilinear = enable;
}

//////////////////////////////////
bool ScanRender::isBilinearInterpolationEnabled()
//////////////////////////////////
{
  return(bBilinear);
}

///////////////////////////////////////////////
void ScanRender::resetScanPoly(int sx, int sy)
///////////////////////////////////////////////
{
  plMinY =  999999;
  plMaxY = -999999;

  if (sy >= MAX_BK_SCANLINES)
  {
    qDebug("ScanRender::resetScanPoly fail!");
    return;
  }

  m_sx = sx;
  m_sy = sy;
}

//////////////////////////////////////////////////////////
void ScanRender::scanLine(int x1, int y1, int x2, int y2)
//////////////////////////////////////////////////////////
{
  int side;

  if (y1 > y2)
  {
    qSwap(x1, x2);
    qSwap(y1, y2);
    side = 0;
  }
  else
  {
    side = 1;
  }

  if (y2 < 0)
  {
    return; // offscreen
  }

  if (y1 >= m_sy)
  {
    return; // offscreen
  }

  float dy = (float)(y2 - y1);

  if (dy == 0) // hor. line
  {
    return;
  }

  float dx = (float)(x2 - x1) / dy;
  float x = x1;
  int   y;

  if (y2 >= m_sy)
  {
    y2 = m_sy - 1;
  }

  if (y1 < 0)
  { // partially off screen
    float m = (float) -y1;

    x += dx * m;
    y1 = 0;
  }

  int minY = qMin(y1, y2);
  int maxY = qMax(y1, y2);

  if (minY < plMinY)
    plMinY = minY;
  if (maxY > plMaxY)
    plMaxY = maxY;

#define SCAN_FIX_PT 1

#if SCAN_FIX_PT

#define FP 16

  int fx = (int)(x * (float)(1 << FP));
  int fdx = (int)(dx * (float)(1 << FP));

  for (y = y1; y <= y2; y++)
  {
    scLR[y].scan[side] = fx >> FP;
    fx += fdx;
  }

#else

  for (y = y1; y <= y2; y++)
  {
    if (side == 1)
    { // side left
      scLR[y].scan[0] = float2int(x);
    }
    else
    { // side right
      scLR[y].scan[1] = float2int(x);
    }
    x += dx;
  }

#endif
}


//////////////////////////////////////////////////////////////////////////////////////////////////
void ScanRender::scanLine(int x1, int y1, int x2, int y2, float u1, float v1, float u2, float v2)
//////////////////////////////////////////////////////////////////////////////////////////////////
{
  int side;

  if (y1 > y2)
  {
    qSwap(x1, x2);
    qSwap(y1, y2);
    qSwap(u1, u2);
    qSwap(v1, v2);
    side = 0;
  }
  else
  {
    side = 1;
  }

  if (y2 < 0)
    return; // offscreen
  if (y1 >= m_sy)
    return; // offscreen

  float dy = (float)(y2 - y1);

  if (dy == 0) // hor. line
    return;

  float dx = (float)(x2 - x1) / dy;
  float x = x1;
  int   y;

  if (y2 >= m_sy)
    y2 = m_sy - 1;

  float duv[2];
  float uv[2] = {u1, v1};

  duv[0] = (u2 - u1) / dy;
  duv[1] = (v2 - v1) / dy;

  if (y1 < 0)
  { // partially off screen
    float m = (float) -y1;

    uv[0] += duv[0] * m;
    uv[1] += duv[1] * m;

    x += dx * m;
    y1 = 0;
  }

  int minY = qMin(y1, y2);
  int maxY = qMax(y1, y2);

  if (minY < plMinY)
    plMinY = minY;
  if (maxY > plMaxY)
    plMaxY = maxY;

  for (y = y1; y <= y2; y++)
  {
    scLR[y].scan[side] = (int)x;
    scLR[y].uv[side][0] = uv[0];
    scLR[y].uv[side][1] = uv[1];

    x += dx;

    uv[0] += duv[0];
    uv[1] += duv[1];
  }
}


////////////////////////////////////////////////////////
void ScanRender::renderPolygon(QColor col, QImage *dst)
////////////////////////////////////////////////////////
{ 
  quint32   c = col.rgb();
  quint32  *bits = (quint32 *)dst->bits();
  int       dw = dst->width();
  bkScan_t *scan = scLR;

  for (int y = plMinY; y <= plMaxY; y++)
  {
    int px1 = scan[y].scan[0];
    int px2 = scan[y].scan[1];

    if (px1 > px2)
    {
      qSwap(px1, px2);
    }

    if (px1 < 0)
    {
      px1 = 0;
      if (px2 < 0)
      {
        continue;
      }
    }

    if (px2 >= m_sx)
    {
      if (px1 >= m_sx)
      {
        continue;
      }
      px2 = m_sx - 1;
    }

    quint32 *pDst = bits + (y * dw) + px1;    
    for (int x = px1; x < px2; x++)
    {
      *pDst = c;
      pDst++;
    }    
  }
}


/////////////////////////////////////////////////////////////
void ScanRender::renderPolygonAlpha(QColor col, QImage *dst)
/////////////////////////////////////////////////////////////
{
  quint32   c = col.rgba();
  quint32  *bits = (quint32 *)dst->bits();
  int       dw = dst->width();
  bkScan_t *scan = scLR;
  float     a = qAlpha(c) / 256.0f;
  int       rc = qRed(c);
  int       gc = qGreen(c);
  int       bc = qBlue(c);

  for (int y = plMinY; y <= plMaxY; y++)
  {
    int px1 = scan[y].scan[0];
    int px2 = scan[y].scan[1];    

    if (px1 > px2)
    {
      qSwap(px1, px2);
    }

    if (px1 < 0)
    {
      px1 = 0;
    }

    if (px2 >= m_sx)
    {
      px2 = m_sx - 1;
    }

    quint32 *pDst = bits + (y * dw) + px1;
    for (int x = px1; x < px2; x++)
    {
      QRgb rgbd = *pDst;     

      *pDst = qRgb(LERP(a, qRed(rgbd), rc),
                   LERP(a, qGreen(rgbd), gc),
                   LERP(a, qBlue(rgbd), bc)
                  );     
      pDst++;
    }
  }
}

void ScanRender::setOpacity(float opacity)
{
  m_opacity = opacity;
}

/////////////////////////////////////////////////////////
void ScanRender::renderPolygon(QImage *dst, QImage *src)
/////////////////////////////////////////////////////////
{
  if (bBilinear)
    renderPolygonBI(dst, src);
  else
    renderPolygonNI(dst, src);
}

void ScanRender::renderPolygon(int interpolation, QPointF *pts, QImage *pDest, QImage *pSrc, QPointF *uv)
{
  QPointF Auv = uv[0];
  QPointF Buv = uv[1];
  QPointF Cuv = uv[2];
  QPointF Duv = uv[3];  

  if (interpolation < 2)
  {
    resetScanPoly(pDest->width(), pDest->height());
    scanLine(pts[0].x(), pts[0].y(), pts[1].x(), pts[1].y(), 1, 1, 1, 0);
    scanLine(pts[1].x(), pts[1].y(), pts[2].x(), pts[2].y(), 1, 0, 0, 0);
    scanLine(pts[2].x(), pts[2].y(), pts[3].x(), pts[3].y(), 0, 0, 0, 1);
    scanLine(pts[3].x(), pts[3].y(), pts[0].x(), pts[0].y(), 0, 1, 1, 1);
    renderPolygon(pDest, pSrc);
    return;
  }

  QPointF A = pts[0];
  QPointF B = pts[1];
  QPointF C = pts[2];
  QPointF D = pts[3];

  //p->setPen(Qt::green);

  for (int i = 0; i < interpolation; i++)
  {
    QPointF P1 = A + i * (D - A) / interpolation;
    QPointF P1uv = Auv + i * (Duv - Auv) / interpolation;

    QPointF P2 = B + i * (C - B) / interpolation;
    QPointF P2uv = Buv + i * (Cuv - Buv) / interpolation;

    QPointF Q1 = A + (i + 1) * (D - A) / interpolation;
    QPointF Q1uv = Auv + (i + 1) * (Duv - Auv) / interpolation;

    QPointF Q2 = B + (i + 1) * (C - B) / interpolation;
    QPointF Q2uv = Buv + (i + 1) * (Cuv - Buv) / interpolation;

    for (int j = 0; j < interpolation; j++)
    {
      QPointF A1 = P1 + j * (P2 - P1) / interpolation;
      QPointF A1uv = P1uv + j * (P2uv - P1uv) / interpolation;

      QPointF B1 = P1 + (j + 1) * (P2 - P1) / interpolation;
      QPointF B1uv = P1uv + (j + 1) * (P2uv - P1uv) / interpolation;

      QPointF C1 = Q1 + (j + 1) * (Q2 - Q1) / interpolation;
      QPointF C1uv = Q1uv + (j + 1) * (Q2uv - Q1uv) / interpolation;

      QPointF D1 = Q1 + j * (Q2 - Q1) / interpolation;
      QPointF D1uv = Q1uv + j * (Q2uv - Q1uv) / interpolation;

      resetScanPoly(pDest->width(), pDest->height());
      scanLine(A1.x(), A1.y(), B1.x(), B1.y(), A1uv.x(), A1uv.y(), B1uv.x(), B1uv.y());
      scanLine(B1.x(), B1.y(), C1.x(), C1.y(), B1uv.x(), B1uv.y(), C1uv.x(), C1uv.y());
      scanLine(C1.x(), C1.y(), D1.x(), D1.y(), C1uv.x(), C1uv.y(), D1uv.x(), D1uv.y());
      scanLine(D1.x(), D1.y(), A1.x(), A1.y(), D1uv.x(), D1uv.y(), A1uv.x(), A1uv.y());
      renderPolygon(pDest, pSrc);

      //p->drawLine(A1, B1);
      //p->drawLine(B1, C1);
      //p->drawLine(C1, D1);
      //p->drawLine(D1, A1);
    }
  }
}

///////////////////////////////////////////////////////////
void ScanRender::renderPolygonNI(QImage *dst, QImage *src)
///////////////////////////////////////////////////////////
{
  int w = dst->width();
  int sw = src->width();
  int sh = src->height();
  float tsx = src->width() - 1;
  float tsy = src->height() - 1;
  const quint32 *bitsSrc = (quint32 *)src->constBits();
  quint32 *bitsDst = (quint32 *)dst->bits();
  bkScan_t *scan = scLR;
  bool bw = src->format() == QImage::Format_Indexed8 || src->format() == QImage::Format_Grayscale8;      

  //#pragma omp parallel for
  for (int y = plMinY; y <= plMaxY; y++)
  {   
    if (scan[y].scan[0] > scan[y].scan[1])
    {
      qSwap(scan[y].scan[0], scan[y].scan[1]);
      qSwap(scan[y].uv[0][0], scan[y].uv[1][0]);
      qSwap(scan[y].uv[0][1], scan[y].uv[1][1]);
    }    

    int px1 = scan[y].scan[0];
    int px2 = scan[y].scan[1];

    float dx = px2 - px1;
    if (dx == 0)
      continue;

    float duv[2];
    float uv[2];

    duv[0] = (float)(scan[y].uv[1][0] - scan[y].uv[0][0]) / dx;
    duv[1] = (float)(scan[y].uv[1][1] - scan[y].uv[0][1]) / dx;

    uv[0] = scan[y].uv[0][0];
    uv[1] = scan[y].uv[0][1];

    if (px1 < 0)
    {
      float m = (float)-px1;

      px1 = 0;
      uv[0] += duv[0] * m;
      uv[1] += duv[1] * m;
    }

    if (px2 >= w)
      px2 = w - 1;

    uv[0] *= tsx;
    uv[1] *= tsy;

    duv[0] *= tsx;
    duv[1] *= tsy;

    quint32 *pDst = bitsDst + (y * w) + px1;

    int fuv[2];
    int fduv[2];

    fuv[0] = uv[0] * 65536;
    fuv[1] = uv[1] * 65536;

    fduv[0] = duv[0] * 65536;    
    fduv[1] = duv[1] * 65536;

    fuv[0] = CLAMP(fuv[0], 0, (sw - 1) * 65536.);
    fuv[1] = CLAMP(fuv[1], 0, (sh - 1) * 65536.);

    if (bw)
    {
      for (int x = px1; x < px2; x++)
      {
        const uchar *pSrc = (uchar *)bitsSrc + (fuv[0] >> 16) + ((fuv[1] >> 16) * sw);
        *pDst = qRgb(*pSrc, *pSrc, *pSrc);
        pDst++;

        fuv[0] += fduv[0];
        fuv[1] += fduv[1];
      }      
    }
    else
    {                  
      for (int x = px1; x < px2; x++)
      {        
        int offset = (fuv[0] >> 16) + ((fuv[1] >> 16) * sw);        

        const quint32 *pSrc = bitsSrc + offset;
        *pDst = (*pSrc) | (0xFF << 24);

        pDst++;

        fuv[0] += fduv[0];
        fuv[1] += fduv[1];
      }
    }
  }
}


///////////////////////////////////////////////////////////
void ScanRender::renderPolygonBI(QImage *dst, QImage *src)
///////////////////////////////////////////////////////////
{
  int w = dst->width();
  int sw = src->width();
  int sh = src->height();
  float tsx = src->width() - 1;
  float tsy = src->height() - 1;
  const quint32 *bitsSrc = (quint32 *)src->constBits();
  const uchar *bitsSrc8 = (uchar *)src->constBits();
  quint32 *bitsDst = (quint32 *)dst->bits();
  bkScan_t *scan = scLR;
  bool bw = src->format() == QImage::Format_Indexed8 || src->format() == QImage::Format_Grayscale8;

#ifdef PARALLEL_OMP
  #pragma omp parallel for
#endif
  for (int y = plMinY; y <= plMaxY; y++)
  {
    if (scan[y].scan[0] > scan[y].scan[1])
    {
      qSwap(scan[y].scan[0], scan[y].scan[1]);
      qSwap(scan[y].uv[0][0], scan[y].uv[1][0]);
      qSwap(scan[y].uv[0][1], scan[y].uv[1][1]);
    }

    int px1 = scan[y].scan[0];
    int px2 = scan[y].scan[1];

    float dx = px2 - px1;
    if (dx == 0)
      continue;

    float duv[2];
    float uv[2];

    duv[0] = (float)(scan[y].uv[1][0] - scan[y].uv[0][0]) / dx;
    duv[1] = (float)(scan[y].uv[1][1] - scan[y].uv[0][1]) / dx;

    uv[0] = scan[y].uv[0][0];
    uv[1] = scan[y].uv[0][1];

    if (px1 < 0)
    {
      float m = (float)-px1;

      px1 = 0;
      uv[0] += duv[0] * m;
      uv[1] += duv[1] * m;
    }

    if (px2 >= w)
      px2 = w - 1;

    uv[0] *= tsx;
    uv[1] *= tsy;

    duv[0] *= tsx;
    duv[1] *= tsy;

    int size = sw * sh;

    quint32 *pDst = bitsDst + (y * w) + px1;
    if (bw)
    {
      for (int x = px1; x < px2; x++)
      {
        float x_diff = uv[0] - static_cast<int>(uv[0]);
        float y_diff = uv[1] - static_cast<int>(uv[1]);
        float x_1diff = 1 - x_diff;
        float y_1diff = 1 - y_diff;

        int index = ((int)uv[0] + ((int)uv[1] * sw));

        uchar a = bitsSrc8[index];
        uchar b = bitsSrc8[(index + 1) % size];
        uchar c = bitsSrc8[(index + sw) % size];
        uchar d = bitsSrc8[(index + sw + 1) % size];

        int val = (a&0xff)*(x_1diff)*(y_1diff) + (b&0xff)*(x_diff)*(y_1diff) +
                  (c&0xff)*(y_diff)*(x_1diff)   + (d&0xff)*(x_diff*y_diff);

        *pDst = 0xff000000 |
                ((((int)val)<<16)&0xff0000) |
                ((((int)val)<<8)&0xff00) |
                ((int)val) ;
        pDst++;

        uv[0] += duv[0];
        uv[1] += duv[1];
      }
    }
    else
    {
      for (int x = px1; x < px2; x++)
      {
        float x_diff = uv[0] - static_cast<int>(uv[0]);
        float y_diff = uv[1] - static_cast<int>(uv[1]);
        float x_1diff = 1 - x_diff;
        float y_1diff = 1 - y_diff;

        int index = ((int)uv[0] + ((int)uv[1] * sw));

        quint32 a = bitsSrc[index];
        quint32 b = bitsSrc[(index + 1) % size];
        quint32 c = bitsSrc[(index + sw) % size];
        quint32 d = bitsSrc[(index + sw + 1) % size];

        int qxy1 = (x_1diff * y_1diff) * 65536;
        int qxy2 =(x_diff * y_1diff) * 65536;
        int qxy = (x_diff * y_diff) * 65536;
        int qyx1 = (y_diff * x_1diff) * 65536;

        // blue element
        int blue = ((a&0xff)*(qxy1) + (b&0xff)*(qxy2) + (c&0xff)*(qyx1)  + (d&0xff)*(qxy)) >> 16;

        // green element
        int green = (((a>>8)&0xff)*(qxy1) + ((b>>8)&0xff)*(qxy2) + ((c>>8)&0xff)*(qyx1)  + ((d>>8)&0xff)*(qxy)) >> 16;

        // red element
        int red = (((a>>16)&0xff)*(qxy1) + ((b>>16)&0xff)*(qxy2) +((c>>16)&0xff)*(qyx1)  + ((d>>16)&0xff)*(qxy)) >> 16;

        *pDst = 0xff000000 | (((red)<<16)&0xff0000) | (((green)<<8)&0xff00) | (blue);

        pDst++;

        uv[0] += duv[0];
        uv[1] += duv[1];
      }
    }
  }
}

void ScanRender::renderPolygonAlpha(QImage *dst, QImage *src)
{
  if (bBilinear)
    renderPolygonAlphaBI(dst, src);
  else
    renderPolygonAlphaNI(dst, src);
}


void ScanRender::renderPolygonAlphaBI(QImage *dst, QImage *src)
{
  int w = dst->width();
  int sw = src->width();
  int sh = src->height();
  float tsx = src->width() - 1;
  float tsy = src->height() - 1;
  const quint32 *bitsSrc = (quint32 *)src->constBits();  
  quint32 *bitsDst = (quint32 *)dst->bits();
  bkScan_t *scan = scLR;
  bool bw = src->format() == QImage::Format_Indexed8;
  float opacity = (m_opacity / 65536.) * 0.00390625f;

#ifdef PARALLEL_OMP
  #pragma omp parallel for shared(bitsDst, bitsSrc, scan, tsx, tsy, w, sw)
#endif
  for (int y = plMinY; y <= plMaxY; y++)
  {
    if (scan[y].scan[0] > scan[y].scan[1])
    {
      qSwap(scan[y].scan[0], scan[y].scan[1]);
      qSwap(scan[y].uv[0][0], scan[y].uv[1][0]);
      qSwap(scan[y].uv[0][1], scan[y].uv[1][1]);
    }

    int px1 = scan[y].scan[0];
    int px2 = scan[y].scan[1];

    float dx = px2 - px1;
    if (dx == 0)
      continue;

    float duv[2];
    float uv[2];

    duv[0] = (float)(scan[y].uv[1][0] - scan[y].uv[0][0]) / dx;
    duv[1] = (float)(scan[y].uv[1][1] - scan[y].uv[0][1]) / dx;

    uv[0] = scan[y].uv[0][0];
    uv[1] = scan[y].uv[0][1];

    if (px1 < 0)
    {
      float m = (float)-px1;

      px1 = 0;
      uv[0] += duv[0] * m;
      uv[1] += duv[1] * m;
    }

    if (px2 >= w)
      px2 = w - 1;

    uv[0] *= tsx;
    uv[1] *= tsy;

    duv[0] *= tsx;
    duv[1] *= tsy;

    int size = sw * sh;

    quint32 *pDst = bitsDst + (y * w) + px1;
    if (bw)
    {
      /*
      for (int x = px1; x < px2; x++)
      {
        float x_diff = uv[0] - static_cast<int>(uv[0]);
        float y_diff = uv[1] - static_cast<int>(uv[1]);
        float x_1diff = 1 - x_diff;
        float y_1diff = 1 - y_diff;

        int index = ((int)uv[0] + ((int)uv[1] * sw));

        uchar a = bitsSrc8[index];
        uchar b = bitsSrc8[(index + 1) % size];
        uchar c = bitsSrc8[(index + sw) % size];
        uchar d = bitsSrc8[(index + sw + 1) % size];

        int val = (a&0xff)*(x_1diff)*(y_1diff) + (b&0xff)*(x_diff)*(y_1diff) +
                  (c&0xff)*(y_diff)*(x_1diff)   + (d&0xff)*(x_diff*y_diff);


        *pDst = 0xff000000 |
                ((((int)val)<<16)&0xff0000) |
                ((((int)val)<<8)&0xff00) |
                ((int)val) ;
        pDst++;

        uv[0] += duv[0];
        uv[1] += duv[1];
      }
      */
    }
    else
    {      
      for (int x = px1; x < px2; x++)
      {
        float x_diff = uv[0] - static_cast<int>(uv[0]);
        float y_diff = uv[1] - static_cast<int>(uv[1]);
        float x_1diff = 1 - x_diff;
        float y_1diff = 1 - y_diff;

        int index = ((int)uv[0] + ((int)uv[1] * sw));

        quint32 a = bitsSrc[index];
        quint32 b = bitsSrc[(index + 1) % size];
        quint32 c = bitsSrc[(index + sw) % size];
        quint32 d = bitsSrc[(index + sw + 1) % size];

        int x1y1 = (x_1diff * y_1diff) * 65536;
        int xy = (x_diff * y_diff) * 65536;
        int x1y = (y_diff * x_1diff) * 65536;
        int xy1 = (x_diff *y_1diff) * 65536;

        float alpha = ((((a>>24)&0xff)*(x1y1) + ((b>>24)&0xff)*(xy1) +
                        ((c>>24)&0xff)*(x1y) + ((d>>24)&0xff)*(xy)) * opacity);

        if (alpha > 0.00390625f) // > 1 / 256.
        {
          // blue element
          int blue = ((a&0xff)*(x1y1) + (b&0xff)* (xy1) +
                     (c&0xff)*(x1y)  + (d&0xff)*(xy)) >> 16;

          // green element
          int green = (((a>>8)&0xff)*(x1y1) + ((b>>8)&0xff)*(xy1) +
                      ((c>>8)&0xff)*(x1y)  + ((d>>8)&0xff)*(xy)) >> 16;

          // red element
          int red = (((a>>16)&0xff)*(x1y1) + ((b>>16)&0xff)*(xy1) +
                    ((c>>16)&0xff)*(x1y)  + ((d>>16)&0xff)*(xy)) >> 16;

          int rd = qRed(*pDst);
          int gd = qGreen(*pDst);
          int bd = qBlue(*pDst);

          *pDst = qRgb(LERP(alpha, rd,  red), LERP(alpha, gd, green),  LERP(alpha, bd, blue));
        }

        pDst++;

        uv[0] += duv[0];
        uv[1] += duv[1];
      }
    }
  }
}


////////////////////////////////////////////////////////////////
void ScanRender::renderPolygonAlphaNI(QImage *dst, QImage *src)
////////////////////////////////////////////////////////////////
{
  int w = dst->width();
  int sw = src->width();
  float tsx = src->width() - 1;
  float tsy = src->height() - 1;
  const quint32 *bitsSrc = (quint32 *)src->constBits();
  quint32 *bitsDst = (quint32 *)dst->bits();
  bkScan_t *scan = scLR;
  float opacity = 0.00390625f * m_opacity;    

#ifdef PARALLEL_OMP
  #pragma omp parallel for shared(bitsDst, bitsSrc, scan, tsx, tsy, w, sw)
#endif
  for (int y = plMinY; y <= plMaxY; y++)
  {
    if (scan[y].scan[0] > scan[y].scan[1])
    {
      qSwap(scan[y].scan[0], scan[y].scan[1]);
      qSwap(scan[y].uv[0][0], scan[y].uv[1][0]);
      qSwap(scan[y].uv[0][1], scan[y].uv[1][1]);
    }

    int px1 = scan[y].scan[0];
    int px2 = scan[y].scan[1];

    float dx = px2 - px1;
    if (dx == 0)
      continue;

    float duv[2];
    float uv[2];

    duv[0] = (float)(scan[y].uv[1][0] - scan[y].uv[0][0]) / dx;
    duv[1] = (float)(scan[y].uv[1][1] - scan[y].uv[0][1]) / dx;

    uv[0] = scan[y].uv[0][0];
    uv[1] = scan[y].uv[0][1];

    if (px1 < 0)
    {
      float m = (float)-px1;

      px1 = 0;
      uv[0] += duv[0] * m;
      uv[1] += duv[1] * m;
    }

    if (px2 >= w)
      px2 = w - 1;

    quint32 *pDst = bitsDst + (y * w) + px1;

    uv[0] *= tsx;
    uv[1] *= tsy;

    duv[0] *= tsx;
    duv[1] *= tsy;

    for (int x = px1; x < px2; x++)
    {
      const quint32 *pSrc = bitsSrc + ((int)(uv[0])) + ((int)(uv[1]) * sw);
      QRgb rgbs = *pSrc;
      QRgb rgbd = *pDst;
      float a = qAlpha(*pSrc) * opacity;

      if (a > 0.00390625f)
      {
        *pDst = qRgb(LERP(a, qRed(rgbd), qRed(rgbs)),
                     LERP(a, qGreen(rgbd), qGreen(rgbs)),
                     LERP(a, qBlue(rgbd), qBlue(rgbs))
                     );        
      }
      pDst++;

      uv[0] += duv[0];
      uv[1] += duv[1];
    }
  }  
}

#pragma GCC diagnostic pop
