/** *************************************************************************
                          pointnode.h  -  K Desktop Planetarium
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

#include "skymaplite.h"
#include "pointnode.h"
#include "rootnode.h"

PointNode::PointNode(char sp, RootNode* p, float size)
    :spType(sp), texture(new QSGSimpleTextureNode), parentNode(p), skyMapLite(SkyMapLite::Instance())
{
    appendChildNode(texture);
    setSize(size);
}

void PointNode::setSize(float size) {
    int isize = qMin(static_cast<int>(size), 14);
    texture->setTexture(parentNode->getCachedTexture(isize, spType));

    QSize tSize = texture->texture()->textureSize();
    QRectF oldRect = texture->rect();
    texture->setRect(QRect(oldRect.x(),oldRect.y(),tSize.width(),tSize.height()));
}

void PointNode::changePos(QPointF pos) {
    QRectF oldRect = texture->rect();
    texture->setRect(QRect(pos.x(),pos.y(),oldRect.width(),oldRect.height()));
}
