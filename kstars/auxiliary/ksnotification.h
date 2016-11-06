/*  General KStars Notifications for desktop and lite version
    Copyright (C) 2016 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

*/

#ifndef KSNotifications_H
#define KSNotifications_H

#include <QString>

namespace KSNotification
{
    void error(const QString &value);
    void sorry(const QString &value);
    void info(const QString &value);
}

#endif // KSNotifications_H
