/** *************************************************************************
                          polynode.h  -  K Desktop Planetarium
                             -------------------
    begin                : 22/06/2016
    copyright            : (C) 2016 by Artem Fedoskin
    email                : afedoskin3@gmail.com
 ***************************************************************************/
/** *************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

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
