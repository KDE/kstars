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

#include <kdebug.h>
#include <klocale.h>
#include "kstarsdata.h"
#include "ksuserdb.h"

bool KSUserDB::Initialize() {
    // Every logged in user has their own db.
    // TODO(spacetime): This needs to be merged with verification,
    // and made into a static member
    // to achieve 'aggregation' with kstarsdata
    userdb_ = QSqlDatabase::addDatabase("QSQLITE", "userdb");
    QString dbfile = KStandardDirs::locateLocal("appdata", "userdb.sqlite");
    QFile testdb(dbfile);
    bool first_run = false;
    if (!testdb.exists()) {
        kDebug()<< i18n("User DB does not exist! New User DB will be "
                          "created.");
        first_run = true;
    }
    userdb_.setDatabaseName(dbfile);
    if (!userdb_.open()) {
           kWarning() << i18n("Unable to open user database file!");
           kWarning() << LastError();
    } else {
        kDebug() << i18n("Opened the User DB. Ready!");
        if (first_run == true) {
            FirstRun();
        }
    }
    userdb_.close();
    return true;
}

KSUserDB::~KSUserDB() {
    userdb_.close();
    QSqlDatabase::removeDatabase("userdb");
}

QSqlError KSUserDB::LastError() {
    // error description is in QSqlError::text()
    return userdb_.lastError();
}

bool KSUserDB::FirstRun() {
    kWarning() << i18n("Rebuilding User Database");
    QVector<QString> tables;
    tables.append("CREATE TABLE user ( "
                  "id INTEGER DEFAULT NULL PRIMARY KEY AUTOINCREMENT, "
                  "Name TEXT NOT NULL  DEFAULT 'NULL', "
                  "Surname TEXT NOT NULL  DEFAULT 'NULL', "
                  "Contact TEXT DEFAULT NULL)");

    tables.append("CREATE TABLE telescope ( "
                  "id INTEGER DEFAULT NULL PRIMARY KEY AUTOINCREMENT, "
                  "Vendor TEXT DEFAULT NULL, "
                  "Aperture REAL NOT NULL  DEFAULT NULL, "
                  "Model TEXT DEFAULT NULL, "
                  "Driver TEXT DEFAULT NULL, "
                  "Type TEXT DEFAULT NULL, "
                  "FocalLength REAL DEFAULT NULL)");

    tables.append("CREATE TABLE flags ( "
                  "id INTEGER DEFAULT NULL PRIMARY KEY AUTOINCREMENT, "
                  "RA TEXT NOT NULL  DEFAULT NULL, "
                  "Dec TEXT NOT NULL  DEFAULT NULL, "
                  "Icon TEXT NOT NULL  DEFAULT 'NULL', "
                  "Label TEXT NOT NULL  DEFAULT 'NULL', "
                  "Color TEXT DEFAULT NULL, "
                  "Epoch TEXT DEFAULT NULL)");

    tables.append("CREATE TABLE lens ( "
                  "id INTEGER DEFAULT NULL PRIMARY KEY AUTOINCREMENT, "
                  "Vendor TEXT NOT NULL  DEFAULT 'NULL', "
                  "Model TEXT DEFAULT NULL, "
                  "Factor REAL NOT NULL  DEFAULT NULL)");

    tables.append("CREATE TABLE eyepiece ( "
                  "id INTEGER DEFAULT NULL PRIMARY KEY AUTOINCREMENT, "
                  "Vendor TEXT DEFAULT NULL, "
                  "Model TEXT DEFAULT NULL, "
                  "FocalLength REAL NOT NULL  DEFAULT NULL, "
                  "ApparentFOV REAL NOT NULL  DEFAULT NULL, "
                  "FOVUnit TEXT NOT NULL  DEFAULT NULL)");

    tables.append("CREATE TABLE filter ( "
                  "id INTEGER DEFAULT NULL PRIMARY KEY AUTOINCREMENT, "
                  "Vendor TEXT DEFAULT NULL, "
                  "Model TEXT DEFAULT NULL, "
                  "Type TEXT DEFAULT NULL, "
                  "Color TEXT DEFAULT NULL)");

    tables.append("CREATE TABLE wishlist ( "
                  "id INTEGER DEFAULT NULL PRIMARY KEY AUTOINCREMENT, "
                  "Date NUMERIC NOT NULL  DEFAULT NULL, "
                  "Type TEXT DEFAULT NULL, "
                  "UIUD TEXT DEFAULT NULL)");

    tables.append("CREATE TABLE fov ( "
                  "id INTEGER DEFAULT NULL PRIMARY KEY AUTOINCREMENT, "
                  "name TEXT NOT NULL  DEFAULT 'NULL', "
                  "color TEXT DEFAULT NULL, "
                  "sizeX NUMERIC DEFAULT NULL, "
                  "sizeY NUMERIC DEFAULT NULL, "
                  "shape TEXT DEFAULT NULL)");

    tables.append("CREATE TABLE logentry ( "
                  "id INTEGER DEFAULT NULL PRIMARY KEY AUTOINCREMENT, "
                  "content TEXT NOT NULL  DEFAULT 'NULL', "
                  "UIUD TEXT DEFAULT NULL, "
                  "DateTime NUMERIC NOT NULL  DEFAULT NULL, "
                  "User INTEGER DEFAULT NULL REFERENCES user (id), "
                  "Location TEXT DEFAULT NULL, "
                  "Telescope INTEGER DEFAULT NULL REFERENCES telescope (id),"
                  "Filter INTEGER DEFAULT NULL REFERENCES filter (id), "
                  "lens INTEGER DEFAULT NULL REFERENCES lens (id), "
                  "Eyepiece INTEGER DEFAULT NULL REFERENCES eyepiece (id), "
                  "FOV INTEGER DEFAULT NULL REFERENCES fov (id))");

    for (int i = 0; i < tables.count(); ++i) {
        QSqlQuery query(userdb_);
        if (!query.exec(tables[i])) {
            kDebug() << query.lastError();
        }
    }
    return true;
}

/*
 * Observer Section
*/
void KSUserDB::AddObserver(QString name, QString surname, QString contact) {
    userdb_.open();
    QSqlTableModel users(0, userdb_);
    users.setTable("user");
    users.setFilter("Name LIKE \'" + name + "\' AND Surname LIKE \'" +
                    surname + "\'");
    users.select();
    if (users.rowCount() > 0) {
            QSqlRecord record = users.record(0);
            record.setValue("Name", name);
            record.setValue("Surname", surname);
            record.setValue("Contact", contact);
            users.setRecord(0, record);
            users.submitAll();
    } else {
        int row = 0;
        users.insertRows(row, 1);
        users.setData(users.index(row, 1), name);  // row0 is autoincerement ID
        users.setData(users.index(row, 2), surname);
        users.setData(users.index(row, 3), contact);
        users.submitAll();
    }
    userdb_.close();
}

int KSUserDB::FindObserver(QString name, QString surname) {
    userdb_.open();
    QSqlTableModel users(0, userdb_);
    users.setTable("user");
    users.setFilter("Name LIKE \'" + name + "\' AND Surname LIKE \'" +
                    surname + "\'");
    users.select();
    int observer_count = users.rowCount();
    users.clear();
    userdb_.close();
    return (observer_count > 0);
}

// TODO(spacetime): This method is currently unused.
bool KSUserDB::DeleteObserver(QString id) {
    userdb_.open();
    QSqlTableModel users(0, userdb_);
    users.setTable("user");
    users.setFilter("id = \'"+id+"\'");
    users.select();
    users.removeRows(0, 1);
    users.submitAll();
    int observer_count = users.rowCount();
    users.clear();
    userdb_.close();
    return (observer_count > 0);
}

void KSUserDB::GetAllObservers(QList<Observer *> &observer_list) {
    userdb_.open();
    observer_list.clear();
    QSqlTableModel users(0, userdb_);
    users.setTable("user");
    users.select();
    for (int i =0; i < users.rowCount(); ++i) {
        QSqlRecord record = users.record(i);
        QString id = record.value("id").toString();
        QString name = record.value("Name").toString();
        QString surname = record.value("Surname").toString();
        QString contact = record.value("Contact").toString();
        OAL::Observer *o= new OAL::Observer(id, name, surname, contact);
        observer_list.append(o);
    }
    users.clear();
    userdb_.close();
    return;
}

/*
 * Flag Section
*/

void KSUserDB::EraseAllFlags() {
    userdb_.open();
    QSqlTableModel flags(0, userdb_);
    flags.setTable("flags");
    flags.select();
    flags.removeRows(0, flags.rowCount());
    flags.submitAll();
    flags.clear();
    userdb_.close();
}

void KSUserDB::AddFlag(QString ra, QString dec,
                       QString epoch, QString image_name,
                       QString label, QString labelColor) {
    userdb_.open();
    QSqlTableModel flags(0, userdb_);
    flags.setTable("flags");
    int row = 0;
    flags.insertRows(row, 1);
    flags.setData(flags.index(row, 1), ra);  // row,0 is autoincerement ID
    flags.setData(flags.index(row, 2), dec);
    flags.setData(flags.index(row, 3), image_name);
    flags.setData(flags.index(row, 4), label);
    flags.setData(flags.index(row, 5), labelColor);
    flags.setData(flags.index(row, 6), epoch);
    flags.submitAll();
    flags.clear();
    userdb_.close();
}

QList<QStringList> KSUserDB::ReturnAllFlags() {
    QList<QStringList> flagList;
    userdb_.open();
    QSqlTableModel flags(0, userdb_);
    flags.setTable("flags");
    flags.select();
    for (int i =0; i < flags.rowCount(); ++i) {
        QStringList flagEntry;
        QSqlRecord record = flags.record(i);
        /* flagEntry order description
         * 
        */
        flagEntry.append(record.value(1).toString());
        flagEntry.append(record.value(2).toString());
        flagEntry.append(record.value(6).toString());
        flagEntry.append(record.value(3).toString());
        flagEntry.append(record.value(4).toString());
        flagEntry.append(record.value(5).toString());
        flagList.append(flagEntry);
    }
    flags.clear();
    userdb_.close();
    return flagList;
}

/*
 * Generic Section
 */
void KSUserDB::EraseEquipment(QString type, int id) {
    userdb_.open();
    QSqlTableModel equip(0, userdb_);
    equip.setTable(type);
    equip.setFilter("id = " + QString::number(id));
    equip.select();
    equip.removeRows(0, equip.rowCount());
    equip.submitAll();
    equip.clear();
    userdb_.close();
}

void KSUserDB::EraseAllEquipment(QString type) {
    userdb_.open();
    QSqlTableModel equip(0, userdb_);
    equip.setTable(type);
    equip.setFilter("id >= 1");
    equip.select();
    equip.removeRows(0, equip.rowCount());
    equip.submitAll();
    equip.clear();
    userdb_.close();
}

/*
 * Telescope section
 */
void KSUserDB::AddScope(QString model, QString vendor, QString driver,
                       QString type, double focalLength, double aperture) {
    userdb_.open();
    QSqlTableModel equip(0, userdb_);
    equip.setTable("telescope");
    int row = 0;
    equip.insertRows(row, 1);
    equip.setData(equip.index(row, 1), vendor);  // row,0 is autoincerement ID
    equip.setData(equip.index(row, 2), aperture);
    equip.setData(equip.index(row, 3), model);
    equip.setData(equip.index(row, 4), driver);
    equip.setData(equip.index(row, 5), type);
    equip.setData(equip.index(row, 6), focalLength);
    equip.submitAll();
    equip.clear();
    userdb_.close();
}

void KSUserDB::AddScope(QString model, QString vendor, QString driver,
                        QString type, double focalLength, double aperture,
                        QString id) {
    userdb_.open();
    QSqlTableModel equip(0, userdb_);
    equip.setTable("telescope");
    equip.setFilter("id = "+id);
    equip.select();
    if (equip.rowCount() > 0) {
        QSqlRecord record = equip.record(0);
        record.setValue(1, vendor);
        record.setValue(2, aperture);
        record.setValue(3, model);
        record.setValue(4, driver);
        record.setValue(5, type);
        record.setValue(6, focalLength);
        equip.setRecord(0, record);
        equip.submitAll();
    }
    userdb_.close();
}

void KSUserDB::GetAllScopes(QList<Scope *> &scope_list) {
    userdb_.open();
    scope_list.clear();
    QSqlTableModel equip(0, userdb_);
    equip.setTable("telescope");
    equip.select();
    for (int i =0; i < equip.rowCount(); ++i) {
        QSqlRecord record = equip.record(i);
        QString id = record.value("id").toString();
        QString vendor = record.value("Vendor").toString();
        double aperture = record.value("Aperture").toDouble();
        QString model = record.value("Model").toString();
        QString driver = record.value("Driver").toString();
        QString type = record.value("Type").toString();
        double focalLength = record.value("FocalLength").toDouble();
        OAL::Scope *o= new OAL::Scope(id, model, vendor, type,
                                      focalLength, aperture);
        o->setINDIDriver(driver);
        scope_list.append(o);
    }
    equip.clear();
    userdb_.close();
}

/*
 * Eyepiece section
 */
void KSUserDB::AddEyepiece(QString vendor, QString model, double focalLength,
                           double fov, QString fovunit) {
    userdb_.open();
    QSqlTableModel equip(0, userdb_);
    equip.setTable("eyepiece");
    int row = 0;
    equip.insertRows(row, 1);
    equip.setData(equip.index(row, 1), vendor);  // row,0 is autoincerement ID
    equip.setData(equip.index(row, 2), model);
    equip.setData(equip.index(row, 3), focalLength);
    equip.setData(equip.index(row, 4), fov);
    equip.setData(equip.index(row, 5), fovunit);
    equip.submitAll();
    equip.clear();
    userdb_.close();
}

void KSUserDB::AddEyepiece(QString vendor, QString model, double focalLength,
                           double fov, QString fovunit, QString id) {
    userdb_.open();
    QSqlTableModel equip(0, userdb_);
    equip.setTable("eyepiece");
    equip.setFilter("id = " + id);
    equip.select();
    if (equip.rowCount()>0) {
        QSqlRecord record = equip.record(0);
        record.setValue(1, vendor);
        record.setValue(2, model);
        record.setValue(3, focalLength);
        record.setValue(4, fov);
        record.setValue(5, fovunit);
        equip.setRecord(0, record);
        equip.submitAll();
    }
    userdb_.close();
}

void KSUserDB::GetAllEyepieces(QList<OAL::Eyepiece *> &eyepiece_list) {
    userdb_.open();
    eyepiece_list.clear();
    QSqlTableModel equip(0, userdb_);
    equip.setTable("eyepiece");
    equip.select();
    for (int i =0; i < equip.rowCount(); ++i) {
        QSqlRecord record = equip.record(i);
        QString id = record.value("id").toString();
        QString vendor = record.value("Vendor").toString();
        QString model = record.value("Model").toString();
        double focalLength = record.value("FocalLength").toDouble();
        double fov = record.value("ApparentFOV").toDouble();
        QString fovUnit = record.value("FOVUnit").toString();
        OAL::Eyepiece *o = new OAL::Eyepiece(id, model, vendor, fov,
                                             fovUnit, focalLength);
        eyepiece_list.append(o);
    }
    equip.clear();
    userdb_.close();
}

/*
 * lens section
 */
void KSUserDB::AddLens(QString vendor, QString model, double factor) {
    userdb_.open();
    QSqlTableModel equip(0, userdb_);
    equip.setTable("lens");
    int row = 0;
    equip.insertRows(row, 1);
    equip.setData(equip.index(row, 1), vendor);  // row,0 is autoincerement ID
    equip.setData(equip.index(row, 2), model);
    equip.setData(equip.index(row, 3), factor);
    equip.submitAll();
    equip.clear();
    userdb_.close();
}

void KSUserDB::AddLens(QString vendor, QString model,
                       double factor, QString id) {
    userdb_.open();
    QSqlTableModel equip(0, userdb_);
    equip.setTable("lens");
    equip.setFilter("id = "+id);
    equip.select();
    if (equip.rowCount()>0) {
        QSqlRecord record = equip.record(0);
        record.setValue(1, vendor);
        record.setValue(2, model);
        record.setValue(3, factor);
        equip.submitAll();
    }
    userdb_.close();
}

void KSUserDB::GetAllLenses(QList<OAL::Lens *> &lens_list) {
    userdb_.open();
    lens_list.clear();
    QSqlTableModel equip(0, userdb_);
    equip.setTable("lens");
    equip.select();
    for (int i =0; i < equip.rowCount(); ++i) {
        QSqlRecord record = equip.record(i);
        QString id = record.value("id").toString();
        QString vendor = record.value("Vendor").toString();
        QString model = record.value("Model").toString();
        double factor = record.value("Factor").toDouble();
        OAL::Lens *o = new OAL::Lens(id, model, vendor, factor);
        lens_list.append(o);
    }
    equip.clear();
    userdb_.close();
}

/*
 *  filter section
 */
void KSUserDB::AddFilter(QString vendor, QString model,
                         QString type, QString color) {
    userdb_.open();
    QSqlTableModel equip(0, userdb_);
    equip.setTable("filter");
    int row = 0;
    equip.insertRows(row, 1);
    equip.setData(equip.index(row, 1), vendor);  // row,0 is autoincerement ID
    equip.setData(equip.index(row, 2), model);
    equip.setData(equip.index(row, 3), type);
    equip.setData(equip.index(row, 4), color);
    equip.submitAll();
    equip.clear();
    userdb_.close();
}

void KSUserDB::AddFilter(QString vendor, QString model, QString type,
                         QString color, QString id) {
    userdb_.open();
    QSqlTableModel equip(0, userdb_);
    equip.setTable("filter");
    equip.setFilter("id = " + id);
    equip.select();
    if (equip.rowCount() > 0) {
        QSqlRecord record = equip.record(0);
        record.setValue(1, vendor);
        record.setValue(2, model);
        record.setValue(3, type);
        record.setValue(4, color);
        equip.submitAll();
    }
    userdb_.close();
}

void KSUserDB::GetAllFilters(QList<OAL::Filter *> &filter_list) {
    userdb_.open();
    filter_list.clear();
    QSqlTableModel equip(0, userdb_);
    equip.setTable("filter");
    equip.select();
    for (int i =0; i < equip.rowCount(); ++i) {
        QSqlRecord record = equip.record(i);
        QString id = record.value("id").toString();
        QString vendor = record.value("Vendor").toString();
        QString model = record.value("Model").toString();
        QString type = record.value("Type").toString();
        QString color = record.value("Color").toString();
        OAL::Filter *o= new OAL::Filter(id, model, vendor, type, color);
        filter_list.append(o);
    }
    equip.clear();
    userdb_.close();
    return;
}
