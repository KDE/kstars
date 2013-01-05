/***************************************************************************
                          fovwidget.h  -  description
                             -------------------
    begin                : Sat 22 Sept 2007
    copyright            : (C) 2007 by Jason Harris
    email                : kstars@30doradus.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef FOVWIDGET_H_
#define FOVWIDGET_H_

#include <QFrame>

class FOV;

class FOVWidget : public QFrame {
    Q_OBJECT
    public:
        explicit FOVWidget( QWidget *parent=0 );
        ~FOVWidget();

        void setFOV( FOV *f );

    protected:
        void paintEvent( QPaintEvent *e );

    private:
        FOV *m_FOV;
};

#endif
