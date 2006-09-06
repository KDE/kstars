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

#include "fitshistogramdraw.h"
#include "../fitshistogram.h"

#include <QPainter>
#include <QPen>
#include <QPainterPath>

histDrawArea::histDrawArea(QWidget* parent) : QFrame(parent)
{
	data = (FITSHistogram*) parent;
}

histDrawArea::~histDrawArea()
{
}

void histDrawArea::paintEvent(QPaintEvent *event)
{
   int hist_height    = height(); 
  QPainter painter(this);
  QPen pen;
   //pen.setColor(Qt::Black);
   pen.setWidth(5);
   painter.setPen(pen);

 for (int i=0; i < 500; i++)
     painter.drawLine(i, hist_height , i, hist_height - (int) ((double) data->histArray[i] / (double) data->maxHeight)); 
 
	



}

#include "fitshistogramdraw.moc"
