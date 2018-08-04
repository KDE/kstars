/*  Ekos Live Media

    Copyright (C) 2018 Jasem Mutlaq <mutlaqja@ikarustech.com>

    Media Channel

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#include "media.h"
#include "commands.h"
#include "profileinfo.h"

#include "fitsviewer/fitsview.h"
#include "fitsviewer/fitsdata.h"

#include "ekos_debug.h"

#include <KFormat>

namespace EkosLive
{

Media::Media(EkosManager *manager): m_Manager(manager)
{
    connect(&m_WebSocket, &QWebSocket::connected, this, &Media::onConnected);
    connect(&m_WebSocket, &QWebSocket::disconnected, this, &Media::onDisconnected);
    connect(&m_WebSocket, static_cast<void(QWebSocket::*)(QAbstractSocket::SocketError)>(&QWebSocket::error), this, &Media::onError);

}

void Media::connectServer()
{
    QUrl requestURL(m_URL);

    QUrlQuery query;
    query.addQueryItem("username", m_AuthResponse["username"].toString());
    query.addQueryItem("token", m_AuthResponse["token"].toString());
    query.addQueryItem("email", m_AuthResponse["email"].toString());
    query.addQueryItem("from_date", m_AuthResponse["from_date"].toString());
    query.addQueryItem("to_date", m_AuthResponse["to_date"].toString());
    query.addQueryItem("plan_id", m_AuthResponse["plan_id"].toString());

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

    connect(&m_WebSocket, &QWebSocket::textMessageReceived,  this, &Media::onTextReceived);
    connect(&m_WebSocket, &QWebSocket::binaryMessageReceived, this, &Media::onBinaryReceived);

    m_isConnected = true;
    m_ReconnectTries=0;

    emit connected();
}

void Media::onDisconnected()
{
    qCInfo(KSTARS_EKOS) << "Disonnected from media Websocket server.";
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
}

void Media::onBinaryReceived(const QByteArray &message)
{
    // For now, we are only receiving binary image (jpg or FITS) for load and slew
    QTemporaryFile file(QString("/tmp/XXXXXX.%1").arg(extension));
    file.setAutoRemove(false);
    file.open();
    file.write(message);
    file.close();

    Ekos::Align *align = m_Manager->alignModule();

    const QString filename = file.fileName();

    temporaryFiles << filename;

    align->loadAndSlew(filename);
}

void Media::sendPreviewImage(FITSView *view, const QString &uuid)
{
    if (m_isConnected == false || m_Options[OPTION_SET_IMAGE_TRANSFER] == false || m_sendBlobs == false)
        return;

    QByteArray jpegData;
    QBuffer buffer(&jpegData);
    buffer.open(QIODevice::WriteOnly);
    QImage scaledImage = view->getDisplayImage()->scaledToWidth(m_Options[OPTION_SET_HIGH_BANDWIDTH] ? HB_WIDTH : HB_WIDTH/2);
    scaledImage.save(&buffer, "jpg", m_Options[OPTION_SET_HIGH_BANDWIDTH] ? HB_IMAGE_QUALITY : HB_IMAGE_QUALITY/2);
    buffer.close();

    const FITSData *imageData = view->getImageData();
    QString resolution = QString("%1x%2").arg(imageData->getWidth()).arg(imageData->getHeight());
    QString sizeBytes = KFormat().formatByteSize(imageData->getSize());
    QVariant xbin(1), ybin(1);
    imageData->getRecordValue("XBINNING", xbin);
    imageData->getRecordValue("YBINNING", ybin);
    QString binning = QString("%1x%2").arg(xbin.toString()).arg(ybin.toString());
    QString bitDepth = QString::number(imageData->getBPP());

    QJsonObject metadata = {
        {"resolution",resolution},
        {"size",sizeBytes},
        {"bin",binning},
        {"bpp",bitDepth},
        {"uuid", imageData->isTempFile() ? "" : uuid},
    };

    m_WebSocket.sendTextMessage(QJsonDocument(metadata).toJson(QJsonDocument::Compact));
    m_WebSocket.sendBinaryMessage(jpegData);
}

void Media::sendUpdatedFrame(FITSView *view)
{
    if (m_isConnected == false || m_Options[OPTION_SET_HIGH_BANDWIDTH] == false || m_sendBlobs == false)
        return;

    QByteArray jpegData;
    QBuffer buffer(&jpegData);
    buffer.open(QIODevice::WriteOnly);
    QPixmap displayPixmap = view->getDisplayPixmap();
    if (correctionVector.isNull() == false)
    {
        QPointF center = 0.5 * correctionVector.p1() + 0.5 * correctionVector.p2();
        double length = correctionVector.length();
        if (length < 100)
            length = 100;
        QRect boundingRectable;
        boundingRectable.setSize(QSize(static_cast<int>(length*2), static_cast<int>(length*2)));

        QPoint topLeft = (center - QPointF(length, length)).toPoint();
        boundingRectable.moveTo(topLeft);

        boundingRectable = boundingRectable.intersected(displayPixmap.rect());

        emit newBoundingRect(boundingRectable, displayPixmap.size());

        displayPixmap = displayPixmap.copy(boundingRectable);
    }
    else
        emit newBoundingRect(QRect(), QSize());
    displayPixmap.save(&buffer, "jpg", m_Options[OPTION_SET_HIGH_BANDWIDTH] ? HB_PAH_IMAGE_QUALITY : HB_PAH_IMAGE_QUALITY/2);
    buffer.close();

    m_WebSocket.sendBinaryMessage(jpegData);
}

void Media::sendVideoFrame(std::unique_ptr<QImage> & frame)
{
    if (m_isConnected == false || m_Options[OPTION_SET_IMAGE_TRANSFER] == false || m_sendBlobs == false || !frame)
        return;

    // TODO Scale should be configurable
    QImage scaledImage =  frame.get()->scaledToWidth(m_Options[OPTION_SET_HIGH_BANDWIDTH] ? HB_WIDTH : HB_WIDTH/2);
    QTemporaryFile jpegFile;
    jpegFile.open();
    jpegFile.close();

    // TODO Quality should be configurable
    scaledImage.save(jpegFile.fileName(), "jpg", m_Options[OPTION_SET_HIGH_BANDWIDTH] ? HB_VIDEO_QUALITY : HB_VIDEO_QUALITY/2);

    jpegFile.open();

    m_WebSocket.sendBinaryMessage(jpegFile.readAll());
}

void Media::registerCameras()
{
    if (m_isConnected == false)
        return;

    for(ISD::GDInterface *gd : m_Manager->findDevices(KSTARS_CCD))
    {
        ISD::CCD *oneCCD = dynamic_cast<ISD::CCD*>(gd);
        connect(oneCCD, &ISD::CCD::newVideoFrame, this, &Media::sendVideoFrame, Qt::UniqueConnection);
    }
}

void Media::resetPolarView()
{
    this->correctionVector = QLineF();

    m_Manager->alignModule()->zoomAlignView();
}

}
