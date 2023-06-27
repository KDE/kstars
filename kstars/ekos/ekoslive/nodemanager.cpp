/* EkosLive Node

    SPDX-FileCopyrightText: 2023 Jasem Mutlaq <mutlaqja@ikarustech.com>

    Node Manager

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "nodemanager.h"
#include "version.h"

#include <QWebSocket>
#include <QUrlQuery>
#include <QTimer>
#include <QJsonDocument>

#include <KActionCollection>
#include <KLocalizedString>
#include <basedevice.h>
#include <QUuid>

namespace EkosLive
{

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
NodeManager::NodeManager(uint32_t mask)
{
    m_NetworkManager = new QNetworkAccessManager(this);
    connect(m_NetworkManager, &QNetworkAccessManager::finished, this, &NodeManager::onResult);

    // Configure nodes
    if (mask & Message)
        m_Nodes[Message] = new Node("message");
    if (mask & Media)
        m_Nodes[Media] = new Node("media");
    if (mask & Cloud)
        m_Nodes[Cloud] = new Node("cloud");

    for (auto &node : m_Nodes)
    {
        connect(node, &Node::connected, this, &NodeManager::setConnected);
        connect(node, &Node::disconnected, this, &NodeManager::setDisconnected);
    }
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void NodeManager::setURLs(const QUrl &service, const QUrl &websocket)
{
    m_ServiceURL = service;
    m_WebsocketURL = websocket;
    for (auto &node : m_Nodes)
        node->setProperty("url", m_WebsocketURL);
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
bool NodeManager::isConnected() const
{
    return std::all_of(m_Nodes.begin(), m_Nodes.end(), [](const auto & node)
    {
        return node->isConnected();
    });
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void NodeManager::setConnected()
{
    auto node = qobject_cast<Node*>(sender());
    if (!node)
        return;

    auto isConnected = std::all_of(m_Nodes.begin(), m_Nodes.end(), [](const auto & node)
    {
        return node->isConnected();
    });

    // Only emit once all nodes are connected.
    if (isConnected)
        emit connected();
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void NodeManager::setDisconnected()
{
    auto node = qobject_cast<Node*>(sender());
    if (!node)
        return;

    // Only emit once all nodes are disconnected.
    if (isConnected() == false)
        emit disconnected();
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void NodeManager::disconnectNodes()
{
    for (auto &node : m_Nodes)
        node->disconnectServer();
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void NodeManager::setCredentials(const QString &username, const QString &password)
{
    m_Username = username;
    m_Password = password;
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void NodeManager::authenticate()
{
    QNetworkRequest request;
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QUrl authURL(m_ServiceURL);
    authURL.setPath("/api/authenticate");

    request.setUrl(authURL);

    QJsonObject json = {{"username", m_Username}, {"password", m_Password}};

    auto postData = QJsonDocument(json).toJson(QJsonDocument::Compact);

    m_NetworkManager->post(request, postData);
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void NodeManager::onResult(QNetworkReply *reply)
{
    if (reply->error() != QNetworkReply::NoError)
    {
        // If connection refused, retry up to 3 times
        if (reply->error() == QNetworkReply::ConnectionRefusedError && m_ReconnectTries++ < RECONNECT_MAX_TRIES)
        {
            reply->deleteLater();
            QTimer::singleShot(RECONNECT_INTERVAL, this, &NodeManager::authenticate);
            return;
        }

        m_ReconnectTries = 0;
        emit authenticationError(i18n("Error authentication with Ekos Live server: %1", reply->errorString()));
        reply->deleteLater();
        return;
    }

    m_ReconnectTries = 0;
    QJsonParseError error;
    auto response = QJsonDocument::fromJson(reply->readAll(), &error);

    if (error.error != QJsonParseError::NoError)
    {
        emit authenticationError(i18n("Error parsing server response: %1", error.errorString()));
        reply->deleteLater();
        return;
    }

    m_AuthResponse = response.object();

    if (m_AuthResponse["success"].toBool() == false)
    {
        emit authenticationError(m_AuthResponse["message"].toString());
        reply->deleteLater();
        return;
    }

    // Cloud only supported for plan_id = 1
    if (m_AuthResponse["plan_id"].toString("2") == "2")
        m_Nodes.remove(Cloud);

    for (auto &node : m_Nodes)
    {
        node->setAuthResponse(m_AuthResponse);
        node->connectServer();
    }

    reply->deleteLater();
}

}
