/** *************************************************************************
                          pointnode.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 05/05/2016
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

#include <QImage>
#include <QQuickWindow>

#include "pointnode.h"
#include "../rootnodes/rootnode.h"

PointNode::PointNode(RootNode* p, char sp, float size)
    :spType(sp), texture(new QSGSimpleTextureNode), m_rootNode(p)
{
    appendChildNode(texture);
    setSize(size);
}

void PointNode::setSize(float size) {
    int isize = qMin(static_cast<int>(size), 14);
    texture->setTexture(m_rootNode->getCachedTexture(isize, spType));
    //markDirty(QSGNode::DirtyMaterial);

    QSize tSize = texture->texture()->textureSize();
    QRectF oldRect = texture->rect();
    texture->setRect(QRect(oldRect.x(),oldRect.y(),tSize.width(),tSize.height()));
}

void PointNode::show() {
    if(!opacity()) {
        setOpacity(1);
        markDirty(QSGNode::DirtyOpacity);
    }
}

void PointNode::hide() {
    if(opacity()) {
        setOpacity(0);
        markDirty(QSGNode::DirtyOpacity);
    }
}
