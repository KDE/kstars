/** *************************************************************************
                          pointsourcenode.h  -  K Desktop Planetarium
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
#ifndef POINTSOURCENODE_H_
#define POINTSOURCENODE_H_
#include "skynode.h"
#include "../labelsitem.h"

class PlanetItemNode;
class SkyMapLite;
class PointNode;
class LabelNode;
class ConstellationsArt;
class QSGSimpleTextureNode;

/** @class PointSourceNode
 *
 * A SkyNode derived class used for displaying PointNode with coordinates provided by SkyObject.
 *
 *@short A SkyNode derived class that represents stars and objects that are drawn as stars
 *@author Artem Fedoskin
 *@version 1.0
 */

class RootNode;

class ConstellationArtNode : public SkyNode  {
public:
    /**
     * @short Constructor
     * @param skyObject pointer to SkyObject that has to be displayed on SkyMapLite
     * @param parentNode pointer to the top parent node, which holds texture cache
     * @param spType spectral class of PointNode
     * @param size initial size of PointNode
     */
    ConstellationArtNode(ConstellationsArt *obj);
    /**
     * @short changePos changes the position m_point
     * @param pos new position
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

