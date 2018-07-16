/*  Ekos Live Client

    Copyright (C) 2018 Jasem Mutlaq <mutlaqja@ikarustech.com>

    Media Channel

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
class Media : public QObject
{
    Q_OBJECT

public:
    Media(EkosManager *manager);
    virtual ~Media() = default;

    void sendResponse(const QString &command, const QJsonObject &payload);
    void sendResponse(const QString &command, const QJsonArray &payload);

    void setAuthResponse(const QJsonObject &response) {m_AuthResponse = response;}
    void setURL(const QUrl &url) {m_URL = url;}    

    void registerCameras();

    // Ekos Media Message to User
    void sendPreviewImage(FITSView *view, const QString &uuid);
    void sendUpdatedFrame(FITSView *view);

signals:
    void connected();
    void disconnected();

    void newBoundingRect(QRect rect, QSize view);

public slots:
    void connectServer();
    void disconnectServer();

    // Capture
    void sendVideoFrame(std::unique_ptr<QImage> & frame);

    // Options
    void setOptions(QMap<int,bool> options) {m_Options = options;}

    // Correction Vector
    void setCorrectionVector(QLineF correctionVector) { this->correctionVector = correctionVector;}

    // Polar View
    void resetPolarView();

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
    QJsonObject m_AuthResponse;
    uint16_t m_ReconnectTries {0};
    EkosManager *m_Manager { nullptr };
    QUrl m_URL;

    QMap<int,bool> m_Options;

    QString extension;
    QStringList temporaryFiles;
    QLineF correctionVector;

    bool m_isConnected { false };
    bool m_sendBlobs { true};

    // Image width for high-bandwidth setting
    static const uint16_t HB_WIDTH = 640;
    // Image high bandwidth image quality (jpg)
    static const uint8_t HB_IMAGE_QUALITY = 76;
    // Video high bandwidth video quality (jpg)
    static const uint8_t HB_VIDEO_QUALITY = 64;
    // Image high bandwidth image quality (jpg) for PAH
    static const uint8_t HB_PAH_IMAGE_QUALITY = 50;
    // Video high bandwidth video quality (jpg) for PAH
    static const uint8_t HB_PAH_VIDEO_QUALITY = 25;

    // Retry every 5 seconds in case remote server is down
    static const uint16_t RECONNECT_INTERVAL = 5000;
    // Retry for 1 hour before giving up
    static const uint16_t RECONNECT_MAX_TRIES = 720;
};
}
