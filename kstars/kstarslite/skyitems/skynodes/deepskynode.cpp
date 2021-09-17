/*
    SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "deepskynode.h"

#include "deepskyobject.h"
#include "dsosymbolnode.h"
#include "labelnode.h"
#include "Options.h"
#include "skyobject.h"
#include "trixelnode.h"
#include "nodes/pointnode.h"
#include "../rootnode.h"
#include "../labelsitem.h"

#include <QSGSimpleTextureNode>
#include <QQuickWindow>


DeepSkyNode::DeepSkyNode(DeepSkyObject *skyObject, DSOSymbolNode *symbol, LabelsItem::label_t labelType, short trixel)
    : m_trixel(trixel), m_labelType(labelType), m_dso(skyObject), m_symbol(symbol)
{
    m_symbol->hide();
}

DeepSkyNode::~DeepSkyNode()
{
    if (m_label)
        SkyMapLite::rootNode()->labelsItem()->deleteLabel(m_label);
}

void DeepSkyNode::changePos(QPointF pos)
{
    QSizeF size = m_objImg->rect().size();
    QMatrix4x4 m(1, 0, 0, pos.x(), 0, 1, 0, pos.y(), 0, 0, 1, 0, 0, 0, 0, 1);
    //FIXME: this is probably incorrect (inherited from drawDeepSkyImage())
    m.rotate(m_angle, 0, 0, 1);
    m.translate(-0.5 * size.width(), -0.5 * size.height());

    setMatrix(m);
    markDirty(QSGNode::DirtyMatrix);
}

void DeepSkyNode::setColor(QColor color, TrixelNode *symbolTrixel)
{
    if (m_symbol->getColor() != color)
    {
        delete m_symbol;
        m_symbol = new DSOSymbolNode(m_dso, color);
        symbolTrixel->appendChildNode(m_symbol);
    }

    //m_label->setColor
}

void DeepSkyNode::update(bool drawImage, bool drawLabel, QPointF pos)
{
    if (pos.x() == -1 && pos.y() == -1)
    {
        const Projector *proj = projector();
        if (!proj->checkVisibility(m_dso))
        {
            hide();
            return;
        }

        bool visible = false;
        pos          = proj->toScreen(m_dso, true, &visible);
        if (!visible || !proj->onScreen(pos))
        {
            hide();
            return;
        }
    }
    show();

    // if size is 0.0 set it to 1.0, this are normally stars (type 0 and 1)
    // if we use size 0.0 the star wouldn't be drawn
    float majorAxis = m_dso->a();
    if (majorAxis == 0.0)
    {
        majorAxis = 1.0;
    }

    float size = majorAxis * dms::PI * Options::zoomFactor() / 10800.0;

    double zoom = Options::zoomFactor();
    double w    = m_dso->a() * dms::PI * zoom / 10800.0;
    double h    = m_dso->e() * w;

    m_angle = projector()->findPA(m_dso, pos.x(), pos.y());

    if (drawImage && Options::zoomFactor() > 5. * MINZOOM)
    {
        if (!(m_dso->image().isNull()))
        {
            if (!m_objImg)
            {
                m_objImg = new QSGSimpleTextureNode;
                m_objImg->setTexture(SkyMapLite::Instance()->window()->createTextureFromImage(
                    m_dso->image(), QQuickWindow::TextureCanUseAtlas));
                m_objImg->setOwnsTexture(true);
                m_opacity->appendChildNode(m_objImg);
            }

            m_objImg->setRect(0, 0, w, h);
            changePos(pos);
        }
    }
    else
    {
        hide();
    }

    //Draw symbol
    m_symbol->update(size, pos, m_angle);

    // Draw label
    if (drawLabel)
    {
        if (!m_label)
        {
            if (m_trixel != -1)
            {
                m_label = SkyMapLite::rootNode()->labelsItem()->addLabel(m_dso, m_labelType, m_trixel);
            }
            else
            {
                m_label = SkyMapLite::rootNode()->labelsItem()->addLabel(m_dso, m_labelType);
            }
        }
        m_label->setLabelPos(pos);
    }
    else if (m_label)
    {
        m_label->hide();
    }
}

void DeepSkyNode::hide()
{
    SkyNode::hide();
    if (m_label)
        m_label->hide();
    m_symbol->hide();
}
