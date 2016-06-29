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

#include <QStandardPaths>

#include "ksuserdb.h"
#include "kstarsdata.h"
#include "artificialhorizoncomponent.h"
#include "version.h"
#include "linelist.h"
#include "kspaths.h"

/*
 * TODO (spacetime):
 * The database supports storing logs. But it needs to be implemented.
 * 
 * One of the unresolved problems was the creation of a unique identifier
 * for each object (DSO,planet,star etc) for use in the database.
*/

bool KSUserDB::Initialize() {
    // Every logged in user has their own db.
    userdb_ = QSqlDatabase::addDatabase("QSQLITE", "userdb");
    QString dbfile = KSPaths::writableLocation(QStandardPaths::GenericDataLocation) + QDir::separator() + "userdb.sqlite";
    QFile testdb(dbfile);
    bool first_run = false;
    if (!testdb.exists()) {
        qDebug()<< "User DB does not exist. New User DB will be created.";
        first_run = true;
    }
    userdb_.setDatabaseName(dbfile);
    if (!userdb_.open())
    {
           qWarning() << "Unable to open user database file.";
           qWarning() << LastError();
           return false;
    }
    else
    {
        qDebug() << "Opened the User DB. Ready.";
        if (first_run == true)
            FirstRun();
        else
        {
            // Update table if previous version exists
            QSqlTableModel version(0, userdb_);
            version.setTable("Version");
            version.select();
            QSqlRecord record = version.record(0);
            qDebug() << " Version string is " << record.value("Version").toString();
            version.clear();

            // If prior to 2.4.0 upgrade database for horizon table
            if (record.value("Version").toString() < "2.4.0")
            {
                QSqlQuery query(userdb_);
                QString versionQuery = QString("UPDATE Version SET Version='%1'").arg(KSTARS_VERSION);

                if (!query.exec(versionQuery))
                    qDebug() << query.lastError();

                if (!query.exec("CREATE TABLE IF NOT EXISTS horizons (id INTEGER DEFAULT NULL PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL, label TEXT NOT NULL, enabled INTEGER NOT NULL)"))
                    qDebug() << query.lastError();
           }

           // If prior to 2.6.0 upgrade database for profiles tables
           if (record.value("Version").toString() < "2.6.0")
           {
                QSqlQuery query(userdb_);
                QString versionQuery = QString("UPDATE Version SET Version='%1'").arg(KSTARS_VERSION);

                if (!query.exec(versionQuery))
                    qDebug() << query.lastError();

                // Profiles
                if (!query.exec("CREATE TABLE IF NOT EXISTS profile (id INTEGER DEFAULT NULL PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL, host TEXT, port INTEGER, city TEXT, province TEXT, country TEXT, indiwebmanagerport INTEGER DEFAULT NULL)"))
                    qDebug() << query.lastError();

                // Drivers
                if (!query.exec("CREATE TABLE IF NOT EXISTS driver (id INTEGER DEFAULT NULL PRIMARY KEY AUTOINCREMENT, label TEXT NOT NULL, role TEXT NOT NULL, profile INTEGER NOT NULL, FOREIGN KEY(profile) REFERENCES profile(id))"))
                    qDebug() << query.lastError();

                // Custom Drivers
                //if (!query.exec("CREATE TABLE IF NOT EXISTS custom_driver (id INTEGER DEFAULT NULL PRIMARY KEY AUTOINCREMENT, drivers TEXT NOT NULL, profile INTEGER NOT NULL, FOREIGN KEY(profile) REFERENCES profile(id))"))
                    //qDebug() << query.lastError();

                // Add sample profile
                if (!query.exec("INSERT INTO profile (name) VALUES ('Simulators')"))
                    qDebug() << query.lastError();

                // Add sample profile drivers
                if (!query.exec("INSERT INTO driver (label, role, profile) VALUES ('Telescope Simulator', 'Mount', 1)"))
                    qDebug() << query.lastError();
                if (!query.exec("INSERT INTO driver (label, role, profile) VALUES ('CCD Simulator', 'CCD', 1)"))
                    qDebug() << query.lastError();
                if (!query.exec("INSERT INTO driver (label, role, profile) VALUES ('Focuser Simulator', 'Focuser', 1)"))
                    qDebug() << query.lastError();
           }
        }
    }
    userdb_.close();
    return true;
}

KSUserDB::~KSUserDB() {
    userdb_.close();
}

QSqlError KSUserDB::LastError() {
    // error description is in QSqlError::text()
    return userdb_.lastError();
}

bool KSUserDB::FirstRun() {
    if (!RebuildDB())
      return false;

    /*ImportFlags();
    ImportUsers();
    ImportEquipment();*/

    return true;
}


bool KSUserDB::RebuildDB() {
    qWarning() << "Rebuilding User Database";
    QVector<QString> tables;
    tables.append("CREATE TABLE Version ("
                  "Version CHAR DEFAULT NULL)");
    tables.append("INSERT INTO Version VALUES (\""  KSTARS_VERSION "\")");
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

    tables.append("CREATE TABLE horizons ( "
                  "id INTEGER DEFAULT NULL PRIMARY KEY AUTOINCREMENT, "
                  "name TEXT NOT NULL,"
                  "label TEXT NOT NULL,"
                  "enabled INTEGER NOT NULL)");

     tables.append("CREATE TABLE profile (id INTEGER DEFAULT NULL PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL, host TEXT, port INTEGER, city TEXT, province TEXT, country TEXT, indiwebmanagerport INTEGER DEFAULT NULL)");
     tables.append("CREATE TABLE driver (id INTEGER DEFAULT NULL PRIMARY KEY AUTOINCREMENT, label TEXT NOT NULL, role TEXT NOT NULL, profile INTEGER NOT NULL, FOREIGN KEY(profile) REFERENCES profile(id))");
     //tables.append("CREATE TABLE custom_driver (id INTEGER DEFAULT NULL PRIMARY KEY AUTOINCREMENT, drivers TEXT NOT NULL, profile INTEGER NOT NULL, FOREIGN KEY(profile) REFERENCES profile(id))");
     tables.append("INSERT INTO profile (name) VALUES ('Simulators')");
     tables.append("INSERT INTO driver (label, role, profile) VALUES ('Telescope Simulator', 'Mount', 1)");
     tables.append("INSERT INTO driver (label, role, profile) VALUES ('CCD Simulator', 'CCD', 1)");
     tables.append("INSERT INTO driver (label, role, profile) VALUES ('Focuser Simulator', 'Focuser', 1)");

    for (int i = 0; i < tables.count(); ++i) {
        QSqlQuery query(userdb_);
        if (!query.exec(tables[i])) {
            qDebug() << query.lastError();
        }
    }
    return true;
}

/*
 * Observer Section
*/
void KSUserDB::AddObserver(const QString& name, const QString& surname,
                           const QString& contact) {
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

int KSUserDB::FindObserver(const QString &name, const QString &surname) {
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
bool KSUserDB::DeleteObserver(const QString &id) {
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
QSqlDatabase KSUserDB::GetDatabase()
{
    userdb_.open();
    return userdb_;
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
}

/*
 * Flag Section
*/

void KSUserDB::DeleteAllFlags() {
    userdb_.open();
    QSqlTableModel flags(0, userdb_);
    flags.setEditStrategy(QSqlTableModel::OnManualSubmit);
    flags.setTable("flags");
    flags.select();

    flags.removeRows(0, flags.rowCount());
    flags.submitAll();

    flags.clear();
    userdb_.close();
}

void KSUserDB::AddFlag(const QString &ra, const QString &dec,
                       const QString &epoch, const QString &image_name,
                       const QString &label, const QString &labelColor) {
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

QList<QStringList> KSUserDB::GetAllFlags() {
    QList<QStringList> flagList;

    userdb_.open();
    QSqlTableModel flags(0, userdb_);
    flags.setTable("flags");
    flags.select();

    for (int i =0; i < flags.rowCount(); ++i) {
        QStringList flagEntry;
        QSqlRecord record = flags.record(i);
        /* flagEntry order description
         * The variation in the order is due to variation
         * in flag entry description order and flag database
         * description order.
         * flag (database): ra, dec, icon, label, color, epoch
         * flag (object):  ra, dec, epoch, icon, label, color
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
void KSUserDB::DeleteEquipment(const QString &type, const int &id) {
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

void KSUserDB::DeleteAllEquipment(const QString &type) {
    userdb_.open();
    QSqlTableModel equip(0, userdb_);
    equip.setEditStrategy(QSqlTableModel::OnManualSubmit);
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
void KSUserDB::AddScope(const QString &model, const QString &vendor,
                        const QString &driver, const QString &type,
                        const double & focalLength, const double &aperture) {
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

    equip.clear();  //DB will not close if linked object not cleared
    userdb_.close();
}

void KSUserDB::AddScope(const QString &model, const QString &vendor,
                        const QString &driver, const QString &type,
                        const double &focalLength, const double &aperture,
                        const QString &id) {
    userdb_.open();
    QSqlTableModel equip(0, userdb_);
    equip.setTable("telescope");
    equip.setFilter("id = " + id);
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
    scope_list.clear();

    userdb_.open();
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
void KSUserDB::AddEyepiece(const QString &vendor, const QString &model,
                           const double &focalLength, const double &fov,
                           const QString &fovunit) {
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

void KSUserDB::AddEyepiece(const QString &vendor, const QString &model,
                           const double &focalLength, const double &fov,
                           const QString &fovunit, const QString &id) {
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
    eyepiece_list.clear();

    userdb_.open();
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
void KSUserDB::AddLens(const QString &vendor, const QString &model,
                       const double &factor) {
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

void KSUserDB::AddLens(const QString &vendor, const QString &model,
                       const double &factor, const QString &id) {
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
    lens_list.clear();

    userdb_.open();
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
void KSUserDB::AddFilter(const QString &vendor, const QString &model,
                         const QString &type, const QString &color) {
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

void KSUserDB::AddFilter(const QString &vendor, const QString &model,
                         const QString &type, const QString &color,
                         const QString &id) {
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

#if 0
bool KSUserDB::ImportFlags() {
    QString flagfilename = KSPaths::writableLocation(QStandardPaths::GenericDataLocation) + QDir::separator() + "flags.dat";
    QFile flagsfile(flagfilename);
    if (!flagsfile.exists()) {
        return false;  // No upgrade needed. Flags file doesn't exist.
    }

    QList< QPair<QString, KSParser::DataTypes> > flag_file_sequence;
    flag_file_sequence.append(qMakePair(QString("RA"), KSParser::D_QSTRING));
    flag_file_sequence.append(qMakePair(QString("Dec"), KSParser::D_QSTRING));
    flag_file_sequence.append(qMakePair(QString("epoch"), KSParser::D_QSTRING));
    flag_file_sequence.append(qMakePair(QString("icon"), KSParser::D_QSTRING));
    flag_file_sequence.append(qMakePair(QString("label"), KSParser::D_QSTRING));
    flag_file_sequence.append(qMakePair(QString("color"), KSParser::D_QSTRING));
    KSParser flagparser(flagfilename,'#',flag_file_sequence,' ');

    QHash<QString, QVariant> row_content;
    while (flagparser.HasNextRow()){
        row_content = flagparser.ReadNextRow();
        QString ra = row_content["RA"].toString();
        QString dec = row_content["Dec"].toString();
        QString epoch = row_content["epoch"].toString();
        QString icon = row_content["icon"].toString();
        QString label = row_content["label"].toString();
        QString color = row_content["color"].toString();

        AddFlag(ra,dec,epoch,icon,label,color);
    }
    return true;
}

bool KSUserDB::ImportUsers() {
    QString usersfilename = KSPaths::writableLocation(QStandardPaths::GenericDataLocation) + QDir::separator() + "observerlist.xml";
    QFile usersfile(usersfilename);

    if (!usersfile.exists()) {
        return false;  // No upgrade needed. Users file doesn't exist.
    }

    if( ! usersfile.open( QIODevice::ReadOnly ) )
        return false;

    QXmlStreamReader *reader = new QXmlStreamReader(&usersfile);

    while( ! reader->atEnd() ) {
        reader->readNext();

        if( reader->isEndElement() )
            break;

        if( reader->isStartElement() ) {
           if (reader->name() != "observers")
                continue;

           //Read all observers
           while( ! reader->atEnd() ) {
                reader->readNext();

                if( reader->isEndElement() )
                    break;

                if( reader->isStartElement() ) {
                    // Read single observer
                    if( reader->name() == "observer" ) {
                      QString name, surname, contact;
                      while( ! reader->atEnd() ) {
                          reader->readNext();

                          if( reader->isEndElement() )
                              break;

                          if( reader->isStartElement() ) {
                              if( reader->name() == "name" ) {
                                  name = reader->readElementText();
                              } else if( reader->name() == "surname" ) {
                                  surname = reader->readElementText();
                              } else if( reader->name() == "contact" ) {
                                  contact = reader->readElementText();
                              }
                          }
                      }
                      AddObserver(name, surname, contact);
                    }
               }
            }
        }
    }    
    delete reader_;
    usersfile.close();
    return true;
}

bool KSUserDB::ImportEquipment() {
    QString equipfilename = KSPaths::writableLocation(QStandardPaths::GenericDataLocation) + QDir::separator() + "equipmentlist.xml";
    QFile equipfile(equipfilename);

    if (!equipfile.exists()) {
        return false;  // No upgrade needed. File doesn't exist.
    }

    if( ! equipfile.open( QIODevice::ReadOnly ) )
        return false;

    reader_ = new QXmlStreamReader(&equipfile);
    while( ! reader_->atEnd() ) {
        reader_->readNext();
        if( reader_->isStartElement() ) {
                while( ! reader_->atEnd() ) {
                  reader_->readNext();

                  if( reader_->isEndElement() )
                      break;

                  if( reader_->isStartElement() ) {
                    if( reader_->name() == "scopes" )
                          readScopes();
                    else if( reader_->name() == "eyepieces" )
                          readEyepieces();
                    else if( reader_->name() =="lenses" )
                          readLenses();
                    else if( reader_->name() =="filters" )
                          readFilters();
                  }
              }
        }
    }
    delete reader_;
    equipfile.close();
    return true;
}
#endif

void KSUserDB::readScopes() {
    while( ! reader_->atEnd() ) {
        reader_->readNext();

        if( reader_->isEndElement() )
            break;

        if( reader_->isStartElement() ) {
            if( reader_->name() == "scope" )
                readScope();
        }
    }
}

void KSUserDB::readEyepieces() {
    while( ! reader_->atEnd() ) {
        reader_->readNext();

        if( reader_->isEndElement() )
            break;

        if( reader_->isStartElement() ) {
            if( reader_->name() == "eyepiece" )
                readEyepiece();
        }
    }
}

void KSUserDB::readLenses() {
    while( ! reader_->atEnd() ) {
        reader_->readNext();

        if( reader_->isEndElement() )
            break;

        if( reader_->isStartElement() ) {
            if( reader_->name() == "lens" )
                readLens();
        }
    }
}

void KSUserDB::readFilters() {
    while( ! reader_->atEnd() ) {
        reader_->readNext();

        if( reader_->isEndElement() )
            break;

        if( reader_->isStartElement() ) {
            if( reader_->name() == "filter" )
                readFilter();
        }
    }
}

void KSUserDB::readScope() {
    QString model, vendor, type, driver = i18nc("No driver", "None");
    double aperture, focalLength;
    while( ! reader_->atEnd() ) {
        reader_->readNext();

        if( reader_->isEndElement() )
            break;

        if( reader_->isStartElement() ) {
            if( reader_->name() == "model" ) {
                model = reader_->readElementText();
            } else if( reader_->name() == "vendor" ) {
                vendor = reader_->readElementText() ;
            } else if( reader_->name() == "type" ) {
                type = reader_->readElementText() ;
                if( type == "N" ) type = "Newtonian";
                if( type == "R" ) type = "Refractor";
                if( type == "M" ) type = "Maksutov";
                if( type == "S" ) type = "Schmidt-Cassegrain";
                if( type == "K" ) type = "Kutter (Schiefspiegler)";
                if( type == "C" ) type = "Cassegrain";
            } else if( reader_->name() == "focalLength" ) {
                focalLength = (reader_->readElementText()).toDouble() ;
            } else if( reader_->name() == "aperture" )
                aperture = (reader_->readElementText()).toDouble() ;
              else if ( reader_->name() == "driver")
                 driver = reader_->readElementText();
        }
    }
    
    AddScope(model, vendor, driver, type, focalLength, aperture);
}

void KSUserDB::readEyepiece() {
    QString model, focalLength, vendor, fov, fovUnit;
    while( ! reader_->atEnd() ) {
        reader_->readNext();

        if( reader_->isEndElement() )
            break;

        if( reader_->isStartElement() ) {
            if( reader_->name() == "model" ) {
                model = reader_->readElementText();
            } else if( reader_->name() == "vendor" ) {
                vendor = reader_->readElementText() ;
            } else if( reader_->name() == "apparentFOV" ) {
                fov = reader_->readElementText();
                fovUnit = reader_->attributes().value( "unit" ).toString();
            } else if( reader_->name() == "focalLength" ) {
                focalLength = reader_->readElementText() ;
            }
        }
    }
    
    AddEyepiece(vendor, model, focalLength.toDouble(), fov.toDouble(), fovUnit);
}

void KSUserDB::readLens() {
    QString model, factor, vendor;
    while( ! reader_->atEnd() ) {
        reader_->readNext();

        if( reader_->isEndElement() )
            break;

        if( reader_->isStartElement() ) {
            if( reader_->name() == "model" ) {
                model = reader_->readElementText();
            } else if( reader_->name() == "vendor" ) {
                vendor = reader_->readElementText() ;
            } else if( reader_->name() == "factor" ) {
                factor = reader_->readElementText() ;
            }
        }
    }
    
    AddLens(vendor, model, factor.toDouble());
}

void KSUserDB::readFilter() {
    QString model, vendor, type, color;
    while( ! reader_->atEnd() ) {
        reader_->readNext();

        if( reader_->isEndElement() )
            break;

        if( reader_->isStartElement() ) {
            if( reader_->name() == "model" ) {
                model = reader_->readElementText();
            } else if( reader_->name() == "vendor" ) {
                vendor = reader_->readElementText() ;
            } else if( reader_->name() == "type" ) {
                type = reader_->readElementText() ;
            } else if( reader_->name() == "color" ) {
                color = reader_->readElementText() ;
            }
        }
    }
    AddFilter(vendor, model, type, color );
}

QList<ArtificialHorizonEntity *> KSUserDB::GetAllHorizons()
{
    QList<ArtificialHorizonEntity *> horizonList;

    userdb_.open();
    QSqlTableModel regions(0, userdb_);
    regions.setTable("horizons");
    regions.select();

    QSqlTableModel points(0, userdb_);

    for (int i =0; i < regions.rowCount(); ++i)
    {
        QSqlRecord record = regions.record(i);
        QString regionTable = record.value("name").toString();
        QString regionName  = record.value("label").toString();
        bool    enabled     = record.value("enabled").toInt() == 1 ? true : false;

        points.setTable(regionTable);
        points.select();

        LineList* skyList = new LineList();

        ArtificialHorizonEntity *horizon = new ArtificialHorizonEntity;

        horizon->setRegion(regionName);
        horizon->setEnabled(enabled);
        horizon->setList(skyList);

        horizonList.append(horizon);

        for (int j=0; j < points.rowCount(); j++)
        {
            record = points.record(j);

            SkyPoint *p = new SkyPoint();
            p->setAz(record.value(0).toDouble());
            p->setAlt(record.value(1).toDouble());
            p->HorizontalToEquatorial(KStarsData::Instance()->lst(), KStarsData::Instance()->geo()->lat());
            skyList->append(p);
        }

        points.clear();
    }

    regions.clear();
    userdb_.close();
    return horizonList;
}

void KSUserDB::DeleteAllHorizons()
{
    userdb_.open();
    QSqlTableModel regions(0, userdb_);
    regions.setEditStrategy(QSqlTableModel::OnManualSubmit);
    regions.setTable("horizons");
    regions.select();

    QSqlQuery query(userdb_);

    for (int i =0; i < regions.rowCount(); ++i)
    {
        QSqlRecord record = regions.record(i);
        QString tableQuery = QString("DROP TABLE %1").arg(record.value("name").toString());
        if (!query.exec(tableQuery))
            qDebug() << query.lastError().text();
    }

    regions.removeRows(0, regions.rowCount());
    regions.submitAll();

    regions.clear();
    userdb_.close();
}

void KSUserDB::AddHorizon(ArtificialHorizonEntity *horizon)
{
    userdb_.open();
    QSqlTableModel regions(0, userdb_);
    regions.setTable("horizons");

    regions.select();
    QString tableName = QString("horizon_%1").arg(regions.rowCount()+1);

    regions.insertRow(0);
    regions.setData(regions.index(0, 1), tableName);
    regions.setData(regions.index(0, 2), horizon->region());
    regions.setData(regions.index(0, 3), horizon->enabled() ? 1 : 0);
    regions.submitAll();
    regions.clear();


    QString tableQuery = QString("CREATE TABLE %1 (Az REAL NOT NULL, Alt REAL NOT NULL)").arg(tableName);
    QSqlQuery query(userdb_);
    query.exec(tableQuery);

    QSqlTableModel points(0, userdb_);    

    points.setTable(tableName);

    SkyList *skyList = horizon->list()->points();

    for (int i=0; i < skyList->size(); i++)
    {
        points.select();
        QSqlRecord rec( points.record() );
        rec.setValue( "Az", skyList->at(i)->az().Degrees());
        rec.setValue( "Alt", skyList->at(i)->alt().Degrees());
        points.insertRecord( -1, rec );
    }

    points.submitAll();
    points.clear();

    userdb_.close();
}

int KSUserDB::AddProfile(const QString &name)
{
    userdb_.open();
    int id=-1;

    QSqlQuery query(userdb_);
    bool rc = query.exec(QString("INSERT INTO profile (name) VALUES('%1')").arg(name));

    if (rc == false)
        qDebug() << query.lastQuery() << query.lastError().text();
    else
        id = query.lastInsertId().toInt();

    userdb_.close();

    return id;
}

bool KSUserDB::DeleteProfile(ProfileInfo *pi)
{
    userdb_.open();

    QSqlQuery query(userdb_);
    bool rc;

    rc = query.exec("DELETE FROM profile WHERE id=" + QString::number(pi->id));

    if (rc == false)
        qDebug() << query.lastQuery() << query.lastError().text();

    userdb_.close();

    return rc;
}

void KSUserDB::SaveProfile(ProfileInfo *pi)
{
    // Remove all drivers
    DeleteProfileDrivers(pi);

    userdb_.open();
    QSqlQuery query(userdb_);

    // Clear data
    if (!query.exec(QString("UPDATE profile SET host=null,port=null,city=null,province=null,country=null,indiwebmanagerport=NULL WHERE id=%1").arg(pi->id)))
        qDebug() << query.lastQuery() << query.lastError().text();

    // Update Name
    if (!query.exec(QString("UPDATE profile SET name='%1' WHERE id=%2").arg(pi->name).arg(pi->id)))
        qDebug() << query.lastQuery() << query.lastError().text();

    // Update Remote Data
    if (pi->host.isEmpty() == false)
    {
        if (!query.exec(QString("UPDATE profile SET host='%1',port=%2 WHERE id=%3").arg(pi->host).arg((pi->port)).arg(pi->id)))
            qDebug() << query.lastQuery() << query.lastError().text();

        if (pi->INDIWebManagerPort != -1)
        {
            if (!query.exec(QString("UPDATE profile SET indiwebmanagerport='%1' WHERE id=%2").arg(pi->INDIWebManagerPort).arg(pi->id)))
                qDebug() << query.lastQuery() << query.lastError().text();
        }
    }

    // Update City Info
    if (pi->city.isEmpty() == false)
    {
        if (!query.exec(QString("UPDATE profile SET city='%1',province='%2',country='%3' WHERE id=%4").arg(pi->city).arg(pi->province).arg(pi->country).arg(pi->id)))
            qDebug() << query.lastQuery() << query.lastError().text();
    }

    QMapIterator<QString, QString> i(pi->drivers);
    while (i.hasNext())
    {
        i.next();
        if (!query.exec(QString("INSERT INTO driver (label, role, profile) VALUES('%1','%2',%3)").arg(i.value()).arg(i.key()).arg(pi->id)))
            qDebug() <<  query.lastQuery() << query.lastError().text();
    }

    /*if (pi->customDrivers.isEmpty() == false && !query.exec(QString("INSERT INTO custom_driver (drivers, profile) VALUES('%1',%2)").arg(pi->customDrivers).arg(pi->id)))
        qDebug()  << query.lastQuery() << query.lastError().text();*/

    userdb_.close();
}


QList<ProfileInfo *> KSUserDB::GetAllProfiles()
{
    QList<ProfileInfo *> profiles;

    userdb_.open();
    QSqlTableModel profile(0, userdb_);
    profile.setTable("profile");
    profile.select();

    for (int i =0; i < profile.rowCount(); ++i)
    {
        QSqlRecord record = profile.record(i);

        int     id   = record.value("id").toInt();
        QString name = record.value("name").toString();

        ProfileInfo *pi = new ProfileInfo(id, name);

        // Add host and port
        pi->host = record.value("host").toString();
        pi->port = record.value("port").toInt();

        // City info
        pi->city     = record.value("city").toString();
        pi->province = record.value("province").toString();
        pi->country  = record.value("country").toString();

        pi->INDIWebManagerPort = record.value("indiwebmanagerport").toInt();

        GetProfileDrivers(pi);
        //GetProfileCustomDrivers(pi);

        profiles.append(pi);
    }

    profile.clear();
    userdb_.close();

    return profiles;
}

void KSUserDB::GetProfileDrivers(ProfileInfo* pi)
{
    userdb_.open();

    QSqlTableModel driver(0, userdb_);
    driver.setTable("driver");
    driver.setFilter("profile=" + QString::number(pi->id));
    if (driver.select() == false)
        qDebug() << "driver select error: " << driver.query().lastQuery() <<  driver.lastError().text();

    for (int i =0; i < driver.rowCount(); ++i)
    {
        QSqlRecord record = driver.record(i);
        QString label = record.value("label").toString();
        QString role  = record.value("role").toString();

        pi->drivers[role] = label;
    }

    driver.clear();
    userdb_.close();
}

/*void KSUserDB::GetProfileCustomDrivers(ProfileInfo* pi)
{
    userdb_.open();
    QSqlTableModel custom_driver(0, userdb_);
    custom_driver.setTable("driver");
    custom_driver.setFilter("profile=" + QString::number(pi->id));
    if (custom_driver.select() == false)
        qDebug() << "custom driver select error: " << custom_driver.query().lastQuery() << custom_driver.lastError().text();

    QSqlRecord record = custom_driver.record(0);
    pi->customDrivers   = record.value("drivers").toString();

    custom_driver.clear();
    userdb_.close();
}*/

void KSUserDB::DeleteProfileDrivers(ProfileInfo* pi)
{
    userdb_.open();

    QSqlQuery query(userdb_);

    /*if (!query.exec("DELETE FROM custom_driver WHERE profile=" + QString::number(pi->id)))
        qDebug() << query.lastQuery() << query.lastError().text();*/

    if (!query.exec("DELETE FROM driver WHERE profile=" + QString::number(pi->id)))
        qDebug() << query.lastQuery() << query.lastError().text();

    userdb_.close();
}

