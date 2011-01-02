/***************************************************************************
                   skymapqdraw.h  -  K Desktop Planetarium
                             -------------------
    begin                : Tue Dec 21 2010 07:10 AM UTC-6
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

#ifndef SKYMAPQDRAW_H_
#define SKYMAPQDRAW_H_

#include "skymapdrawabstract.h"

#include<QWidget>

/**
 *@short This class draws the SkyMap using native QPainter. It
 * implements SkyMapDrawAbstract
 *@version 1.0
 *@author Akarsh Simha <akarsh.simha@kdemail.net>
 */

class SkyMapQDraw : public QWidget, public SkyMapDrawAbstract {
    
    Q_OBJECT;

 public:
    /**
     *@short Constructor
     */
    SkyMapQDraw( SkyMap *parent );

    /**
     *@short Destructor
     */
    ~SkyMapQDraw();

 protected:

    virtual void paintEvent( QPaintEvent *e );
    
    virtual void resizeEvent( QResizeEvent *e );

    QPixmap *m_SkyPixmap;
    
};

#endif
