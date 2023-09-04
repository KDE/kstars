/*
    SPDX-FileCopyrightText: 2023 Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later

    Based on https://github.com/lennig/rectangleOverlap
    with permission from Matt Lennig
*/

#pragma once

#include <QVector>
#include <QPoint>

/**
 * @class RectangleOverlap
 *
 * This class checks if two rectangles overlap. It is more general that QRect::intersects
 * in that the rectangles are not necessarily aligned to the axes (that is they can be
 * specified as rotated around their center points).
 *
 * @short Checks if two potentially rotated rectangles intersect.
 * @author Hy Murveit
 */

class RectangleOverlap
{
public:
    /** Constructor specifying reference rectangle
         * @param center the center of the rectangle
         * @param width its width
         * @param height its height
         * @param rotationDegrees amount in degrees the rectangle is rotated counter-clockwise
         */
    RectangleOverlap(const QPointF &center, int width, int height, double rotationDegrees = 0.0);

    /**
     * @brief Check if the input rectangle overlaps the reference rectangle
     *
     * @param center the center of the input rectangle
     * @param width its width
     * @param height its height
     * @param rotationDegrees amount in degrees the rectangle is rotated counter-clockwise
     * @return true if the rectangles overlap
     **/
    bool intersects(const QPointF &center, int width, int height, double rotationDegrees = 0.0) const;

private:
    // The coordinates of the reference triangle's vertices, counter-clockwise.
    QVector<QPointF> m_Vertices;
};


