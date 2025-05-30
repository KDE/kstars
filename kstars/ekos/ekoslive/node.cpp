/* EkosLive Node

    SPDX-FileCopyrightText: 2023 Jasem Mutlaq <mutlaqja@ikarustech.com>

    Communication Node

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "node.h"
#include "version.h"
#include "Options.h"
#include "ekos_debug.h"

#include <QWebSocket>
#include <QUrlQuery>
#include <QTimer>
#include <QJsonDocument>

#include <KActionCollection>
#include <basedevice.h>
#include <QUuid>

namespace EkosLive
{
Node::Node(const QString &name) : m_Name(name)
{
    connect(&m_WebSocket, &QWebSocket::connected, this, &Node::onConnected);
    connect(&m_WebSocket, &QWebSocket::disconnected, this, &Node::onDisconnected);
    connect(&m_WebSocket, static_cast<void(QWebSocket::*)(QAbstractSocket::SocketError)>(&QWebSocket::error), this,
            &Node::onError);

    m_Path = "/" + m_Name + "/ekos";
}

void Node::connectServer()
{
    qCDebug(KSTARS_EKOS) << "Node(" << m_Name << "): Entered connectServer(). Base URL:" << m_URL.toDisplayString() << "Path:"
                         << m_Path;
    QUrl requestURL(m_URL);

    QUrlQuery query;
    query.addQueryItem("observatory", Options::ekosLiveObservatory());
    query.addQueryItem("username", m_AuthResponse["username"].toString());
    query.addQueryItem("token", m_AuthResponse["token"].toString());
    if (m_AuthResponse.contains("remoteToken"))
        query.addQueryItem("remoteToken", m_AuthResponse["remoteToken"].toString());
    if (m_AuthResponse.contains("machine_id"))
        query.addQueryItem("machine_id", m_AuthResponse["machine_id"].toString());
    query.addQueryItem("cloudEnabled", Options::ekosLiveCloud() ? "true" : "false");
    query.addQueryItem("email", m_AuthResponse["email"].toString());
    query.addQueryItem("from_date", m_AuthResponse["from_date"].toString());
    query.addQueryItem("to_date", m_AuthResponse["to_date"].toString());
    query.addQueryItem("plan_id", m_AuthResponse["plan_id"].toString());
    query.addQueryItem("type", m_AuthResponse["type"].toString());
    query.addQueryItem("version", KSTARS_VERSION);


    requestURL.setPath(m_Path);
    requestURL.setQuery(query);

    if (m_Name == "message" || m_Name == "Message")   // Log more details for message node
    {
        qCDebug(KSTARS_EKOS) << "Node(" << m_Name << "): About to open websocket. Request URL:" << requestURL.toDisplayString() <<
                                "Is valid:" << requestURL.isValid();
        qCDebug(KSTARS_EKOS) << "Node(" << m_Name << "): Auth Token used:" << m_AuthResponse["token"].toString().left(
                                 10) << "..."; // Log part of token
    }

    m_WebSocket.open(requestURL);

    qCInfo(KSTARS_EKOS) << "Connecting to " << m_Name << "Websocket server at" << requestURL.toDisplayString();
}

void Node::disconnectServer()
{
    m_WebSocket.close();
}

void Node::onConnected()
{
    //qCInfo(KSTARS_EKOS) << "Connected to" << m_Name << "Websocket server at" << m_URL.toDisplayString();

    m_isConnected = true;
    m_ReconnectTries = 0;

    connect(&m_WebSocket, &QWebSocket::textMessageReceived,  this, &Node::onTextReceived, Qt::UniqueConnection);
    connect(&m_WebSocket, &QWebSocket::binaryMessageReceived,  this, &Node::onBinaryReceived, Qt::UniqueConnection);

    emit connected();
}

void Node::onDisconnected()
{
    qCInfo(KSTARS_EKOS) << "Disconnected from" << m_Name << "Websocket server at" << m_URL.toDisplayString();
    m_isConnected = false;

    disconnect(&m_WebSocket, &QWebSocket::textMessageReceived,  this, &Node::onTextReceived);
    disconnect(&m_WebSocket, &QWebSocket::binaryMessageReceived,  this, &Node::onBinaryReceived);

    emit disconnected();
}

void Node::onError(QAbstractSocket::SocketError error)
{
    qCritical(KSTARS_EKOS) << m_Name << "Websocket connection error from" << m_URL.toDisplayString() << ":" <<
                           m_WebSocket.errorString();
    if (error == QAbstractSocket::RemoteHostClosedError ||
            error == QAbstractSocket::ConnectionRefusedError)
    {
        if (m_ReconnectTries++ < RECONNECT_MAX_TRIES)
            QTimer::singleShot(RECONNECT_INTERVAL, this, &Node::connectServer);
    }
}


///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Node::sendResponse(const QString &command, const QJsonObject &payload)
{
    if (m_isConnected == false)
        return;

    m_WebSocket.sendTextMessage(QJsonDocument({{"type", command}, {"payload", payload}}).toJson(QJsonDocument::Compact));
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Node::sendResponse(const QString &command, const QJsonArray &payload)
{
    if (m_isConnected == false)
        return;

    m_WebSocket.sendTextMessage(QJsonDocument({{"type", command}, {"payload", payload}}).toJson(QJsonDocument::Compact));
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Node::sendResponse(const QString &command, const QString &payload)
{
    if (m_isConnected == false)
        return;

    m_WebSocket.sendTextMessage(QJsonDocument({{"type", command}, {"payload", payload}}).toJson(QJsonDocument::Compact));
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Node::sendResponse(const QString &command, bool payload)
{
    if (m_isConnected == false)
        return;

    m_WebSocket.sendTextMessage(QJsonDocument({{"type", command}, {"payload", payload}}).toJson(QJsonDocument::Compact));
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Node::sendTextMessage(const QString &message)
{
    if (m_isConnected == false)
        return;
    m_WebSocket.sendTextMessage(message);
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Node::sendBinaryMessage(const QByteArray &message)
{
    if (m_isConnected == false)
        return;
    m_WebSocket.sendBinaryMessage(message);
}

}
