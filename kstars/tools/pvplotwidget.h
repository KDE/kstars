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
    PVPlotWidget(QWidget *parent = 0);
    ~PVPlotWidget();

  public slots:
    void slotZoomIn();
    void slotZoomOut();

  signals:
    void doubleClicked(double, double);

  protected:
    void keyPressEvent(QKeyEvent *e) Q_DECL_OVERRIDE;
    void mousePressEvent(QMouseEvent *e) Q_DECL_OVERRIDE;
    void mouseMoveEvent(QMouseEvent *e) Q_DECL_OVERRIDE;
    void mouseReleaseEvent(QMouseEvent *) Q_DECL_OVERRIDE;
    void mouseDoubleClickEvent(QMouseEvent *e) Q_DECL_OVERRIDE;
    void wheelEvent(QWheelEvent *e) Q_DECL_OVERRIDE;

  private:
    bool mouseButtonDown;
    int oldx, oldy;
    double factor;
    PlanetViewer *pv;
    void updateFactor(const int modifier);
};

#endif
