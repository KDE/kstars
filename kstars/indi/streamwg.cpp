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

StreamWG::StreamWG(ISD::CCD *ccd) : QDialog(KStars::Instance())
{
    setupUi(this);
    currentCCD     = ccd;
    streamWidth    = streamHeight = -1;
    processStream  = colorFrame = isRecording = false;

    dirPath     = QUrl::fromLocalFile(QDir::homePath());

    setWindowTitle(i18n("%1 Live Video", ccd->getDeviceName()));

    QString filename, directory;
    ccd->getSERNameDirectory(filename, directory);

    recordFilenameEdit->setText(filename);
    recordDirectoryEdit->setText(directory);

#ifdef Q_OS_OSX
        setWindowFlags(Qt::Tool| Qt::WindowStaysOnTopHint);
#endif

    selectDirB->setIcon(QIcon::fromTheme("document-open-folder", QIcon(":/icons/breeze/default/document-open-folder.svg")));
    connect(selectDirB, SIGNAL(clicked()), this, SLOT(selectRecordDirectory()));

    recordIcon   = QIcon::fromTheme( "media-record", QIcon(":/icons/breeze/default/media-record.svg"));
    stopIcon     = QIcon::fromTheme( "media-playback-stop", QIcon(":/icons/breeze/default/media-playback-stop.svg"));

    recordB->setIcon(recordIcon);

    connect(recordB, SIGNAL(clicked()), this, SLOT(toggleRecord()));
    connect(ccd, SIGNAL(videoRecordToggled(bool)), this, SLOT(updateRecordStatus(bool)));

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

void StreamWG::selectRecordDirectory()
{
    QString dir = QFileDialog::getExistingDirectory(KStars::Instance(), i18n("SER Record Directory"), dirPath.toLocalFile());

    if (dir.isEmpty())
        return;

    recordDirectoryEdit->setText(dir);
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
        //playB->setIcon(pausePix);
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

    videoFrame->setTotalBaseCount(wd * ht);
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
        currentCCD->setSERNameDirectory(recordFilenameEdit->text(), recordDirectoryEdit->text());

        if (recordUntilStoppedR->isChecked())
        {
            isRecording = currentCCD->startRecording();
        }
        else if (recordDurationR->isChecked())
        {
            isRecording = currentCCD->startDurationRecording(durationSpin->value());
        }
        else
        {
            isRecording = currentCCD->startFramesRecording(framesSpin->value());
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
