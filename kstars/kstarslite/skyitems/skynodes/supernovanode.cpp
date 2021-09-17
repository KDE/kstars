/*
    SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "supernovanode.h"

#include "ksutils.h"
#include "linelist.h"
#include "Options.h"
#include "supernova.h"
#include "nodes/pointnode.h"
#include "nodes/polynode.h"

#include <QSGFlatColorMaterial>

SupernovaNode::SupernovaNode(Supernova *snova) : m_snova(snova)
{
}

void SupernovaNode::update()
{
    KStarsData *data        = KStarsData::Instance();
    const Projector *m_proj = SkyMapLite::Instance()->projector();
    if (!m_proj->checkVisibility(m_snova))
    {
        hide();
        return;
    }

    bool visible = false;
    QPointF pos  = m_proj->toScreen(m_snova, true, &visible);
    //qDebug()<<"sup->ra() = "<<(sup->ra()).toHMSString()<<"sup->dec() = "<<sup->dec().toDMSString();
    //qDebug()<<"pos = "<<pos<<"m_proj->onScreen(pos) = "<<m_proj->onScreen(pos);
    if (!visible || !m_proj->onScreen(pos))
    {
        hide();
        return;
    }

    QColor color = data->colorScheme()->colorNamed("SupernovaColor");

    //Initialize m_lines if not already done
    if (!m_lines)
    {
        m_lines    = new QSGGeometryNode;
        m_geometry = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), 0);
        m_lines->setGeometry(m_geometry);
        m_lines->setFlag(QSGNode::OwnsGeometry);

        m_material = new QSGFlatColorMaterial;
        m_lines->setOpaqueMaterial(m_material);
        m_lines->setFlag(QSGNode::OwnsMaterial);
        addChildNode(m_lines);

        m_geometry->allocate(4);
        QSGGeometry::Point2D *vertex = m_geometry->vertexDataAsPoint2D();

        vertex[0].set(-2.0, 0.0);
        vertex[1].set(2.0, 0.0);
        vertex[2].set(0.0, -2.0);
        vertex[3].set(0.0, 2.0);
    }
    show();
    if (m_material->color() != color)
    {
        m_material->setColor(color);
        m_lines->markDirty(QSGNode::DirtyMaterial);
    }

    changePos(pos);

    return;
}

void SupernovaNode::changePos(QPointF pos)
{
    //QSizeF size = m_point->size();
    QMatrix4x4 m(1, 0, 0, pos.x(), 0, 1, 0, pos.y(), 0, 0, 1, 0, 0, 0, 0, 1);
    //m.translate(-0.5*size.width(), -0.5*size.height());

    setMatrix(m);
    markDirty(QSGNode::DirtyMatrix);
}
