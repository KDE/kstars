/*
    SPDX-FileCopyrightText: 2023 Jasem Mutlaq <mutlaqja@ikarustech.com>

    Node Manager

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QtWebSockets/QWebSocket>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QNetworkReply>
#include <memory>

#include "node.h"

namespace EkosLive
{
class NodeManager : public QObject
{
    Q_PROPERTY(QUrl serviceURL MEMBER m_ServiceURL)
    Q_PROPERTY(QUrl websocketURL MEMBER m_WebsocketURL)
    Q_OBJECT

    public:
        explicit NodeManager(uint32_t mask);
        virtual ~NodeManager() = default;

        bool isConnected() const;

        typedef enum
        {
            Message = 1 << 0,
            Media   = 1 << 1,
            Cloud   = 1 << 2
        } Channels;

        void setURLs(const QUrl &service, const QUrl &websocket);
        void setCredentials(const QString &username, const QString &password);
        void setAuthResponse(const QJsonObject &response)
        {
            m_AuthResponse = response;
        }

        Node *message() {return m_Nodes[Message];}
        Node *media() {return m_Nodes[Media];}
        Node *cloud() {return m_Nodes.contains(Cloud) ? m_Nodes[Cloud] : nullptr;}

    signals:
        void connected();
        void disconnected();
        void authenticationError(QString);

    public slots:
        void authenticate();
        void disconnectNodes();

        void setConnected();
        void setDisconnected();

      protected slots:
       void onResult(QNetworkReply *reply);

      private:
        QJsonObject m_AuthResponse;
        uint16_t m_ReconnectTries {0};
        QUrl m_ServiceURL, m_WebsocketURL;
        QString m_Username, m_Password;

        QPointer<QNetworkAccessManager> m_NetworkManager;
        QMap<Channels, Node*> m_Nodes;

        // Retry every 5 seconds in case remote server is down
        static const uint16_t RECONNECT_INTERVAL = 5000;
        // Retry authentication up to 3 times
        static const uint16_t RECONNECT_MAX_TRIES = 3;
        // Throttle interval
        static const uint16_t THROTTLE_INTERVAL = 1000;
};
}
