/** *************************************************************************
                          linenode.h  -  K Desktop Planetarium
                             -------------------
    begin                : 05/06/2016
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
#ifndef LINENODE_H_
#define LINENODE_H_
#include <QSGGeometryNode>
#include "linelist.h"
#include <QColor>
#include "../../skyopacitynode.h"

class QSGFlatColorMaterial;
class SkyMapLite;
class SkipList;

/** @class LineNode
 *
 *
 *@short SkyOpacityNode derived class that draws lines from LineList
 *@author Artem Fedoskin
 *@version 1.0
 */

class LineNode : public SkyOpacityNode  {

public:
    /**
     * @short Constructor
     * @param lineList - lines that have to be drawn
     * @param skipList - lines that have to be skipped
     * @param drawStyle - not used currently
     */
    LineNode(LineList *lineList, SkipList *skipList, QColor color, int width, Qt::PenStyle drawStyle);
    virtual ~LineNode();

    void setColor(QColor color);
    void setWidth(int width);
    void setDrawStyle(Qt::PenStyle drawStyle);

    void setStyle(QColor color, int width, Qt::PenStyle drawStyle);

    /**
     * @short Update lines based on the visibility of line segments in m_lineList
     */
    void updateGeometry();

    inline LineList *lineList() { return m_lineList; }
private:
    QSGGeometryNode *m_geometryNode;
    LineList *m_lineList;
    SkipList *m_skipList;

    QSGGeometry *m_geometry;
    QSGFlatColorMaterial *m_material;

    Qt::PenStyle m_drawStyle;
    QColor m_color;

    QSGTransformNode *debug;
};

#endif

