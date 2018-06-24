/*  Ekos Live Client

    Copyright (C) 2018 Jasem Mutlaq <mutlaqja@ikarustech.com>

    Cloud Channel

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#pragma once

#include <QtWebSockets/QWebSocket>
#include <memory>

#include "ekos/ekos.h"
#include "ekos/ekosmanager.h"

class FITSView;

namespace EkosLive
{
class Cloud : public QObject
{
    Q_OBJECT

public:
    Cloud(EkosManager *manager);
    virtual ~Cloud() = default;

    void sendResponse(const QString &command, const QJsonObject &payload);
    void sendResponse(const QString &command, const QJsonArray &payload);

    void setAuthResponse(const QJsonObject &response) {m_AuthResponse = response;}
    void setURL(const QUrl &url) {m_URL = url;}    

    void registerCameras();

    // Ekos Cloud Message to User
    void sendPreviewImage(FITSView *view, const QString &uuid);

signals:
    void connected();
    void disconnected();

public slots:
    void connectServer();
    void disconnectServer();
    void setOptions(QMap<int,bool> options) {m_Options = options;}

private slots:
    // Connection
    void onConnected();
    void onDisconnected();
    void onError(QAbstractSocket::SocketError error);

    // Communication
    void onTextReceived(const QString &message);    

private:    
    QWebSocket m_WebSocket;
    QJsonObject m_AuthResponse;
    uint16_t m_ReconnectTries {0};
    EkosManager *m_Manager { nullptr };
    QUrl m_URL;

    QString extension;
    QStringList temporaryFiles;

    bool m_isConnected {false};
    bool m_sendBlobs {true};

    QMap<int,bool> m_Options;

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
