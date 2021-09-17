/*
    SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QColor>
#include <QSGGeometryNode>

class QSGGeometry;
class QSGFlatColorMaterial;

/**
 * @class RectNode
 * @short QSGGeometryNode derived class that draws filled and non-filled rectangles
 *
 * @author Artem Fedoskin
 * @version 1.0
 */
class RectNode : public QSGGeometryNode
{
  public:
    explicit RectNode(bool filled = false);

    /**
     * @brief setRect sets rectangle to display
     * @param x - x coordinate of left-top corner
     * @param y - y coordinate of left-top corner
     * @param w - width
     * @param h - height
     */
    void setRect(int x, int y, int w, int h);

    /** setColor sets the color of rectangle */
    void setColor(const QColor &color);

    /**
     * @brief setFilled sets whether the rectangle should be filled or no
     * @param filled true to be filled, false otherwise
     */
    void setFilled(bool filled);

  private:
    QSGGeometry *m_geometry { nullptr };
    QSGFlatColorMaterial *m_material { nullptr };
    bool m_filled { false };
};
