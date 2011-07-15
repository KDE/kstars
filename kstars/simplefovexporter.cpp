#include "simplefovexporter.h"

#include "kstarsdata.h"
#include "skymap.h"
#include "skyqpainter.h"
#include "fov.h"
#include "skymapcomposite.h"

#include "kstars/Options.h"

SimpleFovExporter::SimpleFovExporter() :
        m_KSData(KStarsData::Instance()), m_Map(KStars::Instance()->map()),
        m_StopClock(false), m_OverrideFovShape(true), m_DrawFovSymbol(true)
{}

SimpleFovExporter::~SimpleFovExporter()
{}

void SimpleFovExporter::exportFov(SkyPoint *point, FOV *fov, QPaintDevice *pd)
{
    // stop simulation if it's not already stopped
    bool prevClockState = m_KSData->clock()->isActive();
    if(m_StopClock && prevClockState)
    {
        m_KSData->clock()->stop();
    }

    // disable useAnimatedSlewing option
    bool prevSlewing = Options::useAnimatedSlewing();
    if(prevSlewing)
    {
        Options::setUseAnimatedSlewing(false);
    }

    // save current central point and zoom level
    SkyPoint *prevPoint = m_Map->focusPoint();
    double prevZoom = Options::zoomFactor();

    // center sky map on selected point
    m_Map->setClickedPoint(point);
    m_Map->slotCenter();

    // this is temporary 'solution' that will be changed during the implementation of printing
    // on large paper sizes (>A4), in which case it'll be desirable to export high-res FOV
    // representations
    if(pd->height() > m_Map->height() || pd->width() > m_Map->width())
    {
        return;
    }

    // calculate zoom factor
    double zoom = 0;
    if(fov->sizeX() > fov->sizeY())
    {
        zoom = calculateZoomLevel(pd->width(), fov->sizeX());
    }

    else
    {
        zoom = calculateZoomLevel(pd->height(), fov->sizeY());
    }

    m_Map->setZoomFactor(zoom);

    SkyQPainter painter(m_Map, pd);
    painter.begin();

    // calculate clipping region
//    double sizeX = calculatePixelSize(fov->sizeX(), zoom);
//    double sizeY = calculatePixelSize(fov->sizeY(), zoom);
//    QRegion region;
//    if(fov->shape() == FOV::SQUARE)
//    {
//        region = QRegion(0, 0.5 * (pd->height() - sizeY), sizeX, sizeY, QRegion::Rectangle);
//    }
//
//    else
//    {
//        region = QRegion(0, 0.5 * (pd->height() - sizeY), sizeX, sizeY, QRegion::Ellipse);
//    }

    painter.drawSkyBackground();

    // translate painter coordinates - it's necessary to extract only the area of interest (FOV)
    int dx = (m_Map->width() - pd->width()) / 2;
    int dy = (m_Map->height() - pd->height()) / 2;
    painter.translate(-dx, -dy);

    m_KSData->skyComposite()->draw(&painter);
    m_Map->getSkyMapDrawAbstract()->drawOverlays(painter, false);

    painter.resetTransform();

    if(m_DrawFovSymbol)
    {
        fov->draw(painter, zoom);
    }
    // restore previous useAnimatedSlewing option
    if(prevSlewing)
    {
        Options::setUseAnimatedSlewing(true);
    }

    // restore previous central point and zoom level
    m_Map->setClickedPoint(prevPoint);
    m_Map->slotCenter();
    m_Map->setZoomFactor(prevZoom);

    // synchronize clock and restore its state (if it was stopped)
    if(m_StopClock && prevClockState)
    {
        KStars::Instance()->slotSetTimeToNow();
        m_KSData->clock()->start();
    }
}

void SimpleFovExporter::exportFov(const QList<SkyPoint*> &points, const QList<FOV*> &fovs, const QList<QPaintDevice*> &pds)
{
    Q_ASSERT(points.size() == fovs.size() == pds.size());

    for(int i = 0; i < points.size(); i++)
    {
        exportFov(points.value(i), fovs.at(i), pds.value(i));
    }
}

void SimpleFovExporter::exportFov(const QList<SkyPoint*> &points, FOV *fov, const QList<QPaintDevice*> &pds)
{
    Q_ASSERT(points.size() == pds.size());

    for(int i = 0; i < points.size(); i++)
    {
        exportFov(points.at(i), fov, pds.at(i));
    }
}
