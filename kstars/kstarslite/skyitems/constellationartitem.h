/** *************************************************************************
                          constellationartitem.h  -  K Desktop Planetarium
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
#ifndef CONSTELLATIONARTITEM_H_
#define CONSTELLATIONARTITEM_H_

#include "skyitem.h"

class RootNode;
class ConstellationArtComponent;
    /**
     * @class ConstellationArtItem
     * This class handles constellation art in SkyMapLite. Each constellation image is represented by ConstellationArtNode.
     * @see ConstellationArtNode
     * @author Artem Fedoskin
     * @version 1.0
     */

class ConstellationArtItem : public SkyItem {
public:
    /**
     * @param artComp - pointer to ConstellationArtComponent instance, that handles constellation art data
     * @param rootNode - pointer to the root node
     */
    ConstellationArtItem(ConstellationArtComponent *artComp,RootNode *rootNode = 0);

    /**
     * @short calls update() of all child ConstellationArtNodes if constellation art is on. Otherwise calls deleteNodes()
     */
    void update() override;

    /**
     * @short deleteNodes deletes constellation art data and ConstellationArtNodes
     * @see ConstellationArtComponent::deleteData()
     */
    void deleteNodes();

    /**
     * @short loadNodes loads constellation art data and creates ConstellationArtNodes
     * @see ConstellationArtComponent::loadData()
     */
    void loadNodes();

private:
    ConstellationArtComponent *m_artComp;
};
#endif
