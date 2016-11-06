/*  General KStars Notifications for desktop and lite version
    Copyright (C) 2016 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

*/

#include "ksnotification.h"

#ifdef KSTARS_LITE
#include "kstarslite.h"
#else
#include <KMessageBox>
#endif

namespace KSNotification
{
    void error(const QString &value)
    {
        #ifdef KSTARS_LITE
        KStarsLite::Instance()->notificationMessage(value);
        #else
        KMessageBox::error(0, value);
        #endif
    }

    void sorry(const QString &value)
    {
        #ifdef KSTARS_LITE
        KStarsLite::Instance()->notificationMessage(value);
        #else
        KMessageBox::sorry(0, value);
        #endif
    }

    void info(const QString &value)
    {
        #ifdef KSTARS_LITE
        KStarsLite::Instance()->notificationMessage(value);
        #else
        KMessageBox::info(0, value);
        #endif
    }
}
