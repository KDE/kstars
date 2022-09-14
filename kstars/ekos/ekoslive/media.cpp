/*
    SPDX-FileCopyrightText: 2018 Jasem Mutlaq <mutlaqja@ikarustech.com>

    Media Channel

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "media.h"
#include "commands.h"
#include "profileinfo.h"
#include "skymapcomposite.h"
#include "fitsviewer/fitsview.h"
#include "fitsviewer/fitsdata.h"
#include "indi/indilistener.h"
#include "hips/hipsfinder.h"
#include "ekos/auxiliary/darklibrary.h"
#include "kspaths.h"
#include "Options.h"

#include "ekos_debug.h"

#include <QtConcurrent>
#include <KFormat>

namespace EkosLive
{

Media::Media(Ekos::Manager * manager): m_Manager(manager)
{
    connect(&m_WebSocket, &QWebSocket::connected, this, &Media::onConnected);
    connect(&m_WebSocket, &QWebSocket::disconnected, this, &Media::onDisconnected);
    connect(&m_WebSocket, static_cast<void(QWebSocket::*)(QAbstractSocket::SocketError)>(&QWebSocket::error), this,
            &Media::onError);


    connect(this, &Media::newMetadata, this, &Media::uploadMetadata);
    connect(this, &Media::newImage, this, [this](const QByteArray & image)
    {
        uploadImage(image);
        m_TemporaryView.clear();
    });
}

void Media::connectServer()
{
    QUrl requestURL(m_URL);

    QUrlQuery query;
    query.addQueryItem("username", m_AuthResponse["username"].toString());
    query.addQueryItem("token", m_AuthResponse["token"].toString());
    if (m_AuthResponse.contains("remoteToken"))
        query.addQueryItem("remoteToken", m_AuthResponse["remoteToken"].toString());
    if (m_Options[OPTION_SET_CLOUD_STORAGE])
        query.addQueryItem("cloudEnabled", "true");
    query.addQueryItem("email", m_AuthResponse["email"].toString());
    query.addQueryItem("from_date", m_AuthResponse["from_date"].toString());
    query.addQueryItem("to_date", m_AuthResponse["to_date"].toString());
    query.addQueryItem("plan_id", m_AuthResponse["plan_id"].toString());
    query.addQueryItem("type", m_AuthResponse["type"].toString());

    requestURL.setPath("/media/ekos");
    requestURL.setQuery(query);


    m_WebSocket.open(requestURL);

    qCInfo(KSTARS_EKOS) << "Connecting to Websocket server at" << requestURL.toDisplayString();
}

void Media::disconnectServer()
{
    m_WebSocket.close();
}

void Media::onConnected()
{
    qCInfo(KSTARS_EKOS) << "Connected to media Websocket server at" << m_URL.toDisplayString();

    connect(&m_WebSocket, &QWebSocket::textMessageReceived,  this, &Media::onTextReceived, Qt::UniqueConnection);
    connect(&m_WebSocket, &QWebSocket::binaryMessageReceived, this, &Media::onBinaryReceived, Qt::UniqueConnection);

    m_isConnected = true;
    m_ReconnectTries = 0;

    emit connected();
}

void Media::onDisconnected()
{
    qCInfo(KSTARS_EKOS) << "Disconnected from media Websocket server.";
    m_isConnected = false;

    disconnect(&m_WebSocket, &QWebSocket::textMessageReceived,  this, &Media::onTextReceived);
    disconnect(&m_WebSocket, &QWebSocket::binaryMessageReceived, this, &Media::onBinaryReceived);

    m_sendBlobs = true;

    for (const QString &oneFile : temporaryFiles)
        QFile::remove(oneFile);
    temporaryFiles.clear();

    emit disconnected();
}

void Media::onError(QAbstractSocket::SocketError error)
{
    qCritical(KSTARS_EKOS) << "Media Websocket connection error" << m_WebSocket.errorString();
    if (error == QAbstractSocket::RemoteHostClosedError ||
            error == QAbstractSocket::ConnectionRefusedError)
    {
        if (m_ReconnectTries++ < RECONNECT_MAX_TRIES)
            QTimer::singleShot(RECONNECT_INTERVAL, this, SLOT(connectServer()));
    }
}

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

        // Object Names
        QVariantList objectNames = payload["names"].toArray().toVariantList();

        for (auto &oneName : objectNames)
        {
            const QString name = oneName.toString();
            SkyObject *oneObject = KStarsData::Instance()->skyComposite()->findByName(name, false);
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
                    // Send everything as strings
                    QJsonObject metadata =
                    {
                        {"uuid", "hips"},
                        {"name", name},
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
}

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

void Media::sendDarkLibraryData(const QSharedPointer<FITSData> &data)
{
    sendData(data, "+D");
};

void Media::sendData(const QSharedPointer<FITSData> &data, const QString &uuid)
{
    if (m_isConnected == false || m_Options[OPTION_SET_IMAGE_TRANSFER] == false || m_sendBlobs == false)
        return;

    m_UUID = uuid;

    m_TemporaryView.reset(new FITSView());
    m_TemporaryView->loadData(data);
    QtConcurrent::run(this, &Media::upload, m_TemporaryView);
}

void Media::sendFile(const QString &filename, const QString &uuid)
{
    if (m_isConnected == false || m_Options[OPTION_SET_IMAGE_TRANSFER] == false || m_sendBlobs == false)
        return;

    m_UUID = uuid;

    QSharedPointer<FITSView> previewImage(new FITSView());
    connect(previewImage.get(), &FITSView::loaded, this, [this, previewImage]()
    {
        QtConcurrent::run(this, &Media::upload, previewImage);
    });
    previewImage->loadFile(filename);
}

void Media::sendView(const QSharedPointer<FITSView> &view, const QString &uuid)
{
    if (m_isConnected == false || m_Options[OPTION_SET_IMAGE_TRANSFER] == false || m_sendBlobs == false)
        return;

    m_UUID = uuid;

    upload(view);
}

void Media::upload(const QSharedPointer<FITSView> &view)
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
        {"bin", QString("%1x%2").arg(xbin.toString(), ybin.toString())},
        {"bpp", QString::number(imageData->bpp())},
        {"uuid", m_UUID},
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

    auto sendPixmap = (!m_Options[OPTION_SET_HIGH_BANDWIDTH] || m_UUID[0] == "+");
    auto scaleWidth = sendPixmap ? HB_IMAGE_WIDTH / 2 : HB_IMAGE_WIDTH;

    // For low bandwidth images
    // Except for dark frames +D
    if (sendPixmap)
    {
        QPixmap scaledImage = view->getDisplayPixmap().width() > scaleWidth ?
                              view->getDisplayPixmap().scaledToWidth(scaleWidth, Qt::FastTransformation) :
                              view->getDisplayPixmap();
        scaledImage.save(&buffer, ext.toLatin1().constData(), HB_IMAGE_QUALITY);
    }
    // For high bandwidth images
    else
    {
        QImage scaledImage =  view->getDisplayImage().width() > scaleWidth ?
                              view->getDisplayImage().scaledToWidth(scaleWidth, Qt::SmoothTransformation) :
                              view->getDisplayImage();
        scaledImage.save(&buffer, ext.toLatin1().constData(), HB_IMAGE_QUALITY);
    }
    buffer.close();

    emit newImage(jpegData);
}

void Media::sendUpdatedFrame(const QSharedPointer<FITSView> &view)
{
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

void Media::sendVideoFrame(const QSharedPointer<QImage> &frame)
{
    if (m_isConnected == false || m_Options[OPTION_SET_IMAGE_TRANSFER] == false || m_sendBlobs == false || !frame)
        return;

    int32_t width = m_Options[OPTION_SET_HIGH_BANDWIDTH] ? HB_VIDEO_WIDTH : HB_VIDEO_WIDTH / 2;
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

    m_WebSocket.sendBinaryMessage(image);
}

void Media::registerCameras()
{
    if (m_isConnected == false)
        return;

    for(auto &oneDevice : INDIListener::devices())
    {
        auto camera = oneDevice->getCamera();
        if (camera)
            connect(camera, &ISD::Camera::newVideoFrame, this, &Media::sendVideoFrame, Qt::UniqueConnection);
    }
}

void Media::resetPolarView()
{
    this->correctionVector = QLineF();

    m_Manager->alignModule()->zoomAlignView();
}

void Media::uploadMetadata(const QByteArray &metadata)
{
    m_WebSocket.sendTextMessage(metadata);
}

void Media::uploadImage(const QByteArray &image)
{
    m_WebSocket.sendBinaryMessage(image);
}

void Media::processNewBLOB(IBLOB *bp)
{
    Q_UNUSED(bp)
}

void Media::sendModuleFrame(const QSharedPointer<FITSView> &view)
{
    if (m_isConnected == false || m_Options[OPTION_SET_IMAGE_TRANSFER] == false || m_sendBlobs == false)
        return;

    if (qobject_cast<Ekos::Align*>(sender()) == m_Manager->alignModule())
        sendView(view, "+A");
    else if (qobject_cast<Ekos::Focus*>(sender()) == m_Manager->focusModule())
        sendView(view, "+F");
    else if (qobject_cast<Ekos::Guide*>(sender()) == m_Manager->guideModule())
        sendView(view, "+G");
    else if (qobject_cast<Ekos::DarkLibrary*>(sender()) == Ekos::DarkLibrary::Instance())
        sendView(view, "+D");
}
}
