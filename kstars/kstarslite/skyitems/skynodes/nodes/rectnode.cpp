/** *************************************************************************
                          rectnode.cpp  -  K Desktop Planetarium
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
#include "rectnode.h"
#include <QSGGeometryNode>
#include <QSGFlatColorMaterial>

RectNode::RectNode(bool filled, QColor color) {
    m_geometry = new QSGGeometry (QSGGeometry::defaultAttributes_Point2D(),0);
    m_geometry->allocate(4);
    setGeometry(m_geometry);
    setFlag(QSGNode::OwnsGeometry);

    m_material = new QSGFlatColorMaterial;
    setMaterial(m_material);
    setFlag(QSGNode::OwnsMaterial);

    setFilled(filled);
}

void RectNode::setRect(int x, int y, int w, int h) {
    QSGGeometry::Point2D * vertex = m_geometry->vertexDataAsPoint2D();
    vertex[0].set(x,y);
    vertex[1].set(x + w,y);
    vertex[2].set(x + w,y + h);
    vertex[3].set(x,y + h);
    markDirty(QSGNode::DirtyGeometry);
}

void RectNode::setColor(QColor color) {
    if(m_material->color() != color) {
        m_material->setColor(color);
        markDirty(QSGNode::DirtyMaterial);
    }
}

void RectNode::setFilled(bool filled) {
    m_filled = filled;
    if(filled) {
        m_geometry->setDrawingMode(GL_TRIANGLE_FAN);
    } else {
        m_geometry->setDrawingMode(GL_LINE_LOOP);
    }
}
