/*
    SPDX-FileCopyrightText: 2003 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later

    2004-03-16: A class to handle video streaming.
*/

#include "streamwg.h"

#include "kstars.h"
#include "Options.h"
#include "kstars_debug.h"
#include "collimationoverlayoptions.h"
#include "qobjectdefs.h"

#include <basedevice.h>

#include <KLocalizedString>
#include <KMessageBox>

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
#include <QTimer>

#include <cstdlib>
#include <fcntl.h>

RecordOptions::RecordOptions(QWidget *parent) : QDialog(parent)
{
#ifdef Q_OS_OSX
    setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint);
#endif

    setupUi(this);

    dirPath = QUrl::fromLocalFile(QDir::homePath());

    selectDirB->setIcon(
        QIcon::fromTheme("document-open-folder"));
    connect(selectDirB, SIGNAL(clicked()), this, SLOT(selectRecordDirectory()));
}

void RecordOptions::selectRecordDirectory()
{
    QString dir =
        QFileDialog::getExistingDirectory(KStars::Instance(), i18nc("@title:window", "SER Record Directory"),
                                          dirPath.toLocalFile());

    if (dir.isEmpty())
        return;

    recordDirectoryEdit->setText(dir);
}

StreamWG::StreamWG(ISD::Camera *ccd) : QDialog(KStars::Instance())
{
    setupUi(this);
    m_Camera  = ccd;
    streamWidth = streamHeight = -1;
    processStream = colorFrame = isRecording = false;

    options = new RecordOptions(this);
    connect(optionsB, SIGNAL(clicked()), options, SLOT(show()));

    collimationOptionsB->setIcon(QIcon::fromTheme("run-build-prune"));
    connect(collimationOptionsB, &QPushButton::clicked, this, [this]()
            {
                CollimationOverlayOptions::Instance(this)->openEditor();
            });

    collimationB->setIcon(QIcon::fromTheme("crosshairs"));
    connect(CollimationOverlayOptions::Instance(this), SIGNAL(updated()), videoFrame, SLOT(modelChanged()));
    connect(collimationB, &QPushButton::clicked, videoFrame, &VideoWG::toggleOverlay);

    QString filename, directory;
    ccd->getSERNameDirectory(filename, directory);

    double duration = 0.1;
    bool hasStreamExposure = m_Camera->getStreamExposure(&duration);
    if (hasStreamExposure)
        targetFrameDurationSpin->setValue(duration);
    else
    {
        targetFrameDurationSpin->setEnabled(false);
        changeFPSB->setEnabled(false);
    }

    options->recordFilenameEdit->setText(filename);
    options->recordDirectoryEdit->setText(directory);

    setWindowTitle(i18nc("@title:window", "%1 Live Video", ccd->getDeviceName()));

#if defined(Q_OS_OSX)
    setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint);
#else
    setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);
#endif
    recordIcon = QIcon::fromTheme("media-record");
    stopIcon   = QIcon::fromTheme("media-playback-stop");

    optionsB->setIcon(QIcon::fromTheme("run-build"));
    resetFrameB->setIcon(QIcon::fromTheme("view-restore"));

    connect(resetFrameB, SIGNAL(clicked()), this, SLOT(resetFrame()));

    recordB->setIcon(recordIcon);

    connect(recordB, SIGNAL(clicked()), this, SLOT(toggleRecord()));
    connect(ccd, SIGNAL(videoRecordToggled(bool)), this, SLOT(updateRecordStatus(bool)));

    connect(videoFrame, &VideoWG::newSelection, this, &StreamWG::setStreamingFrame);
    connect(videoFrame, &VideoWG::imageChanged, this, &StreamWG::imageChanged);

    resize(Options::streamWindowWidth(), Options::streamWindowHeight());

    eoszoom = m_Camera->getProperty("eoszoom");
    if (eoszoom == nullptr)
    {
        zoomLevelCombo->hide();
    }
    else
    {
        connect(zoomLevelCombo, static_cast<void(QComboBox::*)(int)>(&QComboBox::activated), [&]()
        {
            auto tvp = eoszoom->getText();
            QString zoomLevel = zoomLevelCombo->currentText().remove("x");
            tvp->at(0)->setText(zoomLevel.toLatin1().constData());
            handLabel->setEnabled(true);
            NSSlider->setEnabled(true);
            WESlider->setEnabled(true);
            // Set it twice!
            m_Camera->sendNewProperty(tvp);
            QTimer::singleShot(1000, this, [ &, tvp]()
            {
                m_Camera->sendNewProperty(tvp);
            });

        });
    }

    eoszoomposition = m_Camera->getProperty("eoszoomposition");
    if (eoszoomposition == nullptr)
    {
        handLabel->hide();
        NSSlider->hide();
        WESlider->hide();

        horizontalSpacer->changeSize(1, 1, QSizePolicy::Expanding);
    }
    else
    {
        connect(NSSlider, &QSlider::sliderReleased, [&]()
        {
            auto tvp = eoszoomposition->getText();
            QString pos = QString("%1,%2").arg(WESlider->value()).arg(NSSlider->value());
            tvp->at(0)->setText(pos.toLatin1().constData());
            m_Camera->sendNewProperty(tvp);
        });

        connect(WESlider, &QSlider::sliderReleased, [&]()
        {
            auto tvp = eoszoomposition->getText();
            QString pos = QString("%1,%2").arg(WESlider->value()).arg(NSSlider->value());
            tvp->at(0)->setText(pos.toLatin1().constData());
            m_Camera->sendNewProperty(tvp);
        });

        horizontalSpacer->changeSize(1, 1, QSizePolicy::Preferred);
    }

    connect(m_Camera, &ISD::Camera::newFPS, this, &StreamWG::updateFPS);
    connect(m_Camera, &ISD::Camera::propertyUpdated, this, [this](INDI::Property prop)
    {
        if (prop.isNameMatch("CCD_INFO") || prop.isNameMatch("CCD_CFA"))
            syncDebayerParameters();
    });
    connect(changeFPSB, &QPushButton::clicked, this, [&]()
    {
        if (m_Camera)
        {
            m_Camera->setStreamExposure(targetFrameDurationSpin->value());
            m_Camera->setVideoStreamEnabled(false);
            QTimer::singleShot(1000, this, [&]()
            {
                m_Camera->setVideoStreamEnabled(true);
            });
        }
    });

    debayerB->setIcon(QIcon(":/icons/cfa.svg"));
    connect(debayerB, &QPushButton::clicked, this, [this]()
    {
        m_DebayerActive = !m_DebayerActive;
    });
    syncDebayerParameters();
}

void StreamWG::syncDebayerParameters()
{
    m_DebayerSupported = queryDebayerParameters();
    debayerB->setEnabled(m_DebayerSupported);
    m_DebayerActive = m_DebayerSupported;
}

bool StreamWG::queryDebayerParameters()
{
    if (!m_Camera)
        return false;

    ISD::CameraChip *targetChip = m_Camera->getChip(ISD::CameraChip::PRIMARY_CCD);
    if (!targetChip)
        return false;

    // DSLRs always send motion JPGs when streaming so
    // bayered images are not streamed.
    if (targetChip->getISOList().isEmpty() == false)
        return false;

    uint16_t w, h;
    QString pattern;

    if (targetChip->getImageInfo(w, h, pixelX, pixelY, m_BBP) == false)
        return false;

    // Limit only to 8 and 16 bit, nothing in between or less or more.
    if (m_BBP > 8)
        m_BBP = 16;
    else
        m_BBP = 8;

    if (targetChip->getBayerInfo(offsetX, offsetY, pattern) == false)
        return false;

    m_DebayerParams.method = DC1394_BAYER_METHOD_NEAREST;
    m_DebayerParams.filter = DC1394_COLOR_FILTER_RGGB;

    if (pattern == "GBRG")
        m_DebayerParams.filter = DC1394_COLOR_FILTER_GBRG;
    else if (pattern == "GRBG")
        m_DebayerParams.filter = DC1394_COLOR_FILTER_GRBG;
    else if (pattern == "BGGR")
        m_DebayerParams.filter = DC1394_COLOR_FILTER_BGGR;

    return true;
}

QSize StreamWG::sizeHint() const
{
    QSize size(Options::streamWindowWidth(), Options::streamWindowHeight());
    return size;
}

void StreamWG::showEvent(QShowEvent *ev)
{
    if (m_Camera)
    {
        // Always reset to 1x for DSLRs since they reset whenever they are triggered again.
        if (eoszoom)
            zoomLevelCombo->setCurrentIndex(0);
    }

    ev->accept();
}

void StreamWG::closeEvent(QCloseEvent * ev)
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
        //instFPS->setText("--");
        avgFPS->setText("--");
        hide();
    }
}

void StreamWG::setSize(int wd, int ht)
{
    if (wd != streamWidth || ht != streamHeight)
    {
        streamWidth  = wd;
        streamHeight = ht;

        NSSlider->setMaximum(ht);
        NSSlider->setSingleStep(ht / 30);
        WESlider->setMaximum(wd);
        WESlider->setSingleStep(wd / 30);

        videoFrame->setSize(wd, ht);
    }
}

/*void StreamWG::resizeEvent(QResizeEvent *ev)
{
    streamFrame->resize(ev->size().width() - layout()->margin() * 2, ev->size().height() - playB->height() - layout()->margin() * 4 - layout()->spacing());

}*/

void StreamWG::updateRecordStatus(bool enabled)
{
    if ((enabled && isRecording) || (!enabled && !isRecording))
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

        m_Camera->stopRecording();
    }
    else
    {
        QString directory, filename;
        m_Camera->getSERNameDirectory(filename, directory);
        if (filename != options->recordFilenameEdit->text() ||
                directory != options->recordDirectoryEdit->text())
        {
            m_Camera->setSERNameDirectory(options->recordFilenameEdit->text(), options->recordDirectoryEdit->text());
            // Save config in INDI so the filename and directory templates are reloaded next time
            m_Camera->setConfig(SAVE_CONFIG);
        }

        if (options->recordUntilStoppedR->isChecked())
        {
            isRecording = m_Camera->startRecording();
        }
        else if (options->recordDurationR->isChecked())
        {
            isRecording = m_Camera->startDurationRecording(options->durationSpin->value());
        }
        else
        {
            isRecording = m_Camera->startFramesRecording(options->framesSpin->value());
        }

        if (isRecording)
        {
            recordB->setIcon(stopIcon);
            recordB->setToolTip(i18n("Stop recording"));
        }
    }
}

void StreamWG::newFrame(INDI::Property prop)
{
    auto bp = prop.getBLOB()->at(0);

    bool rc = (m_DebayerActive
               && !strcmp(bp->getFormat(), ".stream")) ? videoFrame->newBayerFrame(bp, m_DebayerParams) : videoFrame->newFrame(bp);

    if (rc == false)
        qCWarning(KSTARS) << "Failed to load video frame.";
}

void StreamWG::resetFrame()
{
    m_Camera->resetStreamingFrame();
}

void StreamWG::setStreamingFrame(QRect newFrame)
{
    if (newFrame.isNull())
    {
        resetFrame();
        return;
    }

    if (newFrame.width() < 5 || newFrame.height() < 5)
        return;

    int w = newFrame.width();
    // Must be divisable by 4
    while (w % 4)
    {
        w++;
    }

    m_Camera->setStreamingFrame(newFrame.x(), newFrame.y(), w, newFrame.height());
}

void StreamWG::updateFPS(double instantFPS, double averageFPS)
{
    Q_UNUSED(instantFPS)
    //instFPS->setText(QString::number(instantFPS, 'f', 1));
    avgFPS->setText(QString::number(averageFPS, 'f', 1));
}

StreamWG::~StreamWG()
{
    CollimationOverlayOptions::Instance(this)->release();
}
