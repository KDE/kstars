/** *************************************************************************
                          satellitenode.h  -  K Desktop Planetarium
                             -------------------
    begin                : 25/06/2016
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

#include "satellitenode.h"
#include "satellite.h"
#include "nodes/polynode.h"
#include "Options.h"
#include "ksutils.h"
#include "linelist.h"
#include "nodes/pointnode.h"
#include <QSGFlatColorMaterial>
#include "../rootnode.h"
#include "../labelsitem.h"
#include "labelnode.h"

SatelliteNode::SatelliteNode(Satellite* sat, RootNode *rootNode)
    :m_sat(sat), m_lines(0), m_point(0), m_rootNode(rootNode), m_label(0)
{

}

void SatelliteNode::initLines() {
    if(m_point) {
        delete m_point;
        m_point = 0;
    }

    if(!m_lines) {
        m_lines = new QSGGeometryNode;
        m_opacity->appendChildNode(m_lines);
        m_geometry = new QSGGeometry (QSGGeometry::defaultAttributes_Point2D(),0);
        m_lines->setGeometry(m_geometry);
        m_lines->setFlag(QSGNode::OwnsGeometry);

        m_material = new QSGFlatColorMaterial;
        m_lines->setOpaqueMaterial(m_material);
        m_lines->setFlag(QSGNode::OwnsMaterial);

        m_geometry->allocate(8);
        QSGGeometry::Point2D * vertex = m_geometry->vertexDataAsPoint2D();

        vertex[0].set( -0.5, -0.5); vertex[1].set(0.5,-0.5);
        vertex[2].set( 0.5, -0.5); vertex[3].set(0.5,0.5);
        vertex[4].set( 0.5, 0.5); vertex[5].set(-0.5,0.5);
        vertex[6].set( -0.5, 0.5); vertex[7].set(-0.5,-0.5);
    }
}

void SatelliteNode::initPoint() {
    if(m_lines) {
        delete m_lines;
        m_lines = 0;
    }
    if(!m_point) {
        m_point = new PointNode(m_rootNode, 'B', 3.5);
        m_opacity->appendChildNode(m_point);
    }
}

void SatelliteNode::update() {
    if(m_sat->selected()) {
        KStarsData *data = KStarsData::Instance();
        const Projector *m_proj = SkyMapLite::Instance()->projector();
        QPointF pos;

        bool visible = false;

        m_sat->HorizontalToEquatorial( data->lst(), data->geo()->lat() );

        pos = m_proj->toScreen( m_sat, true, &visible );

        if( !visible || !m_proj->onScreen( pos ) ) {
            hide();
            return;
        }
        show();

        if ( true ) {
            initPoint();
        } else {
            QColor color;
            initLines();
            if ( m_sat->isVisible() )
                color = data->colorScheme()->colorNamed( "VisibleSatColor" );
            else
                color = data->colorScheme()->colorNamed( "SatColor" );

            m_material->setColor(color);
        }

        changePos(pos);

        if ( Options::showSatellitesLabels() ) {
            if(!m_label) {
                m_label = SkyMapLite::rootNode()->labelsItem()->addLabel(m_sat,
                                                               LabelsItem::label_t::SATELLITE_LABEL);
            }
            m_label->setLabelPos(pos);
        }

    } else {
        hide();
    }
}

void SatelliteNode::hide() {
    SkyNode::hide();
    if(m_label) m_label->hide();
}

void SatelliteNode::changePos(QPointF pos) {
    //QSizeF size = m_point->size();
    QMatrix4x4 m (1,0,0,pos.x(),
                  0,1,0,pos.y(),
                  0,0,1,0,
                  0,0,0,1);
    //m.translate(-0.5*size.width(), -0.5*size.height());

    setMatrix(m);
    markDirty(QSGNode::DirtyMatrix);
}
