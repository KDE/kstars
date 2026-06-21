/*
    SPDX-FileCopyrightText: 2026 Eric Dejouhanet <eric.dejouhanet@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QByteArray>
#include <QHash>
#include <QHostAddress>
#include <QObject>

#include <functional>

class QTcpServer;
class QTcpSocket;

namespace Ekos
{

/** Minimal bounded HTTP server for the Analyze OpenMetrics endpoint. */
class OpenMetricsServer : public QObject
{
        Q_OBJECT

    public:
        using RenderFunction = std::function<QByteArray()>;

        explicit OpenMetricsServer(RenderFunction renderer, QObject *parent = nullptr);
        ~OpenMetricsServer() override;

        bool start(const QHostAddress &address, quint16 port, QString *error = nullptr);
        bool restart(const QHostAddress &address, quint16 port, QString *error = nullptr);
        void stop();
        bool isListening() const;
        QHostAddress address() const;
        quint16 port() const;

    private Q_SLOTS:
        void acceptConnections();
        void readRequest();
        void removeConnection();

    private:
        static constexpr int MAX_CONNECTIONS = 8;
        static constexpr int MAX_REQUEST_BYTES = 8192;
        static constexpr int REQUEST_TIMEOUT_MS = 5000;

        void respond(QTcpSocket *socket, int status, const QByteArray &reason,
                     const QByteArray &contentType, const QByteArray &body,
                     bool headOnly = false, const QByteArray &extraHeaders = QByteArray());
        void processRequest(QTcpSocket *socket, const QByteArray &request);

        RenderFunction m_renderer;
        QTcpServer *m_server { nullptr };
        QHash<QTcpSocket *, QByteArray> m_requests;
};

} // namespace Ekos
