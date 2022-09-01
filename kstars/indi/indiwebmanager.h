/*
    SPDX-FileCopyrightText: 2016 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QNetworkAccessManager>
#include <QFuture>

class QByteArray;
class QJsonDocument;
class QUrl;

class ProfileInfo;

namespace INDI
{
namespace WebManager
{
bool getWebManagerResponse(QNetworkAccessManager::Operation operation, const QUrl &url, QJsonDocument *reply, QByteArray *data = nullptr);
bool isOnline(const QSharedPointer<ProfileInfo> &pi);
bool checkVersion(const QSharedPointer<ProfileInfo> &pi);
bool syncCustomDrivers(const QSharedPointer<ProfileInfo> &pi);
bool areDriversRunning(const QSharedPointer<ProfileInfo> &pi);
bool startProfile(const QSharedPointer<ProfileInfo> &pi);
bool stopProfile(const QSharedPointer<ProfileInfo> &pi);
bool restartDriver(const QSharedPointer<ProfileInfo> &pi, const QString &label);
}

namespace AsyncWebManager
{
QFuture<bool> isOnline(const QSharedPointer<ProfileInfo> &pi);
QFuture<bool> isStellarMate(const QSharedPointer<ProfileInfo> &pi);
QFuture<bool> syncCustomDrivers(const QSharedPointer<ProfileInfo> &pi);
QFuture<bool> areDriversRunning(const QSharedPointer<ProfileInfo> &pi);
QFuture<bool> startProfile(const QSharedPointer<ProfileInfo> &pi);
QFuture<bool> stopProfile(const QSharedPointer<ProfileInfo> &pi);
}

}
