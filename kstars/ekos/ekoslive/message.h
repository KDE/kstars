/*
    SPDX-FileCopyrightText: 2018 Jasem Mutlaq <mutlaqja@ikarustech.com>

    Message Channel

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QtWebSockets/QWebSocket>
#include <memory>

#include "ekos/ekos.h"
#include "ekos/align/polaralignmentassistant.h"
#include "ekos/manager.h"
#include "catalogsdb.h"
#include "nodemanager.h"
#include <QQueue>

namespace EkosLive
{
class Message : public QObject
{
        Q_OBJECT

    public:
        explicit Message(Ekos::Manager *manager, QVector<QSharedPointer<NodeManager>> &nodeManagers);
        virtual ~Message() = default;

        bool isConnected() const;

        struct PendingProperty
        {
            QString device;
            QString name;
            bool operator==(const PendingProperty &other) const
            {
                return device == other.device && name == other.name;
            }
        };

        // Module Status Updates
        void updateMountStatus(const QJsonObject &status, bool throttle = false);
        void updateCaptureStatus(const QJsonObject &status);
        void updateFocusStatus(const QJsonObject &status);
        void updateGuideStatus(const QJsonObject &status);
        void updateDomeStatus(const QJsonObject &status);
        void updateCapStatus(const QJsonObject &status);
        void updateAlignStatus(const QJsonObject &status);

        // Send devices as they come
        void sendEvent(const QString &message, KSNotification::EventSource source, KSNotification::EventType event);
        void sendScopes();
        void sendDSLRLenses();
        void sendDrivers();
        void sendDevices();
        void sendTrains();
        void sendTrainProfiles();

        // Scheduler
        void sendSchedulerJobs();
        void sendSchedulerJobList(QJsonArray jobsList);
        void sendSchedulerStatus(const QJsonObject &status);

    signals:
        void connected();
        void disconnected();
        void globalLogoutTriggered(const QUrl &url);
        void optionsUpdated();
        void resetPolarView();

    public slots:
        // Connection
        void sendConnection();
        void sendModuleState(const QString &name);
        void setPendingPropertiesEnabled(bool enabled);

        // Ekos
        void setEkosStatingStatus(Ekos::CommunicationStatus status);

        // INDI
        void setINDIStatus(Ekos::CommunicationStatus status);

        // Alignment
        void setAlignStatus(Ekos::AlignState newState);
        void setAlignSolution(const QVariantMap &solution);
        void sendAlignSettings(const QVariantMap &settings);

        // Guide
        void sendGuideSettings(const QVariantMap &settings);

        // Focus
        void sendFocusSettings(const QVariantMap &settings);

        // Mount
        void sendMountSettings(const QVariantMap &settings);

        // Dark Library
        void sendDarkLibrarySettings(const QVariantMap &settings);

        //Scheduler
        void sendSchedulerSettings(const QVariantMap &settings);

        // Polar
        void setPAHStage(Ekos::PolarAlignmentAssistant::Stage stage);
        void setPAHMessage(const QString &message);
        void setPolarResults(QLineF correctionVector, double polarError, double azError, double altError);
        void setUpdatedErrors(double total, double az, double alt);
        void setPAHEnabled(bool enabled);
        void setBoundingRect(QRect rect, QSize view, double currentZoom);

        // Capture
        void sendCaptureSequence(const QJsonArray &sequenceArray);
        void sendPreviewLabel(const QString &preview);
        void sendCaptureSettings(const QVariantMap &settings);

        // Focus
        void autofocusAborted();

        // DSLR
        void requestDSLRInfo(const QString &cameraName);

        // Port Selection
        void requestPortSelection(bool show);

        // Trains
        void requestOpticalTrains(bool show);

        // Dialogs
        void sendDialog(const QJsonObject &message);
        void processDialogResponse(const QJsonObject &payload);

        // Process properties
        void processNewProperty(INDI::Property prop);
        void processDeleteProperty(INDI::Property prop);
        void processUpdateProperty(INDI::Property prop);

        // Process message
        void processMessage(const QSharedPointer<ISD::GenericDevice> &device, int id);

        // StellarSolver
        void sendStellarSolverProfiles();

        void sendManualRotatorStatus(double currentPA, double targetPA, double threshold);

    private slots:

        // Connection
        void onConnected();
        void onDisconnected();

        // Communication
        void onTextReceived(const QString &);

    private:
        // Profiles
        void sendProfiles();
        void setProfileMapping(const QJsonObject &payload);
        void sendStates();

        // Capture
        void processCaptureCommands(const QString &command, const QJsonObject &payload);
        void sendTemperature(double value);

        // Mount
        void processMountCommands(const QString &command, const QJsonObject &payload);

        // Focus
        void processFocusCommands(const QString &command, const QJsonObject &payload);

        // Guide
        void processGuideCommands(const QString &command, const QJsonObject &payload);

        // Align
        void processAlignCommands(const QString &command, const QJsonObject &payload);

        // Scheduler
        void processSchedulerCommands(const QString &command, const QJsonObject &payload);

        // Polar
        void processPolarCommands(const QString &command, const QJsonObject &payload);

        // Profile
        void processProfileCommands(const QString &command, const QJsonObject &payload);

        // Options
        void processOptionsCommands(const QString &command, const QJsonObject &payload);

        // Scopes
        void processScopeCommands(const QString &command, const QJsonObject &payload);

        // DSLRs
        void processDSLRCommands(const QString &command, const QJsonObject &payload);

        // Trains
        void processTrainCommands(const QString &command, const QJsonObject &payload);

        // Filter Manager commands
        void processFilterManagerCommands(const QString &command, const QJsonObject &payload);

        // Dark Library commands
        void processDarkLibraryCommands(const QString &command, const QJsonObject &payload);

        // Low-level Device commands
        void processDeviceCommands(const QString &command, const QJsonObject &payload);

        // Process Astronomy Library command
        void processAstronomyCommands(const QString &command, const QJsonObject &payload);

        // Process file commands
        void processFileCommands(const QString &command, const QJsonObject &payload);

        void dispatchDebounceQueue();

        KStarsDateTime getNextDawn();

        void sendResponse(const QString &command, const QJsonObject &payload);
        void sendResponse(const QString &command, const QJsonArray &payload);
        void sendResponse(const QString &command, const QString &payload);
        void sendResponse(const QString &command, bool payload);

        void sendPendingProperties();

        typedef struct
        {
            int number_integer;
            uint number_unsigned_integer;
            double number_double;
            bool boolean;
            QString text;
            QUrl url;
        } SimpleTypes;

        QObject *findObject(const QString &name);
        void invokeMethod(QObject *context, const QJsonObject &payload);
#if QT_VERSION >= QT_VERSION_CHECK(6, 2, 0)
        bool parseArgument(QVariant::Type type, const QVariant &arg, QMetaMethodArgument &genericArg, SimpleTypes &types);
#else
        bool parseArgument(QVariant::Type type, const QVariant &arg, QGenericArgument &genericArg, SimpleTypes &types);
#endif
        Ekos::Manager *m_Manager { nullptr };
        QVector<QSharedPointer<NodeManager>> m_NodeManagers;

        bool m_sendBlobs { true};

        QMap<QString, QSet<QString>> m_PropertySubscriptions;
        QLineF correctionVector;
        QRect m_BoundingRect;
        QSize m_ViewSize;
        double m_CurrentZoom {100};

        QSet<PendingProperty> m_PendingProperties;
        QTimer m_PendingPropertiesTimer;
        QTimer m_DebouncedSend;
        QMap<QString, QVariantMap> m_DebouncedMap;

        QDateTime m_ThrottleTS;
        CatalogsDB::DBManager m_DSOManager;

        typedef enum
        {
            North,
            East,
            South,
            West,
            All
        } Direction;

        // Throttle interval
        static const uint16_t THROTTLE_INTERVAL = 1000;
};

inline uint qHash(const Message::PendingProperty &key, uint seed = 0) noexcept
{
    return qHash(key.device, seed) ^ qHash(key.name, seed);
}

}
