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
#include "planetmoonscomponent.h"

#include "skynodes/planetmoonsnode.h"
#include "skynodes/planetnode.h"
#include "skynodes/rootnodes/rootnode.h"

#include "Options.h"

PlanetsItem::PlanetsItem(QQuickItem* parent)
    :SkyItem(parent)
{

}

void PlanetsItem::addPlanet(SolarSystemSingleComponent* planetComp) {
    if(!m_planetComponents.contains(planetComp) && !m_planetsToAdd.contains(planetComp)) {
        m_planetsToAdd.append(planetComp);
    }
}

void PlanetsItem::addMoons(PlanetMoonsComponent * moonsComponent) {
    if(!m_moonsComponents.contains(moonsComponent) && !m_moonsToAdd.contains(moonsComponent)) {
        m_moonsToAdd.append(moonsComponent);
    }
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
                 in the new instance of RootNode*/
            int mCompLen = m_moonsComponents.length();
            foreach(SolarSystemSingleComponent * planetComp, m_planetComponents) {
                KSPlanetBase *planet = planetComp->planet();
                PlanetMoonsNode *pNode = new PlanetMoonsNode(planet, n);
                n->appendSkyNode(pNode);
                /* If there are some moons that have been already displayed once then recreate them
                     in the new instance of RootNode*/
                if(mCompLen > 0) {
                    foreach(PlanetMoonsComponent * moonsComp, m_moonsComponents) {
                        // Find planet of moons
                        if(planet == moonsComp->getPlanet()) {
                            pNode->addMoons(moonsComp->getMoons());
                        }
                    }
                }
            }
        }
    }

    int addPlanetLen = m_planetsToAdd.length();
    if(addPlanetLen > 0) { // If there are some new planets to add
        foreach(SolarSystemSingleComponent * planetComp, m_planetsToAdd) {
            m_planetComponents.append(planetComp);

            KSPlanetBase *planet = planetComp->planet();
            PlanetMoonsNode *pNode = new PlanetMoonsNode(planet, n);
            n->appendSkyNode(pNode);
        }
        m_planetsToAdd.clear();
    }

    int addMoonLen = m_moonsToAdd.length();

    //Update clipping geometry. If m_clipPoly in SkyMapLite wasn't changed, geometry is not updated
    n->updateClipPoly();
    //Traverse all children nodes of RootNode
    for(int i = 0; i < n->skyNodesCount(); ++i) {
        PlanetMoonsNode *pNode = static_cast<PlanetMoonsNode *>(n->skyNodeAtIndex(i));

        SkyObject * planet = pNode->getSkyObject();

        if(addMoonLen > 0) { // If there are moons to add
            foreach(PlanetMoonsComponent * moonsComp, m_moonsToAdd) {
                if(planet == moonsComp->getPlanet()) {
                    pNode->addMoons(moonsComp->getMoons());

                    m_moonsComponents.append(moonsComp);
                    m_moonsToAdd.removeOne(moonsComp);
                }
            }
        }

        bool selected = getParentComponent(pNode->getSkyObject())->selected();
        if(selected) pNode->update();
        else pNode->hide();
    }
    return n;
}
