/** *************************************************************************
                          planetmoonsnode.h  -  K Desktop Planetarium
                             -------------------
    begin                : 24/05/2016
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
#ifndef PLANETMOONSNODE_H_
#define PLANETMOONSNODE_H_
#include "skynode.h"

class PlanetNode;
class PlanetMoons;
class RootNode;
class PointSourceNode;
class KSPlanetBase;
class QSGSimpleTextureNode;

/** @class PlanetMoonsNode
 *
 * A SkyNode derived class used as a container for displaying a planet with its moons (if any). Unlike
 * PlanetMoons in SkyComponents PlanetMoonsNode represents both planet and moons. Thus the planet has to
 * be created only using this class that in turn will create an object of type PlanetNode. Although all
 * SkyNodes are "movable" objects this class is just a container that provides z-order for moons and
 * planets, which changes their positions on their own.
 *
 *@short A container for planets and moons that provides z-order.
 *@author Artem Fedoskin
 *@version 1.0
 */

class PlanetMoonsNode : public SkyNode {
public:
    PlanetMoonsNode(KSPlanetBase* planet, RootNode* parentNode);

    /**
     * @short Add object of type PlanetMoons to this node
     * @param planetMoons PlanetMoons component
     */
    inline void addMoons(PlanetMoons * planetMoons) { pmoons = planetMoons; }

    virtual void update() override;
    virtual void hide() override;
    /**
     * @note PlanetMoonsNode is not meant to be moved. PlanetNode and PointSourceNodes handle this on
     * their own.
     */
    virtual void changePos(QPointF pos) { }

    /**
     * @short update position of moons if planet has them
     */
    void updateMoons();

private:
    PlanetNode *m_planetNode;
    PlanetMoons *pmoons;

    QList<PointSourceNode *> m_moonNodes;
    RootNode *m_rootNode;

};


#endif

