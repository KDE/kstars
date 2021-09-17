/*
    SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "skyitem.h"

class HorizonComponent;
class GuideLabelNode;

/**
 * @class HorizonItem
 *
 * Handles representation of HorizonComponent in SkyMapLite (lines, filled polygon and compass
 * labels).
 *
 * @author Artem Fedoskin
 * @version 1.0
 */
class HorizonItem : public SkyItem
{
  public:
    /**
     * @short Constructor.
     * @param hComp pointer to HorizonComponent which HorizonItem represents in SkyMapLite
     * @param rootNode parent RootNode that instantiated this object
     */
    HorizonItem(HorizonComponent *hComp, RootNode *rootNode);

    /**
     * @short setter for m_horizonComp
     * @param hComp pointer to HorizonComponent
     */
    inline void setHorizonComp(HorizonComponent *hComp) { m_horizonComp = hComp; }

    /**
     * @short Call update() of HorizonNode and update/hide compass labels based on their visibility
     */
    virtual void update();

  private:
    HorizonComponent *m_horizonComp { nullptr };
    QMap<SkyPoint *, LabelNode *> m_compassLabels;
};
