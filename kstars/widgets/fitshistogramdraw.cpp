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
#include "fitshistogram.h"

#include <QPainter>
#include <QPen>
#include <QPainterPath>
#include <QMouseEvent>

histDrawArea::histDrawArea(QWidget* parent) : QFrame(parent), height_adj(10), line_height(70)
{
    data = (FITSHistogram*) parent;

    circle_drag_upper = false;
    circle_drag_lower = false;

    setCursor(Qt::CrossCursor);
    setMouseTracking(true);

}

void histDrawArea::init()
{
    valid_width  = maximumWidth() - CIRCLE_DIM;
    valid_height = maximumHeight() - CIRCLE_DIM;

    //kDebug() << "constructor VALID Width: " << valid_width << " - VALID height: " << valid_height;
    enclosedRect.setRect(CIRCLE_DIM/2, CIRCLE_DIM/2, valid_width, valid_height);

    upperLimitX = valid_width;
    lowerLimitX  = 0;

    //kDebug() << "Calling construction histogram";
    data->constructHistogram(valid_width, valid_height);
}

histDrawArea::~histDrawArea()
{
}

void histDrawArea::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    int hist_height    = height() - height_adj;
    int red_line_x = (int) (upperLimitX + CIRCLE_DIM / 2.);
    int blue_line_x = (int) (lowerLimitX + CIRCLE_DIM / 2.);

    QPainter painter(this);

    QPen pen;
    pen.setWidth(1);
    painter.setPen(pen);

    // Draw box
    painter.drawRect(enclosedRect);

    hist_height = valid_height + CIRCLE_DIM/2;
    //hist_height = valid_height;// + CIRCLE_DIM/2;

    //kDebug() << "hist_height: " << hist_height << " valid height: " << valid_height << "CIRCLE_DIM " << CIRCLE_DIM;
    // Paint Histogram
    for (int i=0; i < valid_width; i++)
        painter.drawLine(i+CIRCLE_DIM/2, hist_height, i+CIRCLE_DIM/2, hist_height - data->histArray[i]);
    //painter.drawLine(i, hist_height, i, valid_height - data->histArray[i]);

    pen.setColor(Qt::red);
    painter.setPen(pen);
    painter.drawLine(red_line_x, 0, red_line_x, line_height);

    pen.setColor(Qt::blue);
    painter.setPen(pen);
    painter.drawLine(blue_line_x, valid_height , blue_line_x, valid_height - line_height);

    // Outline
    pen.setColor(Qt::black);

    painter.setPen(pen);

    // Paint Red Circle
    painter.setBrush(Qt::red);
    upperLimit.setRect(upperLimitX, 0, CIRCLE_DIM, CIRCLE_DIM);
    painter.drawEllipse(upperLimit);

    // Paint Blue Circle
    painter.setBrush(Qt::blue);
    lowerLimit.setRect(lowerLimitX, valid_height, CIRCLE_DIM, CIRCLE_DIM);
    painter.drawEllipse(lowerLimit);

}

void histDrawArea::mouseMoveEvent ( QMouseEvent * event )
{

    int event_x = (int) (event->x() - CIRCLE_DIM / 2.);

    if (event->buttons() & Qt::LeftButton)
    {

        if (circle_drag_upper)
        {
            upperLimitX = event_x;
            if (upperLimitX < 0)
                upperLimitX = 0;
            else if (upperLimitX > valid_width)
                upperLimitX = valid_width;

            update();
            data->updateBoxes((int) lowerLimitX, (int) upperLimitX);
            return;
        }

        if (circle_drag_lower)
        {
            lowerLimitX = event_x;
            if (lowerLimitX < 0)
                lowerLimitX = 0;
            else if (lowerLimitX > valid_width)
                lowerLimitX = valid_width;

            update();
            data->updateBoxes((int) lowerLimitX, (int) upperLimitX);
            return;
        }

        if (upperLimit.contains(event->pos()))
        {
            circle_drag_upper = true;
            circle_drag_lower = false;
        }
        else if (lowerLimit.contains(event->pos()))
        {
            circle_drag_upper = false;
            circle_drag_lower = true;
        }
    }
    else
        data->updateIntenFreq(event_x);

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

/*void histDrawArea::resizeEvent ( QResizeEvent * event )
{
	//kDebug() << "Resize Event: new Width: " << event->size().width() << " - new Height: " << event->size().height();

	valid_width  = event->size().width() - CIRCLE_DIM;
	valid_height = event->size().height() - CIRCLE_DIM;

	//kDebug() << "Resize Event: VALID Width: " << valid_width << " - VALID height: " << valid_height;
	enclosedRect.setRect(CIRCLE_DIM/2, CIRCLE_DIM/2, valid_width, valid_height);

	upperLimitX = valid_width;
	lowerLimitX  = 0;

	//kDebug() << "Calling construction histogram";
	data->constructHistogram(valid_width, valid_height);
}*/

int histDrawArea::getUpperLimit()
{
    return ( (int) upperLimitX);
}

int histDrawArea::getLowerLimit()
{
    return ( (int) lowerLimitX);
}

int histDrawArea::getValidWidth()
{
    return valid_width;
}

int histDrawArea::getValidHeight()
{
    return valid_height;
}

void histDrawArea::updateLowerLimit()
{
    bool conversion_ok = false;
    int newLowerLimit=0;

    newLowerLimit = data->ui->minOUT->text().toInt(&conversion_ok);

    if (conversion_ok == false)
        return;

    newLowerLimit = (int) ((newLowerLimit - data->fits_min) * data->binSize);

    if (newLowerLimit < 0) newLowerLimit = 0;
    else if (newLowerLimit > valid_width) newLowerLimit = valid_width;

    lowerLimitX = newLowerLimit;

    update();
}

void histDrawArea::updateUpperLimit()
{
    bool conversion_ok = false;
    int newUpperLimit=0;

    newUpperLimit = data->ui->maxOUT->text().toInt(&conversion_ok);

    if (conversion_ok == false)
        return;

    newUpperLimit = (int) ((newUpperLimit - data->fits_min) * data->binSize);

    if (newUpperLimit < 0) newUpperLimit = 0;
    else if (newUpperLimit > valid_width) newUpperLimit = valid_width;

    upperLimitX = newUpperLimit;

    update();
}

#include "fitshistogramdraw.moc"
