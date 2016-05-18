/** *************************************************************************
                          planetsitem.cpp  -  K Desktop Planetarium
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

#include "planetsitem.h"
#include "projections/projector.h"
#include "solarsystemsinglecomponent.h"
#include "ksplanet.h"

#include "nodes/planetnode.h"
#include "nodes/rootnode.h"

#include "Options.h"

PlanetsItem::PlanetsItem(QQuickItem* parent)
    :SkyItem(parent)
{

}

void PlanetsItem::addPlanet(SolarSystemSingleComponent* planetComp) {
    if(!m_planetComponents.contains(planetComp) && !m_toAdd.contains(planetComp))
        m_toAdd.append(planetComp);
}

SolarSystemSingleComponent * PlanetsItem::getParentComponent(SkyObject * planet) {
    foreach(SolarSystemSingleComponent * planetComp, m_planetComponents) {
        if(planetComp->planet() == planet) return planetComp;
    }
    return nullptr;
}

QSGNode* PlanetsItem::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *updatePaintNodeData) {
    RootNode *n = static_cast<RootNode*>(oldNode);
    Q_UNUSED(updatePaintNodeData);
    QRectF rect = boundingRect();

    if (rect.isEmpty()) {
        delete n;
        return 0;
    }

    if(!n) {
        n = new RootNode; // If no RootNode exists create one
        int pCompLen = m_planetComponents.length();
        if(pCompLen > 0) {
            /* If there are some planets that have been already displayed once then recreate them
                 in new instance of PlanetRootNode*/
            for(int i = 0; i < pCompLen; ++i) {
                n->appendSkyNode(new PlanetNode(m_planetComponents[i]->planet(), n));
            }
        }
    }

    int addLength = m_toAdd.length();
    if(addLength > 0) { // If there are some new planets to add
        for(int i = 0; i < addLength; ++i) {
            m_planetComponents.append(m_toAdd[i]);
            n->appendSkyNode(new PlanetNode(m_toAdd[i]->planet(), n));
        }
        m_toAdd.clear();
    }
    //Update clipping geometry. If m_clipPoly in SkyMapLite wasn't changed, geometry is not updated
    n->updateClipPoly();
    //Traverse all children nodes of RootNode
    for(int i = 0; i < n->skyNodesCount(); ++i) {
        PlanetNode* pNode = static_cast<PlanetNode*>(n->skyNodeAtIndex(i));
        bool selected = getParentComponent(pNode->getSkyObject())->selected();
        if(selected) pNode->update();
        else pNode->hide();
    }
    return n;
}
