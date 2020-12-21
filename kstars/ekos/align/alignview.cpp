/*  Ekos Alignment View
 *  Child of AlignView with few additions necessary for Alignment functions

    Copyright (C) 2017 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#include "alignview.h"

#include "ekos_align_debug.h"
#include "kstarsdata.h"
#include "Options.h"
#include "fitsviewer/fitsdata.h"

#include <QPainter>
#include <QtConcurrent>

#define ZOOM_DEFAULT 100.0
#define ZOOM_MIN     10
#define ZOOM_MAX     400

AlignView::AlignView(QWidget *parent, FITSMode mode, FITSScale filter) : FITSView(parent, mode, filter)
{
}

void AlignView::drawOverlay(QPainter *painter, double scale)
{
    Q_UNUSED(scale);
    painter->setOpacity(0.5);
    FITSView::drawOverlay(painter, getScale());
    painter->setOpacity(1);

    if (RACircle.isNull() == false)
        drawCircle(painter);

    if (correctionLine.isNull() == false)
        drawLine(painter);
}

bool AlignView::injectWCS(double orientation, double ra, double dec, double pixscale, bool extras)
{
    bool rc = imageData->injectWCS(orientation, ra, dec, pixscale);
    // If file fails to load, then no WCS data
    if (rc == false)
    {
        qCritical(KSTARS_EKOS_ALIGN) << "Error creating WCS file:" << imageData->getLastError();
        emit wcsToggled(false);
        return false;
    }

    if (wcsWatcher.isRunning() == false && imageData->getWCSState() == FITSData::Idle)
    {
        // Load WCS async
        QFuture<bool> future = QtConcurrent::run(imageData.data(), &FITSData::loadWCS, extras);
        wcsWatcher.setFuture(future);
    }

    return true;
}

void AlignView::setCorrectionParams(QLineF &line, QLineF *altOnlyLine)
{
    if (imageData.isNull())
        return;

    bool RAAxisInside  = imageData->contains(line.p1());
    bool CPPointInside = imageData->contains(line.p2());

    // If points are outside, let's translate the line to be within the frame
    if (RAAxisInside == false || CPPointInside == false)
    {
        QPointF center(imageData->width() / 2, imageData->height() / 2);
        QPointF offset(center - line.p1());
        line.translate(offset);
    }

    correctionLine     = line;
    if (altOnlyLine == nullptr)
        altLine = QLineF();
    else
        altLine = *altOnlyLine;

    celestialPolePoint = line.p1();
    markerCrosshair    = line.p2();

    updateFrame();
}

void AlignView::setCorrectionOffset(QPointF &newOffset)
{
    if (imageData == nullptr)
        return;

    if (newOffset.isNull() == false)
    {
        correctionOffset  = newOffset;
        QPointF offset    = correctionOffset - correctionLine.p1();
        QLineF offsetLine = correctionLine;
        offsetLine.translate(offset);
        markerCrosshair = offsetLine.p2();

        emit newCorrectionVector(offsetLine);
    }
    // Clear points
    else
    {
        correctionOffset = newOffset;
        markerCrosshair  = newOffset;

        emit newCorrectionVector(correctionLine);
    }

    updateFrame();
}

void AlignView::drawLine(QPainter *painter)
{
    painter->setPen(QPen(Qt::magenta, 2));
    painter->setBrush(Qt::NoBrush);

    QLineF zoomedLine = correctionLine;
    QPointF offset;

    if (correctionOffset.isNull() == false)
    {
        offset = correctionOffset - correctionLine.p1();
    }

    zoomedLine.translate(offset);

    const double scale = getScale();
    double x1 = zoomedLine.p1().x() * scale;
    double y1 = zoomedLine.p1().y() * scale;

    double x2 = zoomedLine.p2().x() * scale;
    double y2 = zoomedLine.p2().y() * scale;

    painter->drawLine(x1, y1, x2, y2);

    // If there is an alt line, then draw a separate path, first the altLine,
    // Then from the 2nd point in the altLine to the point in correctionLine
    // that differs from altLine's first point.
    if (correctionOffset.isNull() && !altLine.isNull())
    {
        painter->setPen(QPen(Qt::yellow, 3));
        painter->setBrush(Qt::NoBrush);
        double x1 = altLine.p1().x() * scale;
        double y1 = altLine.p1().y() * scale;
        const double x2 = altLine.p2().x() * scale;
        const double y2 = altLine.p2().y() * scale;
        painter->drawLine(x1, y1, x2, y2);

        painter->setPen(QPen(Qt::green, 3));
        if ((altLine.p1().x() != correctionLine.p2().x()) ||
                (altLine.p1().y() != correctionLine.p2().y()))
        {
            x1 = correctionLine.p2().x() * scale;
            y1 = correctionLine.p2().y() * scale;
        }
        else
        {
            x1 = correctionLine.p1().x() * scale;
            y1 = correctionLine.p1().y() * scale;
        }
        painter->drawLine(x2, y2, x1, y1);
    }

    // In limited memory mode, WCS data is not loaded so no Equatorial Gridlines are drawn
    // so we have to at least draw the NCP/SCP locations
    if (Options::limitedResourcesMode())
    {
        QPen pen;
        pen.setWidth(2);
        pen.setColor(Qt::darkRed);
        painter->setPen(pen);
        double x  = celestialPolePoint.x() * scale;
        double y  = celestialPolePoint.y() * scale;
        double sr = 3 * scale;

        if (KStarsData::Instance()->geo()->lat()->Degrees() > 0)
            painter->drawText(x + sr, y + sr, i18nc("North Celestial Pole", "NCP"));
        else
            painter->drawText(x + sr, y + sr, i18nc("South Celestial Pole", "SCP"));
    }
}

void AlignView::drawCircle(QPainter *painter)
{
    QPen pen(Qt::green);
    pen.setWidth(2);
    pen.setStyle(Qt::DashLine);
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);

    const double scale = getScale();
    QPointF center(RACircle.x() * scale, RACircle.y() * scale);

    // Big Radius
    double r = RACircle.z() * scale;

    // Small radius
    double sr = r / 25.0;

    painter->drawEllipse(center, sr, sr);
    painter->drawEllipse(center, r, r);
    pen.setColor(Qt::darkGreen);
    painter->setPen(pen);
    painter->drawText(center.x() + sr, center.y() + sr, i18n("RA Axis"));
}

void AlignView::setRACircle(const QVector3D &value)
{
    RACircle = value;
    updateFrame();
}

void AlignView::setRefreshEnabled(bool enable)
{
    if (enable)
        setCursorMode(crosshairCursor);
    else
        setCursorMode(selectCursor);
}

void AlignView::processMarkerSelection(int x, int y)
{
    Q_UNUSED(x)
    Q_UNUSED(y)
}
