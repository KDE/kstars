/*  INDI WebManager
    Copyright (C) 2016 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#pragma once

#include <QNetworkAccessManager>

class QByteArray;
class QJsonDocument;
class QUrl;

class ProfileInfo;

namespace INDI
{
namespace WebManager
{
bool getWebManagerResponse(QNetworkAccessManager::Operation operation, const QUrl &url, QJsonDocument *reply,
                           QByteArray *data = nullptr);
bool isOnline(ProfileInfo *pi);
bool syncCustomDrivers(ProfileInfo *pi);
bool areDriversRunning(ProfileInfo *pi);
bool startProfile(ProfileInfo *pi);
bool stopProfile(ProfileInfo *pi);
}
}
