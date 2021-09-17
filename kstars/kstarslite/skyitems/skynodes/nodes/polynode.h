/*
    SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "../../skyopacitynode.h"

class QSGGeometryNode;
class QSGGeometry;
class QSGFlatColorMaterial;

/**
 * @class PolyNode
 *
 * @short A SkyOpacityNode derived class used for drawing of polygons (both filled and non-filled)
 *
 * @author Artem Fedoskin
 * @version 1.0
 */
class PolyNode : public SkyOpacityNode
{
  public:
    /** @short Initialize geometry and material */
    PolyNode();

    void setColor(QColor color);

    /** @short Set thickness of border line */
    void setLineWidth(int width);

    /**
     * @short Update the geometry of polygon
     * @param polygon - polygon that needs to be drawn
     * @param filled - true if it should be filled
     */
    void updateGeometry(const QPolygonF &polygon, bool filled);

  private:
    QSGGeometryNode *m_geometryNode { nullptr };
    QSGGeometry *m_geometry { nullptr };
    QSGFlatColorMaterial *m_material { nullptr };
};
