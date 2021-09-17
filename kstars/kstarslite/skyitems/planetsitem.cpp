/*
    SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "planetsitem.h"
#include "projections/projector.h"
#include "solarsystemsinglecomponent.h"
#include "ksplanet.h"
#include "planetmoonscomponent.h"

#include "skynodes/planetmoonsnode.h"
#include "skynodes/planetnode.h"
#include "Options.h"

PlanetsItem::PlanetsItem(QList<SolarSystemSingleComponent *> planets,
                         /* QList<PlanetMoonsComponent *> moons, */ RootNode *rootNode)
    : SkyItem(LabelsItem::label_t::PLANET_LABEL, rootNode), m_planetComponents(planets) /*, m_moonsComponents(moons)*/
{
    foreach (SolarSystemSingleComponent *planetComp, m_planetComponents)
    {
        KSPlanetBase *planet   = planetComp->planet();
        PlanetMoonsNode *pNode = new PlanetMoonsNode(planet, rootNode);
        appendChildNode(pNode);

        //        foreach(PlanetMoonsComponent * moonsComp, m_moonsComponents) {
        //            // Find planet of moons
        //            if(planet == moonsComp->getPlanet()) {
        //                pNode->addMoons(moonsComp->getMoons());
        //            }
        //        }
    }
}

SolarSystemSingleComponent *PlanetsItem::getParentComponent(SkyObject *planet)
{
    foreach (SolarSystemSingleComponent *planetComp, m_planetComponents)
    {
        if (planetComp->planet() == planet)
            return planetComp;
    }
    return nullptr;
}

void PlanetsItem::update()
{
    show();
    //Traverse all children nodes of RootNode
    QSGNode *n = firstChild();
    while (n != 0)
    {
        PlanetMoonsNode *pNode = static_cast<PlanetMoonsNode *>(n);
        n                      = n->nextSibling();

        bool selected = getParentComponent(pNode->skyObject())->selected();
        if (selected)
            pNode->update();
        else
            pNode->hide();
    }
}

void PlanetsItem::show()
{
    rootNode()->labelsItem()->getLabelNode(LabelsItem::label_t::JUPITER_MOON_LABEL)->show();
    SkyItem::show();
}

void PlanetsItem::hide()
{
    rootNode()->labelsItem()->getLabelNode(LabelsItem::label_t::JUPITER_MOON_LABEL)->hide();
    SkyItem::hide();
}
