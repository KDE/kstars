/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "skyobjects/satellitegroup.h"
#include "skyobjects/satellite.h"
#include "kspaths.h"

#include "../testhelpers.h"

#include <QDir>
#include <QFile>
#include <QStandardPaths>

namespace
{
const QByteArray kIssTLE("ISS (ZARYA)             \n"
                         "1 25544U 98067A   26318.94812294  .00009771  00000+0  18276-3 0  9999\n"
                         "2 25544  51.6323 274.0377 0007413  26.4902 333.6464 15.49382049595126\n");
}

class TestSatelliteGroup : public QObject
{
        Q_OBJECT

    private slots:
        void initTestCase();
        void cleanup();

        void containsTLE_data();
        void containsTLE();

        void saveValidTLE();
        void rejectedReplyKeepsSatellites_data();
        void rejectedReplyKeepsSatellites();

    private:
        QString localFilePath() const;

        const QString m_fileName { QStringLiteral("test_satellitegroup.tle") };
};

void TestSatelliteGroup::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    QVERIFY(QDir().mkpath(KSPaths::writableLocation(QStandardPaths::AppLocalDataLocation)));
}

void TestSatelliteGroup::cleanup()
{
    QFile::remove(localFilePath());
}

QString TestSatelliteGroup::localFilePath() const
{
    return QDir(KSPaths::writableLocation(QStandardPaths::AppLocalDataLocation)).filePath(m_fileName);
}

void TestSatelliteGroup::containsTLE_data()
{
    QTest::addColumn<QByteArray>("data");
    QTest::addColumn<bool>("expected");

    QTest::newRow("valid") << kIssTLE << true;
    QTest::newRow("valid-crlf") << QByteArray(kIssTLE).replace("\n", "\r\n") << true;
    QTest::newRow("empty") << QByteArray() << false;
    // CelesTrak replies to TLE requests for groups whose objects all have six-digit catalog numbers
    QTest::newRow("no-gp-data") << QByteArray("No GP data found") << false;
    // CelesTrak replies to retired group names with HTTP 200 and this text
    QTest::newRow("invalid-query") << QByteArray("Invalid query: \"GROUP=noaa&FORMAT=tle\" (GROUP=noaa not found)") << false;
    QTest::newRow("html") << QByteArray("<!DOCTYPE html>\n<html>\n<head><title>CelesTrak</title></head>\n</html>\n") << false;
    // Two-line elements without a name line are not loaded by readTLE()
    QTest::newRow("no-name-line") << kIssTLE.mid(kIssTLE.indexOf('\n') + 1) << false;
}

void TestSatelliteGroup::containsTLE()
{
    QFETCH(QByteArray, data);
    QFETCH(bool, expected);

    QCOMPARE(SatelliteGroup::containsTLE(data), expected);
}

void TestSatelliteGroup::saveValidTLE()
{
    SatelliteGroup group(QStringLiteral("Test"), m_fileName, QUrl());
    QCOMPARE(group.count(), 0);

    QString error;
    QVERIFY2(group.saveTLE(kIssTLE, error), qPrintable(error));
    QVERIFY(error.isEmpty());
    QCOMPARE(group.count(), 1);
    QCOMPARE(group.first()->name(), QStringLiteral("ISS (ZARYA)"));

    QFile file(localFilePath());
    QVERIFY(file.open(QFile::ReadOnly));
    QCOMPARE(file.readAll(), kIssTLE);
}

void TestSatelliteGroup::rejectedReplyKeepsSatellites_data()
{
    QTest::addColumn<QByteArray>("reply");

    QTest::newRow("empty") << QByteArray();
    QTest::newRow("no-gp-data") << QByteArray("No GP data found");
    QTest::newRow("invalid-query") << QByteArray("Invalid query: \"GROUP=noaa&FORMAT=tle\" (GROUP=noaa not found)");
}

void TestSatelliteGroup::rejectedReplyKeepsSatellites()
{
    QFETCH(QByteArray, reply);

    SatelliteGroup group(QStringLiteral("Test"), m_fileName, QUrl());
    QString error;
    QVERIFY2(group.saveTLE(kIssTLE, error), qPrintable(error));
    QCOMPARE(group.count(), 1);

    // A reply without TLE data must neither overwrite the local file nor drop the loaded satellites
    QVERIFY(!group.saveTLE(reply, error));
    QVERIFY(!error.isEmpty());
    QCOMPARE(group.count(), 1);

    QFile file(localFilePath());
    QVERIFY(file.open(QFile::ReadOnly));
    QCOMPARE(file.readAll(), kIssTLE);
}

QTEST_GUILESS_MAIN(TestSatelliteGroup)

#include "test_satellitegroup.moc"
