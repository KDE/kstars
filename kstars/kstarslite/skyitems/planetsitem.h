/** *************************************************************************
                          planetsitem.h  -  K Desktop Planetarium
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
#ifndef PLANETSITEM_H_
#define PLANETSITEM_H_

#include "skyitem.h"

class SolarSystemSingleComponent;
class PlanetMoonsComponent;
class SkyObject;

class PlanetsItem : public SkyItem {
public:
    PlanetsItem(QQuickItem* parent = 0);
    /** Add a pointer to an object of type SolarSystemSingleComponent to m_toAdd. In the next call to
     * updatePaintNode() the object of type PlanetNode will be created and planetComponent
     * will be moved to m_planetComponents. PlanetNode represents graphically KSPlanetBase on SkyMapLite.
     * This function should be called whenever an object of class SolarSystemSingleComponent is
     * created.
     *
     * @param SolarSystemSingleComponent that should be displayed on SkyMapLite
     */

    void addPlanet(SolarSystemSingleComponent* planetComp);

    /**
     * Add a pointer to an object of type PlanetMoonsComponent to m_moonsToAdd. In the next call to
     * updatePaintNode() PlanetMoons will be added to existing PlanetMoonsNode, which will then control
     * the drawing of moons and planet.
     * @param pMoons pointer to PlanetMoonsComponent which moons should be displayed on SkyMapLite
     */
    void addMoons(PlanetMoonsComponent * pMoons);

    SolarSystemSingleComponent * getParentComponent(SkyObject * planet);

protected:
    virtual QSGNode* updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *updatePaintNodeData) override;
private:
    QList<SolarSystemSingleComponent *> m_planetComponents;
    QList<SolarSystemSingleComponent *> m_planetsToAdd;

    QList<PlanetMoonsComponent *> m_moonsComponents;
    QList<PlanetMoonsComponent *> m_moonsToAdd;
};
#endif
