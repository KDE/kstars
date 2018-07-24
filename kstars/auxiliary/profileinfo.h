/*  Profile Info
    Copyright (C) 2012 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

 */

#pragma once

#include <QMap>
#include <QString>
#include <QJsonObject>

class ProfileInfo
{
  public:
    ProfileInfo(int id, const QString &name);
    ~ProfileInfo() = default;

    // Is connection local or remote
    bool isLocal() { return host.isEmpty(); }
    QJsonObject toJson() const
    {
        return {{"name", name}, {"isLocal", host.isEmpty()}};
    }

    QString mount();
    QString ccd();
    QString guider();
    QString focuser();
    QString filter();
    QString dome();
    QString ao();
    QString weather();
    QString aux1();
    QString aux2();
    QString aux3();
    QString aux4();
    QString remoteDrivers();

    QString name;
    QString host;
    QString city;
    QString province;
    QString country;
    int guidertype { 0 };
    int guiderport { 0 };
    int primaryscope { 0 };
    int guidescope { 0 };
    QString remotedrivers;
    QString guiderhost;
    int id { 0 };
    int port { -1 };
    bool autoConnect { false };
    int INDIWebManagerPort { -1 };
    // driver[role] = label
    QMap<QString, QString> drivers;
};
