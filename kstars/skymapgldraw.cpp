/***************************************************************************
                  skymapgldraw.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Wed Dec 29 2010 04:07 AM UTC-6
    copyright            : (C) 2010 Akarsh Simha
    email                : akarsh.simha@kdemail.net
 ***************************************************************************/
/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "skymapcomposite.h"
#include "skyglpainter.h"
#include "skymapgldraw.h"
#include "skymap.h"

SkyMapGLDraw::SkyMapGLDraw( SkyMap *sm ) : SkyMapDrawAbstract( sm ), QGLWidget( QGLFormat(QGL::SampleBuffers), sm ) {
    // TODO: Any construction to be done?
    if( !format().testOption( QGL::SampleBuffers ) )
        qWarning() << "No sample buffer; can't use multisampling (antialiasing)";
    if( !format().testOption( QGL::StencilBuffer ) )
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

void SkyMapGLDraw::paintEvent( QPaintEvent *event )
{
    QPainter p;
    p.begin(this);
    p.beginNativePainting();

    m_SkyMap->setupProjector();
    if(m_framecount == 25) {
        float sec = m_fpstime.elapsed()/1000.;
        printf("FPS: %.2f\n", m_framecount/sec);
        m_framecount = 0;
        m_fpstime.restart();
    }
    SkyGLPainter psky( m_SkyMap );
    //FIXME: we may want to move this into the components.
    psky.begin();

    //Draw all sky elements
    psky.drawSkyBackground();
    m_KStarsData->skyComposite()->draw( &psky );
    //Finish up
    psky.end();
    
    p.endNativePainting();
    drawOverlays(p);
    p.end();

    ++m_framecount;
}
