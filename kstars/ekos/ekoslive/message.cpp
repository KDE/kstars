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
#include "ekos/auxiliary/opticaltrainmanager.h"
#include "ekos/auxiliary/profilesettings.h"
#include "ekos/capture/capture.h"
#include "ekos/focus/focusmodule.h"
#include "ekos/guide/guide.h"
#include "ekos/mount/mount.h"
#include "ekos/scheduler/scheduler.h"
#include "ekos/scheduler/schedulermodulestate.h"
#include "kstars.h"
#include "kspaths.h"
#include "kstarsdata.h"
#include "ekos_debug.h"
#include "ksalmanac.h"
#include "skymapcomposite.h"
#include "catalogobject.h"
#include "fitsviewer/fitsviewer.h"
#include "fitsviewer/fitstab.h"
#include "ekos/auxiliary/darklibrary.h"
#include "skymap.h"
#include "Options.h"
#include "version.h"

#include <KActionCollection>
#include <basedevice.h>
#include <QUuid>
#include <thread>

namespace EkosLive
{
Message::Message(Ekos::Manager *manager, QVector<QSharedPointer<NodeManager >> &nodeManagers):
    m_Manager(manager), m_NodeManagers(nodeManagers), m_DSOManager(CatalogsDB::dso_db_path())
{
    for (auto &nodeManager : m_NodeManagers)
    {
        connect(nodeManager->message(), &Node::connected, this, &Message::onConnected);
        connect(nodeManager->message(), &Node::disconnected, this, &Message::onDisconnected);
        connect(nodeManager->message(), &Node::onTextReceived, this, &Message::onTextReceived);
    }

    connect(manager, &Ekos::Manager::newModule, this, &Message::sendModuleState);
    connect(INDIListener::Instance(), &INDIListener::deviceRemoved,
            this, [this](const QSharedPointer<ISD::GenericDevice> &device)
    {
        // Clear any pending properties for this device
        QMutableSetIterator<PendingProperty> it(m_PendingProperties);
        while (it.hasNext())
        {
            const auto &pending = it.next();
            if (pending.device == device->getDeviceName())
                it.remove();
        }
    });

    m_ThrottleTS = QDateTime::currentDateTime();

    m_PendingPropertiesTimer.setInterval(500);
    connect(&m_PendingPropertiesTimer, &QTimer::timeout, this, &Message::sendPendingProperties);

    m_DebouncedSend.setInterval(500);
    connect(&m_DebouncedSend, &QTimer::timeout, this, &Message::dispatchDebounceQueue);
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Message::onConnected()
{
    auto node = qobject_cast<Node*>(sender());
    if (!node)
        return;

    qCInfo(KSTARS_EKOS) << "Connected to Message Websocket server at" << node->url().toDisplayString();

    m_PendingPropertiesTimer.start();
    sendConnection();
    sendProfiles();
    emit connected();
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Message::onDisconnected()
{
    auto node = qobject_cast<Node*>(sender());
    if (!node)
        return;

    qCInfo(KSTARS_EKOS) << "Disconnected from Message Websocket server at" << node->url().toDisplayString();

    if (isConnected() == false)
    {
        m_PendingPropertiesTimer.stop();
        emit disconnected();
    }
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Message::onTextReceived(const QString &message)
{
    auto node = qobject_cast<Node*>(sender());
    if (!node || message.isEmpty())
        return;

    qCInfo(KSTARS_EKOS) << "Websocket Message" << message;
    QJsonParseError error;
    auto serverMessage = QJsonDocument::fromJson(message.toUtf8(), &error);
    if (error.error != QJsonParseError::NoError)
    {
        qCWarning(KSTARS_EKOS) << "Ekos Live Parsing Error" << error.errorString();
        return;
    }

    const QJsonObject msgObj = serverMessage.object();
    const QString command = msgObj["type"].toString();
    const QJsonObject payload = msgObj["payload"].toObject();

    if (command == commands[GET_CONNECTION])
    {
        sendConnection();
    }
    else if (command == commands[LOGOUT] || command == commands[SESSION_EXPIRED])
    {
        qCInfo(KSTARS_EKOS) << "Received" << command << "from node" << node->url().toDisplayString()
                            << ". Emitting globalLogoutTriggered signal with URL.";
        emit globalLogoutTriggered(node->url());
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
    else if(command == commands[INVOKE_METHOD])
    {
        auto object = findObject(payload["object"].toString());
        if (object)
            invokeMethod(object, payload);
    }
    else if(command == commands[SET_PROPERTY])
    {
        auto object = findObject(payload["object"].toString());
        if (object)
            object->setProperty(payload["name"].toString().toLatin1().constData(), payload["value"].toVariant());
    }
    else if(command == commands[GET_PROPERTY])
    {
        auto map = QVariantMap();
        map["result"] = false;
        auto object = findObject(payload["object"].toString());
        if (object)
        {
            auto value = object->property(payload["name"].toString().toLatin1().constData());
            if (value.isValid())
            {
                map["result"] = true;
                map["value"] = value;
            }
        }
        sendResponse(commands[GET_PROPERTY], QJsonObject::fromVariantMap(map));
    }
    else if (command == commands[TRAIN_GET_ALL])
        sendTrains();
    else if (command == commands[TRAIN_SETTINGS_GET])
    {
        auto id = payload["id"].toInt(-1);
        if (id > 0)
        {
            Ekos::OpticalTrainSettings::Instance()->setOpticalTrainID(id);
            auto settings = Ekos::OpticalTrainSettings::Instance()->getSettings();
            if (!settings.isEmpty())
                sendResponse(commands[TRAIN_SETTINGS_GET], QJsonObject::fromVariantMap(settings));
        }
    }
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
    else if (command.startsWith("file_"))
        processFileCommands(command, payload);

    if (m_Manager->getEkosStartingStatus() != Ekos::Success)
        return;

    if (command == commands[GET_STATES])
        sendStates();
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
    else if (command.startsWith("train_"))
        processTrainCommands(command, payload);
    else if (command.startsWith("fm_"))
        processFilterManagerCommands(command, payload);
    else if (command.startsWith("dark_library_"))
        processDarkLibraryCommands(command, payload);
    else if (command.startsWith("device_"))
        processDeviceCommands(command, payload);

}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
bool Message::isConnected() const
{
    return std::any_of(m_NodeManagers.begin(), m_NodeManagers.end(), [](auto & nodeManager)
    {
        return nodeManager->message()->isConnected();
    });
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Message::sendStellarSolverProfiles()
{
    if (m_Manager->getEkosStartingStatus() != Ekos::Success)
        return;

    QJsonObject profiles;

    if (m_Manager->focusModule())
        profiles.insert("focus", QJsonArray::fromStringList(m_Manager->focusModule()->mainFocuser()->getStellarSolverProfiles()));
    // TODO
    //    if (m_Manager->guideModule())
    //        profiles.insert("guide", QJsonArray::fromStringList(m_Manager->guideModule()->getStellarSolverProfiles()));
    if (m_Manager->alignModule())
        profiles.insert("align", QJsonArray::fromStringList(m_Manager->alignModule()->getStellarSolverProfiles()));


    sendResponse(commands[GET_STELLARSOLVER_PROFILES], profiles);
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Message::sendDrivers()
{
    sendResponse(commands[GET_DRIVERS], DriverManager::Instance()->getDriverList());
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Message::sendDevices()
{
    if (m_Manager->getEkosStartingStatus() != Ekos::Success)
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

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Message::sendTrains()
{
    QJsonArray trains;

    for(auto &train : Ekos::OpticalTrainManager::Instance()->getOpticalTrains())
        trains.append(QJsonObject::fromVariantMap(train));

    sendResponse(commands[TRAIN_GET_ALL], trains);
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Message::sendTrainProfiles()
{
    if (m_Manager->getEkosStartingStatus() != Ekos::Success)
        return;

    auto profiles = Ekos::ProfileSettings::Instance()->getSettings();

    sendResponse(commands[TRAIN_GET_PROFILES], QJsonObject::fromVariantMap(profiles));
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Message::requestOpticalTrains(bool show)
{
    sendResponse(commands[TRAIN_CONFIGURATION_REQUESTED], show);
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Message::sendScopes()
{
    QJsonArray scopeList;

    QList<OAL::Scope *> allScopes;
    KStarsData::Instance()->userdb()->GetAllScopes(allScopes);

    for (auto &scope : allScopes)
        scopeList.append(scope->toJson());

    sendResponse(commands[GET_SCOPES], scopeList);
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Message::sendDSLRLenses()
{
    QJsonArray dslrList;

    QList<OAL::DSLRLens *> allDslrLens;
    KStarsData::Instance()->userdb()->GetAllDSLRLenses(allDslrLens);

    for (auto &dslrLens : allDslrLens)
        dslrList.append(dslrLens->toJson());

    sendResponse(commands[GET_DSLR_LENSES], dslrList);
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
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

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Message::processCaptureCommands(const QString &command, const QJsonObject &payload)
{
    auto capture = m_Manager->captureModule();

    if (capture == nullptr)
    {
        qCWarning(KSTARS_EKOS) << "Ignoring command" << command << "as capture module is not available";
        return;
    }

    if (command == commands[CAPTURE_PREVIEW])
    {
        capture->mainCamera()->capturePreview();
    }
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
        capture->mainCamera()->startFraming();
    }
    else if (command == commands[CAPTURE_GET_SEQUENCES])
    {
        sendCaptureSequence(capture->getSequence());
    }
    else if (command == commands[CAPTURE_ADD_SEQUENCE])
    {
        // Now add job
        capture->mainCamera()->createJob();
    }
    else if (command == commands[CAPTURE_REMOVE_SEQUENCE])
    {
        if (capture->mainCamera()->removeJob(payload["index"].toInt()) == false)
            sendCaptureSequence(capture->getSequence());
    }
    else if (command == commands[CAPTURE_CLEAR_SEQUENCES])
    {
        capture->clearSequenceQueue();
    }
    else if (command == commands[CAPTURE_SAVE_SEQUENCE_FILE])
    {
        if (capture->saveSequenceQueue(payload["filepath"].toString()))
            sendResponse(commands[CAPTURE_SAVE_SEQUENCE_FILE], QString::fromUtf8(QFile(payload["filepath"].toString()).readAll()));
    }
    else if (command == commands[CAPTURE_LOAD_SEQUENCE_FILE])
    {
        QString path;
        if (payload.contains("filedata"))
        {
            QTemporaryFile file;
            if (file.open())
            {
                file.setAutoRemove(false);
                path = file.fileName();
                file.write(payload["filedata"].toString().toUtf8());
                file.close();
            }
        }
        else
            path = payload["filepath"].toString();

        if (!path.isEmpty())
        {
            auto result = capture->loadSequenceQueue(path);
            QJsonObject response =
            {
                {"result", result},
                {"path", path}
            };
            sendResponse(commands[CAPTURE_LOAD_SEQUENCE_FILE], response);
        }
    }
    else if (command == commands[CAPTURE_GET_ALL_SETTINGS])
    {
        sendCaptureSettings(capture->mainCamera()->getAllSettings());
    }
    else if (command == commands[CAPTURE_SET_ALL_SETTINGS])
    {
        auto settings = payload.toVariantMap();
        capture->mainCamera()->setAllSettings(settings);
        KSUtils::setGlobalSettings(settings);
    }
    else if (command == commands[CAPTURE_GENERATE_DARK_FLATS])
    {
        capture->mainCamera()->generateDarkFlats();
    }
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Message::sendCaptureSequence(const QJsonArray &sequenceArray)
{
    sendResponse(commands[CAPTURE_GET_SEQUENCES], sequenceArray);
}

void Message::sendPreviewLabel(const QString &preview)
{
    const QJsonObject payload =
    {
        {"preview", preview}
    };
    sendResponse(commands[CAPTURE_GET_PREVIEW_LABEL], payload);
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Message::sendCaptureSettings(const QVariantMap &settings)
{
    m_DebouncedSend.start();
    m_DebouncedMap[commands[CAPTURE_GET_ALL_SETTINGS]] = settings;
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Message::sendAlignSettings(const QVariantMap &settings)
{
    m_DebouncedSend.start();
    m_DebouncedMap[commands[ALIGN_GET_ALL_SETTINGS]] = settings;
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Message::sendGuideSettings(const QVariantMap &settings)
{
    m_DebouncedSend.start();
    m_DebouncedMap[commands[GUIDE_GET_ALL_SETTINGS]] = settings;

}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Message::sendFocusSettings(const QVariantMap &settings)
{
    m_DebouncedSend.start();
    m_DebouncedMap[commands[FOCUS_GET_ALL_SETTINGS]] = settings;
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Message::sendMountSettings(const QVariantMap &settings)
{
    m_DebouncedSend.start();
    m_DebouncedMap[commands[MOUNT_GET_ALL_SETTINGS]] = settings;
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Message::sendDarkLibrarySettings(const QVariantMap &settings)
{
    m_DebouncedSend.start();
    m_DebouncedMap[commands[DARK_LIBRARY_GET_ALL_SETTINGS]] = settings;
}


///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Message::sendSchedulerSettings(const QVariantMap &settings)
{
    m_DebouncedSend.start();
    m_DebouncedMap[commands[SCHEDULER_GET_ALL_SETTINGS]] = settings;
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Message::dispatchDebounceQueue()
{
    QMapIterator<QString, QVariantMap> i(m_DebouncedMap);
    while (i.hasNext())
    {
        i.next();
        sendResponse(i.key(), QJsonObject::fromVariantMap(i.value()));
    }
    m_DebouncedMap.clear();

    // Save to disk
    Options::self()->save();
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
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
        auto settings = payload.toVariantMap();
        guide->setAllSettings(settings);
        KSUtils::setGlobalSettings(settings);
    }
    else if (command == commands[GUIDE_GET_ALL_SETTINGS])
        sendGuideSettings(guide->getAllSettings());
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

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Message::processFocusCommands(const QString &command, const QJsonObject &payload)
{
    QSharedPointer<Ekos::Focus> focus;
    if (m_Manager->focusModule())
        focus = m_Manager->focusModule()->mainFocuser();

    if (focus.isNull())
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
    {
        auto settings = payload.toVariantMap();
        focus->setAllSettings(settings);
        KSUtils::setGlobalSettings(settings);
    }

    else if (command == commands[FOCUS_GET_ALL_SETTINGS])
        sendFocusSettings(focus->getAllSettings());
    else if (command == commands[FOCUS_SET_CROSSHAIR])
    {
        double x = payload["x"].toDouble();
        double y = payload["y"].toDouble();
        focus->selectFocusStarFraction(x, y);
    }
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
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
        auto ra = dms::fromString(payload["ra"].toString(), false);
        auto de = dms::fromString(payload["de"].toString(), true);
        mount->sync(ra.Hours(), de.Degrees());
    }
    else if (command == commands[MOUNT_SYNC_TARGET])
    {
        mount->syncTarget(payload["target"].toString());
    }
    else if (command == commands[MOUNT_GOTO_RADE])
    {
        mount->setJ2000Enabled(payload["isJ2000"].toBool());
        auto ra = dms::fromString(payload["ra"].toString(), false);
        auto de = dms::fromString(payload["de"].toString(), true);
        mount->slew(ra.Hours(), de.Degrees());
    }
    else if (command == commands[MOUNT_GOTO_TARGET])
    {
        mount->gotoTarget(ki18n(payload["target"].toString().toLatin1()).toString());
    }
    else if (command == commands[MOUNT_SET_SLEW_RATE])
    {
        int rate = payload["rate"].toInt(-1);
        if (rate >= 0)
            mount->setSlewRate(rate);
    }
    else if (command == commands[MOUNT_SET_ALL_SETTINGS])
    {
        auto settings = payload.toVariantMap();
        mount->setAllSettings(settings);
        KSUtils::setGlobalSettings(settings);
    }
    else if (command == commands[MOUNT_GET_ALL_SETTINGS])
        sendMountSettings(mount->getAllSettings());
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
    else if (command == commands[MOUNT_TOGGLE_AUTOPARK])
        mount->setAutoParkEnabled(payload["toggled"].toBool());
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
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
    {
        auto settings = payload.toVariantMap();
        align->setAllSettings(settings);
        KSUtils::setGlobalSettings(settings);
    }
    else if (command == commands[ALIGN_GET_ALL_SETTINGS])
        sendAlignSettings(align->getAllSettings());
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
        // Check if we have filename payload first
        if (payload.contains("filename"))
        {
            align->loadAndSlew(payload["filename"].toString());
        }
        else
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
    }
    else if (command == commands[ALIGN_MANUAL_ROTATOR_TOGGLE])
    {
        align->toggleManualRotator(payload["toggled"].toBool());
    }
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Message::setAlignStatus(Ekos::AlignState newState)
{
    if (m_Manager->getEkosStartingStatus() != Ekos::Success)
        return;

    QJsonObject alignState =
    {
        {"status", QString::fromLatin1(Ekos::alignStates[newState].untranslatedText())}
    };

    sendResponse(commands[NEW_ALIGN_STATE], alignState);
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Message::setAlignSolution(const QVariantMap &solution)
{
    if (m_Manager->getEkosStartingStatus() != Ekos::Success)
        return;

    QJsonObject alignState =
    {
        {"solution", QJsonObject::fromVariantMap(solution)},
    };

    sendResponse(commands[NEW_ALIGN_STATE], alignState);
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Message::processSchedulerCommands(const QString &command, const QJsonObject &payload)
{
    Ekos::Scheduler *scheduler = m_Manager->schedulerModule();

    if (command == commands[SCHEDULER_GET_JOBS])
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
    else if(command == commands[SCHEDULER_GET_ALL_SETTINGS])
    {
        sendSchedulerSettings(scheduler->getAllSettings());
    }
    else if(command == commands[SCHEDULER_SET_ALL_SETTINGS])
    {
        auto settings = payload.toVariantMap();
        scheduler->setAllSettings(settings);
        KSUtils::setGlobalSettings(settings);
    }
    else if (command == commands[SCHEDULER_SAVE_FILE])
    {
        if (scheduler->saveFile(QUrl::fromLocalFile(payload["filepath"].toString())))
            sendResponse(commands[SCHEDULER_SAVE_FILE], QString::fromUtf8(QFile(payload["filepath"].toString()).readAll()));
    }
    else if (command == commands[SCHEDULER_SAVE_SEQUENCE_FILE])
    {
        QString path;
        bool result = false;
        if (payload.contains("filedata"))
        {
            path = QDir::homePath() + QDir::separator() + payload["path"].toString();
            QFile file(path);
            if (file.open(QIODevice::WriteOnly))
            {
                result = true;
                file.write(payload["filedata"].toString().toUtf8());
                file.close();
            }
        }

        QJsonObject response =
        {
            {"result", result},
            {"path", path}
        };
        sendResponse(commands[SCHEDULER_SAVE_SEQUENCE_FILE], response);
    }
    else if (command == commands[SCHEDULER_LOAD_FILE])
    {
        QString path = payload["filepath"].toString();
        bool success = true;

        if (payload.contains("filedata"))
        {
            // Get path from temporary file if needed
            if (path.isEmpty())
            {
                QTemporaryFile tempFile;
                if (!tempFile.open())
                {
                    success = false;
                }
                else
                {
                    tempFile.setAutoRemove(false);
                    path = tempFile.fileName();
                }
            }
            // Path for filedata is relative to home directory.
            else
                path = QDir::homePath() + QDir::separator() + path;

            // Write file data if we have a valid path
            if (success && !path.isEmpty())
            {
                QFile file(path);
                if (!file.open(QIODevice::WriteOnly) ||
                        file.write(payload["filedata"].toString().toUtf8()) == -1)
                {
                    success = false;
                }
            }
        }

        // Load the file if we have a path
        if (success && !path.isEmpty())
        {
            success = scheduler->loadFile(QUrl::fromLocalFile(path));
        }

        QJsonObject response
        {
            {"result", success}
        };
        if (success && !path.isEmpty())
        {
            response["path"] = path;
        }

        sendResponse(commands[SCHEDULER_LOAD_FILE], response);
    }
    else if(command == commands[SCHEDULER_START_JOB])
    {
        scheduler->toggleScheduler();
    }
    else if(command == commands[SCHEDULER_IMPORT_MOSAIC])
    {
        if (scheduler->importMosaic(payload))
            sendSchedulerJobs();
        else
            sendEvent(i18n("Mosaic import failed."), KSNotification::Scheduler, KSNotification::Alert);
    }
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
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
    else if (command == commands[PAH_REFRESH])
    {
        paa->setPAHRefreshDuration(payload["value"].toDouble(1));
        paa->startPAHRefreshProcess();
    }
    else if (command == commands[PAH_SET_ALGORITHM])
    {
        auto algorithmCombo = paa->findChild<QComboBox*>("PAHRefreshAlgorithmCombo");
        if (algorithmCombo)
            algorithmCombo->setCurrentIndex(static_cast<Ekos::PolarAlignmentAssistant::RefreshAlgorithm>(payload["value"].toInt(1)));
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

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Message::setPAHStage(Ekos::PolarAlignmentAssistant::Stage stage)
{
    if (isConnected() == false || m_Manager->getEkosStartingStatus() != Ekos::Success)
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

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Message::setPAHMessage(const QString &message)
{
    if (isConnected() == false || m_Manager->getEkosStartingStatus() != Ekos::Success)
        return;

    QTextDocument doc;
    doc.setHtml(message);
    QJsonObject polarState =
    {
        {"message", doc.toPlainText()}
    };

    sendResponse(commands[NEW_POLAR_STATE], polarState);
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Message::setPolarResults(QLineF correctionVector, double polarError, double azError, double altError)
{
    if (isConnected() == false || m_Manager->getEkosStartingStatus() != Ekos::Success)
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

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Message::setUpdatedErrors(double total, double az, double alt)
{
    if (isConnected() == false || m_Manager->getEkosStartingStatus() != Ekos::Success)
        return;

    QJsonObject error =
    {
        {"updatedError", total},
        {"updatedAZError", az},
        {"updatedALTError", alt}
    };

    sendResponse(commands[NEW_POLAR_STATE], error);
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Message::setPAHEnabled(bool enabled)
{
    if (m_Manager->getEkosStartingStatus() != Ekos::Success)
        return;

    QJsonObject polarState =
    {
        {"enabled", enabled}
    };

    sendResponse(commands[NEW_POLAR_STATE], polarState);
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
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

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
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

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Message::sendSchedulerJobs()
{
    QJsonObject jobs =
    {
        {"jobs", m_Manager->schedulerModule()->moduleState()->getJSONJobs()}
    };
    sendResponse(commands[SCHEDULER_GET_JOBS], jobs);
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Message::sendSchedulerJobList(QJsonArray jobsList)
{
    QJsonObject jobs =
    {
        {"jobs", jobsList}
    };
    sendResponse(commands[SCHEDULER_GET_JOBS], jobs);
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Message::sendSchedulerStatus(const QJsonObject &status)
{
    if (isConnected() == false)
        return;

    sendResponse(commands[NEW_SCHEDULER_STATE], status);
}

void Message::sendMosaicTiles(const QJsonObject &tiles)
{
    if (isConnected() == false)
        return;

    m_DebouncedSend.start();
    m_DebouncedMap[commands[NEW_MOSAIC_TILES]] = tiles.toVariantMap();
}


///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
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

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Message::setINDIStatus(Ekos::CommunicationStatus status)
{
    QJsonObject connectionState =
    {
        {"status", status},
    };

    sendResponse(commands[NEW_INDI_STATE], connectionState);
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Message::processOptionsCommands(const QString &command, const QJsonObject &payload)
{
    if (command == commands[OPTION_SET])
    {
        const QJsonArray options = payload["options"].toArray();
        for (const auto &oneOption : options)
            Options::self()->setProperty(oneOption[QString("name")].toString().toLatin1(), oneOption[QString("value")].toVariant());

        Options::self()->save();
        emit optionsUpdated();
    }
    else if (command == commands[OPTION_GET])
    {
        const QJsonArray options = payload[QString("options")].toArray();
        QJsonArray result;
        for (const auto &oneOption : options)
        {
            const auto name = oneOption[QString("name")].toString();
            QVariant value = Options::self()->property(name.toLatin1());
            QVariantMap map;
            map["name"] = name;
            map["value"] = value;
            result.append(QJsonObject::fromVariantMap(map));
        }
        sendResponse(commands[OPTION_GET], result);
    }
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Message::processScopeCommands(const QString &command, const QJsonObject &payload)
{
    if (command == commands[ADD_SCOPE])
    {
        KStarsData::Instance()->userdb()->AddScope(payload["model"].toString(), payload["vendor"].toString(),
                payload["type"].toString(), payload["aperture"].toDouble(), payload["focal_length"].toDouble());
    }
    else if (command == commands[UPDATE_SCOPE])
    {
        KStarsData::Instance()->userdb()->AddScope(payload["model"].toString(), payload["vendor"].toString(),
                payload["type"].toString(), payload["aperture"].toDouble(), payload["focal_length"].toDouble(), payload["id"].toString());
    }
    else if (command == commands[DELETE_SCOPE])
    {
        KStarsData::Instance()->userdb()->DeleteEquipment("telescope", payload["id"].toString());
    }

    sendScopes();
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Message::processDSLRCommands(const QString &command, const QJsonObject &payload)
{
    if (command == commands[DSLR_SET_INFO])
    {
        if (m_Manager->captureModule())
            m_Manager->captureModule()->mainCamera()->addDSLRInfo(
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
        KStarsData::Instance()->userdb()->DeleteEquipment("dslrlens", payload["id"].toString());
    }
    else if (command == commands[DSLR_UPDATE_LENS])
    {
        KStarsData::Instance()->userdb()->AddDSLRLens(payload["model"].toString(), payload["vendor"].toString(),
                payload["focal_length"].toDouble(), payload["focal_ratio"].toDouble(), payload["id"].toString());
    }

    sendDSLRLenses();
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Message::processTrainCommands(const QString &command, const QJsonObject &payload)
{
    if (command == commands[TRAIN_GET_PROFILES])
        sendTrainProfiles();
    else if (command == commands[TRAIN_SET])
    {
        auto module = payload["module"].toString();
        auto name = payload["name"].toString();

        if (module == "capture")
        {
            if (m_Manager->captureModule())
                m_Manager->captureModule()->setOpticalTrain(name);
        }
        else if (module == "focus")
        {
            if (m_Manager->focusModule())
                m_Manager->focusModule()->mainFocuser()->setOpticalTrain(name);
        }
        else if (module == "guide")
        {
            if (m_Manager->guideModule())
                m_Manager->guideModule()->setOpticalTrain(name);
        }
        else if (module == "align")
        {
            if (m_Manager->alignModule())
                m_Manager->alignModule()->setOpticalTrain(name);
        }
        else if (module == "mount")
        {
            if (m_Manager->mountModule())
                m_Manager->mountModule()->setOpticalTrain(name);
        }
        else if (module == "darklibrary")
        {
            Ekos::DarkLibrary::Instance()->setOpticalTrain(name);
        }
    }
    else if (command == commands[TRAIN_ADD])
    {
        Ekos::OpticalTrainManager::Instance()->addOpticalTrain(payload);
    }
    else if (command == commands[TRAIN_UPDATE])
    {
        Ekos::OpticalTrainManager::Instance()->setOpticalTrain(payload);
    }
    else if (command == commands[TRAIN_DELETE])
    {
        Ekos::OpticalTrainManager::Instance()->removeOpticalTrain(payload["name"].toString());
    }
    else if (command == commands[TRAIN_RESET])
    {
        Ekos::OpticalTrainManager::Instance()->reset();
    }
    else if (command == commands[TRAIN_ACCEPT])
    {
        requestOpticalTrains(false);
        Ekos::OpticalTrainManager::Instance()->accept();
    }

}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Message::processFilterManagerCommands(const QString &command, const QJsonObject &payload)
{
    QSharedPointer<Ekos::FilterManager> manager;
    if (m_Manager->captureModule())
        manager = m_Manager->captureModule()->mainCamera()->filterManager();

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

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Message::processDarkLibraryCommands(const QString &command, const QJsonObject &payload)
{
    if (command == commands[DARK_LIBRARY_START])
        Ekos::DarkLibrary::Instance()->start();
    else if(command == commands[DARK_LIBRARY_SET_ALL_SETTINGS])
    {
        auto settings = payload.toVariantMap();
        Ekos::DarkLibrary::Instance()->setAllSettings(settings);
        KSUtils::setGlobalSettings(settings);
    }
    else if(command == commands[DARK_LIBRARY_GET_ALL_SETTINGS])
        sendDarkLibrarySettings(Ekos::DarkLibrary::Instance()->getAllSettings());
    else if(command == commands[DARK_LIBRARY_GET_DEFECT_SETTINGS])
        sendResponse(commands[DARK_LIBRARY_GET_DEFECT_SETTINGS], Ekos::DarkLibrary::Instance()->getDefectSettings());
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

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Message::processDeviceCommands(const QString &command, const QJsonObject &payload)
{
    QString device = payload["device"].toString();

    // In case we want to UNSUBSCRIBE from all at once
    if (device.isEmpty() && command == commands[DEVICE_PROPERTY_UNSUBSCRIBE])
    {
        m_PropertySubscriptions.clear();
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
            sendResponse(commands[DEVICE_PROPERTY_GET], propObject);
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
            if (oneDevice->getJSONProperty(oneProp.getName(), singleProp, payload["compact"].toBool(false)))
                properties.append(singleProp);
        }

        QJsonObject response =
        {
            {"device", device},
            {"properties", properties}
        };

        sendResponse(commands[DEVICE_GET], response);
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
                if (indiGroups.contains(oneProp.getGroupName()))
                    props.insert(oneProp.getName());
            }
        }
        // Otherwise, subscribe to ALL property in this device
        else
        {
            for (auto &oneProp : *oneDevice->getProperties())
                props.insert(oneProp.getName());
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
                if (indiGroups.contains(oneProp.getGroupName()))
                    props.remove(oneProp.getName());
            }
        }
        // Otherwise, subscribe to ALL property in this device
        else
        {
            for (auto &oneProp : *oneDevice->getProperties())
                props.remove(oneProp.getName());
        }

        m_PropertySubscriptions[device] = props;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
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
    else if (command == commands[ASTRO_GET_NAMES])
    {
        auto composite = KStarsData::Instance()->skyComposite();
        QStringList all;
        QVector<QPair<QString, const SkyObject * >> allObjects;
        CatalogsDB::CatalogObjectList dsoObjects;

        allObjects.append(composite->objectLists(SkyObject::STAR));
        allObjects.append(composite->objectLists(SkyObject::CATALOG_STAR));
        allObjects.append(composite->objectLists(SkyObject::PLANET));
        allObjects.append(composite->objectLists(SkyObject::MOON));
        allObjects.append(composite->objectLists(SkyObject::COMET));
        allObjects.append(composite->objectLists(SkyObject::ASTEROID));
        allObjects.append(composite->objectLists(SkyObject::SUPERNOVA));
        allObjects.append(composite->objectLists(SkyObject::SATELLITE));
        dsoObjects = m_DSOManager.get_objects_all();

        for (auto &oneObject : allObjects)
            all << oneObject.second->name() << oneObject.second->longname().split(", ");

        for (auto &oneObject : dsoObjects)
            all << oneObject.name() << oneObject.longname().split(", ");

        all.removeDuplicates();
        all.sort(Qt::CaseInsensitive);
        sendResponse(commands[ASTRO_GET_NAMES], QJsonArray::fromStringList(all));
    }
    else if (command == commands[ASTRO_GET_DESIGNATIONS])
    {
        QJsonArray designations;

        for (auto &oneObject : m_DSOManager.get_objects_all())
        {
            QJsonObject oneDesignation =
            {
                {"primary", oneObject.name()},
                {"designations", QJsonArray::fromStringList(oneObject.longname().split(", "))}
            };

            designations.append(oneDesignation);
        }

        sendResponse(commands[ASTRO_GET_DESIGNATIONS], designations);
    }
    else if (command == commands[ASTRO_GET_LOCATION])
    {
        auto geo = KStarsData::Instance()->geo();
        QJsonObject location =
        {
            {"name", geo->name()},
            {"longitude", geo->lng()->Degrees()},
            {"latitude", geo->lat()->Degrees()},
            {"elevation", geo->elevation()},
            {"tz", geo->TZ()},
            {"tz0", geo->TZ0()}
        };

        sendResponse(commands[ASTRO_GET_LOCATION], location);
    }
    // Get a list of object based on criteria
    else if (command == commands[ASTRO_SEARCH_OBJECTS])
    {
        // Set time if required only if Ekos profile is not running.
        if (payload.contains("jd") && m_Manager && m_Manager->getEkosStartingStatus() == Ekos::Idle)
        {
            auto jd = KStarsDateTime(payload["jd"].toDouble());
            KStarsData::Instance()->clock()->setManualMode(false);
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

        QVector<QPair<QString, const SkyObject * >> allObjects;
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

        QMutableVectorIterator<QPair<QString, const SkyObject * >> objectIterator(allObjects);

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
        bool exact = payload["exact"].toBool(false);
        QJsonObject info;
        SkyObject *oneObject = KStarsData::Instance()->skyComposite()->findByName(name, exact);
        if(oneObject)
        {
            info =
            {
                {"name", exact ? name : oneObject->name()},
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
        // Set time if required only if Ekos profile is not running.
        if (payload.contains("jd") && m_Manager && m_Manager->getEkosStartingStatus() == Ekos::Idle)
        {
            auto jd = KStarsDateTime(payload["jd"].toDouble());
            KStarsData::Instance()->clock()->setManualMode(false);
            KStarsData::Instance()->clock()->setUTC(jd);
        }

        // Object Names
        bool exact = payload["exact"].toBool(false);
        QVariantList objectNames = payload["names"].toArray().toVariantList();
        QJsonArray objectsArray;

        for (auto &oneName : objectNames)
        {
            const QString name = oneName.toString();
            SkyObject *oneObject = KStarsData::Instance()->skyComposite()->findByName(name, exact);
            if (oneObject)
            {
                QJsonObject info =
                {
                    {"name", exact ? name : oneObject->name()},
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
        // Set time if required only if Ekos profile is not running.
        if (payload.contains("jd") && m_Manager && m_Manager->getEkosStartingStatus() == Ekos::Idle)
        {
            auto jd = KStarsDateTime(payload["jd"].toDouble());
            KStarsData::Instance()->clock()->setManualMode(false);
            KStarsData::Instance()->clock()->setUTC(jd);
        }

        // Object Names
        QVariantList objectNames = payload["names"].toArray().toVariantList();
        QJsonArray objectsArray;

        bool exact = payload["exact"].toBool(false);
        // Data instance
        auto *data = KStarsData::Instance();
        // Geo Location
        auto *geo = KStarsData::Instance()->geo();
        // UT
        auto ut = data->ut();

        for (auto &oneName : objectNames)
        {
            const QString name = oneName.toString();
            SkyObject *oneObject = data->skyComposite()->findByName(name, exact);
            if (oneObject)
            {
                oneObject->EquatorialToHorizontal(data->lst(), geo->lat());
                dms ha(data->lst()->Degrees() - oneObject->ra().Degrees());
                QJsonObject info =
                {
                    {"name", exact ? name : oneObject->name()},
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
        // Set time if required only if Ekos profile is not running.
        if (payload.contains("jd") && m_Manager && m_Manager->getEkosStartingStatus() == Ekos::Idle)
        {
            auto jd = KStarsDateTime(payload["jd"].toDouble());
            KStarsData::Instance()->clock()->setManualMode(false);
            KStarsData::Instance()->clock()->setUTC(jd);
        }

        // Object Names
        QVariantList objectNames = payload["names"].toArray().toVariantList();
        QJsonArray objectsArray;

        bool exact = payload["exact"].toBool(false);
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
            SkyObject *oneObject = data->skyComposite()->findByName(name, exact);
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

                info["name"] = exact ? name : oneObject->name();
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

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Message::processFileCommands(const QString &command, const QJsonObject &payload)
{
    if (command == commands[FILE_DEFAULT_PATH])
    {
        sendResponse(commands[FILE_DEFAULT_PATH],
                     KSPaths::writableLocation(static_cast<QStandardPaths::StandardLocation>(payload["type"].toInt())));
    }
    else if (command == commands[FILE_DIRECTORY_OPERATION])
    {
        auto path = payload["path"].toString();
        auto operation = payload["operation"].toString();

        if (operation == "create")
        {
            QJsonObject info =
            {
                {"result", QDir().mkpath(path)},
                {"operation", operation}
            };

            sendResponse(commands[FILE_DIRECTORY_OPERATION], info);
        }
        else if (operation == "remove")
        {
            QJsonObject info =
            {
                {"result", QDir(path).removeRecursively()},
                {"operation", operation}
            };

            sendResponse(commands[FILE_DIRECTORY_OPERATION], info);
        }
        else if (operation == "list")
        {
            auto namedFilters = payload["namedFilters"].toString("*").split(",");
            auto filters = static_cast<QDir::Filters>(payload["filters"].toInt(QDir::NoFilter));
            auto sort = static_cast<QDir::SortFlags>(payload["sort"].toInt(QDir::NoSort));
            auto list = QDir(path).entryInfoList(namedFilters, filters, sort);
            auto entries = QJsonArray();
            for (auto &oneEntry : list)
            {
                QJsonObject info =
                {
                    {"name", oneEntry.fileName()},
                    {"path", oneEntry.absolutePath()},
                    {"size", oneEntry.size()},
                    {"isFile", oneEntry.isFile()},
                    {"creation", oneEntry.birthTime().toSecsSinceEpoch()},
                    {"modified", oneEntry.lastModified().toSecsSinceEpoch()}
                };

                entries.push_back(info);
            }

            QJsonObject info =
            {
                {"result", !entries.empty()},
                {"operation", operation},
                {"payload", entries}
            };

            sendResponse(commands[FILE_DIRECTORY_OPERATION], info);
        }
        else if (operation == "exists")
        {
            QJsonObject info =
            {
                {"result", QDir(path).exists()},
                {"operation", operation}
            };

            sendResponse(commands[FILE_DIRECTORY_OPERATION], info);
        }
    }
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
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

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Message::requestDSLRInfo(const QString &cameraName)
{
    sendResponse(commands[DSLR_GET_INFO], cameraName);
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Message::requestPortSelection(bool show)
{
    sendResponse(commands[GET_PROFILE_PORT_SELECTION], show);
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Message::sendDialog(const QJsonObject &message)
{
    sendResponse(commands[DIALOG_GET_INFO], message);
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Message::sendResponse(const QString &command, const QJsonObject &payload)
{
    for (auto &nodeManager : m_NodeManagers)
    {
        nodeManager->message()->sendResponse(command, payload);
    }
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Message::sendResponse(const QString &command, const QJsonArray &payload)
{
    for (auto &nodeManager : m_NodeManagers)
    {
        nodeManager->message()->sendResponse(command, payload);
    }
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Message::sendResponse(const QString &command, const QString &payload)
{
    for (auto &nodeManager : m_NodeManagers)
    {
        nodeManager->message()->sendResponse(command, payload);
    }
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Message::sendResponse(const QString &command, bool payload)
{
    for (auto &nodeManager : m_NodeManagers)
    {
        nodeManager->message()->sendResponse(command, payload);
    }
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Message::autofocusAborted()
{
    QJsonObject cStatus =
    {
        {"status", "Aborted"}
    };
    sendResponse(commands[NEW_FOCUS_STATE], cStatus);
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Message::updateMountStatus(const QJsonObject &status, bool throttle)
{
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

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Message::updateCaptureStatus(const QJsonObject &status)
{
    sendResponse(commands[NEW_CAPTURE_STATE], status);
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Message::updateFocusStatus(const QJsonObject &status)
{
    sendResponse(commands[NEW_FOCUS_STATE], status);
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Message::updateGuideStatus(const QJsonObject &status)
{
    sendResponse(commands[NEW_GUIDE_STATE], status);
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Message::updateDomeStatus(const QJsonObject &status)
{
    sendResponse(commands[NEW_DOME_STATE], status);
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Message::updateCapStatus(const QJsonObject &status)
{
    sendResponse(commands[NEW_CAP_STATE], status);
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Message::updateAlignStatus(const QJsonObject &status)
{
    sendResponse(commands[NEW_ALIGN_STATE], status);
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Message::sendConnection()
{
    QJsonObject connectionState =
    {
        {"connected", true},
        {"online", m_Manager->getEkosStartingStatus() == Ekos::Success}
    };

    sendResponse(commands[NEW_CONNECTION_STATE], connectionState);
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Message::sendStates()
{
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
        QJsonObject focusState = {{ "status", getFocusStatusString(m_Manager->focusModule()->mainFocuser()->status(), false)}};
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
        }
    }
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Message::sendEvent(const QString &message, KSNotification::EventSource source, KSNotification::EventType event)
{
    if (Options::ekosLiveNotifications() == false)
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

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Message::sendManualRotatorStatus(double currentPA, double targetPA, double threshold)
{
    QJsonObject request = {{ "currentPA", currentPA}, {"targetPA", targetPA}, {"threshold", threshold}};
    sendResponse(commands[ALIGN_MANUAL_ROTATOR_STATUS], request);
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Message::setBoundingRect(QRect rect, QSize view, double currentZoom)
{
    m_BoundingRect = rect;
    m_ViewSize = view;
    m_CurrentZoom = currentZoom;
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Message::processDialogResponse(const QJsonObject &payload)
{
    KSMessageBox::Instance()->selectResponse(payload["button"].toString());
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Message::processNewProperty(INDI::Property prop)
{
    // Do not send new properties until all properties settle down
    // then send any properties that appears afterwards since the initial bunch
    // would cause a heavy message congestion.
    if (m_Manager->settleStatus() != Ekos::CommunicationStatus::Success)
        return;

    QJsonObject propObject;
    ISD::propertyToJson(prop, propObject, false);
    sendResponse(commands[DEVICE_PROPERTY_ADD], propObject);
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Message::processDeleteProperty(INDI::Property prop)
{
    QJsonObject payload =
    {
        {"device", prop.getDeviceName()},
        {"name", prop.getName()}
    };

    sendResponse(commands[DEVICE_PROPERTY_REMOVE], payload);
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Message::processMessage(const QSharedPointer<ISD::GenericDevice> &device, int id)
{
    if (Options::ekosLiveNotifications() == false)
        return;

    auto message = QString::fromStdString(device->getBaseDevice().messageQueue(id));
    QJsonObject payload =
    {
        {"device", device->getDeviceName()},
        {"message", message}
    };

    sendResponse(commands[DEVICE_MESSAGE], payload);
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Message::processUpdateProperty(INDI::Property prop)
{
    if (m_PropertySubscriptions.contains(prop.getDeviceName()))
    {
        QSet<QString> subProps = m_PropertySubscriptions[prop.getDeviceName()];
        if (subProps.contains(prop.getName()))
        {
            PendingProperty pending{prop.getDeviceName(), prop.getName()};
            m_PendingProperties.remove(pending);
            m_PendingProperties.insert(pending);
        }
    }
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Message::setPendingPropertiesEnabled(bool enabled)
{
    if (enabled)
        m_PendingPropertiesTimer.start();
    else
    {
        m_PendingProperties.clear();
        // Must stop timer and sleep for 500ms to enable any pending properties to finish
        if (m_PendingPropertiesTimer.isActive())
        {
            m_PendingPropertiesTimer.stop();
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    }
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Message::sendPendingProperties()
{
    // Group properties by device to minimize device lookups
    QMap<QString, QSet<QString >> deviceProperties;

    // First pass - group by device
    for (const auto &pending : m_PendingProperties)
        deviceProperties[pending.device].insert(pending.name);

    // Second pass - process each device's properties
    for (auto it = deviceProperties.constBegin(); it != deviceProperties.constEnd(); ++it)
    {
        QSharedPointer<ISD::GenericDevice> device;
        // Only lookup device once for all its properties
        if (INDIListener::findDevice(it.key(), device))
        {
            // Process all properties for this device
            for (const auto &propName : it.value())
            {
                auto prop = device->getProperty(propName);
                if (prop)
                {
                    QJsonObject propObject;
                    ISD::propertyToJson(prop, propObject);
                    sendResponse(commands[DEVICE_PROPERTY_GET], propObject);
                }
            }
        }
    }

    // Clear all pending properties
    m_PendingProperties.clear();
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Message::sendModuleState(const QString &name)
{
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
        QJsonObject focusState = {{ "status", getFocusStatusString(m_Manager->focusModule()->mainFocuser()->status(), false)}};
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
        }
    }
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
QObject * Message::findObject(const QString &name)
{
    QObject *object {nullptr};
    // Check for manager itself
    if (name == "Manager")
        return m_Manager;
    // Try Manager first
    object = m_Manager->findChild<QObject *>(name);
    if (object)
        return object;
    // Then INDI Listener
    object = INDIListener::Instance()->findChild<QObject *>(name);
    if (object)
        return object;
    // FITS Viewer. Search for any matching imageData
    // TODO Migrate to DBus
    for (auto &viewer : KStars::Instance()->getFITSViewers())
    {
        for (auto &tab : viewer->tabs())
        {
            if (tab->getView()->objectName() == name)
                return tab->getView().get();
        }
    }

    // Finally KStars
    // N.B. This does not include indepdent objects with their parent set to null (e.g. FITSViewer)
    object = KStars::Instance()->findChild<QObject *>(name);
    return object;
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
bool Message::parseArgument(QMetaType::Type type, const QVariant &arg, QMetaMethodArgument &genericArg, SimpleTypes &types)
#else
bool Message::parseArgument(QVariant::Type type, const QVariant &arg, QGenericArgument &genericArg, SimpleTypes &types)
#endif
{
    //QMetaMethodArgument genericArgument;

    switch (type)
    {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        case QMetaType::Int:
#else
        case QVariant::Int:
#endif
            types.number_integer = arg.toInt();
            genericArg = Q_ARG(int, types.number_integer);
            return true;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        case QMetaType::UInt:
#else
        case QVariant::UInt:
#endif
            types.number_unsigned_integer = arg.toUInt();
            genericArg = Q_ARG(uint, types.number_unsigned_integer);
            return true;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        case QMetaType::LongLong:
#else
        case QVariant::LongLong:
#endif
            types.number_integer = arg.toLongLong();
            genericArg = Q_ARG(int, types.number_integer);
            return true;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        case QMetaType::ULongLong:
#else
        case QVariant::ULongLong:
#endif
            types.number_unsigned_integer = arg.toULongLong();
            genericArg = Q_ARG(uint, types.number_unsigned_integer);
            return true;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        case QMetaType::Double:
#else
        case QVariant::Double:
#endif
            types.number_double = arg.toDouble();
            genericArg = Q_ARG(double, types.number_double);
            return true;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        case QMetaType::Bool:
#else
        case QVariant::Bool:
#endif
            types.boolean = arg.toBool();
            genericArg = Q_ARG(bool, types.boolean);
            return true;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        case QMetaType::QString:
#else
        case QVariant::String:
#endif
            types.text = arg.toString();
            genericArg = Q_ARG(QString, types.text);
            return true;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        case QMetaType::QUrl:
#else
        case QVariant::Url:
#endif
            types.url = arg.toUrl();
            genericArg = Q_ARG(QUrl, types.url);
            return true;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        case QMetaType::QSize:
#else
        case QVariant::Size:
#endif
        {
            QJsonObject obj = arg.toJsonObject();
            types.size = QSize(obj["width"].toInt(), obj["height"].toInt());
        }
        genericArg = Q_ARG(QSize, types.size);
        return true;

        default:
            break;
    }

    return false;
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Message::invokeMethod(QObject *context, const QJsonObject &payload)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 2, 0)
    QList<QMetaMethodArgument> argsList;
#else
    QList<QGenericArgument> argsList;
#endif

    QList<SimpleTypes> typesList;

    auto name = payload["name"].toString().toLatin1();

    if (payload.contains("args"))
    {
        QJsonArray args = payload["args"].toArray();

        for (auto oneArg : args)
        {
            auto argObject = oneArg.toObject();
#if QT_VERSION >= QT_VERSION_CHECK(6, 2, 0)
            QMetaMethodArgument genericArgument;
#else
            QGenericArgument genericArgument;
#endif
            SimpleTypes genericType;
            argsList.append(genericArgument);
            typesList.append(genericType);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
            if (parseArgument(static_cast<QMetaType::Type>(argObject["type"].toInt()), argObject["value"].toVariant(), argsList.back(),
                              typesList.last()) == false)
#else
            if (parseArgument(static_cast<QVariant::Type>(argObject["type"].toInt()), argObject["value"].toVariant(), argsList.back(),
                              typesList.last()) == false)
#endif
            {
                argsList.pop_back();
                typesList.pop_back();
            }
        }

        switch (argsList.size())
        {
            case 1:
                QMetaObject::invokeMethod(context, name, argsList[0]);
                break;
            case 2:
                QMetaObject::invokeMethod(context, name, argsList[0], argsList[1]);
                break;
            case 3:
                QMetaObject::invokeMethod(context, name, argsList[0], argsList[1], argsList[2]);
                break;
            case 4:
                QMetaObject::invokeMethod(context, name, argsList[0], argsList[1], argsList[2], argsList[3]);
                break;
            default:
                break;
        }
    }
    else
    {
        QMetaObject::invokeMethod(context, name);
    }
}

}
