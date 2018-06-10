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
#include <memory>

#include "ekos/ekos.h"
#include "ekos/ekosmanager.h"
#include "ekos/align/align.h"
#include "indi/indicommon.h"
#include "ui_ekoslivedialog.h"
#include "ksnotification.h"

class QProgressIndicator;
class QNetworkAccessManager;
class QNetworkReply;
class FITSView;

class EkosLiveClient : public QDialog, public Ui::EkosLiveDialog
{
    Q_OBJECT
public:
    explicit EkosLiveClient(EkosManager *manager);
    ~EkosLiveClient();

    bool isConnected() const { return m_isConnected; }    
    void sendResponse(const QString &command, const QJsonObject &payload);
    void sendResponse(const QString &command, const QJsonArray &payload);

    void updateMountStatus(const QJsonObject &status);
    void updateCaptureStatus(const QJsonObject &status);
    void updateFocusStatus(const QJsonObject &status);
    void updateGuideStatus(const QJsonObject &status);
    void sendPreviewImage(FITSView *view);
    void sendUpdatedFrame(FITSView *view);    
    void sendEvent(const QString &message, KSNotification::EventType event);

    // Send devices as they come
    void sendCameras();
    void sendMounts();
    void sendScopes();
    void sendFilterWheels();

    enum COMMANDS
    {
        GET_CONNECTION,
        GET_STATES,
        GET_CAMERAS,
        GET_MOUNTS,
        GET_SCOPES,
        GET_FILTER_WHEELS,
        NEW_CONNECTION_STATE,
        NEW_MOUNT_STATE,
        NEW_CAPTURE_STATE,
        NEW_GUIDE_STATE,        
        NEW_FOCUS_STATE,
        NEW_ALIGN_STATE,
        NEW_POLAR_STATE,
        NEW_PREVIEW_IMAGE,
        NEW_VIDEO_FRAME,
        NEW_ALIGN_FRAME,
        NEW_NOTIFICATION,
        NEW_TEMPERATURE,

        // Profiles
        GET_PROFILES,
        START_PROFILE,
        STOP_PROFILE,

        // Configuration
        SET_BANDWIDTH,

        // Capture
        CAPTURE_PREVIEW,
        CAPTURE_TOGGLE_VIDEO,
        CAPTURE_START,
        CAPTURE_STOP,
        CAPTURE_GET_SEQUENCES,
        CAPTURE_ADD_SEQUENCE,
        CAPTURE_REMOVE_SEQUENCE,

        // Mount
        MOUNT_PARK,
        MOUNT_UNPARK,
        MOUNT_ABORT,
        MOUNT_SYNC_RADE,
        MOUNT_SYNC_TARGET,
        MOUNT_GOTO_RADE,
        MOUNT_GOTO_TARGET,
        MOUNT_SET_MOTION,
        MOUNT_SET_TRACKING,
        MOUNT_SET_SLEW_RATE,

        // Focus
        FOCUS_START,
        FOCUS_STOP,

        // Guide
        GUIDE_START,
        GUIDE_STOP,
        GUIDE_CLEAR,

        // Align
        ALIGN_SOLVE,
        ALIGN_STOP,
        ALIGN_LOAD_AND_SLEW,
        ALIGN_SELECT_SCOPE,
        ALIGN_SELECT_SOLVER_TYPE,
        ALIGN_SELECT_SOLVER_ACTION,
        ALIGN_SET_FILE_EXTENSION,
        ALIGN_SET_CAPTURE_SETTINGS,

        // Polar Assistant Helper
        PAH_START,
        PAH_STOP,
        PAH_REFRESH,
        PAH_SET_MOUNT_DIRECTION,
        PAH_SET_MOUNT_ROTATION,
        PAH_SET_CROSSHAIR,
        PAH_SELECT_STAR_DONE,
        PAH_REFRESHING_DONE,
    };

    static QMap<COMMANDS, QString> const commands;

public slots:
    void setAlignStatus(Ekos::AlignState newState);
    void setAlignSolution(const QJsonObject &solution);
    void setPAHStage(Ekos::Align::PAHStage stage);
    void setPAHMessage(const QString &message);
    void setPAHEnabled(bool enabled);

    void setFOVTelescopeType(int index);

    void setEkosStatingStatus(EkosManager::CommunicationStatus status);
    //void setAlignFrame(FITSView* view);

    void sendCaptureSequence(const QJsonArray &sequenceArray);

signals:
    void connected();
    void disconnected();

protected slots:
    void authenticate();
    void onResult(QNetworkReply *reply);

private slots:
    void connectAuthServer();
    void disconnectAuthServer();

    void onMessageConnected();
    void onMessageDisconnected();
    void onMessageTextReceived(const QString &message);

    void onMediaConnected();
    void onMediaDisconnected();
    void onMediaTextReceived(const QString &message);
    void onMediaBinaryReceived(const QByteArray &message);

    void sendVideoFrame(std::unique_ptr<QImage> & frame);
    void sendConnection();

private:
    void connectMessageServer();
    void disconnectMessageServer();

    void connectMediaServer();
    void disconnectMediaServer();

    // Capture
    void processCaptureCommands(const QString &command, const QJsonObject &payload);
    void setCaptureSettings(const QJsonObject &settings);
    void sendTemperature(double value);

    // Mount
    void processMountCommands(const QString &command, const QJsonObject &payload);

    // Focus
    void processFocusCommands(const QString &command, const QJsonObject &payload);

    // Guide
    void processGuideCommands(const QString &command, const QJsonObject &payload);

    // Align
    void processAlignCommands(const QString &command, const QJsonObject &payload);

    // Polar
    void processPolarCommands(const QString &command, const QJsonObject &payload);

    // Profile
    void processProfileCommands(const QString &command, const QJsonObject &payload);

    // Profiles
    void sendProfiles();
    void sendStates();    


    QWebSocket m_messageWebSocket, m_mediaWebSocket;
    bool m_isConnected { false };
    EkosManager *m_Manager { nullptr };
    QNetworkAccessManager *networkManager { nullptr };

    QProgressIndicator *pi { nullptr };

    QString token;
    QString extension;
    QStringList temporaryFiles;
    QUrl m_serviceURL, m_wsURL;
};
