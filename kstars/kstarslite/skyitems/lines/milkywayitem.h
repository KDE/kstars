/*
    SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "../skyitem.h"
#include "../skyopacitynode.h"

class LineListIndex;
class MilkyWay;

/** @class MilkyWay
 *
 * @short Class that handles drawing of MilkyWay (both filled and non-filled)
 * @author Artem Fedoskin
 * @version 1.0
 */

class MilkyWayItem : public SkyItem
{
  public:
    /**
     * @short Constructor.
     * @param mwComp - pointer to MilkyWay that handles data
     * @param rootNode - parent RootNode that instantiated this object
     */
    MilkyWayItem(MilkyWay *mwComp, RootNode *rootNode);

    /**
     * @short If m_filled is true SkyPolygonNodes(filled) will be initialized. Otherwise MilkyWay will be
     * drawn with LineNodes(non-filled)
     */
    void initialize();

    /**
     * @short Update position of all nodes that represent MilkyWay
     * If m_filled is not equal to Options::fillMilkyWay() we reinitialize all nodes by calling initialize()
     */
    virtual void update();

  private:
    bool m_filled { false }; //True if the polygon has to be filled
    MilkyWay *m_MWComp { nullptr };
};
