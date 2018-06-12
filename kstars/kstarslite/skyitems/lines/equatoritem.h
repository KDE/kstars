/** *************************************************************************
                          equatoritem.h  -  K Desktop Planetarium
                             -------------------
    begin                : 10/06/2016
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
