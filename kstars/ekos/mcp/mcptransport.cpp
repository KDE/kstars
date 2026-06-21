/*
    SPDX-FileCopyrightText: 2026 Thomas Nemer <thomas.nemer@fortytwo.fr>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "mcptransport.h"
#include "ekos_mcp_debug.h"

#include <QHostAddress>
#include <QJsonDocument>
#include <QTcpServer>
#include <QTcpSocket>

namespace MCP
{

Transport::Transport(QObject *parent) : QObject(parent)
{
    m_server = new QTcpServer(this);
    connect(m_server, &QTcpServer::newConnection, this, &Transport::onNewConnection);

    m_rateLimitTimer = new QTimer(this);
    m_rateLimitTimer->setInterval(10000);
    connect(m_rateLimitTimer, &QTimer::timeout, this, [this]()
    {
        m_requestCount = 0;
    });
    m_rateLimitTimer->start();
}

Transport::~Transport()
{
    stop();
}

bool Transport::start(quint16 port)
{
    if (!m_server->listen(QHostAddress::LocalHost, port))
    {
        qCWarning(KSTARS_EKOS_MCP) << "Failed to listen on port" << port << ":" << m_server->errorString();
        return false;
    }
    qCInfo(KSTARS_EKOS_MCP) << "MCP transport listening on 127.0.0.1:" << m_server->serverPort();
    return true;
}

void Transport::stop()
{
    m_server->close();
    for (auto *socket : m_connections.keys())
        socket->disconnectFromHost();
    m_connections.clear();
    m_sseClients.clear();
}

bool Transport::isListening() const
{
    return m_server->isListening();
}

quint16 Transport::serverPort() const
{
    return m_server->serverPort();
}

void Transport::setToken(const QString &token)
{
    m_token = token;
}

void Transport::setReadOnlyToken(const QString &token)
{
    m_readOnlyToken = token;
}

bool Transport::isReadOnlySession(QTcpSocket *socket) const
{
    auto it = m_connections.constFind(socket);
    if (it == m_connections.constEnd())
        return false;
    return it->readOnlySession;
}

void Transport::onNewConnection()
{
    while (m_server->hasPendingConnections())
    {
        QTcpSocket *socket = m_server->nextPendingConnection();
        m_connections.insert(socket, ConnectionState {});
        connect(socket, &QTcpSocket::readyRead, this, &Transport::onReadyRead);
        connect(socket, &QTcpSocket::disconnected, this, &Transport::onSocketDisconnected);
    }
}

void Transport::onReadyRead()
{
    auto *socket = qobject_cast<QTcpSocket *>(sender());
    if (!socket || !m_connections.contains(socket))
        return;

    ConnectionState &state = m_connections[socket];
    state.buffer.append(socket->readAll());

    if (!state.headersComplete)
        processHeaders(socket, state);

    // processHeaders may close the socket on error; guard before continuing.
    if (!m_connections.contains(socket))
        return;

    // SSE connections stay open — don't process body
    if (state.isSSE)
        return;

    if (state.headersComplete && state.contentLength >= 0
            && state.buffer.size() >= state.contentLength)
    {
        QByteArray body = state.buffer.left(state.contentLength);
        emit requestReceived(socket, body);
    }
}

void Transport::processHeaders(QTcpSocket *socket, ConnectionState &state)
{
    int headerEnd = state.buffer.indexOf("\r\n\r\n");
    if (headerEnd < 0)
        return;

    QByteArray headerSection = state.buffer.left(headerEnd);
    state.buffer = state.buffer.mid(headerEnd + 4);
    state.headersComplete = true;

    QList<QByteArray> lines = headerSection.split('\n');
    if (lines.isEmpty())
    {
        sendErrorResponse(socket, 400, "Bad Request");
        socket->disconnectFromHost();
        m_connections.remove(socket);
        return;
    }

    // Validate request line: must be POST /mcp or GET /mcp/stream
    QByteArray requestLine = lines[0].trimmed();
    QList<QByteArray> parts = requestLine.split(' ');
    if (parts.size() < 2)
    {
        sendErrorResponse(socket, 400, "Bad Request");
        socket->disconnectFromHost();
        m_connections.remove(socket);
        return;
    }

    const QByteArray method = parts[0];
    const QByteArray path   = parts[1];

    const bool isPost      = (method == "POST"  && path == "/mcp");
    const bool isSSEStream = (method == "GET"   && path == "/mcp/stream");

    if (!isPost && !isSSEStream)
    {
        sendErrorResponse(socket, 405, "Method Not Allowed");
        socket->disconnectFromHost();
        m_connections.remove(socket);
        return;
    }

    // Parse all headers in a single pass
    QString authHeader;
    int contentLength = -1;

    for (int i = 1; i < lines.size(); ++i)
    {
        QByteArray line = lines[i].trimmed();
        int colonPos = line.indexOf(':');
        if (colonPos < 0)
            continue;
        QByteArray name  = line.left(colonPos).trimmed().toLower();
        QByteArray value = line.mid(colonPos + 1).trimmed();

        if (name == "content-length")
            contentLength = value.toInt();
        else if (name == "authorization")
            authHeader = QString::fromUtf8(value);
    }

    // Auth check — primary token grants full access; read-only token grants observe-only
    if (!m_token.isEmpty())
    {
        const QString bearerFull = QStringLiteral("Bearer ") + m_token;
        const QString bearerRO   = !m_readOnlyToken.isEmpty()
                                   ? QStringLiteral("Bearer ") + m_readOnlyToken
                                   : QString();

        if (authHeader == bearerFull)
        {
            state.readOnlySession = false;
        }
        else if (!bearerRO.isEmpty() && authHeader == bearerRO)
        {
            state.readOnlySession = true;
        }
        else
        {
            QByteArray body = R"({"error":"Invalid or missing token"})";
            QByteArray response;
            response += "HTTP/1.1 401 Unauthorized\r\n";
            response += "Content-Type: application/json\r\n";
            response += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
            response += "Connection: close\r\n";
            response += "\r\n";
            response += body;
            socket->write(response);
            socket->disconnectFromHost();
            m_connections.remove(socket);
            return;
        }
    }

    // Rate limiting
    m_requestCount++;
    if (m_requestCount > 60)
    {
        QByteArray response;
        response += "HTTP/1.1 429 Too Many Requests\r\n";
        response += "Content-Length: 0\r\n";
        response += "Connection: close\r\n";
        response += "\r\n";
        socket->write(response);
        socket->disconnectFromHost();
        m_connections.remove(socket);
        return;
    }

    if (isSSEStream)
    {
        state.isSSE = true;
        sendSSEStart(socket);
        m_sseClients.insert(socket);
        emit sseClientConnected(socket);
        return;
    }

    // POST /mcp
    state.contentLength = contentLength;
    if (state.contentLength < 0)
    {
        sendErrorResponse(socket, 400, "Bad Request: missing Content-Length");
        socket->disconnectFromHost();
        m_connections.remove(socket);
    }
}

void Transport::onSocketDisconnected()
{
    auto *socket = qobject_cast<QTcpSocket *>(sender());
    if (socket)
    {
        m_sseClients.remove(socket);
        m_connections.remove(socket);
        socket->deleteLater();
    }
}

void Transport::broadcastSSEEvent(const QString &eventType, const QJsonObject &payload)
{
    QByteArray json = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    for (auto *socket : m_sseClients)
    {
        if (socket && socket->state() == QTcpSocket::ConnectedState)
            sendSSEEvent(socket, eventType, json);
    }
}

void Transport::sendResponse(QTcpSocket *socket, const QByteArray &json)
{
    QByteArray response;
    response += "HTTP/1.1 200 OK\r\n";
    response += "Content-Type: application/json\r\n";
    response += "Content-Length: " + QByteArray::number(json.size()) + "\r\n";
    response += "Connection: close\r\n";
    response += "\r\n";
    response += json;
    socket->write(response);
    socket->disconnectFromHost();
}

void Transport::sendNoContent(QTcpSocket *socket)
{
    QByteArray response;
    response += "HTTP/1.1 202 Accepted\r\n";
    response += "Content-Length: 0\r\n";
    response += "Connection: close\r\n";
    response += "\r\n";
    socket->write(response);
    socket->disconnectFromHost();
}

void Transport::sendSSEStart(QTcpSocket *socket)
{
    QByteArray response;
    response += "HTTP/1.1 200 OK\r\n";
    response += "Content-Type: text/event-stream\r\n";
    response += "Cache-Control: no-cache\r\n";
    response += "Connection: keep-alive\r\n";
    response += "\r\n";
    socket->write(response);
}

void Transport::sendSSEEvent(QTcpSocket *socket, const QString &eventType, const QByteArray &json)
{
    QByteArray frame;
    frame += "event: " + eventType.toUtf8() + "\n";
    frame += "data: " + json + "\n";
    frame += "\n";
    socket->write(frame);
    // flush() pushes the frame into the OS send buffer immediately so the client
    // can receive it without waiting for the Qt event loop's write notifier to fire.
    socket->flush();
}

void Transport::sendErrorResponse(QTcpSocket *socket, int code, const QByteArray &message)
{
    QByteArray statusText;
    switch (code)
    {
        case 400:
            statusText = "Bad Request";
            break;
        case 401:
            statusText = "Unauthorized";
            break;
        case 405:
            statusText = "Method Not Allowed";
            break;
        case 429:
            statusText = "Too Many Requests";
            break;
        default:
            statusText = "Error";
            break;
    }
    QByteArray response;
    response += "HTTP/1.1 " + QByteArray::number(code) + " " + statusText + "\r\n";
    response += "Content-Type: text/plain\r\n";
    response += "Content-Length: " + QByteArray::number(message.size()) + "\r\n";
    response += "Connection: close\r\n";
    response += "\r\n";
    response += message;
    socket->write(response);
}

} // namespace MCP
