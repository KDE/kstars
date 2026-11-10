/*
    Tests for the helpers in capturetypes.h.

    SPDX-FileCopyrightText: 2026 J.D. de Keijzer <jdk.ssvw+kde@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "ekos/capture/capturetypes.h"

#include <QLocale>
#include <QTest>

class TestCaptureTypes : public QObject
{
        Q_OBJECT

    private Q_SLOTS:
        void initTestCase();

        /** @brief Exposures in the log keep two significant digits below a millisecond. */
        void testExposureString_data();
        void testExposureString();
};

void TestCaptureTypes::initTestCase()
{
    // %L1 follows the default locale, so fix it for the expected strings.
    QLocale::setDefault(QLocale::c());
}

void TestCaptureTypes::testExposureString_data()
{
    QTest::addColumn<double>("seconds");
    QTest::addColumn<QString>("expected");

    // a millisecond or longer: three decimals, as before
    QTest::newRow("30 s") << 30.0 << "30.000";
    QTest::newRow("1 s") << 1.0 << "1.000";
    QTest::newRow("12 ms") << 0.012 << "0.012";
    QTest::newRow("1 ms") << 0.001 << "0.001";
    // shorter: two significant digits instead of 0.000
    QTest::newRow("0.9 ms") << 0.0009 << "0.00090";
    QTest::newRow("0.25 ms") << 0.00025 << "0.00025";
    QTest::newRow("0.1 ms") << 0.0001 << "0.00010";
    QTest::newRow("32 us") << 0.000032 << "0.000032";
    QTest::newRow("1 us") << 0.000001 << "0.000001";
    // no exposure at all
    QTest::newRow("0") << 0.0 << "0.000";
}

void TestCaptureTypes::testExposureString()
{
    QFETCH(double, seconds);
    QFETCH(QString, expected);

    QCOMPARE(Ekos::exposureString(seconds), expected);
}

QTEST_GUILESS_MAIN(TestCaptureTypes)

#include "test_capturetypes.moc"
