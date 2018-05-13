/*  Ekos Live Client
    Copyright (C) 2018 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#include "Options.h"

#include "ekosliveclient.h"
#include "ekos_debug.h"
#include "ekos/ekosmanager.h"
#include "profileinfo.h"
#include "kspaths.h"
#include "filedownloader.h"
#include "ksnotification.h"
#include "fitsviewer/fitsview.h"
#include "fitsviewer/fitsdata.h"
#include "QProgressIndicator.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>

#include <KWallet>

QMap<EkosLiveClient::COMMANDS, QString> const EkosLiveClient::commands =
{
    {GET_PROFILES, "get_profiles"},
    {GET_STATES, "get_states"},
    {NEW_MOUNT_STATE, "new_mount_state"},
    {NEW_CAPTURE_STATE, "new_capture_state"},
    {NEW_GUIDE_STATE, "new_guide_state"},
    {NEW_FOCUS_STATE, "new_focus_state"},
    {NEW_PREVIEW_IMAGE, "new_preview_image"}
};

EkosLiveClient::EkosLiveClient(EkosManager *manager) : QDialog(manager), m_Manager(manager)
{
    setupUi(this);

    connect(closeB, SIGNAL(clicked()), this, SLOT(close()));

    networkManager = new QNetworkAccessManager(this);
    connect(networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(onResult(QNetworkReply*)));

    QPixmap im;
    if (im.load(KSPaths::locate(QStandardPaths::GenericDataLocation, "ekoslive.png")))
        leftBanner->setPixmap(im);

    pi = new QProgressIndicator(this);
    bottomLayout->insertWidget(1, pi);

    connectionState->setPixmap(QIcon::fromTheme("state-offline").pixmap(QSize(64, 64)));

    connect(connectB, &QPushButton::clicked, [=]()
    {
        if (m_isConnected)
            disconnectServer();
        else
            connectServer();
    });

    rememberCredentialsCheck->setChecked(Options::rememberCredentials());
    connect(rememberCredentialsCheck, &QCheckBox::toggled, [=](bool toggled) { Options::setRememberCredentials(toggled);});

    connect(&m_webSocket, &QWebSocket::connected, this, &EkosLiveClient::onConnected);
    connect(&m_webSocket, &QWebSocket::disconnected, this, &EkosLiveClient::onDisconnected);
    connect(&m_webSocket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error), [=]()
    {
        qCritical(KSTARS_EKOS) << "Websocked connection error" << m_webSocket.errorString();
    });

    //m_serviceURL.setAuthority("https://live.stellarmate.com");
    //m_wsURL.setAuthority("wws://live.stellarmate.com");

    m_serviceURL.setUrl("http://localhost:3000");
    m_wsURL.setUrl("ws://localhost:3000");

    QMap<QString,QString> credentials;
    KWallet::Wallet *localWallet = KWallet::Wallet::openWallet(KWallet::Wallet::LocalWallet(), 0);

    if (localWallet)
    {
        if (localWallet->hasFolder("KStars"))
        {
            localWallet->setFolder("KStars");
            localWallet->readMap(m_serviceURL.toString(), credentials);
            username->setText(credentials.value("username"));
            password->setText(credentials.value("password"));
        }

        delete (localWallet);
    }

}

void EkosLiveClient::connectWebSocketServer()
{
    QUrl requestURL(m_wsURL);

    QUrlQuery query;
    query.addQueryItem("username", username->text());
    query.addQueryItem("token", token);

    requestURL.setPath("/ekos");
    requestURL.setQuery(query);


    m_webSocket.open(requestURL);

    qCInfo(KSTARS_EKOS) << "Connecting to Websocket server at" << requestURL.toDisplayString();
}

void EkosLiveClient::disconnectWebSocketServer()
{
    m_webSocket.close();
}

void EkosLiveClient::sendMessage(const QString &msg)
{
    m_webSocket.sendTextMessage(msg);
}

void EkosLiveClient::sendResponse(const QString &command, const QJsonObject &payload)
{
    //qCDebug(KSTARS_EKOS) << QJsonDocument({{"token",token},{"type",command},{"payload",payload}}).toJson(QJsonDocument::Compact);
    sendMessage(QJsonDocument({{"token",token},{"type",command},{"payload",payload}}).toJson(QJsonDocument::Compact));
}

void EkosLiveClient::sendResponse(const QString &command, const QJsonArray &payload)
{
    sendMessage(QJsonDocument({{"token",token},{"type",command},{"payload",payload}}).toJson(QJsonDocument::Compact));
}

void EkosLiveClient::onConnected()
{
    qCInfo(KSTARS_EKOS) << "Connected to Websocket server at" << m_wsURL.toDisplayString();

    pi->stopAnimation();

    connectB->setText(i18n("Disconnect"));
    connectionState->setPixmap(QIcon::fromTheme("state-ok").pixmap(QSize(64, 64)));

    m_isConnected = true;

    connect(&m_webSocket, &QWebSocket::textMessageReceived,  this, &EkosLiveClient::onTextMessageReceived);
    connect(&m_webSocket, &QWebSocket::binaryMessageReceived, this, &EkosLiveClient::onBinaryMessageReceived);

    //sendMessage(QLatin1Literal("##Ekos##"));
    //sendProfiles();

    emit connected();

    if (rememberCredentialsCheck->isChecked())
    {
        QMap<QString,QString> credentials;

        credentials["username"] = username->text();
        credentials["password"] = password->text();

        KWallet::Wallet *localWallet = KWallet::Wallet::openWallet(KWallet::Wallet::LocalWallet(), 0);

        if (localWallet)
        {
            if (localWallet->hasFolder("KStars") == false)
                localWallet->createFolder("KStars");

            localWallet->setFolder("KStars");
            localWallet->writeMap(m_serviceURL.toString(), credentials);
            delete (localWallet);
        }
    }
}

void EkosLiveClient::onDisconnected()
{
    qCInfo(KSTARS_EKOS) << "Disonnected to Websocket server at" << m_wsURL.toDisplayString();

    connectionState->setPixmap(QIcon::fromTheme("state-offline").pixmap(QSize(64, 64)));
    m_isConnected = false;
    connectB->setText(i18n("Connect"));

    disconnect(&m_webSocket, &QWebSocket::textMessageReceived,  this, &EkosLiveClient::onTextMessageReceived);
    disconnect(&m_webSocket, &QWebSocket::binaryMessageReceived, this, &EkosLiveClient::onBinaryMessageReceived);

    emit disconnected();
}

void EkosLiveClient::onTextMessageReceived(const QString &message)
{
    qCInfo(KSTARS_EKOS) << "Websocket Message" << message;
    QJsonParseError error;
    auto serverMessage = QJsonDocument::fromJson(message.toLatin1(), &error);
    if (error.error != QJsonParseError::NoError)
    {
        qCWarning(KSTARS_EKOS) << "Ekos Live Parsing Error" << error.errorString();
        return;
    }

    QString command = serverMessage.object().value("command").toString();

    if (command == commands[GET_PROFILES])
        sendProfiles();
    else if (command == commands[GET_STATES])
        sendStates();
}

void EkosLiveClient::onBinaryMessageReceived(const QByteArray &message)
{
    qCInfo(KSTARS_EKOS) << "Websocket Message" << message;
}

void EkosLiveClient::sendProfiles()
{
    QJsonArray profileArray;

    for (const auto &oneProfile: m_Manager->profiles)
        profileArray.append(oneProfile->toJson());

    sendResponse(commands[GET_PROFILES], profileArray);

    //QJsonObject profiles = {{"token", token}, {"type", commands[GET_PROFILES]}, {"payload",profileArray}};
    //sendMessage(QJsonDocument(profiles).toJson(QJsonDocument::Compact));
}

void EkosLiveClient::connectServer()
{
    if (username->text().isEmpty() || password->text().isEmpty())
    {
        KSNotification::error(i18n("Username or password is missing."));
        return;
    }

    pi->startAnimation();
    authenticate();
}

void EkosLiveClient::disconnectServer()
{
    token.clear();

    disconnectWebSocketServer();
}

void EkosLiveClient::authenticate()
{
    QNetworkRequest request;
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QUrl authURL(m_serviceURL);
    authURL.setPath("/api/authenticate");

    request.setUrl(authURL);

    QJsonObject json = { {"username" , username->text()},
                         {"password" , password->text()}};

    auto postData = QJsonDocument(json).toJson(QJsonDocument::Compact);

    networkManager->post(request, postData);
}

void EkosLiveClient::onResult(QNetworkReply *reply)
{
    if (reply->error() != QNetworkReply::NoError)
    {
        pi->stopAnimation();
        connectionState->setPixmap(QIcon::fromTheme("state-error").pixmap(QSize(64, 64)));
        KSNotification::error(i18n("Error authentication with Ekos Live server: %1", reply->errorString()));
        return;
    }

    QJsonParseError error;
    auto response = QJsonDocument::fromJson(reply->readAll(), &error);

    if (error.error != QJsonParseError::NoError)
    {
        pi->stopAnimation();
        connectionState->setPixmap(QIcon::fromTheme("state-error").pixmap(QSize(64, 64)));
        KSNotification::error(i18n("Error parsing server response: %1", error.errorString()));
        return;
    }

    auto json = response.object();

    if (json["success"].toBool() == false)
    {
        pi->stopAnimation();
        connectionState->setPixmap(QIcon::fromTheme("state-error").pixmap(QSize(64, 64)));
        KSNotification::error(json["message"].toString());
        return;
    }

    token = json["token"].toString();

    connectWebSocketServer();
}

void EkosLiveClient::updateMountStatus(const QJsonObject &status)
{
    if (m_isConnected == false)
        return;

    sendResponse(EkosLiveClient::commands[EkosLiveClient::NEW_MOUNT_STATE], status);
}

void EkosLiveClient::updateCaptureStatus(const QJsonObject &status)
{
    if (m_isConnected == false)
        return;    

    sendResponse(EkosLiveClient::commands[EkosLiveClient::NEW_CAPTURE_STATE], status);
}

void EkosLiveClient::sendPreviewImage(FITSView *view)
{
    if (m_isConnected == false)
        return;

    // TODO 640 should be configurable later on
    QImage scaledImage = view->getDisplayImage()->scaledToWidth(640);
    QTemporaryFile jpegFile;
    jpegFile.open();
    jpegFile.close();

    scaledImage.save(jpegFile.fileName(), "jpg");

    jpegFile.open();

    QByteArray jpegData = jpegFile.readAll();

    QJsonObject image =
    {
        {"data", QString(jpegData.toBase64()) }
    };

    sendResponse(EkosLiveClient::commands[EkosLiveClient::NEW_PREVIEW_IMAGE], image);
}

void EkosLiveClient::updateFocusStatus(const QJsonObject &status)
{
    if (m_isConnected == false)
        return;

    sendResponse(EkosLiveClient::commands[EkosLiveClient::NEW_FOCUS_STATE], status);
}

void EkosLiveClient::updateGuideStatus(const QJsonObject &status)
{
    if (m_isConnected == false)
        return;

    sendResponse(EkosLiveClient::commands[EkosLiveClient::NEW_GUIDE_STATE], status);
}

void EkosLiveClient::sendStates()
{
    if (m_isConnected == false)
        return;

    QJsonObject captureState = {{ "status", m_Manager->captureStatus->text()}};
    sendResponse(EkosLiveClient::commands[EkosLiveClient::NEW_CAPTURE_STATE], captureState);

    QJsonObject mountState = {{ "status", m_Manager->mountStatus->text()},{"target", m_Manager->mountTarget->text()}};
    sendResponse(EkosLiveClient::commands[EkosLiveClient::NEW_MOUNT_STATE], mountState);

    QJsonObject focusState = {{ "status", m_Manager->focusStatus->text()}};
    sendResponse(EkosLiveClient::commands[EkosLiveClient::NEW_FOCUS_STATE], focusState);

    QJsonObject guideState = {{ "status", m_Manager->guideStatus->text()}};
    sendResponse(EkosLiveClient::commands[EkosLiveClient::NEW_GUIDE_STATE], guideState);
}
