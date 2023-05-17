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
        Q_OBJECT

    public:
        explicit NodeManager(const QUrl &serviceURL, const QUrl &wsURL);
        virtual ~NodeManager() = default;

        bool isConnected() const;
        const QUrl &serviceURL() const {return m_ServiceURL;}
        const QUrl &wsURL() const {return m_WSURL;}

        void setCredentials(const QString &username, const QString &password);
        void setAuthResponse(const QJsonObject &response)
        {
            m_AuthResponse = response;
        }

        Node *message() {return m_Nodes["message"];}
        Node *media() {return m_Nodes["media"];}
        Node *cloud() {return m_Nodes.contains("cloud") ? m_Nodes["cloud"] : nullptr;}

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
        QUrl m_ServiceURL, m_WSURL;
        QString m_Username, m_Password;

        QPointer<QNetworkAccessManager> m_NetworkManager;
        QMap<QString, Node*> m_Nodes;

        // Retry every 5 seconds in case remote server is down
        static const uint16_t RECONNECT_INTERVAL = 5000;
        // Retry authentication up to 3 times
        static const uint16_t RECONNECT_MAX_TRIES = 3;
        // Throttle interval
        static const uint16_t THROTTLE_INTERVAL = 1000;
};
}
