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

    protected:
        void showSelectServersDialog();

    signals:
        void connected();
        void disconnected();

    private slots:
        void onConnected();
        void onDisconnected();
        void processGlobalLogoutTrigger(const QUrl &url);
        void checkAndTriggerAuth(bool force = false);

    private:
        Ekos::Manager *m_Manager { nullptr };
        bool m_isConnected {false};
        bool m_isProcessingGlobalLogout {false};
        bool m_userRequestedDisconnect {false};

        QProgressIndicator *pi { nullptr };
        QVector<QSharedPointer<NodeManager >> m_NodeManagers;

        QPointer<Message> m_Message;
        QPointer<Media> m_Media;
        QPointer<Cloud> m_Cloud;
};
}
