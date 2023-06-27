/*  Mosaic Panel Component

    SPDX-FileCopyrightText: 2022 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "skycomponent.h"
#include <QSharedPointer>

class MosaicTiles;

/**
 * @class MosaicComponent
 * Renders Mosaic Panel on Sky Map in either of two modes depending on scheduler.
 * + Scheduler Idle: Mosaic Panel is rendered in Planning mode where user can pan and adjust location in order to create jobs for the scheduler.
 * + Scheduler Active: Mosaic Panel is locked to the sky map and each tile shows the progress, how images captured, which tile is next and when.
 * @author Jasem Mutlaq
 * @version 1.0
 */
class MosaicComponent : public SkyComponent
{
    public:
        /** Constructor */
        explicit MosaicComponent(SkyComposite *);

        virtual ~MosaicComponent() override = default;

        bool selected() override;
        void draw(SkyPainter *skyp) override;

        const QSharedPointer<MosaicTiles> tiles() const;

    private:

        QSharedPointer<MosaicTiles> m_MosaicTiles;
};
