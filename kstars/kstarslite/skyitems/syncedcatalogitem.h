/** *************************************************************************
                          syncedcatalogitem.h  -  K Desktop Planetarium
                             -------------------
    begin                : 03/09/2016
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
#ifndef SYNCEDCATALOGITEM_H_
#define SYNCEDCATALOGITEM_H_

    /**
    * @class SyncedCatalogItem
    * This class handles representation of objects from SyncedCatalogComponent in SkyMapLite
    *
    * @author Artem Fedoskin
    * @version 1.0
    */

#include "skyitem.h"

class SkyObject;
class SyncedCatalogComponent;

class SyncedCatalogItem : public SkyItem {
public:
    /**
     * @short Constructor
     * @param parent - pointer to SyncedCatalogComponent that handles data
     * @param rootNode parent RootNode that instantiates this object
     */
    SyncedCatalogItem(SyncedCatalogComponent *parent, RootNode *rootNode);

    /**
     * @short Update positions and visibility of objects from this catalog.
     * To check whether we need to create or delete nodes we compare m_ObjectList and
     * SyncedCatalogComponent::objectList(). If objectList was changed we recreate the whole node tree to sync
     * it with objectList.
     */
    virtual void update() override;

private:
    SyncedCatalogComponent *m_parent;
    QList<SkyObject *> m_ObjectList;

    QSGNode *stars;
    QSGNode *dsoSymbols;
    QSGNode *dsoNodes;
};
#endif
