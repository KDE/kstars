/*
    SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QColor>
#include <QSGNode>

class QSGFlatColorMaterial;
class QSGGeometry;
class QSGGeometryNode;

/**
 * @class EllipseNode
 * @short QSGTransformNode derived node used to draw ellipses
 *
 * @author Artem Fedoskin
 * @version 1.0
 */
class EllipseNode : public QSGTransformNode
{
  public:
    explicit EllipseNode(const QColor &color = QColor(), int width = 1);

    void setColor(QColor color);
    void setLineWidth(int width);
    /**
     * @short Redraw ellipse with the given width, height and positions (x,y)
     * @param x position by x
     * @param y position by y
     * @param width the width
     * @param height the height
     * @param filled - if true the ellipse will be filled with color
     */
    void updateGeometry(float x, float y, int width, int height, bool filled);

  private:
    QSGGeometryNode *m_geometryNode { nullptr };
    QSGGeometry *m_geometry { nullptr };
    QSGFlatColorMaterial *m_material { nullptr };
    int m_width { -1 };
    int m_height { -1 };
    float m_x { -1 };
    float m_y { -1 };
};
