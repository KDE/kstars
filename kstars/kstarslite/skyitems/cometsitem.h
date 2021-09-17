/*
    SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "skyitem.h"

class SkyObject;

/**
 * @class CometsItem
 * This class handles comets in SkyMapLite
 *
 * @author Artem Fedoskin
 * @version 1.0
 */
class CometsItem : public SkyItem
{
  public:
    /**
     * @short Constructor
     * @param cometsList const reference to list of comets
     * @param rootNode parent RootNode that instantiates this object
     */
    explicit CometsItem(const QList<SkyObject *> &cometsList, RootNode *rootNode = nullptr);

    /**
     * @short recreates the node tree (deletes old nodes and appends new ones according to
     * m_cometsList)
     */
    void recreateList();

    /**
     * @short Determines the visibility of the object and its label and hides/updates them accordingly
     */
    virtual void update() override;

  private:
    const QList<SkyObject *> &m_cometsList;
};
