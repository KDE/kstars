/*
    SPDX-FileCopyrightText: 2010 Akarsh Simha <akarsh.simha@kdemail.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifdef _WIN32
#include <windows.h>
#endif

#include "texturemanager.h"
#include "skymapcomposite.h"
#include "skyglpainter.h"
#include "skymapgldraw.h"
#include "skymap.h"

SkyMapGLDraw::SkyMapGLDraw(SkyMap *sm) : QGLWidget(sm), SkyMapDrawAbstract(sm)
{
    if (!format().testOption(QGL::SampleBuffers))
        qWarning() << "No sample buffer; can't use multisampling (antialiasing)";
    if (!format().testOption(QGL::StencilBuffer))
        qWarning() << "No stencil buffer; can't draw concave polygons";
}

void SkyMapGLDraw::initializeGL()
{
}

void SkyMapGLDraw::resizeGL(int width, int height)
{
    Q_UNUSED(width)
    Q_UNUSED(height)
    //do nothing since we resize in SkyGLPainter::paintGL()
}

void SkyMapGLDraw::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    // This is machinery to prevent multiple concurrent paint events / recursive paint events
    if (m_DrawLock)
        return;
    setDrawLock(true);

    QPainter p;
    p.begin(this);
    p.beginNativePainting();
    calculateFPS();
    m_SkyMap->setupProjector();
    makeCurrent();

    SkyGLPainter psky(this);
    //FIXME: we may want to move this into the components.
    psky.begin();

    //Draw all sky elements
    psky.drawSkyBackground();
    m_KStarsData->skyComposite()->draw(&psky);
    //Finish up
    psky.end();

    p.endNativePainting();
    drawOverlays(p);
    p.end();

    setDrawLock(false);
}
