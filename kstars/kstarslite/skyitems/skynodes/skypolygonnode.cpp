/** *************************************************************************
                          skypolygonnode.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 23/06/2016
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
#include "skypolygonnode.h"
#include "nodes/polynode.h"
#include "Options.h"
#include "ksutils.h"
#include "linelist.h"

SkyPolygonNode::SkyPolygonNode(LineList* list)
    :m_list(list), m_polygonNode(new PolyNode)
{
    addChildNode(m_polygonNode);
}

void SkyPolygonNode::update(bool forceClip) {
    if(!m_polygonNode->visible()) {
        m_polygonNode->show();
    }

    bool isVisible, isVisibleLast;
    SkyList *points = m_list->points();
    QPolygonF polygon;
    const Projector *m_proj = SkyMapLite::Instance()->projector();

    if (forceClip == false)
    {
        for ( int i = 0; i < points->size(); ++i )
        {
            polygon << m_proj->toScreen( points->at( i ), false, &isVisibleLast);
            isVisible |= isVisibleLast;
        }

        // If 1+ points are visible, draw it
        if ( polygon.size() && isVisible) {
            m_polygonNode->updateGeometry(polygon,true);
        } else {
            m_polygonNode->hide();
        }

        return;
    }


    SkyPoint* pLast = points->last();
    QPointF   oLast = m_proj->toScreen( pLast, true, &isVisibleLast );
    // & with the result of checkVisibility to clip away things below horizon
    isVisibleLast &= m_proj->checkVisibility(pLast);

    for ( int i = 0; i < points->size(); ++i ) {
        SkyPoint* pThis = points->at( i );
        QPointF oThis = m_proj->toScreen( pThis, true, &isVisible );
        // & with the result of checkVisibility to clip away things below horizon
        isVisible &= m_proj->checkVisibility(pThis);


        if ( isVisible && isVisibleLast ) {
            polygon << oThis;
        } else if ( isVisibleLast ) {
            QPointF oMid = m_proj->clipLine( pLast, pThis );
            polygon << oMid;
        } else if ( isVisible ) {
            QPointF oMid = m_proj->clipLine( pThis, pLast );
            polygon << oMid;
            polygon << oThis;
        }

        pLast = pThis;
        oLast = oThis;
        isVisibleLast = isVisible;
    }

    if ( polygon.size() ) {
        m_polygonNode->updateGeometry(polygon,true);
    } else {
        m_polygonNode->hide();
        return;
    }
}

void SkyPolygonNode::setColor(QColor color) {
    m_polygonNode->setColor(color);
}

void SkyPolygonNode::hide() {
    m_polygonNode->hide();
}
