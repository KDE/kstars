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

#include "skymaplite.h"
#include "skynode.h"

SkyNode::SkyNode(SkyObject * skyObject)
    :m_opacity(new SkyOpacityNode), m_skyObject(skyObject), m_drawLabel(false), m_hideCount(0)
{
    appendChildNode(m_opacity);
}

SkyNode::SkyNode()
    :m_opacity(new SkyOpacityNode), m_skyObject(nullptr), m_drawLabel(false)
{
    appendChildNode(m_opacity);
}

void SkyNode::hide() {
    m_opacity->hide();
    m_hideCount++;
}

void SkyNode::show() {
    m_opacity->show();
    m_hideCount = 0;
}

void SkyNode::update(bool drawLabel) {
    m_drawLabel = drawLabel;
    update();
}
