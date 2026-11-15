/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "skyobjects/satellite.h"

#include "../testhelpers.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QScopedPointer>
#include <QStandardPaths>

#include <algorithm>
#include <cmath>

class TestSatellite : public QObject
{
        Q_OBJECT

    private slots:
        void initTestCase();

        void fromOMMMatchesTLE_data();
        void fromOMMMatchesTLE();

        void alpha5CatalogNumber();
        void catalogNumberBeyondAlpha5();
        void negativeAndExponentFields();
        void numericStrings();
        void invalidRecords_data();
        void invalidRecords();

    private:
        static QJsonObject json(const QByteArray &text);
        static bool near(double a, double b);
        static int checksum(const QString &line);
        static void verifyTLELines(const QString &tle);
        static void compareElements(const Satellite *a, const Satellite *b, double epochTolerance);
};

namespace
{
// Pairs of OMM JSON and TLE as served by CelesTrak for the same element sets
const QByteArray kIssOMM(R"j({"OBJECT_NAME":"ISS (ZARYA)","OBJECT_ID":"1998-067A","EPOCH":"2026-11-14T22:45:17.822016",)j"
                         R"j("MEAN_MOTION":15.49382049,"ECCENTRICITY":0.0007413,"INCLINATION":51.6323,)j"
                         R"j("RA_OF_ASC_NODE":274.0377,"ARG_OF_PERICENTER":26.4902,"MEAN_ANOMALY":333.6464,)j"
                         R"j("EPHEMERIS_TYPE":0,"CLASSIFICATION_TYPE":"U","NORAD_CAT_ID":25544,"ELEMENT_SET_NO":999,)j"
                         R"j("REV_AT_EPOCH":59512,"BSTAR":0.00018276,"MEAN_MOTION_DOT":9.771e-5,"MEAN_MOTION_DDOT":0})j");
const QString kIssLine1("1 25544U 98067A   26318.94812294  .00009771  00000+0  18276-3 0  9999");
const QString kIssLine2("2 25544  51.6323 274.0377 0007413  26.4902 333.6464 15.49382049595126");

// Deep-space orbit with a negative first derivative
const QByteArray kTdrsOMM(R"j({"OBJECT_NAME":"TDRS 3","OBJECT_ID":"1988-091B","EPOCH":"2026-11-15T01:14:44.443392",)j"
                          R"j("MEAN_MOTION":1.00270438,"ECCENTRICITY":0.0041684,"INCLINATION":12.4902,)j"
                          R"j("RA_OF_ASC_NODE":339.9998,"ARG_OF_PERICENTER":352.0848,"MEAN_ANOMALY":51.9919,)j"
                          R"j("EPHEMERIS_TYPE":0,"CLASSIFICATION_TYPE":"U","NORAD_CAT_ID":19548,"ELEMENT_SET_NO":999,)j"
                          R"j("REV_AT_EPOCH":12690,"BSTAR":0,"MEAN_MOTION_DOT":-2.89e-06,"MEAN_MOTION_DDOT":0})j");
const QString kTdrsLine1("1 19548U 88091B   26319.05190328 -.00000289  00000+0  00000+0 0  9990");
const QString kTdrsLine2("2 19548  12.4902 339.9998 0041684 352.0848  51.9919  1.00270438126907");

const QByteArray kGoesOMM(R"j({"OBJECT_NAME":"GOES 16","OBJECT_ID":"2016-071A","EPOCH":"2026-11-15T04:03:16.791840",)j"
                          R"j("MEAN_MOTION":1.00271555,"ECCENTRICITY":0.0002435,"INCLINATION":0.712,)j"
                          R"j("RA_OF_ASC_NODE":83.5074,"ARG_OF_PERICENTER":169.3008,"MEAN_ANOMALY":117.6027,)j"
                          R"j("EPHEMERIS_TYPE":0,"CLASSIFICATION_TYPE":"U","NORAD_CAT_ID":41866,"ELEMENT_SET_NO":999,)j"
                          R"j("REV_AT_EPOCH":3661,"BSTAR":0,"MEAN_MOTION_DOT":-7.8e-07,"MEAN_MOTION_DDOT":0})j");
const QString kGoesLine1("1 41866U 16071A   26319.16894435 -.00000078  00000+0  00000+0 0  9995");
const QString kGoesLine2("2 41866   0.7120  83.5074 0002435 169.3008 117.6027  1.00271555 36611");

// Launched in 2026: six-digit catalog number, served by CelesTrak only in OMM formats
const QByteArray kPulsarOMM(R"j({"OBJECT_NAME":"PULSAR-01","OBJECT_ID":"2026-237A","EPOCH":"2026-11-14T09:49:02.142048",)j"
                            R"j("MEAN_MOTION":14.92874201,"ECCENTRICITY":0.00069895,"INCLINATION":52.9878,)j"
                            R"j("RA_OF_ASC_NODE":213.0367,"ARG_OF_PERICENTER":328.5476,"MEAN_ANOMALY":31.5087,)j"
                            R"j("EPHEMERIS_TYPE":0,"CLASSIFICATION_TYPE":"U","NORAD_CAT_ID":101111,"ELEMENT_SET_NO":999,)j"
                            R"j("REV_AT_EPOCH":373,"BSTAR":0.00018667614,"MEAN_MOTION_DOT":1.705e-05,"MEAN_MOTION_DDOT":0})j");
}

void TestSatellite::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
}

QJsonObject TestSatellite::json(const QByteArray &text)
{
    return QJsonDocument::fromJson(text).object();
}

// Equal up to rounding of the last digits
bool TestSatellite::near(double a, double b)
{
    return std::fabs(a - b) <= 1e-12 * std::max({1e-3, std::fabs(a), std::fabs(b)});
}

// Independent implementation of the TLE checksum
int TestSatellite::checksum(const QString &line)
{
    int sum = 0;
    for (const QChar c : line.left(68))
        sum += c.isDigit() ? c.digitValue() : (c == '-' ? 1 : 0);
    return sum % 10;
}

void TestSatellite::verifyTLELines(const QString &tle)
{
    const QStringList lines = tle.split('\n');
    QCOMPARE(lines.size(), 3);
    QCOMPARE(lines.at(1).size(), 69);
    QCOMPARE(lines.at(2).size(), 69);
    QCOMPARE(lines.at(1).at(68).digitValue(), checksum(lines.at(1)));
    QCOMPARE(lines.at(2).at(68).digitValue(), checksum(lines.at(2)));
}

void TestSatellite::compareElements(const Satellite *a, const Satellite *b, double epochTolerance)
{
    QCOMPARE(a->m_number, b->m_number);
    QCOMPARE(a->m_class, b->m_class);
    QCOMPARE(a->m_id.trimmed(), b->m_id.trimmed());
    QCOMPARE(a->m_epoch_year, b->m_epoch_year);
    QVERIFY2(std::fabs(a->m_tle_jd - b->m_tle_jd) < epochTolerance,
             qPrintable(QString("epoch %1 vs %2").arg(a->m_tle_jd, 0, 'f', 9).arg(b->m_tle_jd, 0, 'f', 9)));
    QVERIFY(std::fabs(a->m_epoch - b->m_epoch) < epochTolerance);
    QVERIFY(near(a->m_first_deriv, b->m_first_deriv));
    QVERIFY(near(a->m_second_deriv, b->m_second_deriv));
    QVERIFY(near(a->m_bstar, b->m_bstar));
    QCOMPARE(a->m_ephem_type, b->m_ephem_type);
    QCOMPARE(a->m_elem_number, b->m_elem_number);
    QVERIFY(near(a->m_inclination, b->m_inclination));
    QVERIFY(near(a->m_ra, b->m_ra));
    QVERIFY(near(a->m_eccentricity, b->m_eccentricity));
    QVERIFY(near(a->m_arg_perigee, b->m_arg_perigee));
    QVERIFY(near(a->m_mean_anomaly, b->m_mean_anomaly));
    QVERIFY(near(a->m_mean_motion, b->m_mean_motion));
    QCOMPARE(a->m_nb_revolution, b->m_nb_revolution);

    // Same SGP4 initialisation, including the near-earth / deep-space choice
    QCOMPARE(a->method, b->method);
    QCOMPARE(a->irez, b->irez);
    // Sidereal time at epoch follows the epoch, which may differ within epochTolerance
    QVERIFY(std::fabs(a->gsto - b->gsto) < epochTolerance * 2 * M_PI * 1.1);
    QVERIFY(near(a->cc1, b->cc1));
}

void TestSatellite::fromOMMMatchesTLE_data()
{
    QTest::addColumn<QByteArray>("omm");
    QTest::addColumn<QString>("name");
    QTest::addColumn<QString>("line1");
    QTest::addColumn<QString>("line2");

    QTest::newRow("iss") << kIssOMM << "ISS (ZARYA)" << kIssLine1 << kIssLine2;
    QTest::newRow("tdrs3-deep-space") << kTdrsOMM << "TDRS 3" << kTdrsLine1 << kTdrsLine2;
    QTest::newRow("goes16-deep-space") << kGoesOMM << "GOES 16" << kGoesLine1 << kGoesLine2;
}

void TestSatellite::fromOMMMatchesTLE()
{
    QFETCH(QByteArray, omm);
    QFETCH(QString, name);
    QFETCH(QString, line1);
    QFETCH(QString, line2);

    QScopedPointer<Satellite> fromOMM(Satellite::fromOMM(json(omm)));
    QVERIFY(fromOMM);
    QScopedPointer<Satellite> fromTLE(new Satellite(name, line1, line2));

    // The generated TLE is the one CelesTrak serves for the same element set
    QCOMPARE(fromOMM->tle(), name + "\n" + line1 + "\n" + line2);
    QCOMPARE(fromOMM->name(), name);
    QCOMPARE(fromOMM->id().trimmed(), fromTLE->id().trimmed());

    // OMM epochs have microsecond resolution, TLE epochs 1e-8 day
    compareElements(fromOMM.data(), fromTLE.data(), 1e-8);
}

void TestSatellite::alpha5CatalogNumber()
{
    QScopedPointer<Satellite> sat(Satellite::fromOMM(json(kPulsarOMM)));
    QVERIFY(sat);
    QCOMPARE(sat->m_number, 101111);
    QCOMPARE(sat->name(), QString("PULSAR-01"));
    QCOMPARE(sat->id(), QString("26237A"));

    // OMM keeps more digits than TLE: the elements come from OMM, not from the generated TLE
    QVERIFY(near(sat->m_bstar, 0.00018667614));
    QVERIFY(near(sat->m_eccentricity, 0.00069895));

    // Expected lines computed independently from the TLE specification, with an Alpha-5 catalog number.
    // Like CelesTrak, the eccentricity is truncated to 7 digits.
    QCOMPARE(sat->tle(), QString("PULSAR-01\n"
                                 "1 A1111U 26237A   26318.40905257  .00001705  00000+0  18668-3 0  9990\n"
                                 "2 A1111  52.9878 213.0367 0006989 328.5476  31.5087 14.92874201  3739"));
    verifyTLELines(sat->tle());

    // The Alpha-5 TLE reads back to the same satellite, within TLE resolution
    const QStringList lines = sat->tle().split('\n');
    QScopedPointer<Satellite> fromTLE(new Satellite(lines.at(0), lines.at(1), lines.at(2)));
    QCOMPARE(fromTLE->m_number, 101111);
    QVERIFY(std::fabs(fromTLE->m_tle_jd - sat->m_tle_jd) < 1e-8);
    QVERIFY(std::fabs(fromTLE->m_bstar - sat->m_bstar) < 1e-8);
    QVERIFY(std::fabs(fromTLE->m_eccentricity - sat->m_eccentricity) < 1e-7);
    QVERIFY(near(fromTLE->m_mean_motion, sat->m_mean_motion));
    QCOMPARE(fromTLE->method, sat->method);
}

void TestSatellite::catalogNumberBeyondAlpha5()
{
    QJsonObject omm = json(kIssOMM);
    omm["NORAD_CAT_ID"] = 340000;

    QScopedPointer<Satellite> sat(Satellite::fromOMM(omm));
    QVERIFY(sat);
    QCOMPARE(sat->m_number, 340000);
    // No TLE can hold this number, but the satellite is still usable
    QVERIFY(sat->tle().isEmpty());
    QVERIFY(sat->m_mean_motion > 0);

    // Largest Alpha-5 number
    omm["NORAD_CAT_ID"] = 339999;
    sat.reset(Satellite::fromOMM(omm));
    QVERIFY(sat);
    QVERIFY(sat->tle().split('\n').value(1).startsWith("1 Z9999U "));
    verifyTLELines(sat->tle());
}

void TestSatellite::negativeAndExponentFields()
{
    QJsonObject omm = json(kIssOMM);
    omm["BSTAR"] = -1.2345e-5;
    omm["MEAN_MOTION_DDOT"] = 6.1234e-9;

    QScopedPointer<Satellite> sat(Satellite::fromOMM(omm));
    QVERIFY(sat);
    QCOMPARE(sat->tle(), QString("ISS (ZARYA)\n"
                                 "1 25544U 98067A   26318.94812294  .00009771  61234-8 -12345-4 0  9997\n" +
                                 kIssLine2));
    verifyTLELines(sat->tle());

    const QStringList lines = sat->tle().split('\n');
    QScopedPointer<Satellite> fromTLE(new Satellite(lines.at(0), lines.at(1), lines.at(2)));
    compareElements(sat.data(), fromTLE.data(), 1e-8);
}

void TestSatellite::numericStrings()
{
    // Space-Track serves every OMM value as a string
    QJsonObject omm = json(kIssOMM);
    for (auto it = omm.begin(); it != omm.end(); ++it)
    {
        if (it.value().isDouble())
            it.value() = QString::number(it.value().toDouble(), 'g', 17);
    }
    QVERIFY(omm.value("MEAN_MOTION").isString());

    QScopedPointer<Satellite> fromStrings(Satellite::fromOMM(omm));
    QScopedPointer<Satellite> fromNumbers(Satellite::fromOMM(json(kIssOMM)));
    QVERIFY(fromStrings);
    QVERIFY(fromNumbers);
    QCOMPARE(fromStrings->tle(), fromNumbers->tle());
    compareElements(fromStrings.data(), fromNumbers.data(), 1e-12);
}

void TestSatellite::invalidRecords_data()
{
    QTest::addColumn<QString>("key");
    QTest::addColumn<QJsonValue>("value");

    QTest::newRow("no-epoch") << "EPOCH" << QJsonValue(QJsonValue::Undefined);
    QTest::newRow("bad-epoch") << "EPOCH" << QJsonValue("2026-11-14");
    QTest::newRow("bad-epoch-time") << "EPOCH" << QJsonValue("2026-11-14T25:00:00");
    QTest::newRow("no-mean-motion") << "MEAN_MOTION" << QJsonValue(QJsonValue::Undefined);
    QTest::newRow("zero-mean-motion") << "MEAN_MOTION" << QJsonValue(0);
    QTest::newRow("hyperbolic") << "ECCENTRICITY" << QJsonValue(1.2);
    QTest::newRow("no-catalog-number") << "NORAD_CAT_ID" << QJsonValue(QJsonValue::Undefined);
    QTest::newRow("zero-catalog-number") << "NORAD_CAT_ID" << QJsonValue(0);
    QTest::newRow("text-inclination") << "INCLINATION" << QJsonValue("high");
    QTest::newRow("text-bstar") << "BSTAR" << QJsonValue("n/a");
}

void TestSatellite::invalidRecords()
{
    QFETCH(QString, key);
    QFETCH(QJsonValue, value);

    QJsonObject omm = json(kIssOMM);
    if (value.isUndefined())
        omm.remove(key);
    else
        omm[key] = value;

    QScopedPointer<Satellite> sat(Satellite::fromOMM(omm));
    QVERIFY(sat.isNull());
}

QTEST_GUILESS_MAIN(TestSatellite)

#include "test_satellite.moc"
