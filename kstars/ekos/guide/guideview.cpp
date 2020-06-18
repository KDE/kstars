/*  Ekos GuideView
 *  Child of FITSView with few additions necessary for Internal Guider

    Copyright (C) 2020 Hy Murveit

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include "guideview.h"

#include <QPainter>

GuideView::GuideView(QWidget *parent, FITSMode mode, FITSScale filter) : FITSView(parent, mode, filter)
{
}

void GuideView::drawOverlay(QPainter *painter, double scale)
{
    FITSView::drawOverlay(painter, getScale());

    for (const auto &neighbor : neighbors)
        drawNeighbor(painter, neighbor);
}

void GuideView::addGuideStarNeighbor(double targetX, double targetY, bool found,
                                     double detectedX, double detectedY, bool isGuideStar)
{
    Neighbor n;
    n.targetX = targetX;
    n.targetY = targetY;
    n.found = found;
    n.detectedX = detectedX;
    n.detectedY = detectedY;
    n.isGuideStar = isGuideStar;
    neighbors.append(n);
}

void GuideView::clearNeighbors()
{
    neighbors.clear();
}

void GuideView::drawNeighbor(QPainter *painter, const Neighbor &neighbor)
{
    double origOpacity = painter->opacity();
    QPen pen(neighbor.found ? Qt::green : Qt::red);
    pen.setWidth(2);
    pen.setStyle(Qt::SolidLine);
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);
    const double scale = getScale();

    if (!neighbor.isGuideStar)
    {
        const QPointF center(neighbor.targetX * scale, neighbor.targetY * scale);
        const double r = 10.0 * scale;
        painter->drawEllipse(center, r, r);

        const QRect &box = getTrackingBox();
        const int x1 = (box.x() + box.width() / 2.0) * scale;
        const int y1 = (box.y() + box.height() / 2.0) * scale;
        painter->setOpacity(0.25);
        painter->drawLine(x1, y1, neighbor.targetX * scale, neighbor.targetY * scale);
    }
    if (neighbor.found)
    {
        QPen pen2(Qt::black);
        pen2.setWidth(2);
        pen2.setStyle(Qt::SolidLine);
        painter->setPen(pen2);
        const double dx = neighbor.detectedX * scale;
        const double dy = neighbor.detectedY * scale;
        const double offset = 1 * scale;
        painter->setOpacity(0.5);
        painter->drawLine(dx - offset, dy, dx + offset, dy);
        painter->drawLine(dx, dy - offset, dx, dy + offset);
    }

    painter->setOpacity(origOpacity);
}
