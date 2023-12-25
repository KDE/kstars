/*
    SPDX-FileCopyrightText: 2017 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "videowg.h"
#include "collimationoverlaytypes.h"

#include "kstars_debug.h"
#include "kstarsdata.h"
#include "kstars.h"

#include <QImageReader>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QRubberBand>
#include <QSqlTableModel>
#include <QSqlRecord>
#include <QtMath>

VideoWG::VideoWG(QWidget *parent) : QLabel(parent)
{
    streamImage.reset(new QImage());

    grayTable.resize(256);

    for (int i = 0; i < 256; i++)
        grayTable[i] = qRgb(i, i, i);
}

bool VideoWG::newBayerFrame(IBLOB *bp, const BayerParams &params)
{
    return debayer(bp, params);
}

bool VideoWG::newFrame(IBLOB *bp)
{
    if (bp->size <= 0)
        return false;

    bool rc = false;
    QString format(bp->format);
    if (m_RawFormat != format)
    {
        format.remove('.');
        format.remove("stream_");
        m_RawFormatSupported = QImageReader::supportedImageFormats().contains(format.toLatin1());
        m_RawFormat = format;
    }

    if (m_RawFormatSupported)
        rc = streamImage->loadFromData(static_cast<uchar *>(bp->blob), bp->size);
    else if (static_cast<uint32_t>(bp->size) == totalBaseCount)
    {
        streamImage.reset(new QImage(static_cast<uchar *>(bp->blob), streamW, streamH, QImage::Format_Indexed8));
        streamImage->setColorTable(grayTable);
        rc = !streamImage->isNull();
    }
    else if (static_cast<uint32_t>(bp->size) == totalBaseCount * 3)
    {
        streamImage.reset(new QImage(static_cast<uchar *>(bp->blob), streamW, streamH, QImage::Format_RGB888));
        rc = !streamImage->isNull();
    }

    if (rc)
    {
        kPix = QPixmap::fromImage(streamImage->scaled(size(), Qt::KeepAspectRatio));

        paintOverlay(kPix);

        setPixmap(kPix);
    }

    emit imageChanged(streamImage);

    return rc;
}

bool VideoWG::save(const QString &filename, const char *format)
{
    return kPix.save(filename, format);
}

void VideoWG::setSize(uint16_t w, uint16_t h)
{
    streamW        = w;
    streamH        = h;
    totalBaseCount = w * h;
}

//void VideoWG::resizeEvent(QResizeEvent *ev)
//{
//    setPixmap(QPixmap::fromImage(streamImage->scaled(ev->size(), Qt::KeepAspectRatio)));
//    ev->accept();
//}

void VideoWG::mousePressEvent(QMouseEvent *event)
{
    origin = event->pos();
    if (!rubberBand)
        rubberBand = new QRubberBand(QRubberBand::Rectangle, this);
    rubberBand->setGeometry(QRect(origin, QSize()));
    rubberBand->show();
}

void VideoWG::mouseMoveEvent(QMouseEvent *event)
{
    rubberBand->setGeometry(QRect(origin, event->pos()).normalized());
}

void VideoWG::mouseReleaseEvent(QMouseEvent *event)
{
    rubberBand->hide();

    if (event->button() == Qt::RightButton)
    {
        emit newSelection(QRect());
        return;
    }

    QRect rawSelection = rubberBand->geometry();
    int pixmapX        = (width() - kPix.width()) / 2;
    int pixmapY        = (height() - kPix.height()) / 2;

    QRect finalSelection;

    double scaleX = static_cast<double>(streamImage->width()) / kPix.width();
    double scaleY = static_cast<double>(streamImage->height()) / kPix.height();

    finalSelection.setX((rawSelection.x() - pixmapX) * scaleX);
    finalSelection.setY((rawSelection.y() - pixmapY) * scaleY);
    finalSelection.setWidth(rawSelection.width() * scaleX);
    finalSelection.setHeight(rawSelection.height() * scaleY);

    emit newSelection(finalSelection);
    // determine selection, for example using QRect::intersects()
    // and QRect::contains().
}

bool VideoWG::debayer(const IBLOB *bp, const BayerParams &params)
{
    uint32_t rgb_size = streamW * streamH * 3;
    auto * destinationBuffer = new uint8_t[rgb_size];

    if (destinationBuffer == nullptr)
    {
        qCCritical(KSTARS) << "Unable to allocate memory for temporary bayer buffer.";
        return false;
    }

    int ds1394_height = streamH;

    uint8_t * dc1394_source = reinterpret_cast<uint8_t*>(bp->blob);
    if (params.offsetY == 1)
    {
        dc1394_source += streamW;
        ds1394_height--;
    }
    if (params.offsetX == 1)
    {
        dc1394_source++;
    }
    dc1394error_t error_code = dc1394_bayer_decoding_8bit(dc1394_source, destinationBuffer, streamW, ds1394_height,
                               params.filter, params.method);

    if (error_code != DC1394_SUCCESS)
    {
        qCCritical(KSTARS) << "Debayer failed" << error_code;
        delete[] destinationBuffer;
        return false;
    }

    streamImage.reset(new QImage(destinationBuffer, streamW, streamH, QImage::Format_RGB888));
    bool rc = !streamImage->isNull();

    if (rc)
    {
        kPix = QPixmap::fromImage(streamImage->scaled(size(), Qt::KeepAspectRatio));

        paintOverlay(kPix);

        setPixmap(kPix);
    }

    emit imageChanged(streamImage);

    delete[] destinationBuffer;
    return rc;
}

void VideoWG::paintOverlay(QPixmap &imagePix)
{
    if (!overlayEnabled || m_EnabledOverlayElements.count() == 0) return;

    // Anchor - default to center of image
    QPointF m_anchor (static_cast<float>(kPix.width() / 2), static_cast<float>(kPix.height()/2));
    scale = (static_cast<float>(kPix.width()) / static_cast<float>(streamW));

    // Apply any offset from (only) the first enabled anchor element
    bool foundAnchor = false;
    for (auto &oneElement : m_EnabledOverlayElements) {
        if (oneElement["Type"] == "Anchor" && !foundAnchor) {
            m_anchor.setX(m_anchor.x() + oneElement["OffsetX"].toInt());
            m_anchor.setY(m_anchor.y() + oneElement["OffsetY"].toInt());
            foundAnchor = true;
        }
    }

    painter->begin(&imagePix);
    painter->translate(m_anchor);
    painter->scale(scale, scale);

    for (auto &currentElement : m_EnabledOverlayElements) {

        painter->save();
        QPen m_pen = QPen(QColor(currentElement["Colour"].toString()));
        m_pen.setWidth(currentElement["Thickness"].toUInt());
        m_pen.setCapStyle(Qt::FlatCap);
        m_pen.setJoinStyle(Qt::MiterJoin);
        painter->setPen(m_pen);
        painter->translate(currentElement["OffsetX"].toFloat(), currentElement["OffsetY"].toFloat());

        int m_count = currentElement["Count"].toUInt();
        float m_pcd = currentElement["PCD"].toFloat();

        if (m_count == 1) {
            PaintOneItem(currentElement["Type"].toString(), QPointF(0.0, 0.0), currentElement["SizeX"].toUInt(), currentElement["SizeY"].toUInt(), currentElement["Thickness"].toUInt());
        } else if (m_count > 1) {
            float slice = 360 / m_count;
            for (int i = 0; i < m_count; i++) {
                painter->save();
                painter->rotate((slice * i) + currentElement["Rotation"].toFloat());
                PaintOneItem(currentElement["Type"].toString(), QPointF((m_pcd / 2), 0.0), currentElement["SizeX"].toUInt(), currentElement["SizeY"].toUInt(), currentElement["Thickness"].toUInt());
                painter->restore();
            }
        }

        painter->restore();
     }
    painter->end();
}

void VideoWG::setupOverlay ()
{
    if (overlayEnabled) {
        initOverlayModel();

        typeValues = new QStringList;
        collimationoverlaytype m_types;
        const QMetaObject *m_metaobject = m_types.metaObject();
        QMetaEnum m_metaEnum = m_metaobject->enumerator(m_metaobject->indexOfEnumerator("Types"));
        for (int i = 0; i < m_metaEnum.keyCount(); i++) {
            *typeValues << tr(m_metaEnum.key(i));
        }

        painter = new QPainter;
    }
}

void VideoWG::initOverlayModel()
{
    m_CollimationOverlayElements.clear();
    auto userdb = QSqlDatabase::database(KStarsData::Instance()->userdb()->connectionName());
    m_CollimationOverlayElementsModel = new QSqlTableModel(this, userdb);
    modelChanged();
}

void VideoWG::modelChanged()
{
    m_CollimationOverlayElements.clear();
    m_EnabledOverlayElements.clear();
    KStars::Instance()->data()->userdb()->GetCollimationOverlayElements(m_CollimationOverlayElements);
    for (auto &oneElement : m_CollimationOverlayElements)
        if (oneElement["Enabled"] == Qt::Checked)
            m_EnabledOverlayElements.append(oneElement);
}

void VideoWG::PaintOneItem (QString type, QPointF position, int sizeX, int sizeY, int thickness)
{
    float m_sizeX = sizeX - (thickness / 2);
    float m_sizeY = sizeY - (thickness / 2);

    switch (typeValues->indexOf(type)) {
case 0: // Anchor - ignore as we're not drawing it
    break;

case 1: // Ellipse
    painter->drawEllipse(position, m_sizeX, m_sizeY);
    break;

case 2: // Rectangle
{
    QRect m_rect((position.x() - (m_sizeX / 2)), (position.y() - (m_sizeY / 2)), (m_sizeX - (thickness / 2)), (m_sizeY - (thickness / 2)));
    painter->drawRect(m_rect);
    break;
}

case 3: // Line
    painter->drawLine(position.x(), position.y(), sizeX, sizeY);
    break;

default:
    break;
    };
}

void VideoWG::toggleOverlay()
{
    if (overlayEnabled == false) {
        overlayEnabled = true;
        if (m_CollimationOverlayElementsModel == nullptr) {
            setupOverlay();
        }
    } else if (overlayEnabled == true) {
        overlayEnabled = false;
    }
}

VideoWG::~VideoWG()
{
    delete m_CollimationOverlayElementsModel;
    delete m_CurrentElement;
    delete typeValues;
    delete painter;
}
