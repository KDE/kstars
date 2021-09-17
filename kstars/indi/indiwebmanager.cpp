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

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
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
                qDebug() << "INDI: JSon error during parsing " << parseError.errorString();
                return false;
            }
        }

        return true;
    }
    else
    {
        qDebug() << "INDI: Error communicating with INDI Web Manager: " << response->errorString();
        return false;
    }
}

bool isOnline(ProfileInfo *pi)
{
    QTimer timer;
    timer.setSingleShot(true);
    QNetworkAccessManager manager;
    QUrl url(QString("http://%1:%2/api/server/status").arg(pi->host).arg(pi->INDIWebManagerPort));
    QNetworkReply *response = manager.get(QNetworkRequest(url));

    // Wait synchronously
    QEventLoop event;
    QObject::connect(&timer, &QTimer::timeout, &event, &QEventLoop::quit);
    QObject::connect(response, SIGNAL(finished()), &event, SLOT(quit()));
    timer.start(3000);
    event.exec();

    if (timer.isActive() && response->error() == QNetworkReply::NoError)
        return true;
    // Fallback to default if DNS lookup fails for .local
    else if (pi->host.contains(".local"))
    {
        QUrl url(QString("http://10.250.250.1:8624/api/server/status"));
        QNetworkReply *response = manager.get(QNetworkRequest(url));
        // Wait synchronously
        QEventLoop event;
        QObject::connect(&timer, &QTimer::timeout, &event, &QEventLoop::quit);
        QObject::connect(response, SIGNAL(finished()), &event, SLOT(quit()));
        timer.start(3000);
        event.exec();

        if (timer.isActive() && response->error() == QNetworkReply::NoError)
        {
            pi->host = "10.250.250.1";
            return true;
        }
    }

    return false;
}

bool isStellarMate(ProfileInfo *pi)
{
    QNetworkAccessManager manager;
    QUrl url(QString("http://%1:%2/api/info/version").arg(pi->host).arg(pi->INDIWebManagerPort));

    QJsonDocument json;
    if (getWebManagerResponse(QNetworkAccessManager::GetOperation, url, &json))
    {
        QJsonObject version = json.object();
        if (version.contains("version") == false)
            return false;
        qInfo(KSTARS_EKOS) << "Detect StellarMate version" << version["version"].toString();
        return true;
    }
    return false;
}

bool syncCustomDrivers(ProfileInfo *pi)
{
    QNetworkAccessManager manager;
    QUrl url(QString("http://%1:%2/api/profiles/custom").arg(pi->host).arg(pi->INDIWebManagerPort));

    QStringList customDriversLabels;
    QMapIterator<QString, QString> i(pi->drivers);
    while (i.hasNext())
    {
        QString name       = i.next().value();
        DriverInfo *driver = DriverManager::Instance()->findDriverByName(name);

        if (driver == nullptr)
            driver = DriverManager::Instance()->findDriverByLabel(name);
        if (driver && driver->getDriverSource() == CUSTOM_SOURCE)
            customDriversLabels << driver->getLabel();
    }

    // Search for locked filter by filter color name
    const QList<QVariantMap> &customDrivers = DriverManager::Instance()->getCustomDrivers();

    for (auto label : customDriversLabels)
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

bool areDriversRunning(ProfileInfo *pi)
{
    QUrl url(QString("http://%1:%2/api/server/drivers").arg(pi->host).arg(pi->INDIWebManagerPort));
    QJsonDocument json;

    if (getWebManagerResponse(QNetworkAccessManager::GetOperation, url, &json))
    {
        QJsonArray array = json.array();

        if (array.isEmpty())
            return false;

        QStringList piExecDrivers;
        QMapIterator<QString, QString> i(pi->drivers);
        while (i.hasNext())
        {
            QString name       = i.next().value();
            DriverInfo *driver = DriverManager::Instance()->findDriverByName(name);

            if (driver == nullptr)
                driver = DriverManager::Instance()->findDriverByLabel(name);
            if (driver)
                piExecDrivers << driver->getExecutable();
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

bool syncProfile(ProfileInfo *pi)
{
    QUrl url;
    QJsonDocument jsonDoc;
    QByteArray data;

    //Add Profile
    url = QUrl(QString("http://%1:%2/api/profiles/%3").arg(pi->host).arg(pi->INDIWebManagerPort).arg(pi->name));
    getWebManagerResponse(QNetworkAccessManager::PostOperation, url, nullptr);

    // Update profile info
    url = QUrl(QString("http://%1:%2/api/profiles/%3").arg(pi->host).arg(pi->INDIWebManagerPort).arg(pi->name));
    QJsonObject profileObject{ { "port", pi->port } };
    jsonDoc = QJsonDocument(profileObject);
    data    = jsonDoc.toJson();
    getWebManagerResponse(QNetworkAccessManager::PutOperation, url, nullptr, &data);

    // Add drivers
    url = QUrl(QString("http://%1:%2/api/profiles/%3/drivers").arg(pi->host).arg(pi->INDIWebManagerPort).arg(pi->name));
    QJsonArray driverArray;
    QMapIterator<QString, QString> i(pi->drivers);

    // In case both Guider + CCD are Multiple-Devices-Per-Driver type
    // Then we should not define guider as a separate driver since that would start the driver executable twice
    // when we only need it once
    if (pi->drivers.contains("Guider"))
    {
        if (pi->drivers["Guider"] == pi->drivers["CCD"])
        {
            DriverInfo *guiderInfo = nullptr;
            if ((guiderInfo = DriverManager::Instance()->findDriverByName(pi->drivers["Guider"])) == nullptr)
            {
                if ((guiderInfo = DriverManager::Instance()->findDriverByLabel(pi->drivers["Guider"])) == nullptr)
                {
                    guiderInfo = DriverManager::Instance()->findDriverByExec(pi->drivers["Guider"]);
                }
            }

            if (guiderInfo && guiderInfo->getAuxInfo().value("mdpd", false).toBool())
            {
                pi->drivers.remove("Guider");
                i = QMapIterator<QString, QString>(pi->drivers);
            }
        }
    }

    // Regular Drivers
    while (i.hasNext())
        driverArray.append(QJsonObject({{"label", i.next().value()}}));

    // Remote Drivers
    if (pi->remotedrivers.isEmpty() == false)
    {
        for (auto remoteDriver : pi->remotedrivers.split(","))
        {
            driverArray.append(QJsonObject({{"remote", remoteDriver}}));
        }
    }

    data    = QJsonDocument(driverArray).toJson();
    getWebManagerResponse(QNetworkAccessManager::PostOperation, url, nullptr, &data);

    return true;
}

bool startProfile(ProfileInfo *pi)
{
    // First make sure profile is created and synced on web manager
    syncProfile(pi);

    // Start profile
    QUrl url(QString("http://%1:%2/api/server/start/%3").arg(pi->host).arg(pi->INDIWebManagerPort).arg(pi->name));
    getWebManagerResponse(QNetworkAccessManager::PostOperation, url, nullptr);

    // Make sure drivers are running
    // Try up to 3 times
    for (int i = 0; i < 3; i++)
    {
        if (areDriversRunning(pi))
            return true;
    }

    return false;
}

bool stopProfile(ProfileInfo *pi)
{
    // Stop profile
    QUrl url(QString("http://%1:%2/api/server/stop").arg(pi->host).arg(pi->INDIWebManagerPort));
    return getWebManagerResponse(QNetworkAccessManager::PostOperation, url, nullptr);
}

bool restartDriver(ProfileInfo *pi, const QString &label)
{
    QUrl url(QString("http://%1:%2/api/drivers/restart/%3").arg(pi->host).arg(pi->INDIWebManagerPort).arg(label));
    return getWebManagerResponse(QNetworkAccessManager::PostOperation, url, nullptr);
}
}

// Async version of the Web Manager
namespace AsyncWebManager
{

QFuture<bool> isOnline(ProfileInfo *pi)
{
    return QtConcurrent::run(WebManager::isOnline, pi);
}

QFuture<bool> isStellarMate(ProfileInfo *pi)
{
    return QtConcurrent::run(WebManager::isStellarMate, pi);
}

QFuture<bool> syncCustomDrivers(ProfileInfo *pi)
{
    return QtConcurrent::run(WebManager::syncCustomDrivers, pi);
}

QFuture<bool> areDriversRunning(ProfileInfo *pi)
{
    return QtConcurrent::run(WebManager::areDriversRunning, pi);
}

QFuture<bool> syncProfile(ProfileInfo *pi)
{
    return QtConcurrent::run(WebManager::syncProfile, pi);
}

QFuture<bool> startProfile(ProfileInfo *pi)
{
    return QtConcurrent::run(WebManager::startProfile, pi);
}

QFuture<bool> stopProfile(ProfileInfo *pi)
{
    return QtConcurrent::run(WebManager::stopProfile, pi);
}
}

}
