/***************************************************************************
                          ksuserdb.cpp  -  K Desktop Planetarium
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

#include "kstarsdata.h"
#include "ksuserdb.h"


KSUserDB::KSUserDB()
{
    userdb = QSqlDatabase::addDatabase("QSQLITE");
}

KSUserDB* KSUserDB::pinstance = 0;

bool KSUserDB::loadDatabase(QString dbfile = KSTARS_USERDB)
{
    userdb.setDatabaseName(dbfile);
    if (!db.open()) {
           kDebug() << i18n("Unable to open database file!");
           firstRun();
           return false;
    }
    return true;
}

bool KSUserDB::initialize(){
    return loadDatabase();

}

bool KSUserDB::firstRun(){
    return false;
}

bool KSUserDB::createDefaultDatabase()
{

    QSqlQuery query(userdb);

    // Enable Foreign Keys -- just to ensure integrity of the table!
    if (!query.exec("PRAGMA foreign_keys = ON")) {
        qDebug() << query.lastError();
    }

    // Disable synchronous for speed up
    if (!query.exec("PRAGMA synchronous = OFF")) {
        qDebug() << query.lastError();
    }

    return true;

}

KSUserDB* KSUserDB::Create()
{
    delete pinstance;
    pinstance = new KSUserDB();
    return pinstance;
}

KSUserDB* KSUserDB::Instance()
{
    return pinstance;
}
