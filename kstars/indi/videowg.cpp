/*
    SPDX-FileCopyrightText: 2017 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "videowg.h"

#include "kstars_debug.h"

#include <QImageReader>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QRubberBand>

VideoWG::VideoWG(QWidget *parent) : QLabel(parent)
{
    streamImage.reset(new QImage());

    grayTable.resize(256);

    for (int i = 0; i < 256; i++)
        grayTable[i] = qRgb(i, i, i);
}

bool VideoWG::newBayerFrame(IBLOB *bp, const BayerParams &params)
{
    return debayer(bp, params);
}

bool VideoWG::newFrame(IBLOB *bp)
{
    if (bp->size <= 0)
        return false;

    bool rc = false;
    QString format(bp->format);
    if (m_RawFormat != format)
    {
        format.remove('.');
        format.remove("stream_");
        m_RawFormatSupported = QImageReader::supportedImageFormats().contains(format.toLatin1());
        m_RawFormat = format;
    }

    if (m_RawFormatSupported)
        rc = streamImage->loadFromData(static_cast<uchar *>(bp->blob), bp->size);
    else if (static_cast<uint32_t>(bp->size) == totalBaseCount)
    {
        streamImage.reset(new QImage(static_cast<uchar *>(bp->blob), streamW, streamH, QImage::Format_Indexed8));
        streamImage->setColorTable(grayTable);
        rc = !streamImage->isNull();
    }
    else if (static_cast<uint32_t>(bp->size) == totalBaseCount * 3)
    {
        streamImage.reset(new QImage(static_cast<uchar *>(bp->blob), streamW, streamH, QImage::Format_RGB888));
        rc = !streamImage->isNull();
    }

    if (rc)
    {
        kPix = QPixmap::fromImage(streamImage->scaled(size(), Qt::KeepAspectRatio));
        setPixmap(kPix);
    }

    emit imageChanged(streamImage);

    return rc;
}

bool VideoWG::save(const QString &filename, const char *format)
{
    return kPix.save(filename, format);
}

void VideoWG::setSize(uint16_t w, uint16_t h)
{
    streamW        = w;
    streamH        = h;
    totalBaseCount = w * h;
}

//void VideoWG::resizeEvent(QResizeEvent *ev)
//{
//    setPixmap(QPixmap::fromImage(streamImage->scaled(ev->size(), Qt::KeepAspectRatio)));
//    ev->accept();
//}

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

void VideoWG::mouseReleaseEvent(QMouseEvent *event)
{
    rubberBand->hide();

    if (event->button() == Qt::RightButton)
    {
        emit newSelection(QRect());
        return;
    }

    QRect rawSelection = rubberBand->geometry();
    int pixmapX        = (width() - kPix.width()) / 2;
    int pixmapY        = (height() - kPix.height()) / 2;

    QRect finalSelection;

    double scaleX = static_cast<double>(streamImage->width()) / kPix.width();
    double scaleY = static_cast<double>(streamImage->height()) / kPix.height();

    finalSelection.setX((rawSelection.x() - pixmapX) * scaleX);
    finalSelection.setY((rawSelection.y() - pixmapY) * scaleY);
    finalSelection.setWidth(rawSelection.width() * scaleX);
    finalSelection.setHeight(rawSelection.height() * scaleY);

    emit newSelection(finalSelection);
    // determine selection, for example using QRect::intersects()
    // and QRect::contains().
}

bool VideoWG::debayer(const IBLOB *bp, const BayerParams &params)
{
    uint32_t rgb_size = streamW * streamH * 3;
    auto * destinationBuffer = new uint8_t[rgb_size];

    if (destinationBuffer == nullptr)
    {
        qCCritical(KSTARS) << "Unable to allocate memory for temporary bayer buffer.";
        return false;
    }

    int ds1394_height = streamH;

    uint8_t * dc1394_source = reinterpret_cast<uint8_t*>(bp->blob);
    if (params.offsetY == 1)
    {
        dc1394_source += streamW;
        ds1394_height--;
    }
    if (params.offsetX == 1)
    {
        dc1394_source++;
    }
    dc1394error_t error_code = dc1394_bayer_decoding_8bit(dc1394_source, destinationBuffer, streamW, ds1394_height,
                               params.filter, params.method);

    if (error_code != DC1394_SUCCESS)
    {
        qCCritical(KSTARS) << "Debayer failed" << error_code;
        delete[] destinationBuffer;
        return false;
    }

    streamImage.reset(new QImage(destinationBuffer, streamW, streamH, QImage::Format_RGB888));
    bool rc = !streamImage->isNull();

    if (rc)
    {
        kPix = QPixmap::fromImage(streamImage->scaled(size(), Qt::KeepAspectRatio));
        setPixmap(kPix);
    }

    emit imageChanged(streamImage);

    delete[] destinationBuffer;
    return rc;
}

