/*
    SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "skyitem.h"

#include "rootnode.h"

SkyItem::SkyItem(LabelsItem::label_t labelType, RootNode *parent) : m_rootNode(parent), m_labelType(labelType)
{
    parent->appendChildNode(this);
}

SkyItem::~SkyItem()
{
    /*PointSourceNode in StarItem deletes the label on its own because nodes of this type are created and
     * deleted during the lifetime of program to decrease memory consumption*/
    if (m_labelType != LabelsItem::label_t::STAR_LABEL)
        rootNode()->labelsItem()->deleteLabels(m_labelType);
}

void SkyItem::show()
{
    SkyOpacityNode::show();
    if (labelType() != LabelsItem::label_t::NO_LABEL)
    {
        rootNode()->labelsItem()->getLabelNode(m_labelType)->show();
    }
}

void SkyItem::hideLabels()
{
    rootNode()->labelsItem()->getLabelNode(m_labelType)->hide();
}

void SkyItem::hide()
{
    SkyOpacityNode::hide();
    rootNode()->labelsItem()->hideLabels(m_labelType);
}
