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
bool isOnline(ProfileInfo *pi);
bool isStellarMate(ProfileInfo *pi);
bool syncCustomDrivers(ProfileInfo *pi);
bool areDriversRunning(ProfileInfo *pi);
bool startProfile(ProfileInfo *pi);
bool stopProfile(ProfileInfo *pi);
bool restartDriver(ProfileInfo *pi, const QString &label);
}

namespace AsyncWebManager
{
QFuture<bool> isOnline(ProfileInfo *pi);
QFuture<bool> isStellarMate(ProfileInfo *pi);
QFuture<bool> syncCustomDrivers(ProfileInfo *pi);
QFuture<bool> areDriversRunning(ProfileInfo *pi);
QFuture<bool> startProfile(ProfileInfo *pi);
QFuture<bool> stopProfile(ProfileInfo *pi);
}

}
