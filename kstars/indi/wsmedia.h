/*
    SPDX-FileCopyrightText: 2019 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QtWebSockets/QWebSocket>
#include <memory>

namespace ISD
{
class CCD;
class WSMedia : public QObject
{
        Q_OBJECT

    public:
        WSMedia(CCD *manager);
        virtual ~WSMedia() = default;

        void setURL(const QUrl &url)
        {
            m_URL = url;
        }

    signals:
        void connected();
        void disconnected();
        void newFile(const QByteArray &message, const QString &extension);

    public slots:
        void connectServer();
        void disconnectServer();

    private slots:

        // Connection
        void onConnected();
        void onDisconnected();
        void onError(QAbstractSocket::SocketError error);

        // Communication
        void onTextReceived(const QString &message);
        void onBinaryReceived(const QByteArray &message);

    private:
        QWebSocket m_WebSocket;
        uint16_t m_ReconnectTries {0};
        CCD *m_Manager { nullptr };
        QUrl m_URL;

        bool m_isConnected { false };
        bool m_sendBlobs { true};
        QString extension;

        // Retry every 5 seconds in case remote server is down
        static const uint16_t RECONNECT_INTERVAL = 5000;
        // Retry for 1 hour before giving up
        static const uint16_t RECONNECT_MAX_TRIES = 720;
};
}
