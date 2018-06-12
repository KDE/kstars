/** *************************************************************************
                          deepstaritem.h  -  K Desktop Planetarium
                             -------------------
    begin                : 17/06/2016
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

#include <memory>

class DeepStarComponent;
class SkyMesh;
class StarBlockFactory;
class StarBlockList;

/**
 * @class DeepStarItem
 *
 * @short This class handles representation of unnamed stars in SkyMapLite
 *
 * @author Artem Fedoskin
 * @version 1.0
 */
class DeepStarItem : public SkyItem
{
  public:
    /**
     * @short Constructor. Instantiates nodes for static stars
     * @param deepStarComp - pointer to DeepStarComponent that handles data
     * @param rootNode - parent RootNode that instantiated this object
     */
    DeepStarItem(DeepStarComponent *deepStarComp, RootNode *rootNode);

    /**
     * @short updates all trixels that contain stars
     */
    virtual void update();

  private:
    SkyMesh *m_skyMesh { nullptr };
    StarBlockFactory *m_StarBlockFactory { nullptr };

    DeepStarComponent *m_deepStarComp { nullptr };
    QVector<std::shared_ptr<StarBlockList>> *m_starBlockList { nullptr };
    bool m_staticStars { false };
};
