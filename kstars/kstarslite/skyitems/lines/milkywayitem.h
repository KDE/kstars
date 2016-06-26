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
#include "../typedeflite.h"

class LineListIndex;
class MilkyWay;

    /** @class MilkyWay
     *
     * Class that handles lines (Constellation lines and boundaries and both coordinate grids) in
     * SkyMapLite.
     *
     * To display lines component use addLinesComponent.
     *
     *@note see RootNode::RootNode() for example of adding lines
     *@short Class that handles most of the lines in SkyMapLite
     *@author Artem Fedoskin
     *@version 1.0
     */

class MilkyWayItem : public SkyItem {
public:
    /**
     * @short Constructor.
     * @param rootNode parent RootNode that instantiated this object
     */
    MilkyWayItem(MilkyWay *mwComp, RootNode *rootNode);

    void initialize();

    /**
     * @short updates all trixels that are associated with LineListIndex or hide them if selected()
     * of this LineListIndex returns false
     */

    virtual void update();
private:
    bool m_filled; //True if the polygon has to be filled
    MilkyWay *m_MWComp;
};
#endif

