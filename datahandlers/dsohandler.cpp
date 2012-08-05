/***************************************************************************
                          DSOHandler.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 2012/03/08
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

#include "dsohandler.h"

bool DSOHandler::Initialize() {
    skydb_ = QSqlDatabase::addDatabase("QSQLITE", "skydb");
    QString dbfile = KStandardDirs::locateLocal("data", "skyobjects.db");
    QFile testdb(dbfile);
    bool first_run = false;
    if (!testdb.exists()) {
        kDebug()<< i18n("DSO DB does not exist!");
        first_run = true;
    }
    skydb_.setDatabaseName(dbfile);
    if (!skydb_.open()) {
           kWarning() << i18n("Unable to open user database file!");
           kWarning() << LastError();
    } else {
        kDebug() << i18n("Opened the User DB. Ready!");
        if (first_run == true) {
            //FirstRun();
        }
    }
    skydb_.close();
    return true;
}

DSOHandler::~DSOHandler() {
    skydb_.close();
    QSqlDatabase::removeDatabase("userdb");
}

QSqlError DSOHandler::LastError() {
    // error description is in QSqlError::text()
    return skydb_.lastError();
}
