/*
    SPDX-FileCopyrightText: 2018 Jasem Mutlaq <mutlaqja@ikarustech.com>

    Media Channel

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "media.h"
#include "commands.h"
#include "skymapcomposite.h"
#include "fitsviewer/fitsview.h"
#include "fitsviewer/fitsdata.h"
#include "indi/indilistener.h"
#include "hips/hipsfinder.h"
#include "kstarsdata.h"
#include "ekos/auxiliary/darklibrary.h"
#include "ekos/guide/guide.h"
#include "ekos/align/align.h"
#include "ekos/capture/capture.h"
#include "ekos/capture/cameraprocess.h"
#include "ekos/focus/focusmodule.h"
#include "kspaths.h"
#include "Options.h"

#include "ekos_debug.h"
#include "kstars.h"
#include "version.h"

#include <QtConcurrent>
#include <KFormat>
#include <QImageWriter>

namespace EkosLive
{

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
Media::Media(Ekos::Manager * manager, QVector<QSharedPointer<NodeManager>> &nodeManagers):
    m_Manager(manager), m_NodeManagers(nodeManagers)
{
    for (auto &nodeManager : m_NodeManagers)
    {
        connect(nodeManager->media(), &Node::connected, this, &Media::onConnected);
        connect(nodeManager->media(), &Node::disconnected, this, &Media::onDisconnected);
        connect(nodeManager->media(), &Node::onTextReceived, this, &Media::onTextReceived);
        connect(nodeManager->media(), &Node::onBinaryReceived, this, &Media::onBinaryReceived);
    }

    connect(this, &Media::newImage, this, [this](const QByteArray & image)
    {
        uploadImage(image);
    });
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
bool Media::isConnected() const
{
    return std::any_of(m_NodeManagers.begin(), m_NodeManagers.end(), [](auto & nodeManager)
    {
        return nodeManager->media()->isConnected();
    });
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Media::onConnected()
{
    auto node = qobject_cast<Node*>(sender());
    if (!node)
        return;

    qCInfo(KSTARS_EKOS) << "Connected to Media Websocket server at" << node->url().toDisplayString();

    emit connected();
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Media::onDisconnected()
{
    auto node = qobject_cast<Node*>(sender());
    if (!node)
        return;

    qCInfo(KSTARS_EKOS) << "Disconnected from Message Websocket server at" << node->url().toDisplayString();

    if (isConnected() == false)
    {
        m_sendBlobs = true;

        for (const QString &oneFile : temporaryFiles)
            QFile::remove(oneFile);
        temporaryFiles.clear();

        emit disconnected();
    }
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Media::onTextReceived(const QString &message)
{
    qCInfo(KSTARS_EKOS) << "Media Text Websocket Message" << message;
    QJsonParseError error;
    auto serverMessage = QJsonDocument::fromJson(message.toLatin1(), &error);
    if (error.error != QJsonParseError::NoError)
    {
        qCWarning(KSTARS_EKOS) << "Ekos Live Parsing Error" << error.errorString();
        return;
    }

    const QJsonObject msgObj = serverMessage.object();
    const QString command = msgObj["type"].toString();
    const QJsonObject payload = msgObj["payload"].toObject();

    if (command == commands[ALIGN_SET_FILE_EXTENSION])
        extension = payload["ext"].toString();
    else if (command == commands[SET_BLOBS])
        m_sendBlobs = msgObj["payload"].toBool();
    // Get a list of object based on criteria
    else if (command == commands[ASTRO_GET_OBJECTS_IMAGE])
    {
        int level = payload["level"].toInt(5);
        double zoom = payload["zoom"].toInt(20000);
        bool exact = payload["exact"].toBool(false);

        // Object Names
        QVariantList objectNames = payload["names"].toArray().toVariantList();

        for (auto &oneName : objectNames)
        {
            const QString name = oneName.toString();
            SkyObject *oneObject = KStarsData::Instance()->skyComposite()->findByName(name, exact);
            if (oneObject)
            {
                QImage centerImage(HIPS_TILE_WIDTH, HIPS_TILE_HEIGHT, QImage::Format_ARGB32_Premultiplied);
                double fov_w = 0, fov_h = 0;

                if (oneObject->type() == SkyObject::MOON || oneObject->type() == SkyObject::PLANET)
                {
                    QProcess xplanetProcess;
                    const QString output = KSPaths::writableLocation(QStandardPaths::TempLocation) + QDir::separator() + "xplanet.jpg";
                    xplanetProcess.start(Options::xplanetPath(), QStringList()
                                         << "--num_times" << "1"
                                         << "--geometry" << QString("%1x%2").arg(HIPS_TILE_WIDTH).arg(HIPS_TILE_HEIGHT)
                                         << "--body" << name.toLower()
                                         << "--output" << output);
                    xplanetProcess.waitForFinished(5000);
                    centerImage.load(output);
                }
                else
                    HIPSFinder::Instance()->render(oneObject, level, zoom, &centerImage, fov_w, fov_h);

                if (!centerImage.isNull())
                {
                    // Use seed from name, level, and zoom so that it is unique
                    // even if regenerated again.
                    auto seed = QString("%1%2%3").arg(QString::number(level), QString::number(zoom), name);
                    QString uuid = QString("hips_") + QCryptographicHash::hash(seed.toLatin1(), QCryptographicHash::Md5).toHex();
                    // Send everything as strings
                    QJsonObject metadata =
                    {
                        {"uuid", uuid},
                        {"name", exact ? name : oneObject->name()},
                        {"zoom", zoom},
                        {"resolution", QString("%1x%2").arg(HIPS_TILE_WIDTH).arg(HIPS_TILE_HEIGHT)},
                        {"bin", "1x1"},
                        {"fov_w", QString::number(fov_w)},
                        {"fov_h", QString::number(fov_h)},
                        {"ext", "jpg"}
                    };

                    QByteArray jpegData;
                    QBuffer buffer(&jpegData);
                    buffer.open(QIODevice::WriteOnly);

                    // First METADATA_PACKET bytes of the binary data is always allocated
                    // to the metadata, the rest to the image data.
                    QByteArray meta = QJsonDocument(metadata).toJson(QJsonDocument::Compact);
                    meta = meta.leftJustified(METADATA_PACKET, 0);
                    buffer.write(meta);
                    centerImage.save(&buffer, "jpg", 90);
                    buffer.close();

                    emit newImage(jpegData);
                }
            }
        }
    }
    else if (command == commands[ASTRO_GET_SKYPOINT_IMAGE])
    {
        int level = payload["level"].toInt(5);
        double zoom = payload["zoom"].toInt(20000);
        double ra = payload["ra"].toDouble(0);
        double de = payload["de"].toDouble(0);
        double width = payload["width"].toDouble(512);
        double height = payload["height"].toDouble(512);

        QImage centerImage(width, height, QImage::Format_ARGB32_Premultiplied);
        SkyPoint coords(ra, de);
        SkyPoint J2000Coord(coords.ra(), coords.dec());
        J2000Coord.catalogueCoord(KStars::Instance()->data()->ut().djd());
        coords.setRA0(J2000Coord.ra());
        coords.setDec0(J2000Coord.dec());
        coords.EquatorialToHorizontal(KStarsData::Instance()->lst(), KStarsData::Instance()->geo()->lat());

        volatile auto jnowRAString = coords.ra().toHMSString();
        volatile auto jnowDEString = coords.dec().toDMSString();
        volatile auto j2000RAString = coords.ra0().toHMSString();
        volatile auto j2000DEString = coords.dec0().toDMSString();


        double fov_w = 0, fov_h = 0;
        HIPSFinder::Instance()->render(&coords, level, zoom, &centerImage, fov_w, fov_h);

        if (!centerImage.isNull())
        {
            // Use seed from name, level, and zoom so that it is unique
            // even if regenerated again.
            // Send everything as strings
            QJsonObject metadata =
            {
                {"uuid", "skypoint_hips"},
                {"name", "skypoint_hips"},
                {"zoom", zoom},
                {"resolution", QString("%1x%2").arg(width).arg(height)},
                {"bin", "1x1"},
                {"fov_w", QString::number(fov_w)},
                {"fov_h", QString::number(fov_h)},
                {"ext", "jpg"}
            };

            QByteArray jpegData;
            QBuffer buffer(&jpegData);
            buffer.open(QIODevice::WriteOnly);

            // First METADATA_PACKET bytes of the binary data is always allocated
            // to the metadata, the rest to the image data.
            QByteArray meta = QJsonDocument(metadata).toJson(QJsonDocument::Compact);
            meta = meta.leftJustified(METADATA_PACKET, 0);
            buffer.write(meta);
            centerImage.save(&buffer, "jpg", 95);
            buffer.close();

            emit newImage(jpegData);
        }
    }
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Media::onBinaryReceived(const QByteArray &message)
{
    // Sometimes this is triggered even though it's a text message
    Ekos::Align * align = m_Manager->alignModule();
    if (align)
    {
        QString metadataString = message.left(METADATA_PACKET);
        QJsonDocument metadataDocument = QJsonDocument::fromJson(metadataString.toLatin1());
        QJsonObject metadataJSON = metadataDocument.object();
        QString extension = metadataJSON.value("ext").toString();
        align->loadAndSlew(message.mid(METADATA_PACKET), extension);
    }
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Media::sendDarkLibraryData(const QSharedPointer<FITSData> &data)
{
    sendData(data, "+D");
};

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Media::sendData(const QSharedPointer<FITSData> &data, const QString &uuid)
{
    if (Options::ekosLiveImageTransfer() == false || m_sendBlobs == false || isConnected() == false)
        return;

    StretchParams params;
    QImage image;
    stretch(data, image, params);
    upload(data, image, params, uuid);
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Media::sendFile(const QString &filename, const QString &uuid)
{
    if (Options::ekosLiveImageTransfer() == false || m_sendBlobs == false || isConnected() == false)
        return;

    QSharedPointer<FITSData> data(new FITSData());
    data->loadFromFile(filename);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    QtConcurrent::run(&Media::dispatch, this, data, uuid);
#else
    QtConcurrent::run(this, &Media::dispatch, data, uuid);
#endif
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Media::sendView(const QSharedPointer<FITSView> &view, const QString &uuid)
{
    if (Options::ekosLiveImageTransfer() == false || m_sendBlobs == false || isConnected() == false)
        return;

    upload(view, uuid);
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Media::dispatch(const QSharedPointer<FITSData> &data, const QString &uuid)
{
    QSharedPointer<FITSView> previewImage(new FITSView());
    previewImage->loadData(data);
    upload(previewImage, uuid);
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Media::stretch(const QSharedPointer<FITSData> &data, QImage &image, StretchParams &params) const
{
    double min = 0, max = 0;
    data->getMinMax(&min, &max);
    auto width = data->width();
    auto height = data->height();
    auto channels = data->channels();
    auto dataType = data->dataType();

    if (min == max)
    {
        image.fill(Qt::white);
    }

    if (channels == 1)
    {
        image = QImage(width, height, QImage::Format_Indexed8);

        image.setColorCount(256);
        for (int i = 0; i < 256; i++)
            image.setColor(i, qRgb(i, i, i));
    }
    else
    {
        image = QImage(width, height, QImage::Format_RGB32);
    }

    Stretch stretch(width, height, channels, dataType);

    // Compute new auto-stretch params.
    params = stretch.computeParams(data->getImageBuffer());
    stretch.setParams(params);
    stretch.run(data->getImageBuffer(), &image, 1);
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Media::upload(const QSharedPointer<FITSView> &view, const QString &uuid)
{
    const QString ext = "jpg";
    QByteArray jpegData;
    QBuffer buffer(&jpegData);
    buffer.open(QIODevice::WriteOnly);

    const QSharedPointer<FITSData> imageData = view->imageData();
    QString resolution = QString("%1x%2").arg(imageData->width()).arg(imageData->height());
    QString sizeBytes = KFormat().formatByteSize(imageData->size());
    QVariant xbin(1), ybin(1), exposure(0), focal_length(0), gain(0), pixel_size(0), aperture(0);
    imageData->getRecordValue("XBINNING", xbin);
    imageData->getRecordValue("YBINNING", ybin);
    imageData->getRecordValue("EXPTIME", exposure);
    imageData->getRecordValue("GAIN", gain);
    imageData->getRecordValue("PIXSIZE1", pixel_size);
    imageData->getRecordValue("FOCALLEN", focal_length);
    imageData->getRecordValue("APTDIA", aperture);

    auto stretchParameters = view->getStretchParams();

    // Account for binning
    const double binned_pixel = pixel_size.toDouble() * xbin.toInt();

    // Send everything as strings
    QJsonObject metadata =
    {
        {"resolution", resolution},
        {"size", sizeBytes},
        {"channels", imageData->channels()},
        {"mean", imageData->getAverageMean()},
        {"median", imageData->getAverageMedian()},
        {"stddev", imageData->getAverageStdDev()},
        {"min", imageData->getMin()},
        {"max", imageData->getMax()},
        {"bin", QString("%1x%2").arg(xbin.toString(), ybin.toString())},
        {"bpp", QString::number(imageData->bpp())},
        {"uuid", uuid},
        {"exposure", exposure.toString()},
        {"focal_length", focal_length.toString()},
        {"aperture", aperture.toString()},
        {"gain", gain.toString()},
        {"pixel_size", QString::number(binned_pixel, 'f', 4)},
        {"shadows", stretchParameters.grey_red.shadows},
        {"midtones", stretchParameters.grey_red.midtones},
        {"highlights", stretchParameters.grey_red.highlights},
        {"hasWCS", imageData->hasWCS()},
        {"hfr", imageData->getHFR()},
        {"view", view->objectName()},
        {"ext", ext}
    };

    // First METADATA_PACKET bytes of the binary data is always allocated
    // to the metadata
    // the rest to the image data.
    QByteArray meta = QJsonDocument(metadata).toJson(QJsonDocument::Compact);
    meta = meta.leftJustified(METADATA_PACKET, 0);
    buffer.write(meta);

    auto fastImage = (!Options::ekosLiveHighBandwidth() || uuid[0] == '+');
    auto scaleWidth = fastImage ? HB_IMAGE_WIDTH / 2 : HB_IMAGE_WIDTH;

    // For low bandwidth images
    // Except for dark frames +D
    QPixmap scaledImage = view->getDisplayPixmap().width() > scaleWidth ?
                          view->getDisplayPixmap().scaledToWidth(scaleWidth, fastImage ? Qt::FastTransformation : Qt::SmoothTransformation) :
                          view->getDisplayPixmap();
    scaledImage.save(&buffer, ext.toLatin1().constData(), HB_IMAGE_QUALITY);

    buffer.close();

    emit newImage(jpegData);
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Media::upload(const QSharedPointer<FITSData> &data, const QImage &image, const StretchParams &params,
                   const QString &uuid)
{
    const QString ext = "jpg";
    QByteArray jpegData;
    QBuffer buffer(&jpegData);
    buffer.open(QIODevice::WriteOnly);

    QString resolution = QString("%1x%2").arg(data->width()).arg(data->height());
    QString sizeBytes = KFormat().formatByteSize(data->size());
    QVariant xbin(1), ybin(1), exposure(0), focal_length(0), gain(0), pixel_size(0), aperture(0);
    data->getRecordValue("XBINNING", xbin);
    data->getRecordValue("YBINNING", ybin);
    data->getRecordValue("EXPTIME", exposure);
    data->getRecordValue("GAIN", gain);
    data->getRecordValue("PIXSIZE1", pixel_size);
    data->getRecordValue("FOCALLEN", focal_length);
    data->getRecordValue("APTDIA", aperture);

    // Account for binning
    const double binned_pixel = pixel_size.toDouble() * xbin.toInt();

    // Send everything as strings
    QJsonObject metadata =
    {
        {"resolution", resolution},
        {"size", sizeBytes},
        {"channels", data->channels()},
        {"mean", data->getAverageMean()},
        {"median", data->getAverageMedian()},
        {"stddev", data->getAverageStdDev()},
        {"min", data->getMin()},
        {"max", data->getMax()},
        {"bin", QString("%1x%2").arg(xbin.toString(), ybin.toString())},
        {"bpp", QString::number(data->bpp())},
        {"uuid", uuid},
        {"exposure", exposure.toString()},
        {"focal_length", focal_length.toString()},
        {"aperture", aperture.toString()},
        {"gain", gain.toString()},
        {"pixel_size", QString::number(binned_pixel, 'f', 4)},
        {"shadows", params.grey_red.shadows},
        {"midtones", params.grey_red.midtones},
        {"highlights", params.grey_red.highlights},
        {"hasWCS", data->hasWCS()},
        {"hfr", data->getHFR()},
        {"ext", ext}
    };

    // First METADATA_PACKET bytes of the binary data is always allocated
    // to the metadata
    // the rest to the image data.
    QByteArray meta = QJsonDocument(metadata).toJson(QJsonDocument::Compact);
    meta = meta.leftJustified(METADATA_PACKET, 0);
    buffer.write(meta);

    auto fastImage = (!Options::ekosLiveHighBandwidth() || uuid[0] == '+');
    auto scaleWidth = fastImage ? HB_IMAGE_WIDTH / 2 : HB_IMAGE_WIDTH;

    // For low bandwidth images
    // Except for dark frames +D
    QImage scaledImage = image.width() > scaleWidth ?
                         image.scaledToWidth(scaleWidth, fastImage ? Qt::FastTransformation : Qt::SmoothTransformation) :
                         image;
    scaledImage.save(&buffer, ext.toLatin1().constData(), HB_IMAGE_QUALITY);

    buffer.close();

    emit newImage(jpegData);
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Media::sendUpdatedFrame(const QSharedPointer<FITSView> &view)
{
    if (isConnected() == false)
        return;

    QString ext = "jpg";
    QByteArray jpegData;
    QBuffer buffer(&jpegData);
    buffer.open(QIODevice::WriteOnly);

    const QSharedPointer<FITSData> imageData = view->imageData();

    if (!imageData)
        return;

    const int32_t width = imageData->width();
    const int32_t height = imageData->height();
    QString resolution = QString("%1x%2").arg(width).arg(height);
    QString sizeBytes = KFormat().formatByteSize(imageData->size());
    QVariant xbin(1), ybin(1), exposure(0), focal_length(0), gain(0), pixel_size(0), aperture(0);
    imageData->getRecordValue("XBINNING", xbin);
    imageData->getRecordValue("YBINNING", ybin);
    imageData->getRecordValue("EXPTIME", exposure);
    imageData->getRecordValue("GAIN", gain);
    imageData->getRecordValue("PIXSIZE1", pixel_size);
    imageData->getRecordValue("FOCALLEN", focal_length);
    imageData->getRecordValue("APTDIA", aperture);

    // Account for binning
    const double binned_pixel = pixel_size.toDouble() * xbin.toInt();
    // Send everything as strings
    QJsonObject metadata =
    {
        {"resolution", resolution},
        {"size", sizeBytes},
        {"channels", imageData->channels()},
        {"mean", imageData->getAverageMean()},
        {"median", imageData->getAverageMedian()},
        {"stddev", imageData->getAverageStdDev()},
        {"bin", QString("%1x%2").arg(xbin.toString()).arg(ybin.toString())},
        {"bpp", QString::number(imageData->bpp())},
        {"uuid", "+A"},
        {"exposure", exposure.toString()},
        {"focal_length", focal_length.toString()},
        {"aperture", aperture.toString()},
        {"gain", gain.toString()},
        {"pixel_size", QString::number(binned_pixel, 'f', 4)},
        {"ext", ext}
    };

    // First METADATA_PACKET bytes of the binary data is always allocated
    // to the metadata
    // the rest to the image data.
    QByteArray meta = QJsonDocument(metadata).toJson(QJsonDocument::Compact);
    meta = meta.leftJustified(METADATA_PACKET, 0);
    buffer.write(meta);

    // For low bandwidth images
    QPixmap scaledImage;
    // Align images
    if (correctionVector.isNull() == false)
    {
        scaledImage = view->getDisplayPixmap();
        const double currentZoom = view->getCurrentZoom();
        const double normalizedZoom = currentZoom / 100;
        // If zoom level is not 100%, then scale.
        if (fabs(normalizedZoom - 1) > 0.001)
            scaledImage = view->getDisplayPixmap().scaledToWidth(view->zoomedWidth());
        else
            scaledImage = view->getDisplayPixmap();
        // as we factor in the zoom level, we adjust center and length accordingly
        QPointF center = 0.5 * correctionVector.p1() * normalizedZoom + 0.5 * correctionVector.p2() * normalizedZoom;
        uint32_t length = qMax(correctionVector.length() / normalizedZoom, 100 / normalizedZoom);

        QRect boundingRectable;
        boundingRectable.setSize(QSize(length * 2, length * 2));
        QPoint topLeft = (center - QPointF(length, length)).toPoint();
        boundingRectable.moveTo(topLeft);
        boundingRectable = boundingRectable.intersected(scaledImage.rect());

        emit newBoundingRect(boundingRectable, scaledImage.size(), currentZoom);

        scaledImage = scaledImage.copy(boundingRectable);
    }
    else
    {
        scaledImage = view->getDisplayPixmap().width() > HB_IMAGE_WIDTH / 2 ?
                      view->getDisplayPixmap().scaledToWidth(HB_IMAGE_WIDTH / 2, Qt::FastTransformation) :
                      view->getDisplayPixmap();
        emit newBoundingRect(QRect(), QSize(), 100);
    }

    scaledImage.save(&buffer, ext.toLatin1().constData(), HB_IMAGE_QUALITY);
    buffer.close();
    emit newImage(jpegData);
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Media::sendVideoFrame(const QSharedPointer<QImage> &frame)
{
    if (isConnected() == false ||
            Options::ekosLiveImageTransfer() == false ||
            m_sendBlobs == false ||
            !frame)
        return;

    int32_t width = Options::ekosLiveHighBandwidth() ? HB_VIDEO_WIDTH : HB_VIDEO_WIDTH / 2;
    QByteArray image;
    QBuffer buffer(&image);
    buffer.open(QIODevice::WriteOnly);

    QImage videoImage = (frame->width() > width) ? frame->scaledToWidth(width) : *frame;

    QString resolution = QString("%1x%2").arg(videoImage.width()).arg(videoImage.height());

    // First METADATA_PACKET bytes of the binary data is always allocated
    // to the metadata
    // the rest to the image data.
    QJsonObject metadata =
    {
        {"resolution", resolution},
        {"ext", "jpg"}
    };
    QByteArray meta = QJsonDocument(metadata).toJson(QJsonDocument::Compact);
    meta = meta.leftJustified(METADATA_PACKET, 0);
    buffer.write(meta);

    QImageWriter writer;
    writer.setDevice(&buffer);
    writer.setFormat("JPG");
    writer.setCompression(6);
    writer.write(videoImage);
    buffer.close();

    for (auto &nodeManager : m_NodeManagers)
    {
        nodeManager->media()->sendBinaryMessage(image);
    }
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Media::registerCameras()
{
    static const QRegularExpression re("[-{}]");

    for(auto &oneDevice : INDIListener::devices())
    {
        auto camera = oneDevice->getCamera();
        if (camera)
        {
            camera->disconnect(this);
            connect(camera, &ISD::Camera::newVideoFrame, this, &Media::sendVideoFrame);
        }
    }

    auto captureModule = m_Manager->captureModule();
    if (!captureModule)
        return;

    auto process = captureModule->process().get();
    process->disconnect(this);
    connect(process, &Ekos::CameraProcess::newView, this, [this](const QSharedPointer<FITSView> &view)
    {
        sendView(view, view->imageData()->objectName());
    });
}

void Media::resetPolarView()
{
    this->correctionVector = QLineF();
    m_Manager->alignModule()->zoomAlignView();
}

void Media::uploadImage(const QByteArray &image)
{
    for (auto &nodeManager : m_NodeManagers)
    {
        nodeManager->media()->sendBinaryMessage(image);
    }
}

void Media::processNewBLOB(IBLOB * bp)
{
    Q_UNUSED(bp)
}

void Media::sendModuleFrame(const QSharedPointer<FITSView> &view)
{
    if (Options::ekosLiveImageTransfer() == false || m_sendBlobs == false)
        return;

    if (qobject_cast<Ekos::Align*>(sender()) == m_Manager->alignModule())
        sendView(view, "+A");
    else if (qobject_cast<Ekos::Focus*>(sender()) == m_Manager->focusModule()->mainFocuser())
        sendView(view, "+F");
    else if (qobject_cast<Ekos::Guide*>(sender()) == m_Manager->guideModule())
        sendView(view, "+G");
    else if (qobject_cast<Ekos::DarkLibrary*>(sender()) == Ekos::DarkLibrary::Instance())
        sendView(view, "+D");
}
}
