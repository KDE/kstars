/*  Ekos Live Client
    Copyright (C) 2018 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#include "ekosliveclient.h"
#include "ekos_debug.h"
#include "ekos/ekosmanager.h"
#include "profileinfo.h"

#include <QJsonDocument>
#include <QJsonObject>

EkosLiveClient::EkosLiveClient(EkosManager *manager) : QDialog(manager), m_Manager(manager)
{
    setupUi(this);

    connect(connectB, &QPushButton::clicked, [=]()
    {
        if (isConnected)
        {
            m_webSocket.close();
            connectB->setText(i18n("Connect"));
        }
        else
        {
            connectToServer(QUrl("ws://localhost:3000"));
            connectB->setText(i18n("Disconnect"));
        }

    });

    connect(&m_webSocket, &QWebSocket::connected, this, &EkosLiveClient::onConnected);
    connect(&m_webSocket, &QWebSocket::disconnected, this, &EkosLiveClient::onDisconnected);
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

    isConnected = true;

    connect(&m_webSocket, &QWebSocket::textMessageReceived,  this, &EkosLiveClient::onTextMessageReceived);
    connect(&m_webSocket, &QWebSocket::binaryMessageReceived, this, &EkosLiveClient::onBinaryMessageReceived);

    //sendMessage(QLatin1Literal("##Ekos##"));
    //sendProfiles();

    emit connected();
}

void EkosLiveClient::onDisconnected()
{
    qCInfo(KSTARS_EKOS) << "Disonnected to Websocket server at" << m_url.toDisplayString();

    isConnected = false;

    disconnect(&m_webSocket, &QWebSocket::textMessageReceived,  this, &EkosLiveClient::onTextMessageReceived);
    disconnect(&m_webSocket, &QWebSocket::binaryMessageReceived, this, &EkosLiveClient::onBinaryMessageReceived);

    emit disconnected();
}

void EkosLiveClient::onTextMessageReceived(const QString &message)
{
    qCInfo(KSTARS_EKOS) << "Websocket Message" << message;
    QJsonParseError error;
    auto serverMessage = QJsonDocument::fromJson(message.toLatin1(), &error);
    if (error.error != QJsonParseError::NoError)
    {
        qCWarning(KSTARS_EKOS) << "Ekos Live Parsing Error" << error.errorString();
        return;
    }

    QString command = serverMessage.object().value("command").toString();

    if (command == commands[GET_PROFILES])
        sendProfiles();
}

void EkosLiveClient::onBinaryMessageReceived(const QByteArray &message)
{
    qCInfo(KSTARS_EKOS) << "Websocket Message" << message;
}

void EkosLiveClient::sendProfiles()
{
    QJsonArray profileArray;

    for (const auto &oneProfile: m_Manager->profiles)
        profileArray.append(oneProfile->toJson());

    QJsonObject profiles = {{"type", commands[GET_PROFILES]}, {"payload",profileArray}};
    auto profileDoc = QJsonDocument(profiles);

    sendMessage(profileDoc.toJson());
}
