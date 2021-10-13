/*
    SPDX-FileCopyrightText: 2016 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "ksnotification.h"
#include "config-kstars.h"
#include "Options.h"

#ifdef KSTARS_LITE
#include "kstarslite.h"
#else
#include <QPointer>
#include <QTimer>
#include <KNotification>

#include "ksmessagebox.h"

#ifdef HAVE_INDI
#ifdef HAVE_CFITSIO
#include "kstars.h"
#include "ekos/manager.h"
#endif
#endif
#endif // KSTARS_LITE

namespace KSNotification
{
void error(const QString &message, const QString &title, uint32_t timeout)
{
#ifdef KSTARS_LITE
    Q_UNUSED(title);
    KStarsLite::Instance()->notificationMessage(message);
#else
    KSMessageBox::Instance()->error(message, title, timeout);
#endif
}

void sorry(const QString &message, const QString &title, uint32_t timeout)
{
#ifdef KSTARS_LITE
    Q_UNUSED(title);
    KStarsLite::Instance()->notificationMessage(message);
#else
    KSMessageBox::Instance()->sorry(message, title, timeout);
#endif
}

void info(const QString &message, const QString &title, uint32_t timeout)
{
#ifdef KSTARS_LITE
    Q_UNUSED(title);
    KStarsLite::Instance()->notificationMessage(message);
#else
    KSMessageBox::Instance()->info(message, title, timeout);
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
    QTimer::singleShot(10000, msgBox, [msgBox]()
    {
        if (msgBox) msgBox->close();
    });
#endif
}

void event(const QLatin1String &name, const QString &message, EventType type)
{
    Q_UNUSED(name)
    Q_UNUSED(message)
    Q_UNUSED(type)
#ifndef KSTARS_LITE
    KNotification::event(name, message);

#ifdef HAVE_INDI
#ifdef HAVE_CFITSIO
    Ekos::Manager::Instance()->announceEvent(message, type);
#endif
#endif

#endif
}

}
