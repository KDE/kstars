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
  int hist_height    = height() -5; 
  QPainter painter(this);
  QPen pen;
  pen.setWidth(1);
  painter.setPen(pen);
	
 painter.setRenderHint(QPainter::Antialiasing);

  // Draw box
  QRect enclosedRect = frameRect();
  enclosedRect.setY(enclosedRect.y() + 5);
  enclosedRect.setHeight(enclosedRect.height() - 5);
  painter.drawRect(enclosedRect);

  for (int i=0; i < 500; i++)
   	painter.drawLine(i, hist_height , i, hist_height - data->histArray[i] + 5); 

    painter.setPen(Qt::NoPen);

    painter.setBrush(Qt::red);
    QRectF upperLimit(490.0, -5.0, 10.0, 10.0);
    painter.drawEllipse(upperLimit);

   painter.setBrush(Qt::blue);
   QRectF lowerLimit(10.0, hist_height - 10.0, 10.0, 10.0);
    painter.drawEllipse(lowerLimit);


   
}

#include "fitshistogramdraw.moc"
