/*  Ekos Live Cloud

    Copyright (C) 2018 Jasem Mutlaq <mutlaqja@ikarustech.com>

    Cloud Channel

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#include "cloud.h"
#include "commands.h"
#include "profileinfo.h"

#include "fitsviewer/fitsview.h"
#include "fitsviewer/fitsdata.h"
#include "fitsviewer/fpack.h"

#include "ekos_debug.h"

#include <KFormat>

namespace EkosLive
{

Cloud::Cloud(EkosManager *manager): m_Manager(manager)
{
    connect(&m_WebSocket, &QWebSocket::connected, this, &Cloud::onConnected);
    connect(&m_WebSocket, &QWebSocket::disconnected, this, &Cloud::onDisconnected);
    connect(&m_WebSocket, static_cast<void(QWebSocket::*)(QAbstractSocket::SocketError)>(&QWebSocket::error), this, &Cloud::onError);

}

void Cloud::connectServer()
{
    QUrl requestURL(m_URL);

    QUrlQuery query;
    query.addQueryItem("username", m_AuthResponse["username"].toString());
    query.addQueryItem("token", m_AuthResponse["token"].toString());
    query.addQueryItem("email", m_AuthResponse["email"].toString());
    query.addQueryItem("from_date", m_AuthResponse["from_date"].toString());
    query.addQueryItem("to_date", m_AuthResponse["to_date"].toString());
    query.addQueryItem("plan_id", m_AuthResponse["plan_id"].toString());

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
    m_ReconnectTries=0;

    emit connected();
}

void Cloud::onDisconnected()
{
    qCInfo(KSTARS_EKOS) << "Disonnected from Cloud Websocket server.";
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
}

void Cloud::sendPreviewImage(FITSView *view, const QString &uuid)
{
    const FITSData *imageData = view->getImageData();

    if (m_isConnected == false || m_Options[OPTION_SET_CLOUD_STORAGE] == false
                               || m_sendBlobs == false
                               || imageData->isTempFile())
        return;

    // Send complete metadata
    // Add file name and size
    QJsonObject metadata;
    // Skip empty or useless metadata
    for (FITSData::Record *oneRecord : imageData->getRecords())
    {
        if (oneRecord->key == "EXTEND" || oneRecord->key == "SIMPLE" || oneRecord->key == "COMMENT" ||
            oneRecord->key.isEmpty() || oneRecord->value.toString().isEmpty())
            continue;
        metadata.insert(oneRecord->key.toLower(), QJsonValue::fromVariant(oneRecord->value));
    }

    // Filename only without path
    QString filename = QFileInfo(imageData->getFilename()).fileName();

    // Add filename and size as wells
    metadata.insert("uuid", uuid);
    metadata.insert("filename", filename);
    metadata.insert("filesize", static_cast<int>(imageData->getSize()));
    // Must set Content-Disposition so
    metadata.insert("Content-Disposition", QString("attachment;filename=%1.fz").arg(filename));
    m_WebSocket.sendTextMessage(QJsonDocument(metadata).toJson(QJsonDocument::Compact));

    // Use cfitsio pack to compress the file first
    filename = QDir::tempPath() + QString("/ekoslivecloud%1").arg(uuid);

    fpstate	fpvar;
    std::vector<std::string> arguments = {"fpack", imageData->getFilename().toLatin1().data()};
    std::vector<char *> arglist;
    for (const auto& arg : arguments)
        arglist.push_back((char*)arg.data());
    arglist.push_back(nullptr);

    int argc = arglist.size() - 1;
    char **argv = arglist.data();

    // TODO: Check for errors
    fp_init (&fpvar);
    fp_get_param (argc, argv, &fpvar);
    fp_preflight (argc, argv, FPACK, &fpvar);
    fp_loop (argc, argv, FPACK, filename.toLatin1().data(), fpvar);

    QString newCompressedFile = filename + ".fz";
    // Upload the compressed image
    QFile image(newCompressedFile);
    if (image.open(QIODevice::ReadOnly))
        m_WebSocket.sendBinaryMessage(image.readAll());
    image.close();

    // Remove from disk
    QFile::remove(newCompressedFile);
}


}
