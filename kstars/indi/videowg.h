/*  Video Stream Frame
    Copyright (C) 2017 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

*/

#ifndef VIDEOWG_H_
#define VIDEOWG_H_

#include <QPixmap>
#include <QPaintEvent>
#include <QVector>
#include <QColor>
#include <QImage>
#include <QLabel>


#include <indidevapi.h>

class QRubberBand;

class VideoWG : public QLabel
{
        Q_OBJECT

    public:
        VideoWG(QWidget * parent =0);
        ~VideoWG();

        bool newFrame(IBLOB * bp);

        bool save(const QString &filename, const char * format);

        void setSize(uint16_t w, uint16_t h);

    protected:
        virtual void resizeEvent( QResizeEvent * ev );
        void mousePressEvent(QMouseEvent * event);
        void mouseMoveEvent(QMouseEvent * event);
        void mouseReleaseEvent(QMouseEvent *);

    signals:
        void newSelection(QRect);

    private:
        uint16_t         streamW=-1, streamH=-1;
        uint32_t         totalBaseCount=0;
        QVector<QRgb>    grayTable;
        QImage      *     streamImage;
        QPixmap          kPix;

        QRubberBand   *   rubberBand=NULL;
        QPoint           origin;

};

#endif
