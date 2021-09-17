/*
    SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "satellitenode.h"

#include "ksutils.h"
#include "labelnode.h"
#include "linelist.h"
#include "Options.h"
#include "satellite.h"
#include "nodes/pointnode.h"
#include "nodes/polynode.h"
#include "../rootnode.h"
#include "../labelsitem.h"

#include <QSGFlatColorMaterial>

SatelliteNode::SatelliteNode(Satellite *sat, RootNode *rootNode)
    : m_sat(sat), m_rootNode(rootNode)
{
}

void SatelliteNode::initLines()
{
    if (m_point)
    {
        delete m_point;
        m_point = 0;
    }
    if (!m_lines)
    {
        m_lines    = new QSGGeometryNode;
        m_geometry = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), 0);
        m_lines->setGeometry(m_geometry);
        m_lines->setFlag(QSGNode::OwnsGeometry);
        m_geometry->setDrawingMode(GL_LINES);

        m_material = new QSGFlatColorMaterial;
        m_lines->setOpaqueMaterial(m_material);
        m_lines->setFlag(QSGNode::OwnsMaterial);
        addChildNode(m_lines);

        m_geometry->allocate(8);
        QSGGeometry::Point2D *vertex = m_geometry->vertexDataAsPoint2D();

        vertex[0].set(-0.5, -0.5);
        vertex[1].set(0.5, -0.5);
        vertex[2].set(0.5, -0.5);
        vertex[3].set(0.5, 0.5);
        vertex[4].set(0.5, 0.5);
        vertex[5].set(-0.5, 0.5);
        vertex[6].set(-0.5, 0.5);
        vertex[7].set(-0.5, -0.5);

        m_lines->markDirty(QSGNode::DirtyGeometry);
        m_lines->markDirty(QSGNode::DirtyMaterial);
    }
}

void SatelliteNode::initPoint()
{
    if (m_lines)
    {
        delete m_lines;
        m_lines = 0;
    }
    if (!m_point)
    {
        m_point = new PointNode(m_rootNode, 'B', 3.5);
        addChildNode(m_point);
    }
}

void SatelliteNode::update()
{
    if (m_sat->selected())
    {
        KStarsData *data        = KStarsData::Instance();
        const Projector *m_proj = SkyMapLite::Instance()->projector();
        QPointF pos;

        bool visible = false;

        m_sat->HorizontalToEquatorial(data->lst(), data->geo()->lat());

        pos = m_proj->toScreen(m_sat, true, &visible);

        if (!visible || !m_proj->onScreen(pos))
        {
            hide();
            return;
        }
        show();

        if (Options::drawSatellitesLikeStars())
        {
            initPoint();
        }
        else
        {
            QColor color;
            initLines();
            if (m_sat->isVisible())
                color = data->colorScheme()->colorNamed("VisibleSatColor");
            else
                color = data->colorScheme()->colorNamed("SatColor");

            m_material->setColor(color);
        }

        changePos(pos);

        if (Options::showSatellitesLabels())
        {
            if (!m_label)
            {
                m_label = SkyMapLite::rootNode()->labelsItem()->addLabel(m_sat, LabelsItem::label_t::SATELLITE_LABEL);
            }
            m_label->setLabelPos(pos);
        }
    }
    else
    {
        hide();
    }
}

void SatelliteNode::hide()
{
    SkyNode::hide();
    if (m_label)
        m_label->hide();
}

void SatelliteNode::changePos(QPointF pos)
{
    //QSizeF size = m_point->size();
    QMatrix4x4 m(1, 0, 0, pos.x(), 0, 1, 0, pos.y(), 0, 0, 1, 0, 0, 0, 0, 1);
    //m.translate(-0.5*size.width(), -0.5*size.height());

    setMatrix(m);
    markDirty(QSGNode::DirtyMatrix);
}
