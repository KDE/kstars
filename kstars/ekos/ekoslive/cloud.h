/*
    SPDX-FileCopyrightText: 2018 Jasem Mutlaq <mutlaqja@ikarustech.com>

    Cloud Channel

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
class Cloud : public QObject
{
        Q_OBJECT

    public:
        explicit Cloud(Ekos::Manager * manager, QVector<QSharedPointer<NodeManager>> &nodeManagers);
        virtual ~Cloud() = default;

        bool isConnected() const;
        void registerCameras();

        // Ekos Cloud Message to User
        void sendData(const QSharedPointer<FITSData> &data, const QString &uuid);

    signals:
        void connected();
        void disconnected();
        void globalLogoutTriggered(const QUrl &url);
        void newImage(const QByteArray &image);

    public slots:
        void updateOptions();

    private slots:
        // Connection
        void onConnected();
        void onDisconnected();

        // Communication
        void onTextReceived(const QString &message);
        void uploadImage(const QByteArray &image);

    private:
        void dispatch(const QSharedPointer<FITSData> &data, const QString &uuid);

        Ekos::Manager * m_Manager { nullptr };
        QVector<QSharedPointer<NodeManager>> m_NodeManagers;

        QString extension;
        QStringList temporaryFiles;

        bool m_sendBlobs {true};

        // Image width for high-bandwidth setting
        static const uint16_t HB_WIDTH = 640;
        // Image high bandwidth image quality (jpg)
        static const uint8_t HB_IMAGE_QUALITY = 76;
        // Video high bandwidth video quality (jpg)
        static const uint8_t HB_VIDEO_QUALITY = 64;

        // Binary Metadata Size
        static const uint16_t METADATA_PACKET = 2048;

        // Retry every 5 seconds in case remote server is down
        static const uint16_t RECONNECT_INTERVAL = 5000;
        // Retry for 1 hour before giving up
        static const uint16_t RECONNECT_MAX_TRIES = 720;
};
}
