/*  KStars class tests
    SPDX-FileCopyrightText: 2020 Eric Dejouhanet <eric.dejouhanet@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "testbinhelper.h"

#include "auxiliary/binfilehelper.h"

#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTemporaryDir>

TestBinHelper::TestBinHelper(QObject *parent) : QObject(parent)
{
}

void TestBinHelper::initTestCase()
{
}

void TestBinHelper::cleanupTestCase()
{
}

void TestBinHelper::init()
{
}

void TestBinHelper::cleanup()
{
}

void TestBinHelper::testLoadBinary_data()
{
}

void TestBinHelper::testLoadBinary()
{
    QSKIP("Not implemented yet.");
}

void TestBinHelper::testOpenFileAtNonAsciiPath_data()
{
    QTest::addColumn<QString>("homeName");

    QTest::newRow("ascii") << QString("home");
    QTest::newRow("accented") << QString::fromUtf8("Jos\u00e9");
    QTest::newRow("non-latin1") << QString::fromUtf8("\u0410\u043d\u043d\u0430");
}

void TestBinHelper::testOpenFileAtNonAsciiPath()
{
    QFETCH(QString, homeName);

    // The catalogs are looked up in the user's data folder, which is under the home directory.
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());
    const QString home = QDir(tmpDir.path()).filePath(homeName);
    QVERIFY(QDir().mkpath(home));

    const QByteArray oldHome = qgetenv("HOME");
    qputenv("HOME", QFile::encodeName(home));
    QStandardPaths::setTestModeEnabled(true);

    const QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    const bool underHome = dataDir.startsWith(home);
    const QString fileName("testbinhelper.dat");
    QFile dataFile(QDir(dataDir).filePath(fileName));
    const bool created = QDir().mkpath(dataDir) && dataFile.open(QIODevice::WriteOnly) && dataFile.write("KStars") == 6;
    dataFile.close();

    const bool exists = BinFileHelper::testFileExists(fileName);
    BinFileHelper helper;
    FILE *handle = helper.openFile(fileName);
    const bool opened = handle != nullptr;
    if (opened)
        helper.closeFile();

    qputenv("HOME", oldHome);

    QVERIFY2(underHome, qPrintable(dataDir));
    QVERIFY(created);
    QVERIFY(exists);
    QVERIFY(opened);
}

QTEST_GUILESS_MAIN(TestBinHelper)
