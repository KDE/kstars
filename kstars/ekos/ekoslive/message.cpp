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
#include "ekos/capture/cameraprocess.h"
#include "ekos/focus/focusmodule.h"
#include "ekos/auxiliary/buildfilteroffsets.h"
#include "ekos/guide/guide.h"
#include "ekos/guide/aiguidewizard.h"
#include "ekos/guide/internalguide/internalguider.h"
#include "ekos/guide/internalguide/gmath.h"
#include "ekos/guide/internalguide/mount_guider_factory.h"
#include "ekos/mount/mount.h"
#include "ekos/scheduler/scheduler.h"
#include "ekos/scheduler/schedulermodulestate.h"
#include "kstars.h"
#include "kspaths.h"
#include "kstarsdata.h"
#include "ekos_debug.h"
#include "ksalmanac.h"
#include "skymapcomposite.h"
#include "skycomponents/artificialhorizoncomponent.h"
#include "linelist.h"
#include "catalogobject.h"
#include "fitsviewer/fitsviewer.h"
#include "fitsviewer/fitstab.h"
#include "fitsviewer/pipeline/masterbuilder.h"
#include "fitsviewer/pipeline/directoryinspector.h"
#include "fitsviewer/pipeline/previewrenderer.h"
#include "ekos/auxiliary/darklibrary.h"
#include "ekos/align/mountmodel.h"
#include "skymap.h"
#include "Options.h"
#include "auxiliary/ksutils.h"
#include "version.h"

#include <QtConcurrentRun>
#include <QFutureWatcher>

#include <KActionCollection>
#include <basedevice.h>
#include <QUuid>
#include <QSet>
#include <functional>
#include <thread>

namespace
{
// Rough multiple of one full-frame image that a given post-process step keeps
// alive at peak: channel splits, blur pyramids, chroma planes, per-pixel
// calibration buffers, registered warps and the merged result. Used to refuse a
// step the machine cannot fit rather than letting the OOM killer take KStars down
// on a 4-8 GB StellarMate controller (getAvailableRAM() reports MemAvailable, so
// it already counts reclaimable page cache). 0 = not checked (cheap steps).
int postProcessMemoryWeight(const QString &command)
{
    // This helper sits in the file-scope anonymous namespace, while the command
    // table/enum live in EkosLive — pull them in rather than fully qualifying
    // every comparison.
    using namespace EkosLive;
    if (command == commands[POSTPROCESS_APPLY_DENOISE])           return 8;
    if (command == commands[POSTPROCESS_APPLY_COLOR_CALIBRATION]) return 6;
    if (command == commands[POSTPROCESS_APPLY_BGE])               return 5;
    if (command == commands[POSTPROCESS_APPLY_AUTOSTRETCH]
            || command == commands[POSTPROCESS_APPLY_CURVE]
            || command == commands[POSTPROCESS_APPLY_CURVE_PER_CHANNEL]
            || command == commands[POSTPROCESS_APPLY_SATURATION]
            || command == commands[POSTPROCESS_APPLY_CONTRAST])   return 3;
    if (command == commands[POSTPROCESS_CROP])                    return 2;
    return 0;
}

// Image stats for a postprocess_* preview's metadata header — the same fields
// Media::upload() already sends for a live capture (resolution/size/channels/bpp/
// mean/median/stddev/min/max/hasWCS), minus the capture-specific ones (exposure,
// gain, focal length, ...) that don't apply to a stacked/blended composite.
QJsonObject buildPreviewMetadata(const QSharedPointer<FITSData> &data)
{
    if (!data)
        return {};

    return QJsonObject
    {
        {"resolution", QString("%1x%2").arg(data->width()).arg(data->height())},
        {"size", static_cast<qint64>(data->size())},
        {"channels", data->channels()},
        {"bpp", static_cast<int>(data->bpp())},
        {"mean", data->getAverageMean()},
        {"median", data->getAverageMedian()},
        {"stddev", data->getAverageStdDev()},
        {"min", data->getMin()},
        {"max", data->getMax()},
        {"hasWCS", data->hasWCS()}
    };
}

// Outcome of a crop/apply_*/save operation run on a worker thread (see
// Message::processPostProcessCommands) — mirrors what each command's inline handler
// used to build directly into the JSON response.
struct PostProcessOpResult
{
    bool ok = false;
    QString error;
    QJsonObject extra;
};
}

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
    Q_EMIT connected();
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
        Q_EMIT disconnected();
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
        Q_EMIT globalLogoutTriggered(node->url());
        return;
    }
    else if (command == commands[SET_CLIENT_STATE])
    {
        // If client is connected, make sure clock is ticking
        if (payload["state"].toBool(false))
        {
            qCInfo(KSTARS_EKOS) << "EkosLive client is connected:" << node->url().toDisplayString();

            // Need to update client state in the matching node manager.
            for (auto &nodeManager : m_NodeManagers)
            {
                if (nodeManager->message() == node)
                {
                    node->setClientState(true);
                    nodeManager->media()->setClientState(true);
                }
            }

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
            qCInfo(KSTARS_EKOS) << "EkosLive client is disconnected:" << node->url().toDisplayString();

            // Need to update client state in the matching node manager.
            for (auto &nodeManager : m_NodeManagers)
            {
                if (nodeManager->message() == node)
                {
                    node->setClientState(false);
                    nodeManager->media()->setClientState(false);
                }
            }
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
    else if (command.startsWith("artificial_horizon_"))
        processArtificialHorizonCommands(command, payload);
    else if (command.startsWith("postprocess_"))
    {
        // Post-processing operates on files already on disk and has no
        // dependency on an active Ekos/INDI session, so it must not be
        // gated behind m_Manager->getEkosStartingStatus() below.
        processPostProcessCommands(command, payload);
        return;
    }

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
    else if (command.startsWith("mount_model_"))
        processMountModelCommands(command, payload);
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
    else if (command.startsWith("livestacker_"))
        processLiveStackerCommands(command, payload);
    else if (command.startsWith("filter_offset_"))
        processFilterOffsetCommands(command, payload);
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
    {
        QJsonObject trainObject = QJsonObject::fromVariantMap(train);
        // Auto-detected mount class (WORM_GEAR/HARMONIC_DRIVE/DIRECT_DRIVE/NOT_FOUND) from
        // mount_types.json, keyed by the train's mount device name. Computed fresh on every
        // request so it stays correct as trains are added/edited/reassigned.
        trainObject["mount_type"] = MountGuiderFactory::detectMountType(train["mount"].toString());
        trains.append(trainObject);
    }

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
        {
            QFile file(payload["filepath"].toString());
            if (file.open(QIODevice::ReadOnly | QIODevice::Text))
            {
                sendResponse(commands[CAPTURE_SAVE_SEQUENCE_FILE], QString::fromUtf8(file.readAll()));
                file.close();
            }
        }
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
        const bool isJ2000 = payload["isJ2000"].toBool();
        auto ra = dms::fromString(payload["ra"].toString(), false);
        auto de = dms::fromString(payload["de"].toString(), true);
        if (isJ2000)
        {
            SkyPoint coords(ra, de);
            coords.apparentCoord(static_cast<long double>(J2000), KStarsData::Instance()->ut().djd());
            mount->sync(coords.ra().Hours(), coords.dec().Degrees());
        }
        else
            mount->sync(ra.Hours(), de.Degrees());
    }
    else if (command == commands[MOUNT_SYNC_TARGET])
    {
        mount->syncTarget(payload["target"].toString());
    }
    else if (command == commands[MOUNT_GOTO_RADE])
    {
        const bool isJ2000 = payload["isJ2000"].toBool();
        auto ra = dms::fromString(payload["ra"].toString(), false);
        auto de = dms::fromString(payload["de"].toString(), true);
        if (isJ2000)
        {
            SkyPoint coords(ra, de);
            coords.apparentCoord(static_cast<long double>(J2000), KStarsData::Instance()->ut().djd());
            mount->slew(coords.ra().Hours(), coords.dec().Degrees());
        }
        else
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
            if (file.open())
            {
                file.write(QByteArray::fromBase64(payload["data"].toString().toLatin1()));
                file.close();
                align->loadAndSlew(file.fileName());
            }
            else
            {
                qCWarning(KSTARS_EKOS) << "ALIGN_LOAD_AND_SLEW: failed to open temporary file" << filename;
            }
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
void Message::processMountModelCommands(const QString &command, const QJsonObject &payload)
{
    Ekos::Align *align = m_Manager->alignModule();

    if (align == nullptr)
    {
        qCWarning(KSTARS_EKOS) << "Ignoring command" << command << "as align module is not available";
        return;
    }

    align->ensureMountModelCreated();

    Ekos::MountModel *mountModel = align->mountModel();

    if (command == commands[MOUNT_MODEL_GET_ALL_SETTINGS])
        sendMountModelSettings(mountModel->getAllSettings());
    else if (command == commands[MOUNT_MODEL_SET_ALL_SETTINGS])
    {
        auto settings = payload.toVariantMap();
        mountModel->setAllSettings(settings);
    }
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Message::sendMountModelSettings(const QVariantMap &settings)
{
    m_DebouncedSend.start();
    m_DebouncedMap[commands[MOUNT_MODEL_GET_ALL_SETTINGS]] = settings;
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
    }
    else if (command == commands[SCHEDULER_SAVE_FILE])
    {
        if (scheduler->saveFile(QUrl::fromLocalFile(payload["filepath"].toString())))
        {
            QFile file(payload["filepath"].toString());
            if (file.open(QIODevice::ReadOnly | QIODevice::Text))
            {
                sendResponse(commands[SCHEDULER_SAVE_FILE], QString::fromUtf8(file.readAll()));
                file.close();
            }
        }
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
        Q_EMIT resetPolarView();
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
        Q_EMIT optionsUpdated();
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
        const KStarsDateTime midnight  = KStarsDateTime(localTime.date(), QTime(0, 0), QTimeZone(QTimeZone::systemTimeZoneId()));

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
            applySimulationTime(KStarsDateTime(payload["jd"].toDouble()));

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
                // SkyObject::TYPE code (e.g. GALAXY = 8, COMET = 9) so the app can
                // label the object's category (Galaxies, Comets, ...) from the
                // object info response without a second lookup.
                {"type", oneObject->type()},
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
            applySimulationTime(KStarsDateTime(payload["jd"].toDouble()));

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
                    // SkyObject::TYPE code (e.g. GALAXY = 8, COMET = 9) so the app can
                    // label the object's category (Galaxies, Comets, ...) from the
                    // object info response without a second lookup.
                    {"type", oneObject->type()},
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
            applySimulationTime(KStarsDateTime(payload["jd"].toDouble()));

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
            applySimulationTime(KStarsDateTime(payload["jd"].toDouble()));

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
        KStarsDateTime ut = geo->LTtoUT(KStarsDateTime(midnight));

        int daysToProcess = payload["days"].toInt(0);

        for (auto &oneName : objectNames)
        {
            const QString name = oneName.toString();
            SkyObject *oneObject = data->skyComposite()->findByName(name, exact);
            if (oneObject)
            {
                // Get today's data using the helper function
                QJsonObject todayInfo = getRiseSetAltitudeDataForDay(oneObject, ut, geo, data->lt().date());
                todayInfo["name"] = exact ? name : oneObject->name();

                QJsonArray futureDaysArray;
                for (int i = 1; i <= daysToProcess; ++i)
                {
                    KStarsDateTime futureUt = ut.addDays(i);
                    QDate futureDate = data->lt().date().addDays(i);
                    futureDaysArray.append(getRiseSetAltitudeDataForDay(oneObject, futureUt, geo, futureDate));
                }

                if (!futureDaysArray.isEmpty())
                {
                    todayInfo["days"] = futureDaysArray;
                }

                objectsArray.append(todayInfo);
            }
        }

        sendResponse(commands[ASTRO_GET_OBJECTS_RISESET], objectsArray);
    }
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Message::applySimulationTime(const KStarsDateTime &jd)
{
    auto *clock = KStarsData::Instance()->clock();
    // setUTC() is a no-op while the clock runs in realtime mode, which Ekos
    // enables on profile start. Switch to simulation mode first, otherwise the
    // requested JD is silently ignored.
    if (clock->isRealTime())
        clock->setRealTime(false);
    clock->setManualMode(false);
    clock->setUTC(jd);
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
    const KStarsDateTime midnight  = KStarsDateTime(localTime.date(), QTime(0, 0), QTimeZone(QTimeZone::systemTimeZoneId()));
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
QJsonObject Message::getRiseSetAltitudeDataForDay(SkyObject *oneObject, const KStarsDateTime &ut, GeoLocation *geo,
        const QDate &date)
{
    QJsonObject info;
    // Prepare time/position variables
    // true = use rise time
    QTime riseTime = oneObject->riseSetTime(ut, geo, true);

    // If transit time is before rise time, use transit time for tomorrow
    QTime transitTime = oneObject->transitTime(ut, geo);
    if (transitTime < riseTime)
        transitTime = oneObject->transitTime(ut.addDays(1), geo);

    // If set time is before rise time, use set time for tomorrow
    // false = use set time
    QTime setTime = oneObject->riseSetTime(ut, geo, false);
    // false = use set time
    if (setTime < riseTime)
        setTime = oneObject->riseSetTime(ut.addDays(1), geo, false);

    info["date"] = date.toString("yyyy-MM-dd");
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
    int DayOffset = 0;
    if (ut.time().hour() > 12)
        DayOffset = 1;

    for (double h = -12.0; h <= 12.0; h += 0.5)
    {
        double hour = h + (24.0 * DayOffset);
        KStarsDateTime offset = ut.addSecs(hour * 3600.0);
        CachingDms LST = geo->GSTtoLST(offset.gst());
        oneObject->EquatorialToHorizontal(&LST, geo->lat());
        altitudes.append(oneObject->alt().Degrees());
    }

    info["altitudes"] = altitudes;
    return info;
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
void Message::sendEvent(const QString &command, const QJsonObject &payload)
{
    for (auto &nodeManager : m_NodeManagers)
    {
        nodeManager->message()->sendEvent(command, payload);
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

    sendEvent(commands[NEW_NOTIFICATION], newEvent);
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
void Message::processMessage(const QSharedPointer<ISD::GenericDevice> &device, const QString &message)
{

    if (Options::ekosLiveNotifications() == false)
        return;

    // Message text was already extracted safely in GenericDevice::processMessage.
    // An empty string means the extraction failed (e.g. invalid device state or
    // out-of-range message ID), so there is nothing to forward.
    if (message.isEmpty())
        return;

    // Return if message doesn't contain any log level indicator
    static const QRegularExpression logLevelRegex(QStringLiteral("\\[(INFO|WARNING|ERROR)\\]"));
    if (!logLevelRegex.match(message).hasMatch())
        return;

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

    // Filter Managers
    for (auto &fm : m_Manager->filterManagers())
    {
        if (fm)
        {
            object = fm->findChild<QObject *>(name);
            if (object)
                return object;
        }
    }

    // Finally KStars
    // N.B. This does not include independent objects with their parent set to null (e.g. FITSViewer)
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

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Message::processLiveStackerCommands(const QString &command, const QJsonObject &payload)
{
    if (command == commands[LIVESTACKER_INITIALIZE])
    {
        // Close existing viewer if any
        if (m_LiveStackerViewer)
        {
            m_LiveStackerViewer->close();
            m_LiveStackerViewer = nullptr;
        }

        // Create new FITS Viewer for live stacking
        m_LiveStackerViewer = KStars::Instance()->createFITSViewer();
        if (!m_LiveStackerViewer)
        {
            sendResponse(commands[NEW_LIVESTACKER_STATE],
            QJsonObject{{"state", "error"}, {"message", "Failed to create FITS Viewer"}});
            return;
        }

        // Trigger stack mode (this opens LiveStacker UI)
        m_LiveStackerViewer->stack();

        sendResponse(commands[NEW_LIVESTACKER_STATE], QJsonObject{{"state", "initialized"}});
    }
    else if (command == commands[LIVESTACKER_SET_ALL_SETTINGS])
    {
        // Merge incoming key:value pairs into the existing settings map so that
        // a partial update does not wipe keys not included in this payload.
        for (auto it = payload.constBegin(); it != payload.constEnd(); ++it)
            m_LiveStackerSettings[it.key()] = it.value().toVariant();
        // Return the full (merged) settings so the caller has the complete picture.
        sendResponse(commands[LIVESTACKER_GET_ALL_SETTINGS], QJsonObject::fromVariantMap(m_LiveStackerSettings));
    }
    else if (command == commands[LIVESTACKER_GET_ALL_SETTINGS])
    {
        sendResponse(commands[LIVESTACKER_GET_ALL_SETTINGS], QJsonObject::fromVariantMap(m_LiveStackerSettings));
    }
    else if (command == commands[LIVESTACKER_START])
    {
        if (!m_LiveStackerViewer)
        {
            sendResponse(commands[NEW_LIVESTACKER_STATE],
            QJsonObject{{"state", "error"}, {"message", "LiveStacker not initialized"}});
            return;
        }

        // The FITSView is created synchronously during FITSTab::setupView() / FITSView::initStack(),
        // but the tab is only added to the QTabWidget after the async "noimage.png" load completes.
        // So getCurrentView() (which uses currentIndex()) would return false here. Instead, access
        // the first (and only) tab's view directly since it is always constructed synchronously.
        auto viewerTabs = m_LiveStackerViewer->tabs();
        if (viewerTabs.isEmpty() || viewerTabs.first().isNull())
        {
            sendResponse(commands[NEW_LIVESTACKER_STATE],
            QJsonObject{{"state", "error"}, {"message", "No active view"}});
            return;
        }
        QSharedPointer<FITSView> currentView = viewerTabs.first()->getView();
        if (!currentView)
        {
            sendResponse(commands[NEW_LIVESTACKER_STATE],
            QJsonObject{{"state", "error"}, {"message", "No active view"}});
            return;
        }

        // Get the directory to monitor
        QString directory = m_LiveStackerSettings.value("stackingDirectory").toString();
        if (directory.isEmpty())
        {
            sendResponse(commands[NEW_LIVESTACKER_STATE],
            QJsonObject{{"state", "error"}, {"message", "No stacking directory specified"}});
            return;
        }

        // For active-sequence mode (non-looping), the server passes the base capture directory
        // (fileDirectoryT). However, KStars's PlaceholderPath saves images to a subdirectory
        // structured as <base>/<target>/<type>/<filter>/. Resolve the actual subdirectory from
        // the active job's SJ_Signature so the stacker watches where frames actually land.
        const bool isLooping = m_LiveStackerSettings.value("looping", false).toBool();
        if (!isLooping)
        {
            auto capture = m_Manager->captureModule();
            if (capture)
            {
                auto activeJob = capture->mainCamera()->activeJob();
                if (activeJob)
                {
                    const QString sig = activeJob->getCoreProperty(Ekos::SequenceJob::SJ_Signature).toString();
                    if (!sig.isEmpty())
                    {
                        const QString resolvedDir = QFileInfo(sig).absoluteDir().path();
                        if (!resolvedDir.isEmpty())
                        {
                            qCInfo(KSTARS_EKOS) << "LiveStacker: resolved stacking directory from job signature:"
                                                << directory << "→" << resolvedDir;
                            directory = resolvedDir;
                            // Keep settings in sync so onLiveStackerJobChanged restart uses correct dir
                            m_LiveStackerSettings["stackingDirectory"] = resolvedDir;
                        }
                    }
                    // Seed the job-change tracker so the first arriving frame does not
                    // trigger a spurious restart.
                    m_LiveStackerCurrentTarget = activeJob->getCoreProperty(Ekos::SequenceJob::SJ_TargetName).toString();
                    m_LiveStackerCurrentFilter = activeJob->getCoreProperty(Ekos::SequenceJob::SJ_Filter).toString();
                }
            }
        }

        // Build StackData from settings
        StackData params;
        params.calcSNR = m_LiveStackerSettings.value("calcSNR", true).toBool();
        params.alignMethod = static_cast<StackAlignMethod>(m_LiveStackerSettings.value("alignMethod", 0).toInt());
        params.stackingMethod = static_cast<StackingMethod>(m_LiveStackerSettings.value("stackingMethod", 0).toInt());
        params.downscale = static_cast<StackDownscale>(m_LiveStackerSettings.value("downscale", 0).toInt());
        params.numInMem = m_LiveStackerSettings.value("numInMem", 10).toInt();
        params.weighting = static_cast<StackFrameWeighting>(m_LiveStackerSettings.value("weighting", 0).toInt());
        params.lowSigma = m_LiveStackerSettings.value("lowSigma", 2.0).toDouble();
        params.highSigma = m_LiveStackerSettings.value("highSigma", 3.0).toDouble();

        // Post-processing settings
        params.postProcessing.postProcess = m_LiveStackerSettings.value("postProcess", false).toBool();
        params.postProcessing.sharpenAmt = m_LiveStackerSettings.value("sharpenAmt", 0.0).toDouble();
        params.postProcessing.denoiseAmt = m_LiveStackerSettings.value("denoiseAmt", 0.0).toDouble();
        params.postProcessing.deconvAmt = m_LiveStackerSettings.value("deconvAmt", 0.0).toDouble();
        params.postProcessing.gradientAmt = m_LiveStackerSettings.value("gradientAmt", 0.0).toDouble();

        // Master dark/flat paths
        QString masterDark = m_LiveStackerSettings.value("masterDarkPath").toString();
        QString masterFlat = m_LiveStackerSettings.value("masterFlatPath").toString();

        if (!masterDark.isEmpty())
            params.masterDark = QVector<QString> {masterDark};
        if (!masterFlat.isEmpty())
            params.masterFlat = QVector<QString> {masterFlat};

        // EkosLive integration: output directory where stacked images are saved to disk.
        // This is DIFFERENT from stackingDirectory (input) - the EkosLive server monitors
        // this outputDirectory offline for new stacked frames.
        params.outputDirectory = m_LiveStackerSettings.value("outputDirectory").toString();

        // Start stacking via FITSTab so the GUI is fully updated (directory
        // field, Start/Stop button, counters) before the pipeline begins.
        viewerTabs.first()->startProgrammatically(directory, params);

        // Connect progress signals from the underlying FITSView.
        connect(currentView.get(), &FITSView::stackUpdateStats, this, &Message::sendLiveStackerProgress);
        connect(currentView.get(), &FITSView::resetStack, this, &Message::sendLiveStackerComplete);

        if (m_Manager->captureModule())
        {
            if (isLooping)
            {
                // In looping (framing) mode, JOBTYPE_PREVIEW frames are never saved to disk because
                // KStars forces UPLOAD_CLIENT for preview jobs. Connect to CameraProcess::newImage so
                // every captured frame (full-resolution FITSData) is saved to stackingDirectory where
                // the LiveStacker filesystem watcher picks it up automatically.
                m_LiveStackerLooping = true;
                connect(m_Manager->captureModule()->mainCamera()->process().get(),
                        &Ekos::CameraProcess::newImage,
                        this,
                        &Message::saveLiveStackerFrame,
                        Qt::UniqueConnection);
            }
            else
            {
                // In active-sequence mode, connect to newImage to detect when the target or filter
                // changes (new sequence job) and restart the stacker so it watches the correct
                // subdirectory for the new job. Stacking H_Alpha on OIII or mixing targets would
                // produce scientifically meaningless results.
                connect(m_Manager->captureModule()->mainCamera()->process().get(),
                        &Ekos::CameraProcess::newImage,
                        this,
                        &Message::onLiveStackerJobChanged,
                        Qt::UniqueConnection);
            }
        }

        Q_EMIT liveStackingActiveChanged(true);
        sendResponse(commands[NEW_LIVESTACKER_STATE], QJsonObject{{"state", "started"}});
    }
    else if (command == commands[LIVESTACKER_STOP])
    {
        if (m_Manager->captureModule())
        {
            // Disconnect the looping frame-save slot if it was connected.
            if (m_LiveStackerLooping)
            {
                disconnect(m_Manager->captureModule()->mainCamera()->process().get(),
                           &Ekos::CameraProcess::newImage,
                           this,
                           &Message::saveLiveStackerFrame);
                m_LiveStackerLooping = false;
            }
            else
            {
                // Disconnect the sequence-mode job-change slot.
                disconnect(m_Manager->captureModule()->mainCamera()->process().get(),
                           &Ekos::CameraProcess::newImage,
                           this,
                           &Message::onLiveStackerJobChanged);
            }
        }
        m_LiveStackerCurrentTarget.clear();
        m_LiveStackerCurrentFilter.clear();

        if (m_LiveStackerViewer)
        {
            // Use tabs().first() directly for the same reason as LIVESTACKER_START:
            // the tab may not yet be registered in the QTabWidget index when stop is
            // called quickly after start (before the first frame is processed).
            auto viewerTabs = m_LiveStackerViewer->tabs();
            if (!viewerTabs.isEmpty() && !viewerTabs.first().isNull())
                viewerTabs.first()->stopProgrammatically();
            Q_EMIT liveStackingActiveChanged(false);
            sendResponse(commands[NEW_LIVESTACKER_STATE], QJsonObject{{"state", "stopped"}});
        }
    }
    else if (command == commands[LIVESTACKER_CLOSE])
    {
        if (m_Manager->captureModule())
        {
            if (m_LiveStackerLooping)
            {
                disconnect(m_Manager->captureModule()->mainCamera()->process().get(),
                           &Ekos::CameraProcess::newImage,
                           this,
                           &Message::saveLiveStackerFrame);
                m_LiveStackerLooping = false;
            }
            else
            {
                disconnect(m_Manager->captureModule()->mainCamera()->process().get(),
                           &Ekos::CameraProcess::newImage,
                           this,
                           &Message::onLiveStackerJobChanged);
            }
        }
        m_LiveStackerCurrentTarget.clear();
        m_LiveStackerCurrentFilter.clear();

        Q_EMIT liveStackingActiveChanged(false);
        if (m_LiveStackerViewer)
        {
            m_LiveStackerViewer->close();
            m_LiveStackerViewer = nullptr;
        }
        sendResponse(commands[NEW_LIVESTACKER_STATE], QJsonObject{{"state", "closed"}});
    }
}

///////////////////////////////////////////////////////////////////////////////////////////
/// Batch post-processing pipeline. Deliberately a separate
/// command surface and a separate StackController from LiveStacker above — this drives
/// an already-captured folder of subs through calibration/stacking/crop/PCC(pending)/
/// tone-mapping/finishing with no FITSViewer/FITSTab window involved, as opposed to
/// LiveStacker's live, attended, GUI-backed capture session.
///////////////////////////////////////////////////////////////////////////////////////////
QVector<QPointF> Message::parseCurvePoints(const QJsonArray &points) const
{
    QVector<QPointF> result;
    for (const auto &value : points)
    {
        const QJsonObject point = value.toObject();
        result << QPointF(point["x"].toDouble(), point["y"].toDouble());
    }
    return result;
}

QSharedPointer<StackController> Message::resolvePostProcessSession(const QJsonObject &payload) const
{
    return m_PostProcessSessions.value(payload["sessionId"].toString(m_DefaultPostProcessSession));
}

QVector<ChannelBlendOperation::WeightedInput> Message::parseBlendInputs(const QJsonArray &inputs, QString &error) const
{
    QVector<ChannelBlendOperation::WeightedInput> result;
    for (const auto &value : inputs)
    {
        const QJsonObject obj = value.toObject();
        QString sessionId = obj["sessionId"].toString();
        if (sessionId.isEmpty())
            sessionId = obj["filter"].toString();
        const double weight = obj["weight"].toDouble(1.0);

        if (sessionId.isEmpty())
        {
            error = QStringLiteral("Each blend input needs a \"filter\" or \"sessionId\"");
            return {};
        }

        auto session = m_PostProcessSessions.value(sessionId);
        if (!session || !session->imageData())
        {
            error = QString("No post-processing session named '%1'").arg(sessionId);
            return {};
        }
        const cv::Mat &image = session->imageData()->stackedImageMat();
        if (image.empty())
        {
            error = QString("Session '%1' has no stacked image yet").arg(sessionId);
            return {};
        }
        result.push_back({ image, weight, session->imageData()->getStackWCS() });
    }
    return result;
}

namespace
{
// Recognizes a calibration-type folder name — the vocabulary real capture tools (NINA,
// SharpCap, StellarMate/Ekos captures themselves, ...) use for this, in whatever
// singular/plural/hyphenated form: "Bias", "Biases", "Dark", "Darks", "Flat", "Flats",
// "Light", "Lights", and compounds like "Dark-Flats"/"Light-Flats" (also accepts "_"/" "
// as the joiner). Every hyphen/space/underscore-separated word must be one of these —
// whole-segment match only, so an unrelated folder that merely contains one of these
// words (e.g. a target actually named "Flats Nebula") won't false-positive.
bool isCalibrationTypeFolderName(const QString &name)
{
    static const QSet<QString> words
    {
        "bias", "biases", "dark", "darks", "flat", "flats", "light", "lights"
    };
    QString normalized = name.toLower();
    normalized.replace('_', '-');
    normalized.replace(' ', '-');
    const auto parts = normalized.split('-', Qt::SkipEmptyParts);
    if (parts.isEmpty())
        return false;
    for (const auto &part : parts)
        if (!words.contains(part))
            return false;
    return true;
}
}

QString Message::pipelineSessionRootFor(const QString &sourceDirectory) const
{
    QDir cursor(QDir::cleanPath(sourceDirectory));
    // A handful of levels comfortably covers every real layout seen (root/type,
    // root/type/filter, root/type/filter/exposure) without risking a false match many
    // levels up an unrelated ancestor path.
    for (int depth = 0; depth < 6; depth++)
    {
        if (isCalibrationTypeFolderName(cursor.dirName()))
        {
            QDir root = cursor;
            root.cdUp();
            return root.absolutePath();
        }
        if (!cursor.cdUp())
            break;
    }
    // Nothing recognized — fall back to the old "sibling of the input directory" rule
    // rather than refusing to generate a path at all.
    return QFileInfo(QDir::cleanPath(sourceDirectory)).dir().absolutePath();
}

QString Message::pipelineOutputDirForRoot(const QString &root) const
{
    QDir outputDir(QDir(root).filePath("Output"));
    QDir().mkpath(outputDir.absolutePath());
    return outputDir.absolutePath();
}

QString Message::pipelineOutputDirFor(const QString &sourceDirectory) const
{
    return pipelineOutputDirForRoot(pipelineSessionRootFor(sourceDirectory));
}

QString Message::generatePipelineOutputPath(const QString &sourceDirectory, const QString &baseName) const
{
    // Millisecond timestamp for a unique, naturally-ordered filename — same convention
    // saveLiveStackerFrame() already uses for the same reason.
    const QString filename = QString("%1_%2.fits").arg(baseName).arg(QDateTime::currentMSecsSinceEpoch());
    return QDir(pipelineOutputDirFor(sourceDirectory)).filePath(filename);
}

QString Message::generateStackOutputPathForRoot(const QString &root, const QString &identity) const
{
    const QString filename = identity.isEmpty() ? QStringLiteral("light_master.fits")
                              : QString("light_master_%1.fits").arg(identity);
    return QDir(pipelineOutputDirForRoot(root)).filePath(filename);
}

QString Message::generateStackOutputPath(const QString &sourceDirectory, const QString &identity) const
{
    return generateStackOutputPathForRoot(pipelineSessionRootFor(sourceDirectory), identity);
}

QString Message::describePostProcessStage(StackChannel channel, const QString &stage) const
{
    if (channel == StackChannel::SINGLE || channel == StackChannel::NONE)
        return stage;
    return QString("%1: %2").arg(StackChannelNames.value(channel)).arg(stage);
}

void Message::sendPostProcessState(const QJsonObject &state)
{
    const QString sessionId = state["sessionId"].toString(m_DefaultPostProcessSession);
    m_LastPostProcessState[sessionId] = state;
    sendResponse(commands[NEW_POSTPROCESS_STATE], state);
}

void Message::processPostProcessCommands(const QString &command, const QJsonObject &payload)
{
    if (command == commands[POSTPROCESS_STACK] && payload.contains("channels"))
    {
        // Filter-tagged mode: each entry stacks independently as its own mono session
        // (StackChannel::SINGLE, n==1) — sidesteps initStackChannels()'s positional R/G/B/L
        // assignment (and its outright rejection of exactly 2 directories) entirely, so
        // e.g. a 2-filter Ha+OIII narrowband set can be stacked and later blended with
        // postprocess_blend_channels. Each session is kept alive under m_PostProcessSessions
        // keyed by "filter", concurrently with any other session — nothing here replaces
        // an existing session the way the single-session mode below does.
        const QJsonArray channels = payload["channels"].toArray();
        if (channels.isEmpty())
        {
            sendResponse(commands[NEW_POSTPROCESS_STATE],
            QJsonObject{{"state", "error"}, {"message", "\"channels\" must have at least one entry"}});
            return;
        }

        // Opt-out via "preview": false, same convention as build_master/crop/apply_*.
        const bool wantPreview = payload["preview"].toBool(true);

        QJsonArray startedSessions;
        for (const auto &value : channels)
        {
            const QJsonObject channel = value.toObject();
            const QString filter = channel["filter"].toString();
            const QString directory = channel["directory"].toString();
            if (filter.isEmpty() || directory.isEmpty())
            {
                sendResponse(commands[NEW_POSTPROCESS_STATE],
                QJsonObject{{"state", "error"}, {"message", "Each channels[] entry needs \"filter\" and \"directory\""}});
                return;
            }

            // Shared stacking/post-processing params apply to every channel in this
            // call — only filter/directory/masterDark/masterFlat vary per channel.
            StackData params;
            params.calcSNR = payload["calcSNR"].toBool(true);
            params.alignMethod = static_cast<StackAlignMethod>(payload["alignMethod"].toInt(0));
            // Explicitly pick which frame every sub aligns against, instead of always
            // auto-selecting the first sub found. Real use case: aligning a new batch
            // against a previously-generated master/reference stack rather than
            // whichever sub happens to be discovered first. Note: with alignMethod
            // PLATE_SOLVE, using an already-stacked/processed image (rather than a raw
            // camera sub) as the align master has a known, unresolved failure mode
            // (star extraction on the derived image can come back empty, leaving the
            // batch stuck with no align master at all) — use alignMethod NONE for that
            // case instead, after confirming the two images already share a consistent
            // pixel grid.
            params.alignMaster = payload["alignMaster"].toString();
            params.stackingMethod = static_cast<StackingMethod>(payload["stackingMethod"].toInt(0));
            params.downscale = static_cast<StackDownscale>(payload["downscale"].toInt(0));
            params.numInMem = payload["numInMem"].toInt(0);
            params.weighting = static_cast<StackFrameWeighting>(payload["weighting"].toInt(0));
            params.lowSigma = payload["lowSigma"].toDouble(2.0);
            params.highSigma = payload["highSigma"].toDouble(3.0);
            // Hard reject on obvious star trailing (tracking failure, etc.) - on by
            // default since HFR/NUM_STARS weighting alone only down-weights a bad sub,
            // it never excludes it. See FITSData::detectStarTrailing().
            params.rejectTrailedSubs = payload["rejectTrailedSubs"].toBool(true);
            params.maxStarElongation = payload["maxStarElongation"].toDouble(0.08);
            // Per-sub cosmetic correction (FITSStack::correctSub()) — replaces a pixel
            // that's a k-sigma outlier vs. its local 3x3 median with that median, before
            // the sub ever reaches alignment/stacking. Off by default: a genuinely
            // stuck/hot sensor pixel is consistently bright in every sub, so SIGMA/
            // WINDSOR stacking's frame-to-frame outlier rejection can't catch it —
            // this is the only thing in the pipeline that can.
            params.hotPixels = payload["hotPixels"].toBool(false);
            params.coldPixels = payload["coldPixels"].toBool(false);
            params.postProcessing.postProcess = payload["postProcess"].toBool(false);
            params.postProcessing.gradientAmt = payload["gradientAmt"].toDouble(0.0);
            params.postProcessing.denoiseAmt = payload["denoiseAmt"].toDouble(0.0);
            params.postProcessing.denoiseMethod = static_cast<DenoiseMethod>(payload["denoiseMethod"].toInt(0));
            params.postProcessing.chromaDenoiseAmt = payload["chromaDenoiseAmt"].toDouble(0.0);
            params.postProcessing.deconvAmt = payload["deconvAmt"].toDouble(0.0);
            params.postProcessing.PSFSigma = payload["PSFSigma"].toDouble(1.0);
            params.postProcessing.sharpenAmt = payload["sharpenAmt"].toDouble(0.0);
            params.postProcessing.sharpenKernal = payload["sharpenKernal"].toInt(3);
            params.postProcessing.sharpenSigma = payload["sharpenSigma"].toDouble(3.0);

            const QString masterDark = channel["masterDark"].toString();
            const QString masterFlat = channel["masterFlat"].toString();
            if (!masterDark.isEmpty())
                params.masterDark = QVector<QString> {masterDark};
            if (!masterFlat.isEmpty())
                params.masterFlat = QVector<QString> {masterFlat};

            auto session = QSharedPointer<StackController>::create(this);
            connect(session.data(), &StackController::stackReady, this, [this, filter, session, wantPreview, directory](bool cancelled)
            {
                // Same "headless JPEG preview over wsMedia" pattern as the crop/apply_*/
                // build_master previews above — tagged "+PS", its own slot distinct from
                // build_master's "+P" (a standalone result, no "before" counterpart the
                // way "+PB"/"+PA" pair up for an edit step, but sharing "+P" with
                // build_master let a build_master preview and a stack preview from the
                // same calibrate-then-stack session race on the same tag with no
                // sessionId to break the tie).
                if (!cancelled && wantPreview)
                {
                    QString previewError;
                    const QByteArray jpeg = session->getPreviewJpegBytes(previewError);
                    if (!jpeg.isEmpty())
                        Q_EMIT postProcessPreviewReady(jpeg, QStringLiteral("+PS"), buildPreviewMetadata(session->imageData()));
                }

                QJsonObject state{{"state", cancelled ? "cancelled" : "ready"}, {"sessionId", filter}};
                if (!cancelled)
                {
                    // Auto-save the completed stack to a sibling "output" directory (next
                    // to this channel's own raw-sub directory) so the caller gets a full
                    // path back without a separate postprocess_save call. This is purely
                    // for persistence/traceability — postprocess_blend_channels reads this
                    // session's in-memory image directly and never needs the file on disk.
                    const QString outputPath = generateStackOutputPath(directory, filter);
                    QString saveError;
                    if (session->save(outputPath, saveError))
                        state["outputPath"] = outputPath;
                    else
                        state["saveError"] = saveError;
                }
                sendPostProcessState(state);
            });
            connect(session.data(), &StackController::stackFailed, this, [this, filter](const QString & reason)
            {
                sendPostProcessState(
                QJsonObject{{"state", "error"}, {"sessionId", filter}, {"message", reason}});
            });
            connect(session.data(), &StackController::stackUpdateStats, this,
                    [this, filter](bool ok, int sub, int total, double meanSNR, double minSNR, double maxSNR)
            {
                sendPostProcessState(QJsonObject
                {
                    {"state", "progress"}, {"sessionId", filter}, {"ok", ok}, {"sub", sub}, {"total", total},
                    {"meanSNR", meanSNR}, {"minSNR", minSNR}, {"maxSNR", maxSNR}
                });
            });
            connect(session.data(), &StackController::postProcessProgress, this,
                    [this, filter](StackChannel channel, const QString & stage)
            {
                sendPostProcessState(QJsonObject
                {
                    {"state", "progress"}, {"sessionId", filter}, {"message", describePostProcessStage(channel, stage)}
                });
            });

            session->start(QStringList { directory }, params);
            m_PostProcessSessions[filter] = session;
            m_PostProcessSessionRoots[filter] = pipelineSessionRootFor(directory);
            startedSessions << filter;
        }

        sendResponse(commands[NEW_POSTPROCESS_STATE], QJsonObject{{"state", "started"}, {"sessions", startedSessions}});
    }
    else if (command == commands[POSTPROCESS_STACK])
    {
        // Single-session mode: one mono directory, or positional RGB/RGBL directories
        // (see initStackChannels() — dirs[0]=RED, dirs[1]=GREEN, dirs[2]=BLUE, dirs[3]=LUM
        // for n>=3, dirs[0]=SINGLE for n==1). Stored under "sessionId" (defaults to the
        // single-session key), replacing any existing session under that same id.
        QStringList directories;
        if (payload.contains("directories"))
        {
            for (const auto &value : payload["directories"].toArray())
                directories << value.toString();
        }
        else
        {
            const QString directory = payload["directory"].toString();
            if (!directory.isEmpty())
                directories << directory;
        }

        if (directories.isEmpty() || (directories.size() != 1 && directories.size() < 3))
        {
            sendResponse(commands[NEW_POSTPROCESS_STATE],
            QJsonObject{{"state", "error"}, {"message", "Specify either \"directory\" (mono) or \"directories\" "
                    "with 3 (RGB) or 4 (RGB+L) entries in that order"
                }});
            return;
        }

        StackData params;
        params.calcSNR = payload["calcSNR"].toBool(true);
        params.alignMethod = static_cast<StackAlignMethod>(payload["alignMethod"].toInt(0));
        // See the Mode A alignMaster comment above for the known PLATE_SOLVE-against-
        // an-already-stacked-image caveat.
        params.alignMaster = payload["alignMaster"].toString();
        params.stackingMethod = static_cast<StackingMethod>(payload["stackingMethod"].toInt(0));
        params.downscale = static_cast<StackDownscale>(payload["downscale"].toInt(0));
        params.numInMem = payload["numInMem"].toInt(0);
        params.weighting = static_cast<StackFrameWeighting>(payload["weighting"].toInt(0));
        params.lowSigma = payload["lowSigma"].toDouble(2.0);
        params.highSigma = payload["highSigma"].toDouble(3.0);
        // Hard reject on obvious star trailing (tracking failure, etc.) - on by
        // default since HFR/NUM_STARS weighting alone only down-weights a bad sub,
        // it never excludes it. See FITSData::detectStarTrailing().
        params.rejectTrailedSubs = payload["rejectTrailedSubs"].toBool(true);
        params.maxStarElongation = payload["maxStarElongation"].toDouble(0.08);

        // Per-sub cosmetic correction (FITSStack::correctSub()) — replaces a pixel
        // that's a k-sigma outlier vs. its local 3x3 median with that median, before
        // the sub ever reaches alignment/stacking. Off by default: a genuinely
        // stuck/hot sensor pixel is consistently bright in every sub, so SIGMA/WINDSOR
        // stacking's frame-to-frame outlier rejection can't catch it — this is the
        // only thing in the pipeline that can.
        params.hotPixels = payload["hotPixels"].toBool(false);
        params.coldPixels = payload["coldPixels"].toBool(false);

        // Per-channel, pre-combine post-processing (gradient/background removal,
        // denoise, deconvolution, sharpen) — these run inline during stacking itself
        // (FITSStack::postProcessImage()), unlike the post-combine ops below
        // (crop/autostretch/curve/saturation/contrast), which is why they're set here
        // on StackData rather than exposed as their own postprocess_* commands.
        params.postProcessing.postProcess = payload["postProcess"].toBool(false);
        params.postProcessing.gradientAmt = payload["gradientAmt"].toDouble(0.0);
        params.postProcessing.denoiseAmt = payload["denoiseAmt"].toDouble(0.0);
        params.postProcessing.denoiseMethod = static_cast<DenoiseMethod>(payload["denoiseMethod"].toInt(0));
        params.postProcessing.chromaDenoiseAmt = payload["chromaDenoiseAmt"].toDouble(0.0);
        params.postProcessing.deconvAmt = payload["deconvAmt"].toDouble(0.0);
        params.postProcessing.PSFSigma = payload["PSFSigma"].toDouble(1.0);
        params.postProcessing.sharpenAmt = payload["sharpenAmt"].toDouble(0.0);
        params.postProcessing.sharpenKernal = payload["sharpenKernal"].toInt(3);
        params.postProcessing.sharpenSigma = payload["sharpenSigma"].toDouble(3.0);

        // Per-channel master dark/flat paths, aligned positionally with "directories"
        // (see initStackChannels()). "masterDarkPaths"/"masterFlatPaths" (arrays) are for
        // when each channel needs its own — typically true for flats (different filter
        // transmission per channel) and sometimes for darks (different exposure per
        // channel). The singular "masterDarkPath"/"masterFlatPath" fields remain as a
        // convenience that broadcasts one path to every channel — the common case for
        // darks, which usually don't depend on filter, just exposure/gain/temp.
        auto resolveMasterPaths = [&](const QString & arrayKey, const QString & singularKey) -> QVector<QString>
        {
            if (payload.contains(arrayKey))
            {
                QVector<QString> paths;
                for (const auto &value : payload[arrayKey].toArray())
                    paths << value.toString();
                return paths;
            }
            const QString single = payload[singularKey].toString();
            if (single.isEmpty())
                return {};
            QVector<QString> paths;
            for (int i = 0; i < directories.size(); i++)
                paths << single;
            return paths;
        };
        params.masterDark = resolveMasterPaths("masterDarkPaths", "masterDarkPath");
        params.masterFlat = resolveMasterPaths("masterFlatPaths", "masterFlatPath");

        // Opt-out via "preview": false, same convention as build_master/crop/apply_*.
        const bool wantPreview = payload["preview"].toBool(true);

        const QString sessionId = payload["sessionId"].toString(m_DefaultPostProcessSession);
        auto session = QSharedPointer<StackController>::create(this);
        connect(session.data(), &StackController::stackReady, this, [this, sessionId, session, wantPreview, directories](bool cancelled)
        {
            // Same "headless JPEG preview over wsMedia" pattern as the crop/apply_*/
            // build_master previews above — tagged "+PS", its own slot distinct from
            // build_master's "+P" (a standalone result, no "before" counterpart the way
            // "+PB"/"+PA" pair up for an edit step, but sharing "+P" with build_master
            // let a build_master preview and a stack preview from the same
            // calibrate-then-stack session race on the same tag with no sessionId to
            // break the tie).
            if (!cancelled && wantPreview)
            {
                QString previewError;
                const QByteArray jpeg = session->getPreviewJpegBytes(previewError);
                if (!jpeg.isEmpty())
                    Q_EMIT postProcessPreviewReady(jpeg, QStringLiteral("+PS"), buildPreviewMetadata(session->imageData()));
            }

            QJsonObject state{{"state", cancelled ? "cancelled" : "ready"}, {"sessionId", sessionId}};
            if (!cancelled)
            {
                // Auto-save the completed stack to a sibling "output" directory (next to
                // directories[0]) so the caller gets a full path back without a separate
                // postprocess_save call — see the Mode A "channels" branch above for why
                // this doesn't matter for postprocess_blend_channels itself. No filter
                // concept here (mono single-session, or an already-combined RGB/RGBL
                // result), unlike Mode A's per-filter sessions — but an explicit, non-default
                // sessionId (e.g. a split-exposure workflow's "ha_600"/"ha_900"/"ha_final"
                // intermediates) still needs to be folded in, or every such call would
                // collide on the same plain "light_master.fits" and clobber the last one.
                const QString identity = (sessionId == m_DefaultPostProcessSession) ? QString() : sessionId;
                const QString outputPath = generateStackOutputPath(directories.first(), identity);
                QString saveError;
                if (session->save(outputPath, saveError))
                    state["outputPath"] = outputPath;
                else
                    state["saveError"] = saveError;
            }
            sendPostProcessState(state);
        });
        connect(session.data(), &StackController::stackFailed, this, [this, sessionId](const QString & reason)
        {
            sendPostProcessState(
            QJsonObject{{"state", "error"}, {"sessionId", sessionId}, {"message", reason}});
        });
        connect(session.data(), &StackController::stackUpdateStats, this,
                [this, sessionId](bool ok, int sub, int total, double meanSNR, double minSNR, double maxSNR)
        {
            sendPostProcessState(QJsonObject
            {
                {"state", "progress"}, {"sessionId", sessionId}, {"ok", ok}, {"sub", sub}, {"total", total},
                {"meanSNR", meanSNR}, {"minSNR", minSNR}, {"maxSNR", maxSNR}
            });
        });
        connect(session.data(), &StackController::postProcessProgress, this,
                [this, sessionId](StackChannel channel, const QString & stage)
        {
            sendPostProcessState(QJsonObject
            {
                {"state", "progress"}, {"sessionId", sessionId}, {"message", describePostProcessStage(channel, stage)}
            });
        });

        session->start(directories, params);
        m_PostProcessSessions[sessionId] = session;
        m_PostProcessSessionRoots[sessionId] = pipelineSessionRootFor(directories.first());
        sendPostProcessState(QJsonObject{{"state", "started"}, {"sessionId", sessionId}});
    }
    else if (command == commands[POSTPROCESS_STOP])
    {
        const QString sessionId = payload["sessionId"].toString(m_DefaultPostProcessSession);
        // build_master and blend_channels have no StackController session (see their
        // handlers below) — their cancel path is a plain flag the worker thread polls
        // between steps, keyed the same way their "processing"/"busy" states already
        // are (build_master's fixed key, or blend's own outputSessionId), so a caller
        // can stop either the same way it stops any real session.
        auto cancelFlag = m_PostProcessCancelFlags.value(sessionId);
        if (cancelFlag)
        {
            cancelFlag->storeRelease(1);
            sendResponse(commands[NEW_POSTPROCESS_STATE], QJsonObject{{"state", "stopping"}, {"sessionId", sessionId}});
            return;
        }

        auto session = resolvePostProcessSession(payload);
        if (session)
            session->cancel();
        sendResponse(commands[NEW_POSTPROCESS_STATE], QJsonObject{{"state", "stopped"}});
    }
    else if (command == commands[POSTPROCESS_CLOSE])
    {
        // Removes only the named session (default: the single-session key) — with
        // multiple concurrent sessions (filter-tagged mode), each must be closed
        // individually by its own sessionId.
        const QString closedSessionId = payload["sessionId"].toString(m_DefaultPostProcessSession);
        m_PostProcessSessions.remove(closedSessionId);
        m_PostProcessSessionRoots.remove(closedSessionId);
        sendResponse(commands[NEW_POSTPROCESS_STATE], QJsonObject{{"state", "closed"}});
    }
    else if (command == commands[POSTPROCESS_BUILD_MASTER])
    {
        // Standalone — doesn't need an active post-processing session at all,
        // per §1: building a master is an input to a future start(), not a step that
        // runs against an existing stacked result.
        const QString directory = payload["directory"].toString();
        const QString typeStr = payload["type"].toString();
        // outputPath is now optional — if omitted, a path is auto-generated below (once
        // typeStr is validated) in a sibling "output" directory next to `directory`.
        QString outputPath = payload["outputPath"].toString();
        if (directory.isEmpty())
        {
            sendResponse(commands[NEW_POSTPROCESS_STATE],
            QJsonObject{{"state", "error"}, {"message", "directory is required"}});
            return;
        }

        MasterBuilder::Type type = MasterBuilder::Type::DARK;
        if (typeStr == "bias")
            type = MasterBuilder::Type::BIAS;
        else if (typeStr == "flat")
            type = MasterBuilder::Type::FLAT;
        else if (typeStr != "dark")
        {
            sendResponse(commands[NEW_POSTPROCESS_STATE],
            QJsonObject{{"state", "error"}, {"message", QString("Unknown master type '%1' — expected bias/dark/flat").arg(typeStr)}});
            return;
        }

        if (outputPath.isEmpty())
        {
            // Fold the source folder's own name into the generated filename — building
            // several masters of the same type from sibling subfolders (Dark-Flats/120,
            // /180, /300; Light-Flats/H-Alpha, /OIII, /SII; ...) all land in the same
            // sibling "output" directory (see pipelineOutputDirFor()), and without this
            // they'd be indistinguishable "master_<type>_<timestamp>.fits" files. Skipped
            // when the folder's own name is just the type word again — singular ("Bias",
            // "Dark", "Flat") or plural ("Biases", "Darks", "Flats") — since that adds no
            // information, just a redundant "master_bias_Biases_<ts>.fits". Checking both
            // a trailing "s" and "es" strip covers "Darks"->"dark" and "Biases"->"bias"
            // (English adds "-es", not just "-s", after a word already ending in "s").
            const QString dirName = QFileInfo(QDir::cleanPath(directory)).fileName();
            const QString normalizedDir = dirName.toLower();
            QStringList dirCandidates{normalizedDir};
            if (normalizedDir.endsWith(QStringLiteral("es")))
                dirCandidates << normalizedDir.chopped(2);
            if (normalizedDir.endsWith('s'))
                dirCandidates << normalizedDir.chopped(1);
            QString baseName = QString("master_%1").arg(typeStr);
            if (!dirName.isEmpty() && !dirCandidates.contains(typeStr))
                baseName += "_" + dirName;
            outputPath = generatePipelineOutputPath(directory, baseName);
        }

        const double lowSigma = payload["lowSigma"].toDouble(3.0);
        const double highSigma = payload["highSigma"].toDouble(3.0);
        // Subtracted from each raw sub before combining — real use case is building a
        // proper master flat: flats are usually taken at a much shorter exposure than
        // lights, where the sensor's bias/offset pattern still matters even though dark
        // current doesn't, so pass a pre-built master bias here when type is "flat".
        const QString subtractPath = payload["biasPath"].toString();
        // Only combine files whose EXPTIME header is within exptimeTolerance seconds of
        // matchExptime — for a shared calibration folder that mixes multiple exposure
        // lengths (e.g. light-darks and flat-darks together) with no other way to tell
        // them apart. Omitted/negative (the default) disables filtering, matching the
        // original behavior of combining every FITS-loadable file in the directory.
        const double matchExptime = payload["matchExptime"].toDouble(-1.0);
        const double exptimeTolerance = payload["exptimeTolerance"].toDouble(0.5);
        const bool wantPreview = payload["preview"].toBool(true);

        // No session of its own (see the comment above) — a fixed key stands in for
        // "sessionId" here so it shares the busy-tracking/last-state-cache machinery
        // every real session uses (postprocess_get_state, "state":"busy" rejection of
        // an overlapping call) instead of needing a parallel mechanism just for this.
        const QString buildMasterKey = QStringLiteral("build_master");
        if (m_BusyPostProcessSessions.contains(buildMasterKey))
        {
            sendResponse(commands[NEW_POSTPROCESS_STATE],
            QJsonObject{{"state", "busy"}, {"sessionId", buildMasterKey}});
            return;
        }

        // Combining dozens of subs and writing a large FITS to disk can take a while —
        // run it off the GUI thread (same QtConcurrent::run + QFutureWatcher pattern
        // FITSData::redoPostProcessStack() already uses) so it neither freezes KStars
        // nor holds the caller's request open. Ack immediately; the real outcome
        // follows as a second new_postprocess_state push (or via postprocess_get_state).
        m_BusyPostProcessSessions.insert(buildMasterKey);
        sendPostProcessState(QJsonObject{{"state", "processing"}, {"op", command}, {"sessionId", buildMasterKey}});

        struct BuildMasterResult
        {
            bool ok = false;
            bool cancelled = false;
            QString error;
            cv::Mat builtMaster;
        };

        // Fresh flag per build (build_master's own "busy" check above guarantees only
        // one runs at a time under this fixed key). postprocess_stop sets it;
        // MasterBuilder polls it between subs on the worker thread below.
        auto cancelFlag = QSharedPointer<QAtomicInt>::create(0);
        m_PostProcessCancelFlags[buildMasterKey] = cancelFlag;

        // Human-readable label for the progress messages below, e.g. "Bias frame
        // foo.fits 3/20 processed" — mirrors what a caller building a master flat/dark/
        // bias would want to show a user, same spirit as postprocess_stack's per-sub
        // "progress" state (see the POSTPROCESS_STACK handlers above) but MasterBuilder
        // has no StackController/FITSData session to emit stackUpdateStats from.
        const QString typeLabel = typeStr.isEmpty() ? QStringLiteral("Dark")
                                  : (typeStr.left(1).toUpper() + typeStr.mid(1));

        auto future = QtConcurrent::run([this, directory, type, outputPath, lowSigma, highSigma, subtractPath,
                                         matchExptime, exptimeTolerance, buildMasterKey, typeLabel,
                                         cancelFlag]() -> BuildMasterResult
        {
            BuildMasterResult result;
            auto onProgress = [this, buildMasterKey, typeLabel](int current, int total, const QString & filename)
            {
                // Emitted from this worker thread — QMetaObject::invokeMethod on `this`
                // (GUI-thread-affine) queues it there instead of touching the websocket
                // off-thread, same cross-thread-signal rule the rest of Message relies on.
                QMetaObject::invokeMethod(this, [this, buildMasterKey, typeLabel, current, total, filename]()
                {
                    sendPostProcessState(QJsonObject
                    {
                        {"state", "progress"}, {"sessionId", buildMasterKey}, {"current", current}, {"total", total},
                        {
                            "message", QString("%1 frame %2 %3/%4 processed")
                            .arg(typeLabel).arg(filename).arg(current).arg(total)
                        }
                    });
                }, Qt::QueuedConnection);
            };
            auto isCancelled = [cancelFlag]()
            {
                return cancelFlag->loadAcquire() != 0;
            };
            result.ok = MasterBuilder::buildAndSave(directory, type, outputPath, result.error, lowSigma, highSigma,
                                                    subtractPath, matchExptime, exptimeTolerance, &result.builtMaster,
                                                    onProgress, isCancelled, &result.cancelled);
            return result;
        });

        auto watcher = new QFutureWatcher<BuildMasterResult>(this);
        connect(watcher, &QFutureWatcher<BuildMasterResult>::finished, this,
                [this, watcher, buildMasterKey, outputPath, wantPreview, typeLabel, cancelFlag]()
        {
            m_BusyPostProcessSessions.remove(buildMasterKey);
            // Only clear if it's still ours — a newer build_master call may already have
            // replaced it with its own flag by the time this one's finished() fires.
            if (m_PostProcessCancelFlags.value(buildMasterKey) == cancelFlag)
                m_PostProcessCancelFlags.remove(buildMasterKey);
            const BuildMasterResult result = watcher->result();
            watcher->deleteLater();

            if (!result.ok)
            {
                sendPostProcessState(QJsonObject
                {
                    {"state", result.cancelled ? "cancelled" : "error"},
                    {"sessionId", buildMasterKey}, {"message", result.error}
                });
                return;
            }

            // Reuses the in-memory result directly (no re-reading the just-written file
            // off disk) — see PreviewRenderer; headless, no FITSView/GUI dependency. Sent
            // over the wsMedia binary channel (tagged "+P", same "+X module image"
            // convention as Align/Focus/Guide/DarkLibrary previews) rather than inline in
            // this JSON response — the app receives the fetchable URL asynchronously via
            // the existing NEW_IMAGE_METADATA message. Opt-out via "preview": false, same
            // convention as the crop/apply_*/denoise commands.
            if (wantPreview)
            {
                QString previewError;
                const QByteArray jpeg = PreviewRenderer::renderJpeg(result.builtMaster, previewError);
                if (!jpeg.isEmpty())
                {
                    // No FITSData wrapper for a freshly-built master — read stats straight
                    // off the cv::Mat instead of going through buildPreviewMetadata().
                    const QJsonObject metadata
                    {
                        {"resolution", QString("%1x%2").arg(result.builtMaster.cols).arg(result.builtMaster.rows)},
                        {"channels", result.builtMaster.channels()}
                    };
                    Q_EMIT postProcessPreviewReady(jpeg, QStringLiteral("+P"), metadata);
                }
            }

            sendPostProcessState(QJsonObject
            {
                {"state", "master_built"}, {"sessionId", buildMasterKey}, {"outputPath", outputPath},
                {"message", QString("%1 master frame built successfully").arg(typeLabel)}
            });
        });
        watcher->setFuture(future);
    }
    else if (command == commands[POSTPROCESS_INSPECT_DIRECTORY])
    {
        // Standalone, same as POSTPROCESS_BUILD_MASTER — reports what's actually in a
        // folder (EXPTIME/FILTER/binning/IMAGETYP per file, header-only) so a caller can
        // discover the right matchExptime for build_master, or the right exposure/filter
        // for postprocess_stack, without external tooling. Useful for any folder
        // (bias/dark/flat/light), not darks specifically. Recurses into subfolders (e.g.
        // a Lights/Darks/Flats/Bias layout with Lights further split into HA/SII/OIII),
        // so "tree" below reports per-folder totals like Lights=100, Lights/HA=20.
        const QString directory = payload["directory"].toString();
        if (directory.isEmpty())
        {
            sendResponse(commands[NEW_POSTPROCESS_STATE],
            QJsonObject{{"state", "error"}, {"message", "directory is required"}});
            return;
        }

        QVector<DirectoryInspector::FileInfo> files;
        QVector<DirectoryInspector::Group> groups;
        DirectoryInspector::DirectoryNode tree;
        QString error;
        if (!DirectoryInspector::inspect(directory, files, groups, tree, error))
        {
            sendResponse(commands[NEW_POSTPROCESS_STATE], QJsonObject{{"state", "error"}, {"message", error}});
            return;
        }

        QJsonArray filesArray;
        for (const auto &file : files)
        {
            QJsonObject entry
            {
                {"filename", file.filename}, {"directory", file.directory}, {"exptime", file.exptime},
                {"filter", file.filter}, {"binning", file.binning}, {"imagetyp", file.imagetyp}
            };
            if (!file.error.isEmpty())
                entry["error"] = file.error;
            filesArray << entry;
        }

        std::function<QJsonArray(const QVector<DirectoryInspector::Group> &)> groupsToJson =
                [](const QVector<DirectoryInspector::Group> &groups)
        {
            QJsonArray groupsArray;
            for (const auto &group : groups)
            {
                groupsArray << QJsonObject
                {
                    {"exptime", group.exptime}, {"filter", group.filter},
                    {"binning", group.binning}, {"imagetyp", group.imagetyp}, {"count", group.count}
                };
            }
            return groupsArray;
        };

        // Mirrors DirectoryInspector::DirectoryNode: "groups"/"fileCount" describe only
        // this folder's own files, while "totalGroups"/"totalFileCount" roll up this
        // folder plus every descendant (e.g. Lights totalFileCount = HA + SII + OIII +
        // any files directly under Lights itself).
        std::function<QJsonObject(const DirectoryInspector::DirectoryNode &)> nodeToJson =
                [&](const DirectoryInspector::DirectoryNode & node)
        {
            QJsonArray subdirsArray;
            for (const auto &child : node.subdirs)
                subdirsArray << nodeToJson(child);

            return QJsonObject
            {
                {"name", node.name}, {"path", node.relativePath},
                {"fileCount", node.files.size()}, {"groups", groupsToJson(node.groups)},
                {"totalFileCount", node.totalFileCount}, {"totalGroups", groupsToJson(node.totalGroups)},
                {"subdirs", subdirsArray}
            };
        };

        sendResponse(commands[NEW_POSTPROCESS_STATE], QJsonObject
        {
            {"state", "inspected"}, {"directory", directory}, {"fileCount", files.size()},
            {"files", filesArray}, {"groups", groupsToJson(groups)}, {"tree", nodeToJson(tree)}
        });
    }
    else if (command == commands[POSTPROCESS_LOAD_MASTER])
    {
        // Loads exactly one FITS file from disk into a new session — e.g. a
        // previously-saved master light from an earlier run — with no directory scan
        // involved, unlike postprocess_stack (the only other way to get a file into a
        // session, which always scans+combines a whole directory). Exists so
        // postprocess_blend_channels can be pointed at an on-disk master as an R/G/B
        // input, not just a session freshly stacked in this same run — see the
        // "Session model" section above.
        const QString sessionId = payload["sessionId"].toString();
        const QString path = payload["path"].toString();
        if (sessionId.isEmpty())
        {
            sendResponse(commands[NEW_POSTPROCESS_STATE],
            QJsonObject{{"state", "error"}, {"message", "sessionId is required"}});
            return;
        }
        if (path.isEmpty())
        {
            sendResponse(commands[NEW_POSTPROCESS_STATE],
            QJsonObject{{"state", "error"}, {"sessionId", sessionId}, {"message", "path is required"}});
            return;
        }

        if (m_BusyPostProcessSessions.contains(sessionId))
        {
            sendResponse(commands[NEW_POSTPROCESS_STATE],
            QJsonObject{{"state", "busy"}, {"sessionId", sessionId}});
            return;
        }

        m_BusyPostProcessSessions.insert(sessionId);
        sendPostProcessState(QJsonObject{{"state", "processing"}, {"op", command}, {"sessionId", sessionId}});

        struct LoadMasterResult
        {
            bool ok = false;
            QString error;
            cv::Mat frame;
            struct wcsprm *wcs = nullptr;
            int nwcs = 0;
        };

        auto future = QtConcurrent::run([path]() -> LoadMasterResult
        {
            LoadMasterResult result;
            double median = 0;
            // FITS_NORMAL (not MasterBuilder's own FITS_CALIBRATE default) so
            // FITSData::loadImage() also loads the file's own WCS, if it has one — see
            // MasterBuilder::loadFrame()'s doc comment. A file with no WCS (or a solve
            // that never ran) loads fine here regardless; result.wcs just stays null,
            // same as an alignMethod-NONE stack session.
            result.ok = MasterBuilder::loadFrame(path, result.frame, median, result.error,
                                                 FITS_NORMAL, &result.wcs, &result.nwcs);
            return result;
        });

        auto watcher = new QFutureWatcher<LoadMasterResult>(this);
        connect(watcher, &QFutureWatcher<LoadMasterResult>::finished, this, [this, watcher, sessionId]()
        {
            m_BusyPostProcessSessions.remove(sessionId);
            LoadMasterResult result = watcher->result();
            watcher->deleteLater();

            if (!result.ok)
            {
                sendPostProcessState(
                QJsonObject{{"state", "error"}, {"sessionId", sessionId}, {"message", result.error}});
                return;
            }

            // Passing wcs (deep-copied again inside adopt()) lets this session carry a
            // WCS, same as a real plate-solved stack or a blend_channels output — so
            // postprocess_blend_channels' own cross-channel registration check, and
            // postprocess_apply_color_calibration, both still work against it.
            QString adoptError;
            auto session = QSharedPointer<StackController>::create(this);
            const bool adopted = session->adopt(result.frame, adoptError, result.wcs);
            MasterBuilder::freeWcs(&result.wcs, result.nwcs);
            if (!adopted)
            {
                sendPostProcessState(
                QJsonObject{{"state", "error"}, {"sessionId", sessionId}, {"message", adoptError}});
                return;
            }
            // Same defensive wiring as postprocess_blend_channels' output session (see
            // below) — otherwise a later postprocess_redo_postprocess against this
            // session would complete or fail with no way to tell the caller.
            connect(session.data(), &StackController::stackReady, this, [this, sessionId](bool cancelled)
            {
                sendPostProcessState(
                QJsonObject{{"state", cancelled ? "cancelled" : "ready"}, {"sessionId", sessionId}});
            });
            connect(session.data(), &StackController::stackFailed, this, [this, sessionId](const QString & reason)
            {
                sendPostProcessState(QJsonObject{{"state", "error"}, {"sessionId", sessionId}, {"message", reason}});
            });
            connect(session.data(), &StackController::postProcessProgress, this,
                    [this, sessionId](StackChannel channel, const QString & stage)
            {
                sendPostProcessState(QJsonObject
                {
                    {"state", "progress"}, {"sessionId", sessionId}, {"message", describePostProcessStage(channel, stage)}
                });
            });
            m_PostProcessSessions[sessionId] = session;

            sendPostProcessState(QJsonObject{{"state", "loaded"}, {"sessionId", sessionId}});
        });
        watcher->setFuture(future);
    }
    else if (command == commands[POSTPROCESS_BLEND_CHANNELS])
    {
        // The actual narrowband "pixel math": arbitrary weighted sums of any named,
        // already-stacked mono session into each output R/G/B channel — not just a
        // fixed one-filter-per-slot assignment. See ChannelBlendOperation.
        //
        // parseBlendInputs() itself stays on the GUI thread (it only resolves session
        // ids to their already-stacked cv::Mat/wcsprm, cheap, and touches
        // m_PostProcessSessions which isn't thread-safe) — only the actual
        // registration+blend work below (potentially several WCS registrations, each
        // real image-warping work) moves to a worker thread.
        QString error;
        const auto red = parseBlendInputs(payload["red"].toArray(), error);
        if (!error.isEmpty())
        {
            sendResponse(commands[NEW_POSTPROCESS_STATE], QJsonObject{{"state", "error"}, {"message", error}});
            return;
        }
        const auto green = parseBlendInputs(payload["green"].toArray(), error);
        if (!error.isEmpty())
        {
            sendResponse(commands[NEW_POSTPROCESS_STATE], QJsonObject{{"state", "error"}, {"message", error}});
            return;
        }
        const auto blue = parseBlendInputs(payload["blue"].toArray(), error);
        if (!error.isEmpty())
        {
            sendResponse(commands[NEW_POSTPROCESS_STATE], QJsonObject{{"state", "error"}, {"message", error}});
            return;
        }

        // RAM pre-flight for the blend: every input frame is resident at once, each
        // is then warped onto the reference grid, and three output channels plus the
        // merged result are built — roughly 2x the inputs plus a few output frames.
        {
            double inputBytes = 0.0;
            int inputCount = 0;
            for (const auto *group : { &red, &green, &blue })
                for (const auto &in : *group)
                {
                    inputBytes += static_cast<double>(in.image.total()) * in.image.elemSize();
                    inputCount++;
                }
            const double monoFrame = inputCount > 0 ? inputBytes / inputCount : 0.0;
            const double needBytes = 2.0 * inputBytes + 4.0 * monoFrame;
            const double availBytes = KSUtils::getAvailableRAM();
            if (availBytes > 0.0 && availBytes < needBytes)
            {
                sendResponse(commands[NEW_POSTPROCESS_STATE],
                QJsonObject{{"state", "error"},
                    {"message", QString("Not enough free memory to combine channels: ~%1 MB needed, %2 MB available. "
                                        "Free up memory or combine fewer/smaller channels.")
                     .arg(needBytes / 1e6, 0, 'f', 0).arg(availBytes / 1e6, 0, 'f', 0)}});
                return;
            }
        }

        // Inherit a "session root" (see pipelineSessionRootFor()) from whichever named
        // input session has one, so the blend result can be auto-saved into the same
        // shared "Output" folder its inputs' own masters/stacks already used — a blend
        // has no source `directory` of its own to derive one from. Checked in red/green/
        // blue order; every real input is a stack session that got one when it started
        // (see postprocess_stack above), so this only comes up empty for an unusual case
        // (all inputs already-adopted sessions with no root of their own) — the blend
        // result then simply isn't auto-saved, same as before this feature existed.
        auto findInheritedRoot = [this](const QJsonArray & inputs) -> QString
        {
            for (const auto &value : inputs)
            {
                const QJsonObject obj = value.toObject();
                QString sessionId = obj["sessionId"].toString();
                if (sessionId.isEmpty())
                    sessionId = obj["filter"].toString();
                if (!sessionId.isEmpty() && m_PostProcessSessionRoots.contains(sessionId))
                    return m_PostProcessSessionRoots.value(sessionId);
            }
            return QString();
        };
        QString blendRoot = findInheritedRoot(payload["red"].toArray());
        if (blendRoot.isEmpty())
            blendRoot = findInheritedRoot(payload["green"].toArray());
        if (blendRoot.isEmpty())
            blendRoot = findInheritedRoot(payload["blue"].toArray());

        // The blend result becomes its own session — same crop/apply_*/save lifecycle
        // as any real stack from here on, adopted (with its WCS, if any) once blending
        // finishes below.
        const QString outputSessionId = payload["outputSessionId"].toString(QStringLiteral("blended"));

        // Distinct named input sessions of this blend, gathered here on the GUI
        // thread so the completion lambda can free their pixel buffers once the
        // blend has succeeded — see the release call there. They are dead weight
        // for the rest of the run and, on a small controller, three full-resolution
        // mono stacks (each holding a working Mat AND its FITS buffer) are the
        // single biggest avoidable cost during post-processing.
        QSet<QString> blendInputIds;
        for (const auto &key : { "red", "green", "blue" })
        {
            for (const auto &value : payload[key].toArray())
            {
                const QJsonObject obj = value.toObject();
                const QString id = obj["sessionId"].toString(obj["filter"].toString());
                if (!id.isEmpty() && id != outputSessionId)
                    blendInputIds.insert(id);
            }
        }
        const bool wantPreview = payload["preview"].toBool(true);
        // Level-match the named inputs to a common background before the weighted sum (see
        // ChannelBlendOperation::WeightedInput::background) — the inputs are
        // independently-stacked filters whose sky levels routinely differ, and without this
        // the blend carries that level ratio through as a colour cast. On by default;
        // normalize:false gives the raw literal weighted sum.
        const bool normalize = payload["normalize"].toBool(true);
        if (m_BusyPostProcessSessions.contains(outputSessionId))
        {
            sendResponse(commands[NEW_POSTPROCESS_STATE],
            QJsonObject{{"state", "busy"}, {"sessionId", outputSessionId}});
            return;
        }

        m_BusyPostProcessSessions.insert(outputSessionId);
        sendPostProcessState(QJsonObject{{"state", "processing"}, {"op", command}, {"sessionId", outputSessionId}});

        struct BlendResult
        {
            bool ok = false;
            bool cancelled = false;
            QString error;
            cv::Mat blended;
            const struct wcsprm *refWcs = nullptr;
        };

        // Independent concurrent blends (different outputSessionId) each get their own
        // flag, so stopping one doesn't touch the others — see m_PostProcessCancelFlags.
        auto cancelFlag = QSharedPointer<QAtomicInt>::create(0);
        m_PostProcessCancelFlags[outputSessionId] = cancelFlag;

        auto future = QtConcurrent::run([this, red, green, blue, outputSessionId, cancelFlag, normalize]() -> BlendResult
        {
            BlendResult result;
            auto onProgress = [this, outputSessionId](int current, int total, const QString & label)
            {
                // See the build_master handler above for why this needs
                // QMetaObject::invokeMethod rather than calling sendPostProcessState()
                // directly from this worker thread.
                QMetaObject::invokeMethod(this, [this, outputSessionId, current, total, label]()
                {
                    sendPostProcessState(QJsonObject
                    {
                        {"state", "progress"}, {"sessionId", outputSessionId}, {"current", current}, {"total", total},
                        {"message", label}
                    });
                }, Qt::QueuedConnection);
            };
            auto isCancelled = [cancelFlag]()
            {
                return cancelFlag->loadAcquire() != 0;
            };
            result.ok = ChannelBlendOperation::blendRGB(red, green, blue, result.blended, result.refWcs, result.error,
                                                        normalize, onProgress, isCancelled, &result.cancelled);
            return result;
        });

        auto watcher = new QFutureWatcher<BlendResult>(this);
        connect(watcher, &QFutureWatcher<BlendResult>::finished, this,
                [this, watcher, outputSessionId, cancelFlag, blendRoot, wantPreview, blendInputIds]()
        {
            m_BusyPostProcessSessions.remove(outputSessionId);
            if (m_PostProcessCancelFlags.value(outputSessionId) == cancelFlag)
                m_PostProcessCancelFlags.remove(outputSessionId);
            const BlendResult result = watcher->result();
            watcher->deleteLater();

            if (!result.ok)
            {
                sendPostProcessState(QJsonObject
                {
                    {"state", result.cancelled ? "cancelled" : "error"},
                    {"sessionId", outputSessionId}, {"message", result.error}
                });
                return;
            }

            // Passing refWcs (deep-copied inside adopt()) lets the new session carry a
            // WCS, same as a real plate-solved stack, so crop() keeps tracking it and
            // postprocess_apply_color_calibration can use it.
            QString adoptError;
            auto outputSession = QSharedPointer<StackController>::create(this);
            if (!outputSession->adopt(result.blended, adoptError, result.refWcs))
            {
                sendPostProcessState(
                QJsonObject{{"state", "error"}, {"sessionId", outputSessionId}, {"message", adoptError}});
                return;
            }
            // A standalone completion preview, same "+P"/"+PS" convention build_master/
            // stack already use — blend_channels previously sent none of its own, and the
            // app had to fake one with a no-op postprocess_apply_bge(strength: 0) purely
            // to trigger that command's +PB/+PA mechanism. "+PC" gives it a real preview
            // slot instead — see "Preview images" in the pipeline README.
            if (wantPreview)
            {
                QString previewError;
                const QByteArray jpeg = outputSession->getPreviewJpegBytes(previewError);
                if (!jpeg.isEmpty())
                    Q_EMIT postProcessPreviewReady(jpeg, QStringLiteral("+PC"), buildPreviewMetadata(outputSession->imageData()));
            }
            // Without these, a later postprocess_redo_postprocess against this session
            // would complete (or fail) with no way to tell the caller — nothing
            // forwarded its stackReady/stackFailed/postProcessProgress to a response at
            // all, unlike a postprocess_stack session's connections (see the
            // POSTPROCESS_STACK handlers above).
            connect(outputSession.data(), &StackController::stackReady, this, [this, outputSessionId](bool cancelled)
            {
                sendPostProcessState(
                QJsonObject{{"state", cancelled ? "cancelled" : "ready"}, {"sessionId", outputSessionId}});
            });
            connect(outputSession.data(), &StackController::stackFailed, this,
                    [this, outputSessionId](const QString & reason)
            {
                sendPostProcessState(
                QJsonObject{{"state", "error"}, {"sessionId", outputSessionId}, {"message", reason}});
            });
            connect(outputSession.data(), &StackController::postProcessProgress, this,
                    [this, outputSessionId](StackChannel channel, const QString & stage)
            {
                sendPostProcessState(QJsonObject
                {
                    {"state", "progress"}, {"sessionId", outputSessionId}, {"message", describePostProcessStage(channel, stage)}
                });
            });
            m_PostProcessSessions[outputSessionId] = outputSession;

            QJsonObject state
            {
                {"state", "blended"}, {"sessionId", outputSessionId}, {"outputSessionId", outputSessionId}
            };
            if (!blendRoot.isEmpty())
            {
                // Inherited from an input session above — carry it forward so a later
                // postprocess_save (with no explicit outputPath) against this blend, or a
                // further blend built from it, can keep using the same shared "Output"
                // folder. Always uses outputSessionId as the identity (never suppressed
                // the way postprocess_stack Mode B's internal default key is), since
                // blend_channels' own default, "blended", is already meaningful.
                m_PostProcessSessionRoots[outputSessionId] = blendRoot;
                const QString outputPath = generateStackOutputPathForRoot(blendRoot, outputSessionId);
                QString saveError;
                if (outputSession->save(outputPath, saveError))
                    state["outputPath"] = outputPath;
                else
                    state["saveError"] = saveError;
            }
            sendPostProcessState(state);

            // The blend succeeded and the result is an independent image (blendRGB()'s
            // cv::merge allocates a fresh buffer, so it shares nothing with the
            // inputs), so the input sessions' pixel buffers are now pure overhead.
            // Free them to keep the long post-processing run that follows inside a
            // small controller's RAM budget. The sessions stay registered so their
            // ids still resolve and postprocess_close stays a clean no-op; ops on
            // them now fail with the normal "no stacked image" message, and a
            // re-blend simply needs them re-stacked.
            for (const QString &id : blendInputIds)
            {
                const auto input = m_PostProcessSessions.value(id);
                if (input && input->imageData())
                    input->imageData()->releaseStackedImage();
            }
        });
        watcher->setFuture(future);
    }
    else if (command == commands[POSTPROCESS_REDO_POSTPROCESS])
    {
        // Unlike crop/apply_*/save below, this is asynchronous — it recomputes
        // gradient/denoise/deconv/sharpen from the already-combined stack
        // (FITSStack::redoPostProcessStack(), on a background thread) without
        // re-running calibration/plate-solve/alignment/combine, so callers can
        // iterate on post-processing parameters fast. Completion is reported via
        // the same stackReady-driven "ready" event postprocess_stack's session
        // already emits (session->redoPostProcess() ultimately re-triggers it).
        auto session = resolvePostProcessSession(payload);
        if (!session)
        {
            sendResponse(commands[NEW_POSTPROCESS_STATE],
            QJsonObject{{"state", "error"}, {"message", "No active post-processing session — call postprocess_stack first"}});
            return;
        }
        StackPPData ppParams;
        ppParams.postProcess = payload["postProcess"].toBool(true);
        ppParams.gradientAmt = payload["gradientAmt"].toDouble(0.0);
        ppParams.denoiseAmt = payload["denoiseAmt"].toDouble(0.0);
        ppParams.denoiseMethod = static_cast<DenoiseMethod>(payload["denoiseMethod"].toInt(0));
        ppParams.chromaDenoiseAmt = payload["chromaDenoiseAmt"].toDouble(0.0);
        ppParams.deconvAmt = payload["deconvAmt"].toDouble(0.0);
        ppParams.PSFSigma = payload["PSFSigma"].toDouble(1.0);
        ppParams.sharpenAmt = payload["sharpenAmt"].toDouble(0.0);
        ppParams.sharpenKernal = payload["sharpenKernal"].toInt(3);
        ppParams.sharpenSigma = payload["sharpenSigma"].toDouble(3.0);
        session->redoPostProcess(ppParams);
        sendPostProcessState(
        QJsonObject{{"state", "redoing"}, {"sessionId", payload["sessionId"].toString(m_DefaultPostProcessSession)}});
    }
    else if (command == commands[POSTPROCESS_CROP]
             || command == commands[POSTPROCESS_APPLY_AUTOSTRETCH]
             || command == commands[POSTPROCESS_APPLY_CURVE]
             || command == commands[POSTPROCESS_APPLY_CURVE_PER_CHANNEL]
             || command == commands[POSTPROCESS_APPLY_SATURATION]
             || command == commands[POSTPROCESS_APPLY_CONTRAST]
             || command == commands[POSTPROCESS_APPLY_DENOISE]
             || command == commands[POSTPROCESS_APPLY_BGE]
             || command == commands[POSTPROCESS_APPLY_COLOR_CALIBRATION]
             || command == commands[POSTPROCESS_SAVE])
    {
        auto session = resolvePostProcessSession(payload);
        if (!session)
        {
            sendResponse(commands[NEW_POSTPROCESS_STATE],
            QJsonObject{{"state", "error"}, {"message", "No active post-processing session — call postprocess_stack first"}});
            return;
        }

        const QString sessionId = payload["sessionId"].toString(m_DefaultPostProcessSession);
        if (m_BusyPostProcessSessions.contains(sessionId))
        {
            sendResponse(commands[NEW_POSTPROCESS_STATE],
            QJsonObject{{"state", "busy"}, {"sessionId", sessionId}});
            return;
        }

        // Refuse a memory-heavy step up front if the machine can't hold it, instead
        // of letting an allocation fail (or the OOM killer reap KStars) mid-step.
        // The estimate is a multiple of one full frame — see postProcessMemoryWeight.
        if (const int memWeight = postProcessMemoryWeight(command))
        {
            const cv::Mat &working = session->imageData()->stackedImageMat();
            if (!working.empty())
            {
                const double frameBytes = static_cast<double>(working.total()) * working.elemSize();
                const double needBytes = frameBytes * memWeight;
                const double availBytes = KSUtils::getAvailableRAM();
                if (availBytes > 0.0 && availBytes < needBytes)
                {
                    sendPostProcessState(QJsonObject
                    {
                        {"state", "error"}, {"sessionId", sessionId},
                        {"message", QString("Not enough free memory to run %1: ~%2 MB needed, %3 MB available. "
                                             "Free up memory or process a smaller image.")
                         .arg(command).arg(needBytes / 1e6, 0, 'f', 0).arg(availBytes / 1e6, 0, 'f', 0)}
                    });
                    return;
                }
            }
        }

        // postprocess_save's outputPath is optional too, same as build_master — falls
        // back to the shared "Output" folder for this session's target when one is known
        // (m_PostProcessSessionRoots, populated when the session was created by
        // postprocess_stack/postprocess_blend_channels). Resolved here, on the GUI
        // thread, before dispatch below — the worker lambda only touches
        // `session`/`payload` by design (see its own comment), and
        // m_PostProcessSessionRoots is regular Message state, not safe to read
        // concurrently with a GUI-thread write (a different session starting/closing).
        QString saveOutputPath;
        if (command == commands[POSTPROCESS_SAVE])
        {
            saveOutputPath = payload["outputPath"].toString();
            if (saveOutputPath.isEmpty())
            {
                const QString root = m_PostProcessSessionRoots.value(sessionId);
                if (root.isEmpty())
                {
                    sendResponse(commands[NEW_POSTPROCESS_STATE],
                    QJsonObject{{"state", "error"}, {"sessionId", sessionId},
                        {"message", "outputPath is required — no auto-generated location available for this session"}});
                    return;
                }
                const QString identity = (sessionId == m_DefaultPostProcessSession) ? QString() : sessionId;
                saveOutputPath = generateStackOutputPathForRoot(root, identity);
            }
        }

        // A JPEG preview of the working image, before and after this step — headless,
        // no FITSView/GUI dependency (PreviewRenderer), downscaled first so it stays
        // cheap regardless of the source resolution. Sent over the wsMedia binary
        // channel using the same "+X module image" convention as Align/Focus/Guide/
        // DarkLibrary previews ("+PB" before, "+PA" after) rather than inline in this
        // JSON response — every state update would otherwise carry a full image
        // payload over the JSON socket. The app receives fetchable URLs
        // asynchronously via the existing NEW_IMAGE_METADATA message, one per tag, so
        // it can show a before/after comparison rather than just the end result.
        // Skipped for save() (the working image didn't change) and, for the "after"
        // half, on failure (nothing new to show). Opt-out via "preview": false for a
        // caller that doesn't need visual feedback on every call (e.g. scripted batch
        // adjustments) and wants the fastest possible response.
        const bool wantPreview = command != commands[POSTPROCESS_SAVE] && payload["preview"].toBool(true);
        if (wantPreview)
        {
            QString previewError;
            const QByteArray jpeg = session->getPreviewJpegBytes(previewError);
            if (!jpeg.isEmpty())
                Q_EMIT postProcessPreviewReady(jpeg, QStringLiteral("+PB"), buildPreviewMetadata(session->imageData()));
        }

        // crop/apply_*/save bake directly into the session's working image and can take
        // a while (denoise, color calibration with star matching, writing a large file)
        // — run the actual operation off the GUI thread (same QtConcurrent::run +
        // QFutureWatcher pattern FITSData::redoPostProcessStack() already uses for
        // postprocess_redo_postprocess) so it neither freezes KStars nor holds the
        // caller's request open. Ack immediately with "processing"; the real outcome
        // follows as a second new_postprocess_state push (or via postprocess_get_state).
        // Only one operation may be in flight per session at a time (see the "busy"
        // check above) — that, plus each worker touching only its own session's
        // QSharedPointer, keeps concurrent sessions (e.g. Ha + OIII) from racing.
        m_BusyPostProcessSessions.insert(sessionId);
        sendPostProcessState(QJsonObject{{"state", "processing"}, {"op", command}, {"sessionId", sessionId}});

        // Captures `this` only for parseCurvePoints() — a pure, const helper safe to
        // call from a worker thread; no other Message state is touched here.
        // saveOutputPath is passed in already-resolved (see above) rather than reading
        // payload["outputPath"] again in here, for the same reason.
        auto future = QtConcurrent::run([this, session, command, payload, saveOutputPath]() -> PostProcessOpResult
        {
            PostProcessOpResult result;
            QString error;
            bool ok = false;

            if (command == commands[POSTPROCESS_CROP])
            {
                const QRect roi(payload["x"].toInt(), payload["y"].toInt(), payload["width"].toInt(), payload["height"].toInt());
                ok = session->crop(roi, error);
                result.extra = {{"state", ok ? "cropped" : "error"}};
            }
            else if (command == commands[POSTPROCESS_APPLY_AUTOSTRETCH])
            {
                ok = session->applyAutoStretch(payload["targetBackground"].toDouble(0.25),
                                               payload["shadowsClipping"].toDouble(2.8), error, payload["linked"].toBool(true),
                                               payload["neutralizeBackground"].toBool(false));
                result.extra = {{"state", ok ? "stretched" : "error"}};
            }
            else if (command == commands[POSTPROCESS_APPLY_CURVE])
            {
                const QVector<QPointF> points = parseCurvePoints(payload["points"].toArray());
                ok = session->applyCurve(points, error);
                result.extra = {{"state", ok ? "curve_applied" : "error"}};
            }
            else if (command == commands[POSTPROCESS_APPLY_CURVE_PER_CHANNEL])
            {
                const QVector<QVector<QPointF>> channelPoints
                {
                    parseCurvePoints(payload["red"].toArray()),
                    parseCurvePoints(payload["green"].toArray()),
                    parseCurvePoints(payload["blue"].toArray())
                };
                ok = session->applyCurvePerChannel(channelPoints, error);
                result.extra = {{"state", ok ? "curve_applied" : "error"}};
            }
            else if (command == commands[POSTPROCESS_APPLY_SATURATION])
            {
                ok = session->applySaturation(payload["amt"].toDouble(1.0), error);
                result.extra = {{"state", ok ? "saturation_applied" : "error"}};
            }
            else if (command == commands[POSTPROCESS_APPLY_CONTRAST])
            {
                ok = session->applyContrast(payload["amt"].toDouble(1.0), error);
                result.extra = {{"state", ok ? "contrast_applied" : "error"}};
            }
            else if (command == commands[POSTPROCESS_APPLY_DENOISE])
            {
                // Independent, composable post-combine step — unlike postprocess_start/
                // redo_postprocess's bundled denoise (which always re-runs gradient/
                // deconv/sharpen too), this only touches denoise, operating on whatever
                // the current working image already is (post-crop, post-BGE, ...).
                const auto method = static_cast<DenoiseMethod>(payload["denoiseMethod"].toInt(0));
                ok = session->applyDenoise(payload["denoiseAmt"].toDouble(0.0), method,
                                           payload["chromaDenoiseAmt"].toDouble(0.0), error);
                result.extra = {{"state", ok ? "denoise_applied" : "error"}};
            }
            else if (command == commands[POSTPROCESS_APPLY_BGE])
            {
                // Independent, composable post-combine step — a rebuild of the sampling/
                // fitting core behind gradientAmt (see BGEOperation's class comment for
                // what changed and why), operating on whatever the current working image
                // already is.
                ok = session->applyBGE(payload["strength"].toDouble(0.0), error);
                result.extra = {{"state", ok ? "bge_applied" : "error"}};
            }
            else if (command == commands[POSTPROCESS_APPLY_COLOR_CALIBRATION])
            {
                // Independent, opt-in, composable post-combine step — a caller that never
                // sends this command gets exactly today's behavior. Requires the session
                // to carry a WCS (a plate-solved stack, or a blend of plate-solved stacks
                // — see StackController::adopt()/FITSData::setStackedImage()).
                int starsDetected = 0, starsMatched = 0;
                ok = session->applyPhotometricCalibration(payload["strength"].toDouble(1.0),
                     payload["maxCatalogMagnitude"].toDouble(12.0),
                     payload["matchRadiusArcsec"].toDouble(5.0),
                     error, starsDetected, starsMatched);
                result.extra = {{"state", ok ? "color_calibration_applied" : "error"},
                    {"starsDetected", starsDetected}, {"starsMatched", starsMatched}
                };
            }
            else if (command == commands[POSTPROCESS_SAVE])
            {
                ok = session->save(saveOutputPath, error);
                result.extra = {{"state", ok ? "saved" : "error"}, {"outputPath", saveOutputPath}};
            }

            result.ok = ok;
            result.error = error;
            return result;
        });

        auto watcher = new QFutureWatcher<PostProcessOpResult>(this);
        connect(watcher, &QFutureWatcher<PostProcessOpResult>::finished, this,
                [this, watcher, session, sessionId, wantPreview]()
        {
            m_BusyPostProcessSessions.remove(sessionId);
            const PostProcessOpResult result = watcher->result();
            watcher->deleteLater();

            if (result.ok && wantPreview)
            {
                QString previewError;
                const QByteArray jpeg = session->getPreviewJpegBytes(previewError);
                if (!jpeg.isEmpty())
                    Q_EMIT postProcessPreviewReady(jpeg, QStringLiteral("+PA"), buildPreviewMetadata(session->imageData()));
            }

            QJsonObject response = result.extra;
            response["sessionId"] = sessionId;
            if (!result.ok)
                response["message"] = result.error;
            sendPostProcessState(response);
        });
        watcher->setFuture(future);
    }
    else if (command == commands[POSTPROCESS_GET_STATE])
    {
        // Synchronous, cheap — answers from m_LastPostProcessState (kept current by
        // sendPostProcessState()) rather than waiting on a push, so a caller can poll
        // instead of, or after missing, one (e.g. after a reconnect).
        const QString sessionId = payload["sessionId"].toString(m_DefaultPostProcessSession);
        sendResponse(commands[NEW_POSTPROCESS_STATE],
        m_LastPostProcessState.value(sessionId, QJsonObject{{"state", "unknown"}, {"sessionId", sessionId}}));
    }
    else if (command == commands[POSTPROCESS_UNDO])
    {
        // Reverts the single most recent crop/apply_*/color-calibration step (see
        // FITSData::undoLastOperation()) — synchronous and cheap (a buffer swap plus a
        // FITS re-encode, not real pixel math), unlike the ops it undoes, so this
        // doesn't need the QtConcurrent dispatch/"processing" ack those use. Still
        // checked against the busy-set so it can't race a same-session op that's
        // currently mid-flight.
        auto session = resolvePostProcessSession(payload);
        if (!session)
        {
            sendResponse(commands[NEW_POSTPROCESS_STATE],
            QJsonObject{{"state", "error"}, {"message", "No active post-processing session — call postprocess_stack first"}});
            return;
        }

        const QString sessionId = payload["sessionId"].toString(m_DefaultPostProcessSession);
        if (m_BusyPostProcessSessions.contains(sessionId))
        {
            sendResponse(commands[NEW_POSTPROCESS_STATE],
            QJsonObject{{"state", "busy"}, {"sessionId", sessionId}});
            return;
        }

        const bool wantPreview = payload["preview"].toBool(true);
        if (wantPreview)
        {
            QString previewError;
            const QByteArray jpeg = session->getPreviewJpegBytes(previewError);
            if (!jpeg.isEmpty())
                Q_EMIT postProcessPreviewReady(jpeg, QStringLiteral("+PB"), buildPreviewMetadata(session->imageData()));
        }

        QString error;
        const bool ok = session->undoLastOperation(error);

        if (ok && wantPreview)
        {
            QString previewError;
            const QByteArray jpeg = session->getPreviewJpegBytes(previewError);
            if (!jpeg.isEmpty())
                Q_EMIT postProcessPreviewReady(jpeg, QStringLiteral("+PA"), buildPreviewMetadata(session->imageData()));
        }

        QJsonObject response{{"state", ok ? "undone" : "error"}, {"sessionId", sessionId}};
        if (!ok)
            response["message"] = error;
        sendPostProcessState(response);
    }
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Message::processFilterOffsetCommands(const QString &command, const QJsonObject &payload)
{
    if (!m_Manager)
        return;

    // Find the BuildFilterOffsets dialog via FilterManager
    QSharedPointer<Ekos::FilterManager> fm;
    if (!m_Manager->getFilterManager(fm) || !fm)
        return;

    auto bfo = fm->getBuildFilterOffsets();
    if (!bfo)
    {
        qCWarning(KSTARS_EKOS) << "Ignoring filter offset command" << command << "as BuildFilterOffsets is not available";
        return;
    }

    if (command == commands[FILTER_OFFSET_GET_ALL_SETTINGS])
    {
        sendFilterOffsetSettings(bfo->getAllSettings());
    }
    else if (command == commands[FILTER_OFFSET_SET_ALL_SETTINGS])
    {
        auto settings = payload.toVariantMap();
        bfo->setAllSettings(settings);
    }
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Message::sendFilterOffsetSettings(const QVariantMap &settings)
{
    sendResponse(commands[FILTER_OFFSET_GET_ALL_SETTINGS], QJsonObject::fromVariantMap(settings));
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Message::sendFilterOffsetProgress(int current, int total, const QString &status)
{
    // Send lightweight progress-only payload (current/total/status)
    QJsonObject payload =
    {
        {"current", current},
        {"total", total},
        {"status", status}
    };
    sendResponse(commands[FILTER_OFFSET_PROGRESS], payload);
}

void Message::sendFilterOffsetCalculated(const QString &filter, int newOffset, int average)
{
    QJsonObject payload =
    {
        {"filter", filter},
        {"newOffset", newOffset},
        {"average", average}
    };
    sendResponse(commands[FILTER_OFFSET_CALCULATED], payload);
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Message::sendAIGuideProgress(int current, int total, const QString &status)
{
    QJsonObject payload =
    {
        {"state_string", status},
        {"phases_completed", current},
        {"total_phases", total}
    };
    sendResponse(commands[NEW_AI_GUIDE_PROGRESS], payload);
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Message::sendAIGuideLog(const QString &message)
{
    QJsonObject payload =
    {
        {"message", message}
    };
    sendResponse(commands[NEW_AI_GUIDE_LOG], payload);
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Message::sendAIGuideComplete()
{
    sendResponse(commands[NEW_AI_GUIDE_COMPLETE], QJsonObject());
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Message::sendAIGuideTrainingProgress(const QString &message)
{
    QJsonObject payload =
    {
        {"message", message}
    };
    sendResponse(commands[NEW_AI_GUIDE_TRAINING_PROGRESS], payload);
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Message::sendAIGuideTrainingComplete()
{
    sendResponse(commands[NEW_AI_GUIDE_TRAINING_COMPLETE], QJsonObject());
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Message::sendAIGuideTrainingError(const QString &error)
{
    QJsonObject payload =
    {
        {"message", error}
    };
    sendResponse(commands[NEW_AI_GUIDE_TRAINING_ERROR], payload);
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Message::sendLiveStackerProgress(bool ok, int sub, int total, double meanSNR, double minSNR, double maxSNR)
{
    QJsonObject state =
    {
        {"state", "stacking"},
        {"ok", ok},
        {"frames_stacked", sub},
        {"total_frames", total},
        {"mean_snr", meanSNR},
        {"min_snr", minSNR},
        {"max_snr", maxSNR}
    };
    sendResponse(commands[NEW_LIVESTACKER_STATE], state);
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Message::sendLiveStackerComplete()
{
    QJsonObject state =
    {
        {"state", "complete"}
    };
    sendResponse(commands[NEW_LIVESTACKER_STATE], state);
}

///////////////////////////////////////////////////////////////////////////////////////////
/// Called for every JOBTYPE_PREVIEW (looping/framing) frame while livestacking is active.
/// Saves the full-resolution FITSData to stackingDirectory so the native LiveStacker
/// filesystem watcher picks it up automatically.
///////////////////////////////////////////////////////////////////////////////////////////
void Message::saveLiveStackerFrame(const QSharedPointer<Ekos::SequenceJob> &job, const QSharedPointer<FITSData> &data)
{
    Q_UNUSED(job)

    if (!m_LiveStackerLooping || !data)
        return;

    const QString dir = m_LiveStackerSettings.value("stackingDirectory").toString();
    if (dir.isEmpty())
        return;

    // Use millisecond timestamp to guarantee a unique, naturally-ordered filename.
    const QString filename = QDir(dir).filePath(
                                 QString("frame_%1.fits").arg(QDateTime::currentMSecsSinceEpoch())
                             );

    if (!data->saveImage(filename))
        qCWarning(KSTARS_EKOS) << "LiveStacker looping: failed to save frame to" << filename;
}

///////////////////////////////////////////////////////////////////////////////////////////
/// Called for each captured frame in active-sequence live-stacking mode.
/// Compares the incoming job's target and filter against what the stacker is currently
/// monitoring. If either changed (i.e. a new sequence job started), the stacker is
/// stopped, its input directory is updated to the new job's actual save path (derived
/// from SJ_Signature), and then the stacker is restarted. The output directory remains
/// unchanged so the EkosLive server-side PictureMonitor keeps watching without any
/// server-side changes.
///////////////////////////////////////////////////////////////////////////////////////////
void Message::onLiveStackerJobChanged(const QSharedPointer<Ekos::SequenceJob> &job,
                                      const QSharedPointer<FITSData> &data)
{
    Q_UNUSED(data)

    if (!m_LiveStackerViewer || !job)
        return;

    const QString newTarget = job->getCoreProperty(Ekos::SequenceJob::SJ_TargetName).toString();
    const QString newFilter = job->getCoreProperty(Ekos::SequenceJob::SJ_Filter).toString();

    // Same job — nothing to do
    if (newTarget == m_LiveStackerCurrentTarget && newFilter == m_LiveStackerCurrentFilter)
        return;

    qCInfo(KSTARS_EKOS) << "LiveStacker: sequence job changed from"
                        << m_LiveStackerCurrentTarget << "/" << m_LiveStackerCurrentFilter
                        << "to" << newTarget << "/" << newFilter << "— restarting stacker";

    // Resolve the new job's actual save directory from its SJ_Signature.
    // SJ_Signature is the filename template used by PlaceholderPath; its directory
    // component gives us the subdirectory where the new job's frames are saved.
    const QString sig = job->getCoreProperty(Ekos::SequenceJob::SJ_Signature).toString();
    if (sig.isEmpty())
    {
        qCWarning(KSTARS_EKOS) << "LiveStacker: cannot restart — new job has no SJ_Signature";
        return;
    }

    const QString newDirectory = QFileInfo(sig).absoluteDir().path();
    if (newDirectory.isEmpty())
    {
        qCWarning(KSTARS_EKOS) << "LiveStacker: cannot restart — resolved directory is empty for sig:" << sig;
        return;
    }

    // Update tracking state
    m_LiveStackerCurrentTarget = newTarget;
    m_LiveStackerCurrentFilter = newFilter;

    // Access the live stacker tab (always the first tab)
    auto viewerTabs = m_LiveStackerViewer->tabs();
    if (viewerTabs.isEmpty() || viewerTabs.first().isNull())
        return;

    // Stop the current stack cleanly
    viewerTabs.first()->stopProgrammatically();

    // Update the stacking input directory in our settings so that subsequent
    // calls (e.g. another job change) use the latest directory as the base.
    m_LiveStackerSettings["stackingDirectory"] = newDirectory;

    // Rebuild params from the stored settings (same as in LIVESTACKER_START)
    StackData params;
    params.calcSNR           = m_LiveStackerSettings.value("calcSNR", true).toBool();
    params.alignMethod       = static_cast<StackAlignMethod>(m_LiveStackerSettings.value("alignMethod", 0).toInt());
    params.stackingMethod    = static_cast<StackingMethod>(m_LiveStackerSettings.value("stackingMethod", 0).toInt());
    params.downscale         = static_cast<StackDownscale>(m_LiveStackerSettings.value("downscale", 0).toInt());
    params.numInMem          = m_LiveStackerSettings.value("numInMem", 10).toInt();
    params.weighting         = static_cast<StackFrameWeighting>(m_LiveStackerSettings.value("weighting", 0).toInt());
    params.lowSigma          = m_LiveStackerSettings.value("lowSigma", 2.0).toDouble();
    params.highSigma         = m_LiveStackerSettings.value("highSigma", 3.0).toDouble();
    params.postProcessing.postProcess = m_LiveStackerSettings.value("postProcess", false).toBool();
    params.postProcessing.sharpenAmt  = m_LiveStackerSettings.value("sharpenAmt", 0.0).toDouble();
    params.postProcessing.denoiseAmt  = m_LiveStackerSettings.value("denoiseAmt", 0.0).toDouble();
    params.postProcessing.deconvAmt   = m_LiveStackerSettings.value("deconvAmt", 0.0).toDouble();
    params.postProcessing.gradientAmt   = m_LiveStackerSettings.value("gradientAmt", 0.0).toDouble();

    const QString masterDark = m_LiveStackerSettings.value("masterDarkPath").toString();
    const QString masterFlat = m_LiveStackerSettings.value("masterFlatPath").toString();
    if (!masterDark.isEmpty())
        params.masterDark = QVector<QString> {masterDark};
    if (!masterFlat.isEmpty())
        params.masterFlat = QVector<QString> {masterFlat};

    // The output directory stays the same — the EkosLive server continues
    // watching it for new stacked frames without requiring any server restart.
    params.outputDirectory = m_LiveStackerSettings.value("outputDirectory").toString();

    // Restart the stacker watching the new job's directory
    viewerTabs.first()->startProgrammatically(newDirectory, params);

    // Reconnect the per-frame progress/completion signals on the new view
    QSharedPointer<FITSView> currentView = viewerTabs.first()->getView();
    if (currentView)
    {
        connect(currentView.get(), &FITSView::stackUpdateStats,
                this, &Message::sendLiveStackerProgress, Qt::UniqueConnection);
        connect(currentView.get(), &FITSView::resetStack,
                this, &Message::sendLiveStackerComplete, Qt::UniqueConnection);
    }

    // Notify the server/app so it can clear its stacked-image buffer and show
    // a "new target" banner to the user.
    sendResponse(commands[NEW_LIVESTACKER_STATE], QJsonObject
    {
        {"state",   "restarted"},
        {"target",  newTarget},
        {"filter",  newFilter}
    });
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Message::processArtificialHorizonCommands(const QString &command, const QJsonObject &payload)
{
    auto *horizonComponent = KStarsData::Instance()->skyComposite()->artificialHorizon();
    if (!horizonComponent)
    {
        return;
    }

    if (command == commands[ARTIFICIAL_HORIZON_IMPORT])
    {
        QString name;
        bool isCeiling = false;
        QList<SkyPoint> pts;


        if (payload.contains("data"))
        {

            QString data = payload["data"].toString();

            if (data.isEmpty())
            {
                qCWarning(KSTARS_EKOS) << "Artificial Horizon import: data is empty";
                return;
            }

            QJsonParseError parseError;
            QJsonDocument jsonDoc = QJsonDocument::fromJson(data.toUtf8(), &parseError);

            // Parse as raw KStars horizon text format
            QTextStream in(&data, QIODevice::ReadOnly);
            while (!in.atEnd())
            {
                const QString line = in.readLine().trimmed();
                if (line.isEmpty() || line.startsWith('#'))
                    continue;
                if (line.startsWith("Ceiling"))
                {
                    isCeiling = true;
                    name = line.mid(static_cast<int>(strlen("Ceiling"))).trimmed();
                }
                else if (line.startsWith("Horizon"))
                {
                    name = line.mid(static_cast<int>(strlen("Horizon"))).trimmed();
                }
                else
                {
                    const QStringList cols = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
                    if (cols.size() != 2 || cols[0].isEmpty() || cols[1].isEmpty())
                        continue;
                    SkyPoint pt;
                    pt.setAz(dms::fromString(cols[0], true));
                    pt.setAlt(dms::fromString(cols[1], true));
                    pts.append(pt);
                }
            }
        }


        if (pts.size() < 2 || name.isEmpty())
        {
            return;
        }

        std::shared_ptr<LineList> list(new LineList());
        for (const auto &pt : pts)
        {
            auto sp = std::make_shared<SkyPoint>();
            sp->setAz(pt.az());
            sp->setAlt(pt.alt());
            sp->HorizontalToEquatorial(KStarsData::Instance()->lst(), KStarsData::Instance()->geo()->lat());
            list->append(sp);
        }

        horizonComponent->removeRegion(name);
        horizonComponent->addRegion(name, true, list, isCeiling);
        horizonComponent->save();

        // Ensure the ground/horizon is visible after import
        if (!Options::showGround())
        {
            Options::setShowGround(true);
        }

        SkyMap::Instance()->forceUpdateNow();
    }
    else if (command == commands[ARTIFICIAL_HORIZON_TOGGLE])
    {
        const bool enabled = payload["enabled"].toBool();
        const QString region = payload["region"].toString();

        if (region.isEmpty())
        {
            // Toggle all regions
            for (auto *entity : *horizonComponent->getHorizon().horizonList())
                horizonComponent->setRegionEnabled(entity->region(), enabled);
        }
        else
        {
            // Toggle a specific named region
            horizonComponent->setRegionEnabled(region, enabled);
        }

        horizonComponent->save();
        SkyMap::Instance()->forceUpdateNow();
    }
    else if (command == commands[ARTIFICIAL_HORIZON_GET])
    {
        QJsonArray regionsArray;
        for (const auto *entity : *horizonComponent->getHorizon().horizonList())
        {
            QJsonObject regionObject;
            regionObject["name"]    = entity->region();
            regionObject["enabled"] = entity->enabled();
            regionObject["ceiling"] = entity->ceiling();

            QJsonArray pointsArray;
            if (entity->list())
            {
                const auto *pts = entity->list()->points();
                for (const auto &sp : *pts)
                {
                    QJsonObject pt;
                    pt["az"]  = sp->az().Degrees();
                    pt["alt"] = sp->alt().Degrees();
                    pointsArray.append(pt);
                }
            }
            regionObject["points"] = pointsArray;
            regionsArray.append(regionObject);
        }
        sendResponse(commands[ARTIFICIAL_HORIZON_GET], regionsArray);
    }
}

}
