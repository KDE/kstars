/*
    SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "equator.h"
#include "../skyitem.h"

class KSAsteroid;
class LineListIndex;

/**
 * @class EquatorItem
 *
 * @short Represents Equator in SkyMapLite
 *
 * @author Artem Fedoskin
 * @version 1.0
 */
class EquatorItem : public SkyItem
{
  public:
    /**
     * @short Constructor. Creates TrixelNodes for lines and LabelNodes for compass labels
     * @param equatorComp Equator that needs to be represented in SkyMapLite
     * @param rootNode parent RootNode that instantiated this object
     */

    EquatorItem(Equator *equatorComp, RootNode *rootNode);

    /**
     * @short updates positions of lines and compass labels
     */
    virtual void update();

  private:
    Equator *m_equatorComp { nullptr };
    //Holds compass labels each associated with SkyPoint that is coordinate of this label
    QMap<SkyPoint *, LabelNode *> m_compassLabels;
};
