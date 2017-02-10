/*  Video Stream Frame
    Copyright (C) 2017 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

*/

#include <QImageReader>
#include <QPainter>
#include <QRubberBand>

#include "videowg.h"
#include "Options.h"

VideoWG::VideoWG(QWidget * parent) : QLabel(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent);
    streamImage    = new QImage();

    grayTable.resize(256);

    for (int i=0;i<256;i++)
        grayTable[i]=qRgb(i,i,i);
}

VideoWG::~VideoWG()
{
    delete (streamImage);
}

bool VideoWG::newFrame(IBLOB *bp)
{
    QString format(bp->format);

    format.remove(".");
    format.remove("stream_");
    bool rc = false;

    if (QImageReader::supportedImageFormats().contains(format.toLatin1()))
           rc = streamImage->loadFromData(static_cast<uchar *>(bp->blob), bp->size);
    else if (bp->size > totalBaseCount)
    {
        delete(streamImage);
        streamImage = new QImage(static_cast<uchar *>(bp->blob), streamW, streamH, QImage::Format_RGB888);
        rc = !streamImage->isNull();
    }
    else
    {
        delete(streamImage);
        streamImage = new QImage(static_cast<uchar *>(bp->blob), streamW, streamH, QImage::Format_Indexed8);
        streamImage->setColorTable(grayTable);
        rc = !streamImage->isNull();
    }

    if (rc)
    {
        kPix = QPixmap::fromImage(streamImage->scaled(size(), Qt::KeepAspectRatio));        
        setPixmap(kPix);
    }

    return rc;
}

bool VideoWG::save(const QString &filename, const char *format)
{
    return kPix.save(filename, format);
}

void VideoWG::setSize(uint16_t w, uint16_t h)
{
    streamW = w;
    streamH = h;
    totalBaseCount = w*h;
}

void VideoWG::resizeEvent(QResizeEvent *ev)
{    
    setPixmap(QPixmap::fromImage(streamImage->scaled(ev->size(), Qt::KeepAspectRatio)));
    ev->accept();
}

void VideoWG::mousePressEvent(QMouseEvent *event)
{
     origin = event->pos();
     if (!rubberBand)
         rubberBand = new QRubberBand(QRubberBand::Rectangle, this);
     rubberBand->setGeometry(QRect(origin, QSize()));
     rubberBand->show();
}

void VideoWG::mouseMoveEvent(QMouseEvent *event)
{
     rubberBand->setGeometry(QRect(origin, event->pos()).normalized());
}

void VideoWG::mouseReleaseEvent(QMouseEvent *)
{
     rubberBand->hide();

     QRect rawSelection = rubberBand->geometry();
     int pixmapX = (width() - kPix.width())/2;
     int pixmapY = (height() - kPix.height())/2;

     QRect finalSelection;

     double scaleX = static_cast<double>(streamImage->width())  / kPix.width();
     double scaleY = static_cast<double>(streamImage->height()) / kPix.height();

     finalSelection.setX( (rawSelection.x() - pixmapX) * scaleX);
     finalSelection.setY( (rawSelection.y() - pixmapY) * scaleY);
     finalSelection.setWidth(rawSelection.width()  * scaleX);
     finalSelection.setHeight(rawSelection.height() * scaleY);

     emit newSelection(finalSelection);
     // determine selection, for example using QRect::intersects()
     // and QRect::contains().
}
