/*
    SPDX-FileCopyrightText: 2026 Thomas Nemer <thomas.nemer@fortytwo.fr>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QJsonObject>
#include <QJsonValue>
#include <QObject>
#include <QString>

class QTcpSocket;

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

        Transport    *m_transport { nullptr };
        ToolRegistry *m_registry  { nullptr };

        QString m_token;
        QString m_readOnlyToken;
};

} // namespace MCP
