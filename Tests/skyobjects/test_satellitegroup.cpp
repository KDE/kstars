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
const QByteArray kIssOMM(R"j([{"OBJECT_NAME":"ISS (ZARYA)","OBJECT_ID":"1998-067A","EPOCH":"2026-11-14T22:45:17.822016",)j"
R"j("MEAN_MOTION":15.49382049,"ECCENTRICITY":0.0007413,"INCLINATION":51.6323,)j"
                         R"j("RA_OF_ASC_NODE":274.0377,"ARG_OF_PERICENTER":26.4902,"MEAN_ANOMALY":333.6464,)j"
                         R"j("EPHEMERIS_TYPE":0,"CLASSIFICATION_TYPE":"U","NORAD_CAT_ID":25544,"ELEMENT_SET_NO":999,)j"
                         R"j("REV_AT_EPOCH":59512,"BSTAR":0.00018276,"MEAN_MOTION_DOT":9.771e-5,"MEAN_MOTION_DDOT":0},)j"
                         R"j({"OBJECT_NAME":"PULSAR-01","OBJECT_ID":"2026-237A","EPOCH":"2026-11-14T09:49:02.142048",)j"
                         R"j("MEAN_MOTION":14.92874201,"ECCENTRICITY":0.00069895,"INCLINATION":52.9878,)j"
                         R"j("RA_OF_ASC_NODE":213.0367,"ARG_OF_PERICENTER":328.5476,"MEAN_ANOMALY":31.5087,)j"
                         R"j("EPHEMERIS_TYPE":0,"CLASSIFICATION_TYPE":"U","NORAD_CAT_ID":101111,"ELEMENT_SET_NO":999,)j"
                         R"j("REV_AT_EPOCH":373,"BSTAR":0.00018667614,"MEAN_MOTION_DOT":1.705e-05,"MEAN_MOTION_DDOT":0}])j");

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

        void containsElements_data();
        void containsElements();

        void saveValidTLE();
        void saveOMM();
        void parseSkipsInvalidRecords();
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

void TestSatelliteGroup::containsElements_data()
{
    QTest::addColumn<QByteArray>("data");
    QTest::addColumn<bool>("expected");

    QTest::newRow("valid") << kIssTLE << true;
    QTest::newRow("omm-array") << kIssOMM << true;
    QTest::newRow("omm-object") << kIssOMM.mid(1, kIssOMM.indexOf("},{")) << true;
    QTest::newRow("omm-leading-whitespace") << QByteArray("\n  ") + kIssOMM << true;
    QTest::newRow("omm-empty-array") << QByteArray("[]") << false;
    QTest::newRow("omm-broken-json") << kIssOMM.left(200) << false;
    QTest::newRow("omm-no-valid-record") << QByteArray(R"j([{"OBJECT_NAME":"X","NORAD_CAT_ID":1}])j") << false;
    QTest::newRow("valid-crlf") << QByteArray(kIssTLE).replace("\n", "\r\n") << true;
    QTest::newRow("empty") << QByteArray() << false;
    // CelesTrak replies to TLE requests for groups whose objects all have six-digit catalog numbers
    QTest::newRow("no-gp-data") << QByteArray("No GP data found") << false;
    // CelesTrak replies to retired group names with HTTP 200 and this text
    QTest::newRow("invalid-query") << QByteArray("Invalid query: \"GROUP=noaa&FORMAT=tle\" (GROUP=noaa not found)") << false;
    QTest::newRow("html") << QByteArray("<!DOCTYPE html>\n<html>\n<head><title>CelesTrak</title></head>\n</html>\n") << false;
    // Two-line elements without a name line are not loaded
    QTest::newRow("no-name-line") << kIssTLE.mid(kIssTLE.indexOf('\n') + 1) << false;
}

void TestSatelliteGroup::containsElements()
{
    QFETCH(QByteArray, data);
    QFETCH(bool, expected);

    QCOMPARE(SatelliteGroup::containsElements(data), expected);
}

void TestSatelliteGroup::saveValidTLE()
{
    SatelliteGroup group(QStringLiteral("Test"), m_fileName, QUrl());
    QCOMPARE(group.count(), 0);

    QString error;
    QVERIFY2(group.saveElements(kIssTLE, error), qPrintable(error));
    QVERIFY(error.isEmpty());
    QCOMPARE(group.count(), 1);
    QCOMPARE(group.first()->name(), QStringLiteral("ISS (ZARYA)"));

    QFile file(localFilePath());
    QVERIFY(file.open(QFile::ReadOnly));
    QCOMPARE(file.readAll(), kIssTLE);
}

void TestSatelliteGroup::saveOMM()
{
    SatelliteGroup group(QStringLiteral("Test"), m_fileName, QUrl());
    QString error;
    QVERIFY2(group.saveElements(kIssOMM, error), qPrintable(error));
    QCOMPARE(group.count(), 2);
    QCOMPARE(group.at(0)->name(), QStringLiteral("ISS (ZARYA)"));
    // Six-digit catalog numbers are only available in OMM
    QCOMPARE(group.at(1)->name(), QStringLiteral("PULSAR-01"));

    // Reloading from the local file gives the same satellites
    group.readElements();
    QCOMPARE(group.count(), 2);

    // A local file in TLE format, as written by older versions, still loads
    QVERIFY2(group.saveElements(kIssTLE, error), qPrintable(error));
    QCOMPARE(group.count(), 1);
}

void TestSatelliteGroup::parseSkipsInvalidRecords()
{
    QByteArray data = kIssOMM;
    data.insert(1, R"j({"OBJECT_NAME":"BROKEN","NORAD_CAT_ID":1},)j");
    const QList<Satellite *> satellites = SatelliteGroup::parseElements(data);
    QCOMPARE(satellites.size(), 2);
    QCOMPARE(satellites.at(0)->name(), QStringLiteral("ISS (ZARYA)"));
    qDeleteAll(satellites);
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
    QVERIFY2(group.saveElements(kIssTLE, error), qPrintable(error));
    QCOMPARE(group.count(), 1);

    // A reply without TLE data must neither overwrite the local file nor drop the loaded satellites
    QVERIFY(!group.saveElements(reply, error));
    QVERIFY(!error.isEmpty());
    QCOMPARE(group.count(), 1);

    QFile file(localFilePath());
    QVERIFY(file.open(QFile::ReadOnly));
    QCOMPARE(file.readAll(), kIssTLE);
}

QTEST_GUILESS_MAIN(TestSatelliteGroup)

#include "test_satellitegroup.moc"
