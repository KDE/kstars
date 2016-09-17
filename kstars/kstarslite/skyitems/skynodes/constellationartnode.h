/** *************************************************************************
                          constellationartnode.h  -  K Desktop Planetarium
                             -------------------
    begin                : 02/06/2016
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
#ifndef CONSTELLATIONARTNODE_H
#define CONSTELLATIONARTNODE_H
#include "skynode.h"
#include "../labelsitem.h"

class PlanetItemNode;
class SkyMapLite;
class PointNode;
class LabelNode;
class ConstellationsArt;
class QSGSimpleTextureNode;

/** @class ConstellationArtNode
 *
 *@short A SkyNode derived class that represents ConstellationsArt object.
 *@author Artem Fedoskin
 *@version 1.0
 */

class RootNode;

class ConstellationArtNode : public SkyNode  {
public:
    /**
     * @short Constructor
     * @param obj - a pointer to ConstellationsArt object that is represented by this node
     */
    ConstellationArtNode(ConstellationsArt *obj);
    /**
     * @short changePos change the position of this node
     * @param pos - new position
     * @param positionangle - an angle of ConstellationsArt image rotation
     */
    void changePos(QPointF pos, double positionangle);

    virtual void update() override;
    virtual void hide() override;
private:
    RootNode *m_rootNode;
    ConstellationsArt *m_art;
    QSGSimpleTextureNode *m_texture;
};

#endif

