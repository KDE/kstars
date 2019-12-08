/***************************************************************************
                          kstarssplash.cpp  -  description
                             -------------------
    begin                : Thu Jul 26 2001
    copyright            : (C) 2001 by Heiko Evermann
    email                : heiko@evermann.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "kstarssplash.h"

#include <KLocalizedString>
#include <QStandardPaths>
#include "kspaths.h"

KStarsSplash::KStarsSplash(const QString &customMessage) : QSplashScreen(QPixmap())
{
    /*Background for kstars.png is called "Venus and The Night Sky Over Mammoth"(https://www.flickr.com/photos/newdimensionfilms/7108632527)
     *It was provided by John Lemieux (https://www.flickr.com/photos/newdimensionfilms/) and  is licensed under CC BY 2.0 (https://creativecommons.org/licenses/by/2.0/)*/
    setPixmap(KSPaths::locate(QStandardPaths::GenericDataLocation, "kstars.png"));
    setMessage(customMessage.isEmpty() ? i18n("Welcome to KStars. Please stand by while loading...") : customMessage);
}

void KStarsSplash::setMessage(const QString &message)
{
    showMessage(message, Qt::AlignBottom | Qt::AlignHCenter, Qt::lightGray);
}
