/*
    SPDX-FileCopyrightText: 2015-2017 Pavel Mraz

    SPDX-FileCopyrightText: 2017 Jasem Mutlaq

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "hipsrenderer.h"

#include "colorscheme.h"
#include "kstars_debug.h"
#include "Options.h"
#include "skymap.h"
#include "skyqpainter.h"
#include "projections/projector.h"

HIPSRenderer::HIPSRenderer()
{
    m_scanRender.reset(new ScanRender());
    m_HEALpix.reset(new HEALPix());
}

bool HIPSRenderer::render(uint16_t w, uint16_t h, QImage *hipsImage, const Projector *m_proj)
{
  gridColor = KStarsData::Instance()->colorScheme()->colorNamed("HIPSGridColor").name();

  m_projector = m_proj;

  int level = 1;

  // Min FOV in Degrees
  double minfov = 58.5;
  double fov    = m_proj->fov() * w / (double) h;

  // Find suitable level for current FOV
  while( level < HIPSManager::Instance()->getCurrentOrder() && fov < minfov)
  {
      minfov /= 2;
      level++;
  }

  m_renderedMap.clear();
  m_rendered = 0;
  m_blocks = 0;
  m_size = 0;

  SkyPoint center = SkyMap::Instance()->getCenterPoint();
  //center.deprecess(KStarsData::Instance()->updateNum());
  center.catalogueCoord(KStarsData::Instance()->updateNum()->julianDay());

  double ra = center.ra0().radians();
  double de = center.dec0().radians();

  if (std::isnan(ra) || std::isnan(de))
  {
      qCWarning(KSTARS) << "NAN Center, HiPS draw canceled.";
      return false;
  }

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

  int centerPix = m_HEALpix->getPix(level, ra, de);

  SkyPoint cornerSkyCoords[4];
  QPointF tileLine[2];
  m_HEALpix->getCornerPoints(level, centerPix, cornerSkyCoords);

  //qCDebug(KSTARS) << "#" << i+1 << "RA0" << cornerSkyCoords[i].ra0().toHMSString();
  //qCDebug(KSTARS) << "#" << i+1 << "DE0" << cornerSkyCoords[i].dec0().toHMSString();

  //qCDebug(KSTARS) << "#" << i+1 << "X" << tileLine[i].x();
  //qCDebug(KSTARS) << "#" << i+1 << "Y" << tileLine[i].y();

  for (int i=0; i < 2; i++)
      tileLine[i] = m_projector->toScreen(&cornerSkyCoords[i]);

  int size = std::sqrt(std::pow(tileLine[0].x()-tileLine[1].x(), 2) + std::pow(tileLine[0].y()-tileLine[1].y(), 2));
  if (size < 0)
      size = HIPSManager::Instance()->getCurrentTileWidth();

  bool old = m_scanRender->isBilinearInterpolationEnabled();
  m_scanRender->setBilinearInterpolationEnabled(Options::hIPSBiLinearInterpolation() && (size >= HIPSManager::Instance()->getCurrentTileWidth() || allSky));

  renderRec(allSky, level, centerPix, hipsImage);

  m_scanRender->setBilinearInterpolationEnabled(old);

  return true;
}

void HIPSRenderer::renderRec(bool allsky, int level, int pix, QImage *pDest)
{
  if (m_renderedMap.contains(pix))
  {
    return;
  }

  if (renderPix(allsky, level, pix, pDest))
  {
    m_renderedMap.insert(pix);
    int dirs[8];
    int nside = 1 << level;

    m_HEALpix->neighbours(nside, pix, dirs);

    renderRec(allsky, level, dirs[0], pDest);
    renderRec(allsky, level, dirs[2], pDest);
    renderRec(allsky, level, dirs[4], pDest);
    renderRec(allsky, level, dirs[6], pDest);
  }
}

bool HIPSRenderer::renderPix(bool allsky, int level, int pix, QImage *pDest)
{
  SkyPoint cornerSkyCoords[4];
  QPointF cornerScreenCoords[4];
  bool freeImage = false;

  m_HEALpix->getCornerPoints(level, pix, cornerSkyCoords);
  bool isVisible = false;

  for (int i=0; i < 4; i++)
  {
      cornerScreenCoords[i] = m_projector->toScreen(&cornerSkyCoords[i]);
      isVisible |= m_projector->checkVisibility(&cornerSkyCoords[i]);
  }  

  //if (SKPLANECheckFrustumToPolygon(trfGetFrustum(), pts, 4))
  // Is the right way to do this?

  if (isVisible)
  {
    m_blocks++;

    /*for (int i = 0; i < 4; i++)
    {
      trfProjectPointNoCheck(&pts[i]);
    } */

    QImage *image = HIPSManager::Instance()->getPix(allsky, level, pix, freeImage);

    if (image)      
    {
      m_rendered++;

      #if QT_VERSION >= QT_VERSION_CHECK(5,10,0)
      m_size += image->sizeInBytes();
      #else
      m_size += image->byteCount();
      #endif

      // UV Mapping to apply image unto the destination image
      // 4x4 = 16 points are mapped from the source image unto the destination image.
      // Starting from each grandchild pixel, each pix polygon is mapped accordingly.
      // For example, pixel 357 will have 4 child pixels, each of them will have 4 childs pixels and so
      // on. Each healpix pixel appears roughly as a diamond on the sky map.
      // The corners points for HealPIX moves from NORTH -> EAST -> SOUTH -> WEST
      // Hence first point is 0.25, 0.25 in UV coordinate system.
      // Depending on the selected algorithm, the mapping will either utilize nearest neighbour
      // or bilinear interpolation.
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

      int childPixelID[4];

      // Find all the 4 children of the current pixel
      m_HEALpix->getPixChilds(pix, childPixelID);

      int j = 0;
      for (int id : childPixelID)
      {
        int grandChildPixelID[4];
        // Find the children of this child (i.e. grand child)
        // Then we have 4x4 pixels under the primary pixel
        // The image is interpolated and rendered over these pixels
        // coordinate to minimize any distortions due to the projection
        // system.
        m_HEALpix->getPixChilds(id, grandChildPixelID);

        QPointF fineScreenCoords[4];

        for (int id2 : grandChildPixelID)
        {
          SkyPoint fineSkyPoints[4];
          m_HEALpix->getCornerPoints(level + 2, id2, fineSkyPoints);

          for (int i = 0; i < 4; i++)
              fineScreenCoords[i] = m_projector->toScreen(&fineSkyPoints[i]);
          m_scanRender->renderPolygon(3, fineScreenCoords, pDest, image, uv[j]);
          j++;
        }
      }

      if (freeImage)
      {
        delete image;
      }
    }

    if (Options::hIPSShowGrid())
    {
      QPainter p(pDest);
      p.setRenderHint(QPainter::Antialiasing);
      p.setPen(gridColor);

      p.drawLine(cornerScreenCoords[0].x(), cornerScreenCoords[0].y(), cornerScreenCoords[1].x(), cornerScreenCoords[1].y());
      p.drawLine(cornerScreenCoords[1].x(), cornerScreenCoords[1].y(), cornerScreenCoords[2].x(), cornerScreenCoords[2].y());
      p.drawLine(cornerScreenCoords[2].x(), cornerScreenCoords[2].y(), cornerScreenCoords[3].x(), cornerScreenCoords[3].y());
      p.drawLine(cornerScreenCoords[3].x(), cornerScreenCoords[3].y(), cornerScreenCoords[0].x(), cornerScreenCoords[0].y());
      p.drawText((cornerScreenCoords[0].x() + cornerScreenCoords[1].x() + cornerScreenCoords[2].x() + cornerScreenCoords[3].x()) / 4,
                         (cornerScreenCoords[0].y() + cornerScreenCoords[1].y() + cornerScreenCoords[2].y() + cornerScreenCoords[3].y()) / 4, QString::number(pix) + " / " + QString::number(level));
    }

    return true;
  }

  return false;
}
