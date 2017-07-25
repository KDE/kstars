/** *************************************************************************
                          cometsitem.h  -  K Desktop Planetarium
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
