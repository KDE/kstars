/*
    SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "pointnode.h"

#include "Options.h"
#include "skymaplite.h"
#include "../../rootnode.h"

#include <QQuickWindow>
#include <QSGSimpleTextureNode>

PointNode::PointNode(RootNode *p, char sp, float size)
    : spType(sp), texture(new QSGSimpleTextureNode), m_rootNode(p),
      starColorMode(Options::starColorMode())
{
    appendChildNode(texture);
    setSize(size);
}

void PointNode::setSize(float size)
{
    int isize      = qMin(static_cast<int>(size), 14);
    uint newStarCM = Options::starColorMode();
    if (size != m_size || newStarCM != starColorMode)
    {
        texture->setTexture(m_rootNode->getCachedTexture(isize, spType));

        //We divide size of texture by ratio. Otherwise texture will be very large
        qreal ratio = SkyMapLite::Instance()->window()->effectiveDevicePixelRatio();

        QSize tSize    = texture->texture()->textureSize();
        QRectF oldRect = texture->rect();
        texture->setRect(QRect(oldRect.x(), oldRect.y(), tSize.width() / ratio, tSize.height() / ratio));
        m_size        = size;
        starColorMode = newStarCM;
    }
}

QSizeF PointNode::size() const
{
    return texture->rect().size();
}
