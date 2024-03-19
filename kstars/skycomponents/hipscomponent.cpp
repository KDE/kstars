/*  HiPS : Hierarchical Progressive Surveys
    HiPS is the hierarchical tiling mechanism which allows one to access, visualize and browse seamlessly image, catalogue and cube data.

    The KStars HiPS compoenent is used to load and overlay progress surveys from various online catalogs.

    SPDX-FileCopyrightText: 2017 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "hipscomponent.h"

#include "Options.h"
#include "skypainter.h"
#include "skymap.h"

HIPSComponent::HIPSComponent(SkyComposite *parent) : SkyComponent(parent)
{
    m_ElapsedTimer.start();
    m_RefreshTimer.start();
}

bool HIPSComponent::selected()
{
    return Options::showHIPS();
}

void HIPSComponent::draw(SkyPainter *skyp)
{

#if !defined(KSTARS_LITE)
    if ( (SkyMap::IsSlewing() && !Options::hIPSPanning()) || !selected())
        return;

    // If we are tracking and we currently have a focus object or point
    // Then no need for re-render every update cycle since that is CPU intensive
    // Draw the cached HiPS image for 5000ms. When this expires, render the image again and
    // restart the timer.

    // Keep track of zoom level and redraw if changes.
    ViewParams view = SkyMap::Instance()->projector()->viewParams();
    bool sameView = (
                        view.width == m_previousViewParams.width &&
                        view.height == m_previousViewParams.height &&
                        view.zoomFactor == m_previousViewParams.zoomFactor &&
                        view.rotationAngle == m_previousViewParams.rotationAngle &&
                        view.mirror == m_previousViewParams.mirror &&
                        view.useAltAz == m_previousViewParams.useAltAz
                    );
    if (sameView && Options::isTracking() && SkyMap::IsFocused())
    {
        // We can draw the cache when two conditions are met.
        // 1. It is not yet time to re-draw
        // 2. Refresh time expired.
        if (m_ElapsedTimer.elapsed() < HIPS_REDRAW_PERIOD && m_RefreshTimer.elapsed() > HIPS_REFRESH_PERIOD)
        {
            skyp->drawHips(true);
        }
        else
        {
            skyp->drawHips(false);
            m_ElapsedTimer.restart();
        }

        // If focus object changes, we reset fresh timer to force drawing of uncached image.
        if (SkyMap::Instance()->focusObject() && SkyMap::Instance()->focusObject()->name() != m_LastFocusedObjectName)
        {
            m_LastFocusedObjectName = SkyMap::Instance()->focusObject()->name();
            m_RefreshTimer.restart();
        }
    }
    // When slewing or not tracking, render and draw immediately.
    else
        skyp->drawHips(false);

    m_previousViewParams = view;
#else
    Q_UNUSED(skyp);
#endif
}
