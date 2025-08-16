/*
    SPDX-FileCopyrightText: 2023 Jasem Mutlaq <mutlaqja@ikarustech.com>

    Message Channel

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QtWebSockets/QWebSocket>
#include <QJsonObject>
#include <memory>

namespace EkosLive
{
class Node : public QObject
{
        Q_PROPERTY(QString name MEMBER m_Name)
        Q_PROPERTY(QUrl url MEMBER m_URL READ url)
        Q_OBJECT

    public:
        explicit Node(const QString &name);
        virtual ~Node() = default;

        const QUrl url() const
        {
            return m_URL;
        }

        void setClientState(bool value)
        {
            m_ClientState = value;
        }
        void sendResponse(const QString &command, const QJsonObject &payload);
        void sendResponse(const QString &command, const QJsonArray &payload);
        void sendResponse(const QString &command, const QString &payload);
        void sendResponse(const QString &command, bool payload);
        void sendEvent(const QString &command, const QJsonObject &payload);

        void sendTextMessage(const QString &message);
        void sendBinaryMessage(const QByteArray &message);
        bool isConnected() const
        {
            return m_isConnected;
        }

        void setAuthResponse(const QJsonObject &response)
        {
            m_AuthResponse = response;
        }

    signals:
        void connected();
        void disconnected();
        void onTextReceived(const QString &message);
        void onBinaryReceived(const QByteArray &message);

    public slots:
        void connectServer();
        void disconnectServer();

    private slots:
        // Connection
        void onConnected();
        void onDisconnected();
        void onError(QAbstractSocket::SocketError error);

    private:
        QWebSocket m_WebSocket;
        QJsonObject m_AuthResponse;
        uint16_t m_ReconnectTries {0};
        QUrl m_URL;
        QString m_Name;
        QString m_Path;

        bool m_isConnected { false };
        bool m_ClientState { false };
        bool m_sendBlobs { true};

        QMap<int, bool> m_Options;

        // Retry every 5 seconds in case remote server is down
        static const uint16_t RECONNECT_INTERVAL = 5000;
        // Retry for 1 hour before giving up
        static const uint16_t RECONNECT_MAX_TRIES = 720;
        // Throttle interval
        static const uint16_t THROTTLE_INTERVAL = 1000;
};
}
