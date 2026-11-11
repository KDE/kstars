/*
    KStars UI tests for the slews of the alignment
    SPDX-FileCopyrightText: 2026 Andreas R. <temp@ruthner.net>
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "test_ekos_align_slew.h"

#if defined(HAVE_INDI)

#include "kstars_ui_tests.h"
#include "test_ekos.h"
#include "Options.h"
#include "indi/guimanager.h"
#include "ekos/align/align.h"
#include "ekos/align/aligndiagnosis.h"
#include "ekos/mount/mount.h"

#include <QElapsedTimer>

#include <cmath>

namespace
{
// Kocab, as in test_ekos_align
const double TARGET_RA = 14.845;
const double TARGET_DEC = 74.106;
const QString SLEW_FAILED_LINE = "Slewing failed: the mount stopped without tracking.";

bool alignLogContains(const QString &text)
{
    for (const QString &line : Ekos::Manager::Instance()->alignModule()->logText())
        if (line.contains(text))
            return true;
    return false;
}
}

TestEkosAlignSlew::TestEkosAlignSlew(QObject *parent) : QObject(parent)
{
    m_CaptureHelper = new TestEkosCaptureHelper();
}

void TestEkosAlignSlew::alignStatusChanged(Ekos::AlignState status)
{
    m_AlignStates.append(status);
}

// Regression test: the alignment waits for the mount to track after a slew. A mount that ends the slew idle instead
// (an OnStep mount stopped at its meridian limit, with tracking off) left the alignment waiting forever, so the
// scheduler job hung at the align stage for 85 minutes. The alignment now fails when the mount stays idle for
// 10 s, and the scheduler's error handling can take over. A slew that ends tracking continues as before.
// The Telescope Simulator ends every goto tracking, so the test stops the slew (abort), which leaves it idle.
void TestEkosAlignSlew::testSlewEndsIdle_data()
{
    QTest::addColumn<bool>("stopSlew");

    QTest::newRow("SlewEndsIdle") << true;
    QTest::newRow("SlewEndsTracking") << false;
}

void TestEkosAlignSlew::testSlewEndsIdle()
{
    QFETCH(bool, stopSlew);
    Ekos::Manager * const ekos = Ekos::Manager::Instance();
    Ekos::Mount *mount = ekos->mountModule();
    Ekos::Align *align = ekos->alignModule();

    // Start tracking at the target
    QVERIFY(m_CaptureHelper->slewTo(TARGET_RA, TARGET_DEC, true));

    // Start an alignment with a long exposure and move the mount while it captures: the alignment suspends the
    // capture and waits for the end of the slew in its slewing state
    QVERIFY(m_CaptureHelper->startAligning(10));
    QTRY_VERIFY_WITH_TIMEOUT(m_AlignStates.contains(Ekos::ALIGN_PROGRESS), 10000);
    QTRY_VERIFY_WITH_TIMEOUT(alignLogContains("Capturing image..."), 10000);
    m_AlignStates.clear();
    align->clearLog();
    QVERIFY(mount->slew(TARGET_RA, TARGET_DEC - 14.0));
    QTRY_VERIFY_WITH_TIMEOUT(mount->status() == ISD::Mount::MOUNT_SLEWING, 10000);
    QTRY_VERIFY_WITH_TIMEOUT(m_AlignStates.contains(Ekos::ALIGN_SUSPENDED), 10000);
    QTRY_VERIFY_WITH_TIMEOUT(align->status() == Ekos::ALIGN_SLEWING, 5000);

    if (stopSlew)
    {
        // Stop the slew once the mount is on its way
        QTRY_VERIFY_WITH_TIMEOUT(std::abs(mount->equatorialCoords().value(1, TARGET_DEC) - TARGET_DEC) > 1.0, 10000);
        QVERIFY(mount->abort());
        QTRY_VERIFY_WITH_TIMEOUT(mount->status() == ISD::Mount::MOUNT_IDLE, 10000);
        QElapsedTimer idle;
        idle.start();

        // The mount stays idle: the alignment fails after the grace period instead of waiting forever
        QTRY_VERIFY2_WITH_TIMEOUT(m_AlignStates.contains(Ekos::ALIGN_FAILED),
                                  qPrintable(QString("Alignment still %1 with the mount idle, log: %2")
                                             .arg(Ekos::getAlignStatusString(align->status()), align->getLogText())), 30000);
        QVERIFY2(idle.elapsed() >= 8000, qPrintable(QString("Failed after %1 ms only").arg(idle.elapsed())));
        QVERIFY(alignLogContains(SLEW_FAILED_LINE));
        QCOMPARE(mount->status(), ISD::Mount::MOUNT_IDLE);
        // Reported as its own failure, not as a user abort, and without a stray "Solver aborted."
        QCOMPARE(align->lastFailReason(), static_cast<int>(Ekos::AlignFailReason::SlewEndedIdle));
        QVERIFY2(!alignLogContains("Solver aborted."), qPrintable(align->getLogText()));
    }
    else
    {
        // The slew ends tracking: the alignment continues with a new capture, and doesn't fail for the slew later
        QTRY_VERIFY_WITH_TIMEOUT(mount->status() == ISD::Mount::MOUNT_TRACKING, 60000);
        QTRY_VERIFY_WITH_TIMEOUT(m_AlignStates.contains(Ekos::ALIGN_PROGRESS), 10000);
        QTest::qWait(12000);
        QVERIFY2(!alignLogContains(SLEW_FAILED_LINE), qPrintable(align->getLogText()));
    }
}

/* *********************************************************************************
 *
 * Test infrastructure
 *
 * ********************************************************************************* */

void TestEkosAlignSlew::initTestCase()
{
    KVERIFY_EKOS_IS_HIDDEN();
    KTRY_OPEN_EKOS();
    KVERIFY_EKOS_IS_OPENED();
    QStandardPaths::setTestModeEnabled(true);

    // Telescope Simulator and CCD Simulator
    QVERIFY(m_CaptureHelper->startEkosProfile());

    Options::setVerboseLogging(false);
    Options::setAlignmentLogging(false);
    Options::setLogToFile(false);

    m_CaptureHelper->prepareOpticalTrains();
    m_CaptureHelper->prepareMountModule();

    // The alignment module without the astrometry index files: no test here needs a solve
    Ekos::Align *align = Ekos::Manager::Instance()->alignModule();
    KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(align, 1000);
    KTRY_SET_COMBO(align, opticalTrainCombo, m_CaptureHelper->m_primaryTrain);
    align->setSolverMode(Ekos::Align::SOLVER_LOCAL);
    KTRY_SET_RADIOBUTTON(align, slewR, true);
    Options::setAstrometryUseRotator(false);
    Options::setAlignCheckFrequency(0);

    m_CaptureHelper->init();
    GUIManager::Instance()->close();

    connect(align, &Ekos::Align::newStatus, this, &TestEkosAlignSlew::alignStatusChanged, Qt::UniqueConnection);
}

void TestEkosAlignSlew::cleanupTestCase()
{
    disconnect(Ekos::Manager::Instance()->alignModule(), &Ekos::Align::newStatus, this,
               &TestEkosAlignSlew::alignStatusChanged);
    m_CaptureHelper->cleanup();
    QVERIFY(m_CaptureHelper->shutdownEkosProfile());
    KTRY_CLOSE_EKOS();
    KVERIFY_EKOS_IS_HIDDEN();
}

void TestEkosAlignSlew::init()
{
    m_AlignStates.clear();
}

void TestEkosAlignSlew::cleanup()
{
    Ekos::Manager::Instance()->alignModule()->abort();
    if (Ekos::Manager::Instance()->mountModule()->status() == ISD::Mount::MOUNT_SLEWING)
        Ekos::Manager::Instance()->mountModule()->abort();
}

QTEST_KSTARS_MAIN(TestEkosAlignSlew)

#endif // HAVE_INDI
