/*  Stream Widget
    Copyright (C) 2003 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
    
    2004-03-16: A class to handle video streaming.
 */

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
#include <QDir>
#include <QLayout>
#include <QPaintEvent>
#include <QCloseEvent>
#include <QImageWriter>
#include <QImageReader>
#include <QIcon>

#include <stdlib.h>
#include <fcntl.h>

#include "streamwg.h"
#include "indistd.h"
#include "kstars.h"
#include "Options.h"

StreamWG::StreamWG(QWidget * parent) : QWidget(parent)
{

    setupUi(this);
    streamWidth    = streamHeight = -1;
    processStream  = colorFrame = false;

    playPix    = QIcon::fromTheme( "media-playback-start", QIcon(":/icons/breeze/default/media-playback-start.svg"));
    pausePix   = QIcon::fromTheme( "media-playback-pause", QIcon(":/icons/breeze/default/media-playback-pause.svg"));
    capturePix = QIcon::fromTheme( "media-record", QIcon(":/icons/breeze/default/media-record.svg"));

    foreach (const QByteArray &format, QImageWriter::supportedImageFormats())
    imgFormatCombo->addItem(QString(format));

    playB->setIcon(pausePix);
    captureB->setIcon(capturePix);

    connect(playB, SIGNAL(clicked()), this, SLOT(playPressed()));
    connect(captureB, SIGNAL(clicked()), this, SLOT(captureImage()));

    videoFrame->resize(Options::streamWindowWidth(), Options::streamWindowHeight());
}

StreamWG::~StreamWG()
{
}

void StreamWG::closeEvent ( QCloseEvent * ev )
{
    processStream = false;

    Options::setStreamWindowWidth(videoFrame->width());
    Options::setStreamWindowHeight(videoFrame->height());

    ev->accept();

    emit hidden();
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
    // Initial resize
    if (streamWidth == -1)
        resize(Options::streamWindowWidth() + layout()->margin() * 2,
               Options::streamWindowHeight()+ playB->height() + layout()->margin() * 4 + layout()->spacing());

    streamWidth = wd;
    streamHeight = ht;

    videoFrame->setTotalBaseCount(wd * ht);

    //resize(wd + layout()->margin() * 2 , ht + playB->height() + layout()->margin() * 4 + layout()->spacing());
    //videoFrame->resize(Options::streamWindowWidth(), Options::streamWindowHeight());

    //videoFrame->update();
}

/*void StreamWG::resizeEvent(QResizeEvent *ev)
{
    streamFrame->resize(ev->size().width() - layout()->margin() * 2, ev->size().height() - playB->height() - layout()->margin() * 4 - layout()->spacing());

}*/

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
    bool rc = videoFrame->newFrame(bp);

    if (rc == false)
    {        
        KMessageBox::error(0, i18n("Unable to load video stream."));
        close();
    }
}

void StreamWG::captureImage()
{
    QString fmt;
    QUrl currentFileURL;
    QUrl currentDir(Options::fitsDir());

    fmt = imgFormatCombo->currentText();

    currentFileURL = QFileDialog::getSaveFileUrl(KStars::Instance(), i18n("Save Image"), currentDir, fmt );

    if (currentFileURL.isEmpty()) return;

    if ( currentFileURL.isValid() )
    {
        videoFrame->save(currentFileURL.toLocalFile(), fmt.toLatin1());
    }
    else
    {
        QString message = i18n( "Invalid URL: %1", currentFileURL.url() );
        KMessageBox::sorry( 0, message, i18n( "Invalid URL" ) );
    }
}
