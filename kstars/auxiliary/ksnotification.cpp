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
    void error(const QString &message, const QString &title)
    {
        #ifdef KSTARS_LITE
        Q_UNUSED(title);
        KStarsLite::Instance()->notificationMessage(message);
        #else
        KMessageBox::error(0, message, title);
        #endif
    }

    void sorry(const QString &message, const QString &title)
    {
        #ifdef KSTARS_LITE
        Q_UNUSED(title);
        KStarsLite::Instance()->notificationMessage(message);
        #else
        KMessageBox::sorry(0, message, title);
        #endif
    }

    void info(const QString &message, const QString &title)
    {        
        #ifdef KSTARS_LITE
        Q_UNUSED(title);
        KStarsLite::Instance()->notificationMessage(message);
        #else
        KMessageBox::information(0, message, title);
        #endif
    }
}
