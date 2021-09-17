/*
    SPDX-FileCopyrightText: 2019 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "wsmedia.h"

#include <QTimer>
#include <QTemporaryFile>
#include <KFormat>

#include "indi_debug.h"

namespace ISD
{

WSMedia::WSMedia(CCD *manager): m_Manager(manager)
{
    connect(&m_WebSocket, &QWebSocket::connected, this, &WSMedia::onConnected);
    connect(&m_WebSocket, &QWebSocket::disconnected, this, &WSMedia::onDisconnected);
    connect(&m_WebSocket, static_cast<void(QWebSocket::*)(QAbstractSocket::SocketError)>(&QWebSocket::error), this, &WSMedia::onError);

}

void WSMedia::connectServer()
{
    QUrl requestURL(m_URL);
    m_WebSocket.open(requestURL);

    qCInfo(KSTARS_INDI) << "Connecting to Websocket server at" << requestURL.toDisplayString();
}

void WSMedia::disconnectServer()
{
    m_WebSocket.close();
}

void WSMedia::onConnected()
{
    qCInfo(KSTARS_INDI) << "Connected to media Websocket server at" << m_URL.toDisplayString();

    connect(&m_WebSocket, &QWebSocket::textMessageReceived,  this, &WSMedia::onTextReceived);
    connect(&m_WebSocket, &QWebSocket::binaryMessageReceived, this, &WSMedia::onBinaryReceived);

    m_isConnected = true;
    m_ReconnectTries = 0;

    emit connected();
}

void WSMedia::onDisconnected()
{
    qCInfo(KSTARS_INDI) << "Disonnected from media Websocket server.";
    m_isConnected = false;

    disconnect(&m_WebSocket, &QWebSocket::textMessageReceived,  this, &WSMedia::onTextReceived);
    disconnect(&m_WebSocket, &QWebSocket::binaryMessageReceived, this, &WSMedia::onBinaryReceived);

    m_sendBlobs = true;

    emit disconnected();
}

void WSMedia::onError(QAbstractSocket::SocketError error)
{
    qCritical(KSTARS_INDI) << "Media Websocket connection error" << m_WebSocket.errorString();
    if (error == QAbstractSocket::RemoteHostClosedError ||
            error == QAbstractSocket::ConnectionRefusedError)
    {
        if (m_ReconnectTries++ < RECONNECT_MAX_TRIES)
            QTimer::singleShot(RECONNECT_INTERVAL, this, SLOT(connectServer()));
    }
}

void WSMedia::onTextReceived(const QString &message)
{
    extension = message;
}

void WSMedia::onBinaryReceived(const QByteArray &message)
{
    emit newFile(message, extension);
}

}
