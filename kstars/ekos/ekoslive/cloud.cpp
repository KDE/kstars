/*
    SPDX-FileCopyrightText: 2018 Jasem Mutlaq <mutlaqja@ikarustech.com>

    Cloud Channel

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "cloud.h"
#include "commands.h"
#include "profileinfo.h"

#include "fitsviewer/fitsview.h"
#include "fitsviewer/fitsdata.h"
#include "fitsviewer/fpack.h"

#include "ekos_debug.h"

#include <QtConcurrent>
#include <QFutureWatcher>
#include <KFormat>

namespace EkosLive
{

Cloud::Cloud(Ekos::Manager * manager): m_Manager(manager)
{
    connect(&m_WebSocket, &QWebSocket::connected, this, &Cloud::onConnected);
    connect(&m_WebSocket, &QWebSocket::disconnected, this, &Cloud::onDisconnected);
    connect(&m_WebSocket, static_cast<void(QWebSocket::*)(QAbstractSocket::SocketError)>(&QWebSocket::error), this,
            &Cloud::onError);

    connect(&watcher, &QFutureWatcher<bool>::finished, this, &Cloud::sendImage, Qt::UniqueConnection);

    connect(this, &Cloud::newMetadata, this, &Cloud::uploadMetadata);
    connect(this, &Cloud::newImage, this, &Cloud::uploadImage);
}

void Cloud::connectServer()
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

    requestURL.setPath("/cloud/ekos");
    requestURL.setQuery(query);

    m_WebSocket.open(requestURL);

    qCInfo(KSTARS_EKOS) << "Connecting to cloud websocket server at" << requestURL.toDisplayString();
}

void Cloud::disconnectServer()
{
    m_WebSocket.close();
}

void Cloud::onConnected()
{
    qCInfo(KSTARS_EKOS) << "Connected to Cloud Websocket server at" << m_URL.toDisplayString();

    connect(&m_WebSocket, &QWebSocket::textMessageReceived,  this, &Cloud::onTextReceived);

    m_isConnected = true;
    m_ReconnectTries = 0;

    emit connected();
}

void Cloud::onDisconnected()
{
    qCInfo(KSTARS_EKOS) << "Disconnected from Cloud Websocket server.";
    m_isConnected = false;

    disconnect(&m_WebSocket, &QWebSocket::textMessageReceived,  this, &Cloud::onTextReceived);

    m_sendBlobs = true;

    for (const QString &oneFile : temporaryFiles)
        QFile::remove(oneFile);
    temporaryFiles.clear();

    emit disconnected();
}

void Cloud::onError(QAbstractSocket::SocketError error)
{
    qCritical(KSTARS_EKOS) << "Cloud Websocket connection error" << m_WebSocket.errorString();
    if (error == QAbstractSocket::RemoteHostClosedError ||
            error == QAbstractSocket::ConnectionRefusedError)
    {
        if (m_ReconnectTries++ < RECONNECT_MAX_TRIES)
            QTimer::singleShot(RECONNECT_INTERVAL, this, SLOT(connectServer()));
    }
}

void Cloud::onTextReceived(const QString &message)
{
    qCInfo(KSTARS_EKOS) << "Cloud Text Websocket Message" << message;
    QJsonParseError error;
    auto serverMessage = QJsonDocument::fromJson(message.toLatin1(), &error);
    if (error.error != QJsonParseError::NoError)
    {
        qCWarning(KSTARS_EKOS) << "Ekos Live Parsing Error" << error.errorString();
        return;
    }

    const QJsonObject msgObj = serverMessage.object();
    const QString command = msgObj["type"].toString();
    //    const QJsonObject payload = msgObj["payload"].toObject();

    //    if (command == commands[ALIGN_SET_FILE_EXTENSION])
    //        extension = payload["ext"].toString();
    if (command == commands[SET_BLOBS])
        m_sendBlobs = msgObj["payload"].toBool();
    else if (command == commands[LOGOUT])
        disconnectServer();
}

void Cloud::upload(const QSharedPointer<FITSData> &data, const QString &uuid)
{
    if (m_isConnected == false || m_Options[OPTION_SET_CLOUD_STORAGE] == false  || m_sendBlobs == false)
        return;

    m_UUID = uuid;
    m_ImageData = data;
    sendImage();
}

void Cloud::upload(const QString &filename, const QString &uuid)
{
    if (m_isConnected == false || m_Options[OPTION_SET_CLOUD_STORAGE] == false  || m_sendBlobs == false)
        return;

    watcher.waitForFinished();
    m_UUID = uuid;
    m_ImageData.reset(new FITSData(), &QObject::deleteLater);
    QFuture<bool> result = m_ImageData->loadFromFile(filename);
    watcher.setFuture(result);
}

void Cloud::sendImage()
{
    QtConcurrent::run(this, &Cloud::asyncUpload);
}

void Cloud::asyncUpload()
{
    // Send complete metadata
    // Add file name and size
    QJsonObject metadata;
    // Skip empty or useless metadata
    for (const auto &oneRecord : m_ImageData->getRecords())
    {
        if (oneRecord.key.isEmpty() || oneRecord.value.toString().isEmpty())
            continue;
        metadata.insert(oneRecord.key.toLower(), QJsonValue::fromVariant(oneRecord.value));
    }

    // Filename only without path
    QString filepath = m_ImageData->isCompressed() ? m_ImageData->compressedFilename() : m_ImageData->filename();
    QString filenameOnly = QFileInfo(filepath).fileName();

    // Add filename and size as wells
    metadata.insert("uuid", m_UUID);
    metadata.insert("filename", filenameOnly);
    metadata.insert("filesize", static_cast<int>(m_ImageData->size()));
    // Must set Content-Disposition so
    if (m_ImageData->isCompressed())
        metadata.insert("Content-Disposition", QString("attachment;filename=%1").arg(filenameOnly));
    else
        metadata.insert("Content-Disposition", QString("attachment;filename=%1.fz").arg(filenameOnly));

    emit newMetadata(QJsonDocument(metadata).toJson(QJsonDocument::Compact));
    //m_WebSocket.sendTextMessage(QJsonDocument(metadata).toJson(QJsonDocument::Compact));

    qCInfo(KSTARS_EKOS) << "Uploading file to the cloud with metadata" << metadata;

    QString compressedFile = filepath;
    // Use cfitsio pack to compress the file first
    if (m_ImageData->isCompressed() == false)
    {
        compressedFile = QDir::tempPath() + QString("/ekoslivecloud%1").arg(m_UUID);

        int isLossLess = 0;
        fpstate	fpvar;
        fp_init (&fpvar);
        if (fp_pack(filepath.toLatin1().data(), compressedFile.toLatin1().data(), fpvar, &isLossLess) < 0)
        {
            if (filepath.startsWith(QDir::tempPath()))
                QFile::remove(filepath);
            qCCritical(KSTARS_EKOS) << "Cloud upload failed. Failed to compress" << filepath;
            return;
        }
    }

    // Upload the compressed image
    QFile image(compressedFile);
    if (image.open(QIODevice::ReadOnly))
    {
        //m_WebSocket.sendBinaryMessage(image.readAll());
        emit newImage(image.readAll());
        qCInfo(KSTARS_EKOS) << "Uploaded" << compressedFile << " to the cloud";
    }
    image.close();

    // Remove from disk if temporary
    if (compressedFile != filepath && compressedFile.startsWith(QDir::tempPath()))
        QFile::remove(compressedFile);

    m_ImageData.reset();
}

void Cloud::uploadMetadata(const QByteArray &metadata)
{
    m_WebSocket.sendTextMessage(metadata);
}

void Cloud::uploadImage(const QByteArray &image)
{
    m_WebSocket.sendBinaryMessage(image);
}

void Cloud::setOptions(QMap<int, bool> options)
{
    bool cloudEnabled = m_Options[OPTION_SET_CLOUD_STORAGE];
    m_Options = options;

    // In case cloud storage is toggled, inform cloud
    // websocket channel of this change.
    if (cloudEnabled != m_Options[OPTION_SET_CLOUD_STORAGE])
    {
        bool enabled = m_Options[OPTION_SET_CLOUD_STORAGE];
        QJsonObject payload = {{"value", enabled}};
        QJsonObject message =
        {
            {"type",  commands[OPTION_SET_CLOUD_STORAGE]},
            {"payload", payload}
        };

        m_WebSocket.sendTextMessage(QJsonDocument(message).toJson(QJsonDocument::Compact));
    }
}

}
