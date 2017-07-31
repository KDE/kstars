/*  General KStars Notifications for desktop and lite version
    Copyright (C) 2016 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

*/

#pragma once

#include <KLocalizedString>

#include <QString>

namespace KSNotification
{
void error(const QString &message, const QString &title = i18n("Error"));
void sorry(const QString &message, const QString &title = i18n("Sorry"));
void info(const QString &message, const QString &title = i18n("Info"));
/**
 * @brief transient Non modal message box that gets deleted on close.
 * @param message message content
 * @param title message title
 */
void transient(const QString &message, const QString &title);
}
