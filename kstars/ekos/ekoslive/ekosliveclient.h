/*
    SPDX-FileCopyrightText: 2018 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QDialog>
#include <QtWebSockets/QWebSocket>
#include <memory>

#include "ekos/ekos.h"
#include "ekos/manager.h"
#include "ekos/align/align.h"
#include "indi/indicommon.h"
#include "ksnotification.h"

// Ekos Live Communication Channels
#include "message.h"
#include "media.h"
#include "cloud.h"

#include "ui_ekoslivedialog.h"

class QProgressIndicator;
class QNetworkAccessManager;
class QNetworkReply;

namespace EkosLive
{
class Client : public QDialog, public Ui::EkosLiveDialog
{
        Q_OBJECT
    public:
        explicit Client(Ekos::Manager *manager);
        ~Client();

        bool isConnected() const
        {
            return m_isConnected;
        }

        const QPointer<Message> &message()
        {
            return m_Message;
        }
        const QPointer<Media> &media()
        {
            return m_Media;
        }
        const QPointer<Cloud> &cloud()
        {
            return m_Cloud;
        }

        void setConnected(bool enabled);
        void setConfig(bool onlineService, bool rememberCredentials, bool autoConnect);
        void setUser(const QString &user, const QString &pass);

    signals:
        void connected();
        void disconnected();

    protected slots:
        void authenticate();
        void onResult(QNetworkReply *reply);

    private slots:
        // Auth Server
        void connectAuthServer();
        void disconnectAuthServer();

        void onConnected();
        void onDisconnected();

    private:
        QJsonObject authResponse;
        QWebSocket m_mediaWebSocket;
        bool m_isConnected { false };
        Ekos::Manager *m_Manager { nullptr };
        QNetworkAccessManager *networkManager { nullptr };

        QProgressIndicator *pi { nullptr };

        QString token;
        QUrl m_serviceURL;
        QUrl m_wsURL;

        uint16_t m_AuthReconnectTries {0};

        QPointer<Message> m_Message;
        QPointer<Media> m_Media;
        QPointer<Cloud> m_Cloud;

        // Retry every 3 seconds if connection is refused.
        static const uint16_t RECONNECT_INTERVAL = 3000;
        // Retry for 3 times before giving up
        static const uint16_t RECONNECT_MAX_TRIES = 3;
};
}
