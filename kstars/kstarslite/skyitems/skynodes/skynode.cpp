/*
    SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "skynode.h"

#include "../skyopacitynode.h"

SkyNode::SkyNode(SkyObject *skyObject)
    : m_opacity(new SkyOpacityNode), m_skyObject(skyObject)
{
    appendChildNode(m_opacity);
}

SkyNode::SkyNode() : m_opacity(new SkyOpacityNode)
{
    appendChildNode(m_opacity);
}

void SkyNode::hide()
{
    m_opacity->hide();
    m_hideCount++;
}

void SkyNode::show()
{
    m_opacity->show();
    m_hideCount = 0;
}

void SkyNode::update(bool drawLabel)
{
    m_drawLabel = drawLabel;
    update();
}

void SkyNode::addChildNode(QSGNode *node)
{
    m_opacity->appendChildNode(node);
}

bool SkyNode::visible()
{
    return m_opacity->visible();
}
