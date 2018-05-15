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

#include "ekos/ekos.h"
#include "indi/indicommon.h"
#include "ui_ekoslivedialog.h"
#include "ksnotification.h"

class EkosManager;
class QProgressIndicator;
class QNetworkAccessManager;
class QNetworkReply;
class FITSView;

class EkosLiveClient : public QDialog, public Ui::EkosLiveDialog
{
    Q_OBJECT
public:
    explicit EkosLiveClient(EkosManager *manager);

    bool isConnected() const { return m_isConnected; }
    void sendMessage(const QString &msg);
    void sendResponse(const QString &command, const QJsonObject &payload);
    void sendResponse(const QString &command, const QJsonArray &payload);

    void updateMountStatus(const QJsonObject &status);
    void updateCaptureStatus(const QJsonObject &status);
    void updateFocusStatus(const QJsonObject &status);
    void updateGuideStatus(const QJsonObject &status);
    void sendPreviewImage(FITSView *view);
    void sendEvent(const QString &message, KSNotification::EventType event);

    enum COMMANDS
    {
        GET_PROFILES,
        GET_STATES,
        NEW_MOUNT_STATE,
        NEW_CAPTURE_STATE,
        NEW_GUIDE_STATE,
        NEW_FOCUS_STATE,
        NEW_PREVIEW_IMAGE,
        NEW_NOTIFICATION,
    };

    static QMap<COMMANDS, QString> const commands;

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
    void sendStates();

    QWebSocket m_webSocket;    
    bool m_isConnected { false };
    EkosManager *m_Manager { nullptr };
    QNetworkAccessManager *networkManager { nullptr };

    QProgressIndicator *pi { nullptr };

    QString token;

    QUrl m_serviceURL, m_wsURL;
};
