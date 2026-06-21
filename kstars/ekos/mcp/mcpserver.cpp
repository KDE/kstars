/*
    SPDX-FileCopyrightText: 2026 Thomas Nemer <thomas.nemer@fortytwo.fr>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "mcpserver.h"
#include "mcptransport.h"
#include "mcptoolregistry.h"
#include "ekos_mcp_debug.h"
#include "Options.h"

#include <config-kstars.h>

#ifdef HAVE_KEYCHAIN
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <qt6keychain/keychain.h>
#else
#include <qt5keychain/keychain.h>
#endif
#endif

#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpSocket>
#include <QUuid>

namespace MCP
{

// Service name shared with EkosLive (ekosliveclient.cpp). Per-feature key
// strings ("mcp_token", "mcp_readonly_token") disambiguate inside this service.
static const QLatin1String KEYCHAIN_SERVICE("kstars");
static const QLatin1String KEY_TOKEN("mcp_token");
static const QLatin1String KEY_RO_TOKEN("mcp_readonly_token");

QString Server::loadFromKeychain(const QString &key)
{
#ifdef HAVE_KEYCHAIN
    QKeychain::ReadPasswordJob job(KEYCHAIN_SERVICE);
    job.setAutoDelete(false);
    job.setKey(key);

    QEventLoop loop;
    QObject::connect(&job, &QKeychain::Job::finished, &loop, &QEventLoop::quit);
    job.start();
    loop.exec();

    if (job.error() == QKeychain::NoError)
        return job.textData();
    // EntryNotFound on first-run is expected; everything else is worth a log.
    if (job.error() != QKeychain::EntryNotFound)
        qCWarning(KSTARS_EKOS_MCP) << "Keychain read failed for key" << key << ":" << job.errorString();
#else
    Q_UNUSED(key)
#endif
    return QString();
}

void Server::storeToKeychain(const QString &key, const QString &value)
{
#ifdef HAVE_KEYCHAIN
    auto *job = new QKeychain::WritePasswordJob(KEYCHAIN_SERVICE);
    job->setAutoDelete(true);
    job->setKey(key);
    job->setTextData(value);
    QObject::connect(job, &QKeychain::Job::finished, [key](QKeychain::Job * j)
    {
        if (j->error() != QKeychain::NoError)
            qCWarning(KSTARS_EKOS_MCP) << "Keychain write failed for key" << key << ":" << j->errorString();
    });
    job->start();
#else
    Q_UNUSED(key)
    Q_UNUSED(value)
#endif
}

Server::Server(QObject *parent) : QObject(parent)
{
    m_transport = new Transport(this);
    m_registry  = new ToolRegistry(this);
    connect(m_transport, &Transport::requestReceived, this, &Server::handleRequest);
}

QString Server::token() const
{
    return m_token;
}

QString Server::readOnlyToken() const
{
    return m_readOnlyToken;
}

void Server::setToken(const QString &token)
{
    if (m_token == token)
        return;
    m_token = token;
    m_transport->setToken(token);
    storeToKeychain(KEY_TOKEN, token);
    emit tokenChanged();
}

void Server::setReadOnlyToken(const QString &token)
{
    if (m_readOnlyToken == token)
        return;
    m_readOnlyToken = token;
    m_transport->setReadOnlyToken(token);
    storeToKeychain(KEY_RO_TOKEN, token);
    emit readOnlyTokenChanged();
}

bool Server::start(quint16 port)
{
    if (m_token.isEmpty())
    {
        m_token = loadFromKeychain(KEY_TOKEN);
        if (m_token.isEmpty())
        {
            m_token = QUuid::createUuid().toString(QUuid::WithoutBraces);
            storeToKeychain(KEY_TOKEN, m_token);
        }
        emit tokenChanged();
    }
    m_transport->setToken(m_token);

    if (m_readOnlyToken.isEmpty())
    {
        m_readOnlyToken = loadFromKeychain(KEY_RO_TOKEN);
        if (!m_readOnlyToken.isEmpty())
            emit readOnlyTokenChanged();
    }
    if (!m_readOnlyToken.isEmpty())
        m_transport->setReadOnlyToken(m_readOnlyToken);

    return m_transport->start(port);
}

bool Server::restart(quint16 port)
{
    m_transport->stop();
    return start(port);
}

void Server::regenerateToken()
{
    const QString token = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_token = token;
    m_transport->setToken(token);
    storeToKeychain(KEY_TOKEN, token);
    emit tokenChanged();
}

void Server::regenerateReadOnlyToken()
{
    const QString token = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_readOnlyToken = token;
    m_transport->setReadOnlyToken(token);
    storeToKeychain(KEY_RO_TOKEN, token);
    emit readOnlyTokenChanged();
}

void Server::stop()
{
    m_transport->stop();
}

ToolRegistry *Server::registry()
{
    return m_registry;
}

bool Server::isListening() const
{
    return m_transport->isListening();
}

quint16 Server::port() const
{
    return m_transport->serverPort();
}

void Server::handleRequest(QTcpSocket *socket, const QByteArray &body)
{
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(body, &parseError);

    if (parseError.error != QJsonParseError::NoError || !doc.isObject())
    {
        qCWarning(KSTARS_EKOS_MCP) << "JSON parse error:" << parseError.errorString();
        m_transport->sendResponse(socket,
                                  QJsonDocument(makeError(QJsonValue(), -32700, "Parse error"))
                                  .toJson(QJsonDocument::Compact));
        return;
    }

    QJsonObject req     = doc.object();
    bool isNotification = !req.contains("id");
    QJsonValue id       = isNotification ? QJsonValue() : req["id"];

    // JSON-RPC 2.0 envelope validation: must be version "2.0" with a non-empty method
    const QString version = req["jsonrpc"].toString();
    const QString method  = req["method"].toString();
    if (version != QLatin1String("2.0") || !req.contains("method") || method.isEmpty())
    {
        if (!isNotification)
            m_transport->sendResponse(socket,
                                      QJsonDocument(makeError(id, -32600, "Invalid Request"))
                                      .toJson(QJsonDocument::Compact));
        else
            m_transport->sendNoContent(socket);
        return;
    }

    if (method == "initialize")
    {
        if (isNotification)
        {
            m_transport->sendNoContent(socket);
            return;
        }

        QJsonObject serverInfo;
        serverInfo["name"]    = "kstars-mcp";
        serverInfo["version"] = "1.0.0";

        QJsonObject capabilities;
        capabilities["tools"] = QJsonObject {};

        QJsonObject result;
        result["protocolVersion"] = "2025-03-26";
        result["serverInfo"]      = serverInfo;
        result["capabilities"]    = capabilities;

        m_transport->sendResponse(socket,
                                  QJsonDocument(makeResponse(id, result)).toJson(QJsonDocument::Compact));
    }
    else if (method == "notifications/initialized")
    {
        // Client acknowledgement — close connection without a JSON-RPC body.
        m_transport->sendNoContent(socket);
    }
    else if (method == "tools/list")
    {
        if (isNotification)
        {
            m_transport->sendNoContent(socket);
            return;
        }

        QJsonObject result;
        result["tools"] = m_registry->toolsList();
        m_transport->sendResponse(socket,
                                  QJsonDocument(makeResponse(id, result)).toJson(QJsonDocument::Compact));
    }
    else if (method == "tools/call")
    {
        if (isNotification)
        {
            m_transport->sendNoContent(socket);
            return;
        }

        QJsonObject params = req["params"].toObject();
        QString toolName   = params["name"].toString();
        QJsonObject args   = params["arguments"].toObject();

        // Read-only enforcement: block mutating tools for read-only sessions or server-wide RO mode
        const MCP::ToolDefinition *def = m_registry->find(toolName);
        if (!def)
        {
            m_transport->sendResponse(socket,
                                      QJsonDocument(makeError(id, -32603, QString("Tool not found: %1").arg(toolName)))
                                      .toJson(QJsonDocument::Compact));
            return;
        }

        const bool readOnlySession =
            Options::mCPReadOnlyMode() || m_transport->isReadOnlySession(socket);

        if (readOnlySession && !def->readOnly)
        {
            m_transport->sendResponse(socket,
                                      QJsonDocument(makeError(id, -32601,
                                                    QStringLiteral("Tool '%1' is not available in read-only mode").arg(toolName)))
                                      .toJson(QJsonDocument::Compact));
            return;
        }

        QString error;
        QJsonValue toolResult = m_registry->dispatch(toolName, args, error);

        if (!error.isEmpty())
        {
            m_transport->sendResponse(socket,
                                      QJsonDocument(makeError(id, -32603, error)).toJson(QJsonDocument::Compact));
            return;
        }

        QString resultText;
        if (toolResult.isObject())
            resultText = QString::fromUtf8(
                             QJsonDocument(toolResult.toObject()).toJson(QJsonDocument::Compact));
        else if (toolResult.isArray())
            resultText = QString::fromUtf8(
                             QJsonDocument(toolResult.toArray()).toJson(QJsonDocument::Compact));
        else
            resultText = toolResult.toVariant().toString();

        QJsonObject contentItem;
        contentItem["type"] = "text";
        contentItem["text"] = resultText;

        QJsonObject result;
        result["content"] = QJsonArray { contentItem };

        m_transport->sendResponse(socket,
                                  QJsonDocument(makeResponse(id, result)).toJson(QJsonDocument::Compact));
    }
    else
    {
        if (isNotification)
            m_transport->sendNoContent(socket);
        else
            m_transport->sendResponse(socket,
                                      QJsonDocument(makeError(id, -32601, "Method not found"))
                                      .toJson(QJsonDocument::Compact));
    }
}

QJsonObject Server::makeResponse(const QJsonValue &id, const QJsonValue &result) const
{
    QJsonObject response;
    response["jsonrpc"] = "2.0";
    response["id"]      = id;
    response["result"]  = result;
    return response;
}

QJsonObject Server::makeError(const QJsonValue &id, int code, const QString &message) const
{
    QJsonObject errorObj;
    errorObj["code"]    = code;
    errorObj["message"] = message;

    QJsonObject response;
    response["jsonrpc"] = "2.0";
    response["id"]      = id;
    response["error"]   = errorObj;
    return response;
}

} // namespace MCP
