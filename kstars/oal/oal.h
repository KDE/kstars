/***************************************************************************
                          oal.h  -  description

                             -------------------
    begin                : Friday June 19, 2009
    copyright            : (C) 2009 by Prakash Mohan
    email                : prakash.mohan@kdemail.net
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef OAL_H_
#define OAL_H_

#include <QString>
#include <KLocale>
#include <kmessagebox.h>
namespace OAL {
    class Log;
    class Observer;
    class Observation;
    class Equipment;
    class Eyepiece;
    class Scope;
    class Filter;
    class Imager;
    class Site;
    class Session;
    class Target;
    class Lens;
    inline int warningOverwrite( QString message ) {
        return KMessageBox::warningYesNo( 0, message, xi18n("Overwrite"), KGuiItem(xi18n("Overwrite")), KGuiItem(xi18n("Cancel")) );
    }
}
#endif
