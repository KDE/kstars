/*
    SPDX-FileCopyrightText: 2012 Rishab Arora <ra.rishab@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

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
    m_UserDB.close();

    // Backup
    QString current_dbfile = QDir(KSPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath("userdb.sqlite");
    QString backup_dbfile = QDir(KSPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath("userdb.sqlite.backup");
    QFile::remove(backup_dbfile);
    QFile::copy(current_dbfile, backup_dbfile);
}

bool KSUserDB::Initialize()
{
    // Every logged in user has their own db.
    m_UserDB = QSqlDatabase::addDatabase("QSQLITE", "userdb");
    if (!m_UserDB.isValid())
    {
        qCCritical(KSTARS) << "Unable to prepare database of type sqlite!";
        return false;
    }

    // If the database file does not exist, look for a backup
    // If the database file exists and is empty, look for a backup
    // If the database file exists and has data, use it.
    // If the backup file does not exist, start fresh.
    // If the backup file exists and has data, replace and use it.
    // If the database file exists and has no data and the backup file exists, use it.
    // If the database file exists and has no data and no backup file exists, start fresh.

    QFileInfo dbfile(QDir(KSPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath("userdb.sqlite"));
    QFileInfo backup_file(QDir(KSPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath("userdb.sqlite.backup"));

    bool const first_run = !dbfile.exists() && !backup_file.exists();

    m_UserDB.setDatabaseName(dbfile.filePath());

    // If main fails to open and we have no backup, fail
    if (!m_UserDB.open())
    {
        if (!backup_file.exists())
        {
            qCCritical(KSTARS) << QString("Failed opening user database '%1'.").arg(dbfile.filePath());
            qCCritical(KSTARS) << LastError();
            return false;
        }
    }

    // If no main nor backup existed before opening, rebuild
    if (m_UserDB.isOpen() && first_run)
    {
        qCInfo(KSTARS) << "User DB does not exist. New User DB will be created.";
        FirstRun();
    }

    // If main appears empty/corrupted, restore if possible or rebuild
    if (m_UserDB.tables().empty())
    {
        if (backup_file.exists())
        {
            m_UserDB.close();
            qCWarning(KSTARS) << "Detected corrupted database. Attempting to recover from backup...";
            QFile::remove(dbfile.filePath());
            QFile::copy(backup_file.filePath(), dbfile.filePath());
            QFile::remove(backup_file.filePath());
            return Initialize();
        }
        else if (!FirstRun())
        {
            qCCritical(KSTARS) << QString("Failed initializing user database '%1.").arg(dbfile.filePath());
            return false;
        }
    }

    qCDebug(KSTARS) << "Opened the User DB. Ready.";

    // Update table if previous version exists
    QSqlTableModel version(nullptr, m_UserDB);
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
        QSqlQuery query(m_UserDB);
        QString versionQuery = QString("UPDATE Version SET Version=%1").arg(SCHEMA_VERSION);
        if (!query.exec(versionQuery))
            qCWarning(KSTARS) << query.lastError();
    }

    // If prior to 2.9.4 extend filters table
    if (currentDBVersion < 294)
    {
        QSqlQuery query(m_UserDB);

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
        QSqlQuery query(m_UserDB);

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
        QSqlQuery query(m_UserDB);
        QString columnQuery = QString("ALTER TABLE profile ADD COLUMN remotedrivers TEXT DEFAULT NULL");
        if (!query.exec(columnQuery))
            qCWarning(KSTARS) << query.lastError();

        if (m_UserDB.tables().contains("customdrivers") == false)
        {
            QSqlQuery query(m_UserDB);

            if (!query.exec("CREATE TABLE customdrivers ( "
                            "id INTEGER DEFAULT NULL PRIMARY KEY AUTOINCREMENT, "
                            "Name TEXT DEFAULT NULL, "
                            "Label TEXT DEFAULT NULL UNIQUE, "
                            "Manufacturer TEXT DEFAULT NULL, "
                            "Family TEXT DEFAULT NULL, "
                            "Exec TEXT DEFAULT NULL, "
                            "Version TEXT DEFAULT 1.0)"))
                qCWarning(KSTARS) << query.lastError();
        }
    }

    // Add manufacturer
    if (currentDBVersion < 305)
    {
        QSqlQuery query(m_UserDB);
        QString columnQuery = QString("ALTER TABLE customdrivers ADD COLUMN Manufacturer TEXT DEFAULT NULL");
        if (!query.exec(columnQuery))
            qCWarning(KSTARS) << query.lastError();
    }

    // Add indihub
    if (currentDBVersion < 306)
    {
        QSqlQuery query(m_UserDB);
        QString columnQuery = QString("ALTER TABLE profile ADD COLUMN indihub INTEGER DEFAULT 0");
        if (!query.exec(columnQuery))
            qCWarning(KSTARS) << query.lastError();
    }

    // Add Defect Map
    if (currentDBVersion < 307)
    {
        QSqlQuery query(m_UserDB);
        // If we are upgrading, remove all previous entries.
        QString clearQuery = QString("DELETE FROM darkframe");
        if (!query.exec(clearQuery))
            qCWarning(KSTARS) << query.lastError();
        QString columnQuery = QString("ALTER TABLE darkframe ADD COLUMN defectmap TEXT DEFAULT NULL");
        if (!query.exec(columnQuery))
            qCWarning(KSTARS) << query.lastError();
    }

    // Add port selector
    if (currentDBVersion < 308)
    {
        QSqlQuery query(m_UserDB);
        QString columnQuery = QString("ALTER TABLE profile ADD COLUMN portselector INTEGER DEFAULT 0");
        if (!query.exec(columnQuery))
            qCWarning(KSTARS) << query.lastError();
    }

    m_UserDB.close();
    return true;
}

QSqlError KSUserDB::LastError()
{
    // error description is in QSqlError::text()
    return m_UserDB.lastError();
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

    // Note: enabled now encodes both a bool enabled value as well
    // as another bool indicating if this is a horizon line or a ceiling line.
    tables.append("CREATE TABLE horizons ( "
                  "id INTEGER DEFAULT NULL PRIMARY KEY AUTOINCREMENT, "
                  "name TEXT NOT NULL,"
                  "label TEXT NOT NULL,"
                  "enabled INTEGER NOT NULL)");

    tables.append("CREATE TABLE profile (id INTEGER DEFAULT NULL PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL, host "
                  "TEXT, port INTEGER, city TEXT, province TEXT, country TEXT, indiwebmanagerport INTEGER DEFAULT "
                  "NULL, autoconnect INTEGER DEFAULT 1, guidertype INTEGER DEFAULT 0, guiderhost TEXT, guiderport INTEGER,"
                  "primaryscope INTEGER DEFAULT 0, guidescope INTEGER DEFAULT 0, indihub INTEGER DEFAULT 0,"
                  "portselector INTEGER DEFAULT 1, remotedrivers TEXT DEFAULT NULL)");
    tables.append("CREATE TABLE driver (id INTEGER DEFAULT NULL PRIMARY KEY AUTOINCREMENT, label TEXT NOT NULL, role "
                  "TEXT NOT NULL, profile INTEGER NOT NULL, FOREIGN KEY(profile) REFERENCES profile(id))");
    //tables.append("CREATE TABLE custom_driver (id INTEGER DEFAULT NULL PRIMARY KEY AUTOINCREMENT, drivers TEXT NOT NULL, profile INTEGER NOT NULL, FOREIGN KEY(profile) REFERENCES profile(id))");

#ifdef Q_OS_WIN
    tables.append("INSERT INTO profile (name, host, port) VALUES ('Simulators', 'localhost', 7624)");
#else
    tables.append("INSERT INTO profile (name, portselector) VALUES ('Simulators', 0)");
#endif

    tables.append("INSERT INTO driver (label, role, profile) VALUES ('Telescope Simulator', 'Mount', 1)");
    tables.append("INSERT INTO driver (label, role, profile) VALUES ('CCD Simulator', 'CCD', 1)");
    tables.append("INSERT INTO driver (label, role, profile) VALUES ('Focuser Simulator', 'Focuser', 1)");

    tables.append("CREATE TABLE IF NOT EXISTS darkframe (id INTEGER DEFAULT NULL PRIMARY KEY AUTOINCREMENT, ccd TEXT "
                  "NOT NULL, chip INTEGER DEFAULT 0, binX INTEGER, binY INTEGER, temperature REAL, duration REAL, "
                  "filename TEXT NOT NULL, defectmap TEXT DEFAULT NULL, timestamp DATETIME DEFAULT CURRENT_TIMESTAMP)");

    tables.append("CREATE TABLE IF NOT EXISTS hips (ID TEXT NOT NULL UNIQUE,"
                  "obs_title TEXT NOT NULL, obs_description TEXT NOT NULL, hips_order TEXT NOT NULL,"
                  "hips_frame TEXT NOT NULL, hips_tile_width TEXT NOT NULL, hips_tile_format TEXT NOT NULL,"
                  "hips_service_url TEXT NOT NULL, moc_sky_fraction TEXT NOT NULL)");

    tables.append("INSERT INTO hips (ID, obs_title, obs_description, hips_order, hips_frame, hips_tile_width, hips_tile_format, hips_service_url, moc_sky_fraction)"
                  "VALUES ('CDS/P/DSS2/color', 'DSS Colored', 'Color composition generated by CDS. This HiPS survey is based on 2 others HiPS surveys,"
                  " respectively DSS2-red and DSS2-blue HiPS, both of them directly generated from original scanned plates downloaded"
                  " from STScI site. The red component has been built from POSS-II F, AAO-SES,SR and SERC-ER plates. The blue component"
                  " has been build from POSS-II J and SERC-J,EJ. The green component is based on the mean of other components. Three"
                  " missing plates from red survey (253, 260, 359) has been replaced by pixels from the DSSColor STScI jpeg survey."
                  " The 11 missing blue plates (mainly in galactic plane) have not been replaced (only red component).',"
                  "'9', 'equatorial', '512', 'jpeg fits', 'http://alasky.u-strasbg.fr/DSS/DSSColor','1')");

    tables.append("INSERT INTO hips (ID, obs_title, obs_description, hips_order, hips_frame, hips_tile_width, hips_tile_format, hips_service_url, moc_sky_fraction)"
                  "VALUES ('CDS/P/2MASS/color', '2MASS Color J (1.23 microns), H (1.66 microns), K (2.16 microns)',"
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
                  "VALUES ('CDS/P/Fermi/color', 'Fermi Color HEALPix Survey', 'Launched on June 11, 2008, the Fermi Gamma-ray Space Telescope observes the cosmos using the"
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
                  "Manufacturer TEXT DEFAULT NULL, "
                  "Family TEXT DEFAULT NULL, "
                  "Exec TEXT DEFAULT NULL, "
                  "Version TEXT DEFAULT 1.0)");

    for (int i = 0; i < tables.count(); ++i)
    {
        QSqlQuery query(m_UserDB);
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
    m_UserDB.open();
    QSqlTableModel users(nullptr, m_UserDB);
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

    m_UserDB.close();
}

bool KSUserDB::FindObserver(const QString &name, const QString &surname)
{
    m_UserDB.open();
    QSqlTableModel users(nullptr, m_UserDB);
    users.setTable("user");
    users.setFilter("Name LIKE \'" + name + "\' AND Surname LIKE \'" + surname + "\'");
    users.select();

    int observer_count = users.rowCount();

    users.clear();
    m_UserDB.close();
    return (observer_count > 0);
}

// TODO(spacetime): This method is currently unused.
bool KSUserDB::DeleteObserver(const QString &id)
{
    m_UserDB.open();
    QSqlTableModel users(nullptr, m_UserDB);
    users.setTable("user");
    users.setFilter("id = \'" + id + "\'");
    users.select();

    users.removeRows(0, 1);
    users.submitAll();

    int observer_count = users.rowCount();

    users.clear();
    m_UserDB.close();
    return (observer_count > 0);
}
QSqlDatabase KSUserDB::GetDatabase()
{
    m_UserDB.open();
    return m_UserDB;
}
#ifndef KSTARS_LITE
void KSUserDB::GetAllObservers(QList<Observer *> &observer_list)
{
    m_UserDB.open();
    observer_list.clear();
    QSqlTableModel users(nullptr, m_UserDB);
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
    m_UserDB.close();
}
#endif

/* Dark Library Section */

/**
 * @brief KSUserDB::AddDarkFrame Saves a new dark frame data to the database
 * @param oneFrame Map that contains 1 to 1 correspondence with the database table, except for primary key and timestamp.
 */
void KSUserDB::AddDarkFrame(const QVariantMap &oneFrame)
{
    m_UserDB.open();
    QSqlTableModel darkframe(nullptr, m_UserDB);
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

    m_UserDB.close();
}

/**
 * @brief KSUserDB::UpdateDarkFrame Updates an existing dark frame record in the data, replace all values matching the supplied ID
 * @param oneFrame dark frame to update. The ID should already exist in the database.
 */
void KSUserDB::UpdateDarkFrame(const QVariantMap &oneFrame)
{
    m_UserDB.open();
    QSqlTableModel darkframe(nullptr, m_UserDB);
    darkframe.setTable("darkframe");
    darkframe.setFilter(QString("id=%1").arg(oneFrame["id"].toInt()));
    darkframe.select();

    QSqlRecord record = darkframe.record(0);
    for (QVariantMap::const_iterator iter = oneFrame.begin(); iter != oneFrame.end(); ++iter)
        record.setValue(iter.key(), iter.value());

    darkframe.setRecord(0, record);
    darkframe.submitAll();
    m_UserDB.close();
}

/**
 * @brief KSUserDB::DeleteDarkFrame Delete from database a dark frame record that matches the filename field.
 * @param filename filename of dark frame to delete from database.
 */
void KSUserDB::DeleteDarkFrame(const QString &filename)
{
    m_UserDB.open();
    QSqlTableModel darkframe(nullptr, m_UserDB);
    darkframe.setTable("darkframe");
    darkframe.setFilter("filename = \'" + filename + "\'");

    darkframe.select();

    darkframe.removeRows(0, 1);
    darkframe.submitAll();

    m_UserDB.close();
}

void KSUserDB::GetAllDarkFrames(QList<QVariantMap> &darkFrames)
{
    darkFrames.clear();

    m_UserDB.open();
    QSqlTableModel darkframe(nullptr, m_UserDB);
    darkframe.setTable("darkframe");
    darkframe.select();

    for (int i = 0; i < darkframe.rowCount(); ++i)
    {
        QVariantMap recordMap;
        QSqlRecord record = darkframe.record(i);
        for (int j = 0; j < record.count(); j++)
            recordMap[record.fieldName(j)] = record.value(j);

        darkFrames.append(recordMap);
    }

    m_UserDB.close();
}


/* Effective FOV Section */

void KSUserDB::AddEffectiveFOV(const QVariantMap &oneFOV)
{
    m_UserDB.open();
    QSqlTableModel effectivefov(nullptr, m_UserDB);
    effectivefov.setTable("effectivefov");
    effectivefov.select();

    QSqlRecord record = effectivefov.record();

    // Remove PK so that it gets auto-incremented later
    record.remove(0);

    for (QVariantMap::const_iterator iter = oneFOV.begin(); iter != oneFOV.end(); ++iter)
        record.setValue(iter.key(), iter.value());

    effectivefov.insertRecord(-1, record);

    effectivefov.submitAll();

    m_UserDB.close();
}

bool KSUserDB::DeleteEffectiveFOV(const QString &id)
{
    m_UserDB.open();
    QSqlTableModel effectivefov(nullptr, m_UserDB);
    effectivefov.setTable("effectivefov");
    effectivefov.setFilter("id = \'" + id + "\'");

    effectivefov.select();

    effectivefov.removeRows(0, 1);
    effectivefov.submitAll();

    m_UserDB.close();

    return true;
}

void KSUserDB::GetAllEffectiveFOVs(QList<QVariantMap> &effectiveFOVs)
{
    effectiveFOVs.clear();

    m_UserDB.open();
    QSqlTableModel effectivefov(nullptr, m_UserDB);
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

    m_UserDB.close();
}

/* Driver Alias Section */

bool KSUserDB::AddCustomDriver(const QVariantMap &oneDriver)
{
    m_UserDB.open();
    QSqlTableModel CustomDriver(nullptr, m_UserDB);
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

    m_UserDB.close();

    return rc;
}

bool KSUserDB::DeleteCustomDriver(const QString &id)
{
    m_UserDB.open();
    QSqlTableModel CustomDriver(nullptr, m_UserDB);
    CustomDriver.setTable("customdrivers");
    CustomDriver.setFilter("id = \'" + id + "\'");

    CustomDriver.select();

    CustomDriver.removeRows(0, 1);
    CustomDriver.submitAll();

    m_UserDB.close();

    return true;
}

void KSUserDB::GetAllCustomDrivers(QList<QVariantMap> &CustomDrivers)
{
    CustomDrivers.clear();

    m_UserDB.open();
    QSqlTableModel CustomDriver(nullptr, m_UserDB);
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

    m_UserDB.close();
}

/* HiPS Section */

void KSUserDB::AddHIPSSource(const QMap<QString, QString> &oneSource)
{
    m_UserDB.open();
    QSqlTableModel HIPSSource(nullptr, m_UserDB);
    HIPSSource.setTable("hips");
    HIPSSource.select();

    QSqlRecord record = HIPSSource.record();

    for (QMap<QString, QString>::const_iterator iter = oneSource.begin(); iter != oneSource.end(); ++iter)
        record.setValue(iter.key(), iter.value());

    HIPSSource.insertRecord(-1, record);

    HIPSSource.submitAll();

    m_UserDB.close();
}

bool KSUserDB::DeleteHIPSSource(const QString &ID)
{
    m_UserDB.open();
    QSqlTableModel HIPSSource(nullptr, m_UserDB);
    HIPSSource.setTable("hips");
    HIPSSource.setFilter("ID = \'" + ID + "\'");

    HIPSSource.select();

    HIPSSource.removeRows(0, 1);
    HIPSSource.submitAll();

    m_UserDB.close();

    return true;
}

void KSUserDB::GetAllHIPSSources(QList<QMap<QString, QString>> &HIPSSources)
{
    HIPSSources.clear();

    m_UserDB.open();
    QSqlTableModel HIPSSource(nullptr, m_UserDB);
    HIPSSource.setTable("hips");
    HIPSSource.select();

    for (int i = 0; i < HIPSSource.rowCount(); ++i)
    {
        QMap<QString, QString> recordMap;
        QSqlRecord record = HIPSSource.record(i);
        for (int j = 1; j < record.count(); j++)
            recordMap[record.fieldName(j)] = record.value(j).toString();

        HIPSSources.append(recordMap);
    }

    m_UserDB.close();
}


/* DSLR Section */

void KSUserDB::AddDSLRInfo(const QMap<QString, QVariant> &oneInfo)
{
    m_UserDB.open();
    QSqlTableModel DSLRInfo(nullptr, m_UserDB);
    DSLRInfo.setTable("dslr");
    DSLRInfo.select();

    QSqlRecord record = DSLRInfo.record();

    for (QMap<QString, QVariant>::const_iterator iter = oneInfo.begin(); iter != oneInfo.end(); ++iter)
        record.setValue(iter.key(), iter.value());

    DSLRInfo.insertRecord(-1, record);

    DSLRInfo.submitAll();

    m_UserDB.close();
}

bool KSUserDB::DeleteAllDSLRInfo()
{
    m_UserDB.open();
    QSqlTableModel DSLRInfo(nullptr, m_UserDB);
    DSLRInfo.setTable("dslr");
    DSLRInfo.select();

    DSLRInfo.removeRows(0, DSLRInfo.rowCount());
    DSLRInfo.submitAll();

    m_UserDB.close();

    return true;
}

bool KSUserDB::DeleteDSLRInfo(const QString &model)
{
    m_UserDB.open();
    QSqlTableModel DSLRInfo(nullptr, m_UserDB);
    DSLRInfo.setTable("dslr");
    DSLRInfo.setFilter("model = \'" + model + "\'");

    DSLRInfo.select();

    DSLRInfo.removeRows(0, 1);
    DSLRInfo.submitAll();

    m_UserDB.close();

    return true;
}

void KSUserDB::GetAllDSLRInfos(QList<QMap<QString, QVariant>> &DSLRInfos)
{
    DSLRInfos.clear();

    m_UserDB.open();
    QSqlTableModel DSLRInfo(nullptr, m_UserDB);
    DSLRInfo.setTable("dslr");
    DSLRInfo.select();

    for (int i = 0; i < DSLRInfo.rowCount(); ++i)
    {
        QMap<QString, QVariant> recordMap;
        QSqlRecord record = DSLRInfo.record(i);
        for (int j = 1; j < record.count(); j++)
            recordMap[record.fieldName(j)] = record.value(j);

        DSLRInfos.append(recordMap);
    }

    m_UserDB.close();
}

/*
 * Flag Section
*/

void KSUserDB::DeleteAllFlags()
{
    m_UserDB.open();
    QSqlTableModel flags(nullptr, m_UserDB);
    flags.setEditStrategy(QSqlTableModel::OnManualSubmit);
    flags.setTable("flags");
    flags.select();

    flags.removeRows(0, flags.rowCount());
    flags.submitAll();

    flags.clear();
    m_UserDB.close();
}

void KSUserDB::AddFlag(const QString &ra, const QString &dec, const QString &epoch, const QString &image_name,
                       const QString &label, const QString &labelColor)
{
    m_UserDB.open();
    QSqlTableModel flags(nullptr, m_UserDB);
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
    m_UserDB.close();
}

QList<QStringList> KSUserDB::GetAllFlags()
{
    QList<QStringList> flagList;

    m_UserDB.open();
    QSqlTableModel flags(nullptr, m_UserDB);
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
    m_UserDB.close();
    return flagList;
}

/*
 * Generic Section
 */
void KSUserDB::DeleteEquipment(const QString &type, const int &id)
{
    m_UserDB.open();
    QSqlTableModel equip(nullptr, m_UserDB);
    equip.setTable(type);
    equip.setFilter("id = " + QString::number(id));
    equip.select();

    equip.removeRows(0, equip.rowCount());
    equip.submitAll();

    equip.clear();
    m_UserDB.close();
}

void KSUserDB::DeleteAllEquipment(const QString &type)
{
    m_UserDB.open();
    QSqlTableModel equip(nullptr, m_UserDB);
    equip.setEditStrategy(QSqlTableModel::OnManualSubmit);
    equip.setTable(type);
    equip.setFilter("id >= 1");
    equip.select();
    equip.removeRows(0, equip.rowCount());
    equip.submitAll();

    equip.clear();
    m_UserDB.close();
}

/*
 * Telescope section
 */
void KSUserDB::AddScope(const QString &model, const QString &vendor, const QString &driver, const QString &type,
                        const double &focalLength, const double &aperture)
{
    m_UserDB.open();
    QSqlTableModel equip(nullptr, m_UserDB);
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
    m_UserDB.close();
}

void KSUserDB::AddScope(const QString &model, const QString &vendor, const QString &driver, const QString &type,
                        const double &focalLength, const double &aperture, const QString &id)
{
    m_UserDB.open();
    QSqlTableModel equip(nullptr, m_UserDB);
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

    m_UserDB.close();
}
#ifndef KSTARS_LITE
void KSUserDB::GetAllScopes(QList<Scope *> &scope_list)
{
    scope_list.clear();

    m_UserDB.open();
    QSqlTableModel equip(nullptr, m_UserDB);
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
    m_UserDB.close();
}
#endif
/*
 * Eyepiece section
 */
void KSUserDB::AddEyepiece(const QString &vendor, const QString &model, const double &focalLength, const double &fov,
                           const QString &fovunit)
{
    m_UserDB.open();
    QSqlTableModel equip(nullptr, m_UserDB);
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
    m_UserDB.close();
}

void KSUserDB::AddEyepiece(const QString &vendor, const QString &model, const double &focalLength, const double &fov,
                           const QString &fovunit, const QString &id)
{
    m_UserDB.open();
    QSqlTableModel equip(nullptr, m_UserDB);
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

    m_UserDB.close();
}
#ifndef KSTARS_LITE
void KSUserDB::GetAllEyepieces(QList<OAL::Eyepiece *> &eyepiece_list)
{
    eyepiece_list.clear();

    m_UserDB.open();
    QSqlTableModel equip(nullptr, m_UserDB);
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
    m_UserDB.close();
}
#endif
/*
 * lens section
 */
void KSUserDB::AddLens(const QString &vendor, const QString &model, const double &factor)
{
    m_UserDB.open();
    QSqlTableModel equip(nullptr, m_UserDB);
    equip.setTable("lens");

    int row = 0;
    equip.insertRows(row, 1);
    equip.setData(equip.index(row, 1), vendor); // row,0 is autoincerement ID
    equip.setData(equip.index(row, 2), model);
    equip.setData(equip.index(row, 3), factor);
    equip.submitAll();

    equip.clear();
    m_UserDB.close();
}

void KSUserDB::AddLens(const QString &vendor, const QString &model, const double &factor, const QString &id)
{
    m_UserDB.open();
    QSqlTableModel equip(nullptr, m_UserDB);
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

    m_UserDB.close();
}
#ifndef KSTARS_LITE
void KSUserDB::GetAllLenses(QList<OAL::Lens *> &lens_list)
{
    lens_list.clear();

    m_UserDB.open();
    QSqlTableModel equip(nullptr, m_UserDB);
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
    m_UserDB.close();
}
#endif
/*
 *  filter section
 */
void KSUserDB::AddFilter(const QString &vendor, const QString &model, const QString &type, const QString &color,
                         int offset, double exposure, bool useAutoFocus, const QString &lockedFilter, int absFocusPos)
{
    if (m_UserDB.open())
    {
        QSqlTableModel equip(nullptr, m_UserDB);
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
        m_UserDB.close();
    }
    else qCritical() << "Failed opening database connection to add filters.";
}

void KSUserDB::AddFilter(const QString &vendor, const QString &model, const QString &type, const QString &color,
                         int offset, double exposure, bool useAutoFocus, const QString &lockedFilter, int absFocusPos, const QString &id)
{
    m_UserDB.open();
    QSqlTableModel equip(nullptr, m_UserDB);
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

    m_UserDB.close();
}
#ifndef KSTARS_LITE
void KSUserDB::GetAllFilters(QList<OAL::Filter *> &filter_list)
{
    m_UserDB.open();
    filter_list.clear();
    QSqlTableModel equip(nullptr, m_UserDB);
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
        OAL::Filter *o    = new OAL::Filter(id, model, vendor, type, color, exposure, offset, useAutoFocus, lockedFilter,
                                            absFocusPos);
        filter_list.append(o);
    }

    equip.clear();
    m_UserDB.close();
}
#endif
#if 0
bool KSUserDB::ImportFlags()
{
    QString flagfilename = QDir(KSPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath("flags.dat");
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
    KSParser flagparser(flagfilename, '#', flag_file_sequence, ' ');

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

        AddFlag(ra, dec, epoch, icon, label, color);
    }
    return true;
}

bool KSUserDB::ImportUsers()
{
    QString usersfilename = QDir(KSPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath("observerlist.xml");
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
    QString equipfilename = QDir(KSPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath("equipmentlist.xml");
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
                    else if( reader_->name() == "lenses" )
                        readLenses();
                    else if( reader_->name() == "filters" )
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
                if (type == "RC")
                    type = "Ritchey-Chretien";
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

    m_UserDB.open();
    QSqlTableModel regions(nullptr, m_UserDB);
    regions.setTable("horizons");
    regions.select();

    QSqlTableModel points(nullptr, m_UserDB);

    for (int i = 0; i < regions.rowCount(); ++i)
    {
        QSqlRecord record         = regions.record(i);
        const QString regionTable = record.value("name").toString();
        const QString regionName  = record.value("label").toString();

        const int flags           = record.value("enabled").toInt();
        const bool enabled        = flags & 0x1 ? true : false;
        const bool ceiling        = flags & 0x2 ? true : false;

        points.setTable(regionTable);
        points.select();

        std::shared_ptr<LineList> skyList(new LineList());

        ArtificialHorizonEntity *horizon = new ArtificialHorizonEntity;

        horizon->setRegion(regionName);
        horizon->setEnabled(enabled);
        horizon->setCeiling(ceiling);
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
    m_UserDB.close();
    return horizonList;
}

void KSUserDB::DeleteAllHorizons()
{
    m_UserDB.open();
    QSqlTableModel regions(nullptr, m_UserDB);
    regions.setEditStrategy(QSqlTableModel::OnManualSubmit);
    regions.setTable("horizons");
    regions.select();

    QSqlQuery query(m_UserDB);

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
    m_UserDB.close();
}

void KSUserDB::AddHorizon(ArtificialHorizonEntity *horizon)
{
    m_UserDB.open();
    QSqlTableModel regions(nullptr, m_UserDB);
    regions.setTable("horizons");

    regions.select();
    QString tableName = QString("horizon_%1").arg(regions.rowCount() + 1);

    regions.insertRow(0);
    regions.setData(regions.index(0, 1), tableName);
    regions.setData(regions.index(0, 2), horizon->region());
    int flags = 0;
    if (horizon->enabled()) flags |= 0x1;
    if (horizon->ceiling()) flags |= 0x2;
    regions.setData(regions.index(0, 3), flags);
    regions.submitAll();
    regions.clear();

    QString tableQuery = QString("CREATE TABLE %1 (Az REAL NOT NULL, Alt REAL NOT NULL)").arg(tableName);
    QSqlQuery query(m_UserDB);
    query.exec(tableQuery);

    QSqlTableModel points(nullptr, m_UserDB);

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

    m_UserDB.close();
}

int KSUserDB::AddProfile(const QString &name)
{
    m_UserDB.open();
    int id = -1;

    QSqlQuery query(m_UserDB);
    bool rc = query.exec(QString("INSERT INTO profile (name) VALUES('%1')").arg(name));

    if (rc == false)
        qCWarning(KSTARS) << query.lastQuery() << query.lastError().text();
    else
        id = query.lastInsertId().toInt();

    m_UserDB.close();

    return id;
}

bool KSUserDB::DeleteProfile(ProfileInfo *pi)
{
    m_UserDB.open();

    QSqlQuery query(m_UserDB);
    bool rc;

    rc = query.exec("DELETE FROM profile WHERE id=" + QString::number(pi->id));

    if (rc == false)
        qCWarning(KSTARS) << query.lastQuery() << query.lastError().text();

    m_UserDB.close();

    return rc;
}

void KSUserDB::SaveProfile(ProfileInfo *pi)
{
    // Remove all drivers
    DeleteProfileDrivers(pi);

    m_UserDB.open();
    QSqlQuery query(m_UserDB);

    // Clear data
    if (!query.exec(QString("UPDATE profile SET "
                            "host=null,port=null,city=null,province=null,country=null,indiwebmanagerport=NULL,"
                            "autoconnect=NULL,portselector=NULL,primaryscope=0,guidescope=0,indihub=0 WHERE id=%1")
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

    // Update Port Selector Info
    if (!query.exec(QString("UPDATE profile SET portselector=%1 WHERE id=%2").arg(pi->portSelector ? 1 : 0).arg(pi->id)))
        qCWarning(KSTARS) << query.executedQuery() << query.lastError().text();

    // Update Guide Application Info
    if (!query.exec(QString("UPDATE profile SET guidertype=%1 WHERE id=%2").arg(pi->guidertype).arg(pi->id)))
        qCWarning(KSTARS) << query.executedQuery() << query.lastError().text();

    // Update INDI Hub
    if (!query.exec(QString("UPDATE profile SET indihub=%1 WHERE id=%2").arg(pi->indihub).arg(pi->id)))
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

    m_UserDB.close();
}

void KSUserDB::GetAllProfiles(QList<std::shared_ptr<ProfileInfo>> &profiles)
{
    m_UserDB.open();
    QSqlTableModel profile(nullptr, m_UserDB);
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
        pi->portSelector       = (record.value("portselector").toInt() == 1);

        pi->indihub = record.value("indihub").toInt();

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
    m_UserDB.close();
}

void KSUserDB::GetProfileDrivers(ProfileInfo *pi)
{
    m_UserDB.open();

    QSqlTableModel driver(nullptr, m_UserDB);
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
    m_UserDB.close();
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
    m_UserDB.open();

    QSqlQuery query(m_UserDB);

    /*if (!query.exec("DELETE FROM custom_driver WHERE profile=" + QString::number(pi->id)))
        qDebug() << query.lastQuery() << query.lastError().text();*/

    if (!query.exec("DELETE FROM driver WHERE profile=" + QString::number(pi->id)))
        qCWarning(KSTARS) << query.executedQuery() << query.lastError().text();

    m_UserDB.close();
}
