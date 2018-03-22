/*  Ekos Live Client
    Copyright (C) 2018 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#pragma once

#include <QtCore/QObject>
#include <QtWebSockets/QWebSocket>

class EkosLiveClient : public QObject
{
    Q_OBJECT
public:
    EkosLiveClient();

    void connectToServer(const QUrl &url);
    void sendMessage(const QString &msg);

signals:
    void closed();

private slots:
    void onConnected();
    void onTextMessageReceived(QString message);

private:
    QWebSocket m_webSocket;
    QUrl m_url;
};
