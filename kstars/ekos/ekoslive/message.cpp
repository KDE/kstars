/* Ekos Live Message

    SPDX-FileCopyrightText: 2018 Jasem Mutlaq <mutlaqja@ikarustech.com>

    Message Channel

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "message.h"
#include "commands.h"
#include "profileinfo.h"
#include "indi/drivermanager.h"
#include "indi/indilistener.h"
#include "auxiliary/ksmessagebox.h"
#include "ekos/auxiliary/filtermanager.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "ekos_debug.h"
#include "fitsviewer/fitsview.h"
#include "ksalmanac.h"
#include "skymapcomposite.h"
#include "catalogobject.h"
#include "ekos/auxiliary/darklibrary.h"
#include "skymap.h"
#include "Options.h"

#include <KActionCollection>
#include <basedevice.h>
#include <QUuid>

namespace EkosLive
{
Message::Message(Ekos::Manager *manager): m_Manager(manager), m_DSOManager(CatalogsDB::dso_db_path())
{
    connect(&m_WebSocket, &QWebSocket::connected, this, &Message::onConnected);
    connect(&m_WebSocket, &QWebSocket::disconnected, this, &Message::onDisconnected);
    connect(&m_WebSocket, static_cast<void(QWebSocket::*)(QAbstractSocket::SocketError)>(&QWebSocket::error), this,
            &Message::onError);

    connect(manager, &Ekos::Manager::newModule, this, &Message::sendModuleState);

    m_ThrottleTS = QDateTime::currentDateTime();
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

    connect(&m_WebSocket, &QWebSocket::textMessageReceived,  this, &Message::onTextReceived, Qt::UniqueConnection);

    sendConnection();
    sendProfiles();

    emit connected();
}

void Message::onDisconnected()
{
    qCInfo(KSTARS_EKOS) << "Disconnected from Message Websocket server.";
    m_isConnected = false;
    disconnect(&m_WebSocket, &QWebSocket::textMessageReceived,  this, &Message::onTextReceived);
    m_PropertyCache.clear();

    emit disconnected();
}

void Message::onError(QAbstractSocket::SocketError error)
{
    qCritical(KSTARS_EKOS) << "Websocket connection error" << m_WebSocket.errorString();
    if (error == QAbstractSocket::RemoteHostClosedError ||
            error == QAbstractSocket::ConnectionRefusedError)
    {
        if (m_ReconnectTries++ < RECONNECT_MAX_TRIES)
            QTimer::singleShot(RECONNECT_INTERVAL, this, &Message::connectServer);
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
    else if (command == commands[GET_DSLR_LENSES])
        sendDSLRLenses();
    else if (command.startsWith("scope_"))
        processScopeCommands(command, payload);
    else if (command.startsWith("profile_"))
        processProfileCommands(command, payload);
    else if (command.startsWith("astro_"))
        processAstronomyCommands(command, payload);
    else if (command == commands[DIALOG_GET_RESPONSE])
        processDialogResponse(payload);
    else if (command.startsWith("option_"))
        processOptionsCommands(command, payload);
    else if (command.startsWith("scheduler"))
        processSchedulerCommands(command, payload);
    else if (command.startsWith("dslr_"))
        processDSLRCommands(command, payload);

    if (m_Manager->getEkosStartingStatus() != Ekos::Success)
        return;

    if (command == commands[GET_STATES])
        sendStates();
    else if (command == commands[GET_CAMERAS])
    {
        sendCameras();
        // Try to trigger any signals based on current camera list
        if (m_Manager->captureModule())
            m_Manager->captureModule()->checkCamera();
    }
    else if (command == commands[GET_MOUNTS])
        sendMounts();
    else if (command == commands[GET_FILTER_WHEELS])
        sendFilterWheels();
    else if (command == commands[GET_DOMES])
        sendDomes();
    else if (command == commands[GET_CAPS])
        sendCaps();
    else if (command == commands[GET_STELLARSOLVER_PROFILES])
        sendStellarSolverProfiles();
    else if (command == commands[GET_DEVICES])
        sendDevices();
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
    else if (command.startsWith("fm_"))
        processFilterManagerCommands(command, payload);
    else if (command.startsWith("dark_library_"))
        processDarkLibraryCommands(command, payload);
    else if (command.startsWith("device_"))
        processDeviceCommands(command, payload);

}

void Message::sendCameras()
{
    if (m_isConnected == false)
        return;

    QJsonArray cameraList;

    for(auto &oneDevice : INDIListener::devices())
    {
        auto camera = oneDevice->getCamera();
        if (!camera)
            continue;

        auto primaryChip = camera->getChip(ISD::CameraChip::PRIMARY_CCD);

        double temperature = Ekos::INVALID_VALUE;
        camera->getTemperature(&temperature);

        QJsonObject oneCamera =
        {
            {"name", camera->getDeviceName()},
            {"canBin", primaryChip->canBin()},
            {"hasTemperature", camera->hasCooler()},
            {"canCool", camera->canCool()},
            {"isoList", QJsonArray::fromStringList(primaryChip->getISOList())},
            {"hasVideo", camera->hasVideoStream()},
            {"hasGain", camera->hasGain()}
        };

        cameraList.append(oneCamera);
    }

    sendResponse(commands[GET_CAMERAS], cameraList);

    // Send initial state as well.
    for(auto &oneDevice : INDIListener::devices())
    {
        auto camera = oneDevice->getCamera();
        if (!camera)
            continue;

        QJsonObject state = {{"name", camera->getDeviceName()}};
        double value = 0;

        if (camera->canCool())
        {
            camera->getTemperature(&value);
            state["temperature"] = value;
        }
        if (camera->hasGain())
        {
            camera->getGain(&value);
            state["gain"] = value;
        }
        if (camera->getChip(ISD::CameraChip::PRIMARY_CCD)->getISOIndex() >= 0)
        {
            state["iso"] = camera->getChip(ISD::CameraChip::PRIMARY_CCD)->getISOIndex();
        }

        sendResponse(commands[NEW_CAMERA_STATE], state);
    }

    if (m_Manager->captureModule())
        sendCaptureSettings(m_Manager->captureModule()->getPresetSettings());
    if (m_Manager->alignModule())
        sendAlignSettings(m_Manager->alignModule()->getAllSettings());
    if (m_Manager->focusModule())
        sendFocusSettings(m_Manager->focusModule()->getAllSettings());
    if (m_Manager->guideModule())
        sendGuideSettings(m_Manager->guideModule()->getAllSettings());
}

void Message::sendMounts()
{
    if (m_isConnected == false || m_Manager->getEkosStartingStatus() != Ekos::Success)
        return;

    QJsonArray mountList;

    for(auto &oneDevice : INDIListener::devices())
    {
        auto mount = oneDevice->getMount();
        if (!mount)
            continue;

        QJsonObject oneMount =
        {
            {"name", mount->getDeviceName()},
            {"canPark", mount->canPark()},
            {"canSync", mount->canSync()},
            {"canControlTrack", mount->canControlTrack()},
            {"hasSlewRates", mount->hasSlewRates()},
            {"slewRates", QJsonArray::fromStringList(mount->slewRates())},
        };

        mountList.append(oneMount);
    }

    sendResponse(commands[GET_MOUNTS], mountList);

    // Also send initial slew rate
    for(auto &oneDevice : INDIListener::devices())
    {
        auto mount = oneDevice->getMount();
        if (!mount)
            continue;

        QJsonObject slewRate =
        {
            {"name", mount->getDeviceName() },
            {"slewRate", mount->getSlewRate() },
            {"pierSide", mount->pierSide() },
        };

        sendResponse(commands[NEW_MOUNT_STATE], slewRate);
    }

    if (m_Manager->mountModule())
    {
        // Mount module states
        QJsonObject mountModuleSettings =
        {
            {"altitudeLimitsEnabled", m_Manager->mountModule()->altitudeLimitsEnabled()},
            {"altitudeLimitsMin", m_Manager->mountModule()->altitudeLimits()[0]},
            {"altitudeLimitsMax", m_Manager->mountModule()->altitudeLimits()[1]},
            {"haLimitEnabled", m_Manager->mountModule()->hourAngleLimitEnabled()},
            {"haLimitValue", m_Manager->mountModule()->hourAngleLimit()},
            {"meridianFlipEnabled", m_Manager->mountModule()->meridianFlipEnabled()},
            {"meridianFlipDegreesValue", m_Manager->mountModule()->meridianFlipValue()},
            // obsolete value
            {"meridianFlipValue", m_Manager->mountModule()->meridianFlipValue() / 15},
            {"autoParkEnabled", m_Manager->mountModule()->autoParkEnabled()},
        };

        sendResponse(commands[MOUNT_GET_SETTINGS], mountModuleSettings);
    }
}

void Message::sendDomes()
{
    if (m_isConnected == false || m_Manager->getEkosStartingStatus() != Ekos::Success)
        return;

    QJsonArray domeList;

    for(auto &oneDevice : INDIListener::devices())
    {
        auto dome = oneDevice->getDome();
        if (!dome)
            continue;

        QJsonObject oneDome =
        {
            {"name", dome->getDeviceName()},
            {"canPark", dome->canPark()},
            {"canGoto", dome->canAbsoluteMove()},
            {"canAbort", dome->canAbort()},
        };

        domeList.append(oneDome);
    }

    sendResponse(commands[GET_DOMES], domeList);

    // Also send initial azimuth
    for(auto &oneDevice : INDIListener::devices())
    {
        auto dome = oneDevice->getDome();
        if (!dome)
            continue;

        QJsonObject status =
        {
            { "name", dome->getDeviceName()},
            { "status", ISD::Dome::getStatusString(dome->status())}
        };

        if (dome->canAbsoluteMove())
            status["az"] = dome->position();

        sendResponse(commands[NEW_DOME_STATE], status);

    }
}

void Message::sendCaps()
{
    if (m_isConnected == false || m_Manager->getEkosStartingStatus() != Ekos::Success)
        return;

    QJsonArray capList;

    for(auto &oneDevice : INDIListener::devices())
    {
        auto dustcap = oneDevice->getDustCap();
        if (!dustcap)
            continue;

        QJsonObject oneCap =
        {
            {"name", dustcap->getDeviceName()},
            {"canPark", dustcap->canPark()},
            {"hasLight", dustcap->hasLight()},
        };

        capList.append(oneCap);
    }

    sendResponse(commands[GET_CAPS], capList);

    for(auto &oneDevice : INDIListener::devices())
    {
        auto dustcap = oneDevice->getDustCap();
        if (!dustcap)
            continue;

        QJsonObject status =
        {
            { "name", dustcap->getDeviceName()},
            { "status", ISD::DustCap::getStatusString(dustcap->status())},
            { "lightS", dustcap->isLightOn()}
        };

        updateCapStatus(status);
    }
}


void Message::sendStellarSolverProfiles()
{
    if (m_isConnected == false || m_Manager->getEkosStartingStatus() != Ekos::Success)
        return;

    QJsonObject profiles;

    if (m_Manager->focusModule())
        profiles.insert("focus", QJsonArray::fromStringList(m_Manager->focusModule()->getStellarSolverProfiles()));
    // TODO
    //    if (m_Manager->guideModule())
    //        profiles.insert("guide", QJsonArray::fromStringList(m_Manager->guideModule()->getStellarSolverProfiles()));
    if (m_Manager->alignModule())
        profiles.insert("align", QJsonArray::fromStringList(m_Manager->alignModule()->getStellarSolverProfiles()));


    sendResponse(commands[GET_STELLARSOLVER_PROFILES], profiles);
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

    for(auto &gd : INDIListener::devices())
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


void Message::sendDSLRLenses()
{
    if (m_isConnected == false)
        return;

    QJsonArray dslrList;

    QList<OAL::DSLRLens *> allDslrLens;
    KStarsData::Instance()->userdb()->GetAllDSLRLenses(allDslrLens);

    for (auto &dslrLens : allDslrLens)
        dslrList.append(dslrLens->toJson());

    sendResponse(commands[GET_DSLR_LENSES], dslrList);
}

void Message::sendTemperature(double value)
{
    ISD::Camera *oneCCD = dynamic_cast<ISD::Camera*>(sender());

    if (oneCCD)
    {
        QJsonObject temperature =
        {
            {"name", oneCCD->getDeviceName()},
            {"temperature", value}
        };

        sendResponse(commands[NEW_CAMERA_STATE], temperature);
    }
}

void Message::sendFilterWheels()
{
    if (m_isConnected == false || m_Manager->getEkosStartingStatus() != Ekos::Success)
        return;

    QJsonArray filterList;

    for(auto &oneDevice : INDIListener::devices())
    {
        auto filterwheel = oneDevice->getFilterWheel();
        if (!filterwheel)
            continue;

        auto prop = filterwheel->getProperty("FILTER_NAME");
        if (!prop)
            break;

        auto filterNames = prop->getText();
        if (!filterNames)
            break;

        QJsonArray filters;
        for (const auto &it : *filterNames)
            filters.append(it.getText());

        QJsonObject oneFilter =
        {
            {"name", filterwheel->getDeviceName()},
            {"filters", filters}
        };

        filterList.append(oneFilter);
    }

    sendResponse(commands[GET_FILTER_WHEELS], filterList);
}

void Message::setCapturePresetSettings(const QJsonObject &settings)
{
    m_Manager->captureModule()->setPresetSettings(settings);
}

void Message::processCaptureCommands(const QString &command, const QJsonObject &payload)
{
    Ekos::Capture *capture = m_Manager->captureModule();

    if (capture == nullptr)
    {
        qCWarning(KSTARS_EKOS) << "Ignoring command" << command << "as capture module is not available";
        return;
    }

    if (command == commands[CAPTURE_PREVIEW])
    {
        setCapturePresetSettings(payload);
        capture->captureOne();
    }
    //    else if (command == commands[CAPTURE_TOGGLE_CAMERA])
    //    {
    //        capture->setCamera(payload["camera"].toString());
    //        sendCaptureSettings(capture->getPresetSettings());
    //    }
    //    else if (command == commands[CAPTURE_TOGGLE_FILTER_WHEEL])
    //    {
    //        capture->setFilterWheel(payload["fw"].toString());
    //        sendCaptureSettings(capture->getPresetSettings());
    //    }
    else if (command == commands[CAPTURE_TOGGLE_VIDEO])
    {
        capture->setVideoLimits(payload["maxBufferSize"].toInt(512), payload["maxPreviewFPS"].toInt(10));
        capture->toggleVideo(payload["enabled"].toBool());
    }
    else if (command == commands[CAPTURE_START])
        capture->start();
    else if (command == commands[CAPTURE_STOP])
        capture->stop();
    else if (command == commands[CAPTURE_LOOP])
    {
        setCapturePresetSettings(payload);
        capture->startFraming();
    }
    else if (command == commands[CAPTURE_GET_SEQUENCES])
    {
        sendCaptureSequence(capture->getSequence());
    }
    else if (command == commands[CAPTURE_ADD_SEQUENCE])
    {
        // Set capture settings first
        setCapturePresetSettings(payload["preset"].toObject());

        // Then sequence settings
        capture->setCount(static_cast<uint16_t>(payload["count"].toInt()));
        capture->setDelay(static_cast<uint16_t>(payload["delay"].toInt()));

        // File Settings
        m_Manager->captureModule()->setFileSettings(payload["file"].toObject());

        // Calibration Settings
        m_Manager->captureModule()->setCalibrationSettings(payload["calibration"].toObject());

        // Now add job
        capture->addJob();
    }
    else if (command == commands[CAPTURE_REMOVE_SEQUENCE])
    {
        if (capture->removeJob(payload["index"].toInt()) == false)
            sendCaptureSequence(capture->getSequence());
    }
    else if (command == commands[CAPTURE_CLEAR_SEQUENCES])
    {
        capture->clearSequenceQueue();
    }
    else if (command == commands[CAPTURE_SET_LIMITS])
    {
        capture->setLimitSettings(payload);
    }
    else if (command == commands[CAPTURE_GET_LIMITS])
    {
        sendResponse(commands[CAPTURE_GET_LIMITS], capture->getLimitSettings());
    }
    else if (command == commands[CAPTURE_SAVE_SEQUENCE_FILE])
    {
        capture->saveSequenceQueue(payload["filepath"].toString());
    }
    else if (command == commands[CAPTURE_LOAD_SEQUENCE_FILE])
    {
        capture->loadSequenceQueue(payload["filepath"].toString());
    }
    else if (command == commands[CAPTURE_GET_CALIBRATION_SETTINGS])
    {
        sendResponse(commands[CAPTURE_GET_CALIBRATION_SETTINGS], capture->getCalibrationSettings());
    }
    else if (command == commands[CAPTURE_GET_FILE_SETTINGS])
    {
        sendResponse(commands[CAPTURE_GET_FILE_SETTINGS], capture->getFileSettings());
    }
    else if (command == commands[CAPTURE_GENERATE_DARK_FLATS])
    {
        capture->generateDarkFlats();
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

void Message::sendAlignSettings(const QVariantMap &settings)
{
    sendResponse(commands[ALIGN_GET_ALL_SETTINGS], QJsonObject::fromVariantMap(settings));
}

void Message::sendGuideSettings(const QVariantMap &settings)
{
    sendResponse(commands[GUIDE_GET_ALL_SETTINGS], QJsonObject::fromVariantMap(settings));

}

void Message::sendFocusSettings(const QVariantMap &settings)
{
    sendResponse(commands[FOCUS_GET_ALL_SETTINGS], QJsonObject::fromVariantMap(settings));
}

void Message::sendSchedulerSettings(const QJsonObject &settings)
{
    sendResponse(commands[SCHEDULER_GET_SETTINGS], settings);
}

void Message::processGuideCommands(const QString &command, const QJsonObject &payload)
{
    Ekos::Guide *guide = m_Manager->guideModule();

    if (guide == nullptr)
    {
        qCWarning(KSTARS_EKOS) << "Ignoring command" << command << "as guide module is not available";
        return;
    }

    if (command == commands[GUIDE_START])
    {
        guide->guide();
    }
    else if (command == commands[GUIDE_CAPTURE])
        guide->capture();
    else if (command == commands[GUIDE_LOOP])
        guide->loop();
    else if (command == commands[GUIDE_STOP])
        guide->abort();
    else if (command == commands[GUIDE_CLEAR])
        guide->clearCalibration();
    else if (command == commands[GUIDE_SET_ALL_SETTINGS])
    {
        guide->setAllSettings(payload.toVariantMap());
        sendGuideSettings(m_Manager->guideModule()->getAllSettings());
    }
    else if(command == commands[GUIDE_SET_CALIBRATION_SETTINGS])
    {

        Options::setCalibrationPulseDuration(payload["pulse"].toInt());
        Options::setGuideCalibrationBacklash(payload["max_move"].toInt());
        Options::setTwoAxisEnabled(payload["two_axis"].toBool());
        Options::setGuideAutoSquareSizeEnabled(payload["square_size"].toBool());
        Options::setGuideCalibrationBacklash(payload["calibrationBacklash"].toBool());
        Options::setResetGuideCalibration(payload["resetCalibration"].toBool());
        Options::setReuseGuideCalibration(payload["reuseCalibration"].toBool());
        Options::setReverseDecOnPierSideChange(payload["reverseCalibration"].toBool());
        sendGuideSettings(m_Manager->guideModule()->getAllSettings());
    }
}

void Message::processFocusCommands(const QString &command, const QJsonObject &payload)
{
    Ekos::Focus *focus = m_Manager->focusModule();

    if (focus == nullptr)
    {
        qCWarning(KSTARS_EKOS) << "Ignoring command" << command << "as focus module is not available";
        return;
    }

    if (command == commands[FOCUS_START])
        focus->start();
    else if (command == commands[FOCUS_CAPTURE])
    {
        focus->resetFrame();
        focus->capture();
    }
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
    else if (command == commands[FOCUS_SET_ALL_SETTINGS])
        focus->setAllSettings(payload.toVariantMap());
    else if (command == commands[FOCUS_GET_ALL_SETTINGS])
        sendResponse(commands[FOCUS_GET_ALL_SETTINGS], QJsonObject::fromVariantMap(focus->getAllSettings()));
    else if (command == commands[FOCUS_SET_CROSSHAIR])
    {
        double x = payload["x"].toDouble();
        double y = payload["y"].toDouble();
        focus->selectFocusStarFraction(x, y);
    }
}

void Message::processMountCommands(const QString &command, const QJsonObject &payload)
{
    Ekos::Mount *mount = m_Manager->mountModule();

    if (mount == nullptr)
    {
        qCWarning(KSTARS_EKOS) << "Ignoring command" << command << "as mount module is not available";
        return;
    }

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
        ISD::Mount::MotionCommand action = payload["action"].toBool(false) ?
                                           ISD::Mount::MOTION_START : ISD::Mount::MOTION_STOP;

        if (direction == "N")
            mount->motionCommand(action, ISD::Mount::MOTION_NORTH, -1);
        else if (direction == "S")
            mount->motionCommand(action, ISD::Mount::MOTION_SOUTH, -1);
        else if (direction == "E")
            mount->motionCommand(action, -1, ISD::Mount::MOTION_EAST);
        else if (direction == "W")
            mount->motionCommand(action, -1, ISD::Mount::MOTION_WEST);
    }
    else if (command == commands[MOUNT_SET_ALTITUDE_LIMITS])
    {
        QList<double> limits;
        limits.append(payload["min"].toDouble(0));
        limits.append(payload["max"].toDouble(90));
        mount->setAltitudeLimits(limits);
        mount->setAltitudeLimitsEnabled(payload["enabled"].toBool());
    }
    else if (command == commands[MOUNT_SET_HA_LIMIT])
    {
        mount->setHourAngleLimit(payload["value"].toDouble());
        mount->setHourAngleLimitEnabled(payload["enabled"].toBool());
    }
    else if (command == commands[MOUNT_SET_MERIDIAN_FLIP])
    {
        // Meridian flip value is in degress. Need to convert to hours.
        mount->setMeridianFlipValues(payload["enabled"].toBool(),
                                     payload["value"].toDouble() / 15.0);
    }
    else if (command == commands[MOUNT_SET_EVERYDAY_AUTO_PARK])
    {
        const bool enabled = payload["enabled"].toBool();
        mount->setAutoParkDailyEnabled(enabled);
    }
    else if (command == commands[MOUNT_SET_AUTO_PARK])
    {
        const bool enabled = payload["enabled"].toBool();
        // Only set startup time when enabled.
        if (enabled)
            mount->setAutoParkStartup(QTime::fromString(payload["value"].toString()));
        mount->setAutoParkEnabled(enabled);
    }
    else if (command == commands[MOUNT_GOTO_PIXEL])
    {
        const auto name = payload["camera"].toString();
        const auto xFactor = payload["x"].toDouble();
        const auto yFactor = payload["y"].toDouble();

        for(auto &oneDevice : INDIListener::devices())
        {
            auto camera = oneDevice->getCamera();
            if (!camera  || camera->getDeviceName() != name)
                continue;

            auto primaryChip = camera->getChip(ISD::CameraChip::PRIMARY_CCD);

            if (!primaryChip)
                break;

            auto imageData = primaryChip->getImageData();
            if (!imageData || imageData->hasWCS() == false)
                break;

            auto x = xFactor * imageData->width();
            auto y = yFactor * imageData->height();

            QPointF point(x, y);
            SkyPoint coord;
            if (imageData->pixelToWCS(point, coord))
            {
                // J2000 -> JNow
                coord.apparentCoord(static_cast<long double>(J2000), KStars::Instance()->data()->ut().djd());
                mount->gotoTarget(coord);
                break;
            }
        }
    }
}

void Message::processAlignCommands(const QString &command, const QJsonObject &payload)
{
    Ekos::Align *align = m_Manager->alignModule();

    if (align == nullptr)
    {
        qCWarning(KSTARS_EKOS) << "Ignoring command" << command << "as align module is not available";
        return;
    }

    if (command == commands[ALIGN_SOLVE])
    {
        align->captureAndSolve();
    }
    else if (command == commands[ALIGN_SET_ALL_SETTINGS])
        align->setAllSettings(payload.toVariantMap());
    else if(command == commands[ALIGN_SET_ASTROMETRY_SETTINGS])
    {
        Options::setAstrometryRotatorThreshold(payload["threshold"].toInt());
        Options::setAstrometryUseRotator(payload["rotator_control"].toBool());
        Options::setAstrometryUseImageScale(payload["scale"].toBool());
        Options::setAstrometryUsePosition(payload["position"].toBool());
    }
    else if (command == commands[ALIGN_STOP])
        align->abort();
    else if (command == commands[ALIGN_LOAD_AND_SLEW])
    {
        QString filename = QDir::tempPath() + QDir::separator() +
                           QString("XXXXXXloadslew.%1").arg(payload["ext"].toString("fits"));
        QTemporaryFile file(filename);
        file.setAutoRemove(false);
        file.open();
        file.write(QByteArray::fromBase64(payload["data"].toString().toLatin1()));
        file.close();
        align->loadAndSlew(file.fileName());
    }
    else if (command == commands[ALIGN_MANUAL_ROTATOR_TOGGLE])
    {
        align->toggleManualRotator(payload["toggled"].toBool());
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

void Message::processSchedulerCommands(const QString &command, const QJsonObject &payload)
{
    Ekos::Scheduler *scheduler = m_Manager->schedulerModule();

    if (command == commands[SCHEDULER_SET_PRIMARY_SETTINGS])
    {
        scheduler->setPrimarySettings(payload);
    }
    else if (command == commands[SCHEDULER_SET_JOB_STARTUP_CONDITIONS])
    {
        scheduler->setJobStartupConditions(payload);
        sendSchedulerSettings(m_Manager->schedulerModule()->getSchedulerSettings());
    }
    else if (command == commands[SCHEDULER_SET_JOB_CONSTRAINTS])
    {
        scheduler->setJobConstraints(payload);
        sendSchedulerSettings(m_Manager->schedulerModule()->getSchedulerSettings());
    }
    else if (command == commands[SCHEDULER_SET_JOB_COMPLETION_SETTINGS])
    {
        scheduler->setJobCompletionConditions(payload);
        sendSchedulerSettings(m_Manager->schedulerModule()->getSchedulerSettings());
    }
    else if (command == commands[SCHEDULER_SET_OBSERVATORY_STARTUP_PROCEDURE])
    {
        scheduler->setObservatoryStartupProcedure(payload);
        sendSchedulerSettings(m_Manager->schedulerModule()->getSchedulerSettings());
    }
    else if (command == commands[SCHEDULER_SET_ABORTED_JOB_MANAGEMENT])
    {
        scheduler->setAbortedJobManagementSettings(payload);
        sendSchedulerSettings(m_Manager->schedulerModule()->getSchedulerSettings());
    }
    else if (command == commands[SCHEDULER_SET_OBSERVATORY_SHUTDOWN_PROCEDURE])
    {
        scheduler->setObservatoryShutdownProcedure(payload);
        sendSchedulerSettings(m_Manager->schedulerModule()->getSchedulerSettings());
    }
    else if (command == commands[SCHEDULER_GET_JOBS])
    {
        sendSchedulerJobs();
    }
    else if (command == commands[SCHEDULER_ADD_JOBS])
    {
        scheduler->addJob();
    }
    else if(command == commands[SCHEDULER_REMOVE_JOBS])
    {
        int index = payload["index"].toInt();
        scheduler->removeOneJob(index);
    }
    else if(command == commands[SCHEDULER_GET_SETTINGS])
    {
        sendResponse(commands[SCHEDULER_GET_SETTINGS], scheduler->getSchedulerSettings());
    }
    else if(command == commands[SCHEDULER_START_JOB])
    {
        scheduler->toggleScheduler();
    }

}

void Message::processPolarCommands(const QString &command, const QJsonObject &payload)
{
    Ekos::Align *align = m_Manager->alignModule();
    Ekos::PolarAlignmentAssistant *paa = align->polarAlignmentAssistant();

    if (!paa)
        return;

    if (command == commands[PAH_START])
    {
        paa->startPAHProcess();
    }
    if (command == commands[PAH_STOP])
    {
        paa->stopPAHProcess();
    }
    if (command == commands[PAH_SET_SETTINGS])
    {
        paa->setPAHSettings(payload);
    }
    else if (command == commands[PAH_REFRESH])
    {
        paa->setPAHRefreshDuration(payload["value"].toDouble(1));
        paa->startPAHRefreshProcess();
    }
    else if (command == commands[PAH_SET_ALGORITHM])
    {
        auto algorithmCombo = paa->findChild<QComboBox*>("PAHRefreshAlgorithmCombo");
        if (algorithmCombo)
            algorithmCombo->setCurrentIndex(static_cast<Ekos::PolarAlignmentAssistant::PAHRefreshAlgorithm>(payload["value"].toInt(1)));
    }
    else if (command == commands[PAH_RESET_VIEW])
    {
        emit resetPolarView();
    }
    else if (command == commands[PAH_SET_CROSSHAIR])
    {
        double x = payload["x"].toDouble();
        double y = payload["y"].toDouble();

        if (m_BoundingRect.isNull() == false)
        {
            // #1 Find actual dimension inside the bounding rectangle
            // since if we have bounding rectable then x,y fractions are INSIDE it
            double boundX = x * m_BoundingRect.width();
            double boundY = y * m_BoundingRect.height();

            // #2 Find fraction of the dimensions above the full image size
            // Add to it the bounding rect top left offsets
            // factors in the change caused by zoom
            x = ((boundX + m_BoundingRect.x()) / (m_CurrentZoom / 100)) / m_ViewSize.width();
            y = ((boundY + m_BoundingRect.y()) / (m_CurrentZoom / 100)) / m_ViewSize.height();

        }

        paa->setPAHCorrectionOffsetPercentage(x, y);
    }
    else if (command == commands[PAH_SELECT_STAR_DONE])
    {
        // This button was removed from the desktop PAA scheme.
        // Nothing to do.
        // TODO: Make sure this works.
    }
    else if (command == commands[PAH_REFRESHING_DONE])
    {
        paa->stopPAHProcess();
    }
    else if (command == commands[PAH_SLEW_DONE])
    {
        paa->setPAHSlewDone();
    }
    else if (command == commands[PAH_PAH_SET_ZOOM])
    {
        double scale = payload["scale"].toDouble();
        align->setAlignZoom(scale);
    }

}

void Message::setPAHStage(Ekos::PolarAlignmentAssistant::PAHStage stage)
{
    if (m_isConnected == false || m_Manager->getEkosStartingStatus() != Ekos::Success)
        return;

    Q_UNUSED(stage)
    Ekos::Align *align = m_Manager->alignModule();

    Ekos::PolarAlignmentAssistant *paa = align->polarAlignmentAssistant();

    if (!paa)
        return;

    QJsonObject polarState =
    {
        {"stage", paa->getPAHStageString(false)}
    };


    // Increase size when select star
    if (stage == Ekos::PolarAlignmentAssistant::PAH_STAR_SELECT)
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

void Message::setPolarResults(QLineF correctionVector, double polarError, double azError, double altError)
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
        {"error", polarError},
        {"azError", azError},
        {"altError", altError}
    };

    QJsonObject polarState =
    {
        {"vector", vector}
    };

    sendResponse(commands[NEW_POLAR_STATE], polarState);
}

void Message::setUpdatedErrors(double total, double az, double alt)
{
    if (m_isConnected == false || m_Manager->getEkosStartingStatus() != Ekos::Success)
        return;

    QJsonObject error =
    {
        {"updatedError", total},
        {"updatedAZError", az},
        {"updatedALTError", alt}
    };

    sendResponse(commands[NEW_POLAR_STATE], error);
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
        if (m_Manager->getEkosStartingStatus() != Ekos::Idle)
            m_Manager->stop();

        m_Manager->setProfile(payload["name"].toString());
        // Always Sync time before we start
        KStarsData::Instance()->changeDateTime(KStarsDateTime::currentDateTimeUtc());
        m_Manager->start();
    }
    else if (command == commands[STOP_PROFILE])
    {
        m_Manager->stop();

        // Close all FITS Viewers
        KStars::Instance()->clearAllViewers();

        m_PropertySubscriptions.clear();
        m_PropertyCache.clear();
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
    else if (command == commands[SET_PROFILE_PORT_SELECTION])
    {
        requestPortSelection(false);
        m_Manager->acceptPortSelection();
    }
}

void Message::sendProfiles()
{
    QJsonArray profileArray;

    QSharedPointer<ProfileInfo> profile;
    if (!m_Manager->getCurrentProfile(profile))
        return;

    for (auto &oneProfile : m_Manager->profiles)
        profileArray.append(oneProfile->toJson());

    QJsonObject profiles =
    {
        {"selectedProfile", profile->name},
        {"profiles", profileArray}
    };
    sendResponse(commands[GET_PROFILES], profiles);
}

void Message::sendSchedulerJobs()
{
    QJsonObject jobs =
    {
        {"jobs", m_Manager->schedulerModule()->getJSONJobs()}
    };
    sendResponse(commands[SCHEDULER_GET_JOBS], jobs);
}

void Message::sendSchedulerJobList(QJsonArray jobsList)
{
    QJsonObject jobs =
    {
        {"jobs", jobsList}
    };
    sendResponse(commands[SCHEDULER_GET_JOBS], jobs);
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
    if (command == commands[OPTION_SET])
    {
        const QJsonArray options = payload["options"].toArray();
        for (const auto &oneOption : options)
            Options::self()->setProperty(oneOption["name"].toString().toLatin1(), oneOption["value"].toVariant());
        return;
    }
    else if (command == commands[OPTION_GET])
    {
        const QJsonArray options = payload["options"].toArray();
        QJsonArray result;
        for (const auto &oneOption : options)
        {
            const auto name = oneOption["name"].toString();
            QVariant value = Options::self()->property(name.toLatin1());
            QVariantMap map;
            map["name"] = name;
            map["value"] = value;
            result.append(QJsonObject::fromVariantMap(map));
        }
        sendResponse(commands[OPTION_GET], result);
        return;
    }
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
        KStarsData::Instance()->userdb()->AddScope(payload["model"].toString(), payload["vendor"].toString(),
                payload["type"].toString(), payload["focal_length"].toDouble(), payload["aperture"].toDouble());
    }
    else if (command == commands[UPDATE_SCOPE])
    {
        KStarsData::Instance()->userdb()->AddScope(payload["model"].toString(), payload["vendor"].toString(),
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
    else if(command == commands[DSLR_ADD_LENS])
    {
        KStarsData::Instance()->userdb()->AddDSLRLens(payload["model"].toString(), payload["vendor"].toString(),
                payload["focal_length"].toDouble(), payload["focal_ratio"].toDouble());
    }
    else if (command == commands[DSLR_DELETE_LENS])
    {
        KStarsData::Instance()->userdb()->DeleteEquipment("dslrlens", payload["id"].toInt());
    }
    else if (command == commands[DSLR_UPDATE_LENS])
    {
        KStarsData::Instance()->userdb()->AddDSLRLens(payload["model"].toString(), payload["vendor"].toString(),
                payload["focal_length"].toDouble(), payload["focal_ratio"].toDouble(), payload["id"].toString());
    }

    sendDSLRLenses();
}

void Message::processFilterManagerCommands(const QString &command, const QJsonObject &payload)
{
    QSharedPointer<Ekos::FilterManager> manager;
    if (m_Manager->captureModule())
        manager = m_Manager->captureModule()->filterManager();

    if (manager.isNull())
        return;

    if (command == commands[FM_GET_DATA])
    {
        QJsonObject data = manager->toJSON();
        sendResponse(commands[FM_GET_DATA], data);
    }
    else if (command == commands[FM_SET_DATA])
    {
        manager->setFilterData(payload);
    }
}

void Message::processDarkLibraryCommands(const QString &command, const QJsonObject &payload)
{
    if (command == commands[DARK_LIBRARY_START])
    {
        Ekos::DarkLibrary::Instance()->setDarkSettings(payload);
        Ekos::DarkLibrary::Instance()->start();
    }
    else if(command == commands[DARK_LIBRARY_SET_SETTINGS])
    {
        Ekos::DarkLibrary::Instance()->setDarkSettings(payload);
    }
    else if(command == commands[DARK_LIBRARY_SET_CAMERA_PRESETS])
    {
        Ekos::DarkLibrary::Instance()->setCameraPresets(payload);
    }
    else if (command == commands[DARK_LIBRARY_STOP])
    {
        Ekos::DarkLibrary::Instance()->stop();
    }
    else if (command == commands[DARK_LIBRARY_GET_MASTERS_IMAGE])
    {
        const int row = payload["row"].toInt();
        Ekos::DarkLibrary::Instance()->loadIndexInView(row);
    }
    else if (command == commands[DARK_LIBRARY_GET_DARK_SETTINGS])
    {
        sendResponse(commands[DARK_LIBRARY_GET_DARK_SETTINGS], Ekos::DarkLibrary::Instance()->getDarkSettings());
    }
    else if (command == commands[DARK_LIBRARY_GET_CAMERA_PRESETS])
    {
        sendResponse(commands[DARK_LIBRARY_GET_CAMERA_PRESETS], Ekos::DarkLibrary::Instance()->getCameraPresets());
    }
    else if (command == commands[DARK_LIBRARY_SET_DEFECT_PIXELS])
    {
        Ekos::DarkLibrary::Instance()->setDefectPixels(payload);
    }
    else if (command == commands[DARK_LIBRARY_SAVE_MAP])
    {
        Ekos::DarkLibrary::Instance()->saveMapB->click();
    }
    else if (command == commands[DARK_LIBRARY_SET_DEFECT_FRAME])
    {
        Ekos::DarkLibrary::Instance()->setDefectMapEnabled(false);
    }
    else if (command == commands[DARK_LIBRARY_SET_DEFECT_SETTINGS])
    {
        Ekos::DarkLibrary::Instance()->setDefectSettings(payload);
    }
    else if (command == commands[DARK_LIBRARY_GET_DEFECT_SETTINGS])
    {
        sendResponse(commands[DARK_LIBRARY_GET_DEFECT_SETTINGS], Ekos::DarkLibrary::Instance()->getDefectSettings());
    }
    else if (command == commands[DARK_LIBRARY_GET_VIEW_MASTERS])
    {
        sendResponse(commands[DARK_LIBRARY_GET_VIEW_MASTERS], Ekos::DarkLibrary::Instance()->getViewMasters());
    }
    else if (command == commands[DARK_LIBRARY_CLEAR_MASTERS_ROW])
    {
        const int rowIndex = payload["row"].toInt();
        Ekos::DarkLibrary::Instance()->clearRow(rowIndex);
    }
}


void Message::processDeviceCommands(const QString &command, const QJsonObject &payload)
{
    QString device = payload["device"].toString();

    // In case we want to UNSUBSCRIBE from all at once
    if (device.isEmpty() && command == commands[DEVICE_PROPERTY_UNSUBSCRIBE])
    {
        m_PropertySubscriptions.clear();
        m_PropertyCache.clear();
        return;
    }

    QSharedPointer<ISD::GenericDevice> oneDevice;
    if (!INDIListener::findDevice(device, oneDevice))
        return;

    // Get specific property
    if (command == commands[DEVICE_PROPERTY_GET])
    {
        QJsonObject propObject;
        if (oneDevice->getJSONProperty(payload["property"].toString(), propObject, payload["compact"].toBool(true)))
            m_WebSocket.sendTextMessage(QJsonDocument({{"type", commands[DEVICE_PROPERTY_GET]}, {"payload", propObject}}).toJson(
            QJsonDocument::Compact));
    }
    // Set specific property
    else if (command == commands[DEVICE_PROPERTY_SET])
    {
        oneDevice->setJSONProperty(payload["property"].toString(), payload["elements"].toArray());
    }
    // Return ALL properties
    else if (command == commands[DEVICE_GET])
    {
        QJsonArray properties;
        for (const auto &oneProp : *oneDevice->getProperties())
        {
            QJsonObject singleProp;
            if (oneDevice->getJSONProperty(oneProp->getName(), singleProp, payload["compact"].toBool(false)))
                properties.append(singleProp);
        }

        QJsonObject response =
        {
            {"device", device},
            {"properties", properties}
        };

        m_WebSocket.sendTextMessage(QJsonDocument({{"type", commands[DEVICE_GET]}, {"payload", response}}).toJson(
            QJsonDocument::Compact));
    }
    // Subscribe to one or more properties
    // When subscribed, the updates are immediately pushed as soon as they are received.
    else if (command == commands[DEVICE_PROPERTY_SUBSCRIBE])
    {
        const QJsonArray properties = payload["properties"].toArray();
        const QJsonArray groups = payload["groups"].toArray();

        // Get existing subscribed props for this device
        QSet<QString> props;
        if (m_PropertySubscriptions.contains(device))
            props = m_PropertySubscriptions[device];

        // If it is just a single property, let's insert it to props.
        if (properties.isEmpty() == false)
        {
            for (const auto &oneProp : properties)
                props.insert(oneProp.toString());
        }
        // If group is specified, then we need to add ALL properties belonging to this group.
        else if (groups.isEmpty() == false)
        {
            QVariantList indiGroups = groups.toVariantList();
            for (auto &oneProp : *oneDevice->getProperties())
            {
                if (indiGroups.contains(oneProp->getGroupName()))
                    props.insert(oneProp->getName());
            }
        }
        // Otherwise, subscribe to ALL property in this device
        else
        {
            for (auto &oneProp : *oneDevice->getProperties())
                props.insert(oneProp->getName());
        }

        m_PropertySubscriptions[device] = props;
    }
    else if (command == commands[DEVICE_PROPERTY_UNSUBSCRIBE])
    {
        const QJsonArray properties = payload["properties"].toArray();
        const QJsonArray groups = payload["groups"].toArray();

        // Get existing subscribed props for this device
        QSet<QString> props;
        if (m_PropertySubscriptions.contains(device))
            props = m_PropertySubscriptions[device];

        // If it is just a single property, let's insert it to props.
        // If it is just a single property, let's insert it to props.
        if (properties.isEmpty() == false)
        {
            for (const auto &oneProp : properties)
                props.remove(oneProp.toString());
        }
        // If group is specified, then we need to add ALL properties belonging to this group.
        else if (groups.isEmpty() == false)
        {
            QVariantList indiGroups = groups.toVariantList();
            for (auto &oneProp : *oneDevice->getProperties())
            {
                if (indiGroups.contains(oneProp->getGroupName()))
                    props.remove(oneProp->getName());
            }
        }
        // Otherwise, subscribe to ALL property in this device
        else
        {
            for (auto &oneProp : *oneDevice->getProperties())
                props.remove(oneProp->getName());
        }

        m_PropertySubscriptions[device] = props;
    }
}

void Message::processAstronomyCommands(const QString &command, const QJsonObject &payload)
{
    if (command == commands[ASTRO_GET_ALMANC])
    {
        // Today's date
        const KStarsDateTime localTime  = KStarsData::Instance()->lt();
        // Local Midnight
        const KStarsDateTime midnight  = KStarsDateTime(localTime.date(), QTime(0, 0), Qt::LocalTime);

        KSAlmanac almanac(midnight, KStarsData::Instance()->geo());

        QJsonObject response =
        {
            {"SunRise", almanac.getSunRise()},
            {"SunSet", almanac.getSunSet()},
            {"SunMaxAlt", almanac.getSunMaxAlt()},
            {"SunMinAlt", almanac.getSunMinAlt()},
            {"MoonRise", almanac.getMoonRise()},
            {"MoonSet", almanac.getMoonSet()},
            {"MoonPhase", almanac.getMoonPhase()},
            {"MoonIllum", almanac.getMoonIllum()},
            {"Dawn", almanac.getDawnAstronomicalTwilight()},
            {"Dusk", almanac.getDuskAstronomicalTwilight()},

        };

        sendResponse(commands[ASTRO_GET_ALMANC], response);
    }
    // Get a list of object based on criteria
    else if (command == commands[ASTRO_SEARCH_OBJECTS])
    {
        // Set time if required.
        if (payload.contains("jd"))
        {
            KStarsDateTime jd = KStarsDateTime(payload["jd"].toDouble());
            KStarsData::Instance()->clock()->setUTC(jd);
        }

        // Search Criteria
        // Object Type
        auto objectType = static_cast<SkyObject::TYPE>(payload["type"].toInt(SkyObject::GALAXY));
        // Azimuth restriction
        auto objectDirection = static_cast<Direction>(payload["direction"].toInt(All));
        // Maximum Object Magnitude
        auto objectMaxMagnitude = payload["maxMagnitude"].toDouble(10);
        // Minimum Object Altitude
        auto objectMinAlt = payload["minAlt"].toDouble(15);
        // Minimum Duration that the object must be above the altitude (if any) seconds.
        auto objectMinDuration = payload["minDuration"].toInt(3600);
        // Minimum FOV in arcmins.
        auto objectMinFOV = payload["minFOV"].toDouble(0);
        // Data instance
        auto *data = KStarsData::Instance();
        // Geo Location
        auto *geo = KStarsData::Instance()->geo();
        // If we are before dawn, we check object altitude restrictions
        // Otherwise, all objects are welcome
        auto start = KStarsData::Instance()->lt();
        auto end = getNextDawn();
        if (start > end)
            // Add 1 day
            end = end.addDays(1);

        QVector<QPair<QString, const SkyObject *>> allObjects;
        CatalogsDB::CatalogObjectList dsoObjects;
        bool isDSO = false;

        switch (objectType)
        {
            // Stars
            case SkyObject::STAR:
            case SkyObject::CATALOG_STAR:
                allObjects.append(data->skyComposite()->objectLists(SkyObject::STAR));
                allObjects.append(data->skyComposite()->objectLists(SkyObject::CATALOG_STAR));
                break;
            // Planets & Moon
            case SkyObject::PLANET:
            case SkyObject::MOON:
                allObjects.append(data->skyComposite()->objectLists(SkyObject::PLANET));
                allObjects.append(data->skyComposite()->objectLists(SkyObject::MOON));
                break;
            // Comets & Asteroids
            case SkyObject::COMET:
                allObjects.append(data->skyComposite()->objectLists(SkyObject::COMET));
                break;
            case SkyObject::ASTEROID:
                allObjects.append(data->skyComposite()->objectLists(SkyObject::ASTEROID));
                break;
            // Clusters
            case SkyObject::OPEN_CLUSTER:
                dsoObjects.splice(dsoObjects.end(), m_DSOManager.get_objects(SkyObject::OPEN_CLUSTER, objectMaxMagnitude));
                isDSO = true;
                break;
            case SkyObject::GLOBULAR_CLUSTER:
                dsoObjects.splice(dsoObjects.end(), m_DSOManager.get_objects(SkyObject::GLOBULAR_CLUSTER, objectMaxMagnitude));
                isDSO = true;
                break;
            // Nebuale
            case SkyObject::GASEOUS_NEBULA:
                dsoObjects.splice(dsoObjects.end(), m_DSOManager.get_objects(SkyObject::GASEOUS_NEBULA, objectMaxMagnitude));
                isDSO = true;
                break;
            case SkyObject::PLANETARY_NEBULA:
                dsoObjects.splice(dsoObjects.end(), m_DSOManager.get_objects(SkyObject::PLANETARY_NEBULA, objectMaxMagnitude));
                isDSO = true;
                break;
            case SkyObject::GALAXY:
                dsoObjects.splice(dsoObjects.end(), m_DSOManager.get_objects(SkyObject::GALAXY, objectMaxMagnitude));
                isDSO = true;
                break;
            case SkyObject::SUPERNOVA:
            {
                if (!Options::showSupernovae())
                {
                    Options::setShowSupernovae(true);
                    data->setFullTimeUpdate();
                    KStars::Instance()->map()->forceUpdate();
                }
                allObjects.append(data->skyComposite()->objectLists(SkyObject::SUPERNOVA));
            }
            break;
            case SkyObject::SATELLITE:
            {
                if (!Options::showSatellites())
                {
                    Options::setShowSatellites(true);
                    data->setFullTimeUpdate();
                    KStars::Instance()->map()->forceUpdate();
                }
                allObjects.append(data->skyComposite()->objectLists(SkyObject::SATELLITE));
            }
            break;
            default:
                break;
        }

        // Sort by magnitude
        std::sort(allObjects.begin(), allObjects.end(), [](const auto & a, const auto & b)
        {
            return a.second->mag() < b.second->mag();
        });

        QMutableVectorIterator<QPair<QString, const SkyObject *>> objectIterator(allObjects);

        // Filter direction, if specified.
        if (objectDirection != All)
        {
            QPair<int, int> Quardent1(270, 360), Quardent2(0, 90), Quardent3(90, 180), Quardent4(180, 270);
            QPair<int, int> minAZ, maxAZ;
            switch (objectDirection)
            {
                case North:
                    minAZ = Quardent1;
                    maxAZ = Quardent2;
                    break;
                case East:
                    minAZ = Quardent2;
                    maxAZ = Quardent3;
                    break;
                case South:
                    minAZ = Quardent3;
                    maxAZ = Quardent4;
                    break;
                case West:
                    minAZ = Quardent4;
                    maxAZ = Quardent1;
                    break;
                default:
                    break;
            }

            if (isDSO)
            {
                CatalogsDB::CatalogObjectList::iterator dsoIterator = dsoObjects.begin();
                while (dsoIterator != dsoObjects.end())
                {
                    // If there a more efficient way to do this?
                    const double az = (*dsoIterator).recomputeHorizontalCoords(start, geo).az().Degrees();
                    if (! ((minAZ.first <= az && az <= minAZ.second) || (maxAZ.first <= az && az <= maxAZ.second)))
                        dsoIterator = dsoObjects.erase(dsoIterator);
                    else
                        ++dsoIterator;
                }
            }
            else
            {
                while (objectIterator.hasNext())
                {
                    const auto az = objectIterator.next().second->recomputeHorizontalCoords(start, geo).az().Degrees();
                    if (! ((minAZ.first <= az && az <= minAZ.second) || (maxAZ.first <= az && az <= maxAZ.second)))
                        objectIterator.remove();
                }
            }
        }

        // Maximum Magnitude
        if (!isDSO)
        {
            objectIterator.toFront();
            while (objectIterator.hasNext())
            {
                auto magnitude = objectIterator.next().second->mag();
                // Only filter for objects that have valid magnitude, otherwise, they're automatically included.
                if (magnitude != NaN::f && magnitude > objectMaxMagnitude)
                    objectIterator.remove();
            }
        }

        // Altitude
        if (isDSO)
        {
            CatalogsDB::CatalogObjectList::iterator dsoIterator = dsoObjects.begin();
            while (dsoIterator != dsoObjects.end())
            {
                double duration = 0;
                for (KStarsDateTime t = start; t < end; t = t.addSecs(3600.0))
                {
                    dms LST = geo->GSTtoLST(t.gst());
                    (*dsoIterator).EquatorialToHorizontal(&LST, geo->lat());
                    if ((*dsoIterator).alt().Degrees() >= objectMinAlt)
                        duration += 3600;
                }

                if (duration < objectMinDuration)
                    dsoIterator = dsoObjects.erase(dsoIterator);
                else
                    ++dsoIterator;
            }
        }
        else
        {
            objectIterator.toFront();
            while (objectIterator.hasNext())
            {
                auto oneObject = objectIterator.next().second;
                double duration = 0;

                for (KStarsDateTime t = start; t < end; t = t.addSecs(3600.0))
                {
                    auto LST = geo->GSTtoLST(t.gst());
                    const_cast<SkyObject *>(oneObject)->EquatorialToHorizontal(&LST, geo->lat());
                    if (oneObject->alt().Degrees() >= objectMinAlt)
                        duration += 3600;
                }

                if (duration < objectMinDuration)
                    objectIterator.remove();
            }
        }

        // For DSOs, check minimum required FOV, if any.
        if (isDSO && objectMinFOV > 0)
        {
            CatalogsDB::CatalogObjectList::iterator dsoIterator = dsoObjects.begin();
            while (dsoIterator != dsoObjects.end())
            {
                if ((*dsoIterator).a() < objectMinFOV)
                    dsoIterator = dsoObjects.erase(dsoIterator);
                else
                    ++dsoIterator;
            }
        }

        QStringList searchObjects;
        for (auto &oneObject : allObjects)
            searchObjects.append(oneObject.second->name());
        for (auto &oneObject : dsoObjects)
            searchObjects.append(oneObject.name());

        searchObjects.removeDuplicates();
        QJsonArray response = QJsonArray::fromStringList(searchObjects);

        sendResponse(commands[ASTRO_SEARCH_OBJECTS], response);
    }
    else if(command == commands[ASTRO_GET_OBJECT_INFO])
    {
        const auto name = payload["object"].toString();
        QJsonObject info;
        SkyObject *oneObject = KStarsData::Instance()->skyComposite()->findByName(name, false);
        if(oneObject)
        {
            info =
            {
                {"name", name},
                {"designations", QJsonArray::fromStringList(oneObject->longname().split(", "))},
                {"magnitude", oneObject->mag()},
                {"ra0", oneObject->ra0().Hours()},
                {"de0", oneObject->dec0().Degrees()},
                {"ra", oneObject->ra().Hours()},
                {"de", oneObject->dec().Degrees()},
                {"object", true}
            };
            sendResponse(commands[ASTRO_GET_OBJECT_INFO], info);
        }
        else
        {
            info =
            {
                {"name", name},
                {"object", false},
            };
            sendResponse(commands[ASTRO_GET_OBJECT_INFO], info );
        }

    }
    // Get a list of object based on criteria
    else if (command == commands[ASTRO_GET_OBJECTS_INFO])
    {
        // Set time if required.
        if (payload.contains("jd"))
        {
            KStarsDateTime jd = KStarsDateTime(payload["jd"].toDouble());
            KStarsData::Instance()->clock()->setUTC(jd);
        }

        // Object Names
        QVariantList objectNames = payload["names"].toArray().toVariantList();
        QJsonArray objectsArray;

        for (auto &oneName : objectNames)
        {
            const QString name = oneName.toString();
            SkyObject *oneObject = KStarsData::Instance()->skyComposite()->findByName(name, false);
            if (oneObject)
            {
                QJsonObject info =
                {
                    {"name", name},
                    {"designations", QJsonArray::fromStringList(oneObject->longname().split(", "))},
                    {"magnitude", oneObject->mag()},
                    {"ra0", oneObject->ra0().Hours()},
                    {"de0", oneObject->dec0().Degrees()},
                    {"ra", oneObject->ra().Hours()},
                    {"de", oneObject->dec().Degrees()},
                };

                // If DSO, add angular size.
                CatalogObject *dsoObject = dynamic_cast<CatalogObject*>(oneObject);
                if (dsoObject)
                {
                    info["a"] = dsoObject->a();
                    info["b"] = dsoObject->b();
                    info["pa"] = dsoObject->pa();
                }

                objectsArray.append(info);
            }
        }

        sendResponse(commands[ASTRO_GET_OBJECTS_INFO], objectsArray);
    }
    // Get a object observability alt/az/ha
    else if (command == commands[ASTRO_GET_OBJECTS_OBSERVABILITY])
    {
        // Set time if required.
        if (payload.contains("jd"))
        {
            KStarsDateTime jd = KStarsDateTime(payload["jd"].toDouble());
            KStarsData::Instance()->clock()->setUTC(jd);
        }

        // Object Names
        QVariantList objectNames = payload["names"].toArray().toVariantList();
        QJsonArray objectsArray;

        // Data instance
        auto *data = KStarsData::Instance();
        // Geo Location
        auto *geo = KStarsData::Instance()->geo();
        // UT
        auto ut = data->ut();

        for (auto &oneName : objectNames)
        {
            const QString name = oneName.toString();
            SkyObject *oneObject = data->skyComposite()->findByName(name, false);
            if (oneObject)
            {
                oneObject->EquatorialToHorizontal(data->lst(), geo->lat());
                dms ha(data->lst()->Degrees() - oneObject->ra().Degrees());
                QJsonObject info =
                {
                    {"name", name},
                    {"az", oneObject->az().Degrees()},
                    {"alt", oneObject->alt().Degrees()},
                    {"ha",  ha.Hours()},
                };

                objectsArray.append(info);
            }
        }

        sendResponse(commands[ASTRO_GET_OBJECTS_OBSERVABILITY], objectsArray);
    }
    else if (command == commands[ASTRO_GET_OBJECTS_RISESET])
    {
        // Set time if required.
        if (payload.contains("jd"))
        {
            KStarsDateTime jd = KStarsDateTime(payload["jd"].toDouble());
            KStarsData::Instance()->clock()->setUTC(jd);
        }

        // Object Names
        QVariantList objectNames = payload["names"].toArray().toVariantList();
        QJsonArray objectsArray;

        // Data instance
        auto *data = KStarsData::Instance();
        // Geo Location
        auto *geo = KStarsData::Instance()->geo();
        // UT
        QDateTime midnight = QDateTime(data->lt().date(), QTime());
        KStarsDateTime ut  = geo->LTtoUT(KStarsDateTime(midnight));

        int DayOffset = 0;
        if (data->lt().time().hour() > 12)
            DayOffset = 1;

        for (auto &oneName : objectNames)
        {
            const QString name = oneName.toString();
            SkyObject *oneObject = data->skyComposite()->findByName(name, false);
            if (oneObject)
            {
                QJsonObject info;
                //Prepare time/position variables
                //true = use rise time
                QTime riseTime = oneObject->riseSetTime(ut, geo, true);

                //If transit time is before rise time, use transit time for tomorrow
                QTime transitTime = oneObject->transitTime(ut, geo);
                if (transitTime < riseTime)
                    transitTime   = oneObject->transitTime(ut.addDays(1), geo);

                //If set time is before rise time, use set time for tomorrow
                //false = use set time
                QTime setTime = oneObject->riseSetTime(ut, geo, false);
                //false = use set time
                if (setTime < riseTime)
                    setTime  = oneObject->riseSetTime(ut.addDays(1), geo, false);

                info["name"] = name;
                if (riseTime.isValid())
                {
                    info["rise"] = QString::asprintf("%02d:%02d", riseTime.hour(), riseTime.minute());
                    info["set"] = QString::asprintf("%02d:%02d", setTime.hour(), setTime.minute());
                }
                else
                {
                    if (oneObject->alt().Degrees() > 0.0)
                    {
                        info["rise"] = "Circumpolar";
                        info["set"] = "Circumpolar";
                    }
                    else
                    {
                        info["rise"] = "Never rises";
                        info["set"] = "Never rises";
                    }
                }

                info["transit"] = QString::asprintf("%02d:%02d", transitTime.hour(), transitTime.minute());

                QJsonArray altitudes;
                for (double h = -12.0; h <= 12.0; h += 0.5)
                {
                    double hour = h + (24.0 * DayOffset);
                    KStarsDateTime offset = ut.addSecs(hour * 3600.0);
                    CachingDms LST = geo->GSTtoLST(offset.gst());
                    oneObject->EquatorialToHorizontal(&LST, geo->lat());
                    altitudes.append(oneObject->alt().Degrees());
                }

                info["altitudes"] = altitudes;

                objectsArray.append(info);
            }
        }

        sendResponse(commands[ASTRO_GET_OBJECTS_RISESET], objectsArray);
    }
}

KStarsDateTime Message::getNextDawn()
{
    // Today's date
    const KStarsDateTime localTime  = KStarsData::Instance()->lt();
    // Local Midnight
    const KStarsDateTime midnight  = KStarsDateTime(localTime.date(), QTime(0, 0), Qt::LocalTime);
    // Almanac
    KSAlmanac almanac(midnight, KStarsData::Instance()->geo());
    // Next Dawn
    KStarsDateTime nextDawn = midnight.addSecs(almanac.getDawnAstronomicalTwilight() * 24.0 * 3600.0);
    // If dawn is earliar than now, add a day
    if (nextDawn < localTime)
        nextDawn.addDays(1);

    return nextDawn;
}

void Message::requestDSLRInfo(const QString &cameraName)
{
    m_WebSocket.sendTextMessage(QJsonDocument({{"type", commands[DSLR_GET_INFO]}, {"payload", cameraName}}).toJson(
        QJsonDocument::Compact));
}

void Message::requestPortSelection(bool show)
{
    m_WebSocket.sendTextMessage(QJsonDocument({{"type", commands[GET_PROFILE_PORT_SELECTION]}, {"payload", show}}).toJson(
        QJsonDocument::Compact));
}

void Message::sendDialog(const QJsonObject &message)
{
    m_WebSocket.sendTextMessage(QJsonDocument({{"type", commands[DIALOG_GET_INFO]}, {"payload", message}}).toJson(
        QJsonDocument::Compact));
}

void Message::sendResponse(const QString &command, const QJsonObject &payload)
{
    m_WebSocket.sendTextMessage(QJsonDocument({{"type", command}, {"payload", payload}}).toJson(QJsonDocument::Compact));
}

void Message::sendResponse(const QString &command, const QJsonArray &payload)
{
    m_WebSocket.sendTextMessage(QJsonDocument({{"type", command}, {"payload", payload}}).toJson(QJsonDocument::Compact));
}

void Message::updateMountStatus(const QJsonObject &status, bool throttle)
{
    if (m_isConnected == false)
        return;

    if (throttle)
    {
        QDateTime now = QDateTime::currentDateTime();
        if (m_ThrottleTS.msecsTo(now) >= THROTTLE_INTERVAL)
        {
            m_ThrottleTS = now;
            sendResponse(commands[NEW_MOUNT_STATE], status);
        }
    }
    else
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

    // Send capture sequence if one exists
    if (m_Manager->captureModule())
    {
        QJsonObject captureState = {{ "status", getCaptureStatusString(m_Manager->captureModule()->status(), false)}};
        sendResponse(commands[NEW_CAPTURE_STATE], captureState);
        sendCaptureSequence(m_Manager->captureModule()->getSequence());
    }

    if (m_Manager->mountModule())
    {
        QJsonObject mountState =
        {
            {"status", m_Manager->mountModule()->statusString(false)},
            {"target", m_Manager->capturePreview->mountTarget->text()},
            {"slewRate", m_Manager->mountModule()->slewRate()},
            {"pierSide", m_Manager->mountModule()->pierSide()}
        };

        sendResponse(commands[NEW_MOUNT_STATE], mountState);
    }

    if (m_Manager->focusModule())
    {
        QJsonObject focusState = {{ "status", getFocusStatusString(m_Manager->focusModule()->status(), false)}};
        sendResponse(commands[NEW_FOCUS_STATE], focusState);
    }

    if (m_Manager->guideModule())
    {
        QJsonObject guideState = {{ "status", getGuideStatusString(m_Manager->guideModule()->status(), false)}};
        sendResponse(commands[NEW_GUIDE_STATE], guideState);
    }

    if (m_Manager->alignModule())
    {
        // Align State
        QJsonObject alignState =
        {
            {"status", getAlignStatusString(m_Manager->alignModule()->status(), false)}
        };
        sendResponse(commands[NEW_ALIGN_STATE], alignState);

        // Align settings
        sendAlignSettings(m_Manager->alignModule()->getAllSettings());

        Ekos::PolarAlignmentAssistant *paa = m_Manager->alignModule()->polarAlignmentAssistant();
        if (paa)
        {
            // Polar State
            QTextDocument doc;
            doc.setHtml(paa->getPAHMessage());
            QJsonObject polarState =
            {
                {"stage", paa->getPAHStageString(false)},
                {"enabled", paa->isEnabled()},
                {"message", doc.toPlainText()},
            };
            sendResponse(commands[NEW_POLAR_STATE], polarState);


            // Polar settings
            sendResponse(commands[PAH_SET_SETTINGS], paa->getPAHSettings());
        }
    }
}

void Message::sendEvent(const QString &message, KSNotification::EventSource source, KSNotification::EventType event)
{
    if (m_isConnected == false || m_Options[OPTION_SET_NOTIFICATIONS] == false)
        return;

    QJsonObject newEvent =
    {
        {"source", source},
        {"severity", event},
        {"message", message},
        {"uuid", QUuid::createUuid().toString()}
    };

    sendResponse(commands[NEW_NOTIFICATION], newEvent);
}

void Message::sendManualRotatorStatus(double currentPA, double targetPA, double threshold)
{
    if (m_isConnected == false)
        return;

    QJsonObject request = {{ "currentPA", currentPA}, {"targetPA", targetPA}, {"threshold", threshold}};
    sendResponse(commands[ALIGN_MANUAL_ROTATOR_STATUS], request);
}

void Message::setBoundingRect(QRect rect, QSize view, double currentZoom)
{
    m_BoundingRect = rect;
    m_ViewSize = view;
    m_CurrentZoom = currentZoom;
}

void Message::processDialogResponse(const QJsonObject &payload)
{
    KSMessageBox::Instance()->selectResponse(payload["button"].toString());
}

void Message::processNewProperty(INDI::Property prop)
{
    // Do not send new properties until all properties settle down
    // then send any properties that appears afterwards since the initial bunch
    // would cause a heavy message congestion.
    if (m_Manager->settleStatus() != Ekos::CommunicationStatus::Success)
        return;

    QJsonObject propObject;
    ISD::propertyToJson(prop, propObject, false);
    m_WebSocket.sendTextMessage(QJsonDocument({{"type", commands[DEVICE_PROPERTY_ADD]}, {"payload", propObject}}).toJson(
        QJsonDocument::Compact));
}

void Message::processDeleteProperty(const QString &device, const QString &name)
{
    QJsonObject payload =
    {
        {"device", device},
        {"name", name}
    };

    m_WebSocket.sendTextMessage(QJsonDocument({{"type", commands[DEVICE_PROPERTY_REMOVE]}, {"payload", payload}}).toJson(
        QJsonDocument::Compact));
}

void Message::processNewNumber(INumberVectorProperty * nvp)
{
    if (m_PropertySubscriptions.contains(nvp->device))
    {
        QSet<QString> subProps = m_PropertySubscriptions[nvp->device];
        if (subProps.contains(nvp->name))
        {
            QJsonObject * propObject = new QJsonObject();
            ISD::propertyToJson(nvp, *propObject);

            if (m_PropertyCache.contains(nvp->name) && *m_PropertyCache[nvp->name] == *propObject)
            {
                delete (propObject);
                return;
            }

            m_WebSocket.sendTextMessage(QJsonDocument({{"type", commands[DEVICE_PROPERTY_GET]}, {"payload", *propObject}}).toJson(
                QJsonDocument::Compact));
            m_PropertyCache.insert(nvp->name, propObject);
        }
    }
}

void Message::processNewText(ITextVectorProperty * tvp)
{
    if (m_PropertySubscriptions.contains(tvp->device))
    {
        QSet<QString> subProps = m_PropertySubscriptions[tvp->device];
        if (subProps.contains(tvp->name))
        {
            QJsonObject * propObject = new QJsonObject();
            ISD::propertyToJson(tvp, *propObject);

            if (m_PropertyCache.contains(tvp->name) && *m_PropertyCache[tvp->name] == *propObject)
            {
                delete (propObject);
                return;
            }

            m_WebSocket.sendTextMessage(QJsonDocument({{"type", commands[DEVICE_PROPERTY_GET]}, {"payload", *propObject}}).toJson(
                QJsonDocument::Compact));
            m_PropertyCache.insert(tvp->name, propObject);
        }
    }
}

void Message::processNewSwitch(ISwitchVectorProperty * svp)
{
    if (m_PropertySubscriptions.contains(svp->device))
    {
        QSet<QString> subProps = m_PropertySubscriptions[svp->device];
        if (subProps.contains(svp->name))
        {
            QJsonObject * propObject = new QJsonObject();
            ISD::propertyToJson(svp, *propObject);

            if (m_PropertyCache.contains(svp->name) && *m_PropertyCache[svp->name] == *propObject)
            {
                delete (propObject);
                return;
            }

            m_WebSocket.sendTextMessage(QJsonDocument({{"type", commands[DEVICE_PROPERTY_GET]}, {"payload", *propObject}}).toJson(
                QJsonDocument::Compact));
            m_PropertyCache.insert(svp->name, propObject);
        }
    }
}

void Message::processNewLight(ILightVectorProperty * lvp)
{
    if (m_PropertySubscriptions.contains(lvp->device))
    {
        QSet<QString> subProps = m_PropertySubscriptions[lvp->device];
        if (subProps.contains(lvp->name))
        {
            QJsonObject propObject;
            ISD::propertyToJson(lvp, propObject);
            m_WebSocket.sendTextMessage(QJsonDocument({{"type", commands[DEVICE_PROPERTY_GET]}, {"payload", propObject}}).toJson(
                QJsonDocument::Compact));
        }
    }
}

void Message::sendModuleState(const QString &name)
{
    if (m_isConnected == false)
        return;

    if (name == "Capture")
    {
        QJsonObject captureState = {{ "status", getCaptureStatusString(m_Manager->captureModule()->status(), false)}};
        sendResponse(commands[NEW_CAPTURE_STATE], captureState);
        sendCaptureSequence(m_Manager->captureModule()->getSequence());
    }
    else if (name == "Mount")
    {
        QJsonObject mountState =
        {
            {"status", m_Manager->mountStatus->getStatusText()},
            {"target", m_Manager->capturePreview->mountTarget->text()},
            {"slewRate", m_Manager->mountModule()->slewRate()},
            {"pierSide", m_Manager->mountModule()->pierSide()}
        };

        sendResponse(commands[NEW_MOUNT_STATE], mountState);
    }
    else if (name == "Focus")
    {
        QJsonObject focusState = {{ "status", getFocusStatusString(m_Manager->focusModule()->status(), false)}};
        sendResponse(commands[NEW_FOCUS_STATE], focusState);
    }
    else if (name == "Guide")
    {
        QJsonObject guideState = {{ "status", getGuideStatusString(m_Manager->guideModule()->status(), false)}};
        sendResponse(commands[NEW_GUIDE_STATE], guideState);
    }
    else if (name == "Align")
    {
        // Align State
        QJsonObject alignState =
        {
            {"status", getAlignStatusString(m_Manager->alignModule()->status(), false)}
        };
        sendResponse(commands[NEW_ALIGN_STATE], alignState);

        // Align settings
        sendAlignSettings(m_Manager->alignModule()->getAllSettings());

        Ekos::PolarAlignmentAssistant *paa = m_Manager->alignModule()->polarAlignmentAssistant();
        if (paa)
        {
            // Polar State
            QTextDocument doc;
            doc.setHtml(paa->getPAHMessage());
            QJsonObject polarState =
            {
                {"stage", paa->getPAHStageString(false)},
                {"enabled", paa->isEnabled()},
                {"message", doc.toPlainText()},
            };
            sendResponse(commands[NEW_POLAR_STATE], polarState);

            // Polar settings
            sendResponse(commands[PAH_SET_SETTINGS], paa->getPAHSettings());
        }
    }
}

}
