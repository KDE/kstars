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
    int getHistWidth() { return hist_width; }
    int getHistHeight() { return hist_height;}
    void init();

protected:
    void paintEvent(QPaintEvent *event);
    void mouseMoveEvent ( QMouseEvent * event );

private:
    FITSHistogram *data;
    const int height_adj;
    const int line_height;

    int hist_width;
    int hist_height;

    QRect enclosedRect;

};

#endif

