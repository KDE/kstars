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
#include "Options.h"

#include <kmessagebox.h>
#include <klocale.h>
#include <kdebug.h>
#include <kpushbutton.h>
#include <kiconloader.h>
#include <ktemporaryfile.h>
#include <kio/netaccess.h>
#include <kfiledialog.h>
#include <kcombobox.h>
#include <kurl.h>

#include <QRgb>
#include <qsocketnotifier.h>
#include <qimage.h>
#include <qpainter.h>
#include <qstringlist.h>
#include <qdir.h>
#include <qlayout.h>
#include <QResizeEvent>
#include <QPaintEvent>
#include <QCloseEvent>
#include <QByteArray>
#include <QImageWriter>

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

StreamWG::StreamWG(QWidget * parent) : QWidget(parent)
{

    setupUi(this);
    streamWidth    = streamHeight = -1;
    processStream  = colorFrame = false;

    streamFrame      = new VideoWG(videoFrame);

    playPix    = KIcon( "media-playback-start" );
    pausePix   = KIcon( "media-playback-pause" );
    capturePix = KIcon( "media-record" );

    foreach (const QByteArray &format, QImageWriter::supportedImageFormats())
    imgFormatCombo->addItem(QString(format));

    playB->setIcon(pausePix);
    captureB->setIcon(capturePix);

    connect(playB, SIGNAL(clicked()), this, SLOT(playPressed()));
    connect(captureB, SIGNAL(clicked()), this, SLOT(captureImage()));
}

StreamWG::~StreamWG()
{
    //  delete streamBuffer;
}

void StreamWG::closeEvent ( QCloseEvent * e )
{
    processStream = false;
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

void StreamWG::newFrame(unsigned char *buffer, int buffSiz, int w, int h)
{
    streamFrame->newFrame(buffer, buffSiz, w, h);
}

void StreamWG::captureImage()
{
    QString fname;
    QString fmt;
    KUrl currentFileURL;
    QString currentDir = Options::fitsDir();
    KTemporaryFile tmpfile;
    tmpfile.open();

    fmt = imgFormatCombo->currentText();

    currentFileURL = KFileDialog::getSaveUrl( currentDir, fmt );

    if (currentFileURL.isEmpty()) return;

    if ( currentFileURL.isValid() )
    {
        currentDir = currentFileURL.directory();

        if ( currentFileURL.isLocalFile() )
            fname = currentFileURL.path();
        else
            fname = tmpfile.fileName();

        if (fname.right(fmt.length()).toLower() != fmt.toLower())
        {
            fname += '.';
            fname += fmt.toLower();
        }

        streamFrame->kPix.save(fname, fmt.toAscii());

        //set rwx for owner, rx for group, rx for other
        chmod( fname.toAscii(), S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH );

        if ( tmpfile.fileName() == fname )
        { //need to upload to remote location

            if ( ! KIO::NetAccess::upload( tmpfile.fileName(), currentFileURL, (QWidget*) 0 ) )
            {
                QString message = i18n( "Could not upload image to remote location: %1", currentFileURL.prettyUrl() );
                KMessageBox::sorry( 0, message, i18n( "Could not upload file" ) );
            }
        }
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
    streamImage    = NULL;
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

void VideoWG::newFrame(unsigned char *buffer, int buffSiz, int w, int h)
{

    // TODO: This is highly inefficient. Need to be replaced with a direct blit.
    if (buffSiz > totalBaseCount)
        streamImage = new QImage(buffer, w, h, QImage::Format_RGB32);
    else
    {
        streamImage = new QImage(buffer, w, h, QImage::Format_Indexed8);
        streamImage->setColorTable(grayTable);
    }

    update();
}

void VideoWG::paintEvent(QPaintEvent * /*ev*/)
{

    if (streamImage)
    {
        if (streamImage->isNull()) return;
        kPix = QPixmap::fromImage(streamImage->scaled(width(), height(), Qt::KeepAspectRatio));
        delete (streamImage);
        streamImage = NULL;
    }

    QPainter p(this);
    p.drawPixmap(0, 0, kPix);
    p.end();

}

#include "streamwg.moc"
