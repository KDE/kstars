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

#ifdef _WIN32
#include <windows.h>
#endif

#include "texturemanager.h"
#include "skymapcomposite.h"
#include "skyglpainter.h"
#include "skymapgldraw.h"
#include "skymap.h"


SkyMapGLDraw::SkyMapGLDraw( SkyMap *sm ) :
    QGLWidget( sm ),
    SkyMapDrawAbstract( sm )
{
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
    // This is machinery to prevent multiple concurrent paint events / recursive paint events
    if( m_DrawLock )
        return;
    setDrawLock( true );

    QPainter p;
    p.begin(this);
    p.beginNativePainting();
    calculateFPS();
    m_SkyMap->setupProjector();
    makeCurrent();

    SkyGLPainter psky( this );
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

    setDrawLock( false );
}
