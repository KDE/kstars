/*
    SPDX-FileCopyrightText: 2017 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "fitsviewer/bayer.h"

#include <indidevapi.h>

#include <QPixmap>
#include <QVector>
#include <QColor>
#include <QLabel>

#include <memory>
#include <mutex>

class QImage;
class QRubberBand;

class VideoWG : public QLabel
{
        Q_OBJECT

    public:
        explicit VideoWG(QWidget *parent = nullptr);
        virtual ~VideoWG() override = default;

        bool newFrame(IBLOB *bp);
        bool newBayerFrame(IBLOB *bp, const BayerParams &params);

        bool save(const QString &filename, const char *format);

        void setSize(uint16_t w, uint16_t h);

    protected:
        //virtual void resizeEvent(QResizeEvent *ev) override;
        void mousePressEvent(QMouseEvent *event) override;
        void mouseMoveEvent(QMouseEvent *event) override;
        void mouseReleaseEvent(QMouseEvent *event) override;

    signals:
        void newSelection(QRect);
        void imageChanged(const QSharedPointer<QImage> &frame);

    private:
        bool debayer(const IBLOB *bp, const BayerParams &params);

        uint16_t streamW { 0 };
        uint16_t streamH { 0 };
        uint32_t totalBaseCount { 0 };
        QVector<QRgb> grayTable;
        QSharedPointer<QImage> streamImage;
        QPixmap kPix;
        QRubberBand *rubberBand { nullptr };
        QPoint origin;
        QString m_RawFormat;
        bool m_RawFormatSupported { false };
};
