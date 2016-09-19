/** *************************************************************************
                          polynode.h  -  K Desktop Planetarium
                             -------------------
    begin                : 28/06/2016
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
#ifndef POLYNODE_H_
#define POLYNODE_H_
#include "../../skyopacitynode.h"

    /** @class PolyNode
     *
     * @short A SkyOpacityNode derived class used for drawing of polygons (both filled and non-filled)
     *
     * @author Artem Fedoskin
     * @version 1.0
     */

class QSGGeometryNode;
class QSGGeometry;
class QSGFlatColorMaterial;

class PolyNode : public SkyOpacityNode  {
public:
    /**
     * @short Constructor. Initialize geometry and material
     */
    PolyNode();

    void setColor(QColor color);

    /**
     * @short Set thickness of border line
     */
    void setLineWidth(int width);

    /**
     * @short Update the geometry of polygon
     * @param polygon - polygon that needs to be drawn
     * @param filled - true if it should be filled
     */
    void updateGeometry(const QPolygonF &polygon, bool filled);
private:
    QSGGeometryNode *m_geometryNode;
    QSGGeometry *m_geometry;
    QSGFlatColorMaterial *m_material;
};

#endif

