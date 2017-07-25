/** *************************************************************************
                          skynode.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 16/05/2016
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
