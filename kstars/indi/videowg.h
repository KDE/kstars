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
#include <QSqlDatabase>
#include <QPen>
#include <QPainter>

#include <memory>
#include <mutex>

class QImage;
class QRubberBand;
class QSqlTableModel;

class VideoWG : public QLabel
{
        Q_OBJECT

    public:
        explicit VideoWG(QWidget *parent = nullptr);
        virtual ~VideoWG() override;

        bool newFrame(IBLOB *bp);
        bool newBayerFrame(IBLOB *bp, const BayerParams &params);

        bool save(const QString &filename, const char *format);

        void setSize(uint16_t w, uint16_t h);

    protected:
        //virtual void resizeEvent(QResizeEvent *ev) override;
        void mousePressEvent(QMouseEvent *event) override;
        void mouseMoveEvent(QMouseEvent *event) override;
        void mouseReleaseEvent(QMouseEvent *event) override;
        void initOverlayModel();

    public slots:
        void modelChanged();
        void toggleOverlay();

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

        // Collimation Overlay
        void setupOverlay();
        void paintOverlay(QPixmap &imagePix);
        bool overlayEnabled = false;
        QSqlTableModel *m_CollimationOverlayElementsModel = { nullptr };
        QList<QVariantMap> m_CollimationOverlayElements;
        QList<QVariantMap> m_EnabledOverlayElements;
        QVariantMap *m_CurrentElement = nullptr;
        QStringList *typeValues = nullptr;
        QPainter *painter = nullptr;
        float scale;
        void PaintOneItem (QString type, QPointF position, int sizeX, int sizeY, int thickness);
};
