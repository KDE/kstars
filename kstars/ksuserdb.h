/***************************************************************************
                          ksuserdb.h  -  K Desktop Planetarium
                             -------------------
    begin                : Wed May 2 2012
    copyright            : (C) 2012 by Rishab Arora
    email                : ra.rishab@gmail.com
 ***************************************************************************/


/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef KSUSERDB_H_
#define KSUSERDB_H_
#define KSTARS_USERDB "data/userdb.sqlite"
#include <QSqlDatabase>
#include <QDebug>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QSqlTableModel>
#include <QString>
#include <QHash>
#include <QSqlError>
#include <QVariant>
#include <QFile>
#include "skyobjects/skyobject.h"
#include <kstandarddirs.h>

struct stat;
class KSUserDB
{
public:
    /** Initialize KStarsDB while running splash screen
     * @return true on success
     */
    bool initialize();
    //TODO: To be called before closing the main window
    void deallocate();
    bool addObserver(QString name, QString surname, QString contact);
    bool findObserver(QString name, QString surname);    
private:
    bool verifyDatabase(QString dbfile);
    bool firstRun();
    QSqlDatabase userdb;
    QSqlError lastError();

};

#endif // KSUSERDB_H_
