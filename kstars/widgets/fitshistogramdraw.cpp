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

histDrawArea::histDrawArea(QWidget* parent) : QFrame(parent), height_adj(10), circle_dim(15), line_height(70)
{
	data = (FITSHistogram*) parent;

	upperLimitX = BARS - circle_dim;
	lowerLimitX = 0;
}

histDrawArea::~histDrawArea()
{
}

void histDrawArea::paintEvent(QPaintEvent *event)
{
  Q_UNUSED(event);

  int hist_height    = height() - height_adj; 
  int red_line_x = (int) (upperLimitX + circle_dim / 2.);
  int blue_line_x = (int) (lowerLimitX + circle_dim / 2.);

  QPainter painter(this);
   painter.setRenderHint(QPainter::Antialiasing);
  QPen pen;
  pen.setWidth(1);
  painter.setPen(pen);

  // Draw box
  QRect enclosedRect = frameRect();
  enclosedRect.setY(enclosedRect.y() + height_adj);
  enclosedRect.setHeight(enclosedRect.height() - height_adj);
  painter.drawRect(enclosedRect);

  // Paint Histogram
  for (int i=0; i < BARS; i++)
   	painter.drawLine(i, hist_height, i, hist_height  - data->histArray[i]); 

   pen.setWidth(2);

   pen.setColor(Qt::red);
   painter.drawLine(red_line_x, height_adj, red_line_x, line_height);

   pen.setColor(Qt::blue);
   painter.drawLine(blue_line_x, hist_height , blue_line_x, hist_height - line_height);

   // Outline
   pen.setColor(Qt::black);

   // Paint Red Circle
   painter.setBrush(Qt::red);
   QRectF upperLimit(upperLimitX, height_adj/2., circle_dim, circle_dim);
   painter.drawEllipse(upperLimit);

   // Paint Blue Circle
   painter.setBrush(Qt::blue);
   QRectF lowerLimit(lowerLimitX, hist_height - height_adj, circle_dim, circle_dim);
   painter.drawEllipse(lowerLimit);


   
}

#include "fitshistogramdraw.moc"
