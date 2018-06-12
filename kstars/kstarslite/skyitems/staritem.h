/** *************************************************************************
                          staritem.h  -  K Desktop Planetarium
                             -------------------
    begin                : 15/06/2016
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

#pragma once

#include "skyitem.h"

class SkyMesh;
class SkyOpacityNode;
class StarBlockFactory;
class StarComponent;

/**
 * @class StarItem
 *
 * @short Class that handles Stars
 * @author Artem Fedoskin
 * @version 1.0
 */
class StarItem : public SkyItem
{
  public:
    /**
     * @short Constructor.
     * @param rootNode parent RootNode that instantiated this object
     */
    StarItem(StarComponent *starComp, RootNode *rootNode);

    /**
     * @short Update positions of nodes that represent stars
     * In this function we perform almost the same thing as in DeepSkyItem::updateDeepSkyNode() to reduce
     * memory consumption.
     * @see DeepSkyItem::updateDeepSkyNode()
     */
    virtual void update();

  private:
    StarComponent *m_starComp { nullptr };
    SkyMesh *m_skyMesh { nullptr };
    StarBlockFactory *m_StarBlockFactory { nullptr };

    SkyOpacityNode *m_stars { nullptr };
    SkyOpacityNode *m_deepStars { nullptr };
    SkyOpacityNode *m_starLabels { nullptr };
};
