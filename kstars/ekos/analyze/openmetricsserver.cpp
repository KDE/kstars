/*
    SPDX-FileCopyrightText: 2026 Eric Dejouhanet <eric.dejouhanet@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "openmetricsserver.h"

#include <QDebug>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>

#include <utility>

namespace Ekos
{

OpenMetricsServer::OpenMetricsServer(RenderFunction renderer, QObject *parent)
    : QObject(parent), m_renderer(std::move(renderer)), m_server(new QTcpServer(this))
{
    m_server->setMaxPendingConnections(MAX_CONNECTIONS);
    connect(m_server, &QTcpServer::newConnection, this, &OpenMetricsServer::acceptConnections);
}

OpenMetricsServer::~OpenMetricsServer()
{
    stop();
}

bool OpenMetricsServer::start(const QHostAddress &address, quint16 port, QString *error)
{
    if (m_server->isListening())
        m_server->close();
    if (!m_server->listen(address, port))
    {
        if (error)
            *error = m_server->errorString();
        qWarning() << "OpenMetrics endpoint failed to listen on" << address << port << m_server->errorString();
        return false;
    }
    qInfo() << "OpenMetrics endpoint listening on" << m_server->serverAddress() << m_server->serverPort();
    return true;
}

bool OpenMetricsServer::restart(const QHostAddress &address, quint16 port, QString *error)
{
    if (!isListening())
        return start(address, port, error);
    if (this->address() == address && this->port() == port)
        return true;

    const QHostAddress previousAddress = this->address();
    const quint16 previousPort = this->port();
    QString newError;
    if (start(address, port, &newError))
        return true;

    QString rollbackError;
    if (!start(previousAddress, previousPort, &rollbackError))
        newError += QStringLiteral("; rollback failed: ") + rollbackError;
    if (error)
        *error = newError;
    return false;
}

void OpenMetricsServer::stop()
{
    m_server->close();
    const auto sockets = m_requests.keys();
    for (auto *socket : sockets)
    {
        socket->abort();
        socket->deleteLater();
    }
    m_requests.clear();
}

bool OpenMetricsServer::isListening() const
{
    return m_server->isListening();
}

QHostAddress OpenMetricsServer::address() const
{
    return m_server->serverAddress();
}

quint16 OpenMetricsServer::port() const
{
    return m_server->serverPort();
}

void OpenMetricsServer::acceptConnections()
{
    while (m_server->hasPendingConnections())
    {
        QTcpSocket *socket = m_server->nextPendingConnection();
        if (m_requests.size() >= MAX_CONNECTIONS)
        {
            connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            respond(socket, 503, "Service Unavailable", "text/plain; charset=utf-8", "busy\n");
            continue;
        }

        m_requests.insert(socket, QByteArray());
        connect(socket, &QTcpSocket::readyRead, this, &OpenMetricsServer::readRequest);
        connect(socket, &QTcpSocket::disconnected, this, &OpenMetricsServer::removeConnection);

        auto *timer = new QTimer(socket);
        timer->setSingleShot(true);
        connect(timer, &QTimer::timeout, socket, [this, socket]()
        {
            if (m_requests.contains(socket))
            {
                respond(socket, 408, "Request Timeout", "text/plain; charset=utf-8", "request timeout\n");
                m_requests.remove(socket);
            }
        });
        timer->start(REQUEST_TIMEOUT_MS);
    }
}

void OpenMetricsServer::readRequest()
{
    auto *socket = qobject_cast<QTcpSocket *>(sender());
    if (!socket || !m_requests.contains(socket))
        return;

    QByteArray &request = m_requests[socket];
    request += socket->readAll();
    if (request.size() > MAX_REQUEST_BYTES)
    {
        respond(socket, 431, "Request Header Fields Too Large", "text/plain; charset=utf-8", "request too large\n");
        m_requests.remove(socket);
        return;
    }

    const int end = request.indexOf("\r\n\r\n");
    if (end < 0)
        return;
    const QByteArray completeRequest = request.left(end + 4);
    m_requests.remove(socket);
    processRequest(socket, completeRequest);
}

void OpenMetricsServer::removeConnection()
{
    auto *socket = qobject_cast<QTcpSocket *>(sender());
    if (!socket)
        return;
    m_requests.remove(socket);
    socket->deleteLater();
}

void OpenMetricsServer::processRequest(QTcpSocket *socket, const QByteArray &request)
{
    const int lineEnd = request.indexOf("\r\n");
    const QList<QByteArray> parts = request.left(lineEnd).split(' ');
    if (lineEnd < 0 || parts.size() != 3 || !parts[2].startsWith("HTTP/1."))
    {
        respond(socket, 400, "Bad Request", "text/plain; charset=utf-8", "bad request\n");
        return;
    }

    const QByteArray method = parts[0];
    const QByteArray path = parts[1].split('?').first();
    if (method != "GET" && method != "HEAD")
    {
        respond(socket, 405, "Method Not Allowed", "text/plain; charset=utf-8", "method not allowed\n",
                false, "Allow: GET, HEAD\r\n");
        return;
    }
    const bool headOnly = method == "HEAD";

    if (path == "/metrics")
    {
        const QByteArray body = m_renderer ? m_renderer() : QByteArray("# EOF\n");
        respond(socket, 200, "OK", "application/openmetrics-text; version=1.0.0; charset=utf-8", body, headOnly);
    }
    else if (path == "/healthz")
        respond(socket, 200, "OK", "text/plain; charset=utf-8", "ok\n", headOnly);
    else
        respond(socket, 404, "Not Found", "text/plain; charset=utf-8", "not found\n", headOnly);
}

void OpenMetricsServer::respond(QTcpSocket *socket, int status, const QByteArray &reason,
                                const QByteArray &contentType, const QByteArray &body,
                                bool headOnly, const QByteArray &extraHeaders)
{
    if (!socket)
        return;
    QByteArray response = "HTTP/1.1 " + QByteArray::number(status) + ' ' + reason + "\r\n";
    response += "Content-Type: " + contentType + "\r\n";
    response += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
    response += "Connection: close\r\n";
    response += extraHeaders;
    response += "\r\n";
    if (!headOnly)
        response += body;
    socket->write(response);
    socket->disconnectFromHost();
}

} // namespace Ekos
