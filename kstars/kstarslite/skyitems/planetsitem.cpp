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
#include "Options.h"

PlanetsItem::PlanetsItem(QList<SolarSystemSingleComponent *> planets, QList<PlanetMoonsComponent *> moons, RootNode *rootNode)
    :SkyItem(rootNode), m_planetComponents(planets), m_moonsComponents(moons)
{
    foreach(SolarSystemSingleComponent * planetComp, m_planetComponents) {
        KSPlanetBase *planet = planetComp->planet();
        PlanetMoonsNode *pNode = new PlanetMoonsNode(planet, rootNode);
        appendChildNode(pNode);

        foreach(PlanetMoonsComponent * moonsComp, m_moonsComponents) {
            // Find planet of moons
            if(planet == moonsComp->getPlanet()) {
                pNode->addMoons(moonsComp->getMoons());
            }
        }
    }
}

SolarSystemSingleComponent * PlanetsItem::getParentComponent(SkyObject * planet) {
    foreach(SolarSystemSingleComponent * planetComp, m_planetComponents) {
        if(planetComp->planet() == planet) return planetComp;
    }
    return nullptr;
}

void PlanetsItem::update() {
    show();
    //Traverse all children nodes of RootNode
    QSGNode *n = firstChild();
    while(n != 0) {
        PlanetMoonsNode *pNode = static_cast<PlanetMoonsNode *>(n);
        n = n->nextSibling();

        bool selected = getParentComponent(pNode->skyObject())->selected();
        if(selected) pNode->update();
        else pNode->hide();
    }
}
