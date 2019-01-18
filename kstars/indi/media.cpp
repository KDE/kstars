/*  INDI WebSocket Handler

    Copyright (C) 2019 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#include "media.h"

#include <QTimer>
#include <QTemporaryFile>
#include <KFormat>

#include "indi_debug.h"

namespace ISD
{

Media::Media(CCD *manager): m_Manager(manager)
{
    connect(&m_WebSocket, &QWebSocket::connected, this, &Media::onConnected);
    connect(&m_WebSocket, &QWebSocket::disconnected, this, &Media::onDisconnected);
    connect(&m_WebSocket, static_cast<void(QWebSocket::*)(QAbstractSocket::SocketError)>(&QWebSocket::error), this, &Media::onError);

}

void Media::connectServer()
{
    QUrl requestURL(m_URL);
    m_WebSocket.open(requestURL);

    qCInfo(KSTARS_INDI) << "Connecting to Websocket server at" << requestURL.toDisplayString();
}

void Media::disconnectServer()
{
    m_WebSocket.close();
}

void Media::onConnected()
{
    qCInfo(KSTARS_INDI) << "Connected to media Websocket server at" << m_URL.toDisplayString();

    connect(&m_WebSocket, &QWebSocket::textMessageReceived,  this, &Media::onTextReceived);
    connect(&m_WebSocket, &QWebSocket::binaryMessageReceived, this, &Media::onBinaryReceived);

    m_isConnected = true;
    m_ReconnectTries=0;

    emit connected();
}

void Media::onDisconnected()
{
    qCInfo(KSTARS_INDI) << "Disonnected from media Websocket server.";
    m_isConnected = false;

    disconnect(&m_WebSocket, &QWebSocket::textMessageReceived,  this, &Media::onTextReceived);
    disconnect(&m_WebSocket, &QWebSocket::binaryMessageReceived, this, &Media::onBinaryReceived);

    m_sendBlobs = true;

    emit disconnected();
}

void Media::onError(QAbstractSocket::SocketError error)
{
    qCritical(KSTARS_INDI) << "Media Websocket connection error" << m_WebSocket.errorString();
    if (error == QAbstractSocket::RemoteHostClosedError ||
        error == QAbstractSocket::ConnectionRefusedError)
    {
        if (m_ReconnectTries++ < RECONNECT_MAX_TRIES)
            QTimer::singleShot(RECONNECT_INTERVAL, this, SLOT(connectServer()));
    }
}

void Media::onTextReceived(const QString &message)
{
    qCInfo(KSTARS_INDI) << "Media Text Websocket Message" << message;
    extension = message;
}

void Media::onBinaryReceived(const QByteArray &message)
{
    emit newFile(message, extension);
}

}
