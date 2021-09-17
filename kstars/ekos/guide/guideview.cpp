/*  Ekos GuideView
    Child of FITSView with few additions necessary for Internal Guider

    SPDX-FileCopyrightText: 2020 Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "guideview.h"

#include <QPainter>
#include <math.h>

GuideView::GuideView(QWidget *parent, FITSMode mode, FITSScale filter) : FITSView(parent, mode, filter)
{
}

void GuideView::drawOverlay(QPainter *painter, double scale)
{
    Q_UNUSED(scale);

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

// We draw a circle around each "neighbor" star and draw a line from the guide star to the neighbor
// Which starts at the reticle around the guide star and ends at the circle around the neighbor.
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
        const double dx = neighbor.targetX * scale - x1;
        const double dy = neighbor.targetY * scale - y1;

        const double lineLength = std::hypotf(fabs(dx), fabs(dy));

        // f1 indicates the place along line between the guide star and the neighbor star
        // where the line should start because it intersects the reticle box around the guide star.
        double f1;
        if (std::fabs(dx) > std::fabs(dy))
        {
            const double rBox = scale * box.width() / 2.0;
            f1 = std::hypot(rBox, std::fabs(dy) * rBox / std::fabs(dx)) / lineLength;
        }
        else
        {
            const double rBox = scale * box.height() / 2.0;
            f1 = std::hypotf(rBox, std::fabs(dx) * rBox / std::fabs(dy)) / lineLength;
        }
        // f2 indicates the place along line between the guide star and the neighbor star
        // where the line should stop because it intersects the circle around the neighbor.
        const double f2 = 1.0 - (r / lineLength);

        if (f1 < 1 && lineLength > r)
            painter->drawLine(x1 + dx * f1, y1 + dy * f1, x1 + dx * f2, y1 + dy * f2);
    }
    painter->setOpacity(origOpacity);
}
