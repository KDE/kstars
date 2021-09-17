/*
    SPDX-FileCopyrightText: 2010 Henry de Valence <hdevalence@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "skyqpainter.h"

#include <QPointer>

#include "kstarsdata.h"
#include "Options.h"
#include "skymap.h"
#include "projections/projector.h"
#include "skycomponents/flagcomponent.h"
#include "skycomponents/linelist.h"
#include "skycomponents/linelistlabel.h"
#include "skycomponents/satellitescomponent.h"
#include "skycomponents/skiphashlist.h"
#include "skycomponents/skymapcomposite.h"
#include "skycomponents/solarsystemcomposite.h"
#include "skycomponents/earthshadowcomponent.h"
#include "skyobjects/constellationsart.h"
#include "skyobjects/catalogobject.h"
#include "skyobjects/ksasteroid.h"
#include "skyobjects/kscomet.h"
#include "skyobjects/kssun.h"
#include "skyobjects/satellite.h"
#include "skyobjects/supernova.h"
#include "skyobjects/ksearthshadow.h"
#include "hips/hipsrenderer.h"
#include "terrain/terrainrenderer.h"

namespace
{
// Convert spectral class to numerical index.
// If spectral class is invalid return index for white star (A class)
int harvardToIndex(char c)
{
    switch (c)
    {
        case 'o':
        case 'O':
            return 0;
        case 'b':
        case 'B':
            return 1;
        case 'a':
        case 'A':
            return 2;
        case 'f':
        case 'F':
            return 3;
        case 'g':
        case 'G':
            return 4;
        case 'k':
        case 'K':
            return 5;
        case 'm':
        case 'M':
            return 6;
        // For unknown spectral class assume A class (white star)
        default:
            return 2;
    }
}

// Total number of sizes of stars.
const int nStarSizes = 15;
// Total number of spectral classes
// N.B. Must be in sync with harvardToIndex
const int nSPclasses = 7;

// Cache for star images.
//
// These pixmaps are never deallocated. Not really good...
QPixmap *imageCache[nSPclasses][nStarSizes] = { { nullptr } };

std::unique_ptr<QPixmap> visibleSatPixmap, invisibleSatPixmap;
} // namespace

int SkyQPainter::starColorMode           = 0;
QColor SkyQPainter::m_starColor          = QColor();
QMap<char, QColor> SkyQPainter::ColorMap = QMap<char, QColor>();

void SkyQPainter::releaseImageCache()
{
    for (char &color : ColorMap.keys())
    {
        QPixmap **pmap = imageCache[harvardToIndex(color)];

        for (int size = 1; size < nStarSizes; size++)
        {
            if (pmap[size])
                delete pmap[size];

            pmap[size] = nullptr;
        }
    }
}

SkyQPainter::SkyQPainter(QPaintDevice *pd) : SkyPainter(), QPainter()
{
    Q_ASSERT(pd);
    m_pd         = pd;
    m_size       = QSize(pd->width(), pd->height());
    m_hipsRender = new HIPSRenderer();
}

SkyQPainter::SkyQPainter(QPaintDevice *pd, const QSize &size) : SkyPainter(), QPainter()
{
    Q_ASSERT(pd);
    m_pd         = pd;
    m_size       = size;
    m_hipsRender = new HIPSRenderer();
}

SkyQPainter::SkyQPainter(QWidget *widget, QPaintDevice *pd) : SkyPainter(), QPainter()
{
    Q_ASSERT(widget);
    // Set paint device pointer to pd or to the widget if pd = 0
    m_pd         = (pd ? pd : widget);
    m_size       = widget->size();
    m_hipsRender = new HIPSRenderer();
}

SkyQPainter::~SkyQPainter()
{
    delete (m_hipsRender);
}

void SkyQPainter::begin()
{
    QPainter::begin(m_pd);
    bool aa = !m_sm->isSlewing() && Options::useAntialias();
    setRenderHint(QPainter::Antialiasing, aa);
    setRenderHint(QPainter::HighQualityAntialiasing, aa);
    m_proj = m_sm->projector();
}

void SkyQPainter::end()
{
    QPainter::end();
}

void SkyQPainter::drawSkyBackground()
{
    //FIXME use projector
    fillRect(0, 0, m_size.width(), m_size.height(),
             KStarsData::Instance()->colorScheme()->colorNamed("SkyColor"));
}

void SkyQPainter::setPen(const QPen &pen)
{
    QPainter::setPen(pen);
}

void SkyQPainter::setBrush(const QBrush &brush)
{
    QPainter::setBrush(brush);
}

void SkyQPainter::initStarImages()
{
    const int starColorIntensity = Options::starColorIntensity();

    ColorMap.clear();
    switch (Options::starColorMode())
    {
        case 1: // Red stars.
            m_starColor = Qt::red;
            break;
        case 2: // Black stars.
            m_starColor = Qt::black;
            break;
        case 3: // White stars
            m_starColor = Qt::white;
            break;
        case 0:  // Real color
        default: // And use real color for everything else
            m_starColor = QColor();
            ColorMap.insert('O', QColor::fromRgb(0, 0, 255));
            ColorMap.insert('B', QColor::fromRgb(0, 200, 255));
            ColorMap.insert('A', QColor::fromRgb(0, 255, 255));
            ColorMap.insert('F', QColor::fromRgb(200, 255, 100));
            ColorMap.insert('G', QColor::fromRgb(255, 255, 0));
            ColorMap.insert('K', QColor::fromRgb(255, 100, 0));
            ColorMap.insert('M', QColor::fromRgb(255, 0, 0));
            break;
    }
    if (ColorMap.isEmpty())
    {
        ColorMap.insert('O', m_starColor);
        ColorMap.insert('B', m_starColor);
        ColorMap.insert('A', m_starColor);
        ColorMap.insert('F', m_starColor);
        ColorMap.insert('G', m_starColor);
        ColorMap.insert('K', m_starColor);
        ColorMap.insert('M', m_starColor);
    }

    for (char &color : ColorMap.keys())
    {
        QPixmap BigImage(15, 15);
        BigImage.fill(Qt::transparent);

        QPainter p;
        p.begin(&BigImage);

        if (Options::starColorMode() == 0)
        {
            qreal h, s, v, a;
            p.setRenderHint(QPainter::Antialiasing, false);
            QColor starColor = ColorMap[color];
            starColor.getHsvF(&h, &s, &v, &a);
            for (int i = 0; i < 8; i++)
            {
                for (int j = 0; j < 8; j++)
                {
                    qreal x    = i - 7;
                    qreal y    = j - 7;
                    qreal dist = sqrt(x * x + y * y) / 7.0;
                    starColor.setHsvF(
                        h,
                        qMin(qreal(1),
                             dist < (10 - starColorIntensity) / 10.0 ? 0 : dist),
                        v,
                        qMax(qreal(0),
                             dist < (10 - starColorIntensity) / 20.0 ? 1 : 1 - dist));
                    p.setPen(starColor);
                    p.drawPoint(i, j);
                    p.drawPoint(14 - i, j);
                    p.drawPoint(i, 14 - j);
                    p.drawPoint(14 - i, 14 - j);
                }
            }
        }
        else
        {
            p.setRenderHint(QPainter::Antialiasing, true);
            p.setPen(QPen(ColorMap[color], 2.0));
            p.setBrush(p.pen().color());
            p.drawEllipse(QRectF(2, 2, 10, 10));
        }
        p.end();

        // Cache array slice
        QPixmap **pmap = imageCache[harvardToIndex(color)];

        for (int size = 1; size < nStarSizes; size++)
        {
            if (!pmap[size])
                pmap[size] = new QPixmap();
            *pmap[size] = BigImage.scaled(size, size, Qt::KeepAspectRatio,
                                          Qt::SmoothTransformation);
        }
    }
    starColorMode = Options::starColorMode();

    if (!visibleSatPixmap.get())
        visibleSatPixmap.reset(new QPixmap(":/icons/kstars_satellites_visible.svg"));
    if (!invisibleSatPixmap.get())
        invisibleSatPixmap.reset(new QPixmap(":/icons/kstars_satellites_invisible.svg"));
}

void SkyQPainter::drawSkyLine(SkyPoint *a, SkyPoint *b)
{
    bool aVisible, bVisible;
    QPointF aScreen = m_proj->toScreen(a, true, &aVisible);
    QPointF bScreen = m_proj->toScreen(b, true, &bVisible);

    drawLine(aScreen, bScreen);

    //THREE CASES:
    //    if (aVisible && bVisible)
    //    {
    //        //Both a,b visible, so paint the line normally:
    //        drawLine(aScreen, bScreen);
    //    }
    //    else if (aVisible)
    //    {
    //        //a is visible but b isn't:
    //        drawLine(aScreen, m_proj->clipLine(a, b));
    //    }
    //    else if (bVisible)
    //    {
    //        //b is visible but a isn't:
    //        drawLine(bScreen, m_proj->clipLine(b, a));
    //    } //FIXME: what if both are offscreen but the line isn't?
}

void SkyQPainter::drawSkyPolyline(LineList *list, SkipHashList *skipList,
                                  LineListLabel *label)
{
    SkyList *points = list->points();
    bool isVisible, isVisibleLast;

    if (points->size() == 0)
        return;
    QPointF oLast = m_proj->toScreen(points->first().get(), true, &isVisibleLast);
    // & with the result of checkVisibility to clip away things below horizon
    isVisibleLast &= m_proj->checkVisibility(points->first().get());
    QPointF oThis, oThis2;

    for (int j = 1; j < points->size(); j++)
    {
        SkyPoint *pThis = points->at(j).get();

        oThis2 = oThis = m_proj->toScreen(pThis, true, &isVisible);
        // & with the result of checkVisibility to clip away things below horizon
        isVisible &= m_proj->checkVisibility(pThis);
        bool doSkip = false;
        if (skipList)
        {
            doSkip = skipList->skip(j);
        }

        bool pointsVisible = false;
        //Temporary solution to avoid random lines in Gnomonic projection and draw lines up to horizon
        if (SkyMap::Instance()->projector()->type() == Projector::Gnomonic)
        {
            if (isVisible && isVisibleLast)
                pointsVisible = true;
        }
        else
        {
            if (isVisible || isVisibleLast)
                pointsVisible = true;
        }

        if (!doSkip)
        {
            if (pointsVisible)
            {
                drawLine(oLast, oThis);
                if (label)
                    label->updateLabelCandidates(oThis.x(), oThis.y(), list, j);
            }
        }

        oLast         = oThis2;
        isVisibleLast = isVisible;
    }
}

void SkyQPainter::drawSkyPolygon(LineList *list, bool forceClip)
{
    bool isVisible  = false, isVisibleLast;
    SkyList *points = list->points();
    QPolygonF polygon;

    if (forceClip == false)
    {
        for (const auto &point : *points)
        {
            polygon << m_proj->toScreen(point.get(), false, &isVisibleLast);
            isVisible |= isVisibleLast;
        }

        // If 1+ points are visible, draw it
        if (polygon.size() && isVisible)
            drawPolygon(polygon);

        return;
    }

    SkyPoint *pLast = points->last().get();
    QPointF oLast   = m_proj->toScreen(pLast, true, &isVisibleLast);
    // & with the result of checkVisibility to clip away things below horizon
    isVisibleLast &= m_proj->checkVisibility(pLast);

    for (const auto &point : *points)
    {
        SkyPoint *pThis = point.get();
        QPointF oThis   = m_proj->toScreen(pThis, true, &isVisible);
        // & with the result of checkVisibility to clip away things below horizon
        isVisible &= m_proj->checkVisibility(pThis);

        if (isVisible && isVisibleLast)
        {
            polygon << oThis;
        }
        else if (isVisibleLast)
        {
            QPointF oMid = m_proj->clipLine(pLast, pThis);
            polygon << oMid;
        }
        else if (isVisible)
        {
            QPointF oMid = m_proj->clipLine(pThis, pLast);
            polygon << oMid;
            polygon << oThis;
        }

        pLast         = pThis;
        oLast         = oThis;
        isVisibleLast = isVisible;
    }

    if (polygon.size())
        drawPolygon(polygon);
}

bool SkyQPainter::drawPlanet(KSPlanetBase *planet)
{
    if (!m_proj->checkVisibility(planet))
        return false;

    bool visible = false;
    QPointF pos  = m_proj->toScreen(planet, true, &visible);
    if (!visible || !m_proj->onScreen(pos))
        return false;

    float fakeStarSize = (10.0 + log10(Options::zoomFactor()) - log10(MINZOOM)) *
                         (10 - planet->mag()) / 10;
    if (fakeStarSize > 15.0)
        fakeStarSize = 15.0;

    double size = planet->angSize() * dms::PI * Options::zoomFactor() / 10800.0;
    if (size < fakeStarSize && planet->name() != i18n("Sun") &&
        planet->name() != i18n("Moon"))
    {
        // Draw them as bright stars of appropriate color instead of images
        char spType;
        //FIXME: do these need i18n?
        if (planet->name() == i18n("Mars"))
        {
            spType = 'K';
        }
        else if (planet->name() == i18n("Jupiter") || planet->name() == i18n("Mercury") ||
                 planet->name() == i18n("Saturn"))
        {
            spType = 'F';
        }
        else
        {
            spType = 'B';
        }
        drawPointSource(pos, fakeStarSize, spType);
    }
    else
    {
        float sizemin = 1.0;
        if (planet->name() == i18n("Sun") || planet->name() == i18n("Moon"))
            sizemin = 8.0;

        if (size < sizemin)
            size = sizemin;

        if (Options::showPlanetImages() && !planet->image().isNull())
        {
            //Because Saturn has rings, we inflate its image size by a factor 2.5
            if (planet->name() == "Saturn")
                size = int(2.5 * size);
            // Scale size exponentially so it is visible at large zooms
            else if (planet->name() == "Pluto")
                size = int(size * exp(1.5 * size));

            save();
            translate(pos);
            rotate(m_proj->findPA(planet, pos.x(), pos.y()));
            drawImage(QRectF(-0.5 * size, -0.5 * size, size, size), planet->image());
            restore();
        }
        else //Otherwise, draw a simple circle.
        {
            drawEllipse(pos, size * .5, size * .5);
        }
    }
    return true;
}

bool SkyQPainter::drawEarthShadow(KSEarthShadow *shadow)
{
    if (!m_proj->checkVisibility(shadow))
        return false;

    bool visible = false;
    QPointF pos  = m_proj->toScreen(shadow, true, &visible);

    if (!visible)
        return false;

    double umbra_size =
        shadow->getUmbraAngSize() * dms::PI * Options::zoomFactor() / 10800.0;
    double penumbra_size =
        shadow->getPenumbraAngSize() * dms::PI * Options::zoomFactor() / 10800.0;

    save();
    setBrush(QBrush(QColor(255, 96, 38, 128)));
    drawEllipse(pos, umbra_size, umbra_size);
    setBrush(QBrush(QColor(255, 96, 38, 90)));
    drawEllipse(pos, penumbra_size, penumbra_size);
    restore();

    return true;
}

bool SkyQPainter::drawComet(KSComet *com)
{
    if (!m_proj->checkVisibility(com))
        return false;

    double size =
        com->angSize() * dms::PI * Options::zoomFactor() / 10800.0 / 2; // Radius
    if (size < 1)
        size = 1;

    bool visible = false;
    QPointF pos  = m_proj->toScreen(com, true, &visible);

    // Draw the coma. FIXME: Another Check??
    if (visible && m_proj->onScreen(pos))
    {
        // Draw the comet.
        drawEllipse(pos, size, size);

        double comaLength =
            (com->getComaAngSize().arcmin() * dms::PI * Options::zoomFactor() / 10800.0);

        // If coma is visible and long enough.
        if (Options::showCometComas() && comaLength > size)
        {
            KSSun *sun =
                KStarsData::Instance()->skyComposite()->solarSystemComposite()->sun();

            // Find the angle to the sun.
            double comaAngle = m_proj->findPA(sun, pos.x(), pos.y());

            const QVector<QPoint> coma = { QPoint(pos.x() - size, pos.y()),
                                           QPoint(pos.x() + size, pos.y()),
                                           QPoint(pos.x(), pos.y() + comaLength) };

            QPolygon comaPoly(coma);

            comaPoly =
                QTransform()
                    .translate(pos.x(), pos.y())
                    .rotate(
                        comaAngle) // Already + 180 Deg, because rotated from south, not north.
                    .translate(-pos.x(), -pos.y())
                    .map(comaPoly);

            save();

            // Nice fade for the Coma.
            QLinearGradient linearGrad(pos, comaPoly.point(2));
            linearGrad.setColorAt(0, QColor("white"));
            linearGrad.setColorAt(size / comaLength, QColor("white"));
            linearGrad.setColorAt(0.9, QColor("transparent"));
            setBrush(linearGrad);

            // Render Coma.
            drawConvexPolygon(comaPoly);
            restore();
        }

        return true;
    }
    else
    {
        return false;
    }
}

bool SkyQPainter::drawPointSource(const SkyPoint *loc, float mag, char sp)
{
    //Check if it's even visible before doing anything
    if (!m_proj->checkVisibility(loc))
        return false;

    bool visible = false;
    QPointF pos  = m_proj->toScreen(loc, true, &visible);
    if (visible &&
        m_proj->onScreen(
            pos)) // FIXME: onScreen here should use canvas size rather than SkyMap size, especially while printing in portrait mode!
    {
        drawPointSource(pos, starWidth(mag), sp);
        return true;
    }
    else
    {
        return false;
    }
}

void SkyQPainter::drawPointSource(const QPointF &pos, float size, char sp)
{
    int isize = qMin(static_cast<int>(size), 14);
    if (!m_vectorStars || starColorMode == 0)
    {
        // Draw stars as bitmaps, either because we were asked to, or because we're painting real colors
        QPixmap *im  = imageCache[harvardToIndex(sp)][isize];
        float offset = 0.5 * im->width();
        drawPixmap(QPointF(pos.x() - offset, pos.y() - offset), *im);
    }
    else
    {
        // Draw stars as vectors, for better printing / SVG export etc.
        if (starColorMode != 4)
        {
            setPen(m_starColor);
            setBrush(m_starColor);
        }
        else
        {
            // Note: This is not efficient, but we use vector stars only when plotting SVG, not when drawing the skymap, so speed is not very important.
            QColor c = ColorMap.value(sp, Qt::white);
            setPen(c);
            setBrush(c);
        }

        // Be consistent with old raster representation
        if (size > 14)
            size = 14;
        if (size >= 2)
            drawEllipse(pos.x() - 0.5 * size, pos.y() - 0.5 * size, int(size), int(size));
        else if (size >= 1)
            drawPoint(pos.x(), pos.y());
    }
}

bool SkyQPainter::drawConstellationArtImage(ConstellationsArt *obj)
{
    double zoom = Options::zoomFactor();

    bool visible = false;
    obj->EquatorialToHorizontal(KStarsData::Instance()->lst(),
                                KStarsData::Instance()->geo()->lat());
    QPointF constellationmidpoint = m_proj->toScreen(obj, true, &visible);

    if (!visible || !m_proj->onScreen(constellationmidpoint))
        return false;

    //qDebug() << "o->pa() " << obj->pa();
    float positionangle =
        m_proj->findPA(obj, constellationmidpoint.x(), constellationmidpoint.y());
    //qDebug() << " final PA " << positionangle;

    float w = obj->getWidth() * 60 * dms::PI * zoom / 10800;
    float h = obj->getHeight() * 60 * dms::PI * zoom / 10800;

    save();

    setRenderHint(QPainter::SmoothPixmapTransform);

    translate(constellationmidpoint);
    rotate(positionangle);
    setOpacity(0.7);
    drawImage(QRectF(-0.5 * w, -0.5 * h, w, h), obj->image());
    setOpacity(1);

    setRenderHint(QPainter::SmoothPixmapTransform, false);
    restore();
    return true;
}

bool SkyQPainter::drawHips()
{
    int w             = viewport().width();
    int h             = viewport().height();
    QImage *hipsImage = new QImage(w, h, QImage::Format_ARGB32_Premultiplied);
    bool rendered     = m_hipsRender->render(w, h, hipsImage, m_proj);
    if (rendered)
        drawImage(viewport(), *hipsImage);

    delete (hipsImage);
    return rendered;
}

bool SkyQPainter::drawTerrain()
{
    int w                     = viewport().width();
    int h                     = viewport().height();
    QImage *terrainImage      = new QImage(w, h, QImage::Format_ARGB32_Premultiplied);
    TerrainRenderer *renderer = TerrainRenderer::Instance();
    bool rendered             = renderer->render(w, h, terrainImage, m_proj);
    if (rendered)
        drawImage(viewport(), *terrainImage);

    delete (terrainImage);
    return rendered;
}

void SkyQPainter::drawCatalogObjectImage(const QPointF &pos, const CatalogObject &obj,
                                         float positionAngle)
{
    const auto &image = obj.image();

    if (!image.first)
        return;

    double zoom = Options::zoomFactor();
    double w    = obj.a() * dms::PI * zoom / 10800.0;
    double h    = obj.e() * w;

    save();
    translate(pos);
    rotate(positionAngle);
    drawImage(QRectF(-0.5 * w, -0.5 * h, w, h), image.second);
    restore();
}

bool SkyQPainter::drawCatalogObject(const CatalogObject &obj)
{
    if (!m_proj->checkVisibility(&obj))
        return false;

    bool visible = false;
    QPointF pos  = m_proj->toScreen(&obj, true, &visible);
    if (!visible || !m_proj->onScreen(pos))
        return false;

    // if size is 0.0 set it to 1.0, this are normally stars (type 0 and 1)
    // if we use size 0.0 the star wouldn't be drawn
    float majorAxis = obj.a();
    if (majorAxis == 0.0)
    {
        majorAxis = 1.0;
    }

    float size = majorAxis * dms::PI * Options::zoomFactor() / 10800.0;

    const auto positionAngle =
        m_proj->findNorthPA(&obj, pos.x(), pos.y()) - obj.pa() + 90;

    // Draw image
    if (Options::showInlineImages() && Options::zoomFactor() > 5. * MINZOOM &&
        !Options::showHIPS())
        drawCatalogObjectImage(pos, obj, positionAngle);

    // Draw Symbol
    drawDeepSkySymbol(pos, obj.type(), size, obj.e(), positionAngle);

    return true;
}

void SkyQPainter::drawDeepSkySymbol(const QPointF &pos, int type, float size, float e,
                                    float positionAngle)
{
    float x    = pos.x();
    float y    = pos.y();
    float zoom = Options::zoomFactor();

    int isize = int(size);

    float dx1 = -0.5 * size;
    float dx2 = 0.5 * size;
    float dy1 = -1.0 * e * size / 2.;
    float dy2 = e * size / 2.;
    float x1  = x + dx1;
    float x2  = x + dx2;
    float y1  = y + dy1;
    float y2  = y + dy2;

    float dxa = -size / 4.;
    float dxb = size / 4.;
    float dya = -1.0 * e * size / 4.;
    float dyb = e * size / 4.;
    float xa  = x + dxa;
    float xb  = x + dxb;
    float ya  = y + dya;
    float yb  = y + dyb;

    QString color;

    float psize;

    QBrush tempBrush;

    std::function<void(float, float, float, float)> lambdaDrawEllipse;
    std::function<void(float, float, float, float)> lambdaDrawLine;
    std::function<void(float, float, float, float)> lambdaDrawCross;

    if (Options::useAntialias())
    {
        lambdaDrawEllipse = [this](float x, float y, float width, float height) {
            drawEllipse(QRectF(x, y, width, height));
        };
        lambdaDrawLine = [this](float x1, float y1, float x2, float y2) {
            drawLine(QLineF(x1, y1, x2, y2));
        };
        lambdaDrawCross = [this](float centerX, float centerY, float sizeX, float sizeY) {
            drawLine(
                QLineF(centerX - sizeX / 2., centerY, centerX + sizeX / 2., centerY));
            drawLine(
                QLineF(centerX, centerY - sizeY / 2., centerX, centerY + sizeY / 2.));
        };
    }
    else
    {
        lambdaDrawEllipse = [this](float x, float y, float width, float height) {
            drawEllipse(QRect(x, y, width, height));
        };
        lambdaDrawLine = [this](float x1, float y1, float x2, float y2) {
            drawLine(QLine(x1, y1, x2, y2));
        };
        lambdaDrawCross = [this](float centerX, float centerY, float sizeX, float sizeY) {
            drawLine(QLine(centerX - sizeX / 2., centerY, centerX + sizeX / 2., centerY));
            drawLine(QLine(centerX, centerY - sizeY / 2., centerX, centerY + sizeY / 2.));
        };
    }

    switch (type)
    {
        case 0:
        case 1: //catalog star
            //Some NGC/IC objects are stars...changed their type to 1 (was double star)
            if (size < 2.)
                size = 2.;
            lambdaDrawEllipse(x - size / 2., y - size / 2., size, size);
            break;
        case 2: //Planet
            break;
        case 3:  //Open cluster; draw circle of points
        case 13: // Asterism
        {
            tempBrush = brush();
            color     = pen().color().name();
            setBrush(pen().color());
            psize = 2.;
            if (size > 50.)
                psize *= 2.;
            if (size > 100.)
                psize *= 2.;
            auto putDot = [psize, &lambdaDrawEllipse](float x, float y) {
                lambdaDrawEllipse(x - psize / 2., y - psize / 2., psize, psize);
            };
            putDot(xa, y1);
            putDot(xb, y1);
            putDot(xa, y2);
            putDot(xb, y2);
            putDot(x1, ya);
            putDot(x1, yb);
            putDot(x2, ya);
            putDot(x2, yb);
            setBrush(tempBrush);
            break;
        }
        case 4: //Globular Cluster
            if (size < 2.)
                size = 2.;
            save();
            translate(x, y);
            color = pen().color().name();
            rotate(positionAngle); //rotate the coordinate system
            lambdaDrawEllipse(dx1, dy1, size, e * size);
            lambdaDrawCross(0, 0, size, e * size);
            restore(); //reset coordinate system
            break;

        case 5:  //Gaseous Nebula
        case 15: // Dark Nebula
            save();
            translate(x, y);
            rotate(positionAngle); //rotate the coordinate system
            color = pen().color().name();
            lambdaDrawLine(dx1, dy1, dx2, dy1);
            lambdaDrawLine(dx2, dy1, dx2, dy2);
            lambdaDrawLine(dx2, dy2, dx1, dy2);
            lambdaDrawLine(dx1, dy2, dx1, dy1);
            restore(); //reset coordinate system
            break;
        case 6: //Planetary Nebula
            if (size < 2.)
                size = 2.;
            save();
            translate(x, y);
            rotate(positionAngle); //rotate the coordinate system
            color = pen().color().name();
            lambdaDrawEllipse(dx1, dy1, size, e * size);
            lambdaDrawLine(0., dy1, 0., dy1 - e * size / 2.);
            lambdaDrawLine(0., dy2, 0., dy2 + e * size / 2.);
            lambdaDrawLine(dx1, 0., dx1 - size / 2., 0.);
            lambdaDrawLine(dx2, 0., dx2 + size / 2., 0.);
            restore(); //reset coordinate system
            break;
        case 7: //Supernova remnant // FIXME: Why is SNR drawn different from a gaseous nebula?
            save();
            translate(x, y);
            rotate(positionAngle); //rotate the coordinate system
            color = pen().color().name();
            lambdaDrawLine(0., dy1, dx2, 0.);
            lambdaDrawLine(dx2, 0., 0., dy2);
            lambdaDrawLine(0., dy2, dx1, 0.);
            lambdaDrawLine(dx1, 0., 0., dy1);
            restore(); //reset coordinate system
            break;
        case 8:  //Galaxy
        case 16: // Quasar
            color = pen().color().name();
            if (size < 1. && zoom > 20 * MINZOOM)
                size = 3.; //force ellipse above zoomFactor 20
            if (size < 1. && zoom > 5 * MINZOOM)
                size = 1.; //force points above zoomFactor 5
            if (size > 2.)
            {
                save();
                translate(x, y);
                rotate(positionAngle); //rotate the coordinate system
                lambdaDrawEllipse(dx1, dy1, size, e * size);
                restore(); //reset coordinate system
            }
            else if (size > 0.)
            {
                drawPoint(QPointF(x, y));
            }
            break;
        case 14: // Galaxy cluster - draw a dashed circle
        {
            tempBrush = brush();
            setBrush(QBrush());
            color = pen().color().name();
            save();
            translate(x, y);
            rotate(positionAngle); //rotate the coordinate system
            QPen newPen = pen();
            newPen.setStyle(Qt::DashLine);
            setPen(newPen);
            lambdaDrawEllipse(dx1, dy1, size, e * size);
            restore();
            setBrush(tempBrush);
            break;
        }
        default
            : // Unknown object or something we don't know how to draw. Just draw an ellipse with a ?-mark
            color = pen().color().name();
            if (size < 1. && zoom > 20 * MINZOOM)
                size = 3.; //force ellipse above zoomFactor 20
            if (size < 1. && zoom > 5 * MINZOOM)
                size = 1.; //force points above zoomFactor 5
            if (size > 2.)
            {
                save();
                QFont f             = font();
                const QString qMark = " ? ";

#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
                double scaleFactor = 0.8 * size / fontMetrics().horizontalAdvance(qMark);
#else
                double scaleFactor = 0.8 * size / fontMetrics().width(qMark);
#endif

                f.setPointSizeF(f.pointSizeF() * scaleFactor);
                setFont(f);
                translate(x, y);
                rotate(positionAngle); //rotate the coordinate system
                lambdaDrawEllipse(dx1, dy1, size, e * size);
                if (Options::useAntialias())
                    drawText(QRectF(dx1, dy1, size, e * size), Qt::AlignCenter, qMark);
                else
                {
                    int idx1 = int(dx1);
                    int idy1 = int(dy1);
                    drawText(QRect(idx1, idy1, isize, int(e * size)), Qt::AlignCenter,
                             qMark);
                }
                restore(); //reset coordinate system (and font?)
            }
            else if (size > 0.)
            {
                if (Options::useAntialias())
                    drawPoint(QPointF(x, y));
                else
                    drawPoint(QPoint(x, y));
            }
    }
}

void SkyQPainter::drawObservingList(const QList<SkyObject *> &obs)
{
    foreach (SkyObject *obj, obs)
    {
        bool visible = false;
        QPointF o    = m_proj->toScreen(obj, true, &visible);
        if (!visible || !m_proj->onScreen(o))
            continue;

        float size = 20.;
        float x1   = o.x() - 0.5 * size;
        float y1   = o.y() - 0.5 * size;
        drawArc(QRectF(x1, y1, size, size), -60 * 16, 120 * 16);
        drawArc(QRectF(x1, y1, size, size), 120 * 16, 120 * 16);
    }
}

void SkyQPainter::drawFlags()
{
    KStarsData *data = KStarsData::Instance();
    std::shared_ptr<SkyPoint> point;
    QImage image;
    bool visible = false;
    QPointF pos;

    for (int i = 0; i < data->skyComposite()->flags()->size(); i++)
    {
        point = data->skyComposite()->flags()->pointList().at(i);
        image = data->skyComposite()->flags()->image(i);

        // Set Horizontal coordinates
        point->EquatorialToHorizontal(data->lst(), data->geo()->lat());

        // Get flag position on screen
        pos = m_proj->toScreen(point.get(), true, &visible);

        // Return if flag is not visible
        if (!visible || !m_proj->onScreen(pos))
            continue;

        // Draw flag image
        drawImage(pos.x() - 0.5 * image.width(), pos.y() - 0.5 * image.height(), image);

        // Draw flag label
        setPen(data->skyComposite()->flags()->labelColor(i));
        setFont(QFont("Helvetica", 10, QFont::Bold));
        drawText(pos.x() + 10, pos.y() - 10, data->skyComposite()->flags()->label(i));
    }
}

void SkyQPainter::drawHorizon(bool filled, SkyPoint *labelPoint, bool *drawLabel)
{
    QVector<Eigen::Vector2f> ground = m_proj->groundPoly(labelPoint, drawLabel);
    if (ground.size())
    {
        QPolygonF groundPoly(ground.size());
        for (int i = 0; i < ground.size(); ++i)
            groundPoly[i] = KSUtils::vecToPoint(ground[i]);
        if (filled)
            drawPolygon(groundPoly);
        else
        {
            groundPoly.append(groundPoly.first());
            drawPolyline(groundPoly);
        }
    }
}

bool SkyQPainter::drawSatellite(Satellite *sat)
{
    if (!m_proj->checkVisibility(sat))
        return false;

    QPointF pos;
    bool visible = false;

    //sat->HorizontalToEquatorial( data->lst(), data->geo()->lat() );

    pos = m_proj->toScreen(sat, true, &visible);

    if (!visible || !m_proj->onScreen(pos))
        return false;

    if (Options::drawSatellitesLikeStars())
    {
        drawPointSource(pos, 3.5, 'B');
    }
    else
    {
        if (sat->isVisible())
            drawPixmap(QPoint(pos.x() - 15, pos.y() - 11), *visibleSatPixmap);
        else
            drawPixmap(QPoint(pos.x() - 15, pos.y() - 11), *invisibleSatPixmap);

        //drawPixmap(pos, *genericSatPixmap);
        /*drawLine( QPoint( pos.x() - 0.5, pos.y() - 0.5 ), QPoint( pos.x() + 0.5, pos.y() - 0.5 ) );
        drawLine( QPoint( pos.x() + 0.5, pos.y() - 0.5 ), QPoint( pos.x() + 0.5, pos.y() + 0.5 ) );
        drawLine( QPoint( pos.x() + 0.5, pos.y() + 0.5 ), QPoint( pos.x() - 0.5, pos.y() + 0.5 ) );
        drawLine( QPoint( pos.x() - 0.5, pos.y() + 0.5 ), QPoint( pos.x() - 0.5, pos.y() - 0.5 ) );*/
    }

    return true;

    //if ( Options::showSatellitesLabels() )
    //data->skyComposite()->satellites()->drawLabel( sat, pos );
}

bool SkyQPainter::drawSupernova(Supernova *sup)
{
    KStarsData *data = KStarsData::Instance();
    if (!m_proj->checkVisibility(sup))
    {
        return false;
    }

    bool visible = false;
    QPointF pos  = m_proj->toScreen(sup, true, &visible);
    //qDebug()<<"sup->ra() = "<<(sup->ra()).toHMSString()<<"sup->dec() = "<<sup->dec().toDMSString();
    //qDebug()<<"pos = "<<pos<<"m_proj->onScreen(pos) = "<<m_proj->onScreen(pos);
    if (!visible || !m_proj->onScreen(pos))
        return false;

    setPen(data->colorScheme()->colorNamed("SupernovaColor"));
    //qDebug()<<"Here";
    drawLine(QPoint(pos.x() - 2.0, pos.y()), QPoint(pos.x() + 2.0, pos.y()));
    drawLine(QPoint(pos.x(), pos.y() - 2.0), QPoint(pos.x(), pos.y() + 2.0));
    return true;
}
