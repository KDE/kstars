/***************************************************************************
                          fitshistogramdraw.cpp  -  FITS Historgram
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

#include <cmath>

#include "fitshistogramdraw.h"
#include "fitshistogram.h"

#include <QPainter>
#include <QPen>
#include <QPainterPath>
#include <QMouseEvent>
#include <QDebug>

histDrawArea::histDrawArea(QWidget* parent) : QFrame(parent), height_adj(10), line_height(70)
{
    data = (FITSHistogram*) parent;

    setCursor(Qt::CrossCursor);
    setMouseTracking(true);

}

void histDrawArea::init()
{
    hist_width  = maximumWidth();
    hist_height = maximumHeight();

    //qDebug() << "constructor VALID Width: " << valid_width << " - VALID height: " << valid_height;
    enclosedRect.setRect(0, 0, hist_width-1, hist_height);
}

histDrawArea::~histDrawArea()
{
}

void histDrawArea::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    QPen pen;

    int pixelWidth = floor(data->binWidth/data->histFactor);
    if (pixelWidth < 1)
        pixelWidth = 1;

    pen.setWidth(pixelWidth);
    painter.setPen(pen);


    for (int i=0; i < data->histArray.size(); i++)
        painter.drawLine(i*pixelWidth, hist_height, i*pixelWidth, hist_height - (data->histArray[i]*data->histHeightRatio));


    // Draw box
    pen.setWidth(1);
    painter.setPen(pen);

    painter.drawRect(enclosedRect);



}

void histDrawArea::mouseMoveEvent ( QMouseEvent * event )
{
    int event_x = (int) (event->x());
    data->updateIntenFreq(event_x);
}


