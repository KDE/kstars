/*  Stream Widget
    Copyright (C) 2003 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
    
    2004-03-16: A class to handle video streaming.
 */

#include "streamwg.h"
#include "indistd.h"
#include "kstars.h"
#include "Options.h"

#include <KMessageBox>
#include <KLocalizedString>

#include <QLocale>
#include <QDebug>
#include <QPushButton>
#include <QFileDialog>
#include <QRgb>
#include <QSocketNotifier>
#include <QImage>
#include <QPainter>
#include <QStringList>
#include <QDir>
#include <QLayout>
#include <QResizeEvent>
#include <QPaintEvent>
#include <QCloseEvent>
#include <QByteArray>
#include <QImageWriter>
#include <QImageReader>
#include <QIcon>
#include <QTemporaryFile>

#include <stdlib.h>
#include <fcntl.h>

StreamWG::StreamWG(QWidget * parent) : QWidget(parent)
{

    setupUi(this);
    streamWidth    = streamHeight = -1;
    processStream  = colorFrame = false;

    streamFrame      = new VideoWG(videoFrame);

    playPix    = QIcon::fromTheme( "media-playback-start" );
    pausePix   = QIcon::fromTheme( "media-playback-pause" );
    capturePix = QIcon::fromTheme( "media-record" );

    foreach (const QByteArray &format, QImageWriter::supportedImageFormats())
    imgFormatCombo->addItem(QString(format));

    playB->setIcon(pausePix);
    captureB->setIcon(capturePix);

    connect(playB, SIGNAL(clicked()), this, SLOT(playPressed()));
    connect(captureB, SIGNAL(clicked()), this, SLOT(captureImage()));
}

StreamWG::~StreamWG()
{
   delete streamFrame;
}

void StreamWG::closeEvent ( QCloseEvent * e )
{
    processStream = false;
    emit hidden();
    e->accept();
}

void StreamWG::setColorFrame(bool color)
{
    colorFrame = color;
}

void StreamWG::enableStream(bool enable)
{
    if (enable)
    {
        processStream = true;
        show();
    }
    else
    {
        processStream = false;
        playB->setIcon(pausePix);
        hide();
    }

}

void StreamWG::setSize(int wd, int ht)
{

    streamWidth  = wd;
    streamHeight = ht;

    streamFrame->totalBaseCount = wd * ht;

    resize(wd + layout()->margin() * 2 , ht + playB->height() + layout()->margin() * 4 + layout()->spacing());
    streamFrame->resize(wd, ht);
}

void StreamWG::resizeEvent(QResizeEvent *ev)
{
    streamFrame->resize(ev->size().width() - layout()->margin() * 2, ev->size().height() - playB->height() - layout()->margin() * 4 - layout()->spacing());

}

void StreamWG::playPressed()
{

    if (processStream)
    {
        playB->setIcon(playPix);
        processStream = false;
    }
    else
    {
        playB->setIcon(pausePix);
        processStream = true;
    }

}

void StreamWG::newFrame(IBLOB *bp)
{
    bool rc = streamFrame->newFrame(bp);

    if (rc && streamWidth == -1)
        setSize(streamFrame->imageWidth(), streamFrame->imageHeight());
    else if (rc == false)
    {
        close();
        KMessageBox::error(0, i18n("Unable to load video stream."));
    }
}

void StreamWG::captureImage()
{
    QString fmt;
    QUrl currentFileURL;
    QUrl currentDir(Options::fitsDir());
    QTemporaryFile tmpfile;
    tmpfile.open();

    fmt = imgFormatCombo->currentText();

    currentFileURL = QFileDialog::getSaveFileUrl(KStars::Instance(), i18n("Save Image"), currentDir, fmt );

    if (currentFileURL.isEmpty()) return;

    if ( currentFileURL.isValid() )
    {
        streamFrame->kPix.save(currentFileURL.path(), fmt.toLatin1());        
    }
    else
    {
        QString message = i18n( "Invalid URL: %1", currentFileURL.url() );
        KMessageBox::sorry( 0, message, i18n( "Invalid URL" ) );
    }

}


VideoWG::VideoWG(QWidget * parent) : QFrame(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent);
    streamImage    = new QImage();
    //grayTable=new QRgb[256];
    grayTable.resize(256);
    for (int i=0;i<256;i++)
        grayTable[i]=qRgb(i,i,i);
}

VideoWG::~VideoWG()
{
    delete (streamImage);
    //delete [] (grayTable);
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
        update();

    return rc;
}

void VideoWG::paintEvent(QPaintEvent * /*ev*/)
{
    if (streamImage && !streamImage->isNull())
    {
        kPix = QPixmap::fromImage(streamImage->scaled(width(), height()));

        QPainter p(this);
        p.drawPixmap(0, 0, kPix);
        p.end();
    }
}

int VideoWG::imageWidth()
{
    return streamImage->width();
}

int VideoWG::imageHeight()
{
    return streamImage->height();
}


