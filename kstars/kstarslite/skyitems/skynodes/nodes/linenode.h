/** *************************************************************************
                          linenode.h  -  K Desktop Planetarium
                             -------------------
    begin                : 05/05/2016
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
 * A QSGGeometryNode derived class used for representing stars and planets as stars. Upon
 * construction loads the texture of star cached in parentNode
 *
 *@short QSGOpacityNode derived class that represents stars and planets using cached QSGTexture
 *@author Artem Fedoskin
 *@version 1.0
 */

class LineNode : public SkyOpacityNode  {
public:
    /**
     * @short Constructor
     * @param spType spectral type
     * @param parentNode pointer to the top parent node, which holds texture cache
     * @param size initial size of LineNode
     */
    LineNode(LineList *lineList, SkipList *skipList, QColor color, int width, Qt::PenStyle drawStyle);
    virtual ~LineNode();
    /**
     * @short setSize update size of LineNode with the given parameter
     * @param size new size of LineNode
     */

    void setColor(QColor color);
    void setWidth(int width);
    void setDrawStyle(Qt::PenStyle drawStyle);

    void setStyle(QColor color, int width, Qt::PenStyle drawStyle);

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
};

#endif

