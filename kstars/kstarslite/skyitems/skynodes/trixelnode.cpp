/** *************************************************************************
                          trixelnode.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 01/07/2016
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
