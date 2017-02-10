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

RecordOptions::RecordOptions(QWidget *parent) : QDialog(parent)
{
    setupUi(this);

    dirPath     = QUrl::fromLocalFile(QDir::homePath());

    selectDirB->setIcon(QIcon::fromTheme("document-open-folder", QIcon(":/icons/breeze/default/document-open-folder.svg")));
    connect(selectDirB, SIGNAL(clicked()), this, SLOT(selectRecordDirectory()));
}

void RecordOptions::selectRecordDirectory()
{
    QString dir = QFileDialog::getExistingDirectory(KStars::Instance(), i18n("SER Record Directory"), dirPath.toLocalFile());

    if (dir.isEmpty())
        return;

    recordDirectoryEdit->setText(dir);
}

StreamWG::StreamWG(ISD::CCD *ccd) : QDialog(KStars::Instance())
{
    setupUi(this);
    currentCCD     = ccd;
    streamWidth    = streamHeight = -1;
    processStream  = colorFrame = isRecording = false;

    options = new RecordOptions(this);

    connect(optionsB, SIGNAL(clicked()), options, SLOT(show()));

    QString filename, directory;
    ccd->getSERNameDirectory(filename, directory);

    options->recordFilenameEdit->setText(filename);
    options->recordDirectoryEdit->setText(directory);

    setWindowTitle(i18n("%1 Live Video", ccd->getDeviceName()));    

#ifdef Q_OS_OSX
        setWindowFlags(Qt::Tool| Qt::WindowStaysOnTopHint);
#endif    
    recordIcon   = QIcon::fromTheme( "media-record", QIcon(":/icons/breeze/default/media-record.svg"));
    stopIcon     = QIcon::fromTheme( "media-playback-stop", QIcon(":/icons/breeze/default/media-playback-stop.svg"));

    optionsB->setIcon(QIcon::fromTheme( "run-build", QIcon(":/icons/breeze/default/run-build.svg")));
    resetFrameB->setIcon(QIcon::fromTheme( "view-restore", QIcon(":/icons/breeze/default/view-restore.svg")));

    connect(resetFrameB, SIGNAL(clicked()), this, SLOT(resetFrame()));

    recordB->setIcon(recordIcon);

    connect(recordB, SIGNAL(clicked()), this, SLOT(toggleRecord()));
    connect(ccd, SIGNAL(videoRecordToggled(bool)), this, SLOT(updateRecordStatus(bool)));

    connect(videoFrame, SIGNAL(newSelection(QRect)), this, SLOT(setStreamingFrame(QRect)));

    resize(Options::streamWindowWidth(), Options::streamWindowHeight());
}

StreamWG::~StreamWG()
{
}

QSize StreamWG::sizeHint() const
{
    QSize size(Options::streamWindowWidth(), Options::streamWindowHeight());
    return size;
}

void StreamWG::closeEvent ( QCloseEvent * ev )
{
    processStream = false;

    Options::setStreamWindowWidth(width());
    Options::setStreamWindowHeight(height());

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
        hide();
    }
}

void StreamWG::setSize(int wd, int ht)
{
    // Initial resize
    /*if (streamWidth == -1)
        resize(Options::streamWindowWidth() + layout()->margin() * 2,
               Options::streamWindowHeight()+ playB->height() + layout()->margin() * 4 + layout()->spacing());*/

    streamWidth = wd;
    streamHeight = ht;

    videoFrame->setSize(wd, ht);
}

/*void StreamWG::resizeEvent(QResizeEvent *ev)
{
    streamFrame->resize(ev->size().width() - layout()->margin() * 2, ev->size().height() - playB->height() - layout()->margin() * 4 - layout()->spacing());

}*/

void StreamWG::updateRecordStatus(bool enabled)
{
    if ( (enabled && isRecording) || (!enabled && !isRecording))
        return;

    isRecording = enabled;

    if (isRecording)
    {
        recordB->setIcon(stopIcon);
        recordB->setToolTip(i18n("Stop recording"));
    }
    else
    {
        recordB->setIcon(recordIcon);
        recordB->setToolTip(i18n("Start recording"));
    }
}

void StreamWG::toggleRecord()
{
    if (isRecording)
    {
        recordB->setIcon(recordIcon);
        isRecording = false;
        recordB->setToolTip(i18n("Start recording"));

        currentCCD->stopRecording();
    }
    else
    {
        currentCCD->setSERNameDirectory(options->recordFilenameEdit->text(), options->recordDirectoryEdit->text());

        if (options->recordUntilStoppedR->isChecked())
        {
            isRecording = currentCCD->startRecording();
        }
        else if (options->recordDurationR->isChecked())
        {
            isRecording = currentCCD->startDurationRecording(options->durationSpin->value());
        }
        else
        {
            isRecording = currentCCD->startFramesRecording(options->framesSpin->value());
        }

        if (isRecording)
        {
            recordB->setIcon(stopIcon);
            recordB->setToolTip(i18n("Stop recording"));
        }
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

void StreamWG::resetFrame()
{
   currentCCD->resetStreamingFrame();
}

void StreamWG::setStreamingFrame(QRect newFrame)
{
    int w = newFrame.width();
    // Must be divisable by 4
    while (w % 4)
    {
        w++;
    }

    currentCCD->setStreamingFrame(newFrame.x(), newFrame.y(), w, newFrame.height());
}
