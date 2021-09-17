/*
    SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "linelist.h"
#include "../../skyopacitynode.h"

#include <QColor>
#include <QSGGeometryNode>

class QSGFlatColorMaterial;

class SkyMapLite;
class SkipHashList;

/**
 * @class LineNode
 *
 * @short SkyOpacityNode derived class that draws lines from LineList
 *
 * @author Artem Fedoskin
 * @version 1.0
 */
class LineNode : public SkyOpacityNode
{
  public:
    /**
     * @short Constructor
     * @param lineList - lines that have to be drawn
     * @param skipList - lines that have to be skipped
     * @param color - line color
     * @param width - line width
     * @param drawStyle - not used currently
     */
    LineNode(LineList *lineList, SkipHashList *skipList, QColor color, int width, Qt::PenStyle drawStyle);
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
    QSGGeometryNode *m_geometryNode { nullptr };
    LineList *m_lineList { nullptr };
    SkipHashList *m_skipList { nullptr };

    QSGGeometry *m_geometry { nullptr };
    QSGFlatColorMaterial *m_material { nullptr };

    Qt::PenStyle m_drawStyle;
    QColor m_color;
};
