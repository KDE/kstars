/*  Stream Widget
    Copyright (C) 2003-2017 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
        
*/

#ifndef STREAMWG_H
#define STREAMWG_H

#include <QPixmap>
#include <QVBoxLayout>
#include <QResizeEvent>
#include <QPaintEvent>
#include <QCloseEvent>
#include <QVector>
#include <QColor>
#include <QIcon>
#include <QImage>

#include <indidevapi.h>

#include "ui_streamform.h"

class StreamWG : public QWidget, public Ui::streamForm
{
    Q_OBJECT

public:
    explicit StreamWG(QWidget * parent =0);
    ~StreamWG();

    void setColorFrame(bool color);
    void setSize(int wd, int ht);

    void enableStream(bool enable);    
    bool isStreamEnabled() { return processStream; }

    void newFrame(IBLOB *bp);

    int getStreamWidth() { return streamWidth; }
    int getStreamHeight() { return streamHeight; }

private:
    bool	processStream;
    int     streamWidth, streamHeight;
    bool	colorFrame;
    QIcon   playPix, pausePix, capturePix;

protected:
    void closeEvent ( QCloseEvent * ev );
    //void resizeEvent( QResizeEvent *ev );

public slots:
    void playPressed();
    void captureImage();

signals:
    void hidden();
};

#endif
