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
#ifndef CONSTELLATIONARTITEM_H_
#define CONSTELLATIONARTITEM_H_

#include "skyitem.h"

class RootNode;
class ConstellationArtComponent;
    /**
     * @class PlanetsItem
     * This class handles planets and their moons in SkyMapLite
     *
     * @author Artem Fedoskin
     * @version 1.0
     */

class ConstellationArtItem : public SkyItem {
public:
    /**
     * @short Constructor. Takes lists of pointers to planets(SolarSystemSingleComponent) and their
     * moons (PlanetMoonsComponent) to instantiate PlanetMoonsNodes for each of the planet.
     *
     * @param planets list of pointers to planets
     * @param moons list of pointers to moons
     * @param rootNode parent RootNode that instantiates this object
     */
    ConstellationArtItem(ConstellationArtComponent *artComp,RootNode *rootNode = 0);

    /**
     * @short calls update() of all child PlanetMoonsNodes
     * @note see PlanetMoonsNodes::update()
     */
    void update() override;

private:
    ConstellationArtComponent *m_artComp;
};
#endif
