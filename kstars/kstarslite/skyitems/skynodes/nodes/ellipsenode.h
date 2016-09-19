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
#ifndef ELLIPSENODE_H_
#define ELLIPSENODE_H_
#include <QSGNode>
#include <QColor>

/** @class EllipseNode
 *
 * @short QSGTransformNode derrived node used to draw ellipses
 * @author Artem Fedoskin
 * @version 1.0
 */

class QSGGeometryNode;
class QSGGeometry;
class QSGFlatColorMaterial;

class EllipseNode : public QSGTransformNode  {
public:
    EllipseNode(QColor color = QColor(), int width = 1);

    void setColor(QColor color);
    void setLineWidth(int width);
    /**
     * @short Redraw ellipse with the given width, height and positions (x,y)
     * @param filled - if true the ellipse will be filled with color
     */
    void updateGeometry(float x, float y, int width, int height, bool filled);

private:
    QSGGeometryNode *m_geometryNode;
    QSGGeometry *m_geometry;
    QSGFlatColorMaterial *m_material;

    int m_width;
    int m_height;

    float m_x;
    float m_y;
};

#endif

