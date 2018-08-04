/*  Ekos Live Message

    Copyright (C) 2018 Jasem Mutlaq <mutlaqja@ikarustech.com>

    Message Channel

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#include "message.h"
#include "commands.h"
#include "profileinfo.h"

#include "ekos_debug.h"

#include <QUuid>

namespace EkosLive
{

Message::Message(EkosManager *manager): m_Manager(manager)
{
    connect(&m_WebSocket, &QWebSocket::connected, this, &Message::onConnected);
    connect(&m_WebSocket, &QWebSocket::disconnected, this, &Message::onDisconnected);
    connect(&m_WebSocket, static_cast<void(QWebSocket::*)(QAbstractSocket::SocketError)>(&QWebSocket::error), this, &Message::onError);

}

void Message::connectServer()
{
    QUrl requestURL(m_URL);

    QUrlQuery query;
    query.addQueryItem("username", m_AuthResponse["username"].toString());
    query.addQueryItem("token", m_AuthResponse["token"].toString());
    query.addQueryItem("email", m_AuthResponse["email"].toString());
    query.addQueryItem("from_date", m_AuthResponse["from_date"].toString());
    query.addQueryItem("to_date", m_AuthResponse["to_date"].toString());
    query.addQueryItem("plan_id", m_AuthResponse["plan_id"].toString());

    requestURL.setPath("/message/ekos");
    requestURL.setQuery(query);


    m_WebSocket.open(requestURL);

    qCInfo(KSTARS_EKOS) << "Connecting to Websocket server at" << requestURL.toDisplayString();
}

void Message::disconnectServer()
{
    m_WebSocket.close();
}

void Message::onConnected()
{
    qCInfo(KSTARS_EKOS) << "Connected to Message Websocket server at" << m_URL.toDisplayString();

    m_isConnected = true;
    m_ReconnectTries=0;

    connect(&m_WebSocket, &QWebSocket::textMessageReceived,  this, &Message::onTextReceived);

    sendConnection();
    sendProfiles();

    emit connected();
}

void Message::onDisconnected()
{
    qCInfo(KSTARS_EKOS) << "Disonnected from Message Websocket server.";
    m_isConnected = false;
    disconnect(&m_WebSocket, &QWebSocket::textMessageReceived,  this, &Message::onTextReceived);

    emit disconnected();
}

void Message::onError(QAbstractSocket::SocketError error)
{
    qCritical(KSTARS_EKOS) << "Websocket connection error" << m_WebSocket.errorString();
    if (error == QAbstractSocket::RemoteHostClosedError ||
        error == QAbstractSocket::ConnectionRefusedError)
    {
        if (m_ReconnectTries++ < RECONNECT_MAX_TRIES)
            QTimer::singleShot(RECONNECT_INTERVAL, this, SLOT(connectServer()));
    }
}


void Message::onTextReceived(const QString &message)
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
    else if (command == commands[LOGOUT])
    {
        emit expired();
        return;
    }
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

void Message::sendCameras()
{
    if (m_isConnected == false)
        return;

    QJsonArray cameraList;

    for(ISD::GDInterface *gd : m_Manager->findDevices(KSTARS_CCD))
    {
        ISD::CCD *oneCCD = dynamic_cast<ISD::CCD*>(gd);
        connect(oneCCD, &ISD::CCD::newTemperatureValue, this, &Message::sendTemperature, Qt::UniqueConnection);
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

    sendResponse(commands[GET_CAMERAS], cameraList);
}

void Message::sendMounts()
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

    sendResponse(commands[GET_MOUNTS], mountList);

    // Also send initial slew rate
    for(ISD::GDInterface *gd : m_Manager->findDevices(KSTARS_TELESCOPE))
    {
        ISD::Telescope *oneTelescope = dynamic_cast<ISD::Telescope*>(gd);

        QJsonObject slewRate = {{"slewRate", oneTelescope->getSlewRate() }};

        sendResponse(commands[NEW_MOUNT_STATE], slewRate);
    }
}

void Message::sendScopes()
{
    if (m_isConnected == false ||
            m_Manager->getEkosStartingStatus() != EkosManager::EKOS_STATUS_SUCCESS ||
            m_Manager->mountModule() == nullptr)
        return;

    QJsonArray scopeList = m_Manager->mountModule()->getScopes();
    sendResponse(commands[GET_SCOPES], scopeList);
}

void Message::sendTemperature(double value)
{
    ISD::CCD *oneCCD = dynamic_cast<ISD::CCD*>(sender());

    QJsonObject temperature = {
        {"name", oneCCD->getDeviceName()},
        {"value", value}
    };

    sendResponse(commands[NEW_TEMPERATURE], temperature);
}

void Message::sendFilterWheels()
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

    sendResponse(commands[GET_FILTER_WHEELS], filterList);
}

void Message::setCaptureSettings(const QJsonObject &settings)
{
   m_Manager->captureModule()->setSettings(settings);
}

void Message::processCaptureCommands(const QString &command, const QJsonObject &payload)
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

void Message::sendCaptureSequence(const QJsonArray &sequenceArray)
{
    sendResponse(commands[CAPTURE_GET_SEQUENCES], sequenceArray);
}

void Message::sendCaptureSettings(const QJsonObject &settings)
{
    sendResponse(commands[CAPTURE_SET_SETTINGS], settings);
}

void Message::sendAlignSettings(const QJsonObject &settings)
{
    sendResponse(commands[ALIGN_SET_SETTINGS], settings);
}

void Message::processGuideCommands(const QString &command, const QJsonObject &payload)
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

void Message::processFocusCommands(const QString &command, const QJsonObject &payload)
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

void Message::processMountCommands(const QString &command, const QJsonObject &payload)
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

void Message::processAlignCommands(const QString &command, const QJsonObject &payload)
{
    Ekos::Align *align = m_Manager->alignModule();

    if (command == commands[ALIGN_SOLVE])
        align->captureAndSolve();
    else if (command == commands[ALIGN_SET_SETTINGS])
        align->setSettings(payload);
    else if (command == commands[ALIGN_STOP])        
        align->abort();    
    else if (command == commands[ALIGN_LOAD_AND_SLEW])
    {
        QTemporaryFile file;
        file.open();
        file.write(QByteArray::fromBase64(payload["data"].toString().toLatin1()));
        file.close();
        align->loadAndSlew(file.fileName());
    }
}

void Message::setAlignStatus(Ekos::AlignState newState)
{
    if (m_isConnected == false || m_Manager->getEkosStartingStatus() != EkosManager::EKOS_STATUS_SUCCESS)
        return;

    QJsonObject alignState = {
        {"status", Ekos::alignStates[newState]}
    };

    sendResponse(commands[NEW_ALIGN_STATE], alignState);
}

void Message::setAlignSolution(const QJsonObject &solution)
{
    if (m_isConnected == false || m_Manager->getEkosStartingStatus() != EkosManager::EKOS_STATUS_SUCCESS)
        return;

    QJsonObject alignState = {
        {"solution", solution},
    };

    sendResponse(commands[NEW_ALIGN_STATE], alignState);
}

void Message::processPolarCommands(const QString &command, const QJsonObject &payload)
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
    if (command == commands[PAH_SET_SETTINGS])
    {
        align->setPAHSettings(payload);
    }
    else if (command == commands[PAH_REFRESH])
    {
        align->setPAHRefreshDuration(payload["value"].toDouble(1));
        align->startPAHRefreshProcess();
    }
    else if (command == commands[PAH_RESET_VIEW])
    {
        emit resetPolarView();
    }
    else if (command == commands[PAH_SET_CROSSHAIR])
    {
        double x = payload["x"].toDouble();
        double y = payload["y"].toDouble();

        if (boundingRect.isNull() == false)
        {
            // #1 Find actual dimension inside the bounding rectangle
            // since if we have bounding rectable then x,y fractions are INSIDE it
            double boundX = x * boundingRect.width();
            double boundY = y * boundingRect.height();

            // #2 Find fraction of the dimensions above the full image size
            // Add to it the bounding rect top left offsets
            x = (boundX+boundingRect.x()) / viewSize.width();
            y = (boundY+boundingRect.y()) / viewSize.height();
        }

        align->setPAHCorrectionOffsetPercentage(x,y);
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

void Message::setPAHStage(Ekos::Align::PAHStage stage)
{
    if (m_isConnected == false || m_Manager->getEkosStartingStatus() != EkosManager::EKOS_STATUS_SUCCESS)
        return;

    Q_UNUSED(stage);
    Ekos::Align *align = m_Manager->alignModule();

    QJsonObject polarState = {
        {"stage", align->getPAHStage()}
    };


    // Increase size when select star
    if (stage == Ekos::Align::PAH_STAR_SELECT)
        align->zoomAlignView();

    sendResponse(commands[NEW_POLAR_STATE], polarState);
}

void Message::setPAHMessage(const QString &message)
{
    if (m_isConnected == false || m_Manager->getEkosStartingStatus() != EkosManager::EKOS_STATUS_SUCCESS)
        return;

    QTextDocument doc;
    doc.setHtml(message);
    QJsonObject polarState = {
        {"message", doc.toPlainText()}
    };

    sendResponse(commands[NEW_POLAR_STATE], polarState);
}

void Message::setPolarResults(QLineF correctionVector, QString polarError)
{
    if (m_isConnected == false || m_Manager->getEkosStartingStatus() != EkosManager::EKOS_STATUS_SUCCESS)
        return;

    this->correctionVector = correctionVector;

    QPointF center = 0.5 * correctionVector.p1() + 0.5 * correctionVector.p2();
    QJsonObject vector = {
        {"center_x", center.x()},
        {"center_y", center.y()},
        {"mag", correctionVector.length()},
        {"pa", correctionVector.angle()},
        {"error", polarError}
    };

    QJsonObject polarState = {
        {"vector", vector}
    };

    sendResponse(commands[NEW_POLAR_STATE], polarState);
}

void Message::setPAHEnabled(bool enabled)
{
    if (m_isConnected == false || m_Manager->getEkosStartingStatus() != EkosManager::EKOS_STATUS_SUCCESS)
        return;

    QJsonObject polarState = {
        {"enabled", enabled}
    };

    sendResponse(commands[NEW_POLAR_STATE], polarState);
}

void Message::processProfileCommands(const QString &command, const QJsonObject &payload)
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

void Message::sendProfiles()
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

void Message::setEkosStatingStatus(EkosManager::CommunicationStatus status)
{
    if (status == EkosManager::EKOS_STATUS_PENDING)
        return;

    QJsonObject connectionState = {
        {"connected", true},
        {"online", status == EkosManager::EKOS_STATUS_SUCCESS}
    };
    sendResponse(commands[NEW_CONNECTION_STATE], connectionState);
}

void Message::processOptionsCommands(const QString &command, const QJsonObject &payload)
{
    if (command == commands[OPTION_SET_HIGH_BANDWIDTH])
        m_Options[OPTION_SET_HIGH_BANDWIDTH] = payload["value"].toBool(true);
    else if (command == commands[OPTION_SET_IMAGE_TRANSFER])
        m_Options[OPTION_SET_IMAGE_TRANSFER] = payload["value"].toBool(true);
    else if (command == commands[OPTION_SET_NOTIFICATIONS])
        m_Options[OPTION_SET_NOTIFICATIONS] = payload["value"].toBool(true);
    else if (command == commands[OPTION_SET_CLOUD_STORAGE])
        m_Options[OPTION_SET_CLOUD_STORAGE] = payload["value"].toBool(false);

    emit optionsChanged(m_Options);
}

void Message::sendResponse(const QString &command, const QJsonObject &payload)
{
    m_WebSocket.sendTextMessage(QJsonDocument({{"type",command},{"payload",payload}}).toJson(QJsonDocument::Compact));
}

void Message::sendResponse(const QString &command, const QJsonArray &payload)
{
    m_WebSocket.sendTextMessage(QJsonDocument({{"type",command},{"payload",payload}}).toJson(QJsonDocument::Compact));
}

void Message::updateMountStatus(const QJsonObject &status)
{
    if (m_isConnected == false)
        return;

    sendResponse(commands[NEW_MOUNT_STATE], status);
}

void Message::updateCaptureStatus(const QJsonObject &status)
{
    if (m_isConnected == false)
        return;

    sendResponse(commands[NEW_CAPTURE_STATE], status);
}

void Message::updateFocusStatus(const QJsonObject &status)
{
    if (m_isConnected == false)
        return;

    sendResponse(commands[NEW_FOCUS_STATE], status);
}

void Message::updateGuideStatus(const QJsonObject &status)
{
    if (m_isConnected == false)
        return;

    sendResponse(commands[NEW_GUIDE_STATE], status);
}

void Message::sendConnection()
{
    if (m_isConnected == false)
        return;

    QJsonObject connectionState = {
        {"connected", true},
        {"online", m_Manager->getEkosStartingStatus() == EkosManager::EKOS_STATUS_SUCCESS}
    };
    sendResponse(commands[NEW_CONNECTION_STATE], connectionState);
}

void Message::sendStates()
{
    if (m_isConnected == false)
        return;

    QJsonObject captureState = {{ "status", m_Manager->captureStatus->text()}};
    sendResponse(commands[NEW_CAPTURE_STATE], captureState);

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

        sendResponse(commands[NEW_MOUNT_STATE], mountState);
    }

    QJsonObject focusState = {{ "status", m_Manager->focusStatus->text()}};
    sendResponse(commands[NEW_FOCUS_STATE], focusState);

    QJsonObject guideState = {{ "status", m_Manager->guideStatus->text()}};
    sendResponse(commands[NEW_GUIDE_STATE], guideState);

    if (m_Manager->alignModule())
    {
        // Align State
        QJsonObject alignState = {
            {"status", Ekos::alignStates[m_Manager->alignModule()->getStatus()]},
            {"solvers", QJsonArray::fromStringList(m_Manager->alignModule()->getActiveSolvers())}
        };
        sendResponse(commands[NEW_ALIGN_STATE], alignState);

        // Align settings
        sendResponse(commands[ALIGN_SET_SETTINGS], m_Manager->alignModule()->getSettings());

        // Polar State
        QTextDocument doc;
        doc.setHtml(m_Manager->alignModule()->getPAHMessage());
        QJsonObject polarState = {
            {"stage", m_Manager->alignModule()->getPAHStage()},
            {"enabled", m_Manager->alignModule()->isPAHEnabled()},
            {"message", doc.toPlainText()},
        };
        sendResponse(commands[NEW_POLAR_STATE], polarState);

        // Polar settings
        sendResponse(commands[PAH_SET_SETTINGS], m_Manager->alignModule()->getPAHSettings());
    }
}

void Message::sendEvent(const QString &message, KSNotification::EventType event)
{
    if (m_isConnected == false || m_Options[OPTION_SET_NOTIFICATIONS] == false)
        return;

    QJsonObject newEvent = {{ "severity", event}, {"message", message},{"uuid",QUuid::createUuid().toString()}};
    sendResponse(commands[NEW_NOTIFICATION], newEvent);
}

void Message::setBoundingRect(QRect rect, QSize view)
{
    boundingRect = rect;
    viewSize = view;
}
}
