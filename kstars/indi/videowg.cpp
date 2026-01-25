/*
    SPDX-FileCopyrightText: 2017 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "videowg.h"
#include "collimationoverlaytypes.h"

#include "kstars_debug.h"
#include "kstarsdata.h"
#include "kstars.h"

#include <opencv2/imgproc.hpp>

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

bool VideoWG::newBayerFrame(IBLOB *bp, const BayerParameters &params, const uint8_t bpp)
{
    if (params.engine == DebayerEngine::OpenCV)
    {
        if (bpp == 8)
            return debayerCV<uint8_t>(bp, params);
        else if (bpp == 16)
            return debayerCV<uint16_t>(bp, params);
        else
            return false;
    }
    else
        return debayer1394(bp, params, bpp);
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
        if(overlayEnabled)
        {
            int offX = drawOffsetX + (streamImage->width() - streamImage->width() * drawScale) * 0.5;
            int offY = drawOffsetY + (streamImage->height() - streamImage->height() * drawScale) * 0.5;
            QImage tmp = streamImage->copy(offX, offY, streamImage->width() * drawScale, streamImage->height() * drawScale);
            kPix = QPixmap::fromImage(tmp.scaled(size(), Qt::KeepAspectRatio));
        }
        else
        {
            kPix = QPixmap::fromImage(streamImage->scaled(size(), Qt::KeepAspectRatio));
        }

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
    if(event->button() == Qt::LeftButton)
    {
        if (!rubberBand)
            rubberBand = new QRubberBand(QRubberBand::Rectangle, this);
        rubberBand->setGeometry(QRect(origin, QSize()));
        rubberBand->show();
    }
}

void VideoWG::mouseMoveEvent(QMouseEvent *event)
{
    if(rubberBand && rubberBand->isVisible())
        rubberBand->setGeometry(QRect(origin, event->pos()).normalized());
    else if(overlayEnabled)
    {
        QPoint diff = event->pos() - origin;
        drawOffsetX -= diff.x() * static_cast<double>(streamImage->width()) / kPix.width() * drawScale;
        drawOffsetY -= diff.y() * static_cast<double>(streamImage->height()) / kPix.height() * drawScale;
        origin = event->pos();
    }
}

void VideoWG::mouseReleaseEvent(QMouseEvent *event)
{
    if(event->button() == Qt::LeftButton)
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
}

void VideoWG::wheelEvent(QWheelEvent *event)
{
    if(overlayEnabled)
    {
        if(event->angleDelta().y() < 15.0)
            drawScale *= 1.1;
        if(event->angleDelta().y() > 15.0)
            drawScale *= 0.9;

        drawScale = std::min(drawScale, 1.0);
    }
    else
    {
        event->ignore();
    }
}

// Debayer using libdc1394 - 8bit only
bool VideoWG::debayer1394(const IBLOB *bp, const BayerParameters &params, const uint8_t bpp)
{
    auto start = std::chrono::high_resolution_clock::now();

    if (bpp != 8)
    {
        qCCritical(KSTARS) << "Unable to debayer1394 on bit depth:" << bpp;
        return false;
    }

    DC1394Params dc1394Params;
    if(!BayerUtils::verifyDC1394DebayerParams(params, dc1394Params))
    {
        qCCritical(KSTARS) << "Unable to debayer1394 - inconsistent parameters.";
        return false;
    }

    uint32_t rgb_size = streamW * streamH * 3;
    auto * destinationBuffer = new uint8_t[rgb_size];

    if (destinationBuffer == nullptr)
    {
        qCCritical(KSTARS) << "Unable to allocate memory for temporary bayer buffer.";
        return false;
    }

    int ds1394_height = streamH;

    uint8_t * dc1394_source = reinterpret_cast<uint8_t*>(bp->blob);
    if (dc1394Params.params.offsetY == 1)
    {
        dc1394_source += streamW;
        ds1394_height--;
    }
    if (dc1394Params.params.offsetX == 1)
    {
        dc1394_source++;
    }
    dc1394error_t error_code = dc1394_bayer_decoding_8bit(dc1394_source, destinationBuffer, streamW, ds1394_height,
                               dc1394Params.params.filter, dc1394Params.params.method);

    if (error_code != DC1394_SUCCESS)
    {
        qCCritical(KSTARS) << "Debayer1394 failed" << error_code;
        delete[] destinationBuffer;
        return false;
    }

    // Force a deep copy as we're about to free destinationBuffer
    QImage img(destinationBuffer, streamW, streamH, QImage::Format_RGB888);
    streamImage.reset(new QImage(img.copy()));
    bool rc = !streamImage->isNull();

    if (rc)
    {
        kPix = QPixmap::fromImage(streamImage->scaled(size(), Qt::KeepAspectRatio));

        paintOverlay(kPix);

        setPixmap(kPix);
    }

    emit imageChanged(streamImage);

    delete[] destinationBuffer;
    auto end = std::chrono::high_resolution_clock::now();
    qCDebug(KSTARS) << "Debayer1394 8bit using method:"
                    << BayerUtils::convertDC1394MethodToStr(dc1394Params.params.method)
                    << " took:" << std::chrono::duration<double, std::milli>(end - start).count() << "ms";
    return rc;
}

// Debayer using openCV
template <typename T>
bool VideoWG::debayerCV(const IBLOB *bp, const BayerParameters &params)
{
    try
    {
        auto start = std::chrono::high_resolution_clock::now();

        OpenCVParams cvParams;
        if(!BayerUtils::verifyCVDebayerParams(params, cvParams))
        {
            qCCritical(KSTARS) << "Unable to debayerCV - inconsistent parameters.";
            return false;
        }

        // Check the openCV algo is supported for bit depth, falls back if necessary
        OpenCVAlgo algo;
        BayerUtils::verifyCVAlgo(cvParams.algo, sizeof(T), algo);

        // Get the openCV code from algo and Bayer Pattern
        int cvCode = BayerUtils::getCVDebayerCode(algo, cvParams);

        int cv_height = streamH;
        T * cv_source = reinterpret_cast<T *>(bp->blob);

        if (cvParams.offsetY == 1)
        {
            cv_source += streamW;
            cv_height--;
        }
        if (cvParams.offsetX == 1)
        {
            cv_source++;
        }

        // OpenCV Demosaic
        int cv_type = std::is_same_v<T, uint16_t> ? CV_16UC1 : CV_8UC1;
        cv::Mat raw(cv_height, streamW, cv_type, cv_source);
        cv::Mat RGB;
        cv::demosaicing(raw, RGB, cvCode);

        if constexpr (std::is_same_v<T, uint16_t>)
        {
            // Scale 16-bit RGB values down to 8-bit for QImage
            RGB.convertTo(RGB, CV_8UC3, 1.0 / 256.0);
        }

        QImage img(RGB.data, RGB.cols, RGB.rows, RGB.step, QImage::Format_RGB888);
        streamImage.reset(new QImage(img.copy()));
        bool rc = !streamImage->isNull();

        if (rc)
        {
            kPix = QPixmap::fromImage(streamImage->scaled(size(), Qt::KeepAspectRatio));

            paintOverlay(kPix);

            setPixmap(kPix);
        }

        emit imageChanged(streamImage);

        auto end = std::chrono::high_resolution_clock::now();
        qCDebug(KSTARS) << "Stack Debayer (openCV) using algo:" << BayerUtils::openCVAlgoToString(algo)
                        << " took:" << std::chrono::duration<double, std::milli>(end - start).count() << "ms";
        return true;
    }
    catch (const cv::Exception &ex)
    {
        qCCritical(KSTARS) << QString("DebayerCV failed - openCV exception %1").arg(ex.what());
        return false;
    }
}

void VideoWG::paintOverlay(QPixmap &imagePix)
{
    if (!overlayEnabled || m_EnabledOverlayElements.count() == 0) return;

    // Anchor - default to center of image
    QPointF anchor(0, 0);
    scale = (static_cast<float>(kPix.width()) / static_cast<float>(streamW));

    // Apply any offset from (only) the first enabled anchor element
    bool foundAnchor = false;
    for (auto &oneElement : m_EnabledOverlayElements)
    {
        if (oneElement["Type"] == "Anchor" && !foundAnchor)
        {
            anchor.setX(oneElement["OffsetX"].toInt());
            anchor.setY(oneElement["OffsetY"].toInt());
            foundAnchor = true;
        }
    }

    painter->begin(&imagePix);
    painter->translate(kPix.width() / 2.0, kPix.height() / 2.0);
    painter->scale(scale / drawScale, scale / drawScale);
    painter->translate(anchor);
    painter->translate(-drawOffsetX, -drawOffsetY);

    for (auto &currentElement : m_EnabledOverlayElements)
    {

        painter->save();
        QPen m_pen = QPen(QColor(currentElement["Colour"].toString()));
        m_pen.setWidth(currentElement["Thickness"].toUInt());
        m_pen.setCapStyle(Qt::FlatCap);
        m_pen.setJoinStyle(Qt::MiterJoin);
        painter->setPen(m_pen);
        painter->translate(currentElement["OffsetX"].toFloat(), currentElement["OffsetY"].toFloat());

        int m_count = currentElement["Count"].toUInt();
        float m_pcd = currentElement["PCD"].toFloat();

        if (m_count == 1)
        {
            PaintOneItem(currentElement["Type"].toString(), QPointF(0.0, 0.0), currentElement["SizeX"].toUInt(),
                         currentElement["SizeY"].toUInt(), currentElement["Thickness"].toUInt());
        }
        else if (m_count > 1)
        {
            float slice = 360 / m_count;
            for (int i = 0; i < m_count; i++)
            {
                painter->save();
                painter->rotate((slice * i) + currentElement["Rotation"].toFloat());
                PaintOneItem(currentElement["Type"].toString(), QPointF((m_pcd / 2), 0.0), currentElement["SizeX"].toUInt(),
                             currentElement["SizeY"].toUInt(), currentElement["Thickness"].toUInt());
                painter->restore();
            }
        }

        painter->restore();
    }
    painter->end();
}

void VideoWG::setupOverlay ()
{
    if (overlayEnabled)
    {
        initOverlayModel();

        typeValues = new QStringList;
        collimationoverlaytype m_types;
        const QMetaObject *m_metaobject = m_types.metaObject();
        QMetaEnum m_metaEnum = m_metaobject->enumerator(m_metaobject->indexOfEnumerator("Types"));
        for (int i = 0; i < m_metaEnum.keyCount(); i++)
        {
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

    switch (typeValues->indexOf(type))
    {
        case 0: // Anchor - ignore as we're not drawing it
            break;

        case 1: // Ellipse
            painter->drawEllipse(position, m_sizeX, m_sizeY);
            break;

        case 2: // Rectangle
        {
            QRect m_rect((position.x() - (m_sizeX / 2)), (position.y() - (m_sizeY / 2)), (m_sizeX - (thickness / 2)),
                         (m_sizeY - (thickness / 2)));
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
    if (overlayEnabled == false)
    {
        overlayEnabled = true;
        if (m_CollimationOverlayElementsModel == nullptr)
        {
            setupOverlay();
        }
    }
    else if (overlayEnabled == true)
    {
        overlayEnabled = false;
    }
}

void VideoWG::resetFrame()
{
    drawOffsetX = 0;
    drawOffsetY = 0;
    drawScale = 1;
}

VideoWG::~VideoWG()
{
    delete m_CollimationOverlayElementsModel;
    delete m_CurrentElement;
    delete typeValues;
    delete painter;
}
