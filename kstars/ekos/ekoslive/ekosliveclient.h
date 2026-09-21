/*
    SPDX-FileCopyrightText: 2018 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QDialog>
#include <QtWebSockets/QWebSocket>
#include <QJsonObject>
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
#include "nodemanager.h"

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

        // Per-server connection status. Unlike isConnected() (which requires ALL managers
        // to be connected), these report each EkosLive server independently, so callers can
        // tell whether the online or offline server is the one that is down.
        bool isOnlineConnected() const
        {
            return m_NodeManagers.size() > Online && m_NodeManagers[Online]->isConnected();
        }

        bool isOfflineConnected() const
        {
            return m_NodeManagers.size() > Offline && m_NodeManagers[Offline]->isConnected();
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

        // Current supported server types
        enum
        {
            Online,
            Offline
        };

        void setConnected(bool enabled);
        void syncURLs();
        void setConfig(bool rememberCredentials, bool autoConnect);
        void setUser(const QString &user, const QString &pass);
        void trainSession(const QJsonObject &sysidData);

        // Re-read the device_secret / enrollment_token files from disk and re-authenticate
        // only the managers that are currently disconnected. Used after the App writes a
        // freshly-issued enrollment token so a running KStars picks it up without a restart
        // (and without tearing down an already-healthy offline connection).
        void reloadCredentialsAndReconnect();

    protected:
        void showSelectServersDialog();

    Q_SIGNALS:
        void connected();
        void disconnected();
        void onlineStatusChanged(bool connected);
        void offlineStatusChanged(bool connected);
        void trainSessionResult(bool success, const QJsonObject &result);

    private Q_SLOTS:
        void onConnected();
        void onDisconnected();
        void processGlobalLogoutTrigger(const QUrl &url);
        void checkAndTriggerAuth(bool force = false);

    private:
        // Reads device_secret / enrollment_token from disk into the respective NodeManagers.
        void loadTokensFromDisk();
        // Emits onlineStatusChanged/offlineStatusChanged only when the cached values change.
        void emitStatusChanges();

        Ekos::Manager *m_Manager { nullptr };
        bool m_isConnected {false};
        bool m_isProcessingGlobalLogout {false};
        bool m_userRequestedDisconnect {false};
        // Last emitted per-server status, used to avoid duplicate status signals.
        bool m_lastOnlineConnected {false};
        bool m_lastOfflineConnected {false};

        QProgressIndicator *pi { nullptr };
        QVector<QSharedPointer<NodeManager >> m_NodeManagers;

        QPointer<Message> m_Message;
        QPointer<Media> m_Media;
        QPointer<Cloud> m_Cloud;
};
}
