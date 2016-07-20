/** *************************************************************************
                          telescopesymbolsitem.h  -  K Desktop Planetarium
                             -------------------
    begin                : 17/07/2016
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
#ifndef TELESCOPESYMBOLSITEM_H_
#define TELESCOPESYMBOLSITEM_H_

#include "skyitem.h"
#include "basedevice.h"

class KSAsteroid;
class SkyObject;
class RootNode;
class EllipseNode;
class LineNode;
class CrosshairNode;
class ClientManagerLite;

    /**
     * @class TelescopeSymbolsItem
     * This class handles telescope symbols in SkyMapLite
     *
     * @author Artem Fedoskin
     * @version 1.0
     */

class TelescopeSymbolsItem : public SkyItem {
public:
    /**
     * @short Constructor
     * @param asteroidsList const reference to list of asteroids
     * @param rootNode parent RootNode that instantiates PlanetsItem
     */
    TelescopeSymbolsItem(RootNode *rootNode);

    virtual void update() override;
    void addTelescope(INDI::BaseDevice *bd);
    void removeTelescope(QString deviceName);

private:
    QHash<INDI::BaseDevice *, CrosshairNode *> m_telescopes;
    ClientManagerLite *m_clientManager;
    QColor m_color;
    KStarsData *m_KStarsData;
};
#endif
