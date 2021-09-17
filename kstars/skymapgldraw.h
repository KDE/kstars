/*
    SPDX-FileCopyrightText: 2010 Akarsh Simha <akarsh.simha@kdemail.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

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

class SkyMapGLDraw : public QGLWidget, public SkyMapDrawAbstract
{
    Q_OBJECT

  public:
    /**
         *@short Constructor
         */
    explicit SkyMapGLDraw(SkyMap *parent);

  protected:
    virtual void paintEvent(QPaintEvent *e);

    virtual void initializeGL();

    virtual void resizeGL(int, int);
};

#endif
