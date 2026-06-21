/*
    SPDX-FileCopyrightText: 2026 Thomas Nemer <thomas.nemer@fortytwo.fr>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QByteArray>
#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QSet>
#include <QString>
#include <QTimer>

class QTcpServer;
class QTcpSocket;

namespace MCP
{

/**
 * HTTP/SSE transport for the MCP server.
 *
 * Listens on localhost only, parses POST /mcp requests, and provides helpers
 * for sending JSON responses and SSE streams.
 */
class Transport : public QObject
{
        Q_OBJECT

    public:
        explicit Transport(QObject *parent = nullptr);
        ~Transport() override;

        bool start(quint16 port);
        void stop();
        bool isListening() const;
        quint16 serverPort() const;

        void sendResponse(QTcpSocket *socket, const QByteArray &json);
        void sendNoContent(QTcpSocket *socket);
        void sendSSEStart(QTcpSocket *socket);
        void sendSSEEvent(QTcpSocket *socket, const QString &eventType, const QByteArray &json);

        void broadcastSSEEvent(const QString &eventType, const QJsonObject &payload);

        void setToken(const QString &token);
        void setReadOnlyToken(const QString &token);
        bool isReadOnlySession(QTcpSocket *socket) const;

    signals:
        void requestReceived(QTcpSocket *socket, const QByteArray &body);
        void sseClientConnected(QTcpSocket *socket);

    private slots:
        void onNewConnection();
        void onReadyRead();
        void onSocketDisconnected();

    private:
        struct ConnectionState
        {
            QByteArray buffer;
            bool headersComplete  { false };
            int contentLength     { -1 };
            bool isSSE            { false };
            bool authenticated    { false };
            bool readOnlySession  { false };
        };

        QTcpServer *m_server { nullptr };
        QHash<QTcpSocket *, ConnectionState> m_connections;
        QSet<QTcpSocket *> m_sseClients;

        QString m_token;
        QString m_readOnlyToken;
        int m_requestCount { 0 };
        QTimer *m_rateLimitTimer { nullptr };

        void processHeaders(QTcpSocket *socket, ConnectionState &state);
        void sendErrorResponse(QTcpSocket *socket, int code, const QByteArray &message);
};

} // namespace MCP
