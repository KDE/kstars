/*
    SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "skyitem.h"

class RootNode;
class ConstellationArtComponent;

/**
 * @class ConstellationArtItem
 * This class handles constellation art in SkyMapLite. Each constellation image is represented by ConstellationArtNode.
 * @see ConstellationArtNode
 *
 * @author Artem Fedoskin
 * @version 1.0
 */

class ConstellationArtItem : public SkyItem
{
  public:
    /**
     * @param artComp - pointer to ConstellationArtComponent instance, that handles constellation art data
     * @param rootNode - pointer to the root node
     */
    explicit ConstellationArtItem(ConstellationArtComponent *artComp, RootNode *rootNode = nullptr);

    /**
     * @short calls update() of all child ConstellationArtNodes if constellation art is on. Otherwise
     * calls deleteNodes().
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
    ConstellationArtComponent *m_artComp { nullptr };
};
