/** *************************************************************************
                          LineNode.cpp  -  K Desktop Planetarium
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

#include "linenode.h"
#include <QSGFlatColorMaterial>
#include "skymaplite.h"
#include "projections/projector.h"
#include <QLinkedList>
//#include <forward_list>

LineNode::LineNode(LineList *lineList)
    :m_lineList(lineList), m_material(new QSGFlatColorMaterial),
      m_geometry(new QSGGeometry (QSGGeometry::defaultAttributes_Point2D(),0)),
      m_geometryNode(new QSGGeometryNode)
{
    appendChildNode(m_geometryNode);
    m_geometryNode->setGeometry(m_geometry);
    setFlag(QSGNode::OwnsGeometry);

    m_geometryNode->setOpaqueMaterial(m_material);
    setFlag(QSGNode::OwnsMaterial);
}

void LineNode::setColor(QColor color) {
    m_material->setColor(color);
    m_geometryNode->markDirty(QSGNode::DirtyMaterial);
}

void LineNode::setWidth(int width) {
    m_geometry->setLineWidth(width);
    m_geometryNode->markDirty(QSGNode::DirtyGeometry);
}

void LineNode::setDrawStyle(Qt::PenStyle style) {
    m_drawStyle = style;
}

void LineNode::hide() {
    setOpacity(0);
    markDirty(QSGNode::DirtyOpacity);
}

void LineNode::updateGeometry() {
    setOpacity(1);
    SkyList *points = m_lineList->points();

    m_geometry->setDrawingMode(GL_LINES);

    const Projector *m_proj = SkyMapLite::Instance()->projector();

    bool isVisible, isVisibleLast;

    QPointF oLast = m_proj->toScreen( points->first(), true, &isVisibleLast );
    // & with the result of checkVisibility to clip away things below horizon
    isVisibleLast &= m_proj->checkVisibility( points->first() );
    QPointF oThis, oThis2;

    QLinkedList<QPointF> newPoints;

    for ( int j = 1 ; j < points->size() ; j++ ) {
        SkyPoint* pThis = points->at( j );
        oThis2 = oThis = m_proj->toScreen( pThis, true, &isVisible );
        // & with the result of checkVisibility to clip away things below horizon
        isVisible &= m_proj->checkVisibility(pThis);
        bool doSkip = false;
        /*if( skipList ) {
            doSkip = skipList->skip(j);
        }*/

        if ( !doSkip ) {
            if ( (isVisible) ) {
                    newPoints.append(oLast);
                    newPoints.append(oThis);
                //if ( label )
                //  label->updateLabelCandidates( oThis.x(), oThis.y(), list, j );
            }
        }
        oLast = oThis2;
        isVisibleLast = isVisible;
    }

    int size = newPoints.size();
    m_geometry->allocate(size);

    QSGGeometry::Point2D * vertex = m_geometry->vertexDataAsPoint2D();

    QLinkedList<QPointF>::const_iterator i = newPoints.constBegin();
    int c = 0;
    while ( i != newPoints.constEnd()) {
        vertex[c].x = (*i).x();
        vertex[c].y = (*i).y();
        c++;
        i++;
    }

    m_geometryNode->markDirty(QSGNode::DirtyGeometry);
}
