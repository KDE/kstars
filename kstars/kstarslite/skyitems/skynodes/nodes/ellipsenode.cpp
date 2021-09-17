/*
    SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "ellipsenode.h"

#include <QSGFlatColorMaterial>
#include <QSGGeometry>
#include <QSGGeometryNode>

#include <cmath>

EllipseNode::EllipseNode(const QColor &color, int width)
    : m_geometryNode(new QSGGeometryNode), m_material(new QSGFlatColorMaterial)
{
    m_geometry = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), 0);
    m_geometry->allocate(60);
    m_geometryNode->setGeometry(m_geometry);
    m_geometryNode->setFlag(QSGNode::OwnsGeometry);

    m_geometryNode->setOpaqueMaterial(m_material);
    m_geometryNode->setFlag(QSGNode::OwnsMaterial);

    if (color.isValid())
    {
        setColor(color);
    }
    setLineWidth(width);

    appendChildNode(m_geometryNode);
}

void EllipseNode::setColor(QColor color)
{
    if (color != m_material->color())
    {
        m_material->setColor(color);
        m_geometryNode->markDirty(QSGNode::DirtyMaterial);
    }
}

void EllipseNode::setLineWidth(int width)
{
    if (width != m_geometry->lineWidth())
    {
        m_geometry->setLineWidth(width);
        m_geometryNode->markDirty(QSGNode::DirtyGeometry);
    }
}

void EllipseNode::updateGeometry(float x, float y, int width, int height, bool filled)
{
    if (filled)
    {
        m_geometry->setDrawingMode(GL_TRIANGLE_FAN);
    }
    else
    {
        m_geometry->setDrawingMode(GL_LINE_LOOP);
    }

    QSGGeometry::Point2D *vertex = m_geometry->vertexDataAsPoint2D();

    float rad = M_PI / 180;

    width /= 2;
    height /= 2;

    if (m_width != width || m_height != height)
    {
        for (int i = 0; i < 360; i += 6)
        {
            vertex[i / 6].x = width * cos(i * rad);
            vertex[i / 6].y = height * sin(i * rad);
        }
        m_geometryNode->markDirty(QSGNode::DirtyGeometry);

        m_width  = width;
        m_height = height;
    }
    if (m_x != x || m_y != y)
    {
        QMatrix4x4 m(1, 0, 0, x, 0, 1, 0, y, 0, 0, 1, 0, 0, 0, 0, 1);
        setMatrix(m);
        markDirty(QSGNode::DirtyMatrix);

        m_x = x;
        m_y = y;
    }
}
