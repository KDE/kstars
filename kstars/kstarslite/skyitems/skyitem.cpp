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
#include "../../skymaplite.h"

SkyItem::SkyItem(QQuickItem* parent)
    :QQuickItem(parent), m_skyMapLite(SkyMapLite::Instance())
{
    setFlag(ItemHasContents, true);
    //TODO: Dirty hack to allow call to updatePaintNode
    // Whenever the parent's dimensions changed, change dimensions of this item too
    connect(parent, &QQuickItem::widthChanged, this, &SkyItem::resizeItem);
    connect(parent, &QQuickItem::heightChanged, this, &SkyItem::resizeItem);

    connect(m_skyMapLite, &SkyMapLite::zoomChanged, this, &QQuickItem::update);
    setWidth(1);
    setHeight(1);
}

void SkyItem::resizeItem() {
    setWidth(parentItem()->width());
    setHeight(parentItem()->height());
}
