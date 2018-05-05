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

#include "ui_ekoslivedialog.h"

class EkosManager;
class QProgressIndicator;
class QNetworkAccessManager;
class QNetworkReply;

class EkosLiveClient : public QDialog, public Ui::EkosLiveDialog
{
    Q_OBJECT
public:
    explicit EkosLiveClient(EkosManager *manager);

    void sendMessage(const QString &msg);

    enum COMMANDS
    {
        GET_PROFILES
    };

    const QMap<COMMANDS, QString> commands =
    {
        {GET_PROFILES, "get_profiles"}
    };

signals:
    void connected();
    void disconnected();

protected slots:
    void authenticate();
    void onResult(QNetworkReply *reply);

private slots:
    void connectServer();
    void disconnectServer();
    void onConnected();
    void onDisconnected();
    void onTextMessageReceived(const QString &message);
    void onBinaryMessageReceived(const QByteArray &message);

private:
    void connectWebSocketServer();
    void disconnectWebSocketServer();
    void sendProfiles();

    QWebSocket m_webSocket;    
    bool isConnected { false };
    EkosManager *m_Manager { nullptr };
    QNetworkAccessManager *networkManager { nullptr };

    QProgressIndicator *pi { nullptr };

    QString token;

    QUrl m_serviceURL, m_wsURL;
};
