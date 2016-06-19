#ifndef PROFILEINFO_H
#define PROFILEINFO_H

/*  Profile Info
    Copyright (C) 2012 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

 */

#include <QMap>
#include <QString>

class ProfileInfo
{
public:

    ProfileInfo(int id, const QString &name);
    ~ProfileInfo() {}

    QString name;
    QString host;

    QString city;
    QString province;
    QString country;

    int id;
    int port;
    int  INDIWebManagerPort;

    // driver[role] = label
    QMap<QString,QString> drivers;
    QString customDrivers;

    // Is connection local or remote
    bool isLocal() { return host.isEmpty(); }

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

};


#endif // PROFILEINFO_H
