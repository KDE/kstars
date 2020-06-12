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

void GuideView::addGuideStarNeighbor(double x, double y, bool found)
{
    Neighbor n;
    n.x = x;
    n.y = y;
    n.found = found;
    neighbors.append(n);
}

void GuideView::clearNeighbors()
{
    neighbors.clear();
}

void GuideView::drawNeighbor(QPainter *painter, const Neighbor &neighbor)
{
    QPen pen(neighbor.found ? Qt::green : Qt::red);
    pen.setWidth(2);
    pen.setStyle(Qt::SolidLine);
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);

    const double scale = getScale();
    const QPointF center(neighbor.x * scale, neighbor.y * scale);
    const double r = 10.0 * scale;
    painter->drawEllipse(center, r, r);

    const QRect &box = getTrackingBox();
    const int x1 = (box.x() + box.width() / 2.0) * scale;
    const int y1 = (box.y() + box.height() / 2.0) * scale;
    double origOpacity = painter->opacity();
    painter->setOpacity(0.25);
    painter->drawLine(x1, y1, neighbor.x * scale, neighbor.y * scale);
    painter->setOpacity(origOpacity);
}
