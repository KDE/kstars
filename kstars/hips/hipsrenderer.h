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

#include <memory>

#include "hipsmanager.h"
#include "healpix.h"
#include "scanrender.h"

class Projector;

class HIPSRenderer : public QObject
{
  Q_OBJECT
public:
  explicit HIPSRenderer();
  //void render(mapView_t *view, CSkPainter *painter, QImage *pDest);
  bool render(uint16_t w, uint16_t h, QImage *hipsImage, const Projector *m_proj);
  void renderRec(bool allsky, int level, int pix, QImage *pDest);
  bool renderPix(bool allsky, int level, int pix, QImage *pDest);

signals:

public slots:

private:  
  int         m_blocks;
  int         m_rendered;
  int         m_size;
  QSet <int>  m_renderedMap;
  HEALPix     m_HEALpix;
  std::unique_ptr<ScanRender> scanRender;
  const Projector   *m_projector;  
  QColor gridColor;
};


