/*
    SPDX-FileCopyrightText: 2018 Jasem Mutlaq <mutlaqja@ikarustech.com>

    Media Channel

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QtWebSockets/QWebSocket>
#include <memory>

#include "ekos/manager.h"
#include "nodemanager.h"

class FITSView;

namespace EkosLive
{
class Media : public QObject
{
        Q_OBJECT

    public:
        explicit Media(Ekos::Manager * manager, QVector<QSharedPointer<NodeManager>> &nodeManagers);
        virtual ~Media() = default;

        bool isConnected() const;
        void sendResponse(const QString &command, const QJsonObject &payload);
        void sendResponse(const QString &command, const QJsonArray &payload);

        void registerCameras();

        // Ekos Media Message to User
        void sendFile(const QString &filename, const QString &uuid);
        void sendData(const QSharedPointer<FITSData> &data, const QString &uuid);
        void sendView(const QSharedPointer<FITSView> &view, const QString &uuid);
        void sendUpdatedFrame(const QSharedPointer<FITSView> &view);
        void sendModuleFrame(const QSharedPointer<FITSView> &view);

        // Convenience functions
        void sendDarkLibraryData(const QSharedPointer<FITSData> &data);

    Q_SIGNALS:
        void connected();
        void disconnected();
        void globalLogoutTriggered(const QUrl &url);

        void newBoundingRect(QRect rect, QSize view, double currentZoom);
        void newImage(const QByteArray &image, const QString &uuid);

    public Q_SLOTS:
        // Capture
        void sendVideoFrame(const QSharedPointer<QImage> &frame);

        // LiveStacking
        void setLiveStackingActive(bool active)
        {
            m_liveStackingActive = active;
        }

        // Correction Vector
        void setCorrectionVector(QLineF correctionVector)
        {
            this->correctionVector = correctionVector;
        }

        // Polar View
        void resetPolarView();

        void processNewBLOB(IBLOB *bp);

    private Q_SLOTS:
        // Connection
        void onConnected();
        void onDisconnected();

        // Communication
        void onTextReceived(const QString &message);
        void onBinaryReceived(const QByteArray &message);

        // Metadata and Image upload
        void uploadImage(const QByteArray &image, const QString &uuid);

    private:
        void dispatch(const QSharedPointer<FITSData> &data, const QString &uuid);
        void upload(const QSharedPointer<FITSView> &view, const QString &uuid);

        void upload(const QSharedPointer<FITSData> &data, const QImage &image, const StretchParams &params, const QString &uuid);

        // Returns true if at least one NodeManager has blob sending enabled.
        // Used as a fast early-exit to avoid unnecessary processing when all
        // destinations have blobs turned off.
        bool anyBlobsEnabled() const;
        void stretch(const QSharedPointer<FITSData> &data, QImage &image, StretchParams &params) const;

        Ekos::Manager * m_Manager { nullptr };
        QVector<QSharedPointer<NodeManager>> m_NodeManagers;
        QString extension;
        QStringList temporaryFiles;
        QLineF correctionVector;

        // Per-NodeManager blob sending state.
        // The online server may toggle its blob flag to save bandwidth.
        // The offline (StellarMate App) server always keeps blobs enabled.
        QMap<NodeManager*, bool> m_BlobState;
        // When the native KStars livestacker is active, raw capture frames are
        // suppressed so only the stacked results (delivered via PictureMonitor)
        // reach the App.
        bool m_liveStackingActive { false };

        // Image width for high-bandwidth setting
        static const uint16_t HB_IMAGE_WIDTH = 1920;
        // Video width for high-bandwidth setting
        static const uint16_t HB_VIDEO_WIDTH = 1280;
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
