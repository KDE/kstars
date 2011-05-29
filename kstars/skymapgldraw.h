/***************************************************************************
                   skymapgldraw.h  -  K Desktop Planetarium
                             -------------------
    begin                : Wed Dec 29 2010 03:24 AM UTC -06:00
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

#ifndef SKYMAPGLDRAW_H_
#define SKYMAPGLDRAW_H_

#include "skymapdrawabstract.h"

#include <QGLWidget>
/**
 *@short This class draws the SkyMap using OpenGL. It
 * implements SkyMapDrawAbstract
 *@version 1.0
 *@author Akarsh Simha <akarsh.simha@kdemail.net>
 */

class SkyMapGLDraw : public QGLWidget, public SkyMapDrawAbstract {
    
    Q_OBJECT

 public:
    /**
     *@short Constructor
     */
    SkyMapGLDraw( SkyMap *parent );

 protected:

    virtual void paintEvent( QPaintEvent *e );

    virtual void initializeGL();

    virtual void resizeGL( int, int );

};

#endif
