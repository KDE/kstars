/*
    SPDX-FileCopyrightText: 2018 Jasem Mutlaq <mutlaqja@ikarustech.com>

    Cloud Channel

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QtWebSockets/QWebSocket>
#include <memory>

#include "ekos/ekos.h"
#include "ekos/manager.h"

class FITSView;

namespace EkosLive
{
class Cloud : public QObject
{
        Q_OBJECT

    public:
        explicit Cloud(Ekos::Manager * manager);
        virtual ~Cloud() = default;

        void sendResponse(const QString &command, const QJsonObject &payload);
        void sendResponse(const QString &command, const QJsonArray &payload);

        void setAuthResponse(const QJsonObject &response)
        {
            m_AuthResponse = response;
        }
        void setURL(const QUrl &url)
        {
            m_URL = url;
        }

        void registerCameras();

        // Ekos Cloud Message to User
        void upload(const QString &filename, const QString &uuid);
        void upload(const QSharedPointer<FITSData> &data, const QString &uuid);

    signals:
        void connected();
        void disconnected();

        void newMetadata(const QByteArray &metadata);
        void newImage(const QByteArray &image);

    public slots:
        void connectServer();
        void disconnectServer();
        void setOptions(QMap<int, bool> options);

    private slots:
        // Connection
        void onConnected();
        void onDisconnected();
        void onError(QAbstractSocket::SocketError error);

        // Communication
        void onTextReceived(const QString &message);

        // Send image
        void sendImage();

        // Metadata and Image upload
        void uploadMetadata(const QByteArray &metadata);
        void uploadImage(const QByteArray &image);

    private:
        void asyncUpload();

        QWebSocket m_WebSocket;
        QJsonObject m_AuthResponse;
        uint16_t m_ReconnectTries {0};
        Ekos::Manager * m_Manager { nullptr };
        QUrl m_URL;
        QString m_UUID;

        QSharedPointer<FITSData> m_ImageData;
        QFutureWatcher<bool> watcher;

        QString extension;
        QStringList temporaryFiles;

        bool m_isConnected {false};
        bool m_sendBlobs {true};

        QMap<int, bool> m_Options;

        // Image width for high-bandwidth setting
        static const uint16_t HB_WIDTH = 640;
        // Image high bandwidth image quality (jpg)
        static const uint8_t HB_IMAGE_QUALITY = 76;
        // Video high bandwidth video quality (jpg)
        static const uint8_t HB_VIDEO_QUALITY = 64;

        // Retry every 5 seconds in case remote server is down
        static const uint16_t RECONNECT_INTERVAL = 5000;
        // Retry for 1 hour before giving up
        static const uint16_t RECONNECT_MAX_TRIES = 720;
};
}
