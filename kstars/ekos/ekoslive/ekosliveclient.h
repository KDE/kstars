/*  Ekos Live Client
    Copyright (C) 2018 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#pragma once

#include <QDialog>
#include <QtWebSockets/QWebSocket>
#include <memory>

#include "ekos/ekos.h"
#include "ekos/ekosmanager.h"
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
    explicit Client(EkosManager *manager);
    ~Client();

    bool isConnected() const { return m_isConnected; }    

    const QPointer<Message> & message() { return m_Message; }
    const QPointer<Media> & media() { return m_Media; }
    const QPointer<Cloud> & cloud() { return m_Cloud; }

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
    EkosManager *m_Manager { nullptr };
    QNetworkAccessManager *networkManager { nullptr };

    QProgressIndicator *pi { nullptr };

    QString token;
    QUrl m_serviceURL;

    uint16_t m_MediaReconnectTries {0};

    QPointer<Message> m_Message;
    QPointer<Media> m_Media;
    QPointer<Cloud> m_Cloud;

    // Retry every 5 seconds in case remote server is down
    static const uint16_t RECONNECT_INTERVAL = 5000;
    // Retry for 1 hour before giving up
    static const uint16_t RECONNECT_MAX_TRIES = 720;
};
}
