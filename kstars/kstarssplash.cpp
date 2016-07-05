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


KStarsSplash::KStarsSplash(const QString& customMessage )
        : QSplashScreen(QPixmap() )
{
    setPixmap(KSPaths::locate(QStandardPaths::GenericDataLocation, "kstars.png"));
    setMessage( customMessage.isEmpty() ?
                      i18n( "Welcome to KStars. Please stand by while loading..." ) :
                      customMessage);
}

KStarsSplash::~KStarsSplash() {
}

void KStarsSplash::setMessage(const QString& message) {
    showMessage( message, Qt::AlignLeft, Qt::lightGray);
}


