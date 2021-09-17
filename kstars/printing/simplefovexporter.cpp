/*
    SPDX-FileCopyrightText: 2011 Rafał Kułaga <rl.kulaga@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "simplefovexporter.h"

#include "kstars.h"
#include "kstarsdata.h"
#include "skymap.h"
#include "skyqpainter.h"
#include "fov.h"
#include "skymapcomposite.h"
#include "kstars/Options.h"

SimpleFovExporter::SimpleFovExporter()
    : m_KSData(KStarsData::Instance()), m_Map(KStars::Instance()->map()), m_StopClock(false), m_OverrideFovShape(false),
      m_DrawFovSymbol(false), m_PrevClockState(false), m_PrevSlewing(false), m_PrevPoint(nullptr), m_PrevZoom(0)
{
}

void SimpleFovExporter::exportFov(SkyPoint *point, FOV *fov, QPaintDevice *pd)
{
    saveState(true);
    pExportFov(point, fov, pd);
    restoreState(true);
}

void SimpleFovExporter::exportFov(FOV *fov, QPaintDevice *pd)
{
    pExportFov(nullptr, fov, pd);
}

void SimpleFovExporter::exportFov(QPaintDevice *pd)
{
    SkyQPainter painter(m_Map, pd);
    painter.begin();

    painter.drawSkyBackground();

    // translate painter coordinates - it's necessary to extract only the area of interest (FOV)
    int dx = (m_Map->width() - pd->width()) / 2;
    int dy = (m_Map->height() - pd->height()) / 2;
    painter.translate(-dx, -dy);

    m_KSData->skyComposite()->draw(&painter);
    m_Map->getSkyMapDrawAbstract()->drawOverlays(painter, false);
}

void SimpleFovExporter::exportFov(const QList<SkyPoint *> &points, const QList<FOV *> &fovs,
                                  const QList<QPaintDevice *> &pds)
{
    Q_ASSERT(points.size() == fovs.size() && fovs.size() == pds.size());

    saveState(true);

    for (int i = 0; i < points.size(); i++)
    {
        exportFov(points.value(i), fovs.at(i), pds.value(i));
    }

    restoreState(true);
}

void SimpleFovExporter::exportFov(const QList<SkyPoint *> &points, FOV *fov, const QList<QPaintDevice *> &pds)
{
    Q_ASSERT(points.size() == pds.size());

    saveState(true);

    for (int i = 0; i < points.size(); i++)
    {
        exportFov(points.at(i), fov, pds.at(i));
    }

    restoreState(true);
}

void SimpleFovExporter::pExportFov(SkyPoint *point, FOV *fov, QPaintDevice *pd)
{
    if (point)
    {
        // center sky map on selected point
        m_Map->setClickedPoint(point);
        m_Map->slotCenter();
    }

    // this is temporary 'solution' that will be changed during the implementation of printing
    // on large paper sizes (>A4), in which case it'll be desirable to export high-res FOV
    // representations
    if (pd->height() > m_Map->height() || pd->width() > m_Map->width())
    {
        return;
    }

    // calculate zoom factor
    double zoom = 0;
    QRegion region;
    int regionX(0), regionY(0);
    double fovSizeX(0), fovSizeY(0);
    if (fov->sizeX() > fov->sizeY())
    {
        zoom = calculateZoomLevel(pd->width(), fov->sizeX());

        // calculate clipping region size
        fovSizeX = calculatePixelSize(fov->sizeX(), zoom);
        fovSizeY = calculatePixelSize(fov->sizeY(), zoom);
        regionX  = 0;
        regionY  = 0.5 * (pd->height() - fovSizeY);
    }

    else
    {
        zoom = calculateZoomLevel(pd->height(), fov->sizeY());

        // calculate clipping region size
        fovSizeX = calculatePixelSize(fov->sizeX(), zoom);
        fovSizeY = calculatePixelSize(fov->sizeY(), zoom);
        regionX  = 0.5 * (pd->width() - fovSizeX);
        regionY  = 0;
    }

    if (fov->shape() == FOV::SQUARE)
    {
        region = QRegion(regionX, regionY, fovSizeX, fovSizeY, QRegion::Rectangle);
    }

    else
    {
        region = QRegion(regionX, regionY, fovSizeX, fovSizeY, QRegion::Ellipse);
    }

    m_Map->setZoomFactor(zoom);

    SkyQPainter painter(m_Map, pd);
    painter.begin();

    painter.drawSkyBackground();

    if (!m_OverrideFovShape)
    {
        painter.setClipRegion(region);
    }
    // translate painter coordinates - it's necessary to extract only the area of interest (FOV)
    int dx = (m_Map->width() - pd->width()) / 2;
    int dy = (m_Map->height() - pd->height()) / 2;
    painter.translate(-dx, -dy);

    m_KSData->skyComposite()->draw(&painter);
    m_Map->getSkyMapDrawAbstract()->drawOverlays(painter, false);

    // reset painter coordinate transform to paint FOV symbol in the center
    painter.resetTransform();

    if (m_DrawFovSymbol)
    {
        fov->draw(painter, zoom);
    }
}

void SimpleFovExporter::saveState(bool savePos)
{
    // stop simulation if it's not already stopped
    m_PrevClockState = m_KSData->clock()->isActive();
    if (m_StopClock && m_PrevClockState)
    {
        m_KSData->clock()->stop();
    }

    // disable useAnimatedSlewing option
    m_PrevSlewing = Options::useAnimatedSlewing();
    if (m_PrevSlewing)
    {
        Options::setUseAnimatedSlewing(false);
    }

    // save current central point and zoom level
    m_PrevPoint = savePos ? m_Map->focusPoint() : nullptr;
    m_PrevZoom  = Options::zoomFactor();
}

void SimpleFovExporter::restoreState(bool restorePos)
{
    // restore previous useAnimatedSlewing option
    if (m_PrevSlewing)
    {
        Options::setUseAnimatedSlewing(true);
    }

    if (restorePos)
    {
        // restore previous central point
        m_Map->setClickedPoint(m_PrevPoint);
        m_Map->slotCenter();
    }
    // restore previous zoom level
    m_Map->setZoomFactor(m_PrevZoom);

    // restore clock state (if it was stopped)
    if (m_StopClock && m_PrevClockState)
    {
        m_KSData->clock()->start();
    }
}
