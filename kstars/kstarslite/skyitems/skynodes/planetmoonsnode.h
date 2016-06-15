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
 * PlanetMoons derived from SkyComponent PlanetMoonsNode represents both planet and moons. Ths PlanetNode
 * shouldn't be instantiated outside of this class (exception is AsteroidsItem). Although all SkyNodes
 * are "movable" objects (they change transform matrix to move across the SkyMapLite) this class is
 * just a container that provides z-order for moons and planets that change their positions on their own.
 *
 *@short A container for planets and moons that provides z-order.
 *@author Artem Fedoskin
 *@version 1.0
 */

class PlanetMoonsNode : public SkyNode {
public:
    /**
     * @short Constructor
     * @param planet pointer to planet object
     * @param parentNode pointer to the RootNode. It is needed for PointSourceNodes that use textures,
     * which are cached in RootNode.
     */
    PlanetMoonsNode(KSPlanetBase* planet, RootNode* parentNode);

    /**
     * @short Add object of type PlanetMoons to this node
     * @param planetMoons PlanetMoons component
     */
    inline void addMoons(PlanetMoons * planetMoons) { pmoons = planetMoons; }

    /**
     * If planet has any moons first updateMoons() is called then the planet is updated
     */
    virtual void update() override;

    /**
     * @short Hides both planet and its moons
     */
    virtual void hide() override;
    /**
     * @note PlanetMoonsNode is not meant to be moved. PlanetNode and PointSourceNodes handle this on
     * their own.
     */
    virtual void changePos(QPointF pos) { }

    /**
     * Update position of moons if planet has them. To allow z-ordering we need to change the structure
     * of node tree by removing all child nodes of this tree and adding them again so that moons that
     * are behind the planet are before the m_planetNode in the hierarchy and all others are appended
     * after m_planetNode.
     */
    void updateMoons();

private:
    PlanetNode *m_planetNode;
    PlanetMoons *pmoons;

    QList<SkyNode *> m_moonNodes;
    RootNode *m_rootNode;

};


#endif

