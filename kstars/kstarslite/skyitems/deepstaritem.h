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
#ifndef DEEPSTARITEM_H_
#define DEEPSTARITEM_H_

#include "skyitem.h"
#include "skyopacitynode.h"

    /** @class DeepStarItem
     *
     *@short This class handles representation of unnamed stars in SkyMapLite
     *@author Artem Fedoskin
     *@version 1.0
     */

class DeepStarComponent;
class SkyMesh;
class StarBlockFactory;
class StarBlockList;

class DeepStarItem : public SkyItem {
public:
    /**
     * @short Constructor. Instantiates nodes for static stars
     * @param deepStarComp - pointer to DeepStarComponent that handles data
     * @param rootNode - parent RootNode that instantiated this object
     */
    DeepStarItem(DeepStarComponent *deepStarComp, RootNode *rootNode);

    /**
     * @short Update positions of deep stars in SkyMapLite
     * In this function we perform almost the same thing as in DeepSkyItem::updateDeepSkyNode() to reduce
     * memory consumption.
     * @see DeepSkyItem::updateDeepSkyNode()
     */
    /**
     * @short updates all trixels that contain stars
     */
    virtual void update();

private:
    SkyMesh *m_skyMesh;
    StarBlockFactory *m_StarBlockFactory;

    DeepStarComponent *m_deepStarComp;
    QVector< StarBlockList *> *m_starBlockList;
    bool m_staticStars;
};
#endif

