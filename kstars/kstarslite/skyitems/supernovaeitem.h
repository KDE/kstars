/** *************************************************************************
                          supernovaeitem.h  -  K Desktop Planetarium
                             -------------------
    begin                : 26/06/2016
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
#ifndef SUPERNOVAEITEM_H_
#define SUPERNOVAEITEM_H_

#include "skyitem.h"

class KSComet;
class SkyObject;
class SupernovaeComponent;

    /**
     * @class SupernovaItem
     * This class handles comets in SkyMapLite
     *
     * @author Artem Fedoskin
     * @version 1.0
     */

class SupernovaeItem : public SkyItem {
public:
    /**
     * @short Constructor
     * @param cometsList const reference to list of comets
     * @param rootNode parent RootNode that instantiates this object
     */
    SupernovaeItem(SupernovaeComponent *snovaComp, RootNode *rootNode = 0);

    /**
     * @short recreates the node tree (deletes old nodes and appends new ones according to
     * m_cometsList)
     */
    void recreateList();

    /**
     * @short Determines the visibility of the object and its label and hides/updates them accordingly
     */
    virtual void update() override;

private:
    SupernovaeComponent *m_snovaComp;
};
#endif
