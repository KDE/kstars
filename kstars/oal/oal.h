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
#include <QLocale>
#ifndef KSTARS_LITE
#include <kmessagebox.h>
#endif
#include <KLocalizedString>
#include <KStandardGuiItem>

/**
 * @namespace OAL
 *
 * Open Astronomy Log (OAL) is a free and open XML schema definition for all kinds of astronomical observations.
 * KStars supports this schema and enables an observer to share observations with other observers or move observations among software products.
 *
 * The Schema was developed by the German "Fachgruppe für Computerastronomie" (section for computerastronomy) which is a subsection of Germany's largest
 * astronomy union (VDS - Vereinigung der Sternfreunde e.V.)
 */
namespace OAL
{
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
    inline int warningOverwrite( QString message )
    {
    #ifndef KSTARS_LITE
        return KMessageBox::warningYesNo( 0, message, xi18n("Overwrite"), KStandardGuiItem::overwrite(), KStandardGuiItem::cancel() );
    #else
        return 0;
    #endif
    }
}
#endif
