/*
    SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "skyitem.h"

namespace INDI
{
class BaseDevice;
}

class ClientManagerLite;
class CrosshairNode;
class RootNode;
class SkyObject;

/**
 * @class TelescopeSymbolsItem
 * This class handles representation of telescope symbols in SkyMapLite
 *
 * @author Artem Fedoskin
 * @version 1.0
 */
class TelescopeSymbolsItem : public SkyItem
{
  public:
    /**
     * @short Constructor
     * @param rootNode parent RootNode that instantiates PlanetsItem
     */
    explicit TelescopeSymbolsItem(RootNode *rootNode);

    /**
     * @short Updates position and visibility of CrosshairNodes that represent telescope symbols
     * If client is no more connected to host or device CrosshairNode is deleted.
     */
    virtual void update() override;

    /** Add telescope symbol for device bd */
    void addTelescope(INDI::BaseDevice *bd);

    /** Remove telescope symbol of device bd */
    void removeTelescope(INDI::BaseDevice *bd);

  private:
    QHash<INDI::BaseDevice *, CrosshairNode *> m_telescopes;
    ClientManagerLite *m_clientManager { nullptr };
    QColor m_color;
    KStarsData *m_KStarsData { nullptr };
};
