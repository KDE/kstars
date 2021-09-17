/*
    SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "rectnode.h"

#include <QSGFlatColorMaterial>
#include <QSGGeometryNode>

RectNode::RectNode(bool filled)
{
    m_geometry = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), 0);
    m_geometry->allocate(4);
    setGeometry(m_geometry);
    setFlag(QSGNode::OwnsGeometry);

    m_material = new QSGFlatColorMaterial;
    setMaterial(m_material);
    setFlag(QSGNode::OwnsMaterial);

    setFilled(filled);
}

void RectNode::setRect(int x, int y, int w, int h)
{
    QSGGeometry::Point2D *vertex = m_geometry->vertexDataAsPoint2D();
    vertex[0].set(x, y);
    vertex[1].set(x + w, y);
    vertex[2].set(x + w, y + h);
    vertex[3].set(x, y + h);
    markDirty(QSGNode::DirtyGeometry);
}

void RectNode::setColor(const QColor &color)
{
    if (m_material->color() != color)
    {
        m_material->setColor(color);
        markDirty(QSGNode::DirtyMaterial);
    }
}

void RectNode::setFilled(bool filled)
{
    m_filled = filled;
    if (filled)
    {
        m_geometry->setDrawingMode(GL_TRIANGLE_FAN);
    }
    else
    {
        m_geometry->setDrawingMode(GL_LINE_LOOP);
    }
}
