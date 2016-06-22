/** *************************************************************************
                          polynode.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 28/05/2016
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

#include <QSGGeometryNode>
#include <QSGGeometry>
#include <QSGFlatColorMaterial>
#include <QPolygon>

#include "ellipsenode.h"
//#include <stdio.h>
//#include <stdlib.h>

extern "C"
{
#include "libtess/tessellate.h"
}

EllipseNode::EllipseNode(QColor color, int width)
    :m_geometryNode(new QSGGeometryNode), m_geometry(0),
      m_material(new QSGFlatColorMaterial), m_fillMode(false)
{
    m_geometry = new QSGGeometry (QSGGeometry::defaultAttributes_Point2D(),0);
    m_geometry->allocate(360);
    m_geometryNode->setGeometry(m_geometry);
    m_geometryNode->setFlag(QSGNode::OwnsGeometry);

    m_geometryNode->setOpaqueMaterial(m_material);
    m_geometryNode->setFlag(QSGNode::OwnsMaterial);

    if(color.isValid()) {
        setColor(color);
    }
    setLineWidth(width);

    appendChildNode(m_geometryNode);
}

void EllipseNode::setColor(QColor color) {
    if(color != m_material->color()) {
        m_material->setColor(color);
        m_geometryNode->markDirty(QSGNode::DirtyMaterial);
    }
}

void EllipseNode::setLineWidth(int width) {
    if(width != m_geometry->lineWidth()) {
        m_geometry->setLineWidth(width);
        m_geometryNode->markDirty(QSGNode::DirtyGeometry);
    }
}

void EllipseNode::updateGeometry(float x, float y, int width, int height, bool filled) {
    if(filled) {
        m_geometry->setDrawingMode(GL_POLYGON);
    } else {
        m_geometry->setDrawingMode(GL_LINE_LOOP);
    }

    QSGGeometry::Point2D * vertex = m_geometry->vertexDataAsPoint2D();

    for (int i=0; i < 360; ++i) {
        vertex[i].x = x + width*cos(i*(M_PI/180));
        vertex[i].y = y + height*sin(i*(M_PI/180));
    }
    /*vertex[0].x = 0;
    vertex[0].y = 0;

    vertex[1].x = 50;
    vertex[1].x = 50;*/

    m_geometryNode->markDirty(QSGNode::DirtyGeometry);

}
