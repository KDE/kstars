/*  Ekos Alignment View
 *  Child of AlignView with few additions necessary for Alignment functions

    Copyright (C) 2017 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#include <QtConcurrent>

#include "alignview.h"

#define ZOOM_DEFAULT	100.0
#define ZOOM_MIN        10
#define ZOOM_MAX        400

AlignView::AlignView(QWidget *parent, FITSMode mode, FITSScale filter) : FITSView(parent, mode, filter)
{

}

AlignView::~AlignView()
{

}

void AlignView::drawOverlay(QPainter *painter)
{
    painter->setOpacity(0.5);
    FITSView::drawOverlay(painter);
    painter->setOpacity(1);

    if (RACircle.isNull() == false)
        drawCircle(painter);

    if (correctionLine.isNull() == false)
        drawLine(painter);
}

bool AlignView::createWCSFile(const QString & newWCSFile, double orientation, double ra, double dec, double pixscale)
{
    return imageData->createWCSFile(newWCSFile, orientation, ra, dec, pixscale);
}

void AlignView::setCorrectionParams(QLineF line)
{
    correctionLine   = line;
    markerCrosshair  = line.p2();

    updateFrame();
}

void AlignView::setCorrectionOffset(QPointF newOffset)
{
    if (newOffset.isNull() == false)
    {        
        correctionOffset = newOffset;
        QPointF offset = correctionOffset - correctionLine.p1();
        QLineF  offsetLine = correctionLine;
        offsetLine.translate(offset);
        markerCrosshair = offsetLine.p2();
    }
    // Clear points
    else
    {
        correctionOffset = newOffset;
        markerCrosshair  = newOffset;
    }

    updateFrame();
}

void AlignView::drawLine(QPainter *painter)
{
    painter->setPen(QPen( Qt::magenta , 2));
    painter->setBrush( Qt::NoBrush );
    double zoomFactor = (currentZoom / ZOOM_DEFAULT);

    QLineF zoomedLine = correctionLine;
    QPointF offset;

    if (correctionOffset.isNull() == false)
    {
        offset = correctionOffset - correctionLine.p1();
    }

    zoomedLine.translate(offset);

    double x1 = zoomedLine.p1().x() * zoomFactor;
    double y1 = zoomedLine.p1().y() * zoomFactor;

    double x2 = zoomedLine.p2().x() * zoomFactor;
    double y2 = zoomedLine.p2().y() * zoomFactor;

    painter->drawLine(x1,y1,x2,y2);
}

void AlignView::drawCircle(QPainter *painter)
{
    QPen pen(Qt::green);
    pen.setWidth(2);
    pen.setStyle(Qt::DashLine);
    painter->setPen(pen);
    painter->setBrush( Qt::NoBrush);
    double zoomFactor = (currentZoom / ZOOM_DEFAULT);

    QPointF center(RACircle.x()*zoomFactor, RACircle.y() * zoomFactor);

    // Big Radius
    double r = RACircle.z() * zoomFactor;

    // Small radius
    double sr= r/25.0;

    painter->drawEllipse(center, sr, sr);
    painter->drawEllipse(center, r, r);
    pen.setColor(Qt::darkGreen);
    painter->setPen(pen);
    painter->drawText(center.x()+sr,center.y()+sr, i18n("RA Axis"));

}

void AlignView::setRACircle(const QVector3D &value)
{
    RACircle = value;
    updateFrame();
}

