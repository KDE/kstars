/*
    SPDX-FileCopyrightText: 2003-2017 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "indi/indiccd.h"
#include "ui_streamform.h"
#include "ui_recordingoptions.h"
#include "fitsviewer/bayer.h"
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
        bool isStreamEnabled()
        {
            return processStream;
        }

        void newFrame(IBLOB *bp);

        int getStreamWidth()
        {
            return streamWidth;
        }
        int getStreamHeight()
        {
            return streamHeight;
        }

    protected:
        void closeEvent(QCloseEvent *ev) override;
        void showEvent(QShowEvent *ev) override;
        QSize sizeHint() const override;

    public slots:
        void toggleRecord();
        void updateRecordStatus(bool enabled);
        void resetFrame();
        void syncDebayerParameters();

    protected slots:
        void setStreamingFrame(QRect newFrame);
        void updateFPS(double instantFPS, double averageFPS);

    signals:
        void hidden();
        void imageChanged(const QSharedPointer<QImage> &frame);

    private:
        bool queryDebayerParameters();

        bool processStream;
        int streamWidth, streamHeight;
        bool colorFrame, isRecording;
        QIcon recordIcon, stopIcon;
        ISD::CCD *currentCCD {nullptr};

        // Debayer
        BayerParams m_DebayerParams;
        uint8_t m_BBP {8};
        uint16_t offsetX, offsetY;
        double pixelX, pixelY;
        bool m_DebayerActive { false }, m_DebayerSupported { false };

        // For Canon DSLRs
        INDI::Property *eoszoom {nullptr}, *eoszoomposition {nullptr};
        RecordOptions *options;
};
