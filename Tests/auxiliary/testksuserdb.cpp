/*  KStars class tests
    SPDX-FileCopyrightText: 2020 Eric Dejouhanet <eric.dejouhanet@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "testksuserdb.h"
#include "../testhelpers.h"
#include "ksuserdb.h"

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
