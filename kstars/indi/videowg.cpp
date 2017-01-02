/*  Video Stream Frame
    Copyright (C) 2017 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

*/

#include <QImageReader>
#include <QPainter>

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

    int w = *((int *) bp->aux0);
    int h = *((int *) bp->aux1);

    if (QImageReader::supportedImageFormats().contains(format.toLatin1()))
           rc = streamImage->loadFromData(static_cast<uchar *>(bp->blob), bp->size);
    else if (bp->size > totalBaseCount)
    {
        delete(streamImage);
        streamImage = new QImage(static_cast<uchar *>(bp->blob), w, h, QImage::Format_RGB32);
        rc = !streamImage->isNull();
    }
    else
    {
        delete(streamImage);
        streamImage = new QImage(static_cast<uchar *>(bp->blob), w, h, QImage::Format_Indexed8);
        streamImage->setColorTable(grayTable);
        rc = !streamImage->isNull();
    }

    if (rc)
    {
        setPixmap(QPixmap::fromImage(streamImage->scaled(size(), Qt::KeepAspectRatio)));
    }

    return rc;
}

bool VideoWG::save(const QString &filename, const char *format)
{
    return kPix.save(filename, format);
}

void VideoWG::setTotalBaseCount(int value)
{
    totalBaseCount = value;
}

void VideoWG::resizeEvent(QResizeEvent *ev)
{
    setPixmap(QPixmap::fromImage(streamImage->scaled(ev->size(), Qt::KeepAspectRatio)));

    ev->accept();
}



