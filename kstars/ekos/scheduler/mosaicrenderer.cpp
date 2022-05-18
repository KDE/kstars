/*
    SPDX-FileCopyrightText: 2022 Jasem Mutlaq

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "mosaicrenderer.h"
#include "mosaictilesmanager.h"
#include "projections/projector.h"

namespace Ekos
{

MosaicRenderer::MosaicRenderer()
{
    m_TilesManager = new MosaicTilesManager(this);
    m_TilesScene.addItem(m_TilesManager);
}

bool MosaicRenderer::render(uint16_t w, uint16_t h, QImage *image, const Projector *m_proj)
{
    return false;
}

}
