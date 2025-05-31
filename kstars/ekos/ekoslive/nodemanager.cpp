/* EkosLive Node

    SPDX-FileCopyrightText: 2023 Jasem Mutlaq <mutlaqja@ikarustech.com>

    Node Manager

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "nodemanager.h"
#include "version.h"
#include "../../auxiliary/ksutils.h"
#include "ekos_debug.h"

#include <QWebSocket>
#include <QDateTime>
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
    {
        // If we were re-authenticating, mark it as complete now that all nodes are connected.
        if (m_isReauthenticating)
        {
            qCInfo(KSTARS_EKOS) << "NodeManager for URL" << m_ServiceURL.toDisplayString() <<
                                   "successfully re-authenticated and connected all nodes.";
            setIsReauthenticating(false);
        }
        emit connected();
    }
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
    qCDebug(KSTARS_EKOS) << "NodeManager(" << m_ServiceURL.toDisplayString() <<
                            "): Entering authenticate(). Current re-auth status:" << m_isReauthenticating;
    if (m_isReauthenticating) // Check if already in progress
    {
        // Already trying to authenticate this manager, prevent stacking requests.
        qCInfo(KSTARS_EKOS) << "NodeManager::authenticate called while already in progress for URL:" <<
                            m_ServiceURL.toDisplayString() << ". Ignoring.";
        return;
    }

    m_isReauthenticating = true; // Set the flag at the start of a new authentication attempt.

    QNetworkRequest request;
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QUrl authURL(m_ServiceURL);
    authURL.setPath("/api/authenticate");

    request.setUrl(authURL);

    QJsonObject json = {{"username", m_Username}, {"password", m_Password}, {"machine_id", KSUtils::getMachineID()}};

    auto postData = QJsonDocument(json).toJson(QJsonDocument::Compact);

    m_NetworkManager->post(request, postData);
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
bool NodeManager::isTokenNearlyExpired(int bufferSeconds) const
{
    if (m_AuthResponse.isEmpty() || m_tokenExpiryTimestamp == 0)
    {
        qCWarning(KSTARS_EKOS) << "NodeManager: Token is considered expired (empty auth response or no expiry timestamp).";
        return true; // No token or expiry unknown, assume needs auth
    }

    qlonglong currentTime = QDateTime::currentSecsSinceEpoch();
    bool expired = currentTime >= (m_tokenExpiryTimestamp - bufferSeconds);
    if (expired)
    {
        qCWarning(KSTARS_EKOS) << "NodeManager: Token is nearly expired. CurrentTime:" << currentTime
                               << "Expiry:" << m_tokenExpiryTimestamp << "Buffer:" << bufferSeconds;
    }
    return expired;
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void NodeManager::clearAuthentication()
{
    qCInfo(KSTARS_EKOS) << "NodeManager: Clearing authentication data.";
    m_AuthResponse = QJsonObject();
    m_tokenExpiryTimestamp = 0;
    // Also disconnect nodes if they are connected with old auth
    // disconnectNodes(); // Caller (EkosLiveClient) should manage this if needed before re-auth.
}
///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void NodeManager::onResult(QNetworkReply *reply)
{
    qCDebug(KSTARS_EKOS) << "NodeManager(" << m_ServiceURL.toDisplayString() << "): Entering onResult(). Reply error:" <<
                         reply->error() << "String:" << reply->errorString();
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
        // Reset flag on authentication error
        setIsReauthenticating(false);
        emit authenticationError(i18n("Error authentication with Ekos Live server: %1", reply->errorString()));
        reply->deleteLater();
        return;
    }

    m_ReconnectTries = 0;
    QJsonParseError error;
    auto response = QJsonDocument::fromJson(reply->readAll(), &error);

    if (error.error != QJsonParseError::NoError)
    {
        // Reset flag on authentication error (parse error)
        setIsReauthenticating(false);
        emit authenticationError(i18n("Error parsing server response: %1", error.errorString()));
        reply->deleteLater();
        return;
    }

    m_AuthResponse = response.object();

    if (m_AuthResponse["success"].toBool() == false)
    {
        // Reset flag on authentication error (server denied)
        m_tokenExpiryTimestamp = 0; // Clear any old expiry on auth failure
        setIsReauthenticating(false);
        emit authenticationError(m_AuthResponse["message"].toString());
        reply->deleteLater();
        return;
    }

    // Store token expiry
    m_tokenExpiryTimestamp = KSUtils::getJwtExpiryTimestamp(m_AuthResponse["token"].toString());
    if (m_tokenExpiryTimestamp > 0)
    {
        qCInfo(KSTARS_EKOS) << "NodeManager for URL" << m_ServiceURL.toDisplayString() << "authenticated. Token expiry:"
                            << QDateTime::fromSecsSinceEpoch(m_tokenExpiryTimestamp).toString(Qt::ISODate);
    }
    else
    {
        qCWarning(KSTARS_EKOS) << "NodeManager for URL" << m_ServiceURL.toDisplayString() <<
                                  "authenticated, but failed to parse token expiry.";
        // Decide if this is a critical failure. For now, proceed but token checks might fail.
    }

    // If we were re-authenticating, NodeManager::setConnected will reset the flag once all nodes connect.
    // If not re-authenticating, this is a fresh authentication.

    qCDebug(KSTARS_EKOS) << "NodeManager(" << m_ServiceURL.toDisplayString() << "): Authenticated. Processing" <<
                         m_Nodes.count() << "nodes for reconnection.";
    for (auto &node : m_Nodes)
    {
        // To get node name here, Node class would need a public name() getter.
        // For now, let's log its pointer. The actual name will be logged from Node::connectServer().
        qCDebug(KSTARS_EKOS) << "NodeManager(" << m_ServiceURL.toDisplayString() << "): Processing node with pointer" << node <<
                                "- Calling setAuthResponse and connectServer.";
        node->setAuthResponse(m_AuthResponse);
        node->connectServer();
    }

    reply->deleteLater();
}

}
