/*
    SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "linenode.h"

#include "skymaplite.h"
#include "projections/projector.h"
#include "skycomponents/skiphashlist.h"

#include <QLinkedList>
#include <QSGFlatColorMaterial>

LineNode::LineNode(LineList *lineList, SkipHashList *skipList, QColor color, int width, Qt::PenStyle drawStyle)
    : m_geometryNode(new QSGGeometryNode), m_lineList(lineList), m_skipList(skipList),
      m_material(new QSGFlatColorMaterial), m_drawStyle(drawStyle)
{
    m_geometryNode->setOpaqueMaterial(m_material);
    //m_geometryNode->setFlag(QSGNode::OwnsMaterial);
    m_material->setColor(color);

    m_geometry = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), 0);
    m_geometryNode->setGeometry(m_geometry);
    //m_geometryNode->setFlag(QSGNode::OwnsGeometry);
    setWidth(width);

    appendChildNode(m_geometryNode);
}

LineNode::~LineNode()
{
    delete m_geometry;
    delete m_material;
}

void LineNode::setColor(QColor color)
{
    if (m_material->color() != color)
    {
        m_material->setColor(color);
        m_color = color;
        m_geometryNode->markDirty(QSGNode::DirtyMaterial);
    }
}

void LineNode::setWidth(int width)
{
    m_geometry->setLineWidth(width);
    m_geometryNode->markDirty(QSGNode::DirtyGeometry);
}

void LineNode::setDrawStyle(Qt::PenStyle style)
{
    m_drawStyle = style;
}

void LineNode::setStyle(QColor color, int width, Qt::PenStyle drawStyle)
{
    setColor(color);
    setWidth(width);
    setDrawStyle(drawStyle);
}

void LineNode::updateGeometry()
{
    SkyList *points = m_lineList->points();

    m_geometry->setDrawingMode(GL_LINES);

    const Projector *m_proj = SkyMapLite::Instance()->projector();

    bool isVisible, isVisibleLast;

    QPointF oLast = m_proj->toScreen(points->first().get(), true, &isVisibleLast);

    // & with the result of checkVisibility to clip away things below horizon
    isVisibleLast &= m_proj->checkVisibility(points->first().get());
    QPointF oThis;

    QLinkedList<QPointF> newPoints;

    for (int j = 1; j < points->size(); j++)
    {
        SkyPoint *pThis = points->at(j).get();

        /*In regular KStars we first call checkVisibility and then toScreen
        Here we minimize the number of calls to toScreen by proceeding only if
        checkVisibility is true*/
        oThis = m_proj->toScreen(pThis, true, &isVisible);
        // & with the result of checkVisibility to clip away things below horizon
        isVisible &= m_proj->checkVisibility(pThis);
        bool doSkip = false;
        if (m_skipList)
        {
            doSkip = m_skipList->skip(j);
        }

        bool pointsVisible = false;
        //Temporary solution to avoid random lines in Gnomonic projection and draw lines up to horizon
        if (SkyMapLite::Instance()->projector()->type() == Projector::Gnomonic)
        {
            if (isVisible && isVisibleLast)
                pointsVisible = true;
        }
        else
        {
            if (isVisible || isVisibleLast)
                pointsVisible = true;
        }

        if (!doSkip)
        {
            if (pointsVisible)
            {
                newPoints.append(oLast);
                newPoints.append(oThis);
            }
        }
        oLast         = oThis;
        isVisibleLast = isVisible;
    }

    int size = newPoints.size();
    m_geometry->allocate(size);

    QSGGeometry::Point2D *vertex = m_geometry->vertexDataAsPoint2D();

    QLinkedList<QPointF>::const_iterator i = newPoints.constBegin();
    int c                                  = 0;

    while (i != newPoints.constEnd())
    {
        vertex[c].x = (*i).x();
        vertex[c].y = (*i).y();
        ++c;
        ++i;
    }
    m_geometryNode->markDirty(QSGNode::DirtyGeometry);
}
