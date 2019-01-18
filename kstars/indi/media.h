/*  INDI WebSocket Handler

    Copyright (C) 2019 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#pragma once

#include <QtWebSockets/QWebSocket>
#include <memory>

namespace ISD
{
class CCD;
class Media : public QObject
{
    Q_OBJECT

public:
    Media(CCD *manager);
    virtual ~Media() = default;

    void setURL(const QUrl &url) {m_URL = url;}

signals:
    void connected();
    void disconnected();
    void newFile(const QByteArray &message, const QString &extension);

public slots:
    void connectServer();
    void disconnectServer();

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
    uint16_t m_ReconnectTries {0};
    CCD *m_Manager { nullptr };
    QUrl m_URL;

    bool m_isConnected { false };
    bool m_sendBlobs { true};
    QString extension;

    // Retry every 5 seconds in case remote server is down
    static const uint16_t RECONNECT_INTERVAL = 5000;
    // Retry for 1 hour before giving up
    static const uint16_t RECONNECT_MAX_TRIES = 720;
};
}
