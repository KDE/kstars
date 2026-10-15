/*
    SPDX-FileCopyrightText: 2026 KStars Team

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "ekos/capture/capturehistorynavigation.h"

#include <QTest>

class TestCaptureHistoryNavigation : public QObject
{
        Q_OBJECT

    private Q_SLOTS:
        void testRemoveRunWithoutFrames();
        void testRemoveRunWithoutRuns();
        void testRemoveRunZeroWithFrames();
        void testRemoveRunKeepsLaterRuns();

    private:
        static CaptureHistory::FrameData frame(const QString &filename, double hfr);
};

CaptureHistory::FrameData TestCaptureHistoryNavigation::frame(const QString &filename, double hfr)
{
    CaptureHistory::FrameData data {};
    data.filename = filename;
    data.target = "target";
    data.hfr = hfr;
    return data;
}

// An autofocus run is added before its first frame is captured. If it is aborted and
// its data cleared, the run has no entry in the history list yet.
void TestCaptureHistoryNavigation::testRemoveRunWithoutFrames()
{
    CaptureHistoryNavigation navigation;
    navigation.addRun();
    QCOMPARE(navigation.lastRun(), 1);
    QCOMPARE(navigation.currentRun(), 1);

    navigation.removeRun(navigation.currentRun(), false);

    QCOMPARE(navigation.lastRun(), 0);
    QCOMPARE(navigation.currentRun(), 0);
    QCOMPARE(navigation.captureHistory(navigation.currentRun()).size(), 0);
    QVERIFY(navigation.currentFrame().filename.isEmpty());
}

// Clearing the focus data before any autofocus run removes run 0, which must not make the
// run counters negative (Sentry KSTARS-JD).
void TestCaptureHistoryNavigation::testRemoveRunWithoutRuns()
{
    CaptureHistoryNavigation navigation;
    QCOMPARE(navigation.currentRun(), 0);

    navigation.removeRun(navigation.currentRun(), false);

    QCOMPARE(navigation.lastRun(), 0);
    QCOMPARE(navigation.currentRun(), 0);
    QCOMPARE(navigation.captureHistory(navigation.currentRun()).size(), 0);
    QVERIFY(navigation.currentFrame().filename.isEmpty());
}

// Frames captured before the first run was added are stored in run 0. Removing it clears them.
void TestCaptureHistoryNavigation::testRemoveRunZeroWithFrames()
{
    CaptureHistoryNavigation navigation;
    QVERIFY(navigation.addFrame(frame("/nonexistent/loop-1.fits", 1.5)));
    QCOMPARE(navigation.captureHistory(0).size(), 1);

    navigation.removeRun(0, false);

    QCOMPARE(navigation.lastRun(), 0);
    QCOMPARE(navigation.currentRun(), 0);
    QCOMPARE(navigation.captureHistory(0).size(), 0);
    QVERIFY(navigation.currentFrame().filename.isEmpty());
}

void TestCaptureHistoryNavigation::testRemoveRunKeepsLaterRuns()
{
    CaptureHistoryNavigation navigation;
    navigation.addRun();
    QVERIFY(navigation.addFrame(frame("/nonexistent/run1.fits", 1.0)));
    navigation.addRun();
    QVERIFY(navigation.addFrame(frame("/nonexistent/run2.fits", 2.0)));
    QCOMPARE(navigation.lastRun(), 2);
    QCOMPARE(navigation.currentRun(), 2);

    navigation.removeRun(1, false);

    // run 2 moves down to run 1 and stays selected
    QCOMPARE(navigation.lastRun(), 1);
    QCOMPARE(navigation.currentRun(), 1);
    QCOMPARE(navigation.currentFrame().filename, QString("/nonexistent/run2.fits"));
    QCOMPARE(navigation.currentFrame().hfr, 2.0);
}

QTEST_MAIN(TestCaptureHistoryNavigation)

#include "test_capturehistorynavigation.moc"
