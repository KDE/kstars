/*
    SPDX-FileCopyrightText: 2015-2017 Pavel Mraz

    SPDX-FileCopyrightText: 2017 Jasem Mutlaq

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "healpix.h"
#include "hipsmanager.h"
#include "scanrender.h"

#include <memory>

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
  int m_blocks { 0 };
  int m_rendered { 0 };
  int m_size { 0 };
  QSet<int>  m_renderedMap;
  std::unique_ptr<HEALPix> m_HEALpix;
  std::unique_ptr<ScanRender> m_scanRender;
  const Projector *m_projector;
  QColor gridColor;
};
