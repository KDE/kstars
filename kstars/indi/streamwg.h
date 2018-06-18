/*  Stream Widget
    Copyright (C) 2003-2017 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

*/

#pragma once

#include "indi/indiccd.h"
#include "ui_streamform.h"
#include "ui_recordingoptions.h"

#include <indidevapi.h>

#include <QCloseEvent>
#include <QColor>
#include <QIcon>
#include <QImage>
#include <QPaintEvent>
#include <QPixmap>
#include <QResizeEvent>
#include <QVBoxLayout>
#include <QVector>

class RecordOptions : public QDialog, public Ui::recordingOptions
{
    Q_OBJECT

  public:
    explicit RecordOptions(QWidget *parent);

  public slots:
    void selectRecordDirectory();

  private:
    QUrl dirPath;

    friend class StreamWG;
};

class StreamWG : public QDialog, public Ui::streamForm
{
    Q_OBJECT

  public:
    explicit StreamWG(ISD::CCD *ccd);
    virtual ~StreamWG() override = default;

    void setColorFrame(bool color);
    void setSize(int wd, int ht);

    void enableStream(bool enable);
    bool isStreamEnabled() { return processStream; }

    void newFrame(IBLOB *bp);

    int getStreamWidth() { return streamWidth; }
    int getStreamHeight() { return streamHeight; }

  protected:
    void closeEvent(QCloseEvent *ev) override;
    QSize sizeHint() const override;

  public slots:
    void toggleRecord();
    void updateRecordStatus(bool enabled);
    void resetFrame();

  protected slots:
    void setStreamingFrame(QRect newFrame);
    void updateFPS(double instantFPS, double averageFPS);

  signals:
    void hidden();
    void imageChanged(std::unique_ptr<QImage> & frame);

  private:
    bool processStream;
    int streamWidth, streamHeight;
    bool colorFrame, isRecording;
    QIcon recordIcon, stopIcon;
    ISD::CCD *currentCCD;

    RecordOptions *options;
};
