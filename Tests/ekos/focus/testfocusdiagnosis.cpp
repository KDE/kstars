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

#include <QLocale>
#include <QSet>
#include <QTest>
#include <QTimeZone>

#include <cmath>

using namespace Ekos;
using namespace Ekos::Diagnosis;

Q_DECLARE_METATYPE(Ekos::FocusFacts)
Q_DECLARE_METATYPE(QList<Ekos::FocusPoint>)

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
        void testRunCheck_data();
        void testRunCheck();
        void testReference();
        void testReferenceRuns();
        void testLastGoodTarget();
        void testNoMinimum();
        void testSettingsChecks();

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

namespace
{
QList<FocusPoint> points(const QList<QPair<int, double>> &measures, const QList<int> &outliers = {})
{
    QList<FocusPoint> list;
    for (const auto &measure : measures)
        list << FocusPoint{measure.first, measure.second, outliers.contains(measure.first)};
    return list;
}

// Runs of the Linear 1 Pass algorithm with the CCD and Focuser Simulators, whose best focus is at 36,700
const QList<QPair<int, double>> FAR_SLOPE =
{
    {61000, 6.919}, {60800, 6.040}, {60600, 6.188}, {60400, 6.279}, {60200, 6.272}, {60000, 5.985}, {59800, 6.295},
    {59600, 5.846}, {59400, 5.793}, {59200, 5.866}, {59000, 5.416}
};
const QList<QPair<int, double>> FAR_SLOPE_2 =
{
    {61000, 6.815}, {60800, 6.763}, {60600, 6.720}, {60400, 6.647}, {60200, 6.507}, {60000, 6.493}, {59800, 6.409},
    {59600, 6.269}, {59400, 6.134}, {59200, 6.196}, {59000, 6.051}
};
const QList<QPair<int, double>> FAR_FLAT =
{
    {61000, 4.843}, {60800, 4.632}, {60600, 4.733}, {60400, 5.028}, {60200, 4.318}, {60000, 4.582}, {59800, 4.196},
    {59600, 4.441}, {59400, 4.686}, {59200, 4.491}, {59000, 4.490}
};
const QList<QPair<int, double>> IN_FOCUS =
{
    {50000, 3.025}, {47500, 2.548}, {45000, 2.112}, {42500, 1.920}, {40000, 1.762}, {37500, 1.621}, {35000, 1.639},
    {32500, 1.767}, {30000, 1.974}, {27500, 2.265}, {25000, 2.699}
};
// The in-focus HFR of the simulators
constexpr double REFERENCE = 1.62;

// Runs on the sky with a ToupTek ATR2600M (2x2) and a NightCrawler, 2026-11-16, best focus near 52,713, in-focus HFR
// 0.907 px. A run with steps of 20 that started 3,000 steps out: the curve fit passed, the stars hardly changed.
const QList<QPair<int, double>> SKY_FAR =
{
    {55793, 3.973}, {55773, 3.677}, {55753, 3.792}, {55733, 3.925}, {55713, 3.775}, {55693, 3.658}, {55673, 3.576},
    {55653, 3.643}, {55633, 3.497}, {55613, 3.576}, {55593, 3.560}
};
// The same start before it: no curve fitted the points
const QList<QPair<int, double>> SKY_FAR_NO_FIT =
{
    {55813, 3.852}, {55793, 3.662}, {55773, 3.802}, {55753, 3.719}, {55733, 3.810}, {55713, 3.720}, {55693, 3.671},
    {55673, 3.546}, {55653, 3.688}, {55633, 3.544}, {55613, 3.509}
};
// Steps of 200 from 49,417, 3,300 steps below focus: the stars grew all the way, no curve fitted
const QList<QPair<int, double>> SKY_BELOW =
{
    {50417, 3.004}, {50217, 3.420}, {50017, 3.503}, {49817, 3.902}, {49617, 4.243}, {49417, 4.432}, {49217, 4.715},
    {49017, 5.210}, {48817, 5.379}, {48617, 5.615}, {48417, 5.874}
};
// A good run around focus: its V is shallow, the ends only 1.31 and 1.41 times the smallest stars
const QList<QPair<int, double>> SKY_IN_FOCUS =
{
    {53817, 1.359}, {53617, 1.290}, {53417, 1.137}, {53217, 1.069}, {53017, 1.002}, {52817, 0.941}, {52617, 0.940},
    {52417, 0.941}, {52217, 1.066}, {52017, 1.164}, {51817, 1.306}
};
constexpr double SKY_REFERENCE = 0.907;
}

void TestFocusDiagnosis::testRunCheck_data()
{
    QTest::addColumn<QList<FocusPoint>>("run");
    QTest::addColumn<double>("reference");
    QTest::addColumn<bool>("r2Failed");
    QTest::addColumn<bool>("noMinimum");
    QTest::addColumn<int>("direction");
    QTest::addColumn<bool>("hasV");

    // Started at 60,000 with steps of 200: the stars got smaller toward 59,000 and never reached focus
    QTest::newRow("far, slope") << points(FAR_SLOPE) << REFERENCE << false << true << -1 << false;
    QTest::newRow("far, slope, another star field") << points(FAR_SLOPE_2) << REFERENCE << false << true << -1 << false;
    // With the noise the smallest stars are one point in from the end
    QList<QPair<int, double>> oneIn = FAR_SLOPE;
    std::swap(oneIn[9].second, oneIn[10].second);
    QTest::newRow("far, slope, lowest one in from the end") << points(oneIn) << REFERENCE << false << true << -1 << false;
    // Linear 1 Pass extended the run toward focus to 35 points, from 61,000 down to 54,200: the smallest stars are 4th
    // from the end, within its outer fifth
    QList<QPair<int, double>> walk;
    for (int i = 0; i < 35; i++)
    {
        const int position = 61000 - 200 * i;
        walk << qMakePair(position, 3.7 + 0.12 * (position - 54800) / 200.0 * (position >= 54800 ? 1 : -0.5));
    }
    // A good fit and a slope need ONE_SIDED_RATIO: 2.3 times is not enough. Linear 1 Pass runs out of steps there.
    QTest::newRow("far, long walk toward focus") << points(walk) << REFERENCE << false << false << -1 << false;
    // Further in: on the sky the smallest stars are often a few points in from the end
    QList<QPair<int, double>> twoIn = FAR_SLOPE;
    std::swap(twoIn[8].second, twoIn[10].second);
    QTest::newRow("far, slope, lowest two in from the end") << points(twoIn) << REFERENCE << false << true << -1 << false;
    // The same start a second time: flat in the noise, the curve fit failed
    QTest::newRow("far, flat, poor fit") << points(FAR_FLAT) << REFERENCE << true << true << 0 << false;
    QTest::newRow("far, flat, good fit") << points(FAR_FLAT) << REFERENCE << false << false << 0 << false;
    // A run around focus is never one, also with a poor fit
    QTest::newRow("in focus") << points(IN_FOCUS) << REFERENCE << false << false << 0 << true;
    QTest::newRow("in focus, poor fit") << points(IN_FOCUS) << REFERENCE << true << false << 0 << true;
    // Without a reference nothing is certain
    QTest::newRow("far, slope, no reference") << points(FAR_SLOPE) << -1.0 << true << false << -1 << false;
    // Seeing twice as bad as at the reference: the stars are larger, but the run has a V
    QList<QPair<int, double>> badSeeing;
    for (const auto &measure : IN_FOCUS)
        badSeeing << qMakePair(measure.first, measure.second * 2.0);
    QTest::newRow("in focus, seeing 2x worse") << points(badSeeing) << REFERENCE << true << false << 0 << true;
    // Focus near one end of a run with seeing twice as bad: the end rose again, so the run is not one-sided
    QList<QPair<int, double>> edge;
    for (const auto &measure : IN_FOCUS.mid(0, 8))
        edge << qMakePair(measure.first, measure.second * 2.0);
    QTest::newRow("in focus near the end, seeing 2x worse") << points(edge) << REFERENCE << false << false << -1 << false;
    // The sky: the far run with a good fit stopped at 55,627 before this check knew a slope from where its smallest
    // stars were; the same start without a fit, and a run far below focus, failed as "no curve fitted"
    QTest::newRow("sky: far, good fit") << points(SKY_FAR) << SKY_REFERENCE << false << true << -1 << false;
    QTest::newRow("sky: far, no fit") << points(SKY_FAR_NO_FIT) << SKY_REFERENCE << true << true << -1 << false;
    QTest::newRow("sky: below focus, no fit") << points(SKY_BELOW) << SKY_REFERENCE << true << true << 1 << false;
    QTest::newRow("sky: in focus") << points(SKY_IN_FOCUS) << SKY_REFERENCE << false << false << 0 << true;
    // Too few points to tell anything
    QTest::newRow("four points") << points(FAR_SLOPE.mid(0, 4)) << REFERENCE << true << false << 0 << false;
    // Donuts read too small: dropped as outliers, they do not hide that the run reached focus
    QList<QPair<int, double>> donuts = IN_FOCUS;
    donuts[0].second = 1.0;
    donuts[10].second = 1.1;
    QTest::newRow("in focus, donuts at the ends") << points(donuts, {50000, 25000}) << REFERENCE << false << false << 0 << true;
}

void TestFocusDiagnosis::testRunCheck()
{
    QFETCH(QList<FocusPoint>, run);
    QFETCH(double, reference);
    QFETCH(bool, r2Failed);
    QFETCH(bool, noMinimum);
    QFETCH(int, direction);
    QFETCH(bool, hasV);

    const auto check = FocusDiagnosis::checkRun(run, reference, r2Failed);
    QCOMPARE(check.noMinimum, noMinimum);
    if (check.points >= FocusDiagnosis::MIN_RUN_POINTS)
        QCOMPARE(check.direction, direction);
    QCOMPARE(check.hasV, hasV);
    if (reference > 0 && check.points >= FocusDiagnosis::MIN_RUN_POINTS)
        QVERIFY(qAbs(check.ratio - check.lowest / reference) < 1e-9);
}

void TestFocusDiagnosis::testReference()
{
    const QDateTime now(QDate(2026, 11, 16), QTime(22, 0), QTimeZone::utc());
    QList<FocusReference> history;
    QCOMPARE(FocusDiagnosis::referenceFor(history, 60, 1, now), -1.0);

    // The median of the recent runs
    for (double hfr :
            {
                1.6, 1.8, 3.0
            })
        history << FocusReference{hfr, 1, -1, now.addDays(-1)};
    QCOMPARE(FocusDiagnosis::referenceFor(history, -1, 1, now), 1.8);

    // Only references with the run's binning: an HFR does not convert. On the sky 0.95 at 2x2 was 1.35 at 1x1.
    QList<FocusReference> binned = {FocusReference{0.95, 2, -1, now.addDays(-1)}, FocusReference{0.97, 2, -1, now.addDays(-1)},
                                    FocusReference{0.94, 2, -1, now.addDays(-1)}, FocusReference{1.35, 1, -1, now.addDays(-1)}
                                   };
    QCOMPARE(FocusDiagnosis::referenceFor(binned, -1, 2, now), 0.95);
    QCOMPARE(FocusDiagnosis::referenceFor(binned, -1, 1, now), 1.35);
    QCOMPARE(FocusDiagnosis::referenceFor(binned, -1, 3, now), -1.0);
    // Only the most recent runs are kept
    for (int i = 0; i < 10; i++)
        history = FocusDiagnosis::addReference(history, FocusReference{2.0 + i, 1, -1, now.addDays(-1), "L"});
    QCOMPARE(history.size(), FocusDiagnosis::REFERENCE_RUNS);
    QCOMPARE(history.last().hfr, 11.0);

    // Low in the sky the seeing is worse: at 30 degrees, a reference taken at 70 degrees grows by (2/1.064)^0.6
    history = {FocusReference{2.0, 1, 70, now.addDays(-1)}};
    const double expected = 2.0 * std::pow(FocusDiagnosis::airmass(30) / FocusDiagnosis::airmass(70), 0.6);
    QVERIFY(qAbs(FocusDiagnosis::referenceFor(history, 30, 1, now) - expected) < 1e-9);
    QVERIFY(expected > 2.9 && expected < 3.0);
    // Without the altitude of either, no scaling
    QCOMPARE(FocusDiagnosis::referenceFor(history, -1, 1, now), 2.0);
    // Never down: higher in the sky the stars may be no smaller, as the optics or the pixels set their size
    history = {FocusReference{2.0, 1, 30, now.addDays(-1)}};
    QCOMPARE(FocusDiagnosis::referenceFor(history, 75, 1, now), 2.0);

    // Runs near the run's altitude count as measured, without the model. The sky, a refractor at 2x2: its stars were
    // as large at 75 degrees as at 31, which the model would have expected 37% smaller.
    const QList<FocusReference> sky =
    {
        FocusReference{0.95, 2, 31.3, now.addDays(-1)}, FocusReference{0.96, 2, 35.1, now.addDays(-1)},
        FocusReference{0.97, 2, 37.0, now.addDays(-1)}, FocusReference{0.90, 2, 74.9, now.addDays(-1)},
        FocusReference{0.97, 2, 60.0, now.addDays(-1)}
    };
    QCOMPARE(FocusDiagnosis::referenceFor(sky, 70, 2, now), 0.935);  // the runs at 60 and 74.9
    QCOMPARE(FocusDiagnosis::referenceFor(sky, 33, 2, now), 0.96);   // the runs at 31.3, 35.1 and 37
    // Nothing near 15 degrees: all, scaled up
    QVERIFY(FocusDiagnosis::referenceFor(sky, 15, 2, now) > 0.97);

    // Old runs are no reference: the optics may have changed since
    history = {FocusReference{2.0, 1, -1, now.addDays(-(FocusDiagnosis::REFERENCE_MAX_AGE_DAYS + 1))}};
    QCOMPARE(FocusDiagnosis::referenceFor(history, -1, 1, now), -1.0);
}

void TestFocusDiagnosis::testReferenceRuns()
{
    // A good run with a shallow V is kept, also without a reference yet
    QVERIFY(FocusDiagnosis::isReferenceRun(FocusDiagnosis::checkRun(points(SKY_IN_FOCUS), SKY_REFERENCE, false)));
    QVERIFY(FocusDiagnosis::isReferenceRun(FocusDiagnosis::checkRun(points(SKY_IN_FOCUS), -1, false)));
    QVERIFY(FocusDiagnosis::isReferenceRun(FocusDiagnosis::checkRun(points(IN_FOCUS), REFERENCE, false)));

    // Never a run that stopped away from focus, with or without a reference to tell it: on the sky one such run
    // became a reference 4 times too large, and hid the next one
    for (const double reference :
            {
                SKY_REFERENCE, -1.0
            })
    {
        QVERIFY(!FocusDiagnosis::isReferenceRun(FocusDiagnosis::checkRun(points(SKY_FAR), reference, false)));
        QVERIFY(!FocusDiagnosis::isReferenceRun(FocusDiagnosis::checkRun(points(SKY_BELOW), reference, false)));
    }
    QVERIFY(!FocusDiagnosis::isReferenceRun(FocusDiagnosis::checkRun(points(FAR_FLAT), -1, false)));
    QVERIFY(!FocusDiagnosis::isReferenceRun(FocusDiagnosis::checkRun(points(FAR_SLOPE), -1, false)));
    // Nor one with a poor fit
    QVERIFY(!FocusDiagnosis::isReferenceRun(FocusDiagnosis::checkRun(points(SKY_IN_FOCUS), -1, true)));
}

void TestFocusDiagnosis::testLastGoodTarget()
{
    auto facts = healthy();
    facts.focuserMin = 0;
    facts.focuserMax = 100000;
    QCOMPARE(FocusDiagnosis::lastGoodTarget(facts), -1);

    // Without Adaptive Focus settings: where the last good run ended
    facts.lastGoodPosition = 36700;
    facts.lastGoodTemperature = 8.5;
    facts.lastGoodAltitude = 50;
    facts.temperature = 6.5;
    facts.altitude = 60;
    QCOMPARE(FocusDiagnosis::lastGoodTarget(facts), 36700);

    // Moved as Adaptive Focus does: -10 steps per degree C, 2 degrees colder; 2 steps per degree, 10 degrees higher
    facts.ticksPerTemp = -10;
    QCOMPARE(FocusDiagnosis::lastGoodTarget(facts), 36720);
    facts.ticksPerAlt = 2;
    QCOMPARE(FocusDiagnosis::lastGoodTarget(facts), 36740);

    // An unknown reading moves nothing
    auto noTemperature = facts;
    noTemperature.temperature = -1000;
    QCOMPARE(FocusDiagnosis::lastGoodTarget(noTemperature), 36720);
    auto noAltitude = facts;
    noAltitude.lastGoodAltitude = -1;
    QCOMPARE(FocusDiagnosis::lastGoodTarget(noAltitude), 36720);

    // Within the focuser range
    auto edge = facts;
    edge.lastGoodPosition = 99990;
    QCOMPARE(FocusDiagnosis::lastGoodTarget(edge), 100000);

    // The report moves the focuser there, and says why
    facts.reason = FOCUS_FAIL_NO_MINIMUM;
    facts.canAbsMove = true;
    facts.startPosition = 60000;
    facts.stepSize = 200;
    facts.outSteps = 5;
    facts.filter = "Luminance";
    const Report report = FocusDiagnosis::diagnose(facts);
    const Item *item = findItem(report, "focus_goto_last_good");
    QVERIFY(item);
    QVERIFY2(item->title.contains(QLocale().toString(36740)), qPrintable(item->title));
    QVERIFY2(item->why.contains(QLocale().toString(36700)) && item->why.contains(QLocale().toString(36740)),
             qPrintable(item->why));
}

void TestFocusDiagnosis::testNoMinimum()
{
    auto facts = withReason(FOCUS_FAIL_NO_MINIMUM);
    facts.startPosition = 60000;
    facts.position = 60000;
    facts.stepSize = 200;
    facts.outSteps = 5;
    facts.filter = "Luminance";
    facts.lastGoodPosition = 36700;
    facts.lastGoodTemperature = 8.5;
    facts.run = FocusDiagnosis::checkRun(points(FAR_SLOPE), REFERENCE, false);
    QVERIFY(facts.run->noMinimum);

    Report report = FocusDiagnosis::diagnose(facts);
    QCOMPARE(report.reasonKey, QString("no_minimum"));
    QVERIFY2(report.summary.contains("3.3 times"), qPrintable(report.summary));
    QCOMPARE(ids(report), QStringList({"focus_goto_last_good", "focus_near_focus", "focus_step_raise", "focus_advisor"}));

    const Item *item = findItem(report, "focus_goto_last_good");
    QCOMPARE(item->kind, Kind::Auto);
    QCOMPARE(item->likelihood, Likelihood::Likely);
    QVERIFY(item->cost.testFlag(MovesHardware));
    QVERIFY(!item->autoRetry);
    QVERIFY2(item->title.contains(QLocale().toString(36700)), qPrintable(item->title));
    QVERIFY2(item->why.contains("Luminance") && item->why.contains(QLocale().toString(23300) + " steps away")
             && item->why.contains("8.5"),
             qPrintable(item->why));
    item = findItem(report, "focus_near_focus");
    QVERIFY2(item->why.contains("focus lies below"), qPrintable(item->why));

    QHash<QString, Evidence> byKey;
    for (const auto &evidence : report.evidence)
        byKey.insert(evidence.key, evidence);
    QCOMPARE(byKey["hfr_ratio"].level, Evidence::Bad);
    QCOMPARE(byKey["run_range"].value.toString(), QString("59000 - 61000"));

    // The last good focus inside what the run measured is no help
    facts.lastGoodPosition = 60100;
    QVERIFY(!ids(FocusDiagnosis::diagnose(facts)).contains("focus_goto_last_good"));
    // Unknown: the run tells only the direction
    facts.lastGoodPosition = -1;
    QCOMPARE(ids(FocusDiagnosis::diagnose(facts)).first(), QString("focus_near_focus"));

    // A curve failure that started far from the last good focus suggests it too, as a possible cause
    facts = withReason(FOCUS_FAIL_R2);
    facts.startPosition = 60000;
    facts.stepSize = 200;
    facts.outSteps = 5;
    facts.lastGoodPosition = 36700;
    report = FocusDiagnosis::diagnose(facts);
    item = findItem(report, "focus_goto_last_good");
    QVERIFY(item);
    QCOMPARE(item->likelihood, Likelihood::Possible);
    // Near it: not offered
    facts.startPosition = 36900;
    QVERIFY(!ids(FocusDiagnosis::diagnose(facts)).contains("focus_goto_last_good"));
}

void TestFocusDiagnosis::testSettingsChecks()
{
    // Linear 1 Pass, 5 steps of 2500 out: 12,500 steps of travel needed
    auto facts = healthy();
    facts.outSteps = 5;
    facts.stepSize = 2500;
    facts.maxTravel = 10000;
    Report report = FocusDiagnosis::checkup(facts);
    QCOMPARE(ids(report), QStringList({"focus_max_travel_fit"}));
    QVERIFY2(report.items.first().title.contains(QLocale().toString(12500)), qPrintable(report.items.first().title));
    QCOMPARE(FocusDiagnosis::neededMaxTravel(facts), 12500);
    facts.maxTravel = 12500;
    QVERIFY(FocusDiagnosis::checkup(facts).items.isEmpty());

    // Driver backlash and AF Overscan both set: corrected twice
    facts.driverBacklashSupported = true;
    facts.driverBacklash = 50;
    facts.afOverscan = 100;
    facts.maxTravel = 20000;
    report = FocusDiagnosis::checkup(facts);
    QCOMPARE(ids(report), QStringList({"focus_backlash_twice"}));
    QCOMPARE(report.items.first().kind, Kind::Manual);
    QVERIFY2(report.items.first().why.contains("50") && report.items.first().why.contains("100"),
             qPrintable(report.items.first().why));
    // One of them is fine; a driver without backlash compensation too
    facts.afOverscan = 0;
    QVERIFY(FocusDiagnosis::checkup(facts).items.isEmpty());
    facts.afOverscan = 100;
    facts.driverBacklashSupported = false;
    QVERIFY(FocusDiagnosis::checkup(facts).items.isEmpty());

    // A curve failure reports them as possible causes
    facts = withReason(FOCUS_FAIL_FLUCTUATIONS);
    facts.outSteps = 5;
    facts.stepSize = 2500;
    facts.maxTravel = 10000;
    QCOMPARE(findItem(FocusDiagnosis::diagnose(facts), "focus_max_travel_fit")->likelihood, Likelihood::Possible);
}

QTEST_GUILESS_MAIN(TestFocusDiagnosis)

#include "testfocusdiagnosis.moc"
