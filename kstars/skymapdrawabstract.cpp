/*
    SPDX-FileCopyrightText: 2001 Jason Harris <jharris@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// This file implements the class SkyMapDrawAbstract, and is almost
// identical to the older skymapdraw.cpp file, written by Jason
// Harris. Essentially, skymapdraw.cpp was renamed and modified.
// -- asimha (2011)

#include <QPainter>
#include <QPixmap>
#include <QPainterPath>

#include "skymapdrawabstract.h"
#include "skymap.h"
#include "Options.h"
#include "fov.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "ksnumbers.h"
#include "ksutils.h"
#include "skyobjects/skyobject.h"
#include "skyobjects/catalogobject.h"
#include "catalogsdb.h"
#include "skyobjects/starobject.h"
#include "skyobjects/ksplanetbase.h"
#include "simclock.h"
#include "observinglist.h"
#include "skycomponents/constellationboundarylines.h"
#include "skycomponents/skylabeler.h"
#include "skycomponents/skymapcomposite.h"
#include "skyqpainter.h"
#include "projections/projector.h"
#include "projections/lambertprojector.h"

#include <config-kstars.h>

#ifdef HAVE_INDI
#include <basedevice.h>
#include "indi/indilistener.h"
#include "indi/driverinfo.h"
#include "indi/indistd.h"
#include "indi/indimount.h"
#endif

bool SkyMapDrawAbstract::m_DrawLock = false;

SkyMapDrawAbstract::SkyMapDrawAbstract(SkyMap *sm) : m_KStarsData(KStarsData::Instance()), m_SkyMap(sm)
{
    //m_fpstime.start();
    //m_framecount = 0;
}

void SkyMapDrawAbstract::drawOverlays(QPainter &p, bool drawFov)
{
    if (!KStars::Instance())
        return;

    //draw labels
    SkyLabeler::Instance()->draw(p);

    if (drawFov)
    {
        //draw FOV symbol
        foreach (FOV *fov, m_KStarsData->getVisibleFOVs())
        {
            if (fov->lockCelestialPole())
            {
                SkyPoint centerSkyPoint = SkyMap::Instance()->projector()->fromScreen(p.viewport().center(), KStarsData::Instance()->lst(),
                                          KStarsData::Instance()->geo()->lat());
                QPointF screenSkyPoint = p.viewport().center();
                double northRotation = SkyMap::Instance()->projector()->findNorthPA(&centerSkyPoint, screenSkyPoint.x(),
                                       screenSkyPoint.y());
                fov->setCenter(centerSkyPoint);
                fov->setNorthPA(northRotation);
            }
            fov->draw(p, Options::zoomFactor());
        }
    }

    drawSolverFOV(p);

    drawTelescopeSymbols(p);

    drawZoomBox(p);

    if (m_SkyMap->rotationStart.x() > 0 && m_SkyMap->rotationStart.y() > 0)
    {
        drawOrientationArrows(p);
    }

    // FIXME: Maybe we should take care of this differently. Maybe
    // drawOverlays should remain in SkyMap, since it just calls
    // certain drawing functions which are implemented in
    // SkyMapDrawAbstract. Really, it doesn't draw anything on its
    // own.
    if (m_SkyMap->rulerMode)
    {
        m_SkyMap->updateAngleRuler();
        drawAngleRuler(p);
    }
}

void SkyMapDrawAbstract::drawAngleRuler(QPainter &p)
{
    //FIXME use sky painter.
    p.setPen(QPen(m_KStarsData->colorScheme()->colorNamed("AngularRuler"), 3.0, Qt::DotLine));
    p.drawLine(
        m_SkyMap->m_proj->toScreen(m_SkyMap->AngularRuler.point(
                                       0)), // FIXME: More ugliness. m_proj should probably be a single-instance class, or we should have our own instance etc.
        m_SkyMap->m_proj->toScreen(m_SkyMap->AngularRuler.point(
                                       1))); // FIXME: Again, AngularRuler should be something better -- maybe a class in itself. After all it's used for more than one thing after we integrate the StarHop feature.
}

void SkyMapDrawAbstract::drawOrientationArrows(QPainter &p)
{
    auto* data = m_KStarsData;
    const SkyPoint centerSkyPoint = m_SkyMap->m_proj->fromScreen(
                                                                 p.viewport().center(),
                                                                 data->lst(), data->geo()->lat());

    QPointF centerScreenPoint = p.viewport().center();
    double northRotation = m_SkyMap->m_proj->findNorthPA(
                                                         &centerSkyPoint, centerScreenPoint.x(), centerScreenPoint.y());
    double zenithRotation = m_SkyMap->m_proj->findZenithPA(
                                                           &centerSkyPoint, centerScreenPoint.x(), centerScreenPoint.y());

    QColor overlayColor(data->colorScheme()->colorNamed("CompassColor"));
    p.setPen(Qt::NoPen);
    auto drawArrow = [&](double angle, const QString & marker, const float labelRadius, const bool primary)
    {
        constexpr float radius = 150.0f; // In pixels
        const auto fontMetrics = QFontMetricsF(QFont());
        QTransform transform;
        QColor color = overlayColor;
        color.setAlphaF(primary ? 1.0 : 0.75);
        QPen pen(color, 1.0, primary ? Qt::SolidLine : Qt::DotLine);
        QBrush brush(color);

        QPainterPath arrowstem;
        arrowstem.moveTo(0.f, 0.f);
        arrowstem.lineTo(0.f, -radius + radius / 7.5f);
        transform.reset();
        transform.translate(centerScreenPoint.x(), centerScreenPoint.y());
        transform.rotate(angle);
        arrowstem = transform.map(arrowstem);
        p.strokePath(arrowstem, pen);

        QPainterPath arrowhead;
        arrowhead.moveTo(0.f, 0.f);
        arrowhead.lineTo(-radius / 30.f, radius / 7.5f);
        arrowhead.lineTo(radius / 30.f, radius / 7.5f);
        arrowhead.lineTo(0.f, 0.f);
        arrowhead.addText(QPointF(-1.1 * fontMetrics.averageCharWidth() * marker.size(), radius / 7.5f + 1.2f * fontMetrics.ascent()),
                          QFont(), marker);
        transform.translate(0, -radius);
        arrowhead = transform.map(arrowhead);
        p.fillPath(arrowhead, brush);

        if (labelRadius > 0.f)
        {
            QRectF angleMarkerRect(centerScreenPoint.x() - labelRadius, centerScreenPoint.y() - labelRadius,
                                   2.f * labelRadius, 2.f * labelRadius);
            p.setPen(pen);
            if (abs(angle) < 0.01)
            {
                angle = 0.;
            }
            double arcAngle = angle <= 0. ? -angle : 360. - angle;
            p.drawArc(angleMarkerRect, 90 * 16, int(arcAngle * 16.));

            QPainterPath angleLabel;
            QString angleLabelText = QString::number(int(round(arcAngle))) + "°";
            angleLabel.addText(QPointF(-(fontMetrics.averageCharWidth()*angleLabelText.size()) / 2.f, 1.2f * fontMetrics.ascent()),
                               QFont(), angleLabelText);
            transform.reset();
            transform.translate(centerScreenPoint.x(), centerScreenPoint.y());
            transform.rotate(angle);
            transform.translate(0, -labelRadius);
            transform.rotate(90);
            angleLabel = transform.map(angleLabel);
            p.fillPath(angleLabel, brush);
        }

    };
    auto eastRotation = northRotation + (m_SkyMap->m_proj->viewParams().mirror ? 90 : -90);
    drawArrow(northRotation, i18nc("North", "N"), 80.f, !Options::useAltAz());
    drawArrow(eastRotation, i18nc("East", "E"), -1.f, !Options::useAltAz());
    drawArrow(zenithRotation, i18nc("Zenith", "Z"), 40.f, Options::useAltAz());
}

void SkyMapDrawAbstract::drawZoomBox(QPainter &p)
{
    //draw the manual zoom-box, if it exists
    if (m_SkyMap->ZoomRect.isValid())
    {
        p.setPen(QPen(Qt::white, 1.0, Qt::DotLine));
        p.drawRect(m_SkyMap->ZoomRect.x(), m_SkyMap->ZoomRect.y(), m_SkyMap->ZoomRect.width(),
                   m_SkyMap->ZoomRect.height());
    }
}

void SkyMapDrawAbstract::drawObjectLabels(QList<SkyObject *> &labelObjects)
{
    bool checkSlewing =
        (m_SkyMap->slewing || (m_SkyMap->clockSlewing && m_KStarsData->clock()->isActive())) && Options::hideOnSlew();
    if (checkSlewing && Options::hideLabels())
        return;

    SkyLabeler *skyLabeler = SkyLabeler::Instance();
    skyLabeler->resetFont(); // use the zoom dependent font

    skyLabeler->setPen(m_KStarsData->colorScheme()->colorNamed("UserLabelColor"));

    bool drawPlanets   = Options::showSolarSystem() && !(checkSlewing && Options::hidePlanets());
    bool drawComets    = drawPlanets && Options::showComets();
    bool drawAsteroids = drawPlanets && Options::showAsteroids();
    bool drawOther      = Options::showDeepSky() && Options::showOther() && !(checkSlewing && Options::hideOther());
    bool drawStars      = Options::showStars();
    bool hideFaintStars = checkSlewing && Options::hideStars();

    //Attach a label to the centered object
    if (m_SkyMap->focusObject() != nullptr && Options::useAutoLabel())
    {
        QPointF o =
            m_SkyMap->m_proj->toScreen(m_SkyMap->focusObject()); // FIXME: Same thing. m_proj should be accessible here.
        skyLabeler->drawNameLabel(m_SkyMap->focusObject(), o);
    }

    foreach (SkyObject *obj, labelObjects)
    {
        //Only draw an attached label if the object is being drawn to the map
        //reproducing logic from other draw funcs here...not an optimal solution
        if (obj->type() == SkyObject::STAR || obj->type() == SkyObject::CATALOG_STAR ||
                obj->type() == SkyObject::MULT_STAR)
        {
            if (!drawStars)
                continue;
            //            if ( obj->mag() > Options::magLimitDrawStar() ) continue;
            if (hideFaintStars && obj->mag() > Options::magLimitHideStar())
                continue;
        }
        if (obj->type() == SkyObject::PLANET)
        {
            if (!drawPlanets)
                continue;
            if (obj->name() == i18n("Sun") && !Options::showSun())
                continue;
            if (obj->name() == i18n("Mercury") && !Options::showMercury())
                continue;
            if (obj->name() == i18n("Venus") && !Options::showVenus())
                continue;
            if (obj->name() == i18n("Moon") && !Options::showMoon())
                continue;
            if (obj->name() == i18n("Mars") && !Options::showMars())
                continue;
            if (obj->name() == i18n("Jupiter") && !Options::showJupiter())
                continue;
            if (obj->name() == i18n("Saturn") && !Options::showSaturn())
                continue;
            if (obj->name() == i18n("Uranus") && !Options::showUranus())
                continue;
            if (obj->name() == i18n("Neptune") && !Options::showNeptune())
                continue;
            //if ( obj->name() == i18n( "Pluto" ) && ! Options::showPluto() ) continue;
        }
        if ((obj->type() >= SkyObject::OPEN_CLUSTER && obj->type() <= SkyObject::GALAXY) ||
                (obj->type() >= SkyObject::ASTERISM && obj->type() <= SkyObject::QUASAR) ||
                (obj->type() == SkyObject::RADIO_SOURCE))
        {
            if (((CatalogObject *)obj)->getCatalog().id == -1 && !drawOther)
                continue;
        }
        if (obj->type() == SkyObject::COMET && !drawComets)
            continue;
        if (obj->type() == SkyObject::ASTEROID && !drawAsteroids)
            continue;

        if (!m_SkyMap->m_proj->checkVisibility(obj))
            continue; // FIXME: m_proj should be a member of this class.
        QPointF o = m_SkyMap->m_proj->toScreen(obj);
        if (!m_SkyMap->m_proj->onScreen(o))
            continue;

        skyLabeler->drawNameLabel(obj, o);
    }

    skyLabeler->useStdFont(); // use the StdFont for the guides.
}

void SkyMapDrawAbstract::drawSolverFOV(QPainter &psky)
{
    Q_UNUSED(psky)

#ifdef HAVE_INDI

    for (auto oneFOV : KStarsData::Instance()->getTransientFOVs())
    {
        QVariant visible = oneFOV->property("visible");
        if (visible.isNull() || visible.toBool() == false)
            continue;

        if (oneFOV->objectName() == "sensor_fov")
        {
            oneFOV->setColor(KStars::Instance()->data()->colorScheme()->colorNamed("SensorFOVColor").name());
            SkyPoint centerSkyPoint = SkyMap::Instance()->projector()->fromScreen(psky.viewport().center(),
                                      KStarsData::Instance()->lst(), KStarsData::Instance()->geo()->lat());
            QPointF screenSkyPoint = psky.viewport().center();
            double northRotation = SkyMap::Instance()->projector()->findNorthPA(&centerSkyPoint, screenSkyPoint.x(),
                                   screenSkyPoint.y());
            oneFOV->setCenter(centerSkyPoint);
            oneFOV->setNorthPA(northRotation);
            oneFOV->draw(psky, Options::zoomFactor());
        }
        else if (oneFOV->objectName() == "solver_fov")
        {
            bool isVisible = false;
            SkyPoint p = oneFOV->center();
            if (std::isnan(p.ra().Degrees()))
                continue;

            p.EquatorialToHorizontal(KStarsData::Instance()->lst(), KStarsData::Instance()->geo()->lat());
            QPointF point        = SkyMap::Instance()->projector()->toScreen(&p, true, &isVisible);
            double northRotation = SkyMap::Instance()->projector()->findNorthPA(&p, point.x(), point.y());
            oneFOV->setNorthPA(northRotation);
            oneFOV->draw(psky, Options::zoomFactor());
        }
    }
#endif
}

void SkyMapDrawAbstract::drawTelescopeSymbols(QPainter &psky)
{
    Q_UNUSED(psky)

#ifdef HAVE_INDI
    if (!Options::showTargetCrosshair())
        return;

    if (INDIListener::Instance()->size() == 0)
        return;
    SkyPoint indi_sp;

    psky.setPen(QPen(QColor(m_KStarsData->colorScheme()->colorNamed("TargetColor"))));
    psky.setBrush(Qt::NoBrush);
    float pxperdegree = Options::zoomFactor() / 57.3;

    for (auto &oneDevice : INDIListener::devices())
    {
        if (!(oneDevice->getDriverInterface() & INDI::BaseDevice::TELESCOPE_INTERFACE) || oneDevice->isConnected() == false)
            continue;

        auto mount = oneDevice->getMount();
        if (!mount)
            continue;

        auto coordNP = mount->currentCoordinates();

        QPointF P = m_SkyMap->m_proj->toScreen(&coordNP);
        if (Options::useAntialias())
        {
            float s1 = 0.5 * pxperdegree;
            float s2 = pxperdegree;
            float s3 = 2.0 * pxperdegree;

            float x0 = P.x();
            float y0 = P.y();
            float x1 = x0 - 0.5 * s1;
            float y1 = y0 - 0.5 * s1;
            float x2 = x0 - 0.5 * s2;
            float y2 = y0 - 0.5 * s2;
            float x3 = x0 - 0.5 * s3;
            float y3 = y0 - 0.5 * s3;

            //Draw radial lines
            psky.drawLine(QPointF(x1, y0), QPointF(x3, y0));
            psky.drawLine(QPointF(x0 + s2, y0), QPointF(x0 + 0.5 * s1, y0));
            psky.drawLine(QPointF(x0, y1), QPointF(x0, y3));
            psky.drawLine(QPointF(x0, y0 + 0.5 * s1), QPointF(x0, y0 + s2));
            //Draw circles at 0.5 & 1 degrees
            psky.drawEllipse(QRectF(x1, y1, s1, s1));
            psky.drawEllipse(QRectF(x2, y2, s2, s2));

            psky.drawText(QPointF(x0 + s2 + 2., y0), mount->getDeviceName());
        }
        else
        {
            int s1 = int(0.5 * pxperdegree);
            int s2 = int(pxperdegree);
            int s3 = int(2.0 * pxperdegree);

            int x0 = int(P.x());
            int y0 = int(P.y());
            int x1 = x0 - s1 / 2;
            int y1 = y0 - s1 / 2;
            int x2 = x0 - s2 / 2;
            int y2 = y0 - s2 / 2;
            int x3 = x0 - s3 / 2;
            int y3 = y0 - s3 / 2;

            //Draw radial lines
            psky.drawLine(QPoint(x1, y0), QPoint(x3, y0));
            psky.drawLine(QPoint(x0 + s2, y0), QPoint(x0 + s1 / 2, y0));
            psky.drawLine(QPoint(x0, y1), QPoint(x0, y3));
            psky.drawLine(QPoint(x0, y0 + s1 / 2), QPoint(x0, y0 + s2));
            //Draw circles at 0.5 & 1 degrees
            psky.drawEllipse(QRect(x1, y1, s1, s1));
            psky.drawEllipse(QRect(x2, y2, s2, s2));

            psky.drawText(QPoint(x0 + s2 + 2, y0), mount->getDeviceName());
        }
    }
#endif
}

void SkyMapDrawAbstract::exportSkyImage(QPaintDevice *pd, bool scale)
{
    SkyQPainter p(m_SkyMap, pd);
    p.begin();
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);

    exportSkyImage(&p, scale);

    p.end();
}

void SkyMapDrawAbstract::exportSkyImage(SkyQPainter *painter, bool scale)
{
    bool vectorStarState;
    vectorStarState = painter->getVectorStars();
    painter->setVectorStars(
        true); // Since we are exporting an image, we may use vector stars without worrying about time
    painter->setRenderHint(QPainter::Antialiasing, Options::useAntialias());

    if (scale)
    {
        //scale sky image to fit paint device
        qDebug() << Q_FUNC_INFO << "Scaling true while exporting Sky Image";
        double xscale = double(painter->device()->width()) / double(m_SkyMap->width());
        double yscale = double(painter->device()->height()) / double(m_SkyMap->height());
        double scale  = qMin(xscale, yscale);
        qDebug() << Q_FUNC_INFO << "xscale: " << xscale << "yscale: " << yscale << "chosen scale: " << scale;
        painter->scale(scale, scale);
    }

    painter->drawSkyBackground();
    m_KStarsData->skyComposite()->draw(painter);
    drawOverlays(*painter);
    painter->setVectorStars(vectorStarState); // Restore the state of the painter
}

/* JM 2016-05-03: Not needed since we're not using OpenGL for now
 * void SkyMapDrawAbstract::calculateFPS()
{
    if(m_framecount == 25) {
        //float sec = m_fpstime.elapsed()/1000.;
        // qDebug() << Q_FUNC_INFO << "FPS " << m_framecount/sec;
        m_framecount = 0;
        m_fpstime.restart();
    }
    ++m_framecount;
}*/

void SkyMapDrawAbstract::setDrawLock(bool state)
{
    m_DrawLock = state;
}
