/*
    SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "trixelnode.h"

#include "skynode.h"

#include <QSGSimpleTextureNode>

TrixelNode::TrixelNode(const Trixel &trixel) : m_trixel(trixel)
{
}

void TrixelNode::deleteAllChildNodes()
{
    QLinkedList<QPair<SkyObject *, SkyNode *>>::iterator i = m_nodes.begin();

    while (i != m_nodes.cend())
    {
        SkyNode *node = (*i).second;
        if (node)
        {
            removeChildNode(node);
            delete node;

            *i = QPair<SkyObject *, SkyNode *>((*i).first, 0);
        }
        ++i;
    }
}

void TrixelNode::hide()
{
    m_hideCount++;
    SkyOpacityNode::hide();
}

void TrixelNode::show()
{
    SkyOpacityNode::show();
    m_hideCount = 0;
}
