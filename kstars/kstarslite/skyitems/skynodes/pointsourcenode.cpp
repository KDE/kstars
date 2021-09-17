/*
    SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "skyobject.h"
#include "Options.h"

#include <QSGSimpleTextureNode>

#include "pointsourcenode.h"
#include "nodes/pointnode.h"

#include "../rootnode.h"
#include "../labelsitem.h"
#include "labelnode.h"

PointSourceNode::PointSourceNode(SkyObject *skyObject, RootNode *parentNode, LabelsItem::label_t labelType,
                                 char spType, float size, short trixel)
    : SkyNode(skyObject), m_rootNode(parentNode), m_spType(spType), m_size(size),
      m_labelType(labelType), m_trixel(trixel)
{
}

float PointSourceNode::starWidth(float mag) const
{
    //adjust maglimit for ZoomLevel
    const double maxSize = 10.0;

    double lgmin = log10(MINZOOM);
    //    double lgmax = log10(MAXZOOM);
    double lgz = log10(Options::zoomFactor());

    float sizeFactor = maxSize + (lgz - lgmin);

    float m_sizeMagLim = map()->sizeMagLim();

    float size = (sizeFactor * (m_sizeMagLim - mag) / m_sizeMagLim) + 1.;
    if (size <= 1.0)
        size = 1.0;
    if (size > maxSize)
        size = maxSize;

    return size;
}

PointSourceNode::~PointSourceNode()
{
    if (m_label &&
        (m_labelType == LabelsItem::label_t::STAR_LABEL || m_labelType == LabelsItem::label_t::CATALOG_STAR_LABEL))
    {
        m_rootNode->labelsItem()->deleteLabel(m_label);
    }
}

void PointSourceNode::updatePoint()
{
    if (!m_point)
    {
        m_point = new PointNode(m_rootNode, m_spType, starWidth(m_size));
        addChildNode(m_point);
    }
    show();
    m_point->setSize(starWidth(m_skyObject->mag())); //Set points size base on the magnitude of object
}

void PointSourceNode::changePos(QPointF pos)
{
    QSizeF size = m_point->size();
    QMatrix4x4 m(1, 0, 0, pos.x(), 0, 1, 0, pos.y(), 0, 0, 1, 0, 0, 0, 0, 1);
    m.translate(-0.5 * size.width(), -0.5 * size.height());

    setMatrix(m);
    markDirty(QSGNode::DirtyMatrix);
}

void PointSourceNode::update()
{
    if (!projector()->checkVisibility(m_skyObject))
    {
        hide();
        return;
    }

    bool visible = false;
    pos          = projector()->toScreen(m_skyObject, true, &visible);
    if (visible &&
        projector()->onScreen(
            pos)) // FIXME: onScreen here should use canvas size rather than SkyMap size, especially while printing in portrait mode! (Inherited from SkyMap)
    {
        updatePos(pos, m_drawLabel);
    }
    else
    {
        hide();
    }
}

void PointSourceNode::updatePos(QPointF pos, bool drawLabel)
{
    updatePoint();
    changePos(pos);

    m_drawLabel = drawLabel;

    if (m_drawLabel && m_labelType != LabelsItem::label_t::NO_LABEL)
    {
        if (!m_label) //This way labels will be created only when they are needed
        {
            if (m_trixel != -1)
            {
                m_label = m_rootNode->labelsItem()->addLabel(m_skyObject, m_labelType, m_trixel);
            }
            else
            {
                m_label = m_rootNode->labelsItem()->addLabel(m_skyObject, m_labelType);
            }
        }
        m_label->setLabelPos(pos);
    }
    else
    {
        if (m_label)
            m_label->hide();
    }
}

void PointSourceNode::hide()
{
    if (m_label)
        m_label->hide();
    SkyNode::hide();
}
