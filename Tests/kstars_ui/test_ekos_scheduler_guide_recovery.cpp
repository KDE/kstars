/*
    KStars UI test: a scheduler job resumes its capture sequence after guiding aborted it

    SPDX-FileCopyrightText: 2026 Andreas R. <temp@ruthner.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "test_ekos_scheduler_guide_recovery.h"

#if defined(HAVE_INDI)

#include "kstars_ui_tests.h"
#include "test_ekos.h"
#include "Options.h"
#include "indi/guimanager.h"
#include "ekos/auxiliary/opticaltrainmanager.h"
#include "ekos/capture/capture.h"
#include "ekos/guide/guide.h"
#include "ekos/scheduler/scheduler.h"
#include "ekos/scheduler/schedulerprocess.h"

#include <KMessageBox>

namespace
{
// Kocab, circumpolar and with enough stars for the Guide Simulator
const double TARGET_RA = 14.845;
const double TARGET_DEC = 74.106;
const QString TARGET = "Kocab";
// Three rows with their own filters, so the frames of each row can be counted
const QString SEQUENCE = "Red:2,Green:2,Blue:3";
const double EXPOSURE = 2.0;
}

TestEkosSchedulerGuideRecovery::TestEkosSchedulerGuideRecovery(QObject *parent) : QObject(parent)
{
    m_CaptureHelper = new TestEkosCaptureHelper("Internal");
}

QString TestEkosSchedulerGuideRecovery::framesSummary() const
{
    QStringList parts;
    for (auto it = m_Frames.cbegin(); it != m_Frames.cend(); ++it)
        parts << QString("%1:%2").arg(it.key()).arg(it.value());
    return QString("frames %1, scheduler log: %2").arg(parts.join(",")).arg(m_SchedulerLog.join(" | "));
}

bool TestEkosSchedulerGuideRecovery::setupGuiding()
{
    Ekos::Guide *guide = Ekos::Manager::Instance()->guideModule();
    KWRAP_SUB(KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(guide, 1000));
    // a quick calibration, kept when the scheduler restarts guiding
    Options::setReuseGuideCalibration(false);
    Options::setResetGuideCalibration(false);
    Options::setSerializedCalibration("");
    Options::setAutoModeIterations(2);
    Options::setCalibrationPulseDuration(500);
    Options::setTwoAxisEnabled(true);
    Options::setDitherEnabled(false);
    guide->clearCalibration();
    KTRY_SET_COMBO_SUB(guide, opticalTrainCombo, m_CaptureHelper->m_guidingTrain);
    KTRY_SET_CHECKBOX_SUB(guide, guideAutoStar, true);
    KTRY_SET_COMBO_SUB(guide, guideSquareSize, "32");
    KTRY_SET_COMBO_SUB(guide, guideBinning, "1x1");
    KTRY_SET_DOUBLESPINBOX_SUB(guide, guideExposure, 1.0);
    return true;
}

bool TestEkosSchedulerGuideRecovery::setupSchedulerJob(const QString &sequenceFile)
{
    Ekos::Scheduler *scheduler = Ekos::Manager::Instance()->schedulerModule();
    KWRAP_SUB(KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(scheduler, 1000));
    scheduler->setSequence(sequenceFile);
    KTRY_SET_LINEEDIT_SUB(scheduler, nameEdit, TARGET);
    KTRY_SET_LINEEDIT_SUB(scheduler, raBox, "14 50 42");
    KTRY_SET_LINEEDIT_SUB(scheduler, decBox, "74 09 20");
    KTRY_SET_COMBO_SUB(scheduler, opticalTrainCombo, m_CaptureHelper->m_primaryTrain);
    // guiding only, the mount already points at the target
    KTRY_SET_CHECKBOX_SUB(scheduler, schedulerTrackStep, false);
    KTRY_SET_CHECKBOX_SUB(scheduler, schedulerFocusStep, false);
    KTRY_SET_CHECKBOX_SUB(scheduler, schedulerAlignStep, false);
    KTRY_SET_CHECKBOX_SUB(scheduler, schedulerGuideStep, true);
    KTRY_SET_CHECKBOX_SUB(scheduler, schedulerTwilight, false);
    KTRY_SET_RADIOBUTTON_SUB(scheduler, schedulerCompleteSequences, true);
    KTRY_CLICK_SUB(scheduler, addToQueueB);
    KTRY_GADGET_SUB(scheduler, QTableWidget, queueTable);
    KTRY_VERIFY_WITH_TIMEOUT_SUB(queueTable->rowCount() == 1, 5000);
    return true;
}

// Regression test: when guiding aborted a running capture, the scheduler restarted guiding and then loaded the
// sequence again, so with "Remember job progress" off the job started over from its first row (Discord, lleege:
// M 45 shot 307 of 120 frames all night). The capture now resumes after guiding runs again.
void TestEkosSchedulerGuideRecovery::testGuideAbortResumesSequence()
{
    Options::setRememberJobProgress(false);
    Options::setStopEkosAfterShutdown(false);
    QVERIFY(setupGuiding());

    KTELL("Create the capture sequence with three rows");
    Ekos::Capture *capture = Ekos::Manager::Instance()->captureModule();
    KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(capture, 1000);
    KTRY_SET_CHECKBOX(capture, enforceRefocusEveryN, false);
    const QString imagepath = m_CaptureHelper->getImageLocation()->path() + "/" + TARGET;
    QVERIFY(m_CaptureHelper->fillCaptureSequences(TARGET, SEQUENCE, EXPOSURE, imagepath));
    const QString sequenceFile = m_CaptureHelper->destination->filePath("guide-recovery.esq");
    QVERIFY(capture->saveSequenceQueue(sequenceFile));
    capture->clearSequenceQueue();

    KTELL("Create the scheduler job and start it");
    QVERIFY(setupSchedulerJob(sequenceFile));
    KTRY_CLICK(Ekos::Manager::Instance()->schedulerModule(), startB);

    KTELL("Wait for the first frame of the third row");
    QTRY_VERIFY2_WITH_TIMEOUT(m_Frames.value("Blue") >= 1, qPrintable(framesSummary()), 600000);
    QCOMPARE(m_Frames.value("Red"), 2);
    QCOMPARE(m_Frames.value("Green"), 2);

    KTELL("Abort guiding during the third row");
    Ekos::Manager::Instance()->guideModule()->abort();
    QTRY_VERIFY2_WITH_TIMEOUT(m_SchedulerLog.join("\n").contains("is restarting its guiding procedure"),
                              qPrintable(framesSummary()), 60000);

    KTELL("Wait for the job to finish");
    QTRY_VERIFY2_WITH_TIMEOUT(m_SchedulerLog.join("\n").contains(QString("Job '%1' is complete.").arg(TARGET)),
                              qPrintable(framesSummary()), 600000);

    // The first two rows are not captured again, the third row is completed
    QVERIFY2(m_Frames.value("Red") == 2, qPrintable(framesSummary()));
    QVERIFY2(m_Frames.value("Green") == 2, qPrintable(framesSummary()));
    QVERIFY2(m_Frames.value("Blue") >= 3, qPrintable(framesSummary()));
}

void TestEkosSchedulerGuideRecovery::initTestCase()
{
    KVERIFY_EKOS_IS_HIDDEN();
    KTRY_OPEN_EKOS();
    KVERIFY_EKOS_IS_OPENED();
    QStandardPaths::setTestModeEnabled(true);
    QVERIFY(m_CaptureHelper->startEkosProfile());
    m_CaptureHelper->init();
    Options::setUseFITSViewer(false);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    KMessageBox::saveDontShowAgainTwoActions("astronomical_twilight_warning", KMessageBox::ButtonCode::Cancel);
#else
    KMessageBox::saveDontShowAgainYesNo("astronomical_twilight_warning", KMessageBox::ButtonCode::No);
#endif
    // Primary train with the CCD Simulator, Secondary train with the Guide Simulator
    m_CaptureHelper->prepareOpticalTrains();
    QTRY_VERIFY_WITH_TIMEOUT(Ekos::OpticalTrainManager::Instance()->getTrainNames().contains(
                                 m_CaptureHelper->m_guidingTrain), 10000);
    // Focal lengths and apertures of both scopes, which the guide calibration needs
    m_CaptureHelper->prepareMountModule();
    GUIManager::Instance()->close();
    Ekos::OpticalTrainManager::Instance()->close();
    // Point at a field with stars
    QVERIFY(m_CaptureHelper->slewTo(TARGET_RA, TARGET_DEC, true));
}

void TestEkosSchedulerGuideRecovery::cleanupTestCase()
{
    m_CaptureHelper->cleanup();
    QVERIFY(m_CaptureHelper->shutdownEkosProfile());
    KTRY_CLOSE_EKOS();
    KVERIFY_EKOS_IS_HIDDEN();
}

void TestEkosSchedulerGuideRecovery::init()
{
    m_Frames.clear();
    m_SchedulerLog.clear();
    connect(Ekos::Manager::Instance()->captureModule(), &Ekos::Capture::captureComplete, this,
            [this](const QVariantMap & metadata)
    {
        m_Frames[metadata["filter"].toString()]++;
    });
    connect(Ekos::Manager::Instance()->schedulerModule()->process().data(), &Ekos::SchedulerProcess::newLog, this,
            [this](const QString & text)
    {
        m_SchedulerLog << text;
    });
    QVERIFY(m_CaptureHelper->getImageLocation()->removeRecursively());
}

void TestEkosSchedulerGuideRecovery::cleanup()
{
    m_CaptureHelper->cleanupScheduler();
    Ekos::Manager::Instance()->guideModule()->abort();
    disconnect(Ekos::Manager::Instance()->captureModule(), &Ekos::Capture::captureComplete, this, nullptr);
    disconnect(Ekos::Manager::Instance()->schedulerModule()->process().data(), &Ekos::SchedulerProcess::newLog, this,
               nullptr);
    Ekos::Manager::Instance()->captureModule()->clearSequenceQueue();
}

QTEST_KSTARS_MAIN(TestEkosSchedulerGuideRecovery)

#endif // HAVE_INDI
