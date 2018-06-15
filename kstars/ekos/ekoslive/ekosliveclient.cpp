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
#include "ekos/capture/capture.h"
#include "ekos/mount/mount.h"
#include "ekos/focus/focus.h"
#include "profileinfo.h"
#include "kspaths.h"
#include "kstarsdata.h"
#include "filedownloader.h"
#include "fitsviewer/fitsview.h"
#include "fitsviewer/fitsdata.h"
#include "QProgressIndicator.h"

#include "indi/indilistener.h"
#include "indi/indiccd.h"
#include "indi/indifilter.h"

#include <KFormat>

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QUuid>

#include <KWallet>

QMap<EkosLiveClient::COMMANDS, QString> const EkosLiveClient::commands =
{
    {GET_CONNECTION, "get_connection"},
    {GET_PROFILES, "get_profiles"},
    {GET_STATES, "get_states"},
    {GET_CAMERAS, "get_cameras"},
    {GET_MOUNTS, "get_mounts"},
    {GET_SCOPES, "get_scopes"},
    {GET_FILTER_WHEELS, "get_filter_wheels"},
    {NEW_CONNECTION_STATE, "new_connection_state"},
    {NEW_MOUNT_STATE, "new_mount_state"},
    {NEW_CAPTURE_STATE, "new_capture_state"},
    {NEW_GUIDE_STATE, "new_guide_state"},
    {NEW_FOCUS_STATE, "new_focus_state"},
    {NEW_ALIGN_STATE, "new_align_state"},
    {NEW_POLAR_STATE, "new_polar_state"},
    {NEW_PREVIEW_IMAGE, "new_preview_image"},
    {NEW_VIDEO_FRAME, "new_video_frame"},
    {NEW_ALIGN_FRAME, "new_align_frame"},
    {NEW_NOTIFICATION, "new_notification"},
    {NEW_TEMPERATURE, "new_temperature"},


    {START_PROFILE, "profile_start"},
    {STOP_PROFILE, "profile_stop"},

    {CAPTURE_PREVIEW, "capture_preview"},
    {CAPTURE_TOGGLE_VIDEO, "capture_toggle_video"},
    {CAPTURE_START, "capture_start"},
    {CAPTURE_STOP, "capture_stop"},
    {CAPTURE_GET_SEQUENCES, "capture_get_sequences"},
    {CAPTURE_ADD_SEQUENCE, "capture_add_sequence"},
    {CAPTURE_REMOVE_SEQUENCE, "capture_remove_sequence"},

    {MOUNT_PARK, "mount_park"},
    {MOUNT_UNPARK, "mount_unpark"},
    {MOUNT_ABORT, "mount_abort"},
    {MOUNT_SYNC_RADE, "mount_sync_rade"},
    {MOUNT_SYNC_TARGET, "mount_sync_target"},
    {MOUNT_GOTO_RADE, "mount_goto_rade"},
    {MOUNT_GOTO_TARGET, "mount_goto_target"},
    {MOUNT_SET_MOTION, "mount_set_motion"},
    {MOUNT_SET_TRACKING, "mount_set_tracking"},
    {MOUNT_SET_SLEW_RATE, "mount_set_slew_rate"},

    {FOCUS_START, "focus_start"},
    {FOCUS_STOP, "focus_stop"},
    {FOCUS_STOP, "focus_reset"},

    {GUIDE_START, "guide_start"},
    {GUIDE_STOP, "guide_stop"},
    {GUIDE_CLEAR, "guide_clear"},

    {ALIGN_SOLVE, "align_solve"},
    {ALIGN_STOP, "align_stop"},
    {ALIGN_LOAD_AND_SLEW, "align_load_and_slew"},
    {ALIGN_SELECT_SCOPE, "align_select_scope"},
    {ALIGN_SELECT_SOLVER_TYPE, "align_select_solver_type"},
    {ALIGN_SELECT_SOLVER_ACTION, "align_select_solver_action"},
    {ALIGN_SET_FILE_EXTENSION, "align_set_file_extension"},
    {ALIGN_SET_CAPTURE_SETTINGS, "align_set_capture_settings"},

    {PAH_START, "polar_start"},
    {PAH_STOP, "polar_stop"},
    {PAH_REFRESH, "polar_refresh"},
    {PAH_SET_MOUNT_DIRECTION, "polar_set_mount_direction"},
    {PAH_SET_MOUNT_ROTATION, "polar_set_mount_rotation"},
    {PAH_SET_CROSSHAIR, "polar_set_crosshair"},
    {PAH_SELECT_STAR_DONE, "polar_star_select_done"},
    {PAH_REFRESHING_DONE, "polar_refreshing_done"},

    {OPTION_SET_HIGH_BANDWIDTH, "option_set_high_bandwidth"},
    {OPTION_SET_IMAGE_TRANSFER, "option_set_image_transfer"},
    {OPTION_SET_NOTIFICATIONS, "option_set_notifications"},
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
            disconnectAuthServer();
        else
            connectAuthServer();
    });

    rememberCredentialsCheck->setChecked(Options::rememberCredentials());
    connect(rememberCredentialsCheck, &QCheckBox::toggled, [=](bool toggled) { Options::setRememberCredentials(toggled);});

    connect(&m_messageWebSocket, &QWebSocket::connected, this, &EkosLiveClient::onMessageConnected);
    connect(&m_messageWebSocket, &QWebSocket::disconnected, this, &EkosLiveClient::onMessageDisconnected);
    connect(&m_messageWebSocket, static_cast<void(QWebSocket::*)(QAbstractSocket::SocketError)>(&QWebSocket::error), this, &EkosLiveClient::onMessageError);

    connect(&m_mediaWebSocket, &QWebSocket::connected, this, &EkosLiveClient::onMediaConnected);
    connect(&m_mediaWebSocket, &QWebSocket::disconnected, this, &EkosLiveClient::onMediaDisconnected);
    connect(&m_mediaWebSocket, static_cast<void(QWebSocket::*)(QAbstractSocket::SocketError)>(&QWebSocket::error), this, &EkosLiveClient::onMediaError);


    m_serviceURL.setUrl("https://live.stellarmate.com");
    m_wsURL.setUrl("wss://live.stellarmate.com");

    //m_serviceURL.setUrl("http://localhost:3000");
    //m_wsURL.setUrl("ws://localhost:3000");

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

EkosLiveClient::~EkosLiveClient()
{
    m_messageWebSocket.close();
    m_mediaWebSocket.close();
}

void EkosLiveClient::connectMessageServer()
{
    QUrl requestURL(m_wsURL);

    QUrlQuery query;
    query.addQueryItem("username", username->text());
    query.addQueryItem("token", token);
    query.addQueryItem("email", authResponse["email"].toString());
    query.addQueryItem("from_date", authResponse["from_date"].toString());
    query.addQueryItem("to_date", authResponse["to_date"].toString());
    query.addQueryItem("plan_id", authResponse["plan_id"].toString());

    requestURL.setPath("/message/ekos");
    requestURL.setQuery(query);


    m_messageWebSocket.open(requestURL);

    qCInfo(KSTARS_EKOS) << "Connecting to Websocket server at" << requestURL.toDisplayString();
}

void EkosLiveClient::disconnectMessageServer()
{
    m_messageWebSocket.close();
}

void EkosLiveClient::connectMediaServer()
{
    QUrl requestURL(m_wsURL);

    QUrlQuery query;
    query.addQueryItem("username", username->text());
    query.addQueryItem("token", token);

    requestURL.setPath("/media/ekos");
    requestURL.setQuery(query);

    m_mediaWebSocket.open(requestURL);

    qCInfo(KSTARS_EKOS) << "Connecting to Media Websocket server at" << requestURL.toDisplayString();
}

void EkosLiveClient::disconnectMediaServer()
{
    m_mediaWebSocket.close();
}

void EkosLiveClient::sendResponse(const QString &command, const QJsonObject &payload)
{
    //qCDebug(KSTARS_EKOS) << QJsonDocument({{"token",token},{"type",command},{"payload",payload}}).toJson(QJsonDocument::Compact);
    //sendMessage(QJsonDocument({{"token",token},{"type",command},{"payload",payload}}).toJson(QJsonDocument::Compact));
    m_messageWebSocket.sendTextMessage(QJsonDocument({{"type",command},{"payload",payload}}).toJson(QJsonDocument::Compact));
}

void EkosLiveClient::sendResponse(const QString &command, const QJsonArray &payload)
{
    //sendMessage(QJsonDocument({{"token",token},{"type",command},{"payload",payload}}).toJson(QJsonDocument::Compact));
    m_messageWebSocket.sendTextMessage(QJsonDocument({{"type",command},{"payload",payload}}).toJson(QJsonDocument::Compact));
}

void EkosLiveClient::onMessageConnected()
{
    qCInfo(KSTARS_EKOS) << "Connected to Message Websocket server at" << m_wsURL.toDisplayString();

    pi->stopAnimation();

    connectB->setText(i18n("Disconnect"));
    connectionState->setPixmap(QIcon::fromTheme("state-ok").pixmap(QSize(64, 64)));

    m_isConnected = true;
    m_MessageReconnectTries=0;

    connect(&m_messageWebSocket, &QWebSocket::textMessageReceived,  this, &EkosLiveClient::onMessageTextReceived);

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

    sendConnection();
    sendProfiles();
}

void EkosLiveClient::onMessageDisconnected()
{
    qCInfo(KSTARS_EKOS) << "Disonnected to Message Websocket server.";

    connectionState->setPixmap(QIcon::fromTheme("state-offline").pixmap(QSize(64, 64)));
    m_isConnected = false;
    connectB->setText(i18n("Connect"));

    disconnect(&m_messageWebSocket, &QWebSocket::textMessageReceived,  this, &EkosLiveClient::onMessageTextReceived);        
}

void EkosLiveClient::onMessageError(QAbstractSocket::SocketError error)
{
    qCritical(KSTARS_EKOS) << "Websocket connection error" << m_messageWebSocket.errorString();
    if (error == QAbstractSocket::RemoteHostClosedError ||
        error == QAbstractSocket::ConnectionRefusedError)
    {
        if (m_MessageReconnectTries++ < RECONNECT_MAX_TRIES)
            QTimer::singleShot(RECONNECT_INTERVAL, this, SLOT(connectMessageServer()));
    }
}

void EkosLiveClient::onMediaConnected()
{
    qCInfo(KSTARS_EKOS) << "Connected to Media Websocket server at" << m_wsURL.toDisplayString();
    connect(&m_mediaWebSocket, &QWebSocket::textMessageReceived,  this, &EkosLiveClient::onMediaTextReceived);
    connect(&m_mediaWebSocket, &QWebSocket::binaryMessageReceived, this, &EkosLiveClient::onMediaBinaryReceived);
}

void EkosLiveClient::onMediaDisconnected()
{
    qCInfo(KSTARS_EKOS) << "Disconnected from Media Websocket server";
    m_MediaReconnectTries=0;
    disconnect(&m_mediaWebSocket, &QWebSocket::textMessageReceived,  this, &EkosLiveClient::onMediaTextReceived);
    disconnect(&m_mediaWebSocket, &QWebSocket::binaryMessageReceived, this, &EkosLiveClient::onMediaBinaryReceived);

    for (const QString &oneFile : temporaryFiles)
        QFile::remove(oneFile);
    temporaryFiles.clear();
}

void EkosLiveClient::onMediaError(QAbstractSocket::SocketError error)
{
    qCritical(KSTARS_EKOS) << "Media Websocket connection error" << m_mediaWebSocket.errorString();
    if (error == QAbstractSocket::RemoteHostClosedError ||
        error == QAbstractSocket::ConnectionRefusedError)
    {
        if (m_MediaReconnectTries++ < RECONNECT_MAX_TRIES)
            QTimer::singleShot(RECONNECT_INTERVAL, this, SLOT(connectMediaServer()));
    }
}

void EkosLiveClient::onMediaTextReceived(const QString &message)
{
    qCInfo(KSTARS_EKOS) << "Media Text Websocket Message" << message;
    QJsonParseError error;
    auto serverMessage = QJsonDocument::fromJson(message.toLatin1(), &error);
    if (error.error != QJsonParseError::NoError)
    {
        qCWarning(KSTARS_EKOS) << "Ekos Live Parsing Error" << error.errorString();
        return;
    }

    const QJsonObject msgObj = serverMessage.object();
    const QString command = msgObj["type"].toString();
    const QJsonObject payload = msgObj["payload"].toObject();

    if (command == commands[ALIGN_SET_FILE_EXTENSION])
        extension = payload["ext"].toString();
}

void EkosLiveClient::onMediaBinaryReceived(const QByteArray &message)
{
    // For now, we are only receving binary image (jpg or FITS) for load and slew
    QTemporaryFile file(QString("/tmp/XXXXXX.%1").arg(extension));
    file.setAutoRemove(false);
    file.open();
    file.write(message);
    file.close();

    Ekos::Align *align = m_Manager->alignModule();

    const QString filename = file.fileName();

    temporaryFiles << filename;

    align->loadAndSlew(filename);
}

void EkosLiveClient::onMessageTextReceived(const QString &message)
{
    qCInfo(KSTARS_EKOS) << "Websocket Message" << message;
    QJsonParseError error;
    auto serverMessage = QJsonDocument::fromJson(message.toLatin1(), &error);
    if (error.error != QJsonParseError::NoError)
    {
        qCWarning(KSTARS_EKOS) << "Ekos Live Parsing Error" << error.errorString();
        return;
    }


    // TODO add check to verify token!
    //    const QString serverToken = serverMessage.object().value("token").toString();

    //    if (serverToken != token)
    //    {
    //        qCWarning(KSTARS_EKOS) << "Invalid token received from server!" << serverToken;
    //        return;
    //    }

    const QJsonObject msgObj = serverMessage.object();
    const QString command = msgObj["type"].toString();
    const QJsonObject payload = msgObj["payload"].toObject();

    if (command == commands[GET_CONNECTION])
        sendConnection();
    else if (command == commands[GET_PROFILES])
        sendProfiles();
    else if (command.startsWith("profile_"))
        processProfileCommands(command, payload);

    if (m_Manager->getEkosStartingStatus() != EkosManager::EKOS_STATUS_SUCCESS)
        return;

    if (command == commands[GET_STATES])
        sendStates();
    else if (command == commands[GET_CAMERAS])
        sendCameras();
    else if (command == commands[GET_MOUNTS])
        sendMounts();
    else if (command == commands[GET_SCOPES])
        sendScopes();
    else if (command == commands[GET_FILTER_WHEELS])
        sendFilterWheels();
    else if (command.startsWith("capture_"))
        processCaptureCommands(command, payload);
    else if (command.startsWith("mount_"))
        processMountCommands(command, payload);
    else if (command.startsWith("focus_"))
        processFocusCommands(command, payload);
    else if (command.startsWith("guide_"))
        processGuideCommands(command, payload);
    else if (command.startsWith("align_"))
        processAlignCommands(command, payload);
    else if (command.startsWith("polar_"))
        processPolarCommands(command, payload);
    else if (command.startsWith("option_"))
        processOptionsCommands(command, payload);
}

void EkosLiveClient::sendProfiles()
{
    QJsonArray profileArray;

    for (const auto &oneProfile: m_Manager->profiles)
        profileArray.append(oneProfile->toJson());

    QJsonObject profiles = {
        {"selectedProfile", m_Manager->getCurrentProfile()->name},
        {"profiles", profileArray}
    };
    sendResponse(commands[GET_PROFILES], profiles);
}

void EkosLiveClient::connectAuthServer()
{
    if (username->text().isEmpty() || password->text().isEmpty())
    {
        KSNotification::error(i18n("Username or password is missing."));
        return;
    }

    pi->startAnimation();
    authenticate();
}

void EkosLiveClient::disconnectAuthServer()
{
    token.clear();

    disconnectMessageServer();
    disconnectMediaServer();
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

    authResponse = response.object();

    if (authResponse["success"].toBool() == false)
    {
        pi->stopAnimation();
        connectionState->setPixmap(QIcon::fromTheme("state-error").pixmap(QSize(64, 64)));
        KSNotification::error(authResponse["message"].toString());
        return;
    }

    token = authResponse["token"].toString();

    connectMessageServer();
    connectMediaServer();
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
    if (m_isConnected == false || m_transferImages == false)
        return;

    QByteArray jpegData;
    QBuffer buffer(&jpegData);
    buffer.open(QIODevice::WriteOnly);
    QImage scaledImage = view->getDisplayImage()->scaledToWidth(m_highBandwidth ? HB_WIDTH : HB_WIDTH/2);
    scaledImage.save(&buffer, "jpg", m_highBandwidth ? HB_IMAGE_QUALITY : HB_IMAGE_QUALITY/2);
    buffer.close();

    const FITSData *imageData = view->getImageData();
    QString resolution = QString("%1x%2").arg(imageData->getWidth()).arg(imageData->getHeight());
    QString sizeBytes = KFormat().formatByteSize(imageData->getSize());
    QString xbin("1"), ybin("1");
    imageData->getRecordValue("XBINNING", xbin);
    imageData->getRecordValue("YBINNING", ybin);
    QString binning = QString("%1x%2").arg(xbin).arg(ybin);
    QString bitDepth = QString::number(imageData->getBPP());

    QJsonObject metadata = {
        {"resolution",resolution},
        {"size",sizeBytes},
        {"bin",binning},
        {"bpp",bitDepth},
    };

    m_mediaWebSocket.sendTextMessage(QJsonDocument(metadata).toJson());
    m_mediaWebSocket.sendBinaryMessage(jpegData);
}

void EkosLiveClient::sendUpdatedFrame(FITSView *view)
{
    if (m_isConnected == false || m_transferImages == false)
        return;

    QByteArray jpegData;
    QBuffer buffer(&jpegData);
    buffer.open(QIODevice::WriteOnly);
    //QPixmap scaledPixmap = view->getDisplayPixmap().scaledToWidth(640);
    view->getDisplayPixmap().save(&buffer, "jpg", m_highBandwidth ? HB_IMAGE_QUALITY : HB_IMAGE_QUALITY/2);
    buffer.close();

    m_mediaWebSocket.sendBinaryMessage(jpegData);
}

void EkosLiveClient::sendVideoFrame(std::unique_ptr<QImage> & frame)
{
    if (m_isConnected == false || m_transferImages == false || !frame)
        return;

    // TODO Scale should be configurable
    QImage scaledImage =  frame.get()->scaledToWidth(m_highBandwidth ? HB_WIDTH : HB_WIDTH/2);
    QTemporaryFile jpegFile;
    jpegFile.open();
    jpegFile.close();

    // TODO Quality should be configurable
    scaledImage.save(jpegFile.fileName(), "jpg", m_highBandwidth ? HB_VIDEO_QUALITY : HB_VIDEO_QUALITY/2);

    jpegFile.open();

    m_mediaWebSocket.sendBinaryMessage(jpegFile.readAll());
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

void EkosLiveClient::sendConnection()
{
    if (m_isConnected == false)
        return;

    QJsonObject connectionState = {
        {"connected", true},
        {"online", m_Manager->getEkosStartingStatus() == EkosManager::EKOS_STATUS_SUCCESS}
    };
    sendResponse(EkosLiveClient::commands[EkosLiveClient::NEW_CONNECTION_STATE], connectionState);
}

void EkosLiveClient::sendStates()
{
    if (m_isConnected == false)
        return;

    QJsonObject captureState = {{ "status", m_Manager->captureStatus->text()}};
    sendResponse(EkosLiveClient::commands[EkosLiveClient::NEW_CAPTURE_STATE], captureState);

    // Send capture sequence if one exists
    if (m_Manager->captureModule())
        sendCaptureSequence(m_Manager->captureModule()->getSequence());

    if (m_Manager->mountModule())
    {
        QJsonObject mountState = {
            {"status", m_Manager->mountStatus->text()},
            {"target", m_Manager->mountTarget->text()},
            {"slewRate", m_Manager->mountModule()->getSlewRate()}
        };

        sendResponse(EkosLiveClient::commands[EkosLiveClient::NEW_MOUNT_STATE], mountState);
    }

    QJsonObject focusState = {{ "status", m_Manager->focusStatus->text()}};
    sendResponse(EkosLiveClient::commands[EkosLiveClient::NEW_FOCUS_STATE], focusState);

    QJsonObject guideState = {{ "status", m_Manager->guideStatus->text()}};
    sendResponse(EkosLiveClient::commands[EkosLiveClient::NEW_GUIDE_STATE], guideState);

    if (m_Manager->alignModule())
    {
        QJsonObject alignState = {
            {"status", Ekos::alignStates[m_Manager->alignModule()->getStatus()]},
            {"solvers", QJsonArray::fromStringList(m_Manager->alignModule()->getActiveSolvers())},
            {"solverIndex", m_Manager->alignModule()->getActiveSolverIndex()},
            {"scopeIndex", m_Manager->alignModule()->getFOVTelescopeType()},
        };
        sendResponse(EkosLiveClient::commands[EkosLiveClient::NEW_ALIGN_STATE], alignState);

        QJsonObject polarState = {
            {"stage", m_Manager->alignModule()->getPAHStage()},
            {"enabled", m_Manager->alignModule()->isPAHEnabled()},
            {"message", m_Manager->alignModule()->getPAHMessage()},
        };
        sendResponse(EkosLiveClient::commands[EkosLiveClient::NEW_POLAR_STATE], polarState);
    }
}

void EkosLiveClient::sendEvent(const QString &message, KSNotification::EventType event)
{
    if (m_isConnected == false || m_notifications == false)
        return;

    QJsonObject newEvent = {{ "severity", event}, {"message", message},{"uuid",QUuid::createUuid().toString()}};
    sendResponse(EkosLiveClient::commands[EkosLiveClient::NEW_NOTIFICATION], newEvent);
}

void EkosLiveClient::sendCameras()
{
    if (m_isConnected == false)
        return;

    QJsonArray cameraList;

    for(ISD::GDInterface *gd : m_Manager->findDevices(KSTARS_CCD))
    {
        ISD::CCD *oneCCD = dynamic_cast<ISD::CCD*>(gd);
        connect(oneCCD, &ISD::CCD::newTemperatureValue, this, &EkosLiveClient::sendTemperature, Qt::UniqueConnection);
        connect(oneCCD, &ISD::CCD::newVideoFrame, this, &EkosLiveClient::sendVideoFrame, Qt::UniqueConnection);
        ISD::CCDChip *primaryChip = oneCCD->getChip(ISD::CCDChip::PRIMARY_CCD);

        double temperature=0;
        oneCCD->getTemperature(&temperature);

        QJsonObject oneCamera = {
            {"name", oneCCD->getDeviceName()},
            {"canBin", primaryChip->canBin()},
            {"hasTemperature", oneCCD->hasCooler()},
            {"temperature", temperature},
            {"canCool", oneCCD->canCool()},
            {"isoList", QJsonArray::fromStringList(oneCCD->getChip(ISD::CCDChip::PRIMARY_CCD)->getISOList())},
            {"iso", oneCCD->getChip(ISD::CCDChip::PRIMARY_CCD)->getISOIndex()},
            {"hasVideo", oneCCD->hasVideoStream()}
        };

        cameraList.append(oneCamera);
    }

    sendResponse(EkosLiveClient::commands[EkosLiveClient::GET_CAMERAS], cameraList);
}

void EkosLiveClient::sendMounts()
{
    if (m_isConnected == false || m_Manager->getEkosStartingStatus() != EkosManager::EKOS_STATUS_SUCCESS)
        return;

    QJsonArray mountList;

    for(ISD::GDInterface *gd : m_Manager->findDevices(KSTARS_TELESCOPE))
    {
        ISD::Telescope *oneTelescope = dynamic_cast<ISD::Telescope*>(gd);

        QJsonObject oneMount = {
            {"name", oneTelescope->getDeviceName()},
            {"canPark", oneTelescope->canPark()},
            {"canSync", oneTelescope->canSync()},
            {"canControlTrack", oneTelescope->canControlTrack()},
            {"hasSlewRates", oneTelescope->hasSlewRates()},
            {"slewRates", QJsonArray::fromStringList(oneTelescope->slewRates())},
        };

        mountList.append(oneMount);
    }

    sendResponse(EkosLiveClient::commands[EkosLiveClient::GET_MOUNTS], mountList);

    // Also send initial slew rate
    for(ISD::GDInterface *gd : m_Manager->findDevices(KSTARS_TELESCOPE))
    {
        ISD::Telescope *oneTelescope = dynamic_cast<ISD::Telescope*>(gd);

        QJsonObject slewRate = {{"slewRate", oneTelescope->getSlewRate() }};

        sendResponse(EkosLiveClient::commands[EkosLiveClient::NEW_MOUNT_STATE], slewRate);
    }
}

void EkosLiveClient::sendScopes()
{
    if (m_isConnected == false ||
            m_Manager->getEkosStartingStatus() != EkosManager::EKOS_STATUS_SUCCESS ||
            m_Manager->mountModule() == nullptr)
        return;

    QJsonArray scopeList = m_Manager->mountModule()->getScopes();
    sendResponse(EkosLiveClient::commands[EkosLiveClient::GET_SCOPES], scopeList);
}

void EkosLiveClient::sendTemperature(double value)
{
    ISD::CCD *oneCCD = dynamic_cast<ISD::CCD*>(sender());

    QJsonObject temperature = {
        {"name", oneCCD->getDeviceName()},
        {"value", value}
    };

    sendResponse(EkosLiveClient::commands[EkosLiveClient::NEW_TEMPERATURE], temperature);
}

void EkosLiveClient::sendFilterWheels()
{
    if (m_isConnected == false || m_Manager->getEkosStartingStatus() != EkosManager::EKOS_STATUS_SUCCESS)
        return;

    QJsonArray filterList;

    for(ISD::GDInterface *gd : m_Manager->findDevices(KSTARS_FILTER))
    {
        INDI::Property *prop = gd->getProperty("FILTER_NAME");
        if (prop == nullptr)
            break;

        ITextVectorProperty *filterNames = prop->getText();
        if (filterNames == nullptr)
            break;

        QJsonArray filters;
        for (int i=0; i < filterNames->ntp; i++)
            filters.append(filterNames->tp[i].text);

        QJsonObject oneFilter = {
            {"name", gd->getDeviceName()},
            {"filters", filters}
        };

        filterList.append(oneFilter);
    }

    sendResponse(EkosLiveClient::commands[EkosLiveClient::GET_FILTER_WHEELS], filterList);
}

void EkosLiveClient::setCaptureSettings(const QJsonObject &settings)
{
    Ekos::Capture * capture = m_Manager->captureModule();

    // Camera Name
    capture->setCCD(settings["camera"].toString());

    // Filter Wheel
    QString filterWheel = settings["fw"].toString();
    if (filterWheel.isEmpty() == false)
        capture->setFilter(filterWheel, settings["filter"].toString());

    // Temperature
    double temperature = settings["temperature"].toDouble(-1000);
    if (temperature != -1000)
    {
        capture->setForceTemperature(true);
        capture->setTargetTemperature(temperature);
    }

    // Frame Type
    capture->setFrameType(settings["frame"].toString("Light"));

    // Exposure Duration
    capture->setExposure(settings["exp"].toDouble(1));

    // Binning
    int bin = settings.value("bin").toInt(1);
    capture->setBinning(bin,bin);

    // ISO
    int isoIndex = settings["iso"].toInt(-1);
    if (isoIndex >= 0)
        capture->setISO(isoIndex);
}

void EkosLiveClient::processCaptureCommands(const QString &command, const QJsonObject &payload)
{
    Ekos::Capture *capture = m_Manager->captureModule();

    if (command == commands[CAPTURE_PREVIEW])
    {
        setCaptureSettings(payload);
        capture->captureOne();
    }
    else if (command == commands[CAPTURE_TOGGLE_VIDEO])
    {
        capture->toggleVideo(payload["enabled"].toBool());
    }
    else if (command == commands[CAPTURE_START])
        capture->start();
    else if (command == commands[CAPTURE_STOP])
        capture->stop();
    else if (command == commands[CAPTURE_GET_SEQUENCES])
    {
        sendCaptureSequence(capture->getSequence());
    }
    else if (command == commands[CAPTURE_ADD_SEQUENCE])
    {
        // Set capture settings first
        setCaptureSettings(payload);

        // Then sequence settings
        capture->setCount(static_cast<uint16_t>(payload["count"].toInt()));
        capture->setDelay(static_cast<uint16_t>(payload["delay"].toInt()));
        capture->setPrefix(payload["prefix"].toString());

        // Now add job
        capture->addJob();
    }
    else if (command == commands[CAPTURE_REMOVE_SEQUENCE])
    {
        capture->removeJob(payload["index"].toInt());
    }
}

void EkosLiveClient::sendCaptureSequence(const QJsonArray &sequenceArray)
{
    sendResponse(commands[CAPTURE_GET_SEQUENCES], sequenceArray);
}

void EkosLiveClient::processGuideCommands(const QString &command, const QJsonObject &payload)
{
    Ekos::Guide *guide = m_Manager->guideModule();
    Q_UNUSED(payload);

    if (command == commands[GUIDE_START])
    {
        guide->guide();
    }
    else if (command == commands[GUIDE_STOP])
        guide->abort();
    else if (command == commands[GUIDE_CLEAR])
        guide->clearCalibration();
}

void EkosLiveClient::processFocusCommands(const QString &command, const QJsonObject &payload)
{
    Ekos::Focus *focus = m_Manager->focusModule();
    Q_UNUSED(payload);

    if (command == commands[FOCUS_START])
        focus->start();
    else if (command == commands[FOCUS_STOP])
        focus->abort();
    else if (command == commands[FOCUS_RESET])
        focus->resetFrame();
}

void EkosLiveClient::processMountCommands(const QString &command, const QJsonObject &payload)
{
    Ekos::Mount *mount = m_Manager->mountModule();

    if (command == commands[MOUNT_ABORT])
        mount->abort();
    else if (command == commands[MOUNT_PARK])
        mount->park();
    else if (command == commands[MOUNT_UNPARK])
        mount->unpark();
    else if (command == commands[MOUNT_SET_TRACKING])
        mount->setTrackEnabled(payload["enabled"].toBool());
    else if (command == commands[MOUNT_SYNC_RADE])
    {
        mount->setJ2000Enabled(payload["isJ2000"].toBool());
        mount->sync(payload["ra"].toString(), payload["de"].toString());
    }
    else if (command == commands[MOUNT_SYNC_TARGET])
    {
        mount->syncTarget(payload["target"].toString());
    }
    else if (command == commands[MOUNT_GOTO_RADE])
    {
        mount->setJ2000Enabled(payload["isJ2000"].toBool());
        mount->slew(payload["ra"].toString(), payload["de"].toString());
    }
    else if (command == commands[MOUNT_GOTO_TARGET])
    {
        mount->gotoTarget(payload["target"].toString());
    }
    else if (command == commands[MOUNT_SET_SLEW_RATE])
    {
        int rate = payload["rate"].toInt(-1);
        if (rate >= 0)
            mount->setSlewRate(rate);
    }
    else if (command == commands[MOUNT_SET_MOTION])
    {
        QString direction = payload["direction"].toString();
        ISD::Telescope::TelescopeMotionCommand action = payload["action"].toBool(false) ?
                    ISD::Telescope::MOTION_START : ISD::Telescope::MOTION_STOP;

        if (direction == "N")
            mount->motionCommand(action, ISD::Telescope::MOTION_NORTH, -1);
        else if (direction == "S")
            mount->motionCommand(action, ISD::Telescope::MOTION_SOUTH, -1);
        else if (direction == "E")
            mount->motionCommand(action, -1, ISD::Telescope::MOTION_EAST);
        else if (direction == "W")
            mount->motionCommand(action, -1, ISD::Telescope::MOTION_WEST);
    }
}

void EkosLiveClient::processAlignCommands(const QString &command, const QJsonObject &payload)
{
    Ekos::Align *align = m_Manager->alignModule();

    if (command == commands[ALIGN_SOLVE])
        align->captureAndSolve();
    else if (command == commands[ALIGN_SET_CAPTURE_SETTINGS])
        align->setCaptureSettings(payload);
    else if (command == commands[ALIGN_STOP])
        align->abort();
    else if (command == commands[ALIGN_SELECT_SCOPE])
        align->setFOVTelescopeType(payload["value"].toInt());
    else if (command == commands[ALIGN_SELECT_SOLVER_TYPE])
        align->setSolverType(payload["value"].toInt());
    else if (command == commands[ALIGN_SELECT_SOLVER_ACTION])
        align->setSolverAction(payload["value"].toInt());
    else if (command == commands[ALIGN_LOAD_AND_SLEW])
    {
        QTemporaryFile file;
        file.open();
        file.write(QByteArray::fromBase64(payload["data"].toString().toLatin1()));
        file.close();
        align->loadAndSlew(file.fileName());
    }
}

void EkosLiveClient::setAlignStatus(Ekos::AlignState newState)
{
    if (m_isConnected == false || m_Manager->getEkosStartingStatus() != EkosManager::EKOS_STATUS_SUCCESS)
        return;

    QJsonObject alignState = {
        {"status", Ekos::alignStates[newState]}
    };

    sendResponse(EkosLiveClient::commands[EkosLiveClient::NEW_ALIGN_STATE], alignState);
}

void EkosLiveClient::setAlignSolution(const QJsonObject &solution)
{
    if (m_isConnected == false || m_Manager->getEkosStartingStatus() != EkosManager::EKOS_STATUS_SUCCESS)
        return;

    QJsonObject alignState = {
        {"solution", solution},
    };

    sendResponse(EkosLiveClient::commands[EkosLiveClient::NEW_ALIGN_STATE], alignState);
}

void EkosLiveClient::processPolarCommands(const QString &command, const QJsonObject &payload)
{
    Ekos::Align *align = m_Manager->alignModule();

    if (command == commands[PAH_START])
    {
        align->startPAHProcess();
    }
    if (command == commands[PAH_STOP])
    {
        align->stopPAHProcess();
    }
    else if (command == commands[PAH_REFRESH])
    {
        align->setPAHRefreshDuration(payload["value"].toDouble());
        align->startPAHRefreshProcess();
    }
    else if (command == commands[PAH_SET_MOUNT_DIRECTION])
    {
        align->setPAHMountDirection(payload["value"].toInt());
    }
    else if (command == commands[PAH_SET_MOUNT_ROTATION])
    {
        align->setPAHMountRotation(payload["value"].toInt());
    }
    else if (command == commands[PAH_SET_CROSSHAIR])
    {
        align->setPAHCorrectionOffsetPercentage(payload["x"].toDouble(), payload["y"].toDouble());
    }
    else if (command == commands[PAH_SELECT_STAR_DONE])
    {
        align->setPAHCorrectionSelectionComplete();
    }
    else if (command == commands[PAH_REFRESHING_DONE])
    {
        align->setPAHRefreshComplete();
    }
}

void EkosLiveClient::setPAHStage(Ekos::Align::PAHStage stage)
{
    if (m_isConnected == false || m_Manager->getEkosStartingStatus() != EkosManager::EKOS_STATUS_SUCCESS)
        return;

    Q_UNUSED(stage);
    Ekos::Align *align = m_Manager->alignModule();

    QJsonObject polarState = {
        {"stage", align->getPAHStage()}
    };

    // Increase size so that when we send STAR_SELECT image, it is sizable enough
    // for Ekos Live clients. By default it would be pretty small and usually the user controls it by
    // zooming in and out but for EkosLive, we zoom in at this stage.
    if (stage == Ekos::Align::PAH_THIRD_CAPTURE)
        align->zoomAlignView();

    sendResponse(EkosLiveClient::commands[EkosLiveClient::NEW_POLAR_STATE], polarState);
}

void EkosLiveClient::setPAHMessage(const QString &message)
{
    if (m_isConnected == false || m_Manager->getEkosStartingStatus() != EkosManager::EKOS_STATUS_SUCCESS)
        return;

    QJsonObject polarState = {
        {"message", message}
    };

    sendResponse(EkosLiveClient::commands[EkosLiveClient::NEW_POLAR_STATE], polarState);
}

void EkosLiveClient::setPAHEnabled(bool enabled)
{
    if (m_isConnected == false || m_Manager->getEkosStartingStatus() != EkosManager::EKOS_STATUS_SUCCESS)
        return;

    QJsonObject polarState = {
        {"enabled", enabled}
    };

    sendResponse(EkosLiveClient::commands[EkosLiveClient::NEW_POLAR_STATE], polarState);
}

void EkosLiveClient::setFOVTelescopeType(int index)
{
    if (m_isConnected == false || m_Manager->getEkosStartingStatus() != EkosManager::EKOS_STATUS_SUCCESS)
        return;

    QJsonObject alignState = {
        {"scopeIndex", index}
    };

    sendResponse(EkosLiveClient::commands[EkosLiveClient::NEW_ALIGN_STATE], alignState);
}

void EkosLiveClient::processProfileCommands(const QString &command, const QJsonObject &payload)
{
    if (command == commands[START_PROFILE])
    {
        m_Manager->setProfile(payload["name"].toString());
        m_Manager->start();
    }
    else if (command == commands[STOP_PROFILE])
    {
        m_Manager->stop();
    }
}

void EkosLiveClient::setEkosStatingStatus(EkosManager::CommunicationStatus status)
{
    if (status == EkosManager::EKOS_STATUS_PENDING)
        return;

    QJsonObject connectionState = {
        {"connected", true},
        {"online", status == EkosManager::EKOS_STATUS_SUCCESS}
    };
    sendResponse(EkosLiveClient::commands[EkosLiveClient::NEW_CONNECTION_STATE], connectionState);
}

void EkosLiveClient::processOptionsCommands(const QString &command, const QJsonObject &payload)
{
    if (command == commands[OPTION_SET_HIGH_BANDWIDTH])
        m_highBandwidth = payload["value"].toBool(true);
    else if (command == commands[OPTION_SET_IMAGE_TRANSFER])
        m_transferImages = payload["value"].toBool(true);
    else if (command == commands[OPTION_SET_NOTIFICATIONS])
        m_notifications = payload["value"].toBool(true);
}
