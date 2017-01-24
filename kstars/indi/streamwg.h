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

#include "indi/indiccd.h"

class StreamWG : public QDialog, public Ui::streamForm
{
    Q_OBJECT

public:
    explicit StreamWG(ISD::CCD *ccd);
    ~StreamWG();

    void setColorFrame(bool color);
    void setSize(int wd, int ht);

    void enableStream(bool enable);    
    bool isStreamEnabled() { return processStream; }

    void newFrame(IBLOB *bp);

    int getStreamWidth() { return streamWidth; }
    int getStreamHeight() { return streamHeight; }

protected:
    void closeEvent ( QCloseEvent * ev );

public slots:
    void toggleRecord();
    void updateRecordStatus(bool enabled);
    void selectRecordDirectory();

signals:
    void hidden();

private:
    bool	processStream;
    int     streamWidth, streamHeight;
    bool	colorFrame, isRecording;
    QIcon   recordIcon, stopIcon;
    ISD::CCD *currentCCD;
    QUrl dirPath;

};

#endif
