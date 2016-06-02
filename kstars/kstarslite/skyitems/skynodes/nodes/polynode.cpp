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
#include <QSGFlatColorMaterial>
#include <QSGSimpleTextureNode>
#include <QPainter>

#include "polynode.h"
#include "skymaplite.h"
#include "kstarsdata.h"
#include <stdio.h>
#include <stdlib.h>
extern "C"
{
#include "libtess/tessellate.h"
}
/*
#include "qquickwindow.h"
#include "triangle/include/tpp_interface.hpp"
#include "polypartition/polypartition.h"
#include "poly2tri/poly2tri.h"*/

//using namespace tpp;

PolyNode::PolyNode()
    :m_geometryNode(new QSGGeometryNode), m_geometry(0),
      m_material(new QSGFlatColorMaterial)
{
    m_geometry = new QSGGeometry (QSGGeometry::defaultAttributes_Point2D(),0);
    m_geometryNode->setGeometry(m_geometry);
    m_geometryNode->setFlag(QSGNode::OwnsGeometry);

    m_geometryNode->setOpaqueMaterial(m_material);
    m_geometryNode->setFlag(QSGNode::OwnsMaterial);

    appendChildNode(m_geometryNode);
}

void PolyNode::setColor(QColor color) {
    m_material->setColor(color);
    m_geometryNode->markDirty(QSGNode::DirtyMaterial);
}

void PolyNode::updateGeometry(QPolygonF polygon, bool filled) {
    if(!filled) {
        m_geometry->setDrawingMode(GL_LINE_STRIP);
        int size = polygon.size();
        m_geometry->allocate(size);

        QSGGeometry::Point2D * vertex = m_geometry->vertexDataAsPoint2D();

        for (int i = 0; i < size; ++i) {
            vertex[i].x = polygon[i].x();
            vertex[i].y = polygon[i].y();
        }
    } else {
        m_geometry->setDrawingMode(GL_TRIANGLES);

        double *coordinates_out;
        int *tris_out;
        int nverts, ntris, i;

        QPolygonF pol = polygon;

        int polySize = pol.size()*2;

        double vertices_array[polySize];

        const double *p = vertices_array;

        for(int i = 0; i < polySize; i += 2) {
            vertices_array[i] = pol[i/2].x();
            vertices_array[i+1] = pol[i/2].y();
        }

        const double *contours_array[] = {vertices_array, vertices_array + polySize};
        int contours_size = 2;

        tessellate(&coordinates_out, &nverts,
                   &tris_out, &ntris,
                   contours_array, contours_array + contours_size);

        m_geometry->allocate(3 * ntris);
        QSGGeometry::Point2D * vertex = m_geometry->vertexDataAsPoint2D ();

        for (i=0; i<3 * ntris; ++i) {
            int tris = tris_out[i];
            vertex[i].x = coordinates_out[tris_out[i]*2];
            vertex[i].y = coordinates_out[tris_out[i]*2+1];
        }
        free(coordinates_out);
        if (tris_out) free(tris_out);
    }
    m_geometryNode->markDirty(QSGNode::DirtyGeometry);

}

void PolyNode::show() {
    if(!opacity()) {
        setOpacity(1);
        markDirty(QSGNode::DirtyOpacity);
    }
}

void PolyNode::hide() {
    if(opacity()) {
        setOpacity(0);
        markDirty(QSGNode::DirtyOpacity);
    }
}

