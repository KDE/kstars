/*
    SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "telescopesymbolsitem.h"

#include "kstarslite.h"
#include "labelsitem.h"
#include "Options.h"
#include "indi/clientmanagerlite.h"
#include "kstarslite/skyitems/rootnode.h"
#include "projections/projector.h"
#include "skynodes/crosshairnode.h"

#include <basedevice.h>

TelescopeSymbolsItem::TelescopeSymbolsItem(RootNode *rootNode)
    : SkyItem(LabelsItem::label_t::TELESCOPE_SYMBOL, rootNode)
{
    m_clientManager = KStarsLite::Instance()->clientManagerLite();
    m_KStarsData    = KStarsData::Instance();
}

void TelescopeSymbolsItem::addTelescope(INDI::BaseDevice *bd)
{
    if (!m_telescopes.value(bd))
    {
        CrosshairNode *crossHair = new CrosshairNode(bd, rootNode());
        appendChildNode(crossHair);

        m_telescopes.insert(bd, crossHair);
    }
}

void TelescopeSymbolsItem::removeTelescope(INDI::BaseDevice *bd)
{
    CrosshairNode *crossHair = m_telescopes.value(bd);
    if (crossHair)
    {
        removeChildNode(crossHair);
        delete crossHair;
    }
    m_telescopes.remove(bd);
}

void TelescopeSymbolsItem::update()
{
    QHash<INDI::BaseDevice *, CrosshairNode *>::iterator i;
    bool deleteAll = !m_clientManager->isConnected();

    QColor color = m_KStarsData->colorScheme()->colorNamed("TargetColor");

    bool show = Options::showTargetCrosshair();
    if (!show)
    {
        hide();
    }

    for (i = m_telescopes.begin(); i != m_telescopes.end(); ++i)
    {
        CrosshairNode *crossHair = i.value();
        INDI::BaseDevice *device = i.key();
        if (crossHair)
        {
            if (deleteAll || !(device->isConnected()))
            {
                removeChildNode(crossHair);
                delete crossHair;
                m_telescopes.insert(device, nullptr);
            }
            else if (show)
            {
                if (device->isConnected())
                {
                    crossHair->setColor(color);
                    crossHair->update();
                }
                else
                {
                    crossHair->hide();
                }
            }
        }
    }
    if (deleteAll)
    {
        m_telescopes.clear();
    }
}
