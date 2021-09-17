/*
    SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "skyitem.h"

class RootNode;
class SkyObject;

/**
 * @class AsteroidsItem
 * This class handles asteroids in SkyMapLite
 *
 * @author Artem Fedoskin
 * @version 1.0
 */
class AsteroidsItem : public SkyItem
{
  public:
    /**
     * @short Constructor
     * @param asteroidsList const reference to list of asteroids
     * @param rootNode parent RootNode that instantiates PlanetsItem
     */
    explicit AsteroidsItem(const QList<SkyObject *> &asteroidsList, RootNode *rootNode = nullptr);

    /**
     * @short recreates the node tree (deletes old nodes and appends new ones based on SkyObjects in
     * m_asteroidsList)
     */
    void recreateList();

    /** Determines the visibility of the object and its label and hides/updates them accordingly */
    virtual void update() override;

  private:
    const QList<SkyObject *> &m_asteroidsList;
};
