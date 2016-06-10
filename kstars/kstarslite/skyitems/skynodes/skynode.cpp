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
    :m_skyObject(skyObject), m_opacity(new QSGOpacityNode)
{
    appendChildNode(m_opacity);
}

SkyNode::SkyNode()
    :m_skyObject(nullptr), m_opacity(new QSGOpacityNode)
{

}

void SkyNode::update(bool drawLabel) {
    m_drawLabel = drawLabel;
    update();
}

void SkyNode::hide() {
    if(m_opacity->opacity()) {
        m_opacity->setOpacity(0);
        m_opacity->markDirty(QSGNode::DirtyOpacity);
    }
}

void SkyNode::show() {
    if(!m_opacity->opacity()) {
        m_opacity->setOpacity(1);
        m_opacity->markDirty(QSGNode::DirtyOpacity);
    }
}

bool SkyNode::visible() {
    if(m_opacity->opacity() != 0) {
        return true;
    }
    return false;
}
