/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
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
        bool isLocal()
        {
            return host.isEmpty();
        }
        QJsonObject toJson() const;

        QString mount() const;
        QString ccd() const;
        QString guider() const;
        QString focuser() const;
        QString filter() const;
        QString dome() const;
        QString ao() const;
        QString weather() const;
        QString aux1() const;
        QString aux2() const;
        QString aux3() const;
        QString aux4() const;
        QString remoteDrivers() const;

        QString name;
        QString host;
        QString city;
        QString province;
        QString country;
        int guidertype { 0 };
        int guiderport { 0 };
        int primaryscope { 0 };
        int guidescope { 0 };
        int indihub { 0 };
        QString remotedrivers;
        QString guiderhost;
        int id { 0 };
        int port { -1 };
        bool autoConnect { false };
        bool portSelector {false};
        bool isStellarMate { false };
        int INDIWebManagerPort { -1 };
        // driver[role] = label
        QMap<QString, QString> drivers;
};
