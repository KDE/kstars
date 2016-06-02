/** *************************************************************************
                          cometsitem.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 16/05/2016
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

#include "cometsitem.h"

#include "Options.h"
#include "projections/projector.h"
#include "kscomet.h"

#include "skynodes/rootnodes/rootnode.h"
#include "skynodes/pointsourcenode.h"

CometsItem::CometsItem(QQuickItem* parent)
    :SkyItem(parent)
{

}

void CometsItem::addComet(KSComet * comet) {
    if(!m_comets.contains(comet) && !m_toAdd.contains(comet))
        m_toAdd.append(comet);
}

void CometsItem::clear() {
    m_clear = true;
    m_comets.clear();
    m_toAdd.clear();
}

QSGNode* CometsItem::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *updatePaintNodeData) {

    Q_UNUSED(updatePaintNodeData);

    RootNode *n = static_cast<RootNode*>(oldNode);
    QRectF rect = boundingRect();

    if (!Options::showComets() || Options::zoomFactor() < 10*MINZOOM || rect.isEmpty()) {
        delete n;
        return 0;
    }

    if(!n) {
        n = new RootNode; // If no RootNode exists create one
        int pComLen = m_comets.length();
        if(pComLen > 0) {
            /* If there are some asteroids that have been already displayed once then recreate them
                 in new instance of RootNode*/
            for(int i = 0; i < pComLen; ++i) {
                n->appendSkyNode(new PointSourceNode(m_comets[i],n));
            }
        }
    }

    if(m_clear) {
        n->removeAllSkyNodes();
        m_clear = false;
    }

    int addLength = m_toAdd.length();
    if(addLength > 0) { // If there are some new comets to add
        for(int i = 0; i < addLength; ++i) {
            m_comets.append(m_toAdd[i]);
            n->appendSkyNode(new PointSourceNode(m_toAdd[i],n));
        }
        m_toAdd.clear();
    }
    //Update clipping geometry. If m_clipPoly in SkyMapLite wasn't changed, geometry is not updated
    n->updateClipPoly();

    /*bool hideLabels =  ! Options::showCometNames() ||
                       (SkyMap::Instance()->isSlewing() &&
                        Options::hideLabels() );
    double rsunLabelLimit = Options::maxRadCometName();

    //FIXME: Should these be config'able?
    skyp->setPen( QPen( QColor( "darkcyan" ) ) );
    skyp->setBrush( QBrush( QColor( "darkcyan" ) ) );*/

    //Traverse all children nodes of RootNode
    for(int i = 0; i < n->skyNodesCount(); ++i) {
        SkyNode* skyNode = static_cast<SkyNode*>(n->skyNodeAtIndex(i));

        KSComet *com = static_cast<KSComet *>(skyNode->getSkyObject());
        double mag = com->mag();
        if (std::isnan(mag) == 0)
        {
            skyNode->update();
            //bool drawn = skyp->drawPointSource(com,mag);
            //if ( drawn && !(hideLabels || com->rsun() >= rsunLabelLimit) )
            //  SkyLabeler::AddLabel( com, SkyLabeler::COMET_LABEL );
        } else {
            skyNode->hide();
        }
    }
    return n;
}
