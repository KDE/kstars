/*
    SPDX-FileCopyrightText: 2026 Thomas Nemer <thomas.nemer@fortytwo.fr>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "mcpserver.h"
#include "mcptransport.h"
#include "mcptoolregistry.h"
#include "ekos_mcp_debug.h"
#include "Options.h"
#include "fitsviewer/fitsdata.h"
#include "indi/indicamera.h"
#include "indi/indilistener.h"
#include "indi/indistd.h"

#include <basedevice.h>

#include <config-kstars.h>

#ifdef HAVE_KEYCHAIN
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <qt6keychain/keychain.h>
#else
#include <qt5keychain/keychain.h>
#endif
#endif

#include <QDateTime>
#include <QDir>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QTcpSocket>
#include <QUuid>
#include <QVariant>

namespace MCP
{

// Service name shared with EkosLive (ekosliveclient.cpp). Per-feature key
// strings ("mcp_token", "mcp_readonly_token") disambiguate inside this service.
static const QLatin1String KEYCHAIN_SERVICE("kstars");
static const QLatin1String KEY_TOKEN("mcp_token");
static const QLatin1String KEY_RO_TOKEN("mcp_readonly_token");

bool Server::s_keychainPersistenceEnabled = true;

void Server::setKeychainPersistenceEnabled(bool enabled)
{
    s_keychainPersistenceEnabled = enabled;
}

QString Server::loadFromKeychain(const QString &key)
{
    // Test seam: never read the user's real credential store under test.
    if (!s_keychainPersistenceEnabled)
        return QString();
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
    // Test seam: never overwrite the user's real credential store under test.
    if (!s_keychainPersistenceEnabled)
        return;
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

    // Hook every camera's newImage signal so the image cache sees frames
    // from every workflow (Capture queue, PAA, Focus, Align, ad-hoc
    // camera_capture, raw INDI control). Existing cameras need an initial
    // sweep; future cameras come in via INDIListener::newDevice.
    if (auto *listener = INDIListener::Instance())
    {
        connect(listener, &INDIListener::newDevice, this, &Server::hookCamera);
        for (const auto &dev : INDIListener::devicesByInterface(INDI::BaseDevice::CCD_INTERFACE))
            hookCamera(dev);
    }
}

void Server::hookCamera(const QSharedPointer<ISD::GenericDevice> &device)
{
    if (!device) return;
    // INDIListener::newDevice fires when the GenericDevice is created — before
    // DRIVER_INFO arrives and the concrete ISD::Camera is constructed and added
    // to m_ConcreteDevices. If getCamera() returns null here, install the hook
    // via GenericDevice::newCamera so it lands the moment the camera is
    // registered. Otherwise the cache stays empty for the entire session.
    if (auto *cam = device->getCamera())
    {
        installImageHook(cam);
        return;
    }
    connect(device.data(), &ISD::GenericDevice::newCamera, this,
            [this](ISD::Camera * cam)
    {
        installImageHook(cam);
    });
}

void Server::installImageHook(ISD::Camera *camera)
{
    if (!camera) return;
    const QString cameraName = camera->getDeviceName();
    // installImageHook may be reached more than once for the same camera (e.g.
    // initial sweep + GenericDevice::newCamera). The set deduplicates so we
    // don't stack newImage listeners, which would write the same frame into
    // the cache twice and double the workload of every image_last_* call.
    if (m_hookedCameras.contains(cameraName)) return;
    m_hookedCameras.insert(cameraName);

    connect(camera, &ISD::Camera::newImage, this,
            [this, camera](const QSharedPointer<FITSData> &data, const QString &)
    {
        if (!data) return;
        const QString cameraName = camera->getDeviceName();

        // Ensure the FITSData has an on-disk path. Capture's post-save
        // pipeline (cameraprocess.cpp setFilename) eventually overwrites
        // this with the real sequence path, but ad-hoc captures and PAA
        // frames never run that pipeline and otherwise leave filename
        // empty — which would suppress image_last_info.path and the
        // thumbnail's disk-render fast path. Persist to a per-camera
        // temp file once so every producer has a stable path.
        if (data->filename().isEmpty())
        {
            QString safeName = cameraName;
            safeName.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_-]")),
                             QStringLiteral("_"));
            QString tempPath = QDir::temp().filePath(
                                   QStringLiteral("mcp_cache_%1.fits").arg(safeName));
            if (data->saveImage(tempPath))
                data->setFilename(tempPath);
        }

        LastImage img;
        img.available  = true;
        img.cameraName = cameraName;
        img.receivedAt = QDateTime::currentDateTimeUtc();
        img.hfr        = data->getHFR();
        img.starCount  = data->getStarCenters().size();
        img.width      = data->width();
        img.height     = data->height();
        img.data       = data;

        QVariant v;
        if (data->getRecordValue(QStringLiteral("EXPTIME"),  v)) img.exposure = v.toDouble();
        if (data->getRecordValue(QStringLiteral("OBJECT"),   v)) img.target   = v.toString();
        if (data->getRecordValue(QStringLiteral("DATE-OBS"), v)) img.dateObs  = v.toString();
        if (data->getRecordValue(QStringLiteral("CCD-TEMP"), v)) img.ccdTemp  = v.toDouble();
        if (data->getRecordValue(QStringLiteral("FILTER"),   v)) img.filter   = v.toString();

        m_imagesByCamera.insert(cameraName, img);
        m_mostRecentCamera = cameraName;
    });
}

const Server::LastImage &Server::lastImage() const
{
    if (m_mostRecentCamera.isEmpty()) return m_emptyImage;
    auto it = m_imagesByCamera.constFind(m_mostRecentCamera);
    return it == m_imagesByCamera.constEnd() ? m_emptyImage : it.value();
}

const Server::LastImage &Server::lastImageFor(const QString &cameraName) const
{
    if (cameraName.isEmpty()) return m_emptyImage;
    auto it = m_imagesByCamera.constFind(cameraName);
    return it == m_imagesByCamera.constEnd() ? m_emptyImage : it.value();
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
        result["tools"] = m_registry->toolsList(Options::mCPDisabledTools());
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

        // Disabled-tool enforcement: user unchecked the tool in the Ekos
        // settings tree. Checked before the read-only gate.
        if (Options::mCPDisabledTools().contains(toolName))
        {
            m_transport->sendResponse(socket,
                                      QJsonDocument(makeError(id, -32601,
                                                    QStringLiteral("Tool '%1' is disabled").arg(toolName)))
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
