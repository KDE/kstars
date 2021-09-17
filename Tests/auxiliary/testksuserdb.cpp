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
    KSUserDB * testDB = new KSUserDB();
    QVERIFY(nullptr != testDB);

    // If the KStars folder does not exist, database cannot be initialised
    QVERIFY(QDir(KSPaths::writableLocation(QStandardPaths::AppDataLocation)).removeRecursively());
    QVERIFY(!testDB->Initialize());
    QVERIFY(!QFile(testDB->GetDatabase().databaseName()).exists());

    // If the KStars folder has just been created, database can be initialised
    QVERIFY(QDir(KSPaths::writableLocation(QStandardPaths::AppDataLocation)).mkpath("."));
    QVERIFY(testDB->Initialize());
    QVERIFY(QFile(testDB->GetDatabase().databaseName()).exists());

    // Database can be initialised once again (without change?)
    QVERIFY(testDB->Initialize());
    QVERIFY(QFile(testDB->GetDatabase().databaseName()).exists());

    // If there is garbage in the database and there is no backup, database can be initialised
    QVERIFY(QFile(testDB->GetDatabase().databaseName()).exists());
    {
        QFile dbf(testDB->GetDatabase().databaseName());
        QVERIFY(dbf.open(QIODevice::WriteOnly));
        QCOMPARE(dbf.write("garbage", 7), 7);
    }
    QVERIFY(testDB->Initialize());
    QVERIFY(QFile(testDB->GetDatabase().databaseName()).exists());

    // If there is no database file, but there is a backup, database can be initialised
    QVERIFY(QFile(testDB->GetDatabase().databaseName()).exists());
    QVERIFY(QFile(testDB->GetDatabase().databaseName()).rename(testDB->GetDatabase().databaseName() + ".backup"));
    QVERIFY(testDB->Initialize());
    QVERIFY(QFile(testDB->GetDatabase().databaseName()).exists());

    // If there is garbage in the database but there is a backup, database can be initialised
    QVERIFY(QFile(testDB->GetDatabase().databaseName()).exists());
    {
        QFile dbf(testDB->GetDatabase().databaseName());
        QVERIFY(dbf.open(QIODevice::WriteOnly));
        QCOMPARE(dbf.write("garbage", 7), 7);
    }
    QVERIFY(testDB->Initialize());
    QVERIFY(QFile(testDB->GetDatabase().databaseName()).exists());

    delete testDB;
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
