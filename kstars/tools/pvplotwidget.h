/***************************************************************************
                          pvplotwidget.h
                             -------------------
    begin                : Sat 17 Dec 2005
    copyright            : (C) 2005 by Jason Harris
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

#ifndef PVPLOTWIDGET_H_
#define PVPLOTWIDGET_H_

#include <QFrame>

#include "kplotwidget.h"

class PlanetViewer;

class PVPlotWidget : public KPlotWidget
{
    Q_OBJECT
public:
    PVPlotWidget( QWidget *parent=0 );
    ~PVPlotWidget();

public slots:
    void slotZoomIn();
    void slotZoomOut();

signals:
    void doubleClicked( double, double );

protected:
    virtual void keyPressEvent( QKeyEvent *e );
    virtual void mousePressEvent( QMouseEvent *e );
    virtual void mouseMoveEvent( QMouseEvent *e );
    virtual void mouseReleaseEvent( QMouseEvent * );
    virtual void mouseDoubleClickEvent( QMouseEvent *e );
    virtual void wheelEvent( QWheelEvent *e );

private:
    bool mouseButtonDown;
    int oldx, oldy;
    double initX, initY;
    double factor;
    PlanetViewer *pv;
    void updateFactor( const int modifier );
    QList<KPlotObject*> distancePoints;
};

#endif
