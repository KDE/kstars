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

// Replacing bool KSUserDB::initialize(QString dbfile = KSTARS_USERDB)
// Every logged in user should have their own db.

bool KSUserDB::initialize() {
    //TODO: This needs to be merged with verification, and made into a static member
    //to achieve 'aggregation' with kstarsdata
    userdb = QSqlDatabase::addDatabase("QSQLITE", "userdb");
    QString dbfile = KStandardDirs::locateLocal( "appdata", "userdb.sqlite" );
    QFile testdb(dbfile);
    bool first = false;
    if (!testdb.exists()){
        kWarning() << i18n("User DB does not exist! New User DB will be created.");
        first = true;
    }
    userdb.setDatabaseName(dbfile);
    if (!userdb.open()) {
           kWarning() << i18n("Unable to open user database file!");
           kWarning() << lastError();
    }
    else {
        kWarning() << i18n("Opened the User DB. Ready!");
    }
    if (first == true){
        firstRun();
    }
    userdb.close();

    return true;
}

void KSUserDB::deallocate() {
    userdb.close();
    QSqlDatabase::removeDatabase("userdb");
}

QSqlError KSUserDB::lastError() {
    //error description is in QSqlError::text()
    return userdb.lastError();
}



bool KSUserDB::firstRun() {
    kWarning() << i18n("Rebuilding User Database");
    QVector<QString> tables;
    tables.append("CREATE TABLE user (\
    id INTEGER DEFAULT NULL PRIMARY KEY AUTOINCREMENT,\
    Name TEXT NOT NULL  DEFAULT 'NULL',\
    Surname TEXT NOT NULL  DEFAULT 'NULL',\
    Contact TEXT DEFAULT NULL)");

    tables.append("CREATE TABLE telescope (\
    id INTEGER DEFAULT NULL PRIMARY KEY AUTOINCREMENT,\
    Vendor TEXT DEFAULT NULL,\
    Aperture REAL NOT NULL  DEFAULT NULL,\
    Model TEXT DEFAULT NULL,\
    Driver TEXT DEFAULT NULL,\
    Type TEXT DEFAULT NULL,\
    FocalLength REAL DEFAULT NULL)");

    tables.append("CREATE TABLE flags (\
    id INTEGER DEFAULT NULL PRIMARY KEY AUTOINCREMENT,\
    RA TEXT NOT NULL  DEFAULT NULL,\
    Dec TEXT NOT NULL  DEFAULT NULL,\
    Icon TEXT NOT NULL  DEFAULT 'NULL',\
    Label TEXT NOT NULL  DEFAULT 'NULL',\
    Color TEXT DEFAULT NULL,\
    Epoch TEXT DEFAULT NULL)");

    tables.append("CREATE TABLE lens (\
    id INTEGER DEFAULT NULL PRIMARY KEY AUTOINCREMENT,\
    Vendor TEXT NOT NULL  DEFAULT 'NULL',\
    Model TEXT DEFAULT NULL,\
    Factor REAL NOT NULL  DEFAULT NULL)");

    tables.append("CREATE TABLE eyepiece (\
    id INTEGER DEFAULT NULL PRIMARY KEY AUTOINCREMENT,\
    Vendor TEXT DEFAULT NULL,\
    Model TEXT DEFAULT NULL,\
    FocalLength REAL NOT NULL  DEFAULT NULL,\
    ApparentFOV REAL NOT NULL  DEFAULT NULL)");

    tables.append("CREATE TABLE filter (\
    id INTEGER DEFAULT NULL PRIMARY KEY AUTOINCREMENT,\
    Vendor TEXT DEFAULT NULL,\
    Model TEXT DEFAULT NULL,\
    Type TEXT DEFAULT NULL,\
    Color TEXT DEFAULT NULL)");

    tables.append("CREATE TABLE wishlist (\
    id INTEGER DEFAULT NULL PRIMARY KEY AUTOINCREMENT,\
    Date NUMERIC NOT NULL  DEFAULT NULL,\
    Type TEXT DEFAULT NULL,\
    UIUD TEXT DEFAULT NULL)");

    tables.append("CREATE TABLE fov (\
    id INTEGER DEFAULT NULL PRIMARY KEY AUTOINCREMENT,\
    name TEXT NOT NULL  DEFAULT 'NULL',\
    color TEXT DEFAULT NULL,\
    sizeX NUMERIC DEFAULT NULL,\
    sizeY NUMERIC DEFAULT NULL,\
    shape TEXT DEFAULT NULL)");

    tables.append("CREATE TABLE logentry (\
    id INTEGER DEFAULT NULL PRIMARY KEY AUTOINCREMENT,\
    content TEXT NOT NULL  DEFAULT 'NULL',\
    UIUD TEXT DEFAULT NULL,\
    DateTime NUMERIC NOT NULL  DEFAULT NULL,\
    User INTEGER DEFAULT NULL REFERENCES user (id),\
    Location TEXT DEFAULT NULL,\
    Telescope INTEGER DEFAULT NULL REFERENCES telescope (id),\
    Filter INTEGER DEFAULT NULL REFERENCES filter (id),\
    lens INTEGER DEFAULT NULL REFERENCES lens (id),\
    Eyepiece INTEGER DEFAULT NULL REFERENCES eyepiece (id),\
    FOV INTEGER DEFAULT NULL REFERENCES fov (id))");

    tables.append("CREATE TABLE colorscheme (\
    id INTEGER DEFAULT NULL PRIMARY KEY AUTOINCREMENT)");
    
    for (int i=0; i<tables.count(); i++){
        QSqlQuery query(userdb);
        if (!query.exec(tables[i])) {
            kWarning() << query.lastError();
        }
    }
    return true;

}

/*
 * Observer Section
*/

bool KSUserDB::addObserver(QString name, QString surname, QString contact) {

    userdb.open();
    QSqlTableModel users(0,userdb);
    users.setTable("user");
    users.setFilter("Name LIKE \'"+name+"\' AND Surname LIKE \'"+surname+"\'");
    users.select();
    if (users.rowCount()>0) {
            QSqlRecord record = users.record(0);
            record.setValue("Name",name);
            record.setValue("Surname",surname);
            record.setValue("Contact",contact);
            users.setRecord(0,record);
            users.submitAll();
    }
    else {
        int row=0;
        users.insertRows(row,1);
        users.setData(users.index(row,1),name); //row,0 is autoincerement ID
        users.setData(users.index(row,2),surname);
        users.setData(users.index(row,3),contact);
        users.submitAll();
    }
    
    userdb.close();

    return true;    
}

int KSUserDB::findObserver(QString name, QString surname) {    
    userdb.open();
    QSqlTableModel users(0,userdb);
    users.setTable("user");
    users.setFilter("Name LIKE \'"+name+"\' AND Surname LIKE \'"+surname+"\'");
    users.select();
    int observer_count = users.rowCount();
    users.clear();
    userdb.close();
    return (observer_count>0);
}

//Returns 0 if id invalid. Else deletes the user with provided ID
//TODO: This method is currently unused. Find and link it where needed.
bool KSUserDB::delObserver(QString id) {    
    userdb.open();
    QSqlTableModel users(0,userdb);
    users.setTable("user");
    users.setFilter("id = "+id+"\'");
    users.select();
    users.removeRows(0,1);
    users.submitAll();
    int observer_count = users.rowCount();
    users.clear();
    userdb.close();
    return (observer_count>0);
}

//Clears and then populates M_observerList from UserDB
void KSUserDB::getAllObservers(QList<OAL::Observer *> &m_observerList) {

    userdb.open();
    m_observerList.clear();
    QSqlTableModel users(0,userdb);
    users.setTable("user");
    users.setFilter("id >= 1");
    users.select();
    for (int i =0; i < users.rowCount(); ++i) {
        QSqlRecord record = users.record(i);
        QString id = record.value("id").toString();
        QString name = record.value("Name").toString();
        QString surname = record.value("Surname").toString();
        QString contact = record.value("Contact").toString();
        OAL::Observer *o= new OAL::Observer( id, name, surname, contact );
        m_observerList.append( o );
    }
    users.clear();
    userdb.close();
    return;
}

/*
 * Note to future spacetime/whoever decides to clean up
 * In the current implementation as of June 2012, though a number of bugs and crashes
 * are prevented by porting the user data to a database, there is still a lot to gain
 * from this move. For this, it is needed that we change a how data is being saved and
 * retrieved at each point eg slotSaveScope etc. than in the end.
 * May the force be with you.
 */

/*
 * Flag Section
*/

void KSUserDB::eraseAllFlags() {    
    userdb.open();
    QSqlTableModel flags(0,userdb);
    flags.setTable("flags");
    flags.setFilter("id >= 1");
    flags.select();
    flags.removeRows(0,flags.rowCount());
    flags.submitAll();
    flags.clear();
    userdb.close();
}

bool KSUserDB::addFlag(QString ra, QString dec, QString epoch, 
                       QString imageName, QString label, QString labelColor) {
    
    userdb.open();
    QSqlTableModel flags(0,userdb);
    flags.setTable("flags");
    int row = 0;
    flags.insertRows(row,1);
    flags.setData(flags.index(row,1),ra); //row,0 is autoincerement ID
    flags.setData(flags.index(row,2),dec);
    flags.setData(flags.index(row,3),imageName);
    flags.setData(flags.index(row,4),label);
    flags.setData(flags.index(row,5),labelColor);
    flags.setData(flags.index(row,6),epoch);
    flags.submitAll();
    flags.clear();
    userdb.close();
    return true;    
}

//Return QList of QStringList. Order;QString ra, QString dec, QString epoch, 
//                     QString imageName, QString label, QString labelColor
QList<QStringList> KSUserDB::getAllFlags() {
    QList<QStringList> flagList;
    userdb.open();
    QSqlTableModel flags(0,userdb);
    flags.setTable("flags");
    flags.setFilter("id >= 1");
    flags.select();
    for (int i =0; i < flags.rowCount(); ++i) {
        QStringList flagEntry;
        QSqlRecord record = flags.record(i);
        flagEntry.append(record.value(1).toString());
        flagEntry.append(record.value(2).toString());
        flagEntry.append(record.value(6).toString());
        flagEntry.append(record.value(3).toString());
        flagEntry.append(record.value(4).toString());
        flagEntry.append(record.value(5).toString());
        flagList.append(flagEntry);
    }
    flags.clear();
    userdb.close();
    return flagList;
}

/*
 * Generic Section
 */

void KSUserDB::eraseEquipment(QString type, int id) {    
    userdb.open();
    QSqlTableModel equip(0,userdb);
    equip.setTable(type);
    equip.setFilter("id = "+QString::number(id));
    equip.select();
    equip.removeRows(0,equip.rowCount());
    equip.submitAll();
    equip.clear();
    userdb.close();
}

void KSUserDB::eraseAllEquipment(QString type) {    
    userdb.open();
    QSqlTableModel equip(0,userdb);
    equip.setTable(type);
    equip.setFilter("id >= 1");
    equip.select();
    equip.removeRows(0,equip.rowCount());
    equip.submitAll();
    equip.clear();
    userdb.close();
}

/*
 * Telescope section
 */
bool KSUserDB::addScope(QString model, QString vendor, QString driver,
                       QString type, double focalLength, double aperture) {
    
    userdb.open();
    QSqlTableModel equip(0,userdb);
    equip.setTable("telescope");
    int row = 0;
    equip.insertRows(row,1);
    equip.setData(equip.index(row,1),vendor); //row,0 is autoincerement ID
    equip.setData(equip.index(row,2),aperture);
    equip.setData(equip.index(row,3),model);
    equip.setData(equip.index(row,4),driver);
    equip.setData(equip.index(row,5),type);
    equip.setData(equip.index(row,6),focalLength);
    equip.submitAll();
    equip.clear();
    userdb.close();
    return true;    
}

bool KSUserDB::addScope(QString model, QString vendor, QString driver,
                       QString type, double focalLength, double aperture, QString id) {
    userdb.open();
    QSqlTableModel equip(0,userdb);
    equip.setTable("telescope");
    equip.setFilter("id = "+id);
    equip.select();
    if (equip.rowCount()>0) {
        QSqlRecord record = equip.record(0);
        record.setValue(1,vendor);
        record.setValue(2,aperture);
        record.setValue(3,model);
        record.setValue(4,driver);
        record.setValue(5,type);
        record.setValue(3,focalLength);
        equip.setRecord(0,record);
        equip.submitAll();
    }
    userdb.close();
    return true;
    
}

void KSUserDB::getAllScopes(QList< Scope* >& m_scopeList) {
    userdb.open();
    m_scopeList.clear();
    QSqlTableModel equip(0,userdb);
    equip.setTable("telescope");
    equip.setFilter("2=2"); //dummy filter. no filter=SEGFAULT
    equip.select();
    for (int i =0; i < equip.rowCount(); ++i) {
        QSqlRecord record = equip.record(i);
        QString id = record.value("id").toString();
        QString vendor = record.value("Vendor").toString();
        QString aperture = record.value("Aperture").toString();
        QString model = record.value("Model").toString();
        QString driver = record.value("Driver").toString();
        QString type = record.value("Type").toString();
        QString focalLength = record.value("FocalLength").toString();
        OAL::Scope *o= new OAL::Scope( id, model, vendor, type, focalLength.toDouble(), aperture.toDouble() );
        o->setINDIDriver(driver);
        m_scopeList.append( o );
    }
    equip.clear();
    userdb.close();
    return;

}