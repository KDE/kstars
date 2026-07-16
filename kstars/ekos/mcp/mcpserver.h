/*
    SPDX-FileCopyrightText: 2026 Thomas Nemer <thomas.nemer@fortytwo.fr>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QDateTime>
#include <QHash>
#include <QJsonObject>
#include <QJsonValue>
#include <QObject>
#include <QSet>
#include <QSharedPointer>
#include <QString>

class QTcpSocket;
class FITSData;

namespace ISD
{
class GenericDevice;
class Camera;
}

namespace MCP
{

class Transport;
class ToolRegistry;

class Server : public QObject
{
        Q_OBJECT

    public:
        explicit Server(QObject *parent = nullptr);

        bool start(quint16 port);
        bool restart(quint16 port);
        void stop();
        bool isListening() const;
        quint16 port() const;

        ToolRegistry *registry();

        // Latest captured image — populated by a per-camera hook on
        // ISD::Camera::newImage installed in hookCamera(). Covers every frame
        // producer (Capture queue, PAA, Focus, Align, ad-hoc camera_capture, raw
        // INDI) without requiring per-module wiring.
        struct LastImage
        {
            bool                       available = false;
            QString                    cameraName;
            QDateTime                  receivedAt;
            QString                    filter;
            QString                    target;
            QString                    dateObs;
            double                     exposure = 0.0;
            double                     ccdTemp  = 0.0;
            double                     hfr      = 0.0;
            int                        starCount = 0;
            int                        width    = 0;
            int                        height   = 0;
            // The on-disk path is not cached: ISD::Camera::newImage fires before
            // the file is written, so the FITSData's m_Filename is empty at hook
            // time. Read data->filename() at query time instead — by then the
            // consumer (Capture::cameraprocess for queue, etc.) has set it. For
            // ad-hoc / preview captures there's no disk save at all, and the
            // thumbnail tool round-trips via FITSData::saveImage() to a temp file.
            QSharedPointer<FITSData>   data;
        };
        // Most-recent frame across all cameras. Sentinel (.available == false) if
        // no frame has been received this session.
        const LastImage &lastImage() const;
        // Most-recent frame from a specific camera (matched by device name).
        // Sentinel if no frame from that camera. Empty cameraName returns the
        // same sentinel.
        const LastImage &lastImageFor(const QString &cameraName) const;

        // The in-memory cache (m_token, m_readOnlyToken) is the runtime source of
        // truth. The system keychain (via QtKeychain) is used to persist tokens
        // across launches — never the KConfig file. If the keychain backend is
        // unavailable (headless CI, missing Qt6Keychain at build, no DBus/keyring
        // service) writes fail and a new token is generated next session; the
        // current session keeps working with the in-memory value.
        QString token() const;
        QString readOnlyToken() const;
        // Set tokens explicitly. Updates the in-memory cache and the transport's
        // active credentials, and triggers a best-effort keychain persist (async,
        // errors logged via KSTARS_EKOS_MCP). Used by tests to inject known
        // values; also useful for seeding a chosen token from another source.
        void setToken(const QString &token);
        void setReadOnlyToken(const QString &token);

        void regenerateToken();
        void regenerateReadOnlyToken();

        // Test seam: turn off system-keychain persistence so unit tests never
        // read or overwrite the user's real MCP tokens (service "kstars", keys
        // "mcp_token"/"mcp_readonly_token"). Default enabled. Call once with
        // false from a test's initTestCase(); applies to every Server in the
        // process. Production code never touches this.
        static void setKeychainPersistenceEnabled(bool enabled);

    signals:
        void tokenChanged();
        void readOnlyTokenChanged();

    private slots:
        void handleRequest(QTcpSocket *socket, const QByteArray &body);

    private:
        QJsonObject makeResponse(const QJsonValue &id, const QJsonValue &result) const;
        QJsonObject makeError(const QJsonValue &id, int code, const QString &message) const;

        // Blocking keychain read via a local QEventLoop. The backend returns
        // synchronously for missing keys and within a few ms for hits, so the
        // brief block at server-start time is acceptable. Returns an empty
        // QString in all "no value" cases: key unset, backend unavailable
        // (NoBackendAvailable), or HAVE_KEYCHAIN not defined.
        static QString loadFromKeychain(const QString &key);
        // Async keychain write. Returns immediately; the job's finished signal
        // is hooked only to log failures via the KSTARS_EKOS_MCP category. The
        // caller's session is unaffected by persistence errors — the in-memory
        // cache carries the token until the next launch.
        static void storeToKeychain(const QString &key, const QString &value);

        // Gates loadFromKeychain/storeToKeychain. See setKeychainPersistenceEnabled.
        static bool s_keychainPersistenceEnabled;

        // Hook a device's camera-frame signal. The concrete ISD::Camera object
        // may not exist yet at INDIListener::newDevice time — it's created
        // asynchronously when DRIVER_INFO arrives — so we either install the
        // hook immediately if the camera is already there, or wire up
        // GenericDevice::newCamera to install it when ready.
        void hookCamera(const QSharedPointer<ISD::GenericDevice> &device);
        // Connect ISD::Camera::newImage → image-cache update. Tracked via
        // m_hookedCameras so duplicate calls (initial sweep + signal, or two
        // newCamera deliveries) don't stack listeners.
        void installImageHook(ISD::Camera *camera);

        Transport    *m_transport { nullptr };
        ToolRegistry *m_registry  { nullptr };

        QHash<QString, LastImage> m_imagesByCamera;
        QString                   m_mostRecentCamera;
        QSet<QString>             m_hookedCameras;
        LastImage                 m_emptyImage; // sentinel returned when nothing cached

        QString m_token;
        QString m_readOnlyToken;
};

} // namespace MCP
