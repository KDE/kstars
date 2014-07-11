/*  Stream Widget
    Copyright (C) 2003 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
    
    2004-03-16: A class to handle video streaming.
 */

 #ifndef STREAMWG_H_
 #define STREAMWG_H_

#include <QPixmap>
#include <QVBoxLayout>
#include <QResizeEvent>
#include <QPaintEvent>
#include <QCloseEvent>
#include <QVector>
#include <QColor>

#include <QIcon>

#include "ui_streamform.h"

#include <indidevapi.h>

class QImage;
class VideoWG;
class QVBoxLayout;

class StreamWG : public QWidget, public Ui::streamForm
{
    Q_OBJECT

public:
    explicit StreamWG(QWidget * parent =0);
    ~StreamWG();

    friend class VideoWG;

    void setColorFrame(bool color);
    void setSize(int wd, int ht);
    void enableStream(bool enable);
    bool isStreamEnabled() { return processStream; }
    void newFrame(IBLOB *bp);
    int getWidth() { return streamWidth; }
    int getHeight() { return streamHeight; }


private:
    bool	processStream;
    int     streamWidth, streamHeight;
    VideoWG	*streamFrame;
    bool	colorFrame;
    QIcon   playPix, pausePix, capturePix;

protected:
    void closeEvent ( QCloseEvent * e );
    void resizeEvent(QResizeEvent *ev);


public slots:
    void playPressed();
    void captureImage();

signals:
    void hidden();


};

class VideoWG : public QFrame
{
    Q_OBJECT

public:
    VideoWG(QWidget * parent =0);
    ~VideoWG();

    friend class StreamWG;

   bool newFrame(IBLOB *bp);
   int imageWidth();
   int imageHeight();

private:
    int		totalBaseCount;
    QVector<QRgb>     grayTable;
    QImage		*streamImage;
    QPixmap		 kPix;

protected:
    void paintEvent(QPaintEvent *ev);

};

#endif
