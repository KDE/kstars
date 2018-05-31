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
    {GET_PROFILES, "get_profiles"},
    {GET_STATES, "get_states"},
    {GET_CAMERAS, "get_cameras"},
    {GET_MOUNTS, "get_mounts"},
    {GET_SCOPES, "get_scopes"},
    {GET_FILTER_WHEELS, "get_filter_wheels"},
    {NEW_MOUNT_STATE, "new_mount_state"},
    {NEW_CAPTURE_STATE, "new_capture_state"},
    {NEW_GUIDE_STATE, "new_guide_state"},
    {NEW_FOCUS_STATE, "new_focus_state"},
    {NEW_ALIGN_STATE, "new_align_state"},
    {NEW_PREVIEW_IMAGE, "new_preview_image"},
    {NEW_VIDEO_FRAME, "new_video_frame"},
    {NEW_ALIGN_FRAME, "new_align_frame"},
    {NEW_NOTIFICATION, "new_notification"},
    {NEW_TEMPERATURE, "new_temperature"},

    {CAPTURE_PREVIEW, "capture_preview"},
    {CAPTURE_TOGGLE_VIDEO, "capture_toggle_video"},
    {CAPTURE_START, "capture_start"},
    {CAPTURE_STOP, "capture_stop"},

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

    {GUIDE_START, "guide_start"},
    {GUIDE_STOP, "guide_stop"},
    {GUIDE_CLEAR, "guide_clear"},

    {ALIGN_SOLVE, "align_solve"},
    {ALIGN_STOP, "align_stop"},
    {ALIGN_LOAD_AND_SLEW, "align_load_and_slew"},
    {ALIGN_SELECT_SCOPE, "align_select_scope"},
    {ALIGN_SELECT_SOLVER_TYPE, "align_select_solver_type"},
    {ALIGN_SELECT_SOLVER_ACTION, "align_select_solver_action"},
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
        qCritical(KSTARS_EKOS) << "Websocket connection error" << m_webSocket.errorString();
    });


    connect(&m_mediaSocket, &QWebSocket::connected, this, &EkosLiveClient::onMediaConnected);
    connect(&m_mediaSocket, &QWebSocket::disconnected, this, &EkosLiveClient::onMediaDisconnected);
    connect(&m_mediaSocket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error), [=]()
    {
        qCritical(KSTARS_EKOS) << "Media Websocket connection error" << m_mediaSocket.errorString();
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

    requestURL.setPath("/message/ekos");
    requestURL.setQuery(query);


    m_webSocket.open(requestURL);

    qCInfo(KSTARS_EKOS) << "Connecting to Websocket server at" << requestURL.toDisplayString();
}

void EkosLiveClient::disconnectWebSocketServer()
{
    m_webSocket.close();
}

void EkosLiveClient::connectMediaSocketServer()
{
    QUrl requestURL(m_wsURL);

    QUrlQuery query;
    query.addQueryItem("username", username->text());
    query.addQueryItem("token", token);

    requestURL.setPath("/media/ekos");
    requestURL.setQuery(query);

    m_mediaSocket.open(requestURL);

    qCInfo(KSTARS_EKOS) << "Connecting to Media Websocket server at" << requestURL.toDisplayString();
}

void EkosLiveClient::disconnectMediaSocketServer()
{
    m_mediaSocket.close();
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

void EkosLiveClient::onMediaConnected()
{
    qCInfo(KSTARS_EKOS) << "Connected to Media Websocket server at" << m_wsURL.toDisplayString();
}

void EkosLiveClient::onMediaDisconnected()
{
    qCInfo(KSTARS_EKOS) << "Disconnected from Media Websocket server at" << m_wsURL.toDisplayString();
}

void EkosLiveClient::onMediaBinaryMessageReceived(const QByteArray &message)
{
    Q_UNUSED(message);
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

    // TODO add check to verify token!
    const QString serverToken = serverMessage.object().value("token").toString();

    if (serverToken != token)
    {
        qCWarning(KSTARS_EKOS) << "Invalid token received from server!" << serverToken;
        return;
    }

    const QString command = serverMessage.object().value("type").toString();

    if (command == commands[GET_PROFILES])
        sendProfiles();
    else if (command == commands[GET_STATES])
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
        processCaptureCommands(command, serverMessage.object().value("payload").toObject());
    else if (command.startsWith("mount_"))
        processMountCommands(command, serverMessage.object().value("payload").toObject());
    else if (command.startsWith("focus_"))
        processFocusCommands(command, serverMessage.object().value("payload").toObject());
    else if (command.startsWith("guide_"))
        processGuideCommands(command, serverMessage.object().value("payload").toObject());
    else if (command.startsWith("align_"))
        processAlignCommands(command, serverMessage.object().value("payload").toObject());
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
    disconnectMediaSocketServer();
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
    connectMediaSocketServer();
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

    QJsonObject image =
    {
        //{"data", QString(jpegData.toBase64()) },
        {"metadata", metadata},
    };

    //sendResponse(EkosLiveClient::commands[EkosLiveClient::NEW_PREVIEW_IMAGE], image);

    m_mediaSocket.sendTextMessage(QJsonDocument(metadata).toJson());
    m_mediaSocket.sendBinaryMessage(jpegData);
}

//void EkosLiveClient::setAlignFrame(FITSView *view)
//{
//    if (m_isConnected == false)
//        return;

//    // TODO 640 should be configurable later on
//    QImage scaledImage = view->getDisplayImage()->scaledToWidth(640);
//    QTemporaryFile jpegFile;
//    jpegFile.open();
//    jpegFile.close();

//    scaledImage.save(jpegFile.fileName(), "jpg");

//    jpegFile.open();

//    QByteArray jpegData = jpegFile.readAll();
//    const FITSData *imageData = view->getImageData();
//    QString resolution = QString("%1x%2").arg(imageData->getWidth()).arg(imageData->getHeight());
//    QString sizeBytes = KFormat().formatByteSize(imageData->getSize());
//    QString xbin("1"), ybin("1");
//    imageData->getRecordValue("XBINNING", xbin);
//    imageData->getRecordValue("YBINNING", ybin);
//    QString binning = QString("%1x%2").arg(xbin).arg(ybin);
//    QString bitDepth = QString::number(imageData->getBPP());

//    QJsonObject metadata = {
//      {"resolution",resolution},
//      {"size",sizeBytes},
//      {"bin",binning},
//      {"bpp",bitDepth},
//    };

//    QJsonObject image =
//    {
//        //{"data", QString(jpegData.toBase64()) },
//        {"metadata", metadata},
//        //{"uuid", QUuid::createUuid().toString()}
//    };

//    sendResponse(EkosLiveClient::commands[EkosLiveClient::NEW_ALIGN_FRAME], image);

//    m_mediaSocket.sendBinaryMessage(jpegData);
//}

void EkosLiveClient::sendVideoFrame(std::unique_ptr<QImage> & frame)
{
    if (m_isConnected == false || !frame)
        return;

    // TODO Scale should be configurable
    QImage scaledImage =  frame.get()->scaledToWidth(640);
    QTemporaryFile jpegFile;
    jpegFile.open();
    jpegFile.close();

    // TODO Quality should be configurable
    scaledImage.save(jpegFile.fileName(), "jpg", 50);

    jpegFile.open();    

    m_mediaSocket.sendBinaryMessage(jpegFile.readAll());
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

    QJsonObject mountState = {
        {"status", m_Manager->mountStatus->text()},
        {"target", m_Manager->mountTarget->text()},
        {"slewRate", m_Manager->mountProcess.get()->getSlewRate()}
    };

    sendResponse(EkosLiveClient::commands[EkosLiveClient::NEW_MOUNT_STATE], mountState);

    QJsonObject focusState = {{ "status", m_Manager->focusStatus->text()}};
    sendResponse(EkosLiveClient::commands[EkosLiveClient::NEW_FOCUS_STATE], focusState);

    QJsonObject guideState = {{ "status", m_Manager->guideStatus->text()}};
    sendResponse(EkosLiveClient::commands[EkosLiveClient::NEW_GUIDE_STATE], guideState);

    QJsonObject alignState = {
        {"status", Ekos::alignStates[m_Manager->alignModule()->getStatus()]},
        {"solvers", QJsonArray::fromStringList(m_Manager->alignModule()->getActiveSolvers())},
        {"solverIndex", m_Manager->alignModule()->getActiveSolverIndex()},
    };
    sendResponse(EkosLiveClient::commands[EkosLiveClient::NEW_ALIGN_STATE], alignState);
}

void EkosLiveClient::sendEvent(const QString &message, KSNotification::EventType event)
{
    if (m_isConnected == false)
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
             {"hasVideo", oneCCD->hasVideoStream()}
        };

        cameraList.append(oneCamera);
    }

    sendResponse(EkosLiveClient::commands[EkosLiveClient::GET_CAMERAS], cameraList);
}

void EkosLiveClient::sendMounts()
{
    if (m_isConnected == false)
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
    if (m_isConnected == false)
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
    if (m_isConnected == false)
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

void EkosLiveClient::capturePreview(const QJsonObject &settings)
{
    QString camera = settings.value("camera").toString();
    QString filterWheel = settings.value("fw").toString();
    QString filter = settings.value("filter").toString();
    QString frame = settings.value("frame").toString("Light");
    double exp = settings.value("exp").toDouble(1);
    int bin = settings.value("bin").toInt(1);
    double temperature = settings.value("temperature").toDouble(-1000);


    Ekos::Capture * capture = m_Manager->captureProcess.get();
    capture->setCCD(camera);
    if (filterWheel.isEmpty() == false)
        capture->setFilter(filterWheel, filter);

    if (temperature != -1000)
    {
        capture->setForceTemperature(true);
        capture->setTargetTemperature(temperature);
    }

    capture->setFrameType(frame);
    m_Manager->captureProcess.get()->setExposure(exp);
    m_Manager->captureProcess.get()->setBinning(bin,bin);
    m_Manager->captureProcess.get()->captureOne();
}

void EkosLiveClient::processCaptureCommands(const QString &command, const QJsonObject &payload)
{
    Ekos::Capture *capture = m_Manager->captureModule();

    if (command == commands[CAPTURE_PREVIEW])
    {
        capturePreview(payload);
    }
    else if (command == commands[CAPTURE_TOGGLE_VIDEO])
    {
        capture->toggleVideo(payload["enabled"].toBool());
    }
    else if (command == commands[CAPTURE_START])
        capture->start();
    else if (command == commands[CAPTURE_STOP])
        capture->stop();
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
    {
        align->setCaptureSettings(payload);
        align->captureAndSolve();
    }
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
    if (m_isConnected == false)
        return;

    QJsonObject alignState = {
        {"status", Ekos::alignStates[newState]}
    };

    sendResponse(EkosLiveClient::commands[EkosLiveClient::NEW_ALIGN_STATE], alignState);
}

void EkosLiveClient::setAlignSolution(const QJsonObject &solution)
{
    if (m_isConnected == false)
        return;

    QJsonObject alignState = {
        {"solution", solution},
    };

    sendResponse(EkosLiveClient::commands[EkosLiveClient::NEW_ALIGN_STATE], alignState);
}
