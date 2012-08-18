/***************************************************************************
                catalogDB.cpp  -  K Desktop Planetarium
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

#include "catalogdb.h"

bool CatalogDB::Initialize() {
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
           kWarning() << i18n("Unable to open DSO database file!");
           kWarning() << LastError();
    } else {
        kDebug() << i18n("Opened the DSO Database. Ready!");
        if (first_run == true) {
            //FirstRun();
        }
    }
    skydb_.close();
    return true;
}

CatalogDB::~CatalogDB() {
    skydb_.close();
    QSqlDatabase::removeDatabase("skydb");
}

QSqlError CatalogDB::LastError() {
    // error description is in QSqlError::text()
    return skydb_.lastError();
}

QStringList* CatalogDB::Catalogs() {
    RefreshCatalogList();
    return &catalog_list_;
}

void CatalogDB::RefreshCatalogList()
{  
    catalog_list_.clear();
    skydb_.open();
    QSqlTableModel catalog(0, skydb_);    
    catalog.setTable("Catalog");
    catalog.select();

    for (int i =0; i < catalog.rowCount(); ++i) {
        QSqlRecord record = catalog.record(i);
        // TODO(spacetime): complete list!
        QString name = record.value("Name").toString();
        QString prefix = record.value("Prefix").toString();
        
    }

    catalog.clear();
    skydb_.close();
}
