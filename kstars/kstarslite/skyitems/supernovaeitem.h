/*
    SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "skyitem.h"

class SupernovaeComponent;

/**
 * @class SupernovaeItem
 * This class handles supernovae in SkyMapLite
 *
 * @author Artem Fedoskin
 * @version 1.0
 */
class SupernovaeItem : public SkyItem
{
  public:
    /**
     * @short Constructor
     * @param snovaComp - pointer to SupernovaeComponent that handles data
     * @param rootNode parent RootNode that instantiates this object
     */
    explicit SupernovaeItem(SupernovaeComponent *snovaComp, RootNode *rootNode = nullptr);

    /**
     * @short Recreate the node tree (delete old nodes and append new ones according to
     * SupernovaeItem::objectList())
     */
    void recreateList();

    /** Update positions and visibility of supernovae */
    virtual void update() override;

  private:
    SupernovaeComponent *m_snovaComp { nullptr };
};
