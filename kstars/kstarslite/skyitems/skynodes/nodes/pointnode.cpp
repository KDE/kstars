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
#include "kstarslite/skyitems/rootnode.h"
#include <QSGTexture>
#include "skymaplite.h"
#include "Options.h"

PointNode::PointNode(RootNode* p, char sp, float size)
    :spType(sp), texture(new QSGSimpleTextureNode), m_rootNode(p), m_size(-1), //Important to init to -1
      starColorMode(Options::starColorMode())
{
    appendChildNode(texture);
    setSize(size);
}

void PointNode::setSize(float size) {
    int isize = qMin(static_cast<int>(size), 14);
    uint newStarCM = Options::starColorMode();
    if(size != m_size || newStarCM != starColorMode) {
        texture->setTexture(m_rootNode->getCachedTexture(isize, spType));

        //We divide size of texture by ratio. Otherwise texture will be very large
        qreal ratio = SkyMapLite::Instance()->window()->effectiveDevicePixelRatio();

        QSize tSize = texture->texture()->textureSize();
        QRectF oldRect = texture->rect();
        texture->setRect(QRect(oldRect.x(),oldRect.y(),tSize.width()/ratio,tSize.height()/ratio));
        m_size = size;
        starColorMode = newStarCM;
    }
}
