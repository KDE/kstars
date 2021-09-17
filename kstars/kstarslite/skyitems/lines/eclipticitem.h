/*
    SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "../skyitem.h"

class Ecliptic;

/**
 * @class EclipticItem
 *
 * @short Represents Ecliptic in SkyMapLite
 * @author Artem Fedoskin
 * @version 1.0
 */
class EclipticItem : public SkyItem
{
  public:
    /**
     * @short Constructor. Creates TrixelNodes for lines and LabelNodes for compass labels
     * @param eclipticComp Ecliptic that needs to be represented in SkyMapLite
     * @param rootNode parent RootNode that instantiated this object
     */
    EclipticItem(Ecliptic *eclipticComp, RootNode *rootNode);

    /** @short updates positions of lines and compass labels */
    virtual void update() override;

  private:
    Ecliptic *m_eclipticComp { nullptr };

    //Holds compass labels each associated with SkyPoint that is coordinate of this label
    QMap<SkyPoint *, LabelNode *> m_compassLabels;
};
