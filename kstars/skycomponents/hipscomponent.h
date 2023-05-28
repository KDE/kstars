/*  HiPS : Hierarchical Progressive Surveys
    HiPS is the hierarchical tiling mechanism which allows one to access, visualize and browse seamlessly image, catalogue and cube data.

    The KStars HiPS compoenent is used to load and overlay progress surverys from various online catalogs.

    SPDX-FileCopyrightText: 2017 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "skycomponent.h"
#include "projections/projector.h"

/**
 * @class HIPSComponent
 * Represents the HIPS progress survey overlay
 * @author Jasem Mutlaq
 * @version 1.0
 */
class HIPSComponent : public SkyComponent
{
    public:
        /** Constructor */
        explicit HIPSComponent(SkyComposite *);

        virtual ~HIPSComponent() override = default;

        bool selected() override;
        void draw(SkyPainter *skyp) override;

    private:
        QElapsedTimer m_ElapsedTimer, m_RefreshTimer;
        static constexpr uint32_t HIPS_REDRAW_PERIOD {5000};
        static constexpr uint32_t HIPS_REFRESH_PERIOD {2000};
        ViewParams m_previousViewParams;
        QString m_LastFocusedObjectName;
};
