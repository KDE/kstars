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

#include <kdebug.h>
#include <klocale.h>

KSUserDB* KSUserDB::pinstance = 0;

KSUserDB::KSUserDB()
{
    userdb = QSqlDatabase::addDatabase("QSQLITE", "userdb");
    initialize();
    //TODO:     QSqlDatabase::removeDatabase("userdb"); <-find out the correct place to put this
}



// Replacing bool KSUserDB::loadDatabase(QString dbfile = KSTARS_USERDB)
// Every logged in user should have their own db.
bool KSUserDB::loadDatabase()
{
    QString dbfile = KStandardDirs::locateLocal( "appdata", "userdb.sqlite" );
    userdb.setDatabaseName(dbfile);
    if (!db.open()) {
           kWarning() << i18n("Unable to open user database file! Creating new database!");
           firstRun();
    }
    else kWarning() << i18n("Opened the User DB. Ready!");
    return true;
    //TODO:     Need to close() the connection when done
}

bool KSUserDB::initialize(){
    return loadDatabase();

}

QSqlError KSUserDB::lastError()
    {
    // error description is in QSqlError::text()
    return db.lastError();
    }

bool KSUserDB::firstRun(){
    //TODO: Touch a new file
    
    //TODO: Init tables

    return false;
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

bool KSUserDB::createDefaultDatabase()
{

    QSqlQuery query(userdb);

    // Enable Foreign Keys to ensure integrity of the table!
    if (!query.exec("PRAGMA foreign_keys = ON")) {
        qDebug() << query.lastError();
    }

    // Disable synchronous for speed up
    if (!query.exec("PRAGMA synchronous = OFF")) {
        qDebug() << query.lastError();
    }

    return true;

}
