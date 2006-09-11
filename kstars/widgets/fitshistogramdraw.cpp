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
#include <QMouseEvent>

histDrawArea::histDrawArea(QWidget* parent) : QFrame(parent), height_adj(10), circle_dim(15), line_height(70)
{
	data = (FITSHistogram*) parent;

	upperLimitX = BARS - circle_dim;
	lowerLimitX = 0;

	circle_drag_upper = false;
	circle_drag_lower = false;

	enclosedRect.setRect(0,height_adj, maximumWidth() - 1, maximumHeight() - height_adj);

}

histDrawArea::~histDrawArea()
{
}

void histDrawArea::paintEvent(QPaintEvent *event)
{
  Q_UNUSED(event);

   if (data->histArray == NULL)
	return;

  int displacement = CIRCLE_DIM / 2;
  int hist_height    = height() - height_adj; 
  int red_line_x = (int) (upperLimitX + circle_dim / 2.);
  int blue_line_x = (int) (lowerLimitX + circle_dim / 2.);

  QPainter painter(this);
   //painter.setRenderHint(QPainter::Antialiasing);
  QPen pen;
  pen.setWidth(1);
  painter.setPen(pen);

  // Draw box
  painter.drawRect(enclosedRect);

  // Paint Histogram
  for (int i=0; i < valid_width; i++)
   	painter.drawLine(i+displacement, hist_height, i+displacement, hist_height  - data->histArray[i]); 

   pen.setWidth(2);

   pen.setColor(Qt::red);
   painter.drawLine(red_line_x, height_adj, red_line_x, line_height);

   pen.setColor(Qt::blue);
   painter.drawLine(blue_line_x, hist_height , blue_line_x, hist_height - line_height);

   // Outline
   pen.setColor(Qt::black);

   // Paint Red Circle
   painter.setBrush(Qt::red);
   upperLimit.setRect(upperLimitX, height_adj/2., circle_dim, circle_dim);
   painter.drawEllipse(upperLimit);

   // Paint Blue Circle
   painter.setBrush(Qt::blue);
   lowerLimit.setRect(lowerLimitX, hist_height - height_adj, circle_dim, circle_dim);
   painter.drawEllipse(lowerLimit);

}

void histDrawArea::mouseMoveEvent ( QMouseEvent * event )
{
/*
 	if (event->buttons() & Qt::LeftButton)
	{

		if (circle_drag_upper)
		{
			upperLimitX = event->x() - circle_dim / 2.;
			if (upperLimitX < 0)
				upperLimitX = 0;
			else if (upperLimitX > (BARS - circle_dim / 2.))
				upperLimitX = BARS - circle_dim / 2.;

			update();
			return;
		}
		
		if (circle_drag_lower)
		{
		  	lowerLimitX = event->x() - circle_dim / 2.;
			if (lowerLimitX < 0)
				lowerLimitX = 0;
			else if (lowerLimitX > (BARS - circle_dim / 2.))
				lowerLimitX = BARS - circle_dim / 2.;

			update();
			return;
		}

		if (upperLimit.contains(event->pos()))
			circle_drag_upper = true;
		else if (lowerLimit.contains(event->pos()))
			circle_drag_lower = true;
	}
*/
}

void histDrawArea::mousePressEvent ( QMouseEvent * event )
{
	Q_UNUSED(event);
}

void histDrawArea::mouseReleaseEvent ( QMouseEvent * event )
{
	Q_UNUSED(event);

	circle_drag_upper = false;
	circle_drag_lower = false;

}

void resizeEvent ( QResizeEvent * event )
{
	kDebug() << "Resize Event" << endl;

	valid_width  = event->width() - CIRCLE_DIM;
	valid_height = event->height() - CIRCLE_DIM;

	data->constructHistogram(valid_width, valid_height);
}

#include "fitshistogramdraw.moc"
