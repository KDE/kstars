/** *************************************************************************
                          skyitem.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 02/05/2016
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
#include "skyitem.h"
#include "rootnode.h"

SkyItem::SkyItem(LabelsItem::label_t labelType, RootNode* parent)
    :m_rootNode(parent), m_labelType(labelType)
{
    parent->appendChildNode(this);
}

SkyItem::~SkyItem() {
    rootNode()->labelsItem()->deleteLabels(m_labelType);
}

void SkyItem::show() {
    SkyOpacityNode::show();
    if(labelType() != LabelsItem::label_t::NO_LABEL) {
        rootNode()->labelsItem()->getLabelNode(m_labelType)->show();
    }
}

void SkyItem::hideLabels() {
    rootNode()->labelsItem()->getLabelNode(m_labelType)->hide();
}

void SkyItem::hide() {
    SkyOpacityNode::hide();
    rootNode()->labelsItem()->hideLabels(m_labelType);
}
