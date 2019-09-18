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
#include "indi/drivermanager.h"
#include "auxiliary/ksmessagebox.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "ekos_debug.h"

#include <KActionCollection>
#include <basedevice.h>
#include <QUuid>

namespace EkosLive
{

Message::Message(Ekos::Manager *manager): m_Manager(manager)
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
    if (m_AuthResponse.contains("remoteToken"))
        query.addQueryItem("remoteToken", m_AuthResponse["remoteToken"].toString());
    query.addQueryItem("email", m_AuthResponse["email"].toString());
    query.addQueryItem("from_date", m_AuthResponse["from_date"].toString());
    query.addQueryItem("to_date", m_AuthResponse["to_date"].toString());
    query.addQueryItem("plan_id", m_AuthResponse["plan_id"].toString());
    query.addQueryItem("type", m_AuthResponse["type"].toString());

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
    m_ReconnectTries = 0;

    connect(&m_WebSocket, &QWebSocket::textMessageReceived,  this, &Message::onTextReceived);

    sendConnection();
    sendProfiles();

    emit connected();
}

void Message::onDisconnected()
{
    qCInfo(KSTARS_EKOS) << "Disconnected from Message Websocket server.";
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
    {
        sendConnection();
    }
    else if (command == commands[LOGOUT])
    {
        emit expired();
        return;
    }
    else if (command == commands[SET_CLIENT_STATE])
    {
        // If client is connected, make sure clock is ticking
        if (payload["state"].toBool(false))
        {
            qCInfo(KSTARS_EKOS) << "EkosLive client is connected.";

            // If the clock is PAUSED, run it now and sync time as well.
            if (KStarsData::Instance()->clock()->isActive() == false)
            {
                qCInfo(KSTARS_EKOS) << "Resuming and syncing clock.";
                KStarsData::Instance()->clock()->start();
                QAction *a = KStars::Instance()->actionCollection()->action("time_to_now");
                if (a)
                    a->trigger();
            }
        }
        // Otherwise, if KStars was started in PAUSED state
        // then we pause here as well to save power.
        else
        {
            qCInfo(KSTARS_EKOS) << "EkosLive client is disconnected.";
            // It was started with paused state, so let's pause IF Ekos is not running
            if (KStars::Instance()->isStartedWithClockRunning() == false && m_Manager->ekosStatus() == Ekos::CommunicationStatus::Idle)
            {
                qCInfo(KSTARS_EKOS) << "Stopping the clock.";
                KStarsData::Instance()->clock()->stop();
            }
        }
    }
    else if (command == commands[GET_DRIVERS])
        sendDrivers();
    else if (command == commands[GET_PROFILES])
        sendProfiles();
    else if (command == commands[GET_SCOPES])
        sendScopes();
    else if (command.startsWith("scope_"))
        processScopeCommands(command, payload);
    else if (command.startsWith("profile_"))
        processProfileCommands(command, payload);

    if (m_Manager->getEkosStartingStatus() != Ekos::Success)
        return;

    if (command == commands[GET_STATES])
        sendStates();
    else if (command == commands[GET_CAMERAS])
    {
        sendCameras();
        // Try to trigger any signals based on current camera list
        if (m_Manager->captureModule())
            m_Manager->captureModule()->checkCCD();
    }
    else if (command == commands[GET_MOUNTS])
        sendMounts();
    else if (command == commands[GET_FILTER_WHEELS])
        sendFilterWheels();
    else if (command == commands[GET_DOMES])
        sendDomes();
    else if (command == commands[GET_CAPS])
        sendCaps();
    else if (command == commands[GET_DEVICES])
        sendDevices();
    else if (command == commands[DIALOG_GET_RESPONSE])
        processDialogResponse(payload);
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
    else if (command.startsWith("dome_"))
        processDomeCommands(command, payload);
    else if (command.startsWith("cap_"))
        processCapCommands(command, payload);
    else if (command.startsWith("option_"))
        processOptionsCommands(command, payload);
    else if (command.startsWith("dslr_"))
        processDSLRCommands(command, payload);
    else if (command.startsWith("device_"))
        processDeviceCommands(command, payload);
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
        connect(oneCCD, &ISD::CCD::previewJPEGGenerated, this, &Message::previewJPEGGenerated, Qt::UniqueConnection);
        ISD::CCDChip *primaryChip = oneCCD->getChip(ISD::CCDChip::PRIMARY_CCD);

        double temperature = Ekos::INVALID_VALUE;
        oneCCD->getTemperature(&temperature);

        QJsonObject oneCamera =
        {
            {"name", oneCCD->getDeviceName()},
            {"canBin", primaryChip->canBin()},
            {"hasTemperature", oneCCD->hasCooler()},
            {"canCool", oneCCD->canCool()},
            {"isoList", QJsonArray::fromStringList(oneCCD->getChip(ISD::CCDChip::PRIMARY_CCD)->getISOList())},
            {"hasVideo", oneCCD->hasVideoStream()},
            {"hasGain", oneCCD->hasGain()}
        };

        cameraList.append(oneCamera);
    }

    sendResponse(commands[GET_CAMERAS], cameraList);

    // Send initial state as well.
    for(ISD::GDInterface *gd : m_Manager->findDevices(KSTARS_CCD))
    {
        ISD::CCD *oneCCD = dynamic_cast<ISD::CCD*>(gd);
        QJsonObject state = {{"name", oneCCD->getDeviceName()}};
        double value = 0;

        if (oneCCD->canCool())
        {
            oneCCD->getTemperature(&value);
            state["temperature"] = value;
        }
        if (oneCCD->hasGain())
        {
            oneCCD->getGain(&value);
            state["gain"] = value;
        }
        if (oneCCD->getChip(ISD::CCDChip::PRIMARY_CCD)->getISOIndex() >= 0)
        {
            state["iso"] = oneCCD->getChip(ISD::CCDChip::PRIMARY_CCD)->getISOIndex();
        }

        sendResponse(commands[NEW_CAMERA_STATE], state);
    }

    if (m_Manager->captureModule())
        sendCaptureSettings(m_Manager->captureModule()->getSettings());
}

void Message::sendMounts()
{
    if (m_isConnected == false || m_Manager->getEkosStartingStatus() != Ekos::Success)
        return;

    QJsonArray mountList;

    for(ISD::GDInterface *gd : m_Manager->findDevices(KSTARS_TELESCOPE))
    {
        ISD::Telescope *oneTelescope = dynamic_cast<ISD::Telescope*>(gd);

        QJsonObject oneMount =
        {
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

        QJsonObject slewRate =
        {
            {"name", oneTelescope->getDeviceName() },
            {"slewRate", oneTelescope->getSlewRate() }
        };

        sendResponse(commands[NEW_MOUNT_STATE], slewRate);
    }
}

void Message::sendDomes()
{
    if (m_isConnected == false || m_Manager->getEkosStartingStatus() != Ekos::Success)
        return;

    QJsonArray domeList;

    for(ISD::GDInterface *gd : m_Manager->findDevices(KSTARS_DOME))
    {
        ISD::Dome *dome = dynamic_cast<ISD::Dome*>(gd);

        QJsonObject oneDome =
        {
            {"name", dome->getDeviceName()},
            {"canPark", dome->canPark()},
            {"canGoto", dome->canAbsMove()},
            {"canAbort", dome->canAbort()},
        };

        domeList.append(oneDome);
    }

    sendResponse(commands[GET_DOMES], domeList);

    // Also send initial azimuth
    for(ISD::GDInterface *gd : m_Manager->findDevices(KSTARS_DOME))
    {
        ISD::Dome *oneDome = dynamic_cast<ISD::Dome*>(gd);

        QJsonObject status =
        {
            { "name", oneDome->getDeviceName()},
            { "status", ISD::Dome::getStatusString(oneDome->status())}
        };
        if (oneDome->canAbsMove())
            status["az"] = oneDome->azimuthPosition();
        sendResponse(commands[NEW_DOME_STATE], status);
    }
}

void Message::sendCaps()
{
    if (m_isConnected == false || m_Manager->getEkosStartingStatus() != Ekos::Success)
        return;

    QJsonArray capList;

    for(ISD::GDInterface *gd : m_Manager->findDevices(KSTARS_AUXILIARY))
    {
        if (gd->getDriverInterface() & INDI::BaseDevice::DUSTCAP_INTERFACE)
        {
            ISD::DustCap *dustCap = dynamic_cast<ISD::DustCap*>(gd);

            QJsonObject oneCap =
            {
                {"name", dustCap->getDeviceName()},
                {"canPark", dustCap->canPark()},
                {"hasLight", dustCap->hasLight()},
            };

            capList.append(oneCap);
        }
    }

    sendResponse(commands[GET_CAPS], capList);

    for(ISD::GDInterface *gd : m_Manager->findDevices(KSTARS_AUXILIARY))
    {
        if (gd->getDriverInterface() & INDI::BaseDevice::DUSTCAP_INTERFACE)
        {
            ISD::DustCap *dustCap = dynamic_cast<ISD::DustCap*>(gd);
            QJsonObject status =
            {
                { "name", dustCap->getDeviceName()},
                { "status", ISD::DustCap::getStatusString(dustCap->status())},
                { "lightS", dustCap->isLightOn()}
            };

            updateCapStatus(status);
        }
    }
}

void Message::sendDrivers()
{
    if (m_isConnected == false)
        return;

    sendResponse(commands[GET_DRIVERS], DriverManager::Instance()->getDriverList());
}

void Message::sendDevices()
{
    if (m_isConnected == false || m_Manager->getEkosStartingStatus() != Ekos::Success)
        return;

    QJsonArray deviceList;

    for(ISD::GDInterface *gd : m_Manager->getAllDevices())
    {
        QJsonObject oneDevice =
        {
            {"name", gd->getDeviceName()},
            {"connected", gd->isConnected()},
            {"version", gd->getDriverVersion()},
            {"interface", static_cast<int>(gd->getDriverInterface())},
        };

        deviceList.append(oneDevice);
    }

    sendResponse(commands[GET_DEVICES], deviceList);
}

void Message::sendScopes()
{
    if (m_isConnected == false)
        return;

    QJsonArray scopeList;

    QList<OAL::Scope *> allScopes;
    KStarsData::Instance()->userdb()->GetAllScopes(allScopes);

    for (auto &scope : allScopes)
        scopeList.append(scope->toJson());

    sendResponse(commands[GET_SCOPES], scopeList);
}

void Message::sendTemperature(double value)
{
    ISD::CCD *oneCCD = dynamic_cast<ISD::CCD*>(sender());

    QJsonObject temperature =
    {
        {"name", oneCCD->getDeviceName()},
        {"temperature", value}
    };

    sendResponse(commands[NEW_CAMERA_STATE], temperature);
}

void Message::sendFilterWheels()
{
    if (m_isConnected == false || m_Manager->getEkosStartingStatus() != Ekos::Success)
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
        for (int i = 0; i < filterNames->ntp; i++)
            filters.append(filterNames->tp[i].text);

        QJsonObject oneFilter =
        {
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
    else if (command == commands[CAPTURE_TOGGLE_CAMERA])
    {
        capture->setCamera(payload["camera"].toString());
        sendCaptureSettings(capture->getSettings());
    }
    else if (command == commands[CAPTURE_TOGGLE_FILTER_WHEEL])
    {
        capture->setFilterWheel(payload["fw"].toString());
        sendCaptureSettings(capture->getSettings());
    }
    else if (command == commands[CAPTURE_TOGGLE_VIDEO])
    {
        capture->toggleVideo(payload["enabled"].toBool());
    }
    else if (command == commands[CAPTURE_START])
        capture->start();
    else if (command == commands[CAPTURE_STOP])
        capture->stop();
    else if (command == commands[CAPTURE_LOOP])
        capture->startFraming();
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
    Q_UNUSED(payload)

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
    Q_UNUSED(payload)

    if (command == commands[FOCUS_START])
        focus->start();
    else if (command == commands[FOCUS_STOP])
        focus->abort();
    else if (command == commands[FOCUS_RESET])
        focus->resetFrame();
    else if (command == commands[FOCUS_IN])
        focus->focusIn(payload["steps"].toInt());
    else if (command == commands[FOCUS_OUT])
        focus->focusOut(payload["steps"].toInt());
    else if (command == commands[FOCUS_LOOP])
        focus->startFraming();
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

void Message::processDomeCommands(const QString &command, const QJsonObject &payload)
{
    Ekos::Dome *dome = m_Manager->domeModule();

    Q_ASSERT_X(dome != nullptr, __FUNCTION__, "Dome module is not valid");

    if (command == commands[DOME_PARK])
        dome->park();
    else if (command == commands[DOME_UNPARK])
        dome->unpark();
    else if (command == commands[DOME_STOP])
        dome->abort();
    else if (command == commands[DOME_GOTO])
        dome->setAzimuthPosition(payload["az"].toDouble());
}

void Message::processCapCommands(const QString &command, const QJsonObject &payload)
{
    Ekos::DustCap *cap = m_Manager->capModule();

    Q_ASSERT_X(cap != nullptr, __FUNCTION__, "Dust cap module is not valid");

    if (command == commands[CAP_PARK])
        cap->park();
    else if (command == commands[CAP_UNPARK])
        cap->unpark();
    else if (command == commands[CAP_SET_LIGHT])
        cap->setLightEnabled(payload["enabled"].toBool());
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
    if (m_isConnected == false || m_Manager->getEkosStartingStatus() != Ekos::Success)
        return;

    QJsonObject alignState =
    {
        {"status", Ekos::alignStates[newState]}
    };

    sendResponse(commands[NEW_ALIGN_STATE], alignState);
}

void Message::setAlignSolution(const QVariantMap &solution)
{
    if (m_isConnected == false || m_Manager->getEkosStartingStatus() != Ekos::Success)
        return;

    QJsonObject alignState =
    {
        {"solution", QJsonObject::fromVariantMap(solution)},
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
            x = (boundX + boundingRect.x()) / viewSize.width();
            y = (boundY + boundingRect.y()) / viewSize.height();
        }

        align->setPAHCorrectionOffsetPercentage(x, y);
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
    if (m_isConnected == false || m_Manager->getEkosStartingStatus() != Ekos::Success)
        return;

    Q_UNUSED(stage)
    Ekos::Align *align = m_Manager->alignModule();

    QJsonObject polarState =
    {
        {"stage", align->getPAHStage()}
    };


    // Increase size when select star
    if (stage == Ekos::Align::PAH_STAR_SELECT)
        align->zoomAlignView();

    sendResponse(commands[NEW_POLAR_STATE], polarState);
}

void Message::setPAHMessage(const QString &message)
{
    if (m_isConnected == false || m_Manager->getEkosStartingStatus() != Ekos::Success)
        return;

    QTextDocument doc;
    doc.setHtml(message);
    QJsonObject polarState =
    {
        {"message", doc.toPlainText()}
    };

    sendResponse(commands[NEW_POLAR_STATE], polarState);
}

void Message::setPolarResults(QLineF correctionVector, QString polarError)
{
    if (m_isConnected == false || m_Manager->getEkosStartingStatus() != Ekos::Success)
        return;

    this->correctionVector = correctionVector;

    QPointF center = 0.5 * correctionVector.p1() + 0.5 * correctionVector.p2();
    QJsonObject vector =
    {
        {"center_x", center.x()},
        {"center_y", center.y()},
        {"mag", correctionVector.length()},
        {"pa", correctionVector.angle()},
        {"error", polarError}
    };

    QJsonObject polarState =
    {
        {"vector", vector}
    };

    sendResponse(commands[NEW_POLAR_STATE], polarState);
}

void Message::setPAHEnabled(bool enabled)
{
    if (m_isConnected == false || m_Manager->getEkosStartingStatus() != Ekos::Success)
        return;

    QJsonObject polarState =
    {
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

        // Close all FITS Viewers
        KStars::Instance()->clearAllViewers();
    }
    else if (command == commands[ADD_PROFILE])
    {
        m_Manager->addNamedProfile(payload);
        sendProfiles();
    }
    else if (command == commands[UPDATE_PROFILE])
    {
        m_Manager->editNamedProfile(payload);
        sendProfiles();
    }
    else if (command == commands[GET_PROFILE])
    {
        m_Manager->getNamedProfile(payload["name"].toString());
    }
    else if (command == commands[DELETE_PROFILE])
    {
        m_Manager->deleteNamedProfile(payload["name"].toString());
        sendProfiles();
    }
    else if (command == commands[SET_PROFILE_MAPPING])
    {
        m_Manager->setProfileMapping(payload);
    }
}

void Message::sendProfiles()
{
    QJsonArray profileArray;

    for (const auto &oneProfile : m_Manager->profiles)
        profileArray.append(oneProfile->toJson());

    QJsonObject profiles =
    {
        {"selectedProfile", m_Manager->getCurrentProfile()->name},
        {"profiles", profileArray}
    };
    sendResponse(commands[GET_PROFILES], profiles);
}

void Message::setEkosStatingStatus(Ekos::CommunicationStatus status)
{
    if (status == Ekos::Pending)
        return;

    QJsonObject connectionState =
    {
        {"connected", true},
        {"online", status == Ekos::Success}
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

void Message::processScopeCommands(const QString &command, const QJsonObject &payload)
{
    if (command == commands[ADD_SCOPE])
    {
        KStarsData::Instance()->userdb()->AddScope(payload["model"].toString(), payload["vendor"].toString(), payload["driver"].toString(),
                payload["type"].toString(), payload["focal_length"].toDouble(), payload["aperture"].toDouble());
    }
    else if (command == commands[UPDATE_SCOPE])
    {
        KStarsData::Instance()->userdb()->AddScope(payload["model"].toString(), payload["vendor"].toString(), payload["driver"].toString(),
                payload["type"].toString(), payload["focal_length"].toDouble(), payload["aperture"].toDouble(), payload["id"].toString());
    }
    else if (command == commands[DELETE_SCOPE])
    {
        KStarsData::Instance()->userdb()->DeleteEquipment("telescope", payload["id"].toInt());
    }

    sendScopes();
}

void Message::processDSLRCommands(const QString &command, const QJsonObject &payload)
{
    if (command == commands[DSLR_SET_INFO])
    {
        if (m_Manager->captureModule())
            m_Manager->captureModule()->addDSLRInfo(
                payload["model"].toString(),
                payload["width"].toInt(),
                payload["height"].toInt(),
                payload["pixelw"].toDouble(),
                payload["pixelh"].toDouble());

    }
}

void Message::processDeviceCommands(const QString &command, const QJsonObject &payload)
{
    if (command == commands[DEVICE_GET_PROPERTY])
    {
        QList<ISD::GDInterface *> devices = m_Manager->getAllDevices();
        QString device = payload["device"].toString();
        auto pos = std::find_if(devices.begin(), devices.end(), [device](ISD::GDInterface * oneDevice)
        {
            return (QString(oneDevice->getDeviceName()) == device);
        });

        if (pos == devices.end())
            return;

        auto oneDevice = *pos;

        auto oneProperty = oneDevice->getJSONProperty(payload["property"].toString(), payload["compact"].toBool(true));

        m_WebSocket.sendTextMessage(QJsonDocument({{"type", commands[DEVICE_GET_PROPERTY]}, {"payload", oneProperty}}).toJson(QJsonDocument::Compact));
    }
    else if (command == commands[DEVICE_SET_PROPERTY])
    {
        QList<ISD::GDInterface *> devices = m_Manager->getAllDevices();
        QString device = payload["device"].toString();
        auto pos = std::find_if(devices.begin(), devices.end(), [device](ISD::GDInterface * oneDevice)
        {
            return (QString(oneDevice->getDeviceName()) == device);
        });

        if (pos == devices.end())
            return;

        auto oneDevice = *pos;

        oneDevice->setJSONProperty(payload["property"].toString(), payload["value"].toArray());
    }
}

void Message::requestDSLRInfo(const QString &cameraName)
{
    m_WebSocket.sendTextMessage(QJsonDocument({{"type", commands[DSLR_GET_INFO]}, {"payload", cameraName}}).toJson(QJsonDocument::Compact));
}

void Message::sendDialog(const QJsonObject &message)
{
    m_WebSocket.sendTextMessage(QJsonDocument({{"type", commands[DIALOG_GET_INFO]}, {"payload", message}}).toJson(QJsonDocument::Compact));
}

void Message::sendResponse(const QString &command, const QJsonObject &payload)
{
    m_WebSocket.sendTextMessage(QJsonDocument({{"type", command}, {"payload", payload}}).toJson(QJsonDocument::Compact));
}

void Message::sendResponse(const QString &command, const QJsonArray &payload)
{
    m_WebSocket.sendTextMessage(QJsonDocument({{"type", command}, {"payload", payload}}).toJson(QJsonDocument::Compact));
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

void Message::updateDomeStatus(const QJsonObject &status)
{
    if (m_isConnected == false)
        return;

    sendResponse(commands[NEW_DOME_STATE], status);
}

void Message::updateCapStatus(const QJsonObject &status)
{
    if (m_isConnected == false)
        return;

    sendResponse(commands[NEW_CAP_STATE], status);
}

void Message::sendConnection()
{
    if (m_isConnected == false)
        return;

    QJsonObject connectionState =
    {
        {"connected", true},
        {"online", m_Manager->getEkosStartingStatus() == Ekos::Success}
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
        QJsonObject mountState =
        {
            {"status", m_Manager->mountStatus->text()},
            {"target", m_Manager->mountTarget->text()},
            {"slewRate", m_Manager->mountModule()->slewRate()}
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
        QJsonObject alignState =
        {
            {"status", Ekos::alignStates[m_Manager->alignModule()->status()]},
            {"solvers", QJsonArray::fromStringList(m_Manager->alignModule()->getActiveSolvers())}
        };
        sendResponse(commands[NEW_ALIGN_STATE], alignState);

        // Align settings
        sendResponse(commands[ALIGN_SET_SETTINGS], m_Manager->alignModule()->getSettings());

        // Polar State
        QTextDocument doc;
        doc.setHtml(m_Manager->alignModule()->getPAHMessage());
        QJsonObject polarState =
        {
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

    QJsonObject newEvent = {{ "severity", event}, {"message", message}, {"uuid", QUuid::createUuid().toString()}};
    sendResponse(commands[NEW_NOTIFICATION], newEvent);
}

void Message::setBoundingRect(QRect rect, QSize view)
{
    boundingRect = rect;
    viewSize = view;
}

void Message::processDialogResponse(const QJsonObject &payload)
{
    KSMessageBox::Instance()->selectResponse(payload["button"].toString());
}
}
