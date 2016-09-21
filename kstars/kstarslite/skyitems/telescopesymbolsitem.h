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
     * This class handles representation of telescope symbols in SkyMapLite
     *
     * @author Artem Fedoskin
     * @version 1.0
     */

class TelescopeSymbolsItem : public SkyItem {
public:
    /**
     * @short Constructor
     * @param rootNode parent RootNode that instantiates PlanetsItem
     */
    TelescopeSymbolsItem(RootNode *rootNode);

    /**
     * @short Updates position and visibility of CrosshairNodes that represent telescope symbols
     * If client is no more connected to host or device CrosshairNode is deleted.
     */
    virtual void update() override;

    /**
     * @short Add telescope symbol for device bd
     */
    void addTelescope(INDI::BaseDevice *bd);

    /**
     * @short Remove telescope symbol of device bd
     */
    void removeTelescope(INDI::BaseDevice *bd);

private:
    QHash<INDI::BaseDevice *, CrosshairNode *> m_telescopes;
    ClientManagerLite *m_clientManager;
    QColor m_color;
    KStarsData *m_KStarsData;
};
#endif
