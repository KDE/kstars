/** *************************************************************************
                          horizonitem.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 28/05/2016
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

#include "horizonitem.h"
#include "projections/projector.h"

#include "horizoncomponent.h"
#include "skynodes/rootnodes/rootnode.h"
#include "skynodes/horizonnode.h"

#include "Options.h"

HorizonItem::HorizonItem(QQuickItem* parent)
    :SkyItem(parent), m_horizonComp(0)
{
    Options::setRunClock(false);
    Options::setShowGround(false);
}

QSGNode* HorizonItem::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *updatePaintNodeData) {
    RootNode *n = static_cast<RootNode*>(oldNode);
    Q_UNUSED(updatePaintNodeData);
    QRectF rect = boundingRect();

    if (rect.isEmpty()) {
        delete n;
        return 0;
    }

    if(!n) {
        if(!m_horizonComp) return 0; // HorizonComponent is not ready
        n = new RootNode; // If no RootNode exists create one
        n->appendSkyNode(new HorizonNode(m_horizonComp->pointList()));
    }

    //Update clipping geometry. If m_clipPoly in SkyMapLite wasn't changed, geometry is not updated
    n->updateClipPoly();
    //HorizonNode *hNode = static_cast<HorizonNode *>(n->skyNodeAtIndex(0));
    for(int i = 0; i < n->skyNodesCount(); ++i) {
        SkyNode *hNode = n->skyNodeAtIndex(i);
        if(m_horizonComp->selected()) {
            hNode->update();
        } else {
            hNode->hide();
        }
    }

    return n;
}

