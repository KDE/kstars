/** *************************************************************************
                          rectnode.h  -  K Desktop Planetarium
                             -------------------
    begin                : 20/08/2016
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
#ifndef RECTNODE_H_
#define RECTNODE_H_

/** @class RectNode
 *
 * A QSGGeometryNode derived node that encapsulates everything needed for drawing both filled and
 * non-filled rectangles
 *
 *@short QSGGeometryNode derived class that draws filled and non-filled rectangles
 *@author Artem Fedoskin
 *@version 1.0
 */
#include <QColor>
#include <QSGGeometryNode>

class QSGGeometryNode;
class QSGGeometry;
class QSGFlatColorMaterial;

class RectNode : public QSGGeometryNode  {
public:
    RectNode(bool filled = false, QColor color = "#FFFFFF");
    /**
     * @brief setRect sets rectangle to display
     * @param x - x coordinate of left-top corner
     * @param y - y coordinate of left-top corner
     * @param w - width
     * @param h - height
     */
    void setRect(int x, int y, int w, int h);
    /**
     * @brief setColor sets the color of rectangle
     */
    void setColor(QColor color);
    /**
     * @brief setFilled sets whether the rectangle should be filled or no
     * @param filled true to be filled, false otherwise
     */
    void setFilled(bool filled);

private:
    QSGGeometryNode *m_geometryNode;
    QSGGeometry *m_geometry;
    QSGFlatColorMaterial *m_material;
    bool m_filled;
};

#endif

