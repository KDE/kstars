/*  Ekos Live Client

    Copyright (C) 2018 Jasem Mutlaq <mutlaqja@ikarustech.com>

    Message Channel

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#pragma once

#include <QtWebSockets/QWebSocket>
#include <memory>

#include "ekos/ekos.h"
#include "ekos/manager.h"

namespace EkosLive
{
class Message : public QObject
{
        Q_OBJECT

    public:
        explicit Message(Ekos::Manager *manager);
        virtual ~Message() = default;

        void sendResponse(const QString &command, const QJsonObject &payload);
        void sendResponse(const QString &command, const QJsonArray &payload);

        void setAuthResponse(const QJsonObject &response)
        {
            m_AuthResponse = response;
        }
        void setURL(const QUrl &url)
        {
            m_URL = url;
        }

        // Module Status Updates
        void updateMountStatus(const QJsonObject &status, bool throttle = false);
        void updateCaptureStatus(const QJsonObject &status);
        void updateFocusStatus(const QJsonObject &status);
        void updateGuideStatus(const QJsonObject &status);
        void updateDomeStatus(const QJsonObject &status);
        void updateCapStatus(const QJsonObject &status);

        // Send devices as they come
        void sendEvent(const QString &message, KSNotification::EventType event);
        void sendCameras();
        void sendMounts();
        void sendScopes();
        void sendFilterWheels();
        void sendDomes();
        void sendCaps();
        void sendDrivers();
        void sendDevices();

    signals:
        void connected();
        void disconnected();
        void expired();
        void optionsChanged(QMap<int, bool> options);
        // This is forward signal from INDI::CCD
        //void previewJPEGGenerated(const QString &previewJPEG, QJsonObject metadata);

        void resetPolarView();

    public slots:
        void connectServer();
        void disconnectServer();

        // Connection
        void sendConnection();
        void sendModuleState(const QString &name);

        // Ekos
        void setEkosStatingStatus(Ekos::CommunicationStatus status);

        // Alignment
        void setAlignStatus(Ekos::AlignState newState);
        void setAlignSolution(const QVariantMap &solution);
        void sendAlignSettings(const QJsonObject &settings);

        // Guide
        void sendGuideSettings(const QJsonObject &settings);

        // Focus
        void sendFocusSettings(const QJsonObject &settings);

        // Polar
        void setPAHStage(Ekos::Align::PAHStage stage);
        void setPAHMessage(const QString &message);
        void setPolarResults(QLineF correctionVector, QString polarError);
        void setPAHEnabled(bool enabled);
        void setBoundingRect(QRect rect, QSize view);

        // Capture
        void sendCaptureSequence(const QJsonArray &sequenceArray);
        void sendCaptureSettings(const QJsonObject &settings);

        // DSLR
        void requestDSLRInfo(const QString &cameraName);

        // Dialogs
        void sendDialog(const QJsonObject &message);
        void processDialogResponse(const QJsonObject &payload);

        // Process properties
        void processNewNumber(INumberVectorProperty *nvp);
        void processNewText(ITextVectorProperty *tvp);
        void processNewSwitch(ISwitchVectorProperty *svp);
        void processNewLight(ILightVectorProperty *lvp);
        void processNewProperty(INDI::Property *prop);
        void processDeleteProperty(const QString &device, const QString &name);

        // StellarSolver
        void sendStellarSolverProfiles();

    private slots:

        // Connection
        void onConnected();
        void onDisconnected();
        void onError(QAbstractSocket::SocketError error);

        // Communication
        void onTextReceived(const QString &);

    private:
        // Profiles
        void sendProfiles();
        void setProfileMapping(const QJsonObject &payload);
        void sendStates();

        // Capture
        void processCaptureCommands(const QString &command, const QJsonObject &payload);
        void setCapturePresetSettings(const QJsonObject &settings);
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

        // Dome
        void processDomeCommands(const QString &command, const QJsonObject &payload);

        // Cap
        void processCapCommands(const QString &command, const QJsonObject &payload);

        // Profile
        void processProfileCommands(const QString &command, const QJsonObject &payload);

        // Options
        void processOptionsCommands(const QString &command, const QJsonObject &payload);

        // Scopes
        void processScopeCommands(const QString &command, const QJsonObject &payload);

        // DSLRs
        void processDSLRCommands(const QString &command, const QJsonObject &payload);

        // Filter Manager commands
        void processFilterManagerCommands(const QString &command, const QJsonObject &payload);

        // Low-level Device commands
        void processDeviceCommands(const QString &command, const QJsonObject &payload);

        QWebSocket m_WebSocket;
        QJsonObject m_AuthResponse;
        uint16_t m_ReconnectTries {0};
        Ekos::Manager *m_Manager { nullptr };
        QUrl m_URL;

        bool m_isConnected { false };
        bool m_sendBlobs { true};

        QMap<int, bool> m_Options;
        QMap<QString, QSet<QString>> m_PropertySubscriptions;
        QLineF correctionVector;
        QRect boundingRect;
        QSize viewSize;

        QDateTime m_ThrottleTS;

        // Retry every 5 seconds in case remote server is down
        static const uint16_t RECONNECT_INTERVAL = 5000;
        // Retry for 1 hour before giving up
        static const uint16_t RECONNECT_MAX_TRIES = 720;
        // Throttle interval
        static const uint16_t THROTTLE_INTERVAL = 5000;
};
}
