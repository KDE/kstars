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


// Replacing bool KSUserDB::loadDatabase(QString dbfile = KSTARS_USERDB)
// Every logged in user should have their own db.
bool KSUserDB::verifyDatabase() {
    userdb = QSqlDatabase::database("userdb");
    //db.open() will create the db if it doesn't exist. Hence, we check if it does.
    //If it doesn't exist we run firstRun() to set up tables after db.open() creates the new db file.
    QString dbfile = KStandardDirs::locateLocal( "appdata", "userdb.sqlite" );
    QFile testdb(dbfile);
    bool first = false;
    if (!testdb.exists()){
        kWarning() << i18n("User DB does not exist! New User DB will be created.");
        first = true;
    }
    
    if (!userdb.open()) {
           kWarning() << i18n("Unable to open user database file!");
           kWarning() << lastError();
    }
    else {
        kDebug() << i18n("Opened the User DB. Ready!");
    }

    if (first == true){
        firstRun();
    }
    userdb.close();
    return true;
    
}

bool KSUserDB::initialize() {
    userdb = QSqlDatabase::addDatabase("QSQLITE", "userdb");
    QString dbfile = KStandardDirs::locateLocal( "appdata", "userdb.sqlite" );
    userdb.setDatabaseName(dbfile);
    return verifyDatabase();
}

void KSUserDB::deallocate(){
    userdb = QSqlDatabase::database("userdb");
    userdb.close();
    QSqlDatabase::removeDatabase("userdb");
}

QSqlError KSUserDB::lastError() {
    //error description is in QSqlError::text()
    return userdb.lastError();
    }

bool KSUserDB::firstRun() {
    QSqlQuery query(userdb);
    if (!query.exec("CREATE TABLE users (name TEXT,surname TEXT,contact TEXT)")) {
        kDebug() << query.lastError();
    }
    
    if (!query.exec("CREATE TABLE flags (ra FLOAT,dec FLOAT,epoch INTEGER,imagename TEXT,label TEXT,labelcolor TEXT)")) {
        kDebug() << query.lastError();
    }
     //TODO: Init other tables
     
    return true;

}

bool addObserver(QString name, QString surname, QString contact) {
    
return true;    
}