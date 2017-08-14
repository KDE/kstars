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

#include "hipsrenderer.h"

HiPSRenderer *g_hipsRenderer;

HiPSRenderer::HiPSRenderer()
{
  m_manager.init();
}

void HiPSRenderer::render(QImage *pDest)
{
  if (!m_manager.getParam()->render || m_manager.getParam()->url.isEmpty())
  {
    return;
  }

  m_HEALpix.setParam(m_manager.getParam());

  int level = 1;

  // FIXME
  //double minfov = D2R(58.5);

  // FIXME
  //while( level < m_manager.getParam()->max_level && view->fov < minfov) { minfov /= 2; level++; }

  m_renderedMap.clear();
  m_rendered = 0;
  m_blocks = 0;
  m_size = 0;

  double ra, dec;
  double cx, cy;

  // FIXME
  #if 0
  trfGetCenter(cx, cy);
  trfConvScrPtToXY(cx, cy, ra, dec);
  precess(&ra, &dec, view->jd, JD2000);
#endif

  bool allSky;

  if (level < 3)
  {
    allSky = true;
    level = 3;
  }
  else
  {
    allSky = false;
  }         

  int centerPix = m_HEALpix.getPix(level, ra, dec);

  // calculate healpix grid edge size in pixels
  // FIXME
#if 0
  SKPOINT pts[4];
  m_HEALpix.getCornerPoints(level, centerPix, pts);
  for (int i = 0; i < 2; i++) trfProjectPointNoCheck(&pts[i]);
  int size = sqrt(POW2(pts[0].sx - pts[1].sx) + POW2(pts[0].sy - pts[1].sy));
  if (size < 0) size = getParam()->tileWidth;  

  bool old = scanRender.isBillinearInt();
  scanRender.enableBillinearInt(getParam()->billinear && (size >= getParam()->tileWidth || allSky));

  renderRec(allSky, level, centerPix, painter, pDest);

  scanRender.enableBillinearInt(old);
#endif
}

void HiPSRenderer::renderRec(bool allsky, int level, int pix, QImage *pDest)
{
  if (m_renderedMap.contains(pix))
  {
    return;
  }

  if (renderPix(allsky, level ,pix, pDest))
  {
    m_renderedMap.insert(pix);
    int dirs[8];
    int nside = 1 << level;

    m_HEALpix.neighbours(nside, pix, dirs);    

    renderRec(allsky, level, dirs[0], pDest);
    renderRec(allsky, level, dirs[2], pDest);
    renderRec(allsky, level, dirs[4], pDest);
    renderRec(allsky, level, dirs[6], pDest);
  }    
}

bool HiPSRenderer::renderPix(bool allsky, int level, int pix, QImage *pDest)
{  
// FIXME
#if 0
  SKPOINT pts[4];
  bool freeImage;
  SKPOINT sub[4];

  m_HEALpix.getCornerPoints(level, pix, pts);

  if (SKPLANECheckFrustumToPolygon(trfGetFrustum(), pts, 4))
  {
    m_blocks++;

    for (int i = 0; i < 4; i++)
    {
      trfProjectPointNoCheck(&pts[i]);
    }    

    QImage *image = m_manager.getPix(allsky, level, pix, freeImage);    

    if (image)      
    {
      m_rendered++;
      m_size += image->byteCount();

      QPointF uv[16][4] = {{QPointF(.25, .25), QPointF(0.25, 0), QPointF(0, .0),QPointF(0, .25)},
                           {QPointF(.25, .5), QPointF(0.25, 0.25), QPointF(0, .25),QPointF(0, .5)},
                           {QPointF(.5, .25), QPointF(0.5, 0), QPointF(.25, .0),QPointF(.25, .25)},
                           {QPointF(.5, .5), QPointF(0.5, 0.25), QPointF(.25, .25),QPointF(.25, .5)},

                           {QPointF(.25, .75), QPointF(0.25, 0.5), QPointF(0, 0.5), QPointF(0, .75)},
                           {QPointF(.25, 1), QPointF(0.25, 0.75), QPointF(0, .75),QPointF(0, 1)},
                           {QPointF(.5, .75), QPointF(0.5, 0.5), QPointF(.25, .5),QPointF(.25, .75)},
                           {QPointF(.5, 1), QPointF(0.5, 0.75), QPointF(.25, .75),QPointF(.25, 1)},

                           {QPointF(.75, .25), QPointF(0.75, 0), QPointF(0.5, .0),QPointF(0.5, .25)},
                           {QPointF(.75, .5), QPointF(0.75, 0.25), QPointF(0.5, .25),QPointF(0.5, .5)},
                           {QPointF(1, .25), QPointF(1, 0), QPointF(.75, .0),QPointF(.75, .25)},
                           {QPointF(1, .5), QPointF(1, 0.25), QPointF(.75, .25),QPointF(.75, .5)},

                           {QPointF(.75, .75), QPointF(0.75, 0.5), QPointF(0.5, .5),QPointF(0.5, .75)},
                           {QPointF(.75, 1), QPointF(0.75, 0.75), QPointF(0.5, .75),QPointF(0.5, 1)},
                           {QPointF(1, .75), QPointF(1, 0.5), QPointF(.75, .5),QPointF(.75, .75)},
                           {QPointF(1, 1), QPointF(1, 0.75), QPointF(.75, .75),QPointF(.75, 1)},
                          };

      int ch[4];

      m_HEALpix.getPixChilds(pix, ch);

      int j = 0;
      for (int q = 0; q < 4; q++)
      {
        int ch2[4];
        m_HEALpix.getPixChilds(ch[q], ch2);

        for (int w = 0; w < 4; w++)
        {
          m_HEALpix.getCornerPoints(level + 2, ch2[w], sub);

          for (int i = 0; i < 4; i++) trfProjectPointNoCheck(&sub[i]);
          scanRender.renderPolygon(painter, 3, sub, pDest, image, uv[j]);
          j++;
        }
      }

      if (freeImage)
      {
        delete image;
      }
    }        

    if (getParam()->showGrid)
    {
      painter->setPen(g_skSet.map.drawing.color);
      painter->drawLine(pts[0].sx, pts[0].sy, pts[1].sx, pts[1].sy);
      painter->drawLine(pts[1].sx, pts[1].sy, pts[2].sx, pts[2].sy);
      painter->drawLine(pts[2].sx, pts[2].sy, pts[3].sx, pts[3].sy);
      painter->drawLine(pts[3].sx, pts[3].sy, pts[0].sx, pts[0].sy);
      painter->drawCText((pts[0].sx + pts[1].sx + pts[2].sx + pts[3].sx) / 4,
                         (pts[0].sy + pts[1].sy + pts[2].sy + pts[3].sy) / 4, QString::number(pix) + " / " + QString::number(level));
    }

    return true;
  }


#endif
  return false;
}

void HiPSRenderer::setParam(const hipsParams_t &param)
{
  m_manager.setParam(param);
}

hipsParams_t *HiPSRenderer::getParam()
{
  return m_manager.getParam();
}

HiPSManager *HiPSRenderer::manager()
{
  return &m_manager;
}
