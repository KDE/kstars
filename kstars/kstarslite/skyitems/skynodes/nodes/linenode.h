/** *************************************************************************
                          LineNode.h  -  K Desktop Planetarium
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

class PlanetItemNode;
class SkyMapLite;
class QSGFlatColorMaterial;

/** @class LineNode
 *
 * A QSGGeometryNode derived class used for representing stars and planets as stars. Upon
 * construction loads the texture of star cached in parentNode
 *
 *@short QSGOpacityNode derived class that represents stars and planets using cached QSGTexture
 *@author Artem Fedoskin
 *@version 1.0
 */

class LineNode : public QSGOpacityNode  {
public:
    /**
     * @short Constructor
     * @param spType spectral type
     * @param parentNode pointer to the top parent node, which holds texture cache
     * @param size initial size of LineNode
     */
    LineNode(LineList *lineList);
    /**
     * @short setSize update size of LineNode with the given parameter
     * @param size new size of LineNode
     */
    void setColor(QColor color);
    void setWidth(int width);
    void updateGeometry();
    inline LineList *lineList() { return m_lineList; }
    void hide();
    void setDrawStyle(Qt::PenStyle drawStyle);

private:
    QSGGeometryNode *m_geometryNode;
    LineList *m_lineList;
    QSGGeometry *m_geometry;
    QSGFlatColorMaterial *m_material;
    Qt::PenStyle m_drawStyle;
};

#endif

