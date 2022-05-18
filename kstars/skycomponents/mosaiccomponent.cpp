/*  Mosaic Panel Component

    SPDX-FileCopyrightText: 2022 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "mosaiccomponent.h"

#include "Options.h"
#include "skypainter.h"
#include "skymap.h"
#include "mosaictiles.h"

MosaicComponent::MosaicComponent(SkyComposite *parent) : SkyComponent(parent)
{
    m_MosaicTiles.reset(new MosaicTiles());
}

bool MosaicComponent::selected()
{
    return Options::showMosaicPanel();
}

void MosaicComponent::draw(SkyPainter *skyp)
{
    if (selected())
        skyp->drawMosaicPanel(m_MosaicTiles.get());
}

const QSharedPointer<MosaicTiles> MosaicComponent::tiles() const
{
    return m_MosaicTiles;
}
