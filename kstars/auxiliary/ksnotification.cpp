/*  General KStars Notifications for desktop and lite version
    Copyright (C) 2016 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

*/

#include "ksnotification.h"
#include "config-kstars.h"

#ifdef KSTARS_LITE
#include "kstarslite.h"
#else
#include <QPointer>
#include <KMessageBox>
#include <KNotification>

#ifdef HAVE_INDI
#ifdef HAVE_CFITSIO
#include "kstars.h"
#include "ekos/ekosmanager.h"
#endif
#endif
#endif // KSTARS_LITE

namespace KSNotification
{
void error(const QString &message, const QString &title)
{
#ifdef KSTARS_LITE
    Q_UNUSED(title);
    KStarsLite::Instance()->notificationMessage(message);
#else
    KMessageBox::error(nullptr, message, title);
#endif
}

void sorry(const QString &message, const QString &title)
{
#ifdef KSTARS_LITE
    Q_UNUSED(title);
    KStarsLite::Instance()->notificationMessage(message);
#else
    KMessageBox::sorry(nullptr, message, title);
#endif
}

void info(const QString &message, const QString &title)
{
#ifdef KSTARS_LITE
    Q_UNUSED(title);
    KStarsLite::Instance()->notificationMessage(message);
#else
    KMessageBox::information(nullptr, message, title);
#endif
}

void transient(const QString &message, const QString &title)
{
#ifdef KSTARS_LITE
    Q_UNUSED(title);
    KStarsLite::Instance()->notificationMessage(message);
#else
    QPointer<QMessageBox> msgBox = new QMessageBox();
    msgBox->setAttribute(Qt::WA_DeleteOnClose);
    msgBox->setWindowTitle(title);
    msgBox->setText(message);
    msgBox->setModal(false);
    msgBox->setIcon(QMessageBox::Warning);
    msgBox->show();
#endif
}

void event(const QLatin1String &name, const QString &message, EventType type)
{
    Q_UNUSED(name);
    Q_UNUSED(message);
    Q_UNUSED(type);
#ifndef KSTARS_LITE
    KNotification::event(name, message);

#ifdef HAVE_INDI
#ifdef HAVE_CFITSIO
    EkosManager *manager = KStars::Instance()->ekosManager();
    if (manager)
        manager->announceEvent(message, type);
#endif
#endif

#endif
}

}
