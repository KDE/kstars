/*
    SPDX-FileCopyrightText: 2018 Jasem Mutlaq <mutlaqja@ikarustech.com>

    Media Channel

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
class Media : public QObject
{
        Q_OBJECT

    public:
        explicit Media(Ekos::Manager * manager);
        virtual ~Media() = default;

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

        // Ekos Media Message to User
        //void sendPreviewJPEG(const QString &filename, QJsonObject metadata);
        void sendPreviewImage(const QString &filename, const QString &uuid);
        void sendPreviewImage(const QSharedPointer<FITSData> &data, const QString &uuid);
        void sendPreviewImage(FITSView * view, const QString &uuid);
        void sendUpdatedFrame(FITSView * view);
        void sendModuleFrame(FITSView * view);

    signals:
        void connected();
        void disconnected();

        void newBoundingRect(QRect rect, QSize view, double currentZoom);
        void newMetadata(const QByteArray &metadata);
        void newImage(const QByteArray &image);

    public slots:
        void connectServer();
        void disconnectServer();

        // Capture
        void sendVideoFrame(const QSharedPointer<QImage> &frame);

        // Options
        void setOptions(QMap<int, bool> options)
        {
            m_Options = options;
        }

        // Correction Vector
        void setCorrectionVector(QLineF correctionVector)
        {
            this->correctionVector = correctionVector;
        }

        // Polar View
        void resetPolarView();

        void processNewBLOB(IBLOB *bp);

    private slots:
        // Connection
        void onConnected();
        void onDisconnected();
        void onError(QAbstractSocket::SocketError error);

        // Communication
        void onTextReceived(const QString &message);
        void onBinaryReceived(const QByteArray &message);

        // Send image
        void sendImage();

        // Metadata and Image upload
        void uploadMetadata(const QByteArray &metadata);
        void uploadImage(const QByteArray &image);

    private:
        void upload(FITSView * view);

        QWebSocket m_WebSocket;
        QJsonObject m_AuthResponse;
        uint16_t m_ReconnectTries {0};
        Ekos::Manager * m_Manager { nullptr };
        QUrl m_URL;
        QString m_UUID;

        QMap<int, bool> m_Options;
        std::unique_ptr<FITSView> previewImage;

        QString extension;
        QStringList temporaryFiles;
        QLineF correctionVector;

        bool m_isConnected { false };
        bool m_sendBlobs { true};

        // Image width for high-bandwidth setting
        static const uint16_t HB_WIDTH = 1280;
        // Image high bandwidth image quality (jpg)
        static const uint8_t HB_IMAGE_QUALITY = 90;
        // Video high bandwidth video quality (jpg)
        static const uint8_t HB_VIDEO_QUALITY = 64;
        // Image high bandwidth image quality (jpg) for PAH
        static const uint8_t HB_PAH_IMAGE_QUALITY = 50;
        // Video high bandwidth video quality (jpg) for PAH
        static const uint8_t HB_PAH_VIDEO_QUALITY = 24;

        // Retry every 5 seconds in case remote server is down
        static const uint16_t RECONNECT_INTERVAL = 5000;
        // Retry for 1 hour before giving up
        static const uint16_t RECONNECT_MAX_TRIES = 720;

        // Binary Metadata Size
        static const uint16_t METADATA_PACKET = 512;

        // HIPS Tile Width and Height
        static const uint16_t HIPS_TILE_WIDTH = 512;
        static const uint16_t HIPS_TILE_HEIGHT = 512;
};
}
