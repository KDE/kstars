/*  KStars class tests
    Copyright (C) 2020
    Eric Dejouhanet <eric.dejouhanet@gmail.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include "ksuserdb.h"
#include "kspaths.h"

#include "testksuserdb.h"

TestKSUserDB::TestKSUserDB(QObject *parent) : QObject(parent)
{
}

void TestKSUserDB::initTestCase()
{
    // Ensure we are in test mode (user .qttest)
    QStandardPaths::setTestModeEnabled(true);
    QVERIFY(QStandardPaths::isTestModeEnabled());

    // Remove the user folder that may eventually exist
    QWARN(qPrintable("Removing " + KSPaths::writableLocation(QStandardPaths::GenericDataLocation)));
    QVERIFY(QDir(KSPaths::writableLocation(QStandardPaths::GenericDataLocation)).removeRecursively());
    QVERIFY(!QDir(KSPaths::writableLocation(QStandardPaths::GenericDataLocation)).exists());
}

void TestKSUserDB::cleanupTestCase()
{
    // Ensure we are in test mode (user .qttest)
    QVERIFY(QStandardPaths::isTestModeEnabled());

    // Remove the user folder that may eventually exist
    QDir(KSPaths::writableLocation(QStandardPaths::GenericDataLocation)).removeRecursively();
    QVERIFY(!QDir(KSPaths::writableLocation(QStandardPaths::GenericDataLocation)).exists());
}

void TestKSUserDB::init()
{
}

void TestKSUserDB::cleanup()
{
}

void TestKSUserDB::testInitializeDB()
{
    KSUserDB * testDB = new KSUserDB();
    QVERIFY(nullptr != testDB);

    // If the KStars folder does not exist, database cannot be created and the app cannot start
    QVERIFY(!testDB->Initialize());

    // So create the KStars folder and retry
    QVERIFY(QDir().mkpath(KSPaths::writableLocation(QStandardPaths::GenericDataLocation)));
    QVERIFY(testDB->Initialize());

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
