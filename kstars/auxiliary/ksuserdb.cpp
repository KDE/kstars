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
#include "oal/dslrlens.h"
#include "imageoverlaycomponent.h"
#ifdef HAVE_INDI
#include "tools/imagingplanner.h"
#endif

#include <QSqlQuery>
#include <QSqlRecord>
#include <QSqlTableModel>

#include <QJsonDocument>

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
    // Backup
    QString current_dbfile = QDir(KSPaths::writableLocation(QStandardPaths::AppLocalDataLocation)).filePath("userdb.sqlite");
    QString backup_dbfile = QDir(KSPaths::writableLocation(
                                     QStandardPaths::AppLocalDataLocation)).filePath("userdb.sqlite.backup");
    QFile::remove(backup_dbfile);
    QFile::copy(current_dbfile, backup_dbfile);
}

bool KSUserDB::Initialize()
{
    // If the database file does not exist, look for a backup
    // If the database file exists and is empty, look for a backup
    // If the database file exists and has data, use it.
    // If the backup file does not exist, start fresh.
    // If the backup file exists and has data, replace and use it.
    // If the database file exists and has no data and the backup file exists, use it.
    // If the database file exists and has no data and no backup file exists, start fresh.

    QFileInfo dbfile(QDir(KSPaths::writableLocation(QStandardPaths::AppLocalDataLocation)).filePath("userdb.sqlite"));
    QFileInfo backup_file(QDir(KSPaths::writableLocation(
                                   QStandardPaths::AppLocalDataLocation)).filePath("userdb.sqlite.backup"));

    bool const first_run = !dbfile.exists() && !backup_file.exists();
    m_ConnectionName = dbfile.filePath();

    // Every logged in user has their own db.
    auto db = QSqlDatabase::addDatabase("QSQLITE", m_ConnectionName);
    // This would load the SQLITE file
    db.setDatabaseName(m_ConnectionName);

    if (!db.isValid())
    {
        qCCritical(KSTARS) << "Unable to prepare database of type sqlite!";
        return false;
    }

    // If main fails to open and we have no backup, fail
    if (!db.open())
    {
        if (!backup_file.exists())
        {
            qCCritical(KSTARS) << QString("Failed opening user database '%1'.").arg(dbfile.filePath());
            qCCritical(KSTARS) << db.lastError();
            return false;
        }
    }

    // If no main nor backup existed before opening, rebuild
    if (db.isOpen() && first_run)
    {
        qCInfo(KSTARS) << "User DB does not exist. New User DB will be created.";
        FirstRun();
    }

    // If main appears empty/corrupted, restore if possible or rebuild
    if (db.tables().empty())
    {
        if (backup_file.exists())
        {

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
    QSqlTableModel version(nullptr, db);
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
        QSqlQuery query(db);
        QString versionQuery = QString("UPDATE Version SET Version=%1").arg(SCHEMA_VERSION);
        if (!query.exec(versionQuery))
            qCWarning(KSTARS) << query.lastError();
    }

    // If prior to 2.9.4 extend filters table
    if (currentDBVersion < 294)
    {
        QSqlQuery query(db);

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
        QSqlQuery query(db);

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
        QSqlQuery query(db);
        QString columnQuery = QString("ALTER TABLE profile ADD COLUMN remotedrivers TEXT DEFAULT NULL");
        if (!query.exec(columnQuery))
            qCWarning(KSTARS) << query.lastError();

        if (db.tables().contains("customdrivers") == false)
        {
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
        QSqlQuery query(db);
        QString columnQuery = QString("ALTER TABLE customdrivers ADD COLUMN Manufacturer TEXT DEFAULT NULL");
        if (!query.exec(columnQuery))
            qCWarning(KSTARS) << query.lastError();
    }

    // Add indihub
    if (currentDBVersion < 306)
    {
        QSqlQuery query(db);
        QString columnQuery = QString("ALTER TABLE profile ADD COLUMN indihub INTEGER DEFAULT 0");
        if (!query.exec(columnQuery))
            qCWarning(KSTARS) << query.lastError();
    }

    // Add Defect Map
    if (currentDBVersion < 307)
    {
        QSqlQuery query(db);
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
        QSqlQuery query(db);
        QString columnQuery = QString("ALTER TABLE profile ADD COLUMN portselector INTEGER DEFAULT 0");
        if (!query.exec(columnQuery))
            qCWarning(KSTARS) << query.lastError();
    }

    // Add Gain/ISO to Dark Library
    if (currentDBVersion < 309)
    {
        QSqlQuery query(db);
        QString columnQuery = QString("ALTER TABLE darkframe ADD COLUMN gain INTEGER DEFAULT -1");
        if (!query.exec(columnQuery))
            qCWarning(KSTARS) << query.lastError();
        columnQuery = QString("ALTER TABLE darkframe ADD COLUMN iso TEXT DEFAULT NULL");
        if (!query.exec(columnQuery))
            qCWarning(KSTARS) << query.lastError();
    }

    // Add scripts to profile
    if (currentDBVersion < 310)
    {
        QSqlQuery query(db);
        QString columnQuery = QString("ALTER TABLE profile ADD COLUMN scripts TEXT DEFAULT NULL");
        if (!query.exec(columnQuery))
            qCWarning(KSTARS) << query.lastError();
    }

    // Add optical trains
    if (currentDBVersion < 311)
    {
        QSqlQuery query(db);
        if (!query.exec("CREATE TABLE opticaltrains ( "
                        "id INTEGER DEFAULT NULL PRIMARY KEY AUTOINCREMENT, "
                        "profile INTEGER DEFAULT NULL, "
                        "name TEXT DEFAULT NULL, "
                        "mount TEXT DEFAULT NULL, "
                        "dustcap TEXT DEFAULT NULL, "
                        "lightbox TEXT DEFAULT NULL, "
                        "scope TEXT DEFAULT NULL, "
                        "reducer REAL DEFAULT 1, "
                        "rotator TEXT DEFAULT NULL, "
                        "focuser TEXT DEFAULT NULL, "
                        "filterwheel TEXT DEFAULT NULL, "
                        "camera TEXT DEFAULT NULL, "
                        "guider TEXT DEFAULT NULL)"))
            qCWarning(KSTARS) << query.lastError();

        if (!query.exec("CREATE TABLE profilesettings ( "
                        "id INTEGER DEFAULT NULL PRIMARY KEY AUTOINCREMENT, "
                        "profile INTEGER DEFAULT NULL, "
                        "settings TEXT DEFAULT NULL)"))
            qCWarning(KSTARS) << query.lastError();

        if (!query.exec("CREATE TABLE opticaltrainsettings ( "
                        "id INTEGER DEFAULT NULL PRIMARY KEY AUTOINCREMENT, "
                        "opticaltrain INTEGER DEFAULT NULL, "
                        "settings TEXT DEFAULT NULL)"))
            qCWarning(KSTARS) << query.lastError();

        // Add DSLR lenses table
        if (!query.exec("CREATE TABLE dslrlens ( "
                        "id INTEGER DEFAULT NULL PRIMARY KEY AUTOINCREMENT, "
                        "Vendor TEXT DEFAULT NULL, "
                        "Model TEXT DEFAULT NULL, "
                        "FocalLength REAL DEFAULT NULL, "
                        "FocalRatio REAL DEFAULT NULL)"))
            qCWarning(KSTARS) << query.lastError();

        // Need to offset primary key by 100,000 to differential it from scopes and keep it backward compatible.
        if (!query.exec("UPDATE SQLITE_SEQUENCE SET seq = 100000 WHERE name ='dslrlens'"))
            qCWarning(KSTARS) << query.lastError();
    }

    // Adjust effective FOV
    if (currentDBVersion < 312)
    {
        QSqlQuery query(db);

        if (!query.exec("ALTER TABLE effectivefov ADD COLUMN Train TEXT DEFAULT NULL"))
            qCWarning(KSTARS) << query.lastError();
        if (!query.exec("ALTER TABLE effectivefov ADD COLUMN FocalReducer REAL DEFAULT 0.0"))
            qCWarning(KSTARS) << query.lastError();
        if (!query.exec("ALTER TABLE effectivefov ADD COLUMN FocalRatio REAL DEFAULT 0.0"))
            qCWarning(KSTARS) << query.lastError();
    }

    // Add focusTemperature, focusAltitude, focusTicksPerTemp, focusTicksPerAlt and wavelength to filter table
    if (currentDBVersion < 313)
    {
        QSqlQuery query(db);

        if (!query.exec("ALTER TABLE filter ADD COLUMN FocusTemperature REAL DEFAULT NULL"))
            qCWarning(KSTARS) << query.lastError();
        if (!query.exec("ALTER TABLE filter ADD COLUMN FocusAltitude REAL DEFAULT NULL"))
            qCWarning(KSTARS) << query.lastError();
        if (!query.exec("ALTER TABLE filter ADD COLUMN FocusTicksPerTemp REAL DEFAULT 0.0"))
            qCWarning(KSTARS) << query.lastError();
        if (!query.exec("ALTER TABLE filter ADD COLUMN FocusTicksPerAlt REAL DEFAULT 0.0"))
            qCWarning(KSTARS) << query.lastError();
        if (!query.exec("ALTER TABLE filter ADD COLUMN Wavelength REAL DEFAULT 500.0"))
            qCWarning(KSTARS) << query.lastError();
    }

    // Add collimationoverlayelements table
    if (currentDBVersion < 314)
    {
        QSqlQuery query(db);

        if (!query.exec("CREATE TABLE collimationoverlayelements ( "
                        "id INTEGER DEFAULT NULL PRIMARY KEY AUTOINCREMENT, "
                        "Name TEXT DEFAULT NULL, "
                        "Enabled INTEGER DEFAULT 0, "
                        "Type INTEGER DEFAULT NULL, "
                        "SizeX INTEGER DEFAULT NULL, "
                        "SizeY INTEGER DEFAULT NULL, "
                        "OffsetX INTEGER DEFAULT NULL, "
                        "OffsetY INTEGER DEFAULT NULL, "
                        "Count INTEGER DEFAULT 1, "
                        "PCD INTEGER DEFAULT 100, "
                        "Rotation REAL DEFAULT 0.0, "
                        "Colour TEXT DEFAULT NULL, "
                        "Thickness INTEGER DEFAULT 1)"))
            qCWarning(KSTARS) << query.lastError();
    }

    // Add focusDatetime to filter table. Add the column in the middle of the table
    // but sqlite only allows columns to be added at the end of the table so create
    // a new table with the correct column order, copy the data, delete the original
    // table and rename the new table.
    if (currentDBVersion < 315)
    {
        QSqlQuery query(db);

        bool ok = query.exec("CREATE TABLE tempfilter ( "
                        "id INTEGER DEFAULT NULL PRIMARY KEY AUTOINCREMENT , "
                        "Vendor TEXT DEFAULT NULL, "
                        "Model TEXT DEFAULT NULL, "
                        "Type TEXT DEFAULT NULL, "
                        "Color TEXT DEFAULT NULL,"
                        "Exposure REAL DEFAULT 1.0,"
                        "Offset INTEGER DEFAULT 0,"
                        "UseAutoFocus INTEGER DEFAULT 0,"
                        "LockedFilter TEXT DEFAULT '--',"
                        "AbsoluteFocusPosition INTEGER DEFAULT 0,"
                        "FocusTemperature REAL DEFAULT NULL,"
                        "FocusAltitude REAL DEFAULT NULL,"
                        "FocusDatetime TEXT DEFAULT NULL,"
                        "FocusTicksPerTemp REAL DEFAULT 0.0,"
                        "FocusTicksPerAlt REAL DEFAULT 0.0,"
                        "Wavelength INTEGER DEFAULT 500)");

        if (ok)
            ok = query.exec("INSERT INTO tempfilter (id, Vendor, Model, Type, Color, Exposure, Offset, "
                                  "UseAutoFocus, LockedFilter, AbsoluteFocusPosition, FocusTemperature, "
                                  "FocusAltitude, FocusTicksPerTemp, FocusTicksPerAlt, Wavelength) "
                             "SELECT id, Vendor, Model, Type, Color, Exposure, Offset, "
                                  "UseAutoFocus, LockedFilter, AbsoluteFocusPosition, FocusTemperature, "
                                  "FocusAltitude, FocusTicksPerTemp, FocusTicksPerAlt, Wavelength FROM filter");

        if (ok)
            ok = query.exec("DROP TABLE filter");

        if (ok)
            ok = query.exec("ALTER TABLE tempfilter RENAME to filter");

        if (!ok)
            qCWarning(KSTARS) << query.lastError();
    }
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool KSUserDB::FirstRun()
{
    if (!RebuildDB())
        return false;
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool KSUserDB::RebuildDB()
{
    auto db = QSqlDatabase::database(m_ConnectionName);
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
                  "Aperture REAL NOT NULL DEFAULT NULL, "
                  "Model TEXT DEFAULT NULL, "
                  "Type TEXT DEFAULT NULL, "
                  "FocalLength REAL DEFAULT NULL)");

    tables.append("INSERT INTO telescope (Vendor, Aperture, Model, Type, FocalLength) VALUES "
                  "('Sample', 120, 'Primary', 'Refractor', 700)");

    tables.append("INSERT INTO telescope (Vendor, Aperture, Model, Type, FocalLength) VALUES "
                  "('Sample', 50, 'Guide', 'Refractor', 300)");

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

    tables.append("CREATE TABLE dslrlens ( "
                  "id INTEGER DEFAULT NULL PRIMARY KEY AUTOINCREMENT, "
                  "Vendor TEXT DEFAULT NULL, "
                  "Model TEXT DEFAULT NULL, "
                  "FocalLength REAL DEFAULT NULL, "
                  "FocalRatio REAL DEFAULT NULL)");

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
                  "AbsoluteFocusPosition INTEGER DEFAULT 0,"
                  "FocusTemperature REAL DEFAULT NULL,"
                  "FocusAltitude REAL DEFAULT NULL,"
                  "FocusDatetime TEXT DEFAULT NULL,"
                  "FocusTicksPerTemp REAL DEFAULT 0.0,"
                  "FocusTicksPerAlt REAL DEFAULT 0.0,"
                  "Wavelength INTEGER DEFAULT 500)");

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
                  "indihub INTEGER DEFAULT 0, portselector INTEGER DEFAULT 1, remotedrivers TEXT DEFAULT NULL, "
                  "scripts TEXT DEFAULT NULL)");

#ifdef Q_OS_WIN
    tables.append("INSERT INTO profile (name, host, port) VALUES ('Simulators', 'localhost', 7624)");
#else
    tables.append("INSERT INTO profile (name, portselector) VALUES ('Simulators', 0)");
#endif

    tables.append("CREATE TABLE driver (id INTEGER DEFAULT NULL PRIMARY KEY AUTOINCREMENT, label TEXT NOT NULL, role "
                  "TEXT NOT NULL, profile INTEGER NOT NULL, FOREIGN KEY(profile) REFERENCES profile(id))");
    //tables.append("CREATE TABLE custom_driver (id INTEGER DEFAULT NULL PRIMARY KEY AUTOINCREMENT, drivers TEXT NOT NULL, profile INTEGER NOT NULL, FOREIGN KEY(profile) REFERENCES profile(id))");

    tables.append("INSERT INTO driver (label, role, profile) VALUES ('Telescope Simulator', 'Mount', 1)");
    tables.append("INSERT INTO driver (label, role, profile) VALUES ('CCD Simulator', 'CCD', 1)");
    tables.append("INSERT INTO driver (label, role, profile) VALUES ('Focuser Simulator', 'Focuser', 1)");

    tables.append("CREATE TABLE profilesettings (id INTEGER DEFAULT NULL PRIMARY KEY AUTOINCREMENT, "
                  "profile INTEGER DEFAULT NULL, settings TEXT DEFAULT NULL)");

    tables.append("CREATE TABLE opticaltrains (id INTEGER DEFAULT NULL PRIMARY KEY AUTOINCREMENT, "
                  "profile INTEGER DEFAULT NULL, name TEXT DEFAULT NULL, mount TEXT DEFAULT NULL, "
                  "dustcap TEXT DEFAULT NULL, lightbox TEXT DEFAULT NULL, scope TEXT DEFAULT NULL, reducer REAL DEFAULT 1, "
                  "rotator TEXT DEFAULT NULL, focuser TEXT DEFAULT NULL, filterwheel TEXT DEFAULT NULL, camera TEXT DEFAULT NULL, "
                  "guider TEXT DEFAULT NULL)");

    tables.append("CREATE TABLE opticaltrainsettings (id INTEGER DEFAULT NULL PRIMARY KEY AUTOINCREMENT, "
                  "opticaltrain INTEGER DEFAULT NULL, settings TEXT DEFAULT NULL)");

    tables.append("CREATE TABLE IF NOT EXISTS darkframe (id INTEGER DEFAULT NULL PRIMARY KEY AUTOINCREMENT, ccd TEXT "
                  "NOT NULL, chip INTEGER DEFAULT 0, binX INTEGER, binY INTEGER, temperature REAL, gain INTEGER DEFAULT -1, "
                  "iso TEXT DEFAULT NULL, duration REAL, filename TEXT NOT NULL, defectmap TEXT DEFAULT NULL, timestamp "
                  "DATETIME DEFAULT CURRENT_TIMESTAMP)");

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
                  "Train TEXT DEFAULT NULL, "
                  "Profile TEXT DEFAULT NULL, "
                  "Width INTEGER DEFAULT NULL, "
                  "Height INTEGER DEFAULT NULL, "
                  "PixelW REAL DEFAULT 5.0,"
                  "PixelH REAL DEFAULT 5.0,"
                  "FocalLength REAL DEFAULT 0.0,"
                  "FocalReducer REAL DEFAULT 0.0,"
                  "FocalRatio REAL DEFAULT 0.0,"
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

    tables.append("CREATE TABLE IF NOT EXISTS collimationoverlayelements ( "
                  "id INTEGER DEFAULT NULL PRIMARY KEY AUTOINCREMENT, "
                  "Name TEXT DEFAULT NULL, "
                  "Enabled INTEGER DEFAULT 0, "
                  "Type INTEGER DEFAULT NULL, "
                  "SizeX INTEGER DEFAULT NULL, "
                  "SizeY INTEGER DEFAULT NULL, "
                  "OffsetX INTEGER DEFAULT NULL, "
                  "OffsetY INTEGER DEFAULT NULL, "
                  "Count INTEGER DEFAULT 1, "
                  "PCD INTEGER DEFAULT 100, "
                  "Rotation REAL DEFAULT 0.0, "
                  "Colour TEXT DEFAULT NULL, "
                  "Thickness INTEGER DEFAULT 1)");

    // Need to offset primary key by 100,000 to differential it from scopes and keep it backward compatible.
    tables.append("UPDATE SQLITE_SEQUENCE SET seq = 100000 WHERE name ='dslrlens'");

    for (int i = 0; i < tables.count(); ++i)
    {
        QSqlQuery query(db);
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

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool KSUserDB::AddObserver(const QString &name, const QString &surname, const QString &contact)
{
    auto db = QSqlDatabase::database(m_ConnectionName);
    if (!db.isValid())
    {
        qCCritical(KSTARS) << "Failed to open database:" << db.lastError();
        return false;
    }

    QSqlTableModel users(nullptr, db);
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

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool KSUserDB::FindObserver(const QString &name, const QString &surname)
{
    auto db = QSqlDatabase::database(m_ConnectionName);
    if (!db.isValid())
    {
        qCCritical(KSTARS) << "Failed to open database:" << db.lastError();
        return false;
    }

    QSqlTableModel users(nullptr, db);
    users.setTable("user");
    users.setFilter("Name LIKE \'" + name + "\' AND Surname LIKE \'" + surname + "\'");
    users.select();

    int observer_count = users.rowCount();

    users.clear();
    return (observer_count > 0);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool KSUserDB::DeleteObserver(const QString &id)
{
    auto db = QSqlDatabase::database(m_ConnectionName);
    if (!db.isValid())
    {
        qCCritical(KSTARS) << "Failed to open database:" << db.lastError();
        return false;
    }

    QSqlTableModel users(nullptr, db);
    users.setTable("user");
    users.setFilter("id = \'" + id + "\'");
    users.select();

    users.removeRows(0, 1);
    users.submitAll();

    int observer_count = users.rowCount();

    users.clear();
    return (observer_count > 0);
}

#ifndef KSTARS_LITE
////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool KSUserDB::GetAllObservers(QList<Observer *> &observer_list)
{
    auto db = QSqlDatabase::database(m_ConnectionName);
    if (!db.isValid())
    {
        qCCritical(KSTARS) << "Failed to open database:" << db.lastError();
        return false;
    }

    observer_list.clear();
    QSqlTableModel users(nullptr, db);
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
    return true;
}
#endif

/* Dark Library Section */

/**
 * @brief KSUserDB::AddDarkFrame Saves a new dark frame data to the database
 * @param oneFrame Map that contains 1 to 1 correspondence with the database table, except for primary key and timestamp.
 */
bool KSUserDB::AddDarkFrame(const QVariantMap &oneFrame)
{
    auto db = QSqlDatabase::database(m_ConnectionName);
    if (!db.isValid())
    {
        qCCritical(KSTARS) << "Failed to open database:" << db.lastError();
        return false;
    }

    QSqlTableModel darkframe(nullptr, db);
    darkframe.setTable("darkframe");
    darkframe.select();

    QSqlRecord record = darkframe.record();
    // Remove PK so that it gets auto-incremented later
    record.remove(0);

    for (QVariantMap::const_iterator iter = oneFrame.begin(); iter != oneFrame.end(); ++iter)
        record.setValue(iter.key(), iter.value());

    darkframe.insertRecord(-1, record);
    darkframe.submitAll();
    return true;
}

/**
 * @brief KSUserDB::UpdateDarkFrame Updates an existing dark frame record in the data, replace all values matching the supplied ID
 * @param oneFrame dark frame to update. The ID should already exist in the database.
 */
bool KSUserDB::UpdateDarkFrame(const QVariantMap &oneFrame)
{
    auto db = QSqlDatabase::database(m_ConnectionName);
    if (!db.isValid())
    {
        qCCritical(KSTARS) << "Failed to open database:" << db.lastError();
        return false;
    }

    QSqlTableModel darkframe(nullptr, db);
    darkframe.setTable("darkframe");
    darkframe.setFilter(QString("id=%1").arg(oneFrame["id"].toInt()));
    darkframe.select();

    QSqlRecord record = darkframe.record(0);
    for (QVariantMap::const_iterator iter = oneFrame.begin(); iter != oneFrame.end(); ++iter)
        record.setValue(iter.key(), iter.value());

    darkframe.setRecord(0, record);
    darkframe.submitAll();

    return true;
}

/**
 * @brief KSUserDB::DeleteDarkFrame Delete from database a dark frame record that matches the filename field.
 * @param filename filename of dark frame to delete from database.
 */
bool KSUserDB::DeleteDarkFrame(const QString &filename)
{
    auto db = QSqlDatabase::database(m_ConnectionName);
    if (!db.isValid())
    {
        qCCritical(KSTARS) << "Failed to open database:" << db.lastError();
        return false;
    }

    QSqlTableModel darkframe(nullptr, db);
    darkframe.setTable("darkframe");
    darkframe.setFilter("filename = \'" + filename + "\'");

    darkframe.select();

    darkframe.removeRows(0, 1);
    darkframe.submitAll();

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool KSUserDB::GetAllDarkFrames(QList<QVariantMap> &darkFrames)
{
    auto db = QSqlDatabase::database(m_ConnectionName);
    if (!db.isValid())
    {
        qCCritical(KSTARS) << "Failed to open database:" << db.lastError();
        return false;
    }

    darkFrames.clear();

    QSqlTableModel darkframe(nullptr, db);
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

    return true;
}

/* Effective FOV Section */

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool KSUserDB::AddEffectiveFOV(const QVariantMap &oneFOV)
{
    auto db = QSqlDatabase::database(m_ConnectionName);
    if (!db.isValid())
    {
        qCCritical(KSTARS) << "Failed to open database:" << db.lastError();
        return false;
    }

    QSqlTableModel effectivefov(nullptr, db);
    effectivefov.setTable("effectivefov");
    effectivefov.select();

    QSqlRecord record = effectivefov.record();

    // Remove PK so that it gets auto-incremented later
    record.remove(0);

    for (QVariantMap::const_iterator iter = oneFOV.begin(); iter != oneFOV.end(); ++iter)
        record.setValue(iter.key(), iter.value());

    effectivefov.insertRecord(-1, record);

    effectivefov.submitAll();

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool KSUserDB::DeleteEffectiveFOV(const QString &id)
{
    auto db = QSqlDatabase::database(m_ConnectionName);
    if (!db.isValid())
    {
        qCCritical(KSTARS) << "Failed to open database:" << db.lastError();
        return false;
    }

    QSqlTableModel effectivefov(nullptr, db);
    effectivefov.setTable("effectivefov");
    effectivefov.setFilter("id = \'" + id + "\'");

    effectivefov.select();

    effectivefov.removeRows(0, 1);
    effectivefov.submitAll();
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool KSUserDB::GetAllEffectiveFOVs(QList<QVariantMap> &effectiveFOVs)
{
    auto db = QSqlDatabase::database(m_ConnectionName);
    if (!db.isValid())
    {
        qCCritical(KSTARS) << "Failed to open database:" << db.lastError();
        return false;
    }

    effectiveFOVs.clear();

    QSqlTableModel effectivefov(nullptr, db);
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

    return true;
}

/* Optical Trains Section */

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool KSUserDB::AddOpticalTrain(const QVariantMap &oneTrain)
{
    auto db = QSqlDatabase::database(m_ConnectionName);
    if (!db.isValid())
    {
        qCCritical(KSTARS) << "Failed to open database:" << db.lastError();
        return false;
    }

    QSqlTableModel opticalTrain(nullptr, db);
    opticalTrain.setTable("opticaltrains");
    opticalTrain.select();

    QSqlRecord record = opticalTrain.record();

    // Remove PK so that it gets auto-incremented later
    record.remove(0);

    for (QVariantMap::const_iterator iter = oneTrain.begin(); iter != oneTrain.end(); ++iter)
        record.setValue(iter.key(), iter.value());

    opticalTrain.insertRecord(-1, record);

    if (!opticalTrain.submitAll())
    {
        qCWarning(KSTARS) << opticalTrain.lastError();
        return false;
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool KSUserDB::UpdateOpticalTrain(const QVariantMap &oneTrain, int id)
{
    auto db = QSqlDatabase::database(m_ConnectionName);
    if (!db.isValid())
    {
        qCCritical(KSTARS) << "Failed to open database:" << db.lastError();
        return false;
    }

    QSqlTableModel opticalTrain(nullptr, db);
    opticalTrain.setTable("opticaltrains");
    opticalTrain.setFilter(QString("id=%1").arg(id));
    opticalTrain.select();

    QSqlRecord record = opticalTrain.record(0);

    for (QVariantMap::const_iterator iter = oneTrain.begin(); iter != oneTrain.end(); ++iter)
        record.setValue(iter.key(), iter.value());

    opticalTrain.setRecord(0, record);

    if (!opticalTrain.submitAll())
    {
        qCWarning(KSTARS) << opticalTrain.lastError();
        return false;
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool KSUserDB::DeleteOpticalTrain(int id)
{
    auto db = QSqlDatabase::database(m_ConnectionName);
    if (!db.isValid())
    {
        qCCritical(KSTARS) << "Failed to open database:" << db.lastError();
        return false;
    }

    QSqlTableModel opticalTrain(nullptr, db);
    opticalTrain.setTable("opticaltrains");
    opticalTrain.setFilter(QString("id=%1").arg(id));

    opticalTrain.select();

    opticalTrain.removeRows(0, 1);
    opticalTrain.submitAll();
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool KSUserDB::GetOpticalTrains(uint32_t profileID, QList<QVariantMap> &opticalTrains)
{
    auto db = QSqlDatabase::database(m_ConnectionName);
    if (!db.isValid())
    {
        qCCritical(KSTARS) << "Failed to open database:" << db.lastError();
        return false;
    }

    opticalTrains.clear();

    QSqlTableModel opticalTrain(nullptr, db);
    opticalTrain.setTable("opticaltrains");
    opticalTrain.setFilter(QString("profile=%1").arg(profileID));
    opticalTrain.select();

    for (int i = 0; i < opticalTrain.rowCount(); ++i)
    {
        QVariantMap recordMap;
        QSqlRecord record = opticalTrain.record(i);
        for (int j = 0; j < record.count(); j++)
            recordMap[record.fieldName(j)] = record.value(j);

        opticalTrains.append(recordMap);
    }

    return true;
}

/* Driver Alias Section */

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool KSUserDB::AddCustomDriver(const QVariantMap &oneDriver)
{
    auto db = QSqlDatabase::database(m_ConnectionName);
    if (!db.isValid())
    {
        qCCritical(KSTARS) << "Failed to open database:" << db.lastError();
        return false;
    }

    QSqlTableModel CustomDriver(nullptr, db);
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

    return rc;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool KSUserDB::DeleteCustomDriver(const QString &id)
{
    auto db = QSqlDatabase::database(m_ConnectionName);
    if (!db.isValid())
    {
        qCCritical(KSTARS) << "Failed to open database:" << db.lastError();
        return false;
    }

    QSqlTableModel CustomDriver(nullptr, db);
    CustomDriver.setTable("customdrivers");
    CustomDriver.setFilter("id = \'" + id + "\'");

    CustomDriver.select();

    CustomDriver.removeRows(0, 1);
    CustomDriver.submitAll();

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool KSUserDB::GetAllCustomDrivers(QList<QVariantMap> &CustomDrivers)
{
    auto db = QSqlDatabase::database(m_ConnectionName);
    if (!db.isValid())
    {
        qCCritical(KSTARS) << "Failed to open database:" << db.lastError();
        return false;
    }

    CustomDrivers.clear();
    QSqlTableModel CustomDriver(nullptr, db);
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

    return true;
}

/* HiPS Section */

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool KSUserDB::AddHIPSSource(const QMap<QString, QString> &oneSource)
{
    auto db = QSqlDatabase::database(m_ConnectionName);
    if (!db.isValid())
    {
        qCCritical(KSTARS) << "Failed to open database:" << db.lastError();
        return false;
    }

    QSqlTableModel HIPSSource(nullptr, db);
    HIPSSource.setTable("hips");
    HIPSSource.select();

    QSqlRecord record = HIPSSource.record();

    for (QMap<QString, QString>::const_iterator iter = oneSource.begin(); iter != oneSource.end(); ++iter)
        record.setValue(iter.key(), iter.value());

    HIPSSource.insertRecord(-1, record);
    HIPSSource.submitAll();

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool KSUserDB::DeleteHIPSSource(const QString &ID)
{
    auto db = QSqlDatabase::database(m_ConnectionName);
    if (!db.isValid())
    {
        qCCritical(KSTARS) << "Failed to open database:" << db.lastError();
        return false;
    }

    QSqlTableModel HIPSSource(nullptr, db);
    HIPSSource.setTable("hips");
    HIPSSource.setFilter("ID = \'" + ID + "\'");

    HIPSSource.select();

    HIPSSource.removeRows(0, 1);
    HIPSSource.submitAll();

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool KSUserDB::GetAllHIPSSources(QList<QMap<QString, QString>> &HIPSSources)
{
    auto db = QSqlDatabase::database(m_ConnectionName);
    if (!db.isValid())
    {
        qCCritical(KSTARS) << "Failed to open database:" << db.lastError();
        return false;
    }

    HIPSSources.clear();
    QSqlTableModel HIPSSource(nullptr, db);
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

    return true;
}

/* DSLR Section */

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool KSUserDB::AddDSLRInfo(const QMap<QString, QVariant> &oneInfo)
{
    auto db = QSqlDatabase::database(m_ConnectionName);
    if (!db.isValid())
    {
        qCCritical(KSTARS) << "Failed to open database:" << db.lastError();
        return false;
    }

    QSqlTableModel DSLRInfo(nullptr, db);
    DSLRInfo.setTable("dslr");
    DSLRInfo.select();

    QSqlRecord record = DSLRInfo.record();

    for (QMap<QString, QVariant>::const_iterator iter = oneInfo.begin(); iter != oneInfo.end(); ++iter)
        record.setValue(iter.key(), iter.value());

    DSLRInfo.insertRecord(-1, record);
    DSLRInfo.submitAll();

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool KSUserDB::DeleteAllDSLRInfo()
{
    auto db = QSqlDatabase::database(m_ConnectionName);
    if (!db.isValid())
    {
        qCCritical(KSTARS) << "Failed to open database:" << db.lastError();
        return false;
    }

    QSqlTableModel DSLRInfo(nullptr, db);
    DSLRInfo.setTable("dslr");
    DSLRInfo.select();

    DSLRInfo.removeRows(0, DSLRInfo.rowCount());
    DSLRInfo.submitAll();

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool KSUserDB::DeleteDSLRInfo(const QString &model)
{
    auto db = QSqlDatabase::database(m_ConnectionName);
    if (!db.isValid())
    {
        qCCritical(KSTARS) << "Failed to open database:" << db.lastError();
        return false;
    }

    QSqlTableModel DSLRInfo(nullptr, db);
    DSLRInfo.setTable("dslr");
    DSLRInfo.setFilter("model = \'" + model + "\'");

    DSLRInfo.select();

    DSLRInfo.removeRows(0, 1);
    DSLRInfo.submitAll();

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool KSUserDB::GetAllDSLRInfos(QList<QMap<QString, QVariant>> &DSLRInfos)
{
    auto db = QSqlDatabase::database(m_ConnectionName);
    if (!db.isValid())
    {
        qCCritical(KSTARS) << "Failed to open database:" << db.lastError();
        return false;
    }

    DSLRInfos.clear();

    QSqlTableModel DSLRInfo(nullptr, db);
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

    return true;
}

/*
 * Flag Section
*/

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool KSUserDB::DeleteAllFlags()
{
    auto db = QSqlDatabase::database(m_ConnectionName);
    if (!db.isValid())
    {
        qCCritical(KSTARS) << "Failed to open database:" << db.lastError();
        return false;
    }

    QSqlTableModel flags(nullptr, db);
    flags.setEditStrategy(QSqlTableModel::OnManualSubmit);
    flags.setTable("flags");
    flags.select();

    flags.removeRows(0, flags.rowCount());
    flags.submitAll();

    flags.clear();

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool KSUserDB::AddFlag(const QString &ra, const QString &dec, const QString &epoch, const QString &image_name,
                       const QString &label, const QString &labelColor)
{
    auto db = QSqlDatabase::database(m_ConnectionName);
    if (!db.isValid())
    {
        qCCritical(KSTARS) << "Failed to open database:" << db.lastError();
        return false;
    }

    QSqlTableModel flags(nullptr, db);
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
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool KSUserDB::GetAllFlags(QList<QStringList> &flagList)
{
    auto db = QSqlDatabase::database(m_ConnectionName);
    if (!db.isValid())
    {
        qCCritical(KSTARS) << "Failed to open database:" << db.lastError();
        return false;
    }

    QSqlTableModel flags(nullptr, db);
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
    return true;
}

/*
 * Generic Section
 */

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool KSUserDB::DeleteEquipment(const QString &type, const QString &id)
{
    auto db = QSqlDatabase::database(m_ConnectionName);
    if (!db.isValid())
    {
        qCCritical(KSTARS) << "Failed to open database:" << db.lastError();
        return false;
    }

    QSqlTableModel equip(nullptr, db);
    equip.setTable(type);
    equip.setFilter("id = " + id);
    equip.select();

    equip.removeRows(0, equip.rowCount());
    equip.submitAll();
    equip.clear();

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool KSUserDB::DeleteAllEquipment(const QString &type)
{
    auto db = QSqlDatabase::database(m_ConnectionName);
    if (!db.isValid())
    {
        qCCritical(KSTARS) << "Failed to open database:" << db.lastError();
        return false;
    }

    QSqlTableModel equip(nullptr, db);
    equip.setEditStrategy(QSqlTableModel::OnManualSubmit);
    equip.setTable(type);
    equip.setFilter("id >= 1");
    equip.select();
    equip.removeRows(0, equip.rowCount());
    equip.submitAll();
    equip.clear();

    return true;
}

/*
 * Telescope section
 */

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool KSUserDB::AddScope(const QString &model, const QString &vendor, const QString &type, const double &aperture,
                        const double &focalLength)
{
    auto db = QSqlDatabase::database(m_ConnectionName);
    if (!db.isValid())
    {
        qCCritical(KSTARS) << "Failed to open database:" << db.lastError();
        return false;
    }

    QSqlTableModel equip(nullptr, db);
    equip.setTable("telescope");

    QSqlRecord record = equip.record();
    record.setValue("Vendor", vendor);
    record.setValue("Aperture", aperture);
    record.setValue("Model", model);
    record.setValue("Type", type);
    record.setValue("FocalLength", focalLength);

    equip.insertRecord(-1, record);

    if (!equip.submitAll())
        qCWarning(KSTARS) << equip.lastError().text();

    equip.clear();

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool KSUserDB::AddScope(const QString &model, const QString &vendor, const QString &type,
                        const double &aperture, const double &focalLength, const QString &id)
{
    auto db = QSqlDatabase::database(m_ConnectionName);
    if (!db.isValid())
    {
        qCCritical(KSTARS) << "Failed to open database:" << db.lastError();
        return false;
    }

    QSqlTableModel equip(nullptr, db);
    equip.setTable("telescope");
    equip.setFilter("id = " + id);
    equip.select();

    if (equip.rowCount() > 0)
    {
        QSqlRecord record = equip.record(0);
        record.setValue("Vendor", vendor);
        record.setValue("Aperture", aperture);
        record.setValue("Model", model);
        record.setValue("Type", type);
        record.setValue("FocalLength", focalLength);
        equip.setRecord(0, record);
        equip.submitAll();
    }

    return true;
}

#ifndef KSTARS_LITE

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool KSUserDB::GetAllScopes(QList<Scope *> &scope_list)
{
    auto db = QSqlDatabase::database(m_ConnectionName);
    if (!db.isValid())
    {
        qCCritical(KSTARS) << "Failed to open database:" << db.lastError();
        return false;
    }

    qDeleteAll(scope_list);
    scope_list.clear();

    QSqlTableModel equip(nullptr, db);
    equip.setTable("telescope");
    equip.select();

    for (int i = 0; i < equip.rowCount(); ++i)
    {
        QSqlRecord record  = equip.record(i);
        QString id         = record.value("id").toString();
        QString vendor     = record.value("Vendor").toString();
        double aperture    = record.value("Aperture").toDouble();
        QString model      = record.value("Model").toString();
        QString type       = record.value("Type").toString();
        double focalLength = record.value("FocalLength").toDouble();
        OAL::Scope *o      = new OAL::Scope(id, model, vendor, type, focalLength, aperture);
        scope_list.append(o);
    }

    equip.clear();
    return true;
}
#endif
/*
 * Eyepiece section
 */

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool KSUserDB::AddEyepiece(const QString &vendor, const QString &model, const double &focalLength, const double &fov,
                           const QString &fovunit)
{
    auto db = QSqlDatabase::database(m_ConnectionName);
    if (!db.isValid())
    {
        qCCritical(KSTARS) << "Failed to open database:" << db.lastError();
        return false;
    }

    QSqlTableModel equip(nullptr, db);
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

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool KSUserDB::AddEyepiece(const QString &vendor, const QString &model, const double &focalLength, const double &fov,
                           const QString &fovunit, const QString &id)
{
    auto db = QSqlDatabase::database(m_ConnectionName);
    if (!db.isValid())
    {
        qCCritical(KSTARS) << "Failed to open database:" << db.lastError();
        return false;
    }

    QSqlTableModel equip(nullptr, db);
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

    return true;
}

#ifndef KSTARS_LITE

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool KSUserDB::GetAllEyepieces(QList<OAL::Eyepiece *> &eyepiece_list)
{
    auto db = QSqlDatabase::database(m_ConnectionName);
    if (!db.isValid())
    {
        qCCritical(KSTARS) << "Failed to open database:" << db.lastError();
        return false;
    }

    eyepiece_list.clear();

    QSqlTableModel equip(nullptr, db);
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
    return true;
}
#endif
/*
 * lens section
 */

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool KSUserDB::AddLens(const QString &vendor, const QString &model, const double &factor)
{
    auto db = QSqlDatabase::database(m_ConnectionName);
    if (!db.isValid())
    {
        qCCritical(KSTARS) << "Failed to open database:" << db.lastError();
        return false;
    }

    QSqlTableModel equip(nullptr, db);
    equip.setTable("lens");

    int row = 0;
    equip.insertRows(row, 1);
    equip.setData(equip.index(row, 1), vendor); // row,0 is autoincerement ID
    equip.setData(equip.index(row, 2), model);
    equip.setData(equip.index(row, 3), factor);
    equip.submitAll();

    equip.clear();
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool KSUserDB::AddLens(const QString &vendor, const QString &model, const double &factor, const QString &id)
{
    auto db = QSqlDatabase::database(m_ConnectionName);
    if (!db.isValid())
    {
        qCCritical(KSTARS) << "Failed to open database:" << db.lastError();
        return false;
    }

    QSqlTableModel equip(nullptr, db);
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

    return true;
}
#ifndef KSTARS_LITE

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool KSUserDB::GetAllLenses(QList<OAL::Lens *> &lens_list)
{
    auto db = QSqlDatabase::database(m_ConnectionName);
    if (!db.isValid())
    {
        qCCritical(KSTARS) << "Failed to open database:" << db.lastError();
        return false;
    }

    qDeleteAll(lens_list);
    lens_list.clear();

    QSqlTableModel equip(nullptr, db);
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
    return true;
}
#endif
/*
 *  filter section
 */

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool KSUserDB::AddFilter(const filterProperties *fp)
{
    auto db = QSqlDatabase::database(m_ConnectionName);
    if (!db.isValid())
    {
        qCCritical(KSTARS) << "Failed to open database:" << db.lastError();
        return false;
    }

    QSqlTableModel equip(nullptr, db);
    equip.setTable("filter");

    QSqlRecord record = equip.record();
    record.setValue("Vendor", fp->vendor);
    record.setValue("Model", fp->model);
    record.setValue("Type", fp->type);
    record.setValue("Color", fp->color);
    record.setValue("Offset", fp->offset);
    record.setValue("Exposure", fp->exposure);
    record.setValue("UseAutoFocus", fp->useAutoFocus ? 1 : 0);
    record.setValue("LockedFilter", fp->lockedFilter);
    record.setValue("AbsoluteFocusPosition", fp->absFocusPos);
    record.setValue("FocusTemperature", fp->focusTemperature);
    record.setValue("FocusAltitude", fp->focusAltitude);
    record.setValue("FocusDatetime", fp->focusDatetime);
    record.setValue("FocusTicksPerTemp", fp->focusTicksPerTemp);
    record.setValue("FocusTicksPerAlt", fp->focusTicksPerAlt);
    record.setValue("Wavelength", fp->wavelength);

    if (equip.insertRecord(-1, record) == false)
        qCritical() << __FUNCTION__ << equip.lastError();

    if (equip.submitAll() == false)
        qCritical() << "AddFilter:" << equip.lastError();

    equip.clear();
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool KSUserDB::AddFilter(const filterProperties *fp, const QString &id)
{
    auto db = QSqlDatabase::database(m_ConnectionName);
    if (!db.isValid())
    {
        qCCritical(KSTARS) << "Failed to open database:" << db.lastError();
        return false;
    }

    QSqlTableModel equip(nullptr, db);
    equip.setTable("filter");
    equip.setFilter("id = " + id);
    equip.select();

    if (equip.rowCount() > 0)
    {
        QSqlRecord record = equip.record(0);
        record.setValue("Vendor", fp->vendor);
        record.setValue("Model", fp->model);
        record.setValue("Type", fp->type);
        record.setValue("Color", fp->color);
        record.setValue("Offset", fp->offset);
        record.setValue("Exposure", fp->exposure);
        record.setValue("UseAutoFocus", fp->useAutoFocus ? 1 : 0);
        record.setValue("LockedFilter", fp->lockedFilter);
        record.setValue("AbsoluteFocusPosition", fp->absFocusPos);
        record.setValue("FocusTemperature", fp->focusTemperature);
        record.setValue("FocusAltitude", fp->focusAltitude);
        record.setValue("FocusDatetime", fp->focusDatetime);
        record.setValue("FocusTicksPerTemp", fp->focusTicksPerTemp);
        record.setValue("FocusTicksPerAlt", fp->focusTicksPerAlt);
        record.setValue("Wavelength", fp->wavelength);
        equip.setRecord(0, record);
        if (equip.submitAll() == false)
            qCritical() << "AddFilter:" << equip.lastError();
    }

    return true;
}

#ifndef KSTARS_LITE

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool KSUserDB::GetAllFilters(QList<OAL::Filter *> &filter_list)
{
    auto db = QSqlDatabase::database(m_ConnectionName);
    if (!db.isValid())
    {
        qCCritical(KSTARS) << "Failed to open database:" << db.lastError();
        return false;
    }

    filter_list.clear();
    QSqlTableModel equip(nullptr, db);
    equip.setTable("filter");
    equip.select();

    filterProperties *fp = new filterProperties("", "", "", "");

    for (int i = 0; i < equip.rowCount(); ++i)
    {
        QSqlRecord record     = equip.record(i);
        QString id            = record.value("id").toString();
        fp->vendor            = record.value("Vendor").toString();
        fp->model             = record.value("Model").toString();
        fp->type              = record.value("Type").toString();
        fp->color             = record.value("Color").toString();
        fp->offset            = record.value("Offset").toInt();
        fp->exposure          = record.value("Exposure").toDouble();
        fp->lockedFilter      = record.value("LockedFilter").toString();
        fp->useAutoFocus      = record.value("UseAutoFocus").toInt() == 1;
        fp->absFocusPos       = record.value("AbsoluteFocusPosition").toInt();
        fp->focusTemperature  = record.value("FocusTemperature").toDouble();
        fp->focusAltitude     = record.value("FocusAltitude").toDouble();
        fp->focusDatetime     = record.value("FocusDatetime").toString();
        fp->focusTicksPerTemp = record.value("FocusTicksPerTemp").toDouble();
        fp->focusTicksPerAlt  = record.value("FocusTicksPerAlt").toDouble();
        fp->wavelength        = record.value("Wavelength").toDouble();
        OAL::Filter *o        = new OAL::Filter(id, fp);
        filter_list.append(o);
    }

    equip.clear();
    return true;
}
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
void KSUserDB::readScopes()
{
    while (!reader_->atEnd())
    {
        reader_->readNext();

        if (reader_->isEndElement())
            break;

        if (reader_->isStartElement())
        {
            if (reader_->name().toString() == "scope")
                readScope();
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
void KSUserDB::readEyepieces()
{
    while (!reader_->atEnd())
    {
        reader_->readNext();

        if (reader_->isEndElement())
            break;

        if (reader_->isStartElement())
        {
            if (reader_->name().toString() == "eyepiece")
                readEyepiece();
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
void KSUserDB::readLenses()
{
    while (!reader_->atEnd())
    {
        reader_->readNext();

        if (reader_->isEndElement())
            break;

        if (reader_->isStartElement())
        {
            if (reader_->name().toString() == "lens")
                readLens();
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
void KSUserDB::readFilters()
{
    while (!reader_->atEnd())
    {
        reader_->readNext();

        if (reader_->isEndElement())
            break;

        if (reader_->isStartElement())
        {
            if (reader_->name().toString() == "filter")
                readFilter();
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
void KSUserDB::readScope()
{
    QString model, vendor, type;
    double aperture = 0, focalLength = 0;

    while (!reader_->atEnd())
    {
        reader_->readNext();

        if (reader_->isEndElement())
            break;

        if (reader_->isStartElement())
        {
            if (reader_->name().toString() == "model")
            {
                model = reader_->readElementText();
            }
            else if (reader_->name().toString() == "vendor")
            {
                vendor = reader_->readElementText();
            }
            else if (reader_->name().toString() == "type")
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
            else if (reader_->name().toString() == "focalLength")
            {
                focalLength = (reader_->readElementText()).toDouble();
            }
            else if (reader_->name().toString() == "aperture")
                aperture = (reader_->readElementText()).toDouble();
        }
    }

    AddScope(model, vendor, type, focalLength, aperture);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
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
            if (reader_->name().toString() == "model")
            {
                model = reader_->readElementText();
            }
            else if (reader_->name().toString() == "vendor")
            {
                vendor = reader_->readElementText();
            }
            else if (reader_->name().toString() == "apparentFOV")
            {
                fov     = reader_->readElementText();
                fovUnit = reader_->attributes().value("unit").toString();
            }
            else if (reader_->name().toString() == "focalLength")
            {
                focalLength = reader_->readElementText();
            }
        }
    }

    AddEyepiece(vendor, model, focalLength.toDouble(), fov.toDouble(), fovUnit);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
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
            if (reader_->name().toString() == "model")
            {
                model = reader_->readElementText();
            }
            else if (reader_->name().toString() == "vendor")
            {
                vendor = reader_->readElementText();
            }
            else if (reader_->name().toString() == "factor")
            {
                factor = reader_->readElementText();
            }
        }
    }

    AddLens(vendor, model, factor.toDouble());
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
void KSUserDB::readFilter()
{
    filterProperties *fp = new filterProperties("", "", "", "");

    while (!reader_->atEnd())
    {
        reader_->readNext();

        if (reader_->isEndElement())
            break;

        if (reader_->isStartElement())
        {
            if (reader_->name().toString() == "model")
            {
                fp->model = reader_->readElementText();
            }
            else if (reader_->name().toString() == "vendor")
            {
                fp->vendor = reader_->readElementText();
            }
            else if (reader_->name().toString() == "type")
            {
                fp->type = reader_->readElementText();
            }
            else if (reader_->name().toString() == "offset")
            {
                fp->offset = reader_->readElementText().toInt();
            }
            else if (reader_->name().toString() == "color")
            {
                fp->color = reader_->readElementText();
            }
            else if (reader_->name().toString() == "exposure")
            {
                fp->exposure = reader_->readElementText().toDouble();
            }
            else if (reader_->name().toString() == "lockedFilter")
            {
                fp->lockedFilter = reader_->readElementText();
            }
            else if (reader_->name().toString() == "useAutoFocus")
            {
                fp->useAutoFocus = (reader_->readElementText() == "1");
            }
            else if (reader_->name().toString() == "AbsoluteAutoFocus")
            {
                fp->absFocusPos = (reader_->readElementText().toInt());
            }
            else if (reader_->name().toString() == "FocusTemperature")
            {
                fp->focusTemperature = (reader_->readElementText().toDouble());
            }
            else if (reader_->name().toString() == "FocusAltitude")
            {
                fp->focusAltitude = (reader_->readElementText().toDouble());
            }
            else if (reader_->name().toString() == "FocusDatetime")
            {
                fp->focusDatetime = (reader_->readElementText());
            }
            else if (reader_->name().toString() == "FocusTicksPerTemp")
            {
                fp->focusTicksPerTemp = (reader_->readElementText().toDouble());
            }
            else if (reader_->name().toString() == "FocusTicksPerAlt")
            {
                fp->focusTicksPerAlt = (reader_->readElementText().toDouble());
            }
            else if (reader_->name().toString() == "Wavelength")
            {
                fp->wavelength = (reader_->readElementText().toDouble());
            }
        }
    }
    AddFilter(fp);
}

bool KSUserDB::GetAllHorizons(QList<ArtificialHorizonEntity *> &horizonList)
{
    auto db = QSqlDatabase::database(m_ConnectionName);
    if (!db.isValid())
    {
        qCCritical(KSTARS) << "Failed to open database:" << db.lastError();
        return false;
    }

    qDeleteAll(horizonList);
    horizonList.clear();
    QSqlTableModel regions(nullptr, db);
    regions.setTable("horizons");
    regions.select();

    QSqlTableModel points(nullptr, db);

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

    return true;
}

bool KSUserDB::DeleteAllHorizons()
{
    auto db = QSqlDatabase::database(m_ConnectionName);
    if (!db.isValid())
    {
        qCCritical(KSTARS) << "Failed to open database:" << db.lastError();
        return false;
    }

    QSqlTableModel regions(nullptr, db);
    regions.setEditStrategy(QSqlTableModel::OnManualSubmit);
    regions.setTable("horizons");
    regions.select();

    QSqlQuery query(db);

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
    return true;
}

bool KSUserDB::AddHorizon(ArtificialHorizonEntity *horizon)
{
    auto db = QSqlDatabase::database(m_ConnectionName);
    if (!db.isValid())
    {
        qCCritical(KSTARS) << "Failed to open database:" << db.lastError();
        return false;
    }

    QSqlTableModel regions(nullptr, db);
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
    QSqlQuery query(db);
    query.exec(tableQuery);

    QSqlTableModel points(nullptr, db);

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
    return true;
}

void KSUserDB::CreateImageOverlayTableIfNecessary()
{
    auto db = QSqlDatabase::database(m_ConnectionName);
    QString command = "CREATE TABLE IF NOT EXISTS imageOverlays ( "
                      "id INTEGER DEFAULT NULL PRIMARY KEY AUTOINCREMENT, "
                      "filename TEXT NOT NULL,"
                      "enabled INTEGER DEFAULT 0,"
                      "nickname TEXT DEFAULT NULL,"
                      "status INTEGER DEFAULT 0,"
                      "orientation REAL DEFAULT 0.0,"
                      "ra REAL DEFAULT 0.0,"
                      "dec REAL DEFAULT 0.0,"
                      "pixelsPerArcsec REAL DEFAULT 0.0,"
                      "eastToTheRight INTEGER DEFAULT 0,"
                      "width INTEGER DEFAULT 0,"
                      "height INTEGER DEFAULT 0)";
    QSqlQuery query(db);
    if (!query.exec(command))
    {
        qCDebug(KSTARS) << query.lastError();
        qCDebug(KSTARS) << query.executedQuery();
    }
}

bool KSUserDB::DeleteAllImageOverlays()
{
    CreateImageOverlayTableIfNecessary();
    auto db = QSqlDatabase::database(m_ConnectionName);
    if (!db.isValid())
    {
        qCCritical(KSTARS) << "Failed to open database:" << db.lastError();
        return false;
    }

    QSqlTableModel overlays(nullptr, db);
    overlays.setTable("imageOverlays");
    overlays.setFilter("id >= 1");
    overlays.select();
    overlays.removeRows(0, overlays.rowCount());
    overlays.submitAll();

    QSqlQuery query(db);
    QString dropQuery = QString("DROP TABLE imageOverlays");
    if (!query.exec(dropQuery))
        qCWarning(KSTARS) << query.lastError().text();

    return true;
}

bool KSUserDB::AddImageOverlay(const ImageOverlay &overlay)
{
    CreateImageOverlayTableIfNecessary();
    auto db = QSqlDatabase::database(m_ConnectionName);
    if (!db.isValid())
    {
        qCCritical(KSTARS) << "Failed to open database:" << db.lastError();
        return false;
    }

    QSqlTableModel overlays(nullptr, db);
    overlays.setTable("imageOverlays");
    overlays.setFilter("filename LIKE \'" + overlay.m_Filename + "\'");
    overlays.select();

    if (overlays.rowCount() > 0)
    {
        QSqlRecord record = overlays.record(0);
        record.setValue("filename", overlay.m_Filename);
        record.setValue("enabled", static_cast<int>(overlay.m_Enabled));
        record.setValue("nickname", overlay.m_Nickname);
        record.setValue("status", static_cast<int>(overlay.m_Status));
        record.setValue("orientation", overlay.m_Orientation);
        record.setValue("ra", overlay.m_RA);
        record.setValue("dec", overlay.m_DEC);
        record.setValue("pixelsPerArcsec", overlay.m_ArcsecPerPixel);
        record.setValue("eastToTheRight", static_cast<int>(overlay.m_EastToTheRight));
        record.setValue("width", overlay.m_Width);
        record.setValue("height", overlay.m_Height);
        overlays.setRecord(0, record);
        overlays.submitAll();
    }
    else
    {
        int row = 0;
        overlays.insertRows(row, 1);

        overlays.setData(overlays.index(row, 1), overlay.m_Filename);  // row,0 is autoincerement ID
        overlays.setData(overlays.index(row, 2), static_cast<int>(overlay.m_Enabled));
        overlays.setData(overlays.index(row, 3), overlay.m_Nickname);
        overlays.setData(overlays.index(row, 4), static_cast<int>(overlay.m_Status));
        overlays.setData(overlays.index(row, 5), overlay.m_Orientation);
        overlays.setData(overlays.index(row, 6), overlay.m_RA);
        overlays.setData(overlays.index(row, 7), overlay.m_DEC);
        overlays.setData(overlays.index(row, 8), overlay.m_ArcsecPerPixel);
        overlays.setData(overlays.index(row, 9), static_cast<int>(overlay.m_EastToTheRight));
        overlays.setData(overlays.index(row, 10), overlay.m_Width);
        overlays.setData(overlays.index(row, 11), overlay.m_Height);
        overlays.submitAll();
    }
    return true;
}

bool KSUserDB::GetAllImageOverlays(QList<ImageOverlay> *imageOverlayList)
{
    CreateImageOverlayTableIfNecessary();
    auto db = QSqlDatabase::database(m_ConnectionName);
    if (!db.isValid())
    {
        qCCritical(KSTARS) << "Failed to open database:" << db.lastError();
        return false;
    }

    imageOverlayList->clear();
    QSqlTableModel overlays(nullptr, db);
    overlays.setTable("imageOverlays");
    overlays.select();

    for (int i = 0; i < overlays.rowCount(); ++i)
    {
        QSqlRecord record         = overlays.record(i);

        const QString filename        = record.value("filename").toString();
        const bool    enabled         = static_cast<bool>(record.value("enabled").toInt());
        const QString nickname        = record.value("nickname").toString();
        const ImageOverlay::Status status
            = static_cast<ImageOverlay::Status>(record.value("status").toInt());
        const double  orientation     = record.value("orientation").toDouble();
        const double  ra              = record.value("ra").toDouble();
        const double  dec             = record.value("dec").toDouble();
        const double  pixelsPerArcsec = record.value("pixelsPerArcsec").toDouble();
        const bool    eastToTheRight  = static_cast<bool>(record.value("eastToTheRight").toInt());
        const int     width           = record.value("width").toInt();
        const int     height          = record.value("height").toInt();
        ImageOverlay o(filename, enabled, nickname, status, orientation, ra, dec, pixelsPerArcsec,
                       eastToTheRight, width, height);
        imageOverlayList->append(o);
    }

    overlays.clear();
    return true;
}

void KSUserDB::CreateImagingPlannerTableIfNecessary()
{
    auto db = QSqlDatabase::database(m_ConnectionName);
    QString command = "CREATE TABLE IF NOT EXISTS imagingPlanner ( "
                      "id INTEGER DEFAULT NULL PRIMARY KEY AUTOINCREMENT, "
                      "name TEXT NOT NULL,"
                      "flags INTEGER DEFAULT 0,"
                      "notes TEXT DEFAULT NULL)";
    QSqlQuery query(db);
    if (!query.exec(command))
    {
        qCDebug(KSTARS) << query.lastError();
        qCDebug(KSTARS) << query.executedQuery();
    }
}

bool KSUserDB::DeleteAllImagingPlannerEntries()
{
    CreateImagingPlannerTableIfNecessary();
    auto db = QSqlDatabase::database(m_ConnectionName);
    if (!db.isValid())
    {
        qCCritical(KSTARS) << "Failed to open database:" << db.lastError();
        return false;
    }

    QSqlTableModel entries(nullptr, db);
    entries.setTable("imagingPlanner");
    entries.setFilter("id >= 1");
    entries.select();
    entries.removeRows(0, entries.rowCount());
    entries.submitAll();

    QSqlQuery query(db);
    QString dropQuery = QString("DROP TABLE imagingPlanner");
    if (!query.exec(dropQuery))
        qCWarning(KSTARS) << query.lastError().text();

    return true;
}

bool KSUserDB::AddImagingPlannerEntry(const ImagingPlannerDBEntry &entry)
{
#ifdef HAVE_INDI
    CreateImagingPlannerTableIfNecessary();
    auto db = QSqlDatabase::database(m_ConnectionName);
    if (!db.isValid())
    {
        qCCritical(KSTARS) << "Failed to open database:" << db.lastError();
        return false;
    }

    QSqlTableModel entries(nullptr, db);
    entries.setTable("imagingPlanner");
    entries.setFilter("name LIKE \'" + entry.m_Name + "\'");
    entries.select();

    if (entries.rowCount() > 0)
    {
        QSqlRecord record = entries.record(0);
        record.setValue("name", entry.m_Name);
        record.setValue("flags", static_cast<int>(entry.m_Flags));
        record.setValue("notes", entry.m_Notes);
        entries.setRecord(0, record);
        entries.submitAll();
    }
    else
    {
        int row = 0;
        entries.insertRows(row, 1);
        entries.setData(entries.index(row, 1), entry.m_Name);  // row,0 is autoincerement ID
        entries.setData(entries.index(row, 2), static_cast<int>(entry.m_Flags));
        entries.setData(entries.index(row, 3), entry.m_Notes);
        entries.submitAll();
    }
#endif
    return true;
}

bool KSUserDB::GetAllImagingPlannerEntries(QList<ImagingPlannerDBEntry> *entryList)
{
#ifdef HAVE_INDI
    CreateImagingPlannerTableIfNecessary();
    auto db = QSqlDatabase::database(m_ConnectionName);
    if (!db.isValid())
    {
        qCCritical(KSTARS) << "Failed to open database:" << db.lastError();
        return false;
    }

    entryList->clear();
    QSqlTableModel entries(nullptr, db);
    entries.setTable("imagingPlanner");
    entries.select();

    for (int i = 0; i < entries.rowCount(); ++i)
    {
        QSqlRecord record         = entries.record(i);
        const QString name      = record.value("name").toString();
        const int     flags     = record.value("flags").toInt();
        const QString notes     = record.value("notes").toString();
        ImagingPlannerDBEntry e(name, flags, notes);
        entryList->append(e);
    }

    entries.clear();
#endif
    return true;
}

void KSUserDB::CreateSkyMapViewTableIfNecessary()
{
    auto db = QSqlDatabase::database(m_ConnectionName);
    QString command = "CREATE TABLE IF NOT EXISTS SkyMapViews ( "
                      "id INTEGER DEFAULT NULL PRIMARY KEY AUTOINCREMENT, "
                      "name TEXT NOT NULL UNIQUE, "
                      "data JSON)";
    QSqlQuery query(db);
    if (!query.exec(command))
    {
        qCDebug(KSTARS) << query.lastError();
        qCDebug(KSTARS) << query.executedQuery();
    }
}

bool KSUserDB::DeleteAllSkyMapViews()
{
    CreateSkyMapViewTableIfNecessary();
    auto db = QSqlDatabase::database(m_ConnectionName);
    if (!db.isValid())
    {
        qCCritical(KSTARS) << "Failed to open database:" << db.lastError();
        return false;
    }

    QSqlTableModel views(nullptr, db);
    views.setTable("SkyMapViews");
    views.setFilter("id >= 1");
    views.select();
    views.removeRows(0, views.rowCount());
    views.submitAll();

    QSqlQuery query(db);
    QString dropQuery = QString("DROP TABLE SkyMapViews");
    if (!query.exec(dropQuery))
        qCWarning(KSTARS) << query.lastError().text();

    return true;
}

bool KSUserDB::AddSkyMapView(const SkyMapView &view)
{
    CreateSkyMapViewTableIfNecessary();
    auto db = QSqlDatabase::database(m_ConnectionName);
    if (!db.isValid())
    {
        qCCritical(KSTARS) << "Failed to open database:" << db.lastError();
        return false;
    }

    QSqlTableModel views(nullptr, db);
    views.setTable("SkyMapViews");
    views.setFilter("name LIKE \'" + view.name + "\'");
    views.select();

    if (views.rowCount() > 0)
    {
        QSqlRecord record = views.record(0);
        record.setValue("name", view.name);
        record.setValue("data", QJsonDocument(view.toJson()).toJson(QJsonDocument::Compact));
        views.setRecord(0, record);
        views.submitAll();
    }
    else
    {
        int row = 0;
        views.insertRows(row, 1);

        views.setData(views.index(row, 1), view.name);  // row,0 is autoincerement ID
        views.setData(views.index(row, 2), QJsonDocument(view.toJson()).toJson(QJsonDocument::Compact));
        views.submitAll();
    }
    return true;
}

bool KSUserDB::GetAllSkyMapViews(QList<SkyMapView> &skyMapViewList)
{
    CreateSkyMapViewTableIfNecessary();
    auto db = QSqlDatabase::database(m_ConnectionName);
    if (!db.isValid())
    {
        qCCritical(KSTARS) << "Failed to open database:" << db.lastError();
        return false;
    }

    skyMapViewList.clear();
    QSqlTableModel views(nullptr, db);
    views.setTable("SkyMapViews");
    views.select();

    for (int i = 0; i < views.rowCount(); ++i)
    {
        QSqlRecord record         = views.record(i);

        const QString name        = record.value("name").toString();
        const QJsonDocument data  = QJsonDocument::fromJson(record.value("data").toString().toUtf8());
        Q_ASSERT((!data.isNull()) && data.isObject());
        if (data.isNull() || !data.isObject())
        {
            qCCritical(KSTARS) << "Data associated with sky map view " << name << " is invalid!";
            continue;
        }
        SkyMapView o(name, data.object());
        skyMapViewList.append(o);
    }

    views.clear();
    return true;
}

int KSUserDB::AddProfile(const QString &name)
{
    auto db = QSqlDatabase::database(m_ConnectionName);
    if (!db.isValid())
    {
        qCCritical(KSTARS) << "Failed to open database:" << db.lastError();
        return -1;
    }

    int id = -1;

    QSqlQuery query(db);
    bool rc = query.exec(QString("INSERT INTO profile (name) VALUES('%1')").arg(name));

    if (rc == false)
        qCWarning(KSTARS) << query.lastQuery() << query.lastError().text();
    else
        id = query.lastInsertId().toInt();

    return id;
}

bool KSUserDB::DeleteProfile(const QSharedPointer<ProfileInfo> &pi)
{
    auto db = QSqlDatabase::database(m_ConnectionName);
    if (!db.isValid())
    {
        qCCritical(KSTARS) << "Failed to open database:" << db.lastError();
        return false;
    }

    QSqlQuery query(db);
    bool rc;

    rc = query.exec("DELETE FROM profile WHERE id=" + QString::number(pi->id));

    if (rc == false)
        qCWarning(KSTARS) << query.lastQuery() << query.lastError().text();

    return rc;
}

bool KSUserDB::PurgeProfile(const QSharedPointer<ProfileInfo> &pi)
{
    auto db = QSqlDatabase::database(m_ConnectionName);
    if (!db.isValid())
    {
        qCCritical(KSTARS) << "Failed to open database:" << db.lastError();
        return false;
    }

    QSqlQuery query(db);
    bool rc;

    rc = query.exec("DELETE FROM profile WHERE id=" + QString::number(pi->id));
    if (rc == false)
        qCWarning(KSTARS) << query.lastQuery() << query.lastError().text();
    rc = query.exec("DELETE FROM profilesettings WHERE profile=" + QString::number(pi->id));
    if (rc == false)
        qCWarning(KSTARS) << query.lastQuery() << query.lastError().text();
    rc = query.exec("DELETE FROM opticaltrainsettings WHERE opticaltrain IN (select id FROM opticaltrains WHERE profile=" +
                    QString::number(pi->id) + ")");
    if (rc == false)
        qCWarning(KSTARS) << query.lastQuery() << query.lastError().text();
    rc = query.exec("DELETE FROM opticaltrains WHERE profile=" + QString::number(pi->id));
    if (rc == false)
        qCWarning(KSTARS) << query.lastQuery() << query.lastError().text();

    return rc;
}

bool KSUserDB::SaveProfile(const QSharedPointer<ProfileInfo> &pi)
{
    auto db = QSqlDatabase::database(m_ConnectionName);
    if (!db.isValid())
    {
        qCCritical(KSTARS) << "Failed to open database:" << db.lastError();
        return false;
    }

    // Remove all drivers
    DeleteProfileDrivers(pi);

    QSqlQuery query(db);

    // Clear data
    if (!query.exec(QString("UPDATE profile SET "
                            "host=null,port=null,city=null,province=null,country=null,indiwebmanagerport=NULL,"
                            "autoconnect=NULL,portselector=NULL,indihub=0 WHERE id=%1")
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

    // Update remote drivers
    if (!query.exec(QString("UPDATE profile SET remotedrivers='%1' WHERE id=%2").arg(pi->remotedrivers).arg(pi->id)))
        qCWarning(KSTARS) << query.executedQuery() << query.lastError().text();

    // Update scripts
    if (!query.exec(QString("UPDATE profile SET scripts='%1' WHERE id=%2").arg(QString::fromLocal8Bit(pi->scripts)).arg(
                        pi->id)))
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

    return true;
}

bool KSUserDB::GetAllProfiles(QList<QSharedPointer<ProfileInfo>> &profiles)
{
    auto db = QSqlDatabase::database(m_ConnectionName);
    if (!db.isValid())
    {
        qCCritical(KSTARS) << "Failed to open database:" << db.lastError();
        return false;
    }

    profiles.clear();
    QSqlTableModel profile(nullptr, db);
    profile.setTable("profile");
    profile.select();

    for (int i = 0; i < profile.rowCount(); ++i)
    {
        QSqlRecord record = profile.record(i);

        int id       = record.value("id").toInt();
        QString name = record.value("name").toString();
        QSharedPointer<ProfileInfo> pi(new ProfileInfo(id, name));

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

        pi->remotedrivers = record.value("remotedrivers").toString();

        pi->scripts = record.value("scripts").toByteArray();

        GetProfileDrivers(pi);

        profiles.append(std::move(pi));
    }

    return true;
}

bool KSUserDB::GetProfileDrivers(const QSharedPointer<ProfileInfo> &pi)
{
    auto db = QSqlDatabase::database(m_ConnectionName);
    if (!db.isValid())
    {
        qCCritical(KSTARS) << "Failed to open database:" << db.lastError();
        return false;
    }

    QSqlTableModel driver(nullptr, db);
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
    return true;
}

/*void KSUserDB::GetProfileCustomDrivers(ProfileInfo* pi)
{
    userdb_.open();
    QSqlTableModel custom_driver(0, userdb_);
    custom_driver.setTable("driver");
    custom_driver.setFilter("profile=" + QString::number(pi->id));
    if (custom_driver.select() == false)
        qDebug() << Q_FUNC_INFO << "custom driver select error: " << custom_driver.query().lastQuery() << custom_driver.lastError().text();

    QSqlRecord record = custom_driver.record(0);
    pi->customDrivers   = record.value("drivers").toString();

    custom_driver.clear();
    userdb_.close();
}*/

bool KSUserDB::DeleteProfileDrivers(const QSharedPointer<ProfileInfo> &pi)
{
    auto db = QSqlDatabase::database(m_ConnectionName);
    if (!db.isValid())
    {
        qCCritical(KSTARS) << "Failed to open database:" << db.lastError();
        return false;
    }

    QSqlQuery query(db);

    /*if (!query.exec("DELETE FROM custom_driver WHERE profile=" + QString::number(pi->id)))
        qDebug() << Q_FUNC_INFO << query.lastQuery() << query.lastError().text();*/

    if (!query.exec("DELETE FROM driver WHERE profile=" + QString::number(pi->id)))
        qCWarning(KSTARS) << query.executedQuery() << query.lastError().text();

    return true;
}

/*
 * DSLR Lens Section
*/
bool KSUserDB::AddDSLRLens(const QString &model, const QString &vendor, const double focalLength, const double focalRatio)
{
    auto db = QSqlDatabase::database(m_ConnectionName);
    if (!db.isValid())
    {
        qCCritical(KSTARS) << "Failed to open database:" << db.lastError();
        return false;
    }

    QSqlTableModel equip(nullptr, db);
    equip.setTable("dslrlens");

    QSqlRecord record = equip.record();
    record.setValue("Vendor", vendor);
    record.setValue("Model", model);
    record.setValue("FocalLength", focalLength);
    record.setValue("FocalRatio", focalRatio);

    if (equip.insertRecord(-1, record) == false)
        qCritical() << __FUNCTION__ << equip.lastError();
    equip.submitAll();
    equip.clear();

    return true;

}

bool KSUserDB::AddDSLRLens(const QString &model, const QString &vendor, const double focalLength, const double focalRatio,
                           const QString &id)
{
    auto db = QSqlDatabase::database(m_ConnectionName);
    if (!db.isValid())
    {
        qCCritical(KSTARS) << "Failed to open database:" << db.lastError();
        return false;
    }

    QSqlTableModel equip(nullptr, db);
    equip.setTable("dslrlens");
    equip.setFilter("id = " + id);
    equip.select();

    if (equip.rowCount() > 0)
    {
        QSqlRecord record = equip.record(0);
        record.setValue("Vendor", vendor);
        record.setValue("Model", model);
        record.setValue("FocalLength", focalLength);
        record.setValue("FocalRatio", focalRatio);
        equip.setRecord(0, record);
        equip.submitAll();
    }

    return true;
}

#ifndef KSTARS_LITE
bool KSUserDB::GetAllDSLRLenses(QList<OAL::DSLRLens *> &dslrlens_list)
{
    dslrlens_list.clear();

    auto db = QSqlDatabase::database(m_ConnectionName);
    if (!db.isValid())
    {
        qCCritical(KSTARS) << "Failed to open database:" << db.lastError();
        return false;
    }

    QSqlTableModel equip(nullptr, db);
    equip.setTable("dslrlens");
    equip.select();

    for (int i = 0; i < equip.rowCount(); ++i)
    {
        QSqlRecord record  = equip.record(i);
        QString id         = record.value("id").toString();
        QString vendor     = record.value("Vendor").toString();
        QString model      = record.value("Model").toString();
        double focalLength = record.value("FocalLength").toDouble();
        double focalRatio  = record.value("FocalRatio").toDouble();
        OAL::DSLRLens *o   = new OAL::DSLRLens(id, model, vendor, focalLength, focalRatio);
        dslrlens_list.append(o);
    }

    equip.clear();

    return true;
}
#endif

bool KSUserDB::getOpticalElementByID(int id, QJsonObject &element)
{
    // Get all OAL equipment filter list
    QList<OAL::Scope *> scopeList;
    QList<OAL::DSLRLens *> dslrlensList;
    KStarsData::Instance()->userdb()->GetAllScopes(scopeList);
    KStarsData::Instance()->userdb()->GetAllDSLRLenses(dslrlensList);

    for (auto &oneScope : scopeList)
    {
        if (oneScope->id().toInt() == id)
        {
            element = oneScope->toJson();
            return true;
        }
    }

    for (auto &oneLens : dslrlensList)
    {
        if (oneLens->id().toInt() == id)
        {
            element = oneLens->toJson();
            return true;
        }
    }

    return false;
}

bool KSUserDB::getLastOpticalElement(QJsonObject &element)
{
    // Get all OAL equipment filter list
    QList<OAL::Scope *> scopeList;
    QList<OAL::DSLRLens *> dslrlensList;
    KStarsData::Instance()->userdb()->GetAllScopes(scopeList);
    KStarsData::Instance()->userdb()->GetAllDSLRLenses(dslrlensList);

    if (!scopeList.empty())
    {
        element = scopeList.last()->toJson();
        return true;
    }

    if (!dslrlensList.empty())
    {
        element = dslrlensList.last()->toJson();
        return true;
    }

    return false;
}

bool KSUserDB::getOpticalElementByName(const QString &name, QJsonObject &element)
{
    // Get all OAL equipment filter list
    QList<OAL::Scope *> scopeList;
    QList<OAL::DSLRLens *> dslrlensList;
    KStarsData::Instance()->userdb()->GetAllScopes(scopeList);
    KStarsData::Instance()->userdb()->GetAllDSLRLenses(dslrlensList);

    for (auto &oneScope : scopeList)
    {
        if (oneScope->name() == name)
        {
            element = oneScope->toJson();
            return true;
        }
    }

    for (auto &oneLens : dslrlensList)
    {
        if (oneLens->name() == name)
        {
            element = oneLens->toJson();
            return true;
        }
    }

    return false;
}

QStringList KSUserDB::getOpticalElementNames()
{
    QStringList names;

    // Get all OAL equipment filter list
    QList<OAL::Scope *> scopeList;
    QList<OAL::DSLRLens *> dslrlensList;
    KStarsData::Instance()->userdb()->GetAllScopes(scopeList);
    KStarsData::Instance()->userdb()->GetAllDSLRLenses(dslrlensList);

    for (auto &oneValue : scopeList)
        names << oneValue->name();

    for (auto &oneValue : dslrlensList)
        names << oneValue->name();

    return names;
}

void KSUserDB::AddProfileSettings(uint32_t profile, const QByteArray &settings)
{
    auto db = QSqlDatabase::database(m_ConnectionName);
    if (!db.isValid())
        qCCritical(KSTARS) << "Failed to open database:" << db.lastError();

    QSqlTableModel profileSettings(nullptr, db);
    profileSettings.setTable("profilesettings");
    profileSettings.select();

    QSqlRecord record = profileSettings.record();
    record.setValue("profile", profile);
    record.setValue("settings", settings);
    profileSettings.insertRecord(-1, record);

    if (!profileSettings.submitAll())
        qCWarning(KSTARS) << profileSettings.lastError();

}

void KSUserDB::UpdateProfileSettings(uint32_t profile, const QByteArray &settings)
{
    auto db = QSqlDatabase::database(m_ConnectionName);
    if (!db.isValid())
        qCCritical(KSTARS) << "Failed to open database:" << db.lastError();

    QSqlTableModel profileSettings(nullptr, db);
    profileSettings.setTable("profilesettings");
    profileSettings.setFilter(QString("profile=%1").arg(profile));
    profileSettings.select();

    QSqlRecord record = profileSettings.record(0);
    record.setValue("settings", settings);
    profileSettings.setRecord(0, record);

    if (!profileSettings.submitAll())
        qCWarning(KSTARS) << profileSettings.lastError();
}

void KSUserDB::DeleteProfileSettings(uint32_t profile)
{
    auto db = QSqlDatabase::database(m_ConnectionName);
    if (!db.isValid())
        qCCritical(KSTARS) << "Failed to open database:" << db.lastError();

    QSqlTableModel profileSettings(nullptr, db);
    profileSettings.setTable("profilesettings");
    profileSettings.setFilter(QString("profile=%1").arg(profile));

    profileSettings.select();
    profileSettings.removeRows(0, profileSettings.rowCount() - 1);
    profileSettings.submitAll();
}

bool KSUserDB::GetProfileSettings(uint32_t profile, QVariantMap &settings)
{
    auto db = QSqlDatabase::database(m_ConnectionName);
    if (!db.isValid())
    {
        qCCritical(KSTARS) << "Failed to open database:" << db.lastError();
        return false;
    }

    settings.clear();

    QSqlTableModel profileSettings(nullptr, db);
    profileSettings.setTable("profilesettings");
    profileSettings.setFilter(QString("profile=%1").arg(profile));
    profileSettings.select();

    if (profileSettings.rowCount() > 0)
    {
        QSqlRecord record = profileSettings.record(0);
        auto settingsField = record.value("settings").toByteArray();
        QJsonParseError parserError;
        auto doc = QJsonDocument::fromJson(settingsField, &parserError);
        if (parserError.error == QJsonParseError::NoError)
        {
            settings = doc.object().toVariantMap();

            return true;
        }
    }

    return false;
}

bool KSUserDB::AddOpticalTrainSettings(uint32_t train, const QByteArray &settings)
{
    auto db = QSqlDatabase::database(m_ConnectionName);
    if (!db.isValid())
    {
        qCCritical(KSTARS) << "Failed to open database:" << db.lastError();
        return false;
    }

    QSqlTableModel OpticalTrainSettings(nullptr, db);
    OpticalTrainSettings.setTable("opticaltrainsettings");
    OpticalTrainSettings.select();

    QSqlRecord record = OpticalTrainSettings.record();
    record.setValue("opticaltrain", train);
    record.setValue("settings", settings);
    OpticalTrainSettings.insertRecord(-1, record);

    if (!OpticalTrainSettings.submitAll())
    {
        qCWarning(KSTARS) << OpticalTrainSettings.lastError();
        return false;
    }

    return true;
}

bool KSUserDB::UpdateOpticalTrainSettings(uint32_t train, const QByteArray &settings)
{
    auto db = QSqlDatabase::database(m_ConnectionName);
    if (!db.isValid())
    {
        qCCritical(KSTARS) << "Failed to open database:" << db.lastError();
        return false;
    }

    QSqlTableModel OpticalTrainSettings(nullptr, db);
    OpticalTrainSettings.setTable("opticaltrainsettings");
    OpticalTrainSettings.setFilter(QString("opticaltrain=%1").arg(train));
    OpticalTrainSettings.select();

    QSqlRecord record = OpticalTrainSettings.record(0);
    record.setValue("settings", settings);
    OpticalTrainSettings.setRecord(0, record);

    if (!OpticalTrainSettings.submitAll())
        qCWarning(KSTARS) << OpticalTrainSettings.lastError();

    return true;
}

bool KSUserDB::DeleteOpticalTrainSettings(uint32_t train)
{
    auto db = QSqlDatabase::database(m_ConnectionName);
    if (!db.isValid())
    {
        qCCritical(KSTARS) << "Failed to open database:" << db.lastError();
        return false;
    }

    QSqlQuery query(db);
    return query.exec(QString("DELETE FROM opticaltrainsettings WHERE opticaltrain=%1").arg(train));
}

bool KSUserDB::GetOpticalTrainSettings(uint32_t train, QVariantMap &settings)
{
    auto db = QSqlDatabase::database(m_ConnectionName);
    if (!db.isValid())
    {
        qCCritical(KSTARS) << "Failed to open database:" << db.lastError();
        return false;
    }

    settings.clear();

    QSqlTableModel OpticalTrainSettings(nullptr, db);
    OpticalTrainSettings.setTable("OpticalTrainsettings");
    OpticalTrainSettings.setFilter(QString("opticaltrain=%1").arg(train));
    OpticalTrainSettings.select();

    if (OpticalTrainSettings.rowCount() > 0)
    {
        QSqlRecord record = OpticalTrainSettings.record(0);
        auto settingsField = record.value("settings").toByteArray();
        QJsonParseError parserError;
        auto doc = QJsonDocument::fromJson(settingsField, &parserError);
        if (parserError.error == QJsonParseError::NoError)
        {
            settings = doc.object().toVariantMap();

            return true;
        }
    }

    return false;
}

/* Collimation Overlay Elements Section */

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool KSUserDB::AddCollimationOverlayElement(const QVariantMap &oneElement)
{
    auto db = QSqlDatabase::database(m_ConnectionName);
    if (!db.isValid())
    {
        qCCritical(KSTARS) << "Failed to open database:" << db.lastError();
        return false;
    }

    QSqlTableModel collimationOverlayElement(nullptr, db);
    collimationOverlayElement.setTable("collimationoverlayelements");
    collimationOverlayElement.select();

    QSqlRecord record = collimationOverlayElement.record();

    // Remove PK so that it gets auto-incremented later
    record.remove(0);

    for (QVariantMap::const_iterator iter = oneElement.begin(); iter != oneElement.end(); ++iter)
        record.setValue(iter.key(), iter.value());

    collimationOverlayElement.insertRecord(-1, record);

    if (!collimationOverlayElement.submitAll())
    {
        qCWarning(KSTARS) << collimationOverlayElement.lastError();
        return false;
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool KSUserDB::UpdateCollimationOverlayElement(const QVariantMap &oneElement, int id)
{
    auto db = QSqlDatabase::database(m_ConnectionName);
    if (!db.isValid())
    {
        qCCritical(KSTARS) << "Failed to open database:" << db.lastError();
        return false;
    }

    QSqlTableModel collimationOverlayElement(nullptr, db);
    collimationOverlayElement.setTable("collimationoverlayelements");
    collimationOverlayElement.setFilter(QString("id=%1").arg(id));
    collimationOverlayElement.select();

    QSqlRecord record = collimationOverlayElement.record(0);

    for (QVariantMap::const_iterator iter = oneElement.begin(); iter != oneElement.end(); ++iter)
        record.setValue(iter.key(), iter.value());

    collimationOverlayElement.setRecord(0, record);

    if (!collimationOverlayElement.submitAll())
    {
        qCWarning(KSTARS) << collimationOverlayElement.lastError();
        return false;
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool KSUserDB::DeleteCollimationOverlayElement(int id)
{
    auto db = QSqlDatabase::database(m_ConnectionName);
    if (!db.isValid())
    {
        qCCritical(KSTARS) << "Failed to open database:" << db.lastError();
        return false;
    }

    QSqlTableModel collimationOverlayElement(nullptr, db);
    collimationOverlayElement.setTable("collimationoverlayelements");
    collimationOverlayElement.setFilter(QString("id=%1").arg(id));

    collimationOverlayElement.select();

    collimationOverlayElement.removeRows(0, 1);
    collimationOverlayElement.submitAll();
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool KSUserDB::GetCollimationOverlayElements(QList<QVariantMap> &collimationOverlayElements)
{
    auto db = QSqlDatabase::database(m_ConnectionName);
    if (!db.isValid())
    {
        qCCritical(KSTARS) << "Failed to open database:" << db.lastError();
        return false;
    }

    collimationOverlayElements.clear();

    QSqlTableModel collimationOverlayElement(nullptr, db);
    collimationOverlayElement.setTable("collimationoverlayelements");
    collimationOverlayElement.select();

    for (int i = 0; i < collimationOverlayElement.rowCount(); ++i)
    {
        QVariantMap recordMap;
        QSqlRecord record = collimationOverlayElement.record(i);
        for (int j = 0; j < record.count(); j++)
            recordMap[record.fieldName(j)] = record.value(j);

        collimationOverlayElements.append(recordMap);
    }

    return true;
}
