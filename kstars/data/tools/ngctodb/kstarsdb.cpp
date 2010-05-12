/***************************************************************************
                          kstarsdb.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Wed May 12 2010
    copyright            : (C) 2010 by Victor Carbune
    email                : victor.carbune@kdemail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "kstarsdb.h"
//#include <klocale.h>

KStarsDB::KStarsDB()
{
    db = QSqlDatabase::addDatabase("QSQLITE");
}

KStarsDB* KStarsDB::pinstance = 0;

bool KStarsDB::loadDatabase(QString dbfile = KSTARS_DBFILE)
{
    db.setDatabaseName(dbfile);
    return db.open();
}

bool KStarsDB::createDefaultDatabase(QString dbfile)
{
    db.setDatabaseName(dbfile);
    return createDefaultDatabase();
}

bool KStarsDB::createDefaultDatabase()
{
    if (db.open() == false) {
    //      kDebug() << i18n("Unable to open database file: ");
    //      kDebug() << dbfile;
            qDebug() << "Problem!";
 
            return false;
    }

    QSqlQuery query(db);

    // Enable Foreign Keys -- just to ensure integrity of the table!
    if (!query.exec("PRAGMA foreign_keys = ON")) {
        qDebug() << query.lastError();
    }

    // Create the Catalog entity
    if (!query.exec(QString("CREATE TABLE ctg (name TEXT, source TEXT, description TEXT)"))) {
        qDebug() << query.lastError();
    }

    // Insert the NGC catalog
    if (!query.exec(QString("INSERT INTO ctg VALUES(\"NGC\", \"www.google.com\", \"Basic KStars IC Catalog\")"))) {
        qDebug() << query.lastError();
    }

    // Create the constellation entity
    if (!query.exec(QString("CREATE TABLE cnst (shortname TEXT, latinname TEXT, boundary TEXT, description TEXT)"))) {
        qDebug() << query.lastError();
    }

    // Create the Deep Sky Object entity
    if (!query.exec(QString("CREATE TABLE dso (ra FLOAT, dec FLOAT, bmag FLOAT, angsize FLOAT, dist FLOAT, type INTEGER, minor FLOAT, major FLOAT, idCNST INTEGER, ") +
                QString("FOREIGN KEY (idCNST) REFERENCES cnst(rowid))"))) {
        qDebug() << query.lastError();
    }

    // Create the Object Designation entity
    if (!query.exec(QString("CREATE TABLE od (designation TEXT, idCTG INTEGER NOT NULL, idDSO INTEGER NOT NULL, ") +
                QString("FOREIGN KEY (idCTG) REFERENCES ctg, FOREIGN KEY (idDSO) REFERENCES dso)"))) {
        qDebug() << query.lastError();
    }

    db.close();
    return true;
}



KStarsDB* KStarsDB::Create()
{
    delete pinstance;
    pinstance = new KStarsDB();
    return pinstance;
}

KStarsDB* KStarsDB::Instance()
{
    return pinstance;
}
