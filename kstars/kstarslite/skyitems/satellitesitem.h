/*
    SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "skyitem.h"

class KSComet;
class SkyObject;
class SatellitesComponent;

/**
 * @class SatellitesItem
 * This class handles representation of satellites in SkyMapLite
 *
 * @author Artem Fedoskin
 * @version 1.0
 */

class SatellitesItem : public SkyItem
{
  public:
    /**
     * @short Constructor
     * @param satComp - pointer to SatellitesComponent that handles data
     * @param rootNode parent RootNode that instantiates this object
     */
    explicit SatellitesItem(SatellitesComponent *satComp, RootNode *rootNode = nullptr);

    /**
     * @short recreates the node tree (deletes old nodes and appends new ones according to
     * SatelliteGroups from SatellitesComponent::groups())
     */
    void recreateList();

    /** Update positions and visibility of satellites */
    virtual void update() override;

  private:
    SatellitesComponent *m_satComp { nullptr };
};
