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

#include "ksuserdb.h"

#include "artificialhorizoncomponent.h"
#include "kspaths.h"
#include "kstarsdata.h"
#include "linelist.h"
#include "version.h"

#include <QSqlQuery>
#include <QSqlRecord>
#include <QSqlTableModel>

#include <kstars_debug.h>

/*
 * TODO (spacetime):
 * The database supports storing logs. But it needs to be implemented.
 *
 * One of the unresolved problems was the creation of a unique identifier
 * for each object (DSO,planet,star etc) for use in the database.
*/

KSUserDB::~KSUserDB()
{
    userdb_.close();
}

bool KSUserDB::Initialize()
{
    // Every logged in user has their own db.
    userdb_        = QSqlDatabase::addDatabase("QSQLITE", "userdb");
    QString dbfile = KSPaths::writableLocation(QStandardPaths::GenericDataLocation) + "userdb.sqlite";
    QFile testdb(dbfile);
    bool first_run = false;
    if (!testdb.exists())
    {
        qCInfo(KSTARS) << "User DB does not exist. New User DB will be created.";
        first_run = true;
    }
    userdb_.setDatabaseName(dbfile);
    if (!userdb_.open())
    {
        qCWarning(KSTARS) << "Unable to open user database file.";
        qCritical(KSTARS) << LastError();
        return false;
    }
    else
    {
        qCDebug(KSTARS) << "Opened the User DB. Ready.";
        if (first_run == true)
            FirstRun();
        else
        {
            // Update table if previous version exists
            QSqlTableModel version(nullptr, userdb_);
            version.setTable("Version");
            version.select();
            QSqlRecord record = version.record(0);
            version.clear();

            // Old version had 2.9.5 ..etc, so we remove them
            // Starting with 2.9.7, we are using SCHEMA_VERSION which now decoupled from KStars Version and starts at 300
            int currentDBVersion = record.value("Version").toString().remove(".").toInt();

            // Update database version to current KStars version
            if (currentDBVersion != SCHEMA_VERSION)
            {
                QSqlQuery query(userdb_);
                QString versionQuery = QString("UPDATE Version SET Version=%1").arg(SCHEMA_VERSION);
                if (!query.exec(versionQuery))
                    qCWarning(KSTARS) << query.lastError();
            }

            // If prior to 2.9.4 extend filters table
            if (currentDBVersion < 294)
            {
                QSqlQuery query(userdb_);

                qCWarning(KSTARS) << "Detected old format filter table, re-creating...";
                if (!query.exec("DROP table filter"))
                    qCWarning(KSTARS) << query.lastError();
                if (!query.exec("CREATE TABLE filter ( "
                                "id INTEGER DEFAULT NULL PRIMARY KEY AUTOINCREMENT, "
                                "Vendor TEXT DEFAULT NULL, "
                                "Model TEXT DEFAULT NULL, "
                                "Type TEXT DEFAULT NULL, "
                                "Color TEXT DEFAULT NULL,"
                                "Exposure REAL DEFAULT 1.0,"
                                "Offset INTEGER DEFAULT 0,"
                                "UseAutoFocus INTEGER DEFAULT 0,"
                                "LockedFilter TEXT DEFAULT '--',"
                                "AbsoluteFocusPosition INTEGER DEFAULT 0)"))
                    qCWarning(KSTARS) << query.lastError();
            }

            // If prior to 2.9.5 create fov table
            if (currentDBVersion < 295)
            {
                QSqlQuery query(userdb_);

                if (!query.exec("CREATE TABLE effectivefov ( "
                                "id INTEGER DEFAULT NULL PRIMARY KEY AUTOINCREMENT, "
                                "Profile TEXT DEFAULT NULL, "
                                "Width INTEGER DEFAULT NULL, "
                                "Height INTEGER DEFAULT NULL, "
                                "PixelW REAL DEFAULT 5.0,"
                                "PixelH REAL DEFAULT 5.0,"
                                "FocalLength REAL DEFAULT 0.0,"
                                "FovW REAL DEFAULT 0.0,"
                                "FovH REAL DEFAULT 0.0)"))
                    qCWarning(KSTARS) << query.lastError();
            }

            if (currentDBVersion < 300)
            {
                QSqlQuery query(userdb_);
                QString columnQuery = QString("ALTER TABLE profile ADD COLUMN remotedrivers TEXT DEFAULT NULL");
                if (!query.exec(columnQuery))
                    qCWarning(KSTARS) << query.lastError();

                if (userdb_.tables().contains("customdrivers") == false)
                {
                    QSqlQuery query(userdb_);

                    if (!query.exec("CREATE TABLE customdrivers ( "
                                    "id INTEGER DEFAULT NULL PRIMARY KEY AUTOINCREMENT, "
                                    "Name TEXT DEFAULT NULL, "
                                    "Label TEXT DEFAULT NULL UNIQUE, "
                                    "Family TEXT DEFAULT NULL, "
                                    "Exec TEXT DEFAULT NULL, "
                                    "Version TEXT DEFAULT 1.0)"))
                        qCWarning(KSTARS) << query.lastError();
                }
            }
        }
    }
    userdb_.close();
    return true;
}

QSqlError KSUserDB::LastError()
{
    // error description is in QSqlError::text()
    return userdb_.lastError();
}

bool KSUserDB::FirstRun()
{
    if (!RebuildDB())
        return false;

    /*ImportFlags();
    ImportUsers();
    ImportEquipment();*/

    return true;
}

bool KSUserDB::RebuildDB()
{
    qCInfo(KSTARS) << "Rebuilding User Database";

    QVector<QString> tables;
    tables.append("CREATE TABLE Version ("
                  "Version CHAR DEFAULT NULL)");
    tables.append("INSERT INTO Version VALUES (\"" KSTARS_VERSION "\")");
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
                  "Color TEXT DEFAULT NULL,"
                  "Exposure REAL DEFAULT 1.0,"
                  "Offset INTEGER DEFAULT 0,"
                  "UseAutoFocus INTEGER DEFAULT 0,"
                  "LockedFilter TEXT DEFAULT '--',"
                  "AbsoluteFocusPosition INTEGER DEFAULT 0)");

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

    tables.append("CREATE TABLE profile (id INTEGER DEFAULT NULL PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL, host "
                  "TEXT, port INTEGER, city TEXT, province TEXT, country TEXT, indiwebmanagerport INTEGER DEFAULT "
                  "NULL, autoconnect INTEGER DEFAULT 1, guidertype INTEGER DEFAULT 0, guiderhost TEXT, guiderport INTEGER,"
                  "primaryscope INTEGER DEFAULT 0, guidescope INTEGER DEFAULT 0, remotedrivers TEXT DEFAULT NULL)");
    tables.append("CREATE TABLE driver (id INTEGER DEFAULT NULL PRIMARY KEY AUTOINCREMENT, label TEXT NOT NULL, role "
                  "TEXT NOT NULL, profile INTEGER NOT NULL, FOREIGN KEY(profile) REFERENCES profile(id))");
    //tables.append("CREATE TABLE custom_driver (id INTEGER DEFAULT NULL PRIMARY KEY AUTOINCREMENT, drivers TEXT NOT NULL, profile INTEGER NOT NULL, FOREIGN KEY(profile) REFERENCES profile(id))");

#ifdef Q_OS_WIN
    tables.append("INSERT INTO profile (name, host, port) VALUES ('Simulators', 'localhost', 7624)");
#else
    tables.append("INSERT INTO profile (name) VALUES ('Simulators')");
#endif

    tables.append("INSERT INTO driver (label, role, profile) VALUES ('Telescope Simulator', 'Mount', 1)");
    tables.append("INSERT INTO driver (label, role, profile) VALUES ('CCD Simulator', 'CCD', 1)");
    tables.append("INSERT INTO driver (label, role, profile) VALUES ('Focuser Simulator', 'Focuser', 1)");

    tables.append("CREATE TABLE IF NOT EXISTS darkframe (id INTEGER DEFAULT NULL PRIMARY KEY AUTOINCREMENT, ccd TEXT "
                  "NOT NULL, chip INTEGER DEFAULT 0, binX INTEGER, binY INTEGER, temperature REAL, duration REAL, "
                  "filename TEXT NOT NULL, timestamp DATETIME DEFAULT CURRENT_TIMESTAMP)");

    tables.append("CREATE TABLE IF NOT EXISTS hips (ID TEXT NOT NULL UNIQUE,"
                  "obs_title TEXT NOT NULL, obs_description TEXT NOT NULL, hips_order TEXT NOT NULL,"
                  "hips_frame TEXT NOT NULL, hips_tile_width TEXT NOT NULL, hips_tile_format TEXT NOT NULL,"
                  "hips_service_url TEXT NOT NULL, moc_sky_fraction TEXT NOT NULL)");

    tables.append("INSERT INTO hips (ID, obs_title, obs_description, hips_order, hips_frame, hips_tile_width, hips_tile_format, hips_service_url, moc_sky_fraction)"
                  "VALUES ('CDS/P/DSS2/color', 'DSS colored', 'Color composition generated by CDS. This HiPS survey is based on 2 others HiPS surveys,"
                  " respectively DSS2-red and DSS2-blue HiPS, both of them directly generated from original scanned plates downloaded"
                  " from STScI site. The red component has been built from POSS-II F, AAO-SES,SR and SERC-ER plates. The blue component"
                  " has been build from POSS-II J and SERC-J,EJ. The green component is based on the mean of other components. Three"
                  " missing plates from red survey (253, 260, 359) has been replaced by pixels from the DSSColor STScI jpeg survey."
                  " The 11 missing blue plates (mainly in galactic plane) have not been replaced (only red component).',"
                  "'9', 'equatorial', '512', 'jpeg fits', 'http://alasky.u-strasbg.fr/DSS/DSSColor','1')");

    tables.append("INSERT INTO hips (ID, obs_title, obs_description, hips_order, hips_frame, hips_tile_width, hips_tile_format, hips_service_url, moc_sky_fraction)"
                  "VALUES ('CDS/P/2MASS/color', '2MASS color J (1.23 microns), H (1.66 microns), K (2.16 microns)',"
                  "'2MASS has uniformly scanned the entire sky in three near-infrared bands to detect and characterize point sources"
                  " brighter than about 1 mJy in each band, with signal-to-noise ratio (SNR) greater than 10, using a pixel size of"
                  " 2.0\". This has achieved an 80,000-fold improvement in sensitivity relative to earlier surveys. 2MASS used two"
                  " highly-automated 1.3-m telescopes, one at Mt. Hopkins, AZ, and one at CTIO, Chile. Each telescope was equipped with"
                  " a three-channel camera, each channel consisting of a 256x256 array of HgCdTe detectors, capable of observing the"
                  " sky simultaneously at J (1.25 microns), H (1.65 microns), and Ks (2.17 microns). The University of Massachusetts"
                  " (UMass) was responsible for the overall management of the project, and for developing the infrared cameras and"
                  " on-site computing systems at both facilities. The Infrared Processing and Analysis Center (IPAC) is responsible"
                  " for all data processing through the Production Pipeline, and construction and distribution of the data products."
                  " Funding is provided primarily by NASA and the NSF',"
                  "'9', 'equatorial', '512', 'jpeg fits', 'http://alaskybis.u-strasbg.fr/2MASS/Color', '1')");

    tables.append("INSERT INTO hips (ID, obs_title, obs_description, hips_order, hips_frame, hips_tile_width, hips_tile_format, hips_service_url, moc_sky_fraction)"
                  "VALUES ('CDS/P/Fermi/color', 'Fermi Color HEALPix survey', 'Launched on June 11, 2008, the Fermi Gamma-ray Space Telescope observes the cosmos using the"
                  " highest-energy form of light. This survey sums all data observed by the Fermi mission up to week 396. This version"
                  " of the Fermi survey are intensity maps where the summed counts maps are divided by the exposure for each pixel"
                  ". We anticipate using the HEASARC Hera capabilities to update this survey on a roughly quarterly basis. Data is"
                  " broken into 5 energy bands : 30-100 MeV Band 1, 100-300 MeV Band 2, 300-1000 MeV Band 3, 1-3 GeV Band 4 ,"
                  " 3-300 GeV Band 5. The SkyView data are based upon a Cartesian projection of the counts divided by the exposure maps."
                  " In the Cartesian projection pixels near the pole have a much smaller area than pixels on the equator, so these"
                  " pixels have smaller integrated flux. When creating large scale images in other projections users may wish to make"
                  " sure to compensate for this effect the flux conserving clip-resampling option.', '9', 'equatorial', '512', 'jpeg fits',"
                  "'http://alaskybis.u-strasbg.fr/Fermi/Color', '1')");


    tables.append("CREATE TABLE dslr (id INTEGER DEFAULT NULL PRIMARY KEY AUTOINCREMENT, "
                  "Model TEXT DEFAULT NULL, "
                  "Width INTEGER DEFAULT NULL, "
                  "Height INTEGER DEFAULT NULL, "
                  "PixelW REAL DEFAULT 5.0,"
                  "PixelH REAL DEFAULT 5.0)");


    tables.append("CREATE TABLE effectivefov ( "
                  "id INTEGER DEFAULT NULL PRIMARY KEY AUTOINCREMENT, "
                  "Profile TEXT DEFAULT NULL, "
                  "Width INTEGER DEFAULT NULL, "
                  "Height INTEGER DEFAULT NULL, "
                  "PixelW REAL DEFAULT 5.0,"
                  "PixelH REAL DEFAULT 5.0,"
                  "FocalLength REAL DEFAULT 0.0,"
                  "FovW REAL DEFAULT 0.0,"
                  "FovH REAL DEFAULT 0.0)");

    tables.append("CREATE TABLE customdrivers ( "
                  "id INTEGER DEFAULT NULL PRIMARY KEY AUTOINCREMENT, "
                  "Name TEXT DEFAULT NULL, "
                  "Label TEXT DEFAULT NULL UNIQUE, "
                  "Family TEXT DEFAULT NULL, "
                  "Exec TEXT DEFAULT NULL, "
                  "Version TEXT DEFAULT 1.0)");

    for (int i = 0; i < tables.count(); ++i)
    {
        QSqlQuery query(userdb_);
        if (!query.exec(tables[i]))
        {
            qCDebug(KSTARS) << query.lastError();
            qCDebug(KSTARS) << query.executedQuery();
        }
    }

    return true;
}

/*
 * Observer Section
*/
void KSUserDB::AddObserver(const QString &name, const QString &surname, const QString &contact)
{
    userdb_.open();
    QSqlTableModel users(nullptr, userdb_);
    users.setTable("user");
    users.setFilter("Name LIKE \'" + name + "\' AND Surname LIKE \'" + surname + "\'");
    users.select();

    if (users.rowCount() > 0)
    {
        QSqlRecord record = users.record(0);
        record.setValue("Name", name);
        record.setValue("Surname", surname);
        record.setValue("Contact", contact);
        users.setRecord(0, record);
        users.submitAll();
    }
    else
    {
        int row = 0;
        users.insertRows(row, 1);
        users.setData(users.index(row, 1), name); // row0 is autoincerement ID
        users.setData(users.index(row, 2), surname);
        users.setData(users.index(row, 3), contact);
        users.submitAll();
    }

    userdb_.close();
}

bool KSUserDB::FindObserver(const QString &name, const QString &surname)
{
    userdb_.open();
    QSqlTableModel users(nullptr, userdb_);
    users.setTable("user");
    users.setFilter("Name LIKE \'" + name + "\' AND Surname LIKE \'" + surname + "\'");
    users.select();

    int observer_count = users.rowCount();

    users.clear();
    userdb_.close();
    return (observer_count > 0);
}

// TODO(spacetime): This method is currently unused.
bool KSUserDB::DeleteObserver(const QString &id)
{
    userdb_.open();
    QSqlTableModel users(nullptr, userdb_);
    users.setTable("user");
    users.setFilter("id = \'" + id + "\'");
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
#ifndef KSTARS_LITE
void KSUserDB::GetAllObservers(QList<Observer *> &observer_list)
{
    userdb_.open();
    observer_list.clear();
    QSqlTableModel users(nullptr, userdb_);
    users.setTable("user");
    users.select();

    for (int i = 0; i < users.rowCount(); ++i)
    {
        QSqlRecord record = users.record(i);
        QString id        = record.value("id").toString();
        QString name      = record.value("Name").toString();
        QString surname   = record.value("Surname").toString();
        QString contact   = record.value("Contact").toString();
        OAL::Observer *o  = new OAL::Observer(id, name, surname, contact);
        observer_list.append(o);
    }

    users.clear();
    userdb_.close();
}
#endif

/* Dark Library Section */

void KSUserDB::AddDarkFrame(const QVariantMap &oneFrame)
{
    userdb_.open();
    QSqlTableModel darkframe(nullptr, userdb_);
    darkframe.setTable("darkframe");
    darkframe.select();

    QSqlRecord record = darkframe.record();

    // Remove PK so that it gets auto-incremented later
    record.remove(0);
    // Remove timestamp so that it gets auto-generated
    record.remove(7);

    for (QVariantMap::const_iterator iter = oneFrame.begin(); iter != oneFrame.end(); ++iter)
        record.setValue(iter.key(), iter.value());

    darkframe.insertRecord(-1, record);

    darkframe.submitAll();

    userdb_.close();
}

bool KSUserDB::DeleteDarkFrame(const QString &filename)
{
    userdb_.open();
    QSqlTableModel darkframe(nullptr, userdb_);
    darkframe.setTable("darkframe");
    darkframe.setFilter("filename = \'" + filename + "\'");

    darkframe.select();

    darkframe.removeRows(0, 1);
    darkframe.submitAll();

    userdb_.close();

    return true;
}

void KSUserDB::GetAllDarkFrames(QList<QVariantMap> &darkFrames)
{
    darkFrames.clear();

    userdb_.open();
    QSqlTableModel darkframe(nullptr, userdb_);
    darkframe.setTable("darkframe");
    darkframe.select();

    for (int i = 0; i < darkframe.rowCount(); ++i)
    {
        QVariantMap recordMap;
        QSqlRecord record = darkframe.record(i);
        for (int j = 1; j < record.count(); j++)
            recordMap[record.fieldName(j)] = record.value(j);

        darkFrames.append(recordMap);
    }

    userdb_.close();
}


/* Effective FOV Section */

void KSUserDB::AddEffectiveFOV(const QVariantMap &oneFOV)
{
    userdb_.open();
    QSqlTableModel effectivefov(nullptr, userdb_);
    effectivefov.setTable("effectivefov");
    effectivefov.select();

    QSqlRecord record = effectivefov.record();

    // Remove PK so that it gets auto-incremented later
    record.remove(0);

    for (QVariantMap::const_iterator iter = oneFOV.begin(); iter != oneFOV.end(); ++iter)
        record.setValue(iter.key(), iter.value());

    effectivefov.insertRecord(-1, record);

    effectivefov.submitAll();

    userdb_.close();
}

bool KSUserDB::DeleteEffectiveFOV(const QString &id)
{
    userdb_.open();
    QSqlTableModel effectivefov(nullptr, userdb_);
    effectivefov.setTable("effectivefov");
    effectivefov.setFilter("id = \'" + id + "\'");

    effectivefov.select();

    effectivefov.removeRows(0, 1);
    effectivefov.submitAll();

    userdb_.close();

    return true;
}

void KSUserDB::GetAllEffectiveFOVs(QList<QVariantMap> &effectiveFOVs)
{
    effectiveFOVs.clear();

    userdb_.open();
    QSqlTableModel effectivefov(nullptr, userdb_);
    effectivefov.setTable("effectivefov");
    effectivefov.select();

    for (int i = 0; i < effectivefov.rowCount(); ++i)
    {
        QVariantMap recordMap;
        QSqlRecord record = effectivefov.record(i);
        for (int j = 0; j < record.count(); j++)
            recordMap[record.fieldName(j)] = record.value(j);

        effectiveFOVs.append(recordMap);
    }

    userdb_.close();
}

/* Driver Alias Section */

bool KSUserDB::AddCustomDriver(const QVariantMap &oneDriver)
{
    userdb_.open();
    QSqlTableModel CustomDriver(nullptr, userdb_);
    CustomDriver.setTable("customdrivers");
    CustomDriver.select();

    QSqlRecord record = CustomDriver.record();

    // Remove PK so that it gets auto-incremented later
    record.remove(0);

    for (QVariantMap::const_iterator iter = oneDriver.begin(); iter != oneDriver.end(); ++iter)
        record.setValue(iter.key(), iter.value());

    bool rc = CustomDriver.insertRecord(-1, record);
    if (rc == false)
        return rc;

    rc = CustomDriver.submitAll();

    userdb_.close();

    return rc;
}

bool KSUserDB::DeleteCustomDriver(const QString &id)
{
    userdb_.open();
    QSqlTableModel CustomDriver(nullptr, userdb_);
    CustomDriver.setTable("customdrivers");
    CustomDriver.setFilter("id = \'" + id + "\'");

    CustomDriver.select();

    CustomDriver.removeRows(0, 1);
    CustomDriver.submitAll();

    userdb_.close();

    return true;
}

void KSUserDB::GetAllCustomDrivers(QList<QVariantMap> &CustomDrivers)
{
    CustomDrivers.clear();

    userdb_.open();
    QSqlTableModel CustomDriver(nullptr, userdb_);
    CustomDriver.setTable("customdrivers");
    CustomDriver.select();

    for (int i = 0; i < CustomDriver.rowCount(); ++i)
    {
        QVariantMap recordMap;
        QSqlRecord record = CustomDriver.record(i);
        for (int j = 0; j < record.count(); j++)
            recordMap[record.fieldName(j)] = record.value(j);

        CustomDrivers.append(recordMap);
    }

    userdb_.close();
}

/* HiPS Section */

void KSUserDB::AddHIPSSource(const QMap<QString,QString> &oneSource)
{
    userdb_.open();
    QSqlTableModel HIPSSource(nullptr, userdb_);
    HIPSSource.setTable("hips");
    HIPSSource.select();

    QSqlRecord record = HIPSSource.record();

    for (QMap<QString,QString>::const_iterator iter = oneSource.begin(); iter != oneSource.end(); ++iter)
        record.setValue(iter.key(), iter.value());

    HIPSSource.insertRecord(-1, record);

    HIPSSource.submitAll();

    userdb_.close();
}

bool KSUserDB::DeleteHIPSSource(const QString &ID)
{
    userdb_.open();
    QSqlTableModel HIPSSource(nullptr, userdb_);
    HIPSSource.setTable("hips");
    HIPSSource.setFilter("ID = \'" + ID + "\'");

    HIPSSource.select();

    HIPSSource.removeRows(0, 1);
    HIPSSource.submitAll();

    userdb_.close();

    return true;
}

void KSUserDB::GetAllHIPSSources(QList<QMap<QString, QString>> &HIPSSources)
{
    HIPSSources.clear();

    userdb_.open();
    QSqlTableModel HIPSSource(nullptr, userdb_);
    HIPSSource.setTable("hips");
    HIPSSource.select();

    for (int i = 0; i < HIPSSource.rowCount(); ++i)
    {
        QMap<QString,QString> recordMap;
        QSqlRecord record = HIPSSource.record(i);
        for (int j = 1; j < record.count(); j++)
            recordMap[record.fieldName(j)] = record.value(j).toString();

        HIPSSources.append(recordMap);
    }

    userdb_.close();
}


/* DSLR Section */

void KSUserDB::AddDSLRInfo(const QMap<QString,QVariant> &oneInfo)
{
    userdb_.open();
    QSqlTableModel DSLRInfo(nullptr, userdb_);
    DSLRInfo.setTable("dslr");
    DSLRInfo.select();

    QSqlRecord record = DSLRInfo.record();

    for (QMap<QString,QVariant>::const_iterator iter = oneInfo.begin(); iter != oneInfo.end(); ++iter)
        record.setValue(iter.key(), iter.value());

    DSLRInfo.insertRecord(-1, record);

    DSLRInfo.submitAll();

    userdb_.close();
}

bool KSUserDB::DeleteAllDSLRInfo()
{
    userdb_.open();
    QSqlTableModel DSLRInfo(nullptr, userdb_);
    DSLRInfo.setTable("dslr");
    DSLRInfo.select();

    DSLRInfo.removeRows(0, DSLRInfo.rowCount());
    DSLRInfo.submitAll();

    userdb_.close();

    return true;
}

bool KSUserDB::DeleteDSLRInfo(const QString &model)
{
    userdb_.open();
    QSqlTableModel DSLRInfo(nullptr, userdb_);
    DSLRInfo.setTable("dslr");
    DSLRInfo.setFilter("model = \'" + model + "\'");

    DSLRInfo.select();

    DSLRInfo.removeRows(0, 1);
    DSLRInfo.submitAll();

    userdb_.close();

    return true;
}

void KSUserDB::GetAllDSLRInfos(QList<QMap<QString, QVariant>> &DSLRInfos)
{
    DSLRInfos.clear();

    userdb_.open();
    QSqlTableModel DSLRInfo(nullptr, userdb_);
    DSLRInfo.setTable("dslr");
    DSLRInfo.select();

    for (int i = 0; i < DSLRInfo.rowCount(); ++i)
    {
        QMap<QString,QVariant> recordMap;
        QSqlRecord record = DSLRInfo.record(i);
        for (int j = 1; j < record.count(); j++)
            recordMap[record.fieldName(j)] = record.value(j);

        DSLRInfos.append(recordMap);
    }

    userdb_.close();
}

/*
 * Flag Section
*/

void KSUserDB::DeleteAllFlags()
{
    userdb_.open();
    QSqlTableModel flags(nullptr, userdb_);
    flags.setEditStrategy(QSqlTableModel::OnManualSubmit);
    flags.setTable("flags");
    flags.select();

    flags.removeRows(0, flags.rowCount());
    flags.submitAll();

    flags.clear();
    userdb_.close();
}

void KSUserDB::AddFlag(const QString &ra, const QString &dec, const QString &epoch, const QString &image_name,
                       const QString &label, const QString &labelColor)
{
    userdb_.open();
    QSqlTableModel flags(nullptr, userdb_);
    flags.setTable("flags");

    int row = 0;
    flags.insertRows(row, 1);
    flags.setData(flags.index(row, 1), ra); // row,0 is autoincerement ID
    flags.setData(flags.index(row, 2), dec);
    flags.setData(flags.index(row, 3), image_name);
    flags.setData(flags.index(row, 4), label);
    flags.setData(flags.index(row, 5), labelColor);
    flags.setData(flags.index(row, 6), epoch);
    flags.submitAll();

    flags.clear();
    userdb_.close();
}

QList<QStringList> KSUserDB::GetAllFlags()
{
    QList<QStringList> flagList;

    userdb_.open();
    QSqlTableModel flags(nullptr, userdb_);
    flags.setTable("flags");
    flags.select();

    for (int i = 0; i < flags.rowCount(); ++i)
    {
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
void KSUserDB::DeleteEquipment(const QString &type, const int &id)
{
    userdb_.open();
    QSqlTableModel equip(nullptr, userdb_);
    equip.setTable(type);
    equip.setFilter("id = " + QString::number(id));
    equip.select();

    equip.removeRows(0, equip.rowCount());
    equip.submitAll();

    equip.clear();
    userdb_.close();
}

void KSUserDB::DeleteAllEquipment(const QString &type)
{
    userdb_.open();
    QSqlTableModel equip(nullptr, userdb_);
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
void KSUserDB::AddScope(const QString &model, const QString &vendor, const QString &driver, const QString &type,
                        const double &focalLength, const double &aperture)
{
    userdb_.open();
    QSqlTableModel equip(nullptr, userdb_);
    equip.setTable("telescope");

    int row = 0;
    equip.insertRows(row, 1);
    equip.setData(equip.index(row, 1), vendor); // row,0 is autoincerement ID
    equip.setData(equip.index(row, 2), aperture);
    equip.setData(equip.index(row, 3), model);
    equip.setData(equip.index(row, 4), driver);
    equip.setData(equip.index(row, 5), type);
    equip.setData(equip.index(row, 6), focalLength);
    equip.submitAll();

    equip.clear(); //DB will not close if linked object not cleared
    userdb_.close();
}

void KSUserDB::AddScope(const QString &model, const QString &vendor, const QString &driver, const QString &type,
                        const double &focalLength, const double &aperture, const QString &id)
{
    userdb_.open();
    QSqlTableModel equip(nullptr, userdb_);
    equip.setTable("telescope");
    equip.setFilter("id = " + id);
    equip.select();

    if (equip.rowCount() > 0)
    {
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
#ifndef KSTARS_LITE
void KSUserDB::GetAllScopes(QList<Scope *> &scope_list)
{
    scope_list.clear();

    userdb_.open();
    QSqlTableModel equip(nullptr, userdb_);
    equip.setTable("telescope");
    equip.select();

    for (int i = 0; i < equip.rowCount(); ++i)
    {
        QSqlRecord record  = equip.record(i);
        QString id         = record.value("id").toString();
        QString vendor     = record.value("Vendor").toString();
        double aperture    = record.value("Aperture").toDouble();
        QString model      = record.value("Model").toString();
        QString driver     = record.value("Driver").toString();
        QString type       = record.value("Type").toString();
        double focalLength = record.value("FocalLength").toDouble();
        OAL::Scope *o      = new OAL::Scope(id, model, vendor, type, focalLength, aperture);
        o->setINDIDriver(driver);
        scope_list.append(o);
    }

    equip.clear();
    userdb_.close();
}
#endif
/*
 * Eyepiece section
 */
void KSUserDB::AddEyepiece(const QString &vendor, const QString &model, const double &focalLength, const double &fov,
                           const QString &fovunit)
{
    userdb_.open();
    QSqlTableModel equip(nullptr, userdb_);
    equip.setTable("eyepiece");

    int row = 0;
    equip.insertRows(row, 1);
    equip.setData(equip.index(row, 1), vendor); // row,0 is autoincerement ID
    equip.setData(equip.index(row, 2), model);
    equip.setData(equip.index(row, 3), focalLength);
    equip.setData(equip.index(row, 4), fov);
    equip.setData(equip.index(row, 5), fovunit);
    equip.submitAll();

    equip.clear();
    userdb_.close();
}

void KSUserDB::AddEyepiece(const QString &vendor, const QString &model, const double &focalLength, const double &fov,
                           const QString &fovunit, const QString &id)
{
    userdb_.open();
    QSqlTableModel equip(nullptr, userdb_);
    equip.setTable("eyepiece");
    equip.setFilter("id = " + id);
    equip.select();

    if (equip.rowCount() > 0)
    {
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
#ifndef KSTARS_LITE
void KSUserDB::GetAllEyepieces(QList<OAL::Eyepiece *> &eyepiece_list)
{
    eyepiece_list.clear();

    userdb_.open();
    QSqlTableModel equip(nullptr, userdb_);
    equip.setTable("eyepiece");
    equip.select();

    for (int i = 0; i < equip.rowCount(); ++i)
    {
        QSqlRecord record  = equip.record(i);
        QString id         = record.value("id").toString();
        QString vendor     = record.value("Vendor").toString();
        QString model      = record.value("Model").toString();
        double focalLength = record.value("FocalLength").toDouble();
        double fov         = record.value("ApparentFOV").toDouble();
        QString fovUnit    = record.value("FOVUnit").toString();

        OAL::Eyepiece *o = new OAL::Eyepiece(id, model, vendor, fov, fovUnit, focalLength);
        eyepiece_list.append(o);
    }

    equip.clear();
    userdb_.close();
}
#endif
/*
 * lens section
 */
void KSUserDB::AddLens(const QString &vendor, const QString &model, const double &factor)
{
    userdb_.open();
    QSqlTableModel equip(nullptr, userdb_);
    equip.setTable("lens");

    int row = 0;
    equip.insertRows(row, 1);
    equip.setData(equip.index(row, 1), vendor); // row,0 is autoincerement ID
    equip.setData(equip.index(row, 2), model);
    equip.setData(equip.index(row, 3), factor);
    equip.submitAll();

    equip.clear();
    userdb_.close();
}

void KSUserDB::AddLens(const QString &vendor, const QString &model, const double &factor, const QString &id)
{
    userdb_.open();
    QSqlTableModel equip(nullptr, userdb_);
    equip.setTable("lens");
    equip.setFilter("id = " + id);
    equip.select();

    if (equip.rowCount() > 0)
    {
        QSqlRecord record = equip.record(0);
        record.setValue(1, vendor);
        record.setValue(2, model);
        record.setValue(3, factor);
        equip.submitAll();
    }

    userdb_.close();
}
#ifndef KSTARS_LITE
void KSUserDB::GetAllLenses(QList<OAL::Lens *> &lens_list)
{
    lens_list.clear();

    userdb_.open();
    QSqlTableModel equip(nullptr, userdb_);
    equip.setTable("lens");
    equip.select();

    for (int i = 0; i < equip.rowCount(); ++i)
    {
        QSqlRecord record = equip.record(i);
        QString id        = record.value("id").toString();
        QString vendor    = record.value("Vendor").toString();
        QString model     = record.value("Model").toString();
        double factor     = record.value("Factor").toDouble();
        OAL::Lens *o      = new OAL::Lens(id, model, vendor, factor);
        lens_list.append(o);
    }

    equip.clear();
    userdb_.close();
}
#endif
/*
 *  filter section
 */
void KSUserDB::AddFilter(const QString &vendor, const QString &model, const QString &type, const QString &color,
                         int offset, double exposure, bool useAutoFocus, const QString &lockedFilter, int absFocusPos)
{
    userdb_.open();
    QSqlTableModel equip(nullptr, userdb_);
    equip.setTable("filter");

    QSqlRecord record = equip.record();
    record.setValue("Vendor", vendor);
    record.setValue("Model", model);
    record.setValue("Type", type);
    record.setValue("Color", color);
    record.setValue("Offset", offset);
    record.setValue("Exposure", exposure);
    record.setValue("UseAutoFocus", useAutoFocus ? 1 : 0);
    record.setValue("LockedFilter", lockedFilter);
    record.setValue("AbsoluteFocusPosition", absFocusPos);

    if (equip.insertRecord(-1, record) == false)
        qCritical() << __FUNCTION__ << equip.lastError();


    /*int row = 0;
    equip.insertRows(row, 1);
    equip.setData(equip.index(row, 1), vendor);
    equip.setData(equip.index(row, 2), model);
    equip.setData(equip.index(row, 3), type);
    equip.setData(equip.index(row, 4), offset);
    equip.setData(equip.index(row, 5), color);
    equip.setData(equip.index(row, 6), exposure);
    equip.setData(equip.index(row, 7), lockedFilter);
    equip.setData(equip.index(row, 8), useAutoFocus ? 1 : 0);
    */
    if (equip.submitAll() == false)
        qCritical() << "AddFilter:" << equip.lastError();

    equip.clear();
    userdb_.close();
}

void KSUserDB::AddFilter(const QString &vendor, const QString &model, const QString &type, const QString &color,
                         int offset, double exposure, bool useAutoFocus, const QString &lockedFilter, int absFocusPos, const QString &id)
{
    userdb_.open();
    QSqlTableModel equip(nullptr, userdb_);
    equip.setTable("filter");
    equip.setFilter("id = " + id);
    equip.select();

    if (equip.rowCount() > 0)
    {
        QSqlRecord record = equip.record(0);
        record.setValue("Vendor", vendor);
        record.setValue("Model", model);
        record.setValue("Type", type);
        record.setValue("Color", color);
        record.setValue("Offset", offset);
        record.setValue("Exposure", exposure);
        record.setValue("UseAutoFocus", useAutoFocus ? 1 : 0);
        record.setValue("LockedFilter", lockedFilter);
        record.setValue("AbsoluteFocusPosition", absFocusPos);
        equip.setRecord(0, record);
        if (equip.submitAll() == false)
            qCritical() << "AddFilter:" << equip.lastError();
    }

    userdb_.close();
}
#ifndef KSTARS_LITE
void KSUserDB::GetAllFilters(QList<OAL::Filter *> &filter_list)
{
    userdb_.open();
    filter_list.clear();
    QSqlTableModel equip(nullptr, userdb_);
    equip.setTable("filter");
    equip.select();

    for (int i = 0; i < equip.rowCount(); ++i)
    {
        QSqlRecord record = equip.record(i);
        QString id        = record.value("id").toString();
        QString vendor    = record.value("Vendor").toString();
        QString model     = record.value("Model").toString();
        QString type      = record.value("Type").toString();
        QString color     = record.value("Color").toString();
        int offset        = record.value("Offset").toInt();
        double exposure   = record.value("Exposure").toDouble();
        QString lockedFilter  = record.value("LockedFilter").toString();
        bool useAutoFocus = record.value("UseAutoFocus").toInt() == 1;
        int absFocusPos   = record.value("AbsoluteFocusPosition").toInt();
        OAL::Filter *o    = new OAL::Filter(id, model, vendor, type, color, exposure, offset, useAutoFocus, lockedFilter, absFocusPos);
        filter_list.append(o);
    }

    equip.clear();
    userdb_.close();
}
#endif
#if 0
bool KSUserDB::ImportFlags()
{
    QString flagfilename = KSPaths::writableLocation(QStandardPaths::GenericDataLocation) + QDir::separator() + "flags.dat";
    QFile flagsfile(flagfilename);
    if (!flagsfile.exists())
    {
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
    while (flagparser.HasNextRow())
    {
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

bool KSUserDB::ImportUsers()
{
    QString usersfilename = KSPaths::writableLocation(QStandardPaths::GenericDataLocation) + QDir::separator() + "observerlist.xml";
    QFile usersfile(usersfilename);

    if (!usersfile.exists())
    {
        return false;  // No upgrade needed. Users file doesn't exist.
    }

    if( ! usersfile.open( QIODevice::ReadOnly ) )
        return false;

    QXmlStreamReader * reader = new QXmlStreamReader(&usersfile);

    while( ! reader->atEnd() )
    {
        reader->readNext();

        if( reader->isEndElement() )
            break;

        if( reader->isStartElement() )
        {
            if (reader->name() != "observers")
                continue;

            //Read all observers
            while( ! reader->atEnd() )
            {
                reader->readNext();

                if( reader->isEndElement() )
                    break;

                if( reader->isStartElement() )
                {
                    // Read single observer
                    if( reader->name() == "observer" )
                    {
                        QString name, surname, contact;
                        while( ! reader->atEnd() )
                        {
                            reader->readNext();

                            if( reader->isEndElement() )
                                break;

                            if( reader->isStartElement() )
                            {
                                if( reader->name() == "name" )
                                {
                                    name = reader->readElementText();
                                }
                                else if( reader->name() == "surname" )
                                {
                                    surname = reader->readElementText();
                                }
                                else if( reader->name() == "contact" )
                                {
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

bool KSUserDB::ImportEquipment()
{
    QString equipfilename = KSPaths::writableLocation(QStandardPaths::GenericDataLocation) + QDir::separator() + "equipmentlist.xml";
    QFile equipfile(equipfilename);

    if (!equipfile.exists())
    {
        return false;  // No upgrade needed. File doesn't exist.
    }

    if( ! equipfile.open( QIODevice::ReadOnly ) )
        return false;

    reader_ = new QXmlStreamReader(&equipfile);
    while( ! reader_->atEnd() )
    {
        reader_->readNext();
        if( reader_->isStartElement() )
        {
            while( ! reader_->atEnd() )
            {
                reader_->readNext();

                if( reader_->isEndElement() )
                    break;

                if( reader_->isStartElement() )
                {
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

void KSUserDB::readScopes()
{
    while (!reader_->atEnd())
    {
        reader_->readNext();

        if (reader_->isEndElement())
            break;

        if (reader_->isStartElement())
        {
            if (reader_->name() == "scope")
                readScope();
        }
    }
}

void KSUserDB::readEyepieces()
{
    while (!reader_->atEnd())
    {
        reader_->readNext();

        if (reader_->isEndElement())
            break;

        if (reader_->isStartElement())
        {
            if (reader_->name() == "eyepiece")
                readEyepiece();
        }
    }
}

void KSUserDB::readLenses()
{
    while (!reader_->atEnd())
    {
        reader_->readNext();

        if (reader_->isEndElement())
            break;

        if (reader_->isStartElement())
        {
            if (reader_->name() == "lens")
                readLens();
        }
    }
}

void KSUserDB::readFilters()
{
    while (!reader_->atEnd())
    {
        reader_->readNext();

        if (reader_->isEndElement())
            break;

        if (reader_->isStartElement())
        {
            if (reader_->name() == "filter")
                readFilter();
        }
    }
}

void KSUserDB::readScope()
{
    QString model, vendor, type, driver = i18nc("No driver", "None");
    double aperture = 0, focalLength = 0;

    while (!reader_->atEnd())
    {
        reader_->readNext();

        if (reader_->isEndElement())
            break;

        if (reader_->isStartElement())
        {
            if (reader_->name() == "model")
            {
                model = reader_->readElementText();
            }
            else if (reader_->name() == "vendor")
            {
                vendor = reader_->readElementText();
            }
            else if (reader_->name() == "type")
            {
                type = reader_->readElementText();
                if (type == "N")
                    type = "Newtonian";
                if (type == "R")
                    type = "Refractor";
                if (type == "M")
                    type = "Maksutov";
                if (type == "S")
                    type = "Schmidt-Cassegrain";
                if (type == "K")
                    type = "Kutter (Schiefspiegler)";
                if (type == "C")
                    type = "Cassegrain";
            }
            else if (reader_->name() == "focalLength")
            {
                focalLength = (reader_->readElementText()).toDouble();
            }
            else if (reader_->name() == "aperture")
                aperture = (reader_->readElementText()).toDouble();
            else if (reader_->name() == "driver")
                driver = reader_->readElementText();
        }
    }

    AddScope(model, vendor, driver, type, focalLength, aperture);
}

void KSUserDB::readEyepiece()
{
    QString model, focalLength, vendor, fov, fovUnit;
    while (!reader_->atEnd())
    {
        reader_->readNext();

        if (reader_->isEndElement())
            break;

        if (reader_->isStartElement())
        {
            if (reader_->name() == "model")
            {
                model = reader_->readElementText();
            }
            else if (reader_->name() == "vendor")
            {
                vendor = reader_->readElementText();
            }
            else if (reader_->name() == "apparentFOV")
            {
                fov     = reader_->readElementText();
                fovUnit = reader_->attributes().value("unit").toString();
            }
            else if (reader_->name() == "focalLength")
            {
                focalLength = reader_->readElementText();
            }
        }
    }

    AddEyepiece(vendor, model, focalLength.toDouble(), fov.toDouble(), fovUnit);
}

void KSUserDB::readLens()
{
    QString model, factor, vendor;
    while (!reader_->atEnd())
    {
        reader_->readNext();

        if (reader_->isEndElement())
            break;

        if (reader_->isStartElement())
        {
            if (reader_->name() == "model")
            {
                model = reader_->readElementText();
            }
            else if (reader_->name() == "vendor")
            {
                vendor = reader_->readElementText();
            }
            else if (reader_->name() == "factor")
            {
                factor = reader_->readElementText();
            }
        }
    }

    AddLens(vendor, model, factor.toDouble());
}

void KSUserDB::readFilter()
{
    QString model, vendor, type, color, lockedFilter;
    int offset = 0;
    double exposure = 1.0;
    bool useAutoFocus = false;
    int absFocusPos = 0;

    while (!reader_->atEnd())
    {
        reader_->readNext();

        if (reader_->isEndElement())
            break;

        if (reader_->isStartElement())
        {
            if (reader_->name() == "model")
            {
                model = reader_->readElementText();
            }
            else if (reader_->name() == "vendor")
            {
                vendor = reader_->readElementText();
            }
            else if (reader_->name() == "type")
            {
                type = reader_->readElementText();
            }
            else if (reader_->name() == "offset")
            {
                offset = reader_->readElementText().toInt();
            }
            else if (reader_->name() == "color")
            {
                color = reader_->readElementText();
            }
            else if (reader_->name() == "exposure")
            {
                exposure = reader_->readElementText().toDouble();
            }
            else if (reader_->name() == "lockedFilter")
            {
                lockedFilter = reader_->readElementText();
            }
            else if (reader_->name() == "useAutoFocus")
            {
                useAutoFocus = (reader_->readElementText() == "1");
            }
            else if (reader_->name() == "AbsoluteAutoFocus")
            {
                absFocusPos = (reader_->readElementText().toInt());
            }
        }
    }
    AddFilter(vendor, model, type, color, offset, exposure, useAutoFocus, lockedFilter, absFocusPos);
}

QList<ArtificialHorizonEntity *> KSUserDB::GetAllHorizons()
{
    QList<ArtificialHorizonEntity *> horizonList;

    userdb_.open();
    QSqlTableModel regions(nullptr, userdb_);
    regions.setTable("horizons");
    regions.select();

    QSqlTableModel points(nullptr, userdb_);

    for (int i = 0; i < regions.rowCount(); ++i)
    {
        QSqlRecord record   = regions.record(i);
        QString regionTable = record.value("name").toString();
        QString regionName  = record.value("label").toString();
        bool enabled        = record.value("enabled").toInt() == 1 ? true : false;

        points.setTable(regionTable);
        points.select();

        std::shared_ptr<LineList> skyList(new LineList());

        ArtificialHorizonEntity *horizon = new ArtificialHorizonEntity;

        horizon->setRegion(regionName);
        horizon->setEnabled(enabled);
        horizon->setList(skyList);

        horizonList.append(horizon);

        for (int j = 0; j < points.rowCount(); j++)
        {
            std::shared_ptr<SkyPoint> p(new SkyPoint());

            record = points.record(j);
            p->setAz(record.value(0).toDouble());
            p->setAlt(record.value(1).toDouble());
            p->HorizontalToEquatorial(KStarsData::Instance()->lst(), KStarsData::Instance()->geo()->lat());
            skyList->append(std::move(p));
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
    QSqlTableModel regions(nullptr, userdb_);
    regions.setEditStrategy(QSqlTableModel::OnManualSubmit);
    regions.setTable("horizons");
    regions.select();

    QSqlQuery query(userdb_);

    for (int i = 0; i < regions.rowCount(); ++i)
    {
        QSqlRecord record  = regions.record(i);
        QString tableQuery = QString("DROP TABLE %1").arg(record.value("name").toString());
        if (!query.exec(tableQuery))
            qCWarning(KSTARS) << query.lastError().text();
    }

    regions.removeRows(0, regions.rowCount());
    regions.submitAll();

    regions.clear();
    userdb_.close();
}

void KSUserDB::AddHorizon(ArtificialHorizonEntity *horizon)
{
    userdb_.open();
    QSqlTableModel regions(nullptr, userdb_);
    regions.setTable("horizons");

    regions.select();
    QString tableName = QString("horizon_%1").arg(regions.rowCount() + 1);

    regions.insertRow(0);
    regions.setData(regions.index(0, 1), tableName);
    regions.setData(regions.index(0, 2), horizon->region());
    regions.setData(regions.index(0, 3), horizon->enabled() ? 1 : 0);
    regions.submitAll();
    regions.clear();

    QString tableQuery = QString("CREATE TABLE %1 (Az REAL NOT NULL, Alt REAL NOT NULL)").arg(tableName);
    QSqlQuery query(userdb_);
    query.exec(tableQuery);

    QSqlTableModel points(nullptr, userdb_);

    points.setTable(tableName);

    SkyList *skyList = horizon->list()->points();

    for (const auto &item : *skyList)
    {
        points.select();
        QSqlRecord rec(points.record());

        rec.setValue("Az", item->az().Degrees());
        rec.setValue("Alt", item->alt().Degrees());
        points.insertRecord(-1, rec);
    }

    points.submitAll();
    points.clear();

    userdb_.close();
}

int KSUserDB::AddProfile(const QString &name)
{
    userdb_.open();
    int id = -1;

    QSqlQuery query(userdb_);
    bool rc = query.exec(QString("INSERT INTO profile (name) VALUES('%1')").arg(name));

    if (rc == false)
        qCWarning(KSTARS) << query.lastQuery() << query.lastError().text();
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
        qCWarning(KSTARS) << query.lastQuery() << query.lastError().text();

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
    if (!query.exec(QString("UPDATE profile SET "
                            "host=null,port=null,city=null,province=null,country=null,indiwebmanagerport=NULL,"
                            "autoconnect=NULL,primaryscope=0,guidescope=0 WHERE id=%1")
                    .arg(pi->id)))
        qCWarning(KSTARS) << query.executedQuery() << query.lastError().text();

    // Update Name
    if (!query.exec(QString("UPDATE profile SET name='%1' WHERE id=%2").arg(pi->name).arg(pi->id)))
        qCWarning(KSTARS) << query.executedQuery() << query.lastError().text();

    // Update Remote Data
    if (pi->host.isEmpty() == false)
    {
        if (!query.exec(
                    QString("UPDATE profile SET host='%1',port=%2 WHERE id=%3").arg(pi->host).arg((pi->port)).arg(pi->id)))
            qCWarning(KSTARS) << query.executedQuery() << query.lastError().text();

        if (pi->INDIWebManagerPort != -1)
        {
            if (!query.exec(QString("UPDATE profile SET indiwebmanagerport='%1' WHERE id=%2")
                            .arg(pi->INDIWebManagerPort)
                            .arg(pi->id)))
                qCWarning(KSTARS) << query.executedQuery() << query.lastError().text();
        }
    }

    // Update City Info
    if (pi->city.isEmpty() == false)
    {
        if (!query.exec(QString("UPDATE profile SET city='%1',province='%2',country='%3' WHERE id=%4")
                        .arg(pi->city, pi->province, pi->country)
                        .arg(pi->id)))
        {
            qCWarning(KSTARS) << query.executedQuery() << query.lastError().text();
        }
    }

    // Update Auto Connect Info
    if (!query.exec(QString("UPDATE profile SET autoconnect=%1 WHERE id=%2").arg(pi->autoConnect ? 1 : 0).arg(pi->id)))
        qCWarning(KSTARS) << query.executedQuery() << query.lastError().text();

    // Update Guide Application Info
    if (!query.exec(QString("UPDATE profile SET guidertype=%1 WHERE id=%2").arg(pi->guidertype).arg(pi->id)))
        qCWarning(KSTARS) << query.executedQuery() << query.lastError().text();

    // If using external guider
    if (pi->guidertype != 0)
    {
        if (!query.exec(QString("UPDATE profile SET guiderhost='%1' WHERE id=%2").arg(pi->guiderhost).arg(pi->id)))
            qCWarning(KSTARS) << query.executedQuery() << query.lastError().text();
        if (!query.exec(QString("UPDATE profile SET guiderport=%1 WHERE id=%2").arg(pi->guiderport).arg(pi->id)))
            qCWarning(KSTARS) << query.executedQuery() << query.lastError().text();
    }

    // Update scope selection
    if (!query.exec(QString("UPDATE profile SET primaryscope='%1' WHERE id=%2").arg(pi->primaryscope).arg(pi->id)))
        qCWarning(KSTARS) << query.executedQuery() << query.lastError().text();
    if (!query.exec(QString("UPDATE profile SET guidescope=%1 WHERE id=%2").arg(pi->guidescope).arg(pi->id)))
        qCWarning(KSTARS) << query.executedQuery() << query.lastError().text();

    // Update remote drivers
    if (!query.exec(QString("UPDATE profile SET remotedrivers='%1' WHERE id=%2").arg(pi->remotedrivers).arg(pi->id)))
        qCWarning(KSTARS) << query.executedQuery() << query.lastError().text();

    QMapIterator<QString, QString> i(pi->drivers);
    while (i.hasNext())
    {
        i.next();
        if (!query.exec(QString("INSERT INTO driver (label, role, profile) VALUES('%1','%2',%3)")
                        .arg(i.value(), i.key())
                        .arg(pi->id)))
        {
            qCWarning(KSTARS) << query.executedQuery() << query.lastError().text();
        }
    }

    /*if (pi->customDrivers.isEmpty() == false && !query.exec(QString("INSERT INTO custom_driver (drivers, profile) VALUES('%1',%2)").arg(pi->customDrivers).arg(pi->id)))
        qDebug()  << query.lastQuery() << query.lastError().text();*/

    userdb_.close();
}

void KSUserDB::GetAllProfiles(QList<std::shared_ptr<ProfileInfo>> &profiles)
{
    userdb_.open();
    QSqlTableModel profile(nullptr, userdb_);
    profile.setTable("profile");
    profile.select();

    for (int i = 0; i < profile.rowCount(); ++i)
    {
        QSqlRecord record = profile.record(i);

        int id       = record.value("id").toInt();
        QString name = record.value("name").toString();
        std::shared_ptr<ProfileInfo> pi(new ProfileInfo(id, name));

        // Add host and port
        pi->host = record.value("host").toString();
        pi->port = record.value("port").toInt();

        // City info
        pi->city     = record.value("city").toString();
        pi->province = record.value("province").toString();
        pi->country  = record.value("country").toString();

        pi->INDIWebManagerPort = record.value("indiwebmanagerport").toInt();
        pi->autoConnect        = (record.value("autoconnect").toInt() == 1);

        pi->guidertype = record.value("guidertype").toInt();
        if (pi->guidertype != 0)
        {
            pi->guiderhost = record.value("guiderhost").toString();
            pi->guiderport = record.value("guiderport").toInt();
        }

        pi->primaryscope = record.value("primaryscope").toInt();
        pi->guidescope = record.value("guidescope").toInt();

        pi->remotedrivers = record.value("remotedrivers").toString();

        GetProfileDrivers(pi.get());
        //GetProfileCustomDrivers(pi);

        profiles.append(pi);
    }

    profile.clear();
    userdb_.close();
}

void KSUserDB::GetProfileDrivers(ProfileInfo *pi)
{
    userdb_.open();

    QSqlTableModel driver(nullptr, userdb_);
    driver.setTable("driver");
    driver.setFilter("profile=" + QString::number(pi->id));
    if (driver.select() == false)
        qCWarning(KSTARS) << "Driver select error:" << driver.lastError().text();

    for (int i = 0; i < driver.rowCount(); ++i)
    {
        QSqlRecord record = driver.record(i);
        QString label     = record.value("label").toString();
        QString role      = record.value("role").toString();

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

void KSUserDB::DeleteProfileDrivers(ProfileInfo *pi)
{
    userdb_.open();

    QSqlQuery query(userdb_);

    /*if (!query.exec("DELETE FROM custom_driver WHERE profile=" + QString::number(pi->id)))
        qDebug() << query.lastQuery() << query.lastError().text();*/

    if (!query.exec("DELETE FROM driver WHERE profile=" + QString::number(pi->id)))
        qCWarning(KSTARS) << query.executedQuery() << query.lastError().text();

    userdb_.close();
}
