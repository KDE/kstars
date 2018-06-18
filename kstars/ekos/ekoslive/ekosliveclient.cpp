/*  Ekos Live Client
    Copyright (C) 2018 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#include "ekosliveclient.h"
#include "ekos_debug.h"

EkosLiveClient::EkosLiveClient()
{
    connect(&m_webSocket, &QWebSocket::connected, this, &EkosLiveClient::onConnected);
    connect(&m_webSocket, &QWebSocket::disconnected, this, &EkosLiveClient::closed);
    connect(&m_webSocket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error), [=]()
    {
        qCritical(KSTARS_EKOS) << "Websocked connection error" << m_webSocket.errorString();
    });
}

void EkosLiveClient::connectToServer(const QUrl &url)
{
    m_url = url;
    m_webSocket.open(url);
    qCInfo(KSTARS_EKOS) << "Connecting to Websocket server at" << m_url.toDisplayString();
}

void EkosLiveClient::sendMessage(const QString &msg)
{
    m_webSocket.sendTextMessage(msg);
}
void EkosLiveClient::onConnected()
{
    qCInfo(KSTARS_EKOS) << "Connected to Websocket server at" << m_url.toDisplayString();

    connect(&m_webSocket, &QWebSocket::textMessageReceived,  this, &EkosLiveClient::onTextMessageReceived);
    m_webSocket.sendTextMessage(QStringLiteral("Hello Qt App"));
}

void EkosLiveClient::onTextMessageReceived(QString message)
{
    qCInfo(KSTARS_EKOS) << "Websocket Message" << message;
    m_webSocket.close();
}
