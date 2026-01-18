/*
    SPDX-FileCopyrightText: 2016 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "indiwebmanager.h"

#include "auxiliary/ksnotification.h"
#include "driverinfo.h"
#include "drivermanager.h"
#include "Options.h"
#include "profileinfo.h"

#include <QHostInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QTcpSocket>
#include <QtConcurrent>

#include "ekos_debug.h"

namespace INDI
{

namespace WebManager
{
bool getWebManagerResponse(QNetworkAccessManager::Operation operation, const QUrl &url, QJsonDocument *reply,
                           QByteArray *data)
{
    QNetworkAccessManager manager;
    QNetworkReply *response = nullptr;
    QNetworkRequest request;

    request.setUrl(url);

    if (data)
    {
        request.setRawHeader("Content-Type", "application/json");
        request.setRawHeader("Content-Length", QByteArray::number(data->size()));
    }

    switch (operation)
    {
        case QNetworkAccessManager::GetOperation:
            response = manager.get(request);
            break;

        case QNetworkAccessManager::PostOperation:
            if (data)
                response = manager.post(request, *data);
            else
                response = manager.post(request, QByteArray());
            break;

        case QNetworkAccessManager::DeleteOperation:
            response = manager.deleteResource(request);
            break;

        case QNetworkAccessManager::PutOperation:
            response = manager.put(request, *data);
            break;

        default:
            return false;
    }

    // Wait synchronously
    QEventLoop event;
    QObject::connect(response, SIGNAL(finished()), &event, SLOT(quit()));
    event.exec();

    if (response->error() == QNetworkReply::NoError)
    {
        if (reply)
        {
            QJsonParseError parseError;
            *reply = QJsonDocument::fromJson(response->readAll(), &parseError);

            if (parseError.error != QJsonParseError::NoError)
            {
                qDebug() << Q_FUNC_INFO << "INDI: JSon error during parsing " << parseError.errorString();
                return false;
            }
        }

        return true;
    }
    else
    {
        qDebug() << Q_FUNC_INFO << "INDI: Error communicating with INDI Web Manager: " << response->errorString();
        return false;
    }
}

// Helper function to check if a TCP port is reachable
static bool isPortReachable(const QString &host, int port, int timeoutMs)
{
    QTcpSocket socket;
    socket.connectToHost(host, port);

    if (socket.waitForConnected(timeoutMs))
    {
        socket.disconnectFromHost();
        return true;
    }

    return false;
}

// Helper function to try API connection with timeout
static bool tryApiConnection(const QString &host, int port, int timeoutMs)
{
    QTimer timer;
    timer.setSingleShot(true);
    QNetworkAccessManager manager;
    QUrl url(QString("http://%1:%2/api/server/status").arg(host).arg(port));
    QNetworkReply *response = manager.get(QNetworkRequest(url));

    // Wait synchronously
    QEventLoop event;
    QObject::connect(&timer, &QTimer::timeout, &event, &QEventLoop::quit);
    QObject::connect(response, &QNetworkReply::finished, &event, &QEventLoop::quit);
    timer.start(timeoutMs);
    event.exec();

    bool success = timer.isActive() && response->error() == QNetworkReply::NoError;

    // Clean up
    timer.stop();
    response->deleteLater();

    return success;
}

bool isOnline(const QSharedPointer<ProfileInfo> &pi)
{
    // Layer 1: DNS resolution check (fast fail on DNS issues)
    QHostInfo hostInfo = QHostInfo::fromName(pi->host);
    if (hostInfo.error() != QHostInfo::NoError)
    {
        qCDebug(KSTARS_EKOS) << "INDI: DNS lookup failed for" << pi->host << ":" << hostInfo.errorString();

        // Fallback to default IP if DNS lookup fails for .local addresses
        if (pi->host.contains(".local"))
        {
            qCDebug(KSTARS_EKOS) << "INDI: Attempting fallback to 10.250.250.1";

            // Try the fallback address
            if (isPortReachable("10.250.250.1", 8624, 1000) && tryApiConnection("10.250.250.1", 8624, 1500))
            {
                qCDebug(KSTARS_EKOS) << "INDI: Successfully connected via fallback address";
                pi->host = "10.250.250.1";
                pi->INDIWebManagerPort = 8624;
                return true;
            }
        }
        return false;
    }

    // Layer 2: TCP port check (verify port is open and accepting connections)
    if (!isPortReachable(pi->host, pi->INDIWebManagerPort, 1000))
    {
        qCDebug(KSTARS_EKOS) << "INDI: Port" << pi->INDIWebManagerPort << "not reachable on" << pi->host;

        // Quick retry for transient network issues
        qCDebug(KSTARS_EKOS) << "INDI: Retrying port check...";
        if (!isPortReachable(pi->host, pi->INDIWebManagerPort, 1500))
        {
            qCDebug(KSTARS_EKOS) << "INDI: Port check failed on retry";

            // Try fallback for .local addresses
            if (pi->host.contains(".local"))
            {
                qCDebug(KSTARS_EKOS) << "INDI: Attempting fallback to 10.250.250.1";
                if (isPortReachable("10.250.250.1", 8624, 1000) && tryApiConnection("10.250.250.1", 8624, 1500))
                {
                    qCDebug(KSTARS_EKOS) << "INDI: Successfully connected via fallback address";
                    pi->host = "10.250.250.1";
                    pi->INDIWebManagerPort = 8624;
                    return true;
                }
            }
            return false;
        }
    }

    // Layer 3: API call (verify it's actually the INDI Web Manager)
    qCDebug(KSTARS_EKOS) << "INDI: Attempting API connection to" << pi->host << ":" << pi->INDIWebManagerPort;
    if (tryApiConnection(pi->host, pi->INDIWebManagerPort, 1500))
    {
        qCDebug(KSTARS_EKOS) << "INDI: Web Manager is online at" << pi->host << ":" << pi->INDIWebManagerPort;
        return true;
    }

    // Retry once for transient failures
    qCDebug(KSTARS_EKOS) << "INDI: API call failed, retrying...";
    if (tryApiConnection(pi->host, pi->INDIWebManagerPort, 2000))
    {
        qCDebug(KSTARS_EKOS) << "INDI: Web Manager is online at" << pi->host << ":" << pi->INDIWebManagerPort;
        return true;
    }

    qCDebug(KSTARS_EKOS) << "INDI: Failed to connect to Web Manager at" << pi->host << ":" << pi->INDIWebManagerPort;
    return false;
}

bool checkVersion(const QSharedPointer<ProfileInfo> &pi)
{
    QNetworkAccessManager manager;
    QUrl url(QString("http://%1:%2/api/info/version").arg(pi->host).arg(pi->INDIWebManagerPort));

    QJsonDocument json;
    if (getWebManagerResponse(QNetworkAccessManager::GetOperation, url, &json))
    {
        QJsonObject version = json.object();
        if (version.contains("version") == false)
            return false;
        qInfo(KSTARS_EKOS) << "Detected Web Manager version" << version["version"].toString();
        return true;
    }
    return false;
}

bool syncCustomDrivers(const QSharedPointer<ProfileInfo> &pi)
{
    QNetworkAccessManager manager;
    QUrl url(QString("http://%1:%2/api/profiles/custom/add").arg(pi->host).arg(pi->INDIWebManagerPort));

    QStringList customDriversLabels;
    QMapIterator<DeviceFamily, QList<QString>> i(pi->drivers);
    while (i.hasNext())
    {
        i.next();
        for (const QString &name : i.value())
        {
            auto driver = DriverManager::Instance()->findDriverByName(name);

            if (driver.isNull())
                driver = DriverManager::Instance()->findDriverByLabel(name);
            if (driver && driver->getDriverSource() == CUSTOM_SOURCE)
                customDriversLabels << driver->getLabel();
        }
    }

    // Search for locked filter by filter color name
    const QList<QVariantMap> &customDrivers = DriverManager::Instance()->getCustomDrivers();

    for (auto &label : customDriversLabels)
    {
        auto pos = std::find_if(customDrivers.begin(), customDrivers.end(), [label](QVariantMap oneDriver)
        {
            return (oneDriver["Label"] == label);
        });

        if (pos == customDrivers.end())
            continue;

        QVariantMap driver = (*pos);
        QJsonObject jsonDriver = QJsonObject::fromVariantMap(driver);

        QByteArray data    = QJsonDocument(jsonDriver).toJson();
        getWebManagerResponse(QNetworkAccessManager::PostOperation, url, nullptr, &data);
    }

    return true;
}

bool areDriversRunning(const QSharedPointer<ProfileInfo> &pi)
{
    QUrl url(QString("http://%1:%2/api/server/drivers").arg(pi->host).arg(pi->INDIWebManagerPort));
    QJsonDocument json;

    if (getWebManagerResponse(QNetworkAccessManager::GetOperation, url, &json))
    {
        QJsonArray array = json.array();

        if (array.isEmpty())
            return false;

        QStringList piExecDrivers;
        QMapIterator<DeviceFamily, QList<QString>> i(pi->drivers);
        while (i.hasNext())
        {
            i.next();
            for (const QString &name : i.value())
            {
                auto driver = DriverManager::Instance()->findDriverByName(name);

                if (driver.isNull())
                    driver = DriverManager::Instance()->findDriverByLabel(name);
                if (driver)
                    piExecDrivers << driver->getExecutable();
            }
        }

        if (array.count() < piExecDrivers.count())
            return false;

        // Get all the drivers running remotely
        QStringList webManagerDrivers;
        for (auto value : array)
        {
            QJsonObject driver = value.toObject();
            // Old Web Manager API API
            QString exec = driver["driver"].toString();
            if (exec.isEmpty())
                // New v0.1.5+ Web Manager API
                exec = driver["binary"].toString();
            webManagerDrivers << exec;
        }

        // Make sure all the profile drivers are running there
        for (auto &oneDriverExec : piExecDrivers)
        {
            if (webManagerDrivers.contains(oneDriverExec) == false)
            {
                KSNotification::error(i18n("Driver %1 failed to start on the remote INDI server.", oneDriverExec));
                qCritical(KSTARS_EKOS) << "Driver" << oneDriverExec << "failed to start on the remote INDI server!";
                return false;
            }
        }

        return true;
    }

    return false;
}

bool syncProfile(const QSharedPointer<ProfileInfo> &pi)
{
    QUrl url;
    QJsonDocument jsonDoc;
    QJsonParseError jsonError;
    QByteArray data;
    QJsonArray profileScripts;

    //Add Profile
    url = QUrl(QString("http://%1:%2/api/profiles/%3").arg(pi->host).arg(pi->INDIWebManagerPort).arg(pi->name));
    getWebManagerResponse(QNetworkAccessManager::PostOperation, url, nullptr);

    // Parse scripts if available
    if (pi->scripts.isEmpty() == false)
    {
        auto doc = QJsonDocument::fromJson(pi->scripts, &jsonError);
        if (jsonError.error == QJsonParseError::NoError)
            profileScripts = doc.array();
    }

    // Always update profile info with port and driver_source (include scripts if parsed successfully)
    url = QUrl(QString("http://%1:%2/api/profiles/%3").arg(pi->host).arg(pi->INDIWebManagerPort).arg(pi->name));
    QJsonObject profileObject{ { "port", pi->port }, {"driver_source", pi->driverSource} };
    if (!profileScripts.isEmpty())
        profileObject["scripts"] = profileScripts;

    jsonDoc = QJsonDocument(profileObject);
    data    = jsonDoc.toJson();
    getWebManagerResponse(QNetworkAccessManager::PutOperation, url, nullptr, &data);

    // Add drivers
    url = QUrl(QString("http://%1:%2/api/profiles/%3/drivers").arg(pi->host).arg(pi->INDIWebManagerPort).arg(pi->name));
    QJsonArray driverArray;
    QMapIterator<DeviceFamily, QList<QString>> i(pi->drivers);

    // No changes needed here for now

    // Regular Drivers
    while (i.hasNext())
    {
        i.next();
        for (const QString &label : i.value())
            driverArray.append(QJsonObject({ { "label", label } }));
    }

    // Remote Drivers
    if (pi->remotedrivers.isEmpty() == false)
    {
        for (auto &remoteDriver : pi->remotedrivers.split(","))
        {
            driverArray.append(QJsonObject({{"remote", remoteDriver}}));
        }
    }

    QJsonArray sortedList;
    for (const auto &oneRule : qAsConst(profileScripts))
    {
        auto matchingDriver = std::find_if(driverArray.begin(), driverArray.end(), [oneRule](const auto & oneDriver)
        {
            return oneDriver.toObject()["label"].toString() == oneRule.toObject()["Driver"].toString();
        });

        if (matchingDriver != driverArray.end())
        {
            sortedList.append(*matchingDriver);
        }
    }

    // If we have any profile scripts drivers, let's re-sort managed drivers
    // so that profile script drivers
    if (!sortedList.isEmpty())
    {
        for (const auto oneDriver : driverArray)
        {
            if (sortedList.contains(oneDriver) == false)
                sortedList.append(oneDriver);
        }

        driverArray = sortedList;
    }

    data    = QJsonDocument(driverArray).toJson();
    return getWebManagerResponse(QNetworkAccessManager::PostOperation, url, nullptr, &data);
}

bool startProfile(const QSharedPointer<ProfileInfo> &pi)
{
    // First make sure profile is created and synced on web manager
    syncProfile(pi);

    // Start profile
    QUrl url(QString("http://%1:%2/api/server/start/%3").arg(pi->host).arg(pi->INDIWebManagerPort).arg(pi->name));
    return getWebManagerResponse(QNetworkAccessManager::PostOperation, url, nullptr);
}

bool stopProfile(const QSharedPointer<ProfileInfo> &pi)
{
    // Stop profile
    QUrl url(QString("http://%1:%2/api/server/stop").arg(pi->host).arg(pi->INDIWebManagerPort));
    return getWebManagerResponse(QNetworkAccessManager::PostOperation, url, nullptr);
}

bool restartDriver(const QSharedPointer<ProfileInfo> &pi, const QString &label)
{
    QUrl url(QString("http://%1:%2/api/drivers/restart/%3").arg(pi->host).arg(pi->INDIWebManagerPort).arg(label));
    return getWebManagerResponse(QNetworkAccessManager::PostOperation, url, nullptr);
}
}

// Async version of the Web Manager
namespace AsyncWebManager
{

QFuture<bool> isOnline(const QSharedPointer<ProfileInfo> &pi)
{
    return QtConcurrent::run(WebManager::isOnline, pi);
}

QFuture<bool> isStellarMate(const QSharedPointer<ProfileInfo> &pi)
{
    return QtConcurrent::run(WebManager::checkVersion, pi);
}

QFuture<bool> syncCustomDrivers(const QSharedPointer<ProfileInfo> &pi)
{
    return QtConcurrent::run(WebManager::syncCustomDrivers, pi);
}

QFuture<bool> areDriversRunning(const QSharedPointer<ProfileInfo> &pi)
{
    return QtConcurrent::run(WebManager::areDriversRunning, pi);
}

QFuture<bool> syncProfile(const QSharedPointer<ProfileInfo> &pi)
{
    return QtConcurrent::run(WebManager::syncProfile, pi);
}

QFuture<bool> startProfile(const QSharedPointer<ProfileInfo> &pi)
{
    return QtConcurrent::run(WebManager::startProfile, pi);
}

QFuture<bool> stopProfile(const QSharedPointer<ProfileInfo> &pi)
{
    return QtConcurrent::run(WebManager::stopProfile, pi);
}
}

}
