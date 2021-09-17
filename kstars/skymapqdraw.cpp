/*
    SPDX-FileCopyrightText: 2010 Akarsh Simha <akarsh.simha@kdemail.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "skymapqdraw.h"
#include "skymapcomposite.h"
#include "skyqpainter.h"
#include "skymap.h"
#include "projections/projector.h"
#include "printing/legend.h"
#include "kstars_debug.h"
#include <QPainterPath>

SkyMapQDraw::SkyMapQDraw(SkyMap *sm) : QWidget(sm), SkyMapDrawAbstract(sm)
{
    m_SkyPixmap = new QPixmap(width(), height());
}

SkyMapQDraw::~SkyMapQDraw()
{
    delete m_SkyPixmap;
}

void SkyMapQDraw::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    // This is machinery to prevent multiple concurrent paint events / recursive paint events
    if (m_DrawLock)
    {
        qCDebug(KSTARS) << "Prevented a recursive / concurrent draw!";
        return;
    }
    setDrawLock(true);

    // JM 2016-05-03: Not needed since we're not using OpenGL for now
    //calculateFPS();

    //If computeSkymap is false, then we just refresh the window using the stored sky pixmap
    //and draw the "overlays" on top.  This lets us update the overlay information rapidly
    //without needing to recompute the entire skymap.
    //use update() to trigger this "short" paint event; to force a full "recompute"
    //of the skymap, use forceUpdate().

    if (!m_SkyMap->computeSkymap)
    {
        QPainter p;
        p.begin(this);
        p.drawLine(0, 0, 1, 1); // Dummy operation to circumvent bug. TODO: Add details
        p.drawPixmap(0, 0, *m_SkyPixmap);
        drawOverlays(p);
        p.end();

        setDrawLock(false);
        return; // exit because the pixmap is repainted and that's all what we want
    }

    // FIXME: used to notify infobox about possible change of object coordinates
    // Not elegant at all. Should find better option
    m_SkyMap->showFocusCoords();
    m_SkyMap->setupProjector();

    SkyQPainter psky(this, m_SkyPixmap);
    //FIXME: we may want to move this into the components.
    psky.begin();

    //Draw all sky elements
    psky.drawSkyBackground();

    // Set Clipping
    QPainterPath path;
    path.addPolygon(m_SkyMap->projector()->clipPoly());
    psky.setClipPath(path);
    psky.setClipping(true);

    m_KStarsData->skyComposite()->draw(&psky);
    //Finish up
    psky.end();

    QPainter psky2;
    psky2.begin(this);
    psky2.drawLine(0, 0, 1, 1); // Dummy op.
    psky2.drawPixmap(0, 0, *m_SkyPixmap);
    drawOverlays(psky2);
    psky2.end();

    if (m_SkyMap->m_previewLegend)
    {
        m_SkyMap->m_legend.paintLegend(m_SkyPixmap);
    }

    m_SkyMap->computeSkymap = false; // use forceUpdate() to compute new skymap else old pixmap will be shown

    setDrawLock(false);
}

void SkyMapQDraw::resizeEvent(QResizeEvent *e)
{
    Q_UNUSED(e)
    delete m_SkyPixmap;
    m_SkyPixmap = new QPixmap(width(), height());
}
