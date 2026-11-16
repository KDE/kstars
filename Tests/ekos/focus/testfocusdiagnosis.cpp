/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

/*
 * Tests for the autofocus failure rules: one data row per rule, facts in, item ids and order out.
 * Also the suggested values and their limits, the reason keys, the failures that get no diagnosis,
 * runs started by a sequence, the evidence, and the check-up when nothing failed.
 */

#include "ekos/focus/focusdiagnosis.h"

#include <QSet>
#include <QTest>

using namespace Ekos;
using namespace Ekos::Diagnosis;

Q_DECLARE_METATYPE(Ekos::FocusFacts)

class TestFocusDiagnosis : public QObject
{
        Q_OBJECT

    private slots:
        void testRules_data();
        void testRules();
        void testItemDetails();
        void testSuggestions();
        void testDiagnosable();
        void testReasonKeys();
        void testInSequence();
        void testTriedAndDismissed();
        void testEvidence();
        void testCheckup();

    private:
        /** Facts of a working setup with an absolute focuser: every check passes. */
        static FocusFacts healthy();
        static FocusFacts withReason(AutofocusFailReason reason);
        static QStringList ids(const Report &report);
};

FocusFacts TestFocusDiagnosis::healthy()
{
    FocusFacts facts;
    facts.state = 0;
    facts.train = "Primary";
    facts.hasCamera = true;
    facts.cameraConnected = true;
    facts.cameraName = "CCD Simulator";
    facts.canBin = true;
    facts.hasFocuser = true;
    facts.focuserConnected = true;
    facts.focuserName = "Focuser Simulator";
    facts.canAbsMove = true;
    facts.focuserMin = 0;
    facts.focuserMax = 100000;
    facts.startPosition = 50000;
    facts.position = 50000;
    facts.exposure = 2;
    facts.binIndex = 0;
    facts.binCount = 4;
    facts.algorithm = FocusDiagnosis::ALGORITHM_LINEAR1PASS;
    facts.stepSize = 100;
    facts.maxTravel = 10000;
    facts.maxSingleStep = 1000;
    facts.stars = 20;
    facts.hfr = 2.5;

    ImageStats image;
    image.verdict = ImageVerdict::Good;
    image.stars = 20;
    image.hfr = 2.5;
    image.concentration = 3;
    image.elongation = 1.1;
    facts.image = image;
    return facts;
}

FocusFacts TestFocusDiagnosis::withReason(AutofocusFailReason reason)
{
    FocusFacts facts = healthy();
    facts.reason = reason;
    return facts;
}

QStringList TestFocusDiagnosis::ids(const Report &report)
{
    QStringList list;
    for (const auto &item : report.items)
        list << item.id;
    return list;
}

void TestFocusDiagnosis::testRules_data()
{
    QTest::addColumn<FocusFacts>("facts");
    QTest::addColumn<QStringList>("expected");

    // Devices
    FocusFacts facts = withReason(FOCUS_FAIL_NO_CAMERA);
    facts.hasCamera = false;
    QTest::newRow("no camera in the train") << facts << QStringList({"train_select_camera"});

    facts = withReason(FOCUS_FAIL_NO_CAMERA);
    facts.cameraConnected = false;
    QTest::newRow("camera disconnected") << facts << QStringList({"camera_connect"});

    facts = withReason(FOCUS_FAIL_NO_FOCUSER);
    facts.hasFocuser = false;
    QTest::newRow("no focuser in the train") << facts << QStringList({"train_select_focuser"});

    facts = withReason(FOCUS_FAIL_NO_FOCUSER);
    facts.focuserConnected = false;
    QTest::newRow("focuser disconnected") << facts << QStringList({"focuser_connect"});

    facts = withReason(FOCUS_FAIL_FOCUSER_ERROR);
    QTest::newRow("focuser error") << facts << QStringList({"check_focuser"});

    // A focuser that dropped out is the first thing to fix
    facts = withReason(FOCUS_FAIL_FOCUSER_NO_MOVE);
    facts.focuserConnected = false;
    QTest::newRow("focuser did not move, disconnected") << facts << QStringList({"focuser_connect", "check_focuser"});

    facts = withReason(FOCUS_FAIL_CAPTURE_TIMEOUT);
    QTest::newRow("capture timeout") << facts << QStringList({"check_camera_usb_power"});

    facts = withReason(FOCUS_FAIL_CAPTURE_FAILED);
    facts.cameraConnected = false;
    QTest::newRow("capture failed, camera disconnected") << facts << QStringList({"camera_connect", "check_camera_usb_power"});

    facts = withReason(FOCUS_FAIL_FILTER_MANAGER);
    facts.hasFilterWheel = true;
    facts.filterWheelConnected = false;
    QTest::newRow("filter wheel disconnected") << facts << QStringList({"filter_connect", "filter_test_move"});

    facts = withReason(FOCUS_FAIL_DUSTCAP_ERROR);
    QTest::newRow("dust cap error") << facts << QStringList({"remove_dust_cap"});

    facts = withReason(FOCUS_FAIL_LOW_PULSE);
    facts.canAbsMove = false;
    facts.canTimerMove = true;
    facts.stepSize = 20;
    QTest::newRow("pulse too short") << facts << QStringList({"focus_step_min"});

    // No stars
    facts = withReason(FOCUS_FAIL_NO_STARS);
    facts.image.reset();
    facts.stars = 0;
    QTest::newRow("no stars, frame not measured") << facts
            << QStringList({"focus_exposure_raise", "focus_bin_2x2", "check_sky_and_cap"});

    // Nothing left to raise: the sky and the cap
    facts = withReason(FOCUS_FAIL_NO_STARS);
    facts.exposure = FocusDiagnosis::MAX_SUGGESTED_EXPOSURE;
    facts.binIndex = 1;
    facts.image->verdict = ImageVerdict::FewStars;
    facts.image->stars = 2;
    QTest::newRow("no stars, few in the frame") << facts << QStringList({"check_sky_and_cap"});

    // Rings: far from focus comes first, then the signal
    facts = withReason(FOCUS_FAIL_NO_STARS);
    facts.image->verdict = ImageVerdict::OutOfFocus;
    facts.image->hfr = 9;
    facts.image->centreDip = 0.4;
    QTest::newRow("no stars, rings") << facts
                                     << QStringList({"focus_near_focus", "focus_exposure_raise", "focus_bin_2x2", "focus_advisor", "focus_donut_buster"});

    facts = withReason(FOCUS_FAIL_NO_STARS);
    facts.image->verdict = ImageVerdict::OutOfFocus;
    facts.donutBuster = true;
    facts.canAbsMove = false;
    QTest::newRow("no stars, discs, relative focuser, Donut Buster on") << facts
            << QStringList({"focus_near_focus", "focus_exposure_raise", "focus_bin_2x2"});

    facts = withReason(FOCUS_FAIL_NO_STARS);
    facts.image->verdict = ImageVerdict::Trailed;
    facts.image->trailLength = 30;
    QTest::newRow("no stars, trailed") << facts
                                       << QStringList({"focus_exposure_raise", "focus_bin_2x2", "check_tracking"});

    // The focus curve
    facts = withReason(FOCUS_FAIL_FOCUSER_OOB);
    QTest::newRow("out of bounds, travel can grow") << facts
            << QStringList({"focus_max_travel_raise", "focus_near_focus", "focus_advisor"});

    facts = withReason(FOCUS_FAIL_FOCUSER_OOB);
    facts.maxTravel = 100000;
    QTest::newRow("out of bounds, travel at the range") << facts << QStringList({"focus_near_focus", "focus_advisor"});

    facts = withReason(FOCUS_FAIL_MAX_ITERS);
    QTest::newRow("max iterations") << facts << QStringList({"focus_near_focus", "check_focuser_mechanics", "focus_advisor"});

    facts = withReason(FOCUS_FAIL_MAX_ITERS);
    facts.canAbsMove = false;
    facts.canRelMove = true;
    QTest::newRow("max iterations, relative focuser") << facts << QStringList({"focus_near_focus", "check_focuser_mechanics"});

    facts = withReason(FOCUS_FAIL_SMALL_HFR);
    QTest::newRow("small HFR change") << facts << QStringList({"focus_step_raise", "focus_advisor"});

    facts = withReason(FOCUS_FAIL_SMALL_HFR);
    facts.stepSize = facts.maxSingleStep;
    QTest::newRow("small HFR change, step at its limit") << facts << QStringList({"focus_advisor"});

    facts = withReason(FOCUS_FAIL_FLUCTUATIONS);
    QTest::newRow("fluctuations") << facts
                                  << QStringList({"focus_advisor", "check_focuser_mechanics", "focus_exposure_raise", "check_conditions", "focus_step_raise"});

    facts = withReason(FOCUS_FAIL_R2);
    facts.reasonInfo = "R2=0.52 < Limit=0.80";
    QTest::newRow("R2") << facts << QStringList({"check_conditions", "check_focuser_mechanics", "focus_exposure_raise", "focus_advisor"});

    // Rings explain a curve that failed too: far from focus comes first
    facts = withReason(FOCUS_FAIL_CURVEFIT);
    facts.image->verdict = ImageVerdict::OutOfFocus;
    QTest::newRow("curve fit, rings") << facts
                                      << QStringList({"focus_near_focus", "focus_advisor", "check_focuser_mechanics", "focus_exposure_raise", "check_conditions", "focus_step_raise", "focus_donut_buster"});

    // Nothing specific
    facts = withReason(FOCUS_FAIL_TOLERANCE);
    facts.canAbsMove = false;
    QTest::newRow("tolerance, relative focuser") << facts << QStringList({"focus_check_log"});
}

void TestFocusDiagnosis::testRules()
{
    QFETCH(FocusFacts, facts);
    QFETCH(QStringList, expected);

    const Report report = FocusDiagnosis::diagnose(facts);
    QCOMPARE(ids(report), expected);

    QCOMPARE(report.module, QString("focus"));
    QCOMPARE(report.train, QString("Primary"));
    QCOMPARE(report.reason, static_cast<int>(facts.reason));
    QCOMPARE(report.reasonKey, focusFailReasonKey(facts.reason));
    QVERIFY(!report.title.isEmpty());
    QVERIFY(report.id.isEmpty());

    for (const auto &item : report.items)
    {
        QVERIFY2(!item.title.isEmpty(), qPrintable(item.id));
        QVERIFY2(!item.why.isEmpty(), qPrintable(item.id));
        if (item.kind == Kind::Manual)
            QVERIFY2(!item.steps.isEmpty(), qPrintable(item.id));
        if (item.kind == Kind::Open)
            QVERIFY2(!item.openTarget.isEmpty(), qPrintable(item.id));
        // Not started by a sequence: an auto item retries autofocus, unless it moves hardware
        if (item.kind == Kind::Auto)
            QVERIFY2(item.autoRetry, qPrintable(item.id));
    }
}

void TestFocusDiagnosis::testItemDetails()
{
    auto facts = withReason(FOCUS_FAIL_NO_STARS);
    facts.image->verdict = ImageVerdict::OutOfFocus;
    facts.image->centreDip = 0.4;
    Report report = FocusDiagnosis::diagnose(facts);

    const Item *item = findItem(report, "focus_exposure_raise");
    QVERIFY(item);
    QCOMPARE(item->kind, Kind::Auto);
    QVERIFY(item->cost.testFlag(ChangesSettings));
    QVERIFY2(item->title.contains("4 seconds"), qPrintable(item->title));

    item = findItem(report, "focus_near_focus");
    QVERIFY(item);
    QCOMPARE(item->likelihood, Likelihood::Likely);
    QVERIFY2(item->why.contains("rings"), qPrintable(item->why));

    item = findItem(report, "focus_advisor");
    QVERIFY(item);
    QCOMPARE(item->kind, Kind::Open);
    QCOMPARE(item->openTarget, QString("focus_advisor"));

    // Defocused stars: the signal is only possibly the cause
    QCOMPARE(findItem(report, "focus_bin_2x2")->likelihood, Likelihood::Possible);

    facts = withReason(FOCUS_FAIL_FOCUSER_ERROR);
    report = FocusDiagnosis::diagnose(facts);
    item = findItem(report, "check_focuser");
    QVERIFY(item);
    QVERIFY(item->cost.testFlag(MovesHardware));
    QVERIFY(item->steps.join(" ").contains("power"));

    facts = withReason(FOCUS_FAIL_FOCUSER_OOB);
    report = FocusDiagnosis::diagnose(facts);
    item = findItem(report, "focus_max_travel_raise");
    QVERIFY(item);
    QVERIFY2(item->title.contains("20000"), qPrintable(item->title));
    QVERIFY2(item->why.contains("10000"), qPrintable(item->why));

    facts = withReason(FOCUS_FAIL_LOW_PULSE);
    facts.canAbsMove = false;
    facts.stepSize = 20;
    report = FocusDiagnosis::diagnose(facts);
    item = findItem(report, "focus_step_min");
    QVERIFY(item);
    QVERIFY2(item->title.contains(QString::number(FocusDiagnosis::SUGGESTED_PULSE_STEP)), qPrintable(item->title));
}

void TestFocusDiagnosis::testSuggestions()
{
    QCOMPARE(FocusDiagnosis::suggestedExposure(0.01), 1.0);
    QCOMPARE(FocusDiagnosis::suggestedExposure(2), 4.0);
    QCOMPARE(FocusDiagnosis::suggestedExposure(8), FocusDiagnosis::MAX_SUGGESTED_EXPOSURE);
    QCOMPARE(FocusDiagnosis::suggestedExposure(FocusDiagnosis::MAX_SUGGESTED_EXPOSURE), FocusDiagnosis::MAX_SUGGESTED_EXPOSURE);

    auto facts = healthy();
    QCOMPARE(FocusDiagnosis::suggestedStepSize(facts), 200);
    facts.maxSingleStep = 150;
    QCOMPARE(FocusDiagnosis::suggestedStepSize(facts), 150);
    facts.maxSingleStep = 100;
    QCOMPARE(FocusDiagnosis::suggestedStepSize(facts), 0);
    facts.maxSingleStep = 0;
    QCOMPARE(FocusDiagnosis::suggestedStepSize(facts), 200);

    facts = healthy();
    QCOMPARE(FocusDiagnosis::suggestedMaxTravel(facts), 20000.0);
    facts.focuserMax = 15000;
    QCOMPARE(FocusDiagnosis::suggestedMaxTravel(facts), 15000.0);
    facts.maxTravel = 15000;
    QCOMPARE(FocusDiagnosis::suggestedMaxTravel(facts), 0.0);
    // A relative focuser has no range to grow into
    facts = healthy();
    facts.canAbsMove = false;
    QCOMPARE(FocusDiagnosis::suggestedMaxTravel(facts), 0.0);
}

void TestFocusDiagnosis::testDiagnosable()
{
    const QList<AutofocusFailReason> skipped({FOCUS_FAIL_NONE, FOCUS_FAIL_ABORT, FOCUS_FAIL_FORCE_ABORT, FOCUS_FAIL_INTERNAL,
            FOCUS_FAIL_ADVISOR_COMPLETE, FOCUS_FAIL_ADVISOR_RERUN, FOCUS_FAIL_OPTIMISED_OUT});
    for (int i = 0; i < FOCUS_FAIL_MAX_REASONS; i++)
    {
        const auto reason = static_cast<AutofocusFailReason>(i);
        QCOMPARE(FocusDiagnosis::isDiagnosable(reason), !skipped.contains(reason));
    }

    QVERIFY(FocusDiagnosis::isFrameReason(FOCUS_FAIL_NO_STARS));
    QVERIFY(FocusDiagnosis::isFrameReason(FOCUS_FAIL_R2));
    QVERIFY(!FocusDiagnosis::isFrameReason(FOCUS_FAIL_FOCUSER_ERROR));
    QVERIFY(!FocusDiagnosis::isFrameReason(FOCUS_FAIL_NO_CAMERA));

    // After the retries a run ends with a forced abort: the reason of the failed attempts is reported
    QCOMPARE(FocusDiagnosis::reportedReason(FOCUS_FAIL_FORCE_ABORT, FOCUS_FAIL_NO_STARS), FOCUS_FAIL_NO_STARS);
    QCOMPARE(FocusDiagnosis::reportedReason(FOCUS_FAIL_FORCE_ABORT, FOCUS_FAIL_NONE), FOCUS_FAIL_FORCE_ABORT);
    QVERIFY(!FocusDiagnosis::isDiagnosable(FocusDiagnosis::reportedReason(FOCUS_FAIL_FORCE_ABORT, FOCUS_FAIL_NONE)));
    // A run that failed without retries keeps its own reason
    QCOMPARE(FocusDiagnosis::reportedReason(FOCUS_FAIL_FOCUSER_OOB, FOCUS_FAIL_NO_STARS), FOCUS_FAIL_FOCUSER_OOB);
    // A user abort stays an abort
    QCOMPARE(FocusDiagnosis::reportedReason(FOCUS_FAIL_ABORT, FOCUS_FAIL_NO_STARS), FOCUS_FAIL_ABORT);
}

void TestFocusDiagnosis::testReasonKeys()
{
    QSet<QString> keys;
    for (int i = 0; i < FOCUS_FAIL_MAX_REASONS; i++)
    {
        const auto reason = static_cast<AutofocusFailReason>(i);
        const QString key = focusFailReasonKey(reason);
        QVERIFY2(!keys.contains(key), qPrintable(key));
        keys << key;

        // Every diagnosed reason has a title of its own and at least one item
        if (FocusDiagnosis::isDiagnosable(reason))
        {
            const Report report = FocusDiagnosis::diagnose(withReason(reason));
            QVERIFY2(report.title != QString("Autofocus failed"), qPrintable(key));
            QVERIFY2(!report.items.isEmpty(), qPrintable(key));
        }
    }

    // The keys go out on D-Bus and EkosLive: they do not change
    QCOMPARE(focusFailReasonKey(FOCUS_FAIL_NO_STARS), QString("no_stars"));
    QCOMPARE(focusFailReasonKey(FOCUS_FAIL_FOCUSER_OOB), QString("focuser_out_of_bounds"));
    QCOMPARE(focusFailReasonKey(FOCUS_FAIL_DUSTCAP_ERROR), QString("dustcap_error"));
}

void TestFocusDiagnosis::testInSequence()
{
    // A sequence or the scheduler handles a failed autofocus: no item starts autofocus again
    auto facts = withReason(FOCUS_FAIL_NO_STARS);
    facts.image.reset();
    facts.inSequence = true;
    const Report report = FocusDiagnosis::diagnose(facts);
    QVERIFY(!report.items.isEmpty());
    for (const auto &item : report.items)
        QVERIFY2(!item.autoRetry, qPrintable(item.id));
}

void TestFocusDiagnosis::testTriedAndDismissed()
{
    auto facts = withReason(FOCUS_FAIL_NO_STARS);
    facts.image.reset();

    Report report = FocusDiagnosis::diagnose(facts, {"focus_exposure_raise"});
    QCOMPARE(ids(report), QStringList({"focus_bin_2x2", "check_sky_and_cap"}));
    QCOMPARE(report.tried, QStringList({"focus_exposure_raise"}));

    // Nothing left to change: the sky and the cap become the likely cause
    report = FocusDiagnosis::diagnose(facts, {"focus_exposure_raise"}, {"focus_bin_2x2"});
    QCOMPARE(ids(report), QStringList({"check_sky_and_cap"}));
    QCOMPARE(report.items.first().likelihood, Likelihood::Likely);
}

void TestFocusDiagnosis::testEvidence()
{
    auto facts = withReason(FOCUS_FAIL_R2);
    facts.reasonInfo = "R2=0.52 < Limit=0.80";
    Report report = FocusDiagnosis::diagnose(facts);

    QHash<QString, Evidence> byKey;
    for (const auto &evidence : report.evidence)
        byKey.insert(evidence.key, evidence);
    QCOMPARE(byKey["detail"].value.toString(), QString("R2=0.52 < Limit=0.80"));
    QCOMPARE(byKey["exposure"].value.toDouble(), 2.0);
    QCOMPARE(byKey["step_size"].unit, QString("steps"));
    QCOMPARE(byKey["algorithm"].value.toString(), QString("Linear 1 Pass"));
    QCOMPARE(byKey["focuser_range"].value.toString(), QString("0 - 100000"));
    QCOMPARE(byKey["image"].level, Evidence::Ok);
    QVERIFY(report.ruledOut.join(" ").contains("camera and the focuser are connected"));
    QVERIFY(report.ruledOut.join(" ").contains("round stars"));

    // A focuser that moves by time counts its steps in ms
    facts = withReason(FOCUS_FAIL_LOW_PULSE);
    facts.canAbsMove = false;
    facts.stepSize = 20;
    report = FocusDiagnosis::diagnose(facts);
    byKey.clear();
    for (const auto &evidence : report.evidence)
        byKey.insert(evidence.key, evidence);
    QCOMPARE(byKey["step_size"].unit, QString("ms"));
    QCOMPARE(byKey["step_size"].level, Evidence::Bad);

    // An unmeasured frame is listed as not checked
    facts = withReason(FOCUS_FAIL_NO_STARS);
    facts.image.reset();
    report = FocusDiagnosis::diagnose(facts);
    QVERIFY(report.notChecked.join(" ").contains("Image"));
}

void TestFocusDiagnosis::testCheckup()
{
    auto facts = healthy();
    Report report = FocusDiagnosis::checkup(facts);
    QVERIFY(isCheckup(report));
    QCOMPARE(report.module, QString("focus"));
    QVERIFY(report.items.isEmpty());
    QCOMPARE(report.title, QString("Focus looks ready"));
    QVERIFY(report.ruledOut.join(" ").contains("camera CCD Simulator is connected"));
    QVERIFY(report.ruledOut.join(" ").contains("focuser Focuser Simulator is connected"));

    // A failure reason in the facts does not matter
    facts.reason = FOCUS_FAIL_NO_STARS;
    QVERIFY(FocusDiagnosis::checkup(facts).items.isEmpty());

    facts = healthy();
    facts.hasCamera = false;
    facts.focuserConnected = false;
    facts.hasFilterWheel = true;
    report = FocusDiagnosis::checkup(facts);
    QCOMPARE(ids(report), QStringList({"train_select_camera", "focuser_connect", "filter_connect"}));
    QCOMPARE(report.title, QString("3 things to check"));
    for (const auto &item : report.items)
        QVERIFY2(!item.autoRetry, qPrintable(item.id));

    // A focuser that moves by time with a step autofocus refuses
    facts = healthy();
    facts.canAbsMove = false;
    facts.canTimerMove = true;
    facts.stepSize = FOCUS_MINIMUM_PULSE_TIMER;
    QCOMPARE(ids(FocusDiagnosis::checkup(facts)), QStringList({"focus_step_min"}));
    facts.stepSize = FOCUS_MINIMUM_PULSE_TIMER + 1;
    QVERIFY(FocusDiagnosis::checkup(facts).items.isEmpty());

    // Dismissed items are not offered
    facts = healthy();
    facts.focuserConnected = false;
    QVERIFY(FocusDiagnosis::checkup(facts, {"focuser_connect"}).items.isEmpty());
}

QTEST_GUILESS_MAIN(TestFocusDiagnosis)

#include "testfocusdiagnosis.moc"
