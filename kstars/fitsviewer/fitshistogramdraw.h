/***************************************************************************
                          fitshistogramdraw.h  -  FITS Historgram
                          ---------------
    copyright            : (C) 2006 by Jasem Mutlaq
    email                : mutlaqja@ikarustech.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef FITSHISTOGRAMDRAW_H
#define FITSHISTOGRAMDRAW_H

#include <QFrame>

class FITSHistogram;

class histDrawArea : public QFrame
{
    Q_OBJECT

public:
    histDrawArea(QWidget *parent);
    ~histDrawArea();

    int getUpperLimit();
    int getLowerLimit();
    int getValidWidth();
    int getValidHeight();
    void init();

protected:
    void paintEvent(QPaintEvent *event);
    void mouseMoveEvent ( QMouseEvent * event );
    void mousePressEvent ( QMouseEvent * event );
    void mouseReleaseEvent ( QMouseEvent * event );
    //void resizeEvent ( QResizeEvent * event );

private:
    FITSHistogram *data;
    const int height_adj;
    const int line_height;
    float upperLimitX;
    float lowerLimitX;

    bool circle_drag_upper;
    bool circle_drag_lower;

    int valid_width;
    int valid_height;

    QRectF upperLimit;
    QRectF lowerLimit;
    QRect enclosedRect;

public slots:
    void updateLowerLimit();
    void updateUpperLimit();
};

#endif

