/*
    SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

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
     * @param starComp star component
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
