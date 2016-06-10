/** *************************************************************************
                          constellationnamesitem.h  -  K Desktop Planetarium
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

#ifndef CONSTELLATIONNAMESITEM_H_
#define CONSTELLATIONNAMESITEM_H_

#include <QSGOpacityNode>
#include "skyitem.h"

class ConstellationNamesComponent;

/** @class SkyItem
 *
 *This is an interface for implementing SkyItems that are used to display SkyComponent
 *derived objects on the SkyMapLite.
 *
 *@short A base class that is used for displaying SkyComponents on SkyMapLite.
 *@author Artem Fedoskin
 *@version 1.0
 */

class ConstellationNamesItem : public SkyItem {

public:
   /**
    *Constructor, add SkyItem to parent in a node tree
    *
    * @param parent a pointer to SkyItem's parent node
    */

    explicit ConstellationNamesItem(const QList<SkyObject*>& namesList, RootNode *rootNode = 0);

    virtual void update();

    void hide();
    void show();
    void recreateList();
private:
    const QList<SkyObject*>& m_namesList;
};

#endif
