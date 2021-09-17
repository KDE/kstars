/*
    SPDX-FileCopyrightText: 2001 Heiko Evermann <heiko@evermann.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kstarssplash.h"

#include <KLocalizedString>
#include <QStandardPaths>
#include "kspaths.h"

KStarsSplash::KStarsSplash(const QString &customMessage) : QSplashScreen(QPixmap())
{
    /*Background for kstars.png is called "Venus and The Night Sky Over Mammoth"(https://www.flickr.com/photos/newdimensionfilms/7108632527)
     *It was provided by John Lemieux (https://www.flickr.com/photos/newdimensionfilms/) and  is licensed under CC BY 2.0 (https://creativecommons.org/licenses/by/2.0/)*/
    setPixmap(KSPaths::locate(QStandardPaths::AppDataLocation, "kstars.png"));
    setMessage(customMessage.isEmpty() ? i18n("Welcome to KStars. Please stand by while loading...") : customMessage);
}

void KStarsSplash::setMessage(const QString &message)
{
    showMessage(message, Qt::AlignBottom | Qt::AlignHCenter, Qt::lightGray);
}
