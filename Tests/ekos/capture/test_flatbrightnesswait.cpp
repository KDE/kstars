/*
    Tests for waiting on a light box after a brightness change.

    SPDX-FileCopyrightText: 2026 J.D. de Keijzer <jdk.ssvw+kde@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "test_flatbrightnesswait.h"

using Ekos::FlatBrightnessWait;

namespace
{

// Times in milliseconds
constexpr qint64 SETTLE  = 5000;
constexpr qint64 TIMEOUT = 30000;

} // namespace

TestFlatBrightnessWait::TestFlatBrightnessWait() : QObject()
{
}

void TestFlatBrightnessWait::testConfirmedImmediately()
{
    FlatBrightnessWait wait;
    wait.startRequest(0, SETTLE, TIMEOUT);
    QVERIFY(wait.isActive());
    QVERIFY(!wait.isConfirmed());

    QCOMPARE(wait.report(200, 120, IPS_OK), FlatBrightnessWait::RESULT_WAIT);
    QVERIFY(wait.isConfirmed());
    QCOMPARE(wait.deadline(), 200 + SETTLE);

    QCOMPARE(wait.check(200 + SETTLE - 1), FlatBrightnessWait::RESULT_WAIT);
    QCOMPARE(wait.check(200 + SETTLE), FlatBrightnessWait::RESULT_CAPTURE);
    QVERIFY(!wait.isActive());
}

void TestFlatBrightnessWait::testBusyThenConfirmed()
{
    FlatBrightnessWait wait;
    wait.startRequest(0, SETTLE, TIMEOUT);

    QCOMPARE(wait.report(1000, 80, IPS_BUSY), FlatBrightnessWait::RESULT_WAIT);
    QCOMPARE(wait.report(2000, 100, IPS_BUSY), FlatBrightnessWait::RESULT_WAIT);
    QVERIFY(!wait.isConfirmed());
    // The settle time has not started: waiting this long proves nothing yet.
    QCOMPARE(wait.check(SETTLE + 1000), FlatBrightnessWait::RESULT_WAIT);

    QCOMPARE(wait.report(4000, 120, IPS_OK), FlatBrightnessWait::RESULT_WAIT);
    QCOMPARE(wait.check(4000 + SETTLE - 1), FlatBrightnessWait::RESULT_WAIT);
    QCOMPARE(wait.check(4000 + SETTLE), FlatBrightnessWait::RESULT_CAPTURE);
}

void TestFlatBrightnessWait::testConfirmationAfterTheSettleTime()
{
    // The settle time is a minimum after the confirmation, not a timeout: a light
    // box that confirms after 7 s is waited for, and then gets its 5 s.
    FlatBrightnessWait wait;
    wait.startRequest(0, SETTLE, TIMEOUT);

    QCOMPARE(wait.check(SETTLE), FlatBrightnessWait::RESULT_WAIT);
    QCOMPARE(wait.deadline(), TIMEOUT);

    QCOMPARE(wait.report(7000, 120, IPS_OK), FlatBrightnessWait::RESULT_WAIT);
    QCOMPARE(wait.check(7000 + SETTLE - 1), FlatBrightnessWait::RESULT_WAIT);
    QCOMPARE(wait.check(7000 + SETTLE), FlatBrightnessWait::RESULT_CAPTURE);
}

void TestFlatBrightnessWait::testNoConfirmation()
{
    FlatBrightnessWait wait;
    wait.startRequest(0, SETTLE, TIMEOUT);

    QCOMPARE(wait.check(TIMEOUT - 1), FlatBrightnessWait::RESULT_WAIT);
    QCOMPARE(wait.check(TIMEOUT), FlatBrightnessWait::RESULT_FAILED_NO_CONFIRMATION);
    QVERIFY(!wait.isActive());
}

void TestFlatBrightnessWait::testBusyForever()
{
    // A light box that keeps reporting progress must not stall the calibration.
    FlatBrightnessWait wait;
    wait.startRequest(0, SETTLE, TIMEOUT);

    for (qint64 t = 1000; t < TIMEOUT; t += 1000)
        QCOMPARE(wait.report(t, static_cast<int>(t / 1000), IPS_BUSY), FlatBrightnessWait::RESULT_WAIT);

    QCOMPARE(wait.deadline(), TIMEOUT);
    QCOMPARE(wait.check(TIMEOUT), FlatBrightnessWait::RESULT_FAILED_NO_CONFIRMATION);
}

void TestFlatBrightnessWait::testRejected()
{
    FlatBrightnessWait wait;
    wait.startRequest(0, SETTLE, TIMEOUT);

    QCOMPARE(wait.report(500, 120, IPS_BUSY), FlatBrightnessWait::RESULT_WAIT);
    QCOMPARE(wait.report(900, 120, IPS_ALERT), FlatBrightnessWait::RESULT_FAILED_REJECTED);
    QVERIFY(!wait.isActive());
    // Nothing further happens once it failed.
    QCOMPARE(wait.check(TIMEOUT), FlatBrightnessWait::RESULT_WAIT);
}

void TestFlatBrightnessWait::testLongSettleTime()
{
    // A settle time longer than the confirmation timeout only runs after the OK and
    // must not be mistaken for a missing confirmation.
    const qint64 settle = 60000;
    FlatBrightnessWait wait;
    wait.startRequest(0, settle, TIMEOUT);

    QCOMPARE(wait.report(1000, 120, IPS_OK), FlatBrightnessWait::RESULT_WAIT);
    QCOMPARE(wait.check(TIMEOUT + 1), FlatBrightnessWait::RESULT_WAIT);
    QCOMPARE(wait.check(1000 + settle), FlatBrightnessWait::RESULT_CAPTURE);
}

void TestFlatBrightnessWait::testRepeatedReportsAreIgnored()
{
    // Drivers that publish the brightness on every poll must not postpone the frame.
    FlatBrightnessWait wait;
    wait.startRequest(0, SETTLE, TIMEOUT);

    QCOMPARE(wait.report(1000, 120, IPS_OK), FlatBrightnessWait::RESULT_WAIT);
    for (qint64 t = 1500; t < 1000 + SETTLE; t += 500)
        QCOMPARE(wait.report(t, 120, IPS_OK), FlatBrightnessWait::RESULT_WAIT);

    QCOMPARE(wait.deadline(), 1000 + SETTLE);
    QCOMPARE(wait.check(1000 + SETTLE), FlatBrightnessWait::RESULT_CAPTURE);
}

void TestFlatBrightnessWait::testNewValueWhileSettling()
{
    FlatBrightnessWait wait;
    wait.startRequest(0, SETTLE, TIMEOUT);

    QCOMPARE(wait.report(1000, 118, IPS_OK), FlatBrightnessWait::RESULT_WAIT);
    // The light box corrects itself to a slightly different value.
    QCOMPARE(wait.report(3000, 120, IPS_OK), FlatBrightnessWait::RESULT_WAIT);
    QCOMPARE(wait.check(1000 + SETTLE), FlatBrightnessWait::RESULT_WAIT);
    QCOMPARE(wait.check(3000 + SETTLE), FlatBrightnessWait::RESULT_CAPTURE);
}

void TestFlatBrightnessWait::testBusyAgainWhileSettling()
{
    FlatBrightnessWait wait;
    wait.startRequest(0, SETTLE, TIMEOUT);

    QCOMPARE(wait.report(1000, 120, IPS_OK), FlatBrightnessWait::RESULT_WAIT);
    QCOMPARE(wait.report(2000, 130, IPS_BUSY), FlatBrightnessWait::RESULT_WAIT);
    QVERIFY(!wait.isConfirmed());
    QCOMPARE(wait.deadline(), 2000 + TIMEOUT);

    // The old settle time does not count any more.
    QCOMPARE(wait.check(1000 + SETTLE), FlatBrightnessWait::RESULT_WAIT);

    QCOMPARE(wait.report(4000, 140, IPS_OK), FlatBrightnessWait::RESULT_WAIT);
    QCOMPARE(wait.check(4000 + SETTLE), FlatBrightnessWait::RESULT_CAPTURE);
}

void TestFlatBrightnessWait::testOverallDeadline_data()
{
    QTest::addColumn<bool>("alternateBusy");
    QTest::addColumn<bool>("endConfirmed");
    QTest::addColumn<int>("expected");

    // Reports every second, faster than the settle time: either busy and confirmed in
    // turn, or a new confirmed value each time (review by Andreas R.).
    QTest::newRow("busy and confirmed in turn, confirmed at the end") << true << true
            << int(FlatBrightnessWait::RESULT_CAPTURE);
    QTest::newRow("busy and confirmed in turn, busy at the end") << true << false
            << int(FlatBrightnessWait::RESULT_FAILED_NO_CONFIRMATION);
    QTest::newRow("new confirmed values") << false << true << int(FlatBrightnessWait::RESULT_CAPTURE);
}

void TestFlatBrightnessWait::testOverallDeadline()
{
    QFETCH(bool, alternateBusy);
    QFETCH(bool, endConfirmed);
    QFETCH(int, expected);

    FlatBrightnessWait wait;
    wait.startRequest(0, SETTLE, TIMEOUT);
    const qint64 overall = TIMEOUT + SETTLE;

    int value = 100;
    for (qint64 t = 1000; t < overall; t += 1000)
    {
        const bool last = t + 1000 >= overall;
        IPState state = IPS_OK;
        if (alternateBusy)
            state = last ? (endConfirmed ? IPS_OK : IPS_BUSY) : ((t / 1000) % 2 ? IPS_BUSY : IPS_OK);
        QCOMPARE(wait.report(t, ++value, state), FlatBrightnessWait::RESULT_WAIT);
        // the next check is never due after the overall deadline
        QVERIFY(wait.deadline() <= overall);
        QCOMPARE(wait.check(t), FlatBrightnessWait::RESULT_WAIT);
    }

    QCOMPARE(int(wait.check(overall)), expected);
    QVERIFY(!wait.isActive());
}

void TestFlatBrightnessWait::testWithoutRequestSettlesOnly()
{
    // Before the first frame nothing was requested, so an idle light box has nothing
    // to confirm and only gets the settle time.
    FlatBrightnessWait wait;
    wait.startWithoutRequest(0, IPS_OK, SETTLE, TIMEOUT);
    QVERIFY(wait.isConfirmed());

    QCOMPARE(wait.check(SETTLE - 1), FlatBrightnessWait::RESULT_WAIT);
    QCOMPARE(wait.check(SETTLE), FlatBrightnessWait::RESULT_CAPTURE);
}

void TestFlatBrightnessWait::testWithoutRequestWhileBusy()
{
    // The light box is still changing, e.g. to a per-filter preset.
    FlatBrightnessWait wait;
    wait.startWithoutRequest(0, IPS_BUSY, SETTLE, TIMEOUT);
    QVERIFY(!wait.isConfirmed());
    QCOMPARE(wait.check(SETTLE), FlatBrightnessWait::RESULT_WAIT);

    QCOMPARE(wait.report(2000, 100, IPS_OK), FlatBrightnessWait::RESULT_WAIT);
    QCOMPARE(wait.check(2000 + SETTLE), FlatBrightnessWait::RESULT_CAPTURE);

    // And one that never finishes stops the calibration.
    FlatBrightnessWait stuck;
    stuck.startWithoutRequest(0, IPS_BUSY, SETTLE, TIMEOUT);
    QCOMPARE(stuck.check(TIMEOUT), FlatBrightnessWait::RESULT_FAILED_NO_CONFIRMATION);
}

void TestFlatBrightnessWait::testWithoutRequestPresetApplied()
{
    // The preset arrives while the settle time before the first frame is running:
    // the light gets its full settle time from the preset on.
    FlatBrightnessWait wait;
    wait.startWithoutRequest(0, IPS_OK, SETTLE, TIMEOUT);

    QCOMPARE(wait.report(1500, 100, IPS_OK), FlatBrightnessWait::RESULT_WAIT);
    QCOMPARE(wait.check(SETTLE), FlatBrightnessWait::RESULT_WAIT);
    QCOMPARE(wait.check(1500 + SETTLE), FlatBrightnessWait::RESULT_CAPTURE);
}

void TestFlatBrightnessWait::testInactive()
{
    FlatBrightnessWait wait;
    QVERIFY(!wait.isActive());
    QCOMPARE(wait.report(0, 120, IPS_ALERT), FlatBrightnessWait::RESULT_WAIT);
    QCOMPARE(wait.check(TIMEOUT), FlatBrightnessWait::RESULT_WAIT);

    wait.startRequest(0, SETTLE, TIMEOUT);
    wait.reset();
    QVERIFY(!wait.isActive());
    QCOMPARE(wait.check(TIMEOUT), FlatBrightnessWait::RESULT_WAIT);
}

void TestFlatBrightnessWait::testDefaultSettleTime()
{
    // Ekos uses the default settle time for every light box. Some drivers confirm a
    // brightness before the light has settled, which is what the time after the
    // confirmation is for.
    QCOMPARE(FlatBrightnessWait::DEFAULT_SETTLE_TIME, qint64(1000));

    FlatBrightnessWait wait;
    wait.startRequest(0, FlatBrightnessWait::DEFAULT_SETTLE_TIME);

    QCOMPARE(wait.check(FlatBrightnessWait::DEFAULT_SETTLE_TIME), FlatBrightnessWait::RESULT_WAIT);
    QCOMPARE(wait.report(3000, 120, IPS_OK), FlatBrightnessWait::RESULT_WAIT);
    QCOMPARE(wait.check(3000 + FlatBrightnessWait::DEFAULT_SETTLE_TIME - 1), FlatBrightnessWait::RESULT_WAIT);
    QCOMPARE(wait.check(3000 + FlatBrightnessWait::DEFAULT_SETTLE_TIME), FlatBrightnessWait::RESULT_CAPTURE);
}

QTEST_GUILESS_MAIN(TestFlatBrightnessWait)
