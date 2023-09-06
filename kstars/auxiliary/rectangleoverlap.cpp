/*
    SPDX-FileCopyrightText: 2023 Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later

    Based on https://github.com/lennig/rectangleOverlap
    with permission from Matt Lennig
*/

#include "rectangleoverlap.h"
#include "math.h"

namespace
{

// Rotates in place the points in vertices by rotationDegrees around the center point.
void rotate(QVector<QPointF> &vertices, const QPointF &center, double rotationDegrees)
{
    constexpr double convertDegreesToRadians =  M_PI / 180.0;
    const double cosAngle = cos(rotationDegrees * convertDegreesToRadians);
    const double sinAngle = sin(rotationDegrees * convertDegreesToRadians);

    for (int i = 0; i < vertices.size(); i++)
    {
        // Translate the vertices so that center is at 0,0.
        const double x = vertices[i].x() - center.x();
        const double y = vertices[i].y() - center.y();
        // Rotate the points around the origin, then translate them back.
        vertices[i] = QPointF(center.x() + x * cosAngle - y * sinAngle,
                              center.y() + x * sinAngle + y * cosAngle);
    }
}

// Compute the vertices (corner points) of a possibly rotated rectangle
// with the given center point, width and height.
// Points are returned in the input QVector in counter-clockwise order.
void computeVertices(
    QVector<QPointF> &vertices, const QPointF &center, int width,
    int height, double rotationDegrees)
{
    // Initialize the rectangle unrotated, then rotate if necessary.
    const double dw = width / 2.0;
    const double dh = height / 2.0;
    vertices.push_back(QPoint(center.x() - dw, center.y() - dh));
    vertices.push_back(QPoint(center.x() + dw, center.y() - dh));
    vertices.push_back(QPoint(center.x() + dw, center.y() + dh));
    vertices.push_back(QPoint(center.x() - dw, center.y() + dh));
    if (rotationDegrees != 0.0)
        rotate(vertices, center, rotationDegrees);
}

// Returns the slope of the line between the two points
// Slope returned is guaranteed to be finite--it is 0 for horizontal
// or vertical lines, which works for this code.
double finiteSlope(const QPointF &p1, const QPointF &p2)
{
    const double dx = p2.x() - p1.x();
    if (dx == 0) return 0.0;
    return (p2.y() - p1.y()) / dx;
}

// Returns the axes of the rectangle.
// Axes are lines coming from the origin that are parallel to the sides of the rectangle.
// As this is a rectangle, getting the slope between its first two points
// assuming they are given clockwise or counter-clockwise, will tell us
// all we need to know about the axes.  That is, the rest of the axes are either
// parallel or perpendicular. Similarly, as finiteSlope() returns 0 for horizontal
// or vertical lines, in either case it will give us the axes of the rectangle.
void addAxes(QVector<QPointF> &axes, const QVector<QPointF> &vertices)
{
    const double s = finiteSlope(vertices[0], vertices[1]);
    if (s != 0)
    {
        // Projection axes are not parallel to main axes
        axes.push_back(QPointF(1, s));
        axes.push_back(QPointF(1, -1 / s));
    }
    else
    {
        // Projection axes are parallel to main axes
        axes.push_back(QPointF(1, 0));
        axes.push_back(QPointF(0, 1));
    }
}
}  // namespace

RectangleOverlap::RectangleOverlap(const QPointF &center, int width, int height, double rotationDegrees)
{
    computeVertices(m_Vertices, center, width, height, rotationDegrees);
}

bool RectangleOverlap::intersects(const QPointF &center, int width, int height, double rotationDegrees) const
{
    // Compute the vertices of the test rectangle
    QVector<QPointF> testVertices;
    computeVertices(testVertices, center, width, height, rotationDegrees);

    QVector<QPointF> axes;
    addAxes(axes, m_Vertices);
    addAxes(axes, testVertices);

    // According to the Separating Axis Theorum, two rectangles do not overlap if none of their axes
    // separates the projections of the points from the two rectangles. To phrase that differently,
    // if the projections of one rectangle's vertices onto an axis overlaps with the projections
    // of the other rectangle's vertices onto that axis, then that axis does not rule out an overlap.
    // If none of the axes rule out an overlap, then the rectangles overlap.

    for (const auto &axis : axes)
    {
        // Compute min and max projections of the reference rectangle's vertices of onto an axis.
        double minRefProjection = std::numeric_limits<double>::max();
        double maxRefProjection = std::numeric_limits<double>::lowest();
        for (const auto &vertex : m_Vertices)
        {
            // Compute the dot-product projection.
            const double projection = vertex.x() * axis.x() + vertex.y() * axis.y();
            if (projection > maxRefProjection) maxRefProjection = projection;
            if (projection < minRefProjection) minRefProjection = projection;
        };

        // Compute min and max projections of the test rectangle's vertices of onto an axis.
        double minTestProjection =  std::numeric_limits<double>::max();
        double maxTextProjection = std::numeric_limits<double>::lowest();
        for (const auto &vertex : testVertices)
        {
            const double projection = vertex.x() * axis.x() + vertex.y() * axis.y();
            if (projection > maxTextProjection) maxTextProjection = projection;
            if (projection < minTestProjection) minTestProjection = projection;
        }

        bool separated = minRefProjection > maxTextProjection || minTestProjection > maxRefProjection;
        if (separated)
            return false;
    }
    // None of the axes separate the rectangles' vertices, so they are overlapped.
    return true;
}
