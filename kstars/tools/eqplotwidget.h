/***************************************************************************
                          eqplotwidget.h  -  description
                             -------------------
    begin                : Thu 16 Mar 2007
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

#include "kplotwidget.h"

#ifndef EQPLOTWIDGET_H_
#define EQPLOTWIDGET_H_

class eqPlotWidget : public KPlotWidget {
    Q_OBJECT
public:
    eqPlotWidget( QWidget *parent=0 );

protected:
    virtual void paintEvent( QPaintEvent * );
};

#endif
