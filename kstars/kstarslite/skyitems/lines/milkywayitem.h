/** *************************************************************************
                          milkywayitem.h  -  K Desktop Planetarium
                             -------------------
    begin                : 23/06/2016
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
#ifndef MILKYWAYITEM_H_
#define MILKYWAYITEM_H_

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

class MilkyWayItem : public SkyItem {
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
    bool m_filled; //True if the polygon has to be filled
    MilkyWay *m_MWComp;
};
#endif

