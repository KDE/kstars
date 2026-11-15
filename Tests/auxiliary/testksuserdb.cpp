/*  KStars class tests
    SPDX-FileCopyrightText: 2020 Eric Dejouhanet <eric.dejouhanet@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "testksuserdb.h"
#include "../testhelpers.h"
#include "ksuserdb.h"
#include "cdsmirrors.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

namespace
{
// ID, hips_service_url and hips_service_mirrors of every row of the hips table
QMap<QString, QPair<QString, QString>> readHIPSUrls(const QString &connectionName)
{
    QMap<QString, QPair<QString, QString>> rows;
    QSqlQuery query(QSqlDatabase::database(connectionName));
    if (query.exec("SELECT ID, hips_service_url, hips_service_mirrors FROM hips"))
        while (query.next())
            rows[query.value(0).toString()] = { query.value(1).toString(), query.value(2).toString() };
    return rows;
}
}

TestKSUserDB::TestKSUserDB(QObject *parent) : QObject(parent)
{
}

void TestKSUserDB::initTestCase()
{
}

void TestKSUserDB::cleanupTestCase()
{
}

void TestKSUserDB::init()
{
    KTEST_BEGIN();
}

void TestKSUserDB::cleanup()
{
    KTEST_END();
}

void TestKSUserDB::testInitializeDB()
{
    QScopedPointer<KSUserDB> testDB(new KSUserDB());
    QVERIFY(nullptr != testDB);

    // If the KStars folder does not exist, database cannot be initialised
    QVERIFY(QDir(KSPaths::writableLocation(QStandardPaths::AppLocalDataLocation)).removeRecursively());
    QVERIFY(!testDB->Initialize());
    auto fullpath = testDB->connectionName();
    QVERIFY(!QFile(fullpath).exists());

    // If the KStars folder has just been created, database can be initialised
    QVERIFY(QDir(KSPaths::writableLocation(QStandardPaths::AppLocalDataLocation)).mkpath("."));
    QVERIFY(testDB->Initialize());
    fullpath = testDB->connectionName();
    QVERIFY(QFile(fullpath).exists());

    // Database can be initialised once again (without change?)
    QVERIFY(testDB->Initialize());
    QVERIFY(QFile(fullpath).exists());

    // If there is garbage in the database and there is no backup, database can be initialised
    QVERIFY(QFile(fullpath).exists());
    {
        QFile dbf(fullpath);
        QVERIFY(dbf.open(QIODevice::WriteOnly));
        QCOMPARE(dbf.write("garbage", 7), 7);
    }
    QVERIFY(testDB->Initialize());
    QVERIFY(QFile(fullpath).exists());

    // If there is no database file, but there is a backup, database can be initialised
    QVERIFY(QFile(fullpath).exists());
    QVERIFY(QFile(fullpath).rename(fullpath + ".backup"));
    QVERIFY(testDB->Initialize());
    QVERIFY(QFile(fullpath).exists());

    // If there is garbage in the database but there is a backup, database can be initialised
    QVERIFY(QFile(fullpath).exists());
    {
        QFile dbf(fullpath);
        QVERIFY(dbf.open(QIODevice::WriteOnly));
        QCOMPARE(dbf.write("garbage", 7), 7);
    }
    QVERIFY(testDB->Initialize());
    QVERIFY(QFile(fullpath).exists());
}

void TestKSUserDB::testHIPSDefaults()
{
    // A new database has the default surveys on the current CDS host with their mirrors (bug 528778)
    QVERIFY(QDir(KSPaths::writableLocation(QStandardPaths::AppLocalDataLocation)).mkpath("."));
    QScopedPointer<KSUserDB> testDB(new KSUserDB());
    QVERIFY(testDB->Initialize());

    const auto rows = readHIPSUrls(testDB->connectionName());
    QCOMPARE(rows.count(), 3);
    const QMap<QString, QString> expectedUrls =
    {
        { "CDS/P/DSS2/color", "https://alasky.cds.unistra.fr/DSS/DSSColor" },
        { "CDS/P/2MASS/color", "https://alasky.cds.unistra.fr/2MASS/Color" },
        { "CDS/P/Fermi/color", "https://alasky.cds.unistra.fr/Fermi/Color" },
    };
    for (auto it = expectedUrls.constBegin(); it != expectedUrls.constEnd(); ++it)
    {
        QVERIFY2(rows.contains(it.key()), qPrintable(it.key()));
        QCOMPARE(rows.value(it.key()).first, it.value());
        QCOMPARE(rows.value(it.key()).second,
                 CDSMirrors::defaultHipsMirrors(it.key()).join(CDSMirrors::HipsMirrorSeparator));
    }

    // The sources read by HIPSManager carry the mirrors
    QList<QMap<QString, QString>> sources;
    QVERIFY(testDB->GetAllHIPSSources(sources));
    bool foundDSS = false;
    for (const auto &oneSource : sources)
    {
        if (oneSource.value("obs_title") == "DSS Colored")
        {
            foundDSS = true;
            QVERIFY(oneSource.value("hips_service_mirrors").contains("irsa.ipac.caltech.edu"));
        }
    }
    QVERIFY(foundDSS);
}

void TestKSUserDB::testHIPSMigration()
{
    QVERIFY(QDir(KSPaths::writableLocation(QStandardPaths::AppLocalDataLocation)).mkpath("."));
    QScopedPointer<KSUserDB> testDB(new KSUserDB());
    QVERIFY(testDB->Initialize());

    // Turn the hips table back into the one of schema 317, with the URLs of KStars 3.8.5
    {
        QSqlQuery query(QSqlDatabase::database(testDB->connectionName()));
        QVERIFY2(query.exec("DROP TABLE hips"), qPrintable(query.lastError().text()));
        QVERIFY2(query.exec("CREATE TABLE hips (ID TEXT NOT NULL UNIQUE,"
                            "obs_title TEXT NOT NULL, obs_description TEXT NOT NULL, hips_order TEXT NOT NULL,"
                            "hips_frame TEXT NOT NULL, hips_tile_width TEXT NOT NULL, hips_tile_format TEXT NOT NULL,"
                            "hips_service_url TEXT NOT NULL, moc_sky_fraction TEXT NOT NULL)"),
                 qPrintable(query.lastError().text()));
        const QList<QPair<QString, QString>> oldRows =
        {
            { "CDS/P/DSS2/color", "http://alasky.u-strasbg.fr/DSS/DSSColor" },
            { "CDS/P/2MASS/color", "http://alaskybis.u-strasbg.fr/2MASS/Color" },
            { "CDS/P/Fermi/color", "http://alaskybis.u-strasbg.fr/Fermi/Color" },
            // Added by the user from the HiPS settings
            { "CDS/P/Mellinger/color", "http://alasky.u-strasbg.fr/MellingerRGB" },
            { "ESAVO/P/HST/EPO", "https://skies.esac.esa.int/HST/EPO" },
        };
        for (const auto &row : oldRows)
        {
            query.prepare("INSERT INTO hips VALUES (:id, :id, '', '9', 'equatorial', '512', 'jpeg', :url, '1')");
            query.bindValue(":id", row.first);
            query.bindValue(":url", row.second);
            QVERIFY2(query.exec(), qPrintable(query.lastError().text()));
        }
        QVERIFY2(query.exec("UPDATE Version SET Version=317"), qPrintable(query.lastError().text()));
    }

    // Opening the database again migrates it
    QVERIFY(testDB->Initialize());

    const auto rows = readHIPSUrls(testDB->connectionName());
    QCOMPARE(rows.count(), 5);
    QCOMPARE(rows.value("CDS/P/DSS2/color").first, QString("https://alasky.cds.unistra.fr/DSS/DSSColor"));
    QCOMPARE(rows.value("CDS/P/DSS2/color").second,
             CDSMirrors::defaultHipsMirrors("CDS/P/DSS2/color").join(CDSMirrors::HipsMirrorSeparator));
    // The old main URL was the alaskybis server, which is now the main URL, so it is not repeated as a mirror
    QCOMPARE(rows.value("CDS/P/2MASS/color").first, QString("https://alaskybis.cds.unistra.fr/2MASS/Color"));
    QCOMPARE(rows.value("CDS/P/2MASS/color").second,
             QString("https://irsa.ipac.caltech.edu/data/hips/CDS/2MASS/Color"));
    QCOMPARE(rows.value("CDS/P/Fermi/color").first, QString("https://alaskybis.cds.unistra.fr/Fermi/Color"));
    QCOMPARE(rows.value("CDS/P/Fermi/color").second, QString("https://skies.esac.esa.int/FermiColor"));
    QCOMPARE(rows.value("CDS/P/Mellinger/color").first, QString("https://alasky.cds.unistra.fr/MellingerRGB"));
    QCOMPARE(rows.value("CDS/P/Mellinger/color").second, QString());
    QCOMPARE(rows.value("ESAVO/P/HST/EPO").first, QString("https://skies.esac.esa.int/HST/EPO"));
    QCOMPARE(rows.value("ESAVO/P/HST/EPO").second, QString());

    // Opening it once more changes nothing
    QVERIFY(testDB->Initialize());
    QCOMPARE(readHIPSUrls(testDB->connectionName()), rows);
}

void TestKSUserDB::testCreateScopes_data()
{
}

void TestKSUserDB::testCreateScopes()
{
    QSKIP("Not implemented yet.");
}

void TestKSUserDB::testCreateEyepieces_data()
{
}

void TestKSUserDB::testCreateEyepieces()
{
    QSKIP("Not implemented yet.");
}

void TestKSUserDB::testCreateLenses_data()
{
}

void TestKSUserDB::testCreateLenes()
{
    QSKIP("Not implemented yet.");
}

void TestKSUserDB::testCreateFilters_data()
{
}

void TestKSUserDB::testCreateFilters()
{
    QSKIP("Not implemented yet.");
}

void TestKSUserDB::testCreateProfiles_data()
{
}

void TestKSUserDB::testCreateProfilees()
{
    QSKIP("Not implemented yet.");
}

void TestKSUserDB::testCreateDatabase()
{
    QSKIP("Not implemented yet.");
}

void TestKSUserDB::testCoordinates()
{
    QSKIP("Not implemented yet.");
}

QTEST_GUILESS_MAIN(TestKSUserDB)
