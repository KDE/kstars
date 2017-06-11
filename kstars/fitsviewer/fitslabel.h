/*  FITS Label
    Copyright (C) 2003-2017 Jasem Mutlaq <mutlaqja@ikarustech.com>
    Copyright (C) 2016-2017 Robert Lancaster <rlancaste@gmail.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#ifndef FITSLabel_H_
#define FITSLabel_H_

#include <QFrame>
#include <QImage>
#include <QPixmap>
#include <QMouseEvent>
#include <QResizeEvent>

#include <QGestureEvent>
#include <QPinchGesture>

#include <QLabel>

#include "fitscommon.h"
#include "dms.h"
#include "fitsdata.h"

class FITSView;

class FITSLabel : public QLabel
{
    Q_OBJECT
  public:
    explicit FITSLabel(FITSView *img, QWidget *parent = nullptr);
    virtual ~FITSLabel();
    void setSize(double w, double h);
    void centerTelescope(double raJ2000, double decJ2000);
    bool getMouseButtonDown();

  protected:
    virtual void mouseMoveEvent(QMouseEvent *e);
    virtual void mousePressEvent(QMouseEvent *e);
    virtual void mouseReleaseEvent(QMouseEvent *e);
    virtual void mouseDoubleClickEvent(QMouseEvent *e);

  private:
    bool mouseButtonDown = false;
    QPoint lastMousePoint;
    FITSView *view;
    dms ra;
    dms dec;
    double width, height, size;

  signals:
    void newStatus(const QString &msg, FITSBar id);
    void pointSelected(int x, int y);
    void markerSelected(int x, int y);
};

#endif
