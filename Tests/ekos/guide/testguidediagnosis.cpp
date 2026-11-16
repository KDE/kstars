/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

/*
 * Tests for the Guide failure rules: one data row per rule, facts in, item ids and order out. Calibration failures
 * point at the mount first (balance, clutches, backlash, cables), settings second. Also the suggested values and
 * their limits, the reason keys, the evidence and the check-up when nothing failed.
 */

#include "ekos/guide/guidediagnosis.h"

#include <QSet>
#include <QTest>

using namespace Ekos;
using namespace Ekos::Diagnosis;

Q_DECLARE_METATYPE(Ekos::GuideFacts)

class TestGuideDiagnosis : public QObject
{
        Q_OBJECT

    private slots:
        void testRules_data();
        void testRules();
        void testItemDetails();
        void testSuggestions();
        void testReasonKeys();
        void testEvidence();
        void testCheckup();

    private:
        /** Facts of a working internal guider setup that guides through the mount. */
        static GuideFacts healthy();
        static GuideFacts withReason(GuideFailReason reason);
        static QStringList ids(const Report &report);
};

GuideFacts TestGuideDiagnosis::healthy()
{
    GuideFacts facts;
    facts.train = "Guide";
    facts.guiderType = GuideDiagnosis::GUIDER_INTERNAL;
    facts.hasCamera = true;
    facts.cameraConnected = true;
    facts.cameraName = "Guide Simulator";
    facts.canBin = true;
    facts.hasGuider = true;
    facts.guiderConnected = true;
    facts.guiderName = "Telescope Simulator";
    facts.hasMount = true;
    facts.mountConnected = true;
    facts.focalLength = 400;
    facts.aperture = 80;
    facts.pixelW = 5.2;
    facts.pixelH = 5.2;
    facts.exposure = 1;
    facts.binIndex = 0;
    facts.binCount = 4;
    facts.squareSize = 16;
    facts.largerSquareSize = 32;
    facts.calibrationPulse = 1000;
    facts.lostStarTimeout = 60;
    facts.ditherMaxIterations = 10;
    facts.ditherFailAborts = true;
    return facts;
}

GuideFacts TestGuideDiagnosis::withReason(GuideFailReason reason)
{
    GuideFacts facts = healthy();
    facts.reason = reason;
    return facts;
}

QStringList TestGuideDiagnosis::ids(const Report &report)
{
    QStringList list;
    for (const auto &item : report.items)
        list << item.id;
    return list;
}

void TestGuideDiagnosis::testRules_data()
{
    QTest::addColumn<GuideFacts>("facts");
    QTest::addColumn<QStringList>("expected");

    // Calibration: the mount first
    GuideFacts facts = withReason(GuideFailReason::CalibrationRaDriftTooShort);
    QTest::newRow("RA drift too short") << facts << QStringList({"check_ra_axis", "guide_pulse_raise"});

    facts = withReason(GuideFailReason::CalibrationDecDriftTooShort);
    QTest::newRow("DEC drift too short") << facts << QStringList({"check_dec_axis", "guide_dec_backlash_on", "guide_pulse_raise"});

    facts = withReason(GuideFailReason::CalibrationDecDriftTooShort);
    facts.decBacklashRemoval = true;
    facts.calibrationPulse = GuideDiagnosis::MAX_CALIBRATION_PULSE;
    QTest::newRow("DEC drift too short, nothing left to change") << facts << QStringList({"check_dec_axis"});

    facts = withReason(GuideFailReason::CalibrationDriftTooShort);
    QTest::newRow("drift too short, axis unknown") << facts
            << QStringList({"check_ra_axis", "check_dec_axis", "guide_pulse_raise"});

    facts = withReason(GuideFailReason::CalibrationRaNoReturn);
    QTest::newRow("RA does not come back") << facts << QStringList({"check_backlash"});

    facts = withReason(GuideFailReason::CalibrationDecNoReturn);
    QTest::newRow("DEC does not come back") << facts << QStringList({"check_backlash", "guide_dec_backlash_on"});

    // PHD2 has its own settings: only the mount
    facts = withReason(GuideFailReason::CalibrationRaDriftTooShort);
    facts.guiderType = GuideDiagnosis::GUIDER_PHD2;
    QTest::newRow("RA drift too short, PHD2") << facts << QStringList({"check_ra_axis"});

    facts = withReason(GuideFailReason::Phd2CalibrationFailed);
    facts.guiderType = GuideDiagnosis::GUIDER_PHD2;
    facts.detail = "RA calibration failed: star did not move enough";
    QTest::newRow("PHD2 calibration failed") << facts << QStringList({"check_phd2", "check_ra_axis", "check_dec_axis"});

    facts = withReason(GuideFailReason::CalibrationStarLost);
    QTest::newRow("star lost during calibration") << facts
            << QStringList({"guide_pulse_lower", "guide_exposure_raise", "guide_square_larger"});

    facts = withReason(GuideFailReason::CalibrationStarLost);
    facts.image = ImageStats();
    facts.image->verdict = ImageVerdict::FewStars;
    facts.image->stars = 1;
    QTest::newRow("star lost during calibration, few stars") << facts
            << QStringList({"check_guide_sky", "guide_pulse_lower", "guide_exposure_raise", "guide_square_larger"});

    facts = withReason(GuideFailReason::CalibrationStartFailed);
    facts.focalLength = 0;
    QTest::newRow("calibration cannot start, no optics") << facts << QStringList({"train_set_optics"});

    facts = withReason(GuideFailReason::CalibrationStartFailed);
    facts.hasCamera = false;
    facts.hasGuider = false;
    QTest::newRow("calibration cannot start, empty train") << facts << QStringList({"train_select_camera", "train_select_guider"});

    // Guiding
    facts = withReason(GuideFailReason::NoGuideStar);
    QTest::newRow("no guide star") << facts << QStringList({"guide_exposure_raise", "guide_bin_2x2", "check_guide_sky"});

    facts = withReason(GuideFailReason::NoGuideStar);
    facts.image = ImageStats();
    facts.image->verdict = ImageVerdict::OutOfFocus;
    facts.image->hfr = 6;
    QTest::newRow("no guide star, out of focus") << facts
            << QStringList({"guide_exposure_raise", "guide_focus", "guide_bin_2x2", "check_guide_sky"});

    facts = withReason(GuideFailReason::StarLost);
    QTest::newRow("star lost") << facts
                               << QStringList({"check_guide_sky", "check_guide_cables", "guide_lost_star_timeout_raise", "guide_square_larger"});

    facts = withReason(GuideFailReason::StarLost);
    facts.guiderType = GuideDiagnosis::GUIDER_PHD2;
    QTest::newRow("star lost, PHD2") << facts << QStringList({"check_guide_sky", "check_guide_cables"});

    facts = withReason(GuideFailReason::CaptureTimeout);
    facts.cameraConnected = false;
    QTest::newRow("capture timeout, camera disconnected") << facts << QStringList({"camera_connect", "check_camera_usb_power"});

    facts = withReason(GuideFailReason::CameraLost);
    QTest::newRow("camera lost") << facts << QStringList({"check_camera_usb_power"});

    facts = withReason(GuideFailReason::DitherFailed);
    QTest::newRow("dither failed") << facts << QStringList({"check_dither_settle", "dither_iterations_raise", "dither_continue"});
}

void TestGuideDiagnosis::testRules()
{
    QFETCH(GuideFacts, facts);
    QFETCH(QStringList, expected);

    const Report report = GuideDiagnosis::diagnose(facts);
    QCOMPARE(ids(report), expected);

    QCOMPARE(report.module, QString("guide"));
    QCOMPARE(report.train, QString("Guide"));
    QCOMPARE(report.reason, static_cast<int>(facts.reason));
    QCOMPARE(report.reasonKey, guideFailReasonKey(facts.reason));
    QVERIFY(!report.title.isEmpty());

    for (const auto &item : report.items)
    {
        QVERIFY2(!item.title.isEmpty(), qPrintable(item.id));
        QVERIFY2(!item.why.isEmpty(), qPrintable(item.id));
        if (item.kind == Kind::Manual)
            QVERIFY2(!item.steps.isEmpty(), qPrintable(item.id));
        if (item.kind == Kind::Open)
            QVERIFY2(!item.openTarget.isEmpty(), qPrintable(item.id));
        // "Done, try again" restarts calibration with guide pulses only: nothing here slews or parks the mount
        QVERIFY2(!item.cost.testFlag(MovesHardware), qPrintable(item.id));
        QVERIFY2(item.autoRetry, qPrintable(item.id));
    }
}

void TestGuideDiagnosis::testItemDetails()
{
    auto facts = withReason(GuideFailReason::CalibrationRaDriftTooShort);
    facts.failPulse = 2000;
    facts.failDrift = 0.3;
    Report report = GuideDiagnosis::diagnose(facts);
    const Item *item = findItem(report, "check_ra_axis");
    QVERIFY(item);
    QCOMPARE(item->kind, Kind::Manual);
    QCOMPARE(item->likelihood, Likelihood::Likely);
    QVERIFY2(item->why.contains("2000 ms") && item->why.contains("0.3 pixels"), qPrintable(item->why));
    const QString steps = item->steps.join(" ");
    QVERIFY(steps.contains("RA balance"));
    QVERIFY(steps.contains("clutch"));
    QVERIFY(steps.contains("power supply"));
    // Through the mount: no ST4 step
    QVERIFY(!steps.contains("ST4"));

    item = findItem(report, "guide_pulse_raise");
    QVERIFY(item);
    QCOMPARE(item->likelihood, Likelihood::Possible);
    QVERIFY(item->cost.testFlag(ChangesSettings));
    QVERIFY2(item->title.contains("2000 ms"), qPrintable(item->title));

    // Through the camera, the ST4 cable is a suspect
    facts.guideViaCamera = true;
    facts.guiderName = facts.cameraName;
    report = GuideDiagnosis::diagnose(facts);
    QVERIFY(findItem(report, "check_ra_axis")->steps.join(" ").contains("ST4 cable"));

    // A star that did not come back points at the backlash of that axis
    facts = withReason(GuideFailReason::CalibrationDecNoReturn);
    facts.failDrift = 4.2;
    report = GuideDiagnosis::diagnose(facts);
    item = findItem(report, "check_backlash");
    QVERIFY(item);
    QVERIFY2(item->title.contains("DEC"), qPrintable(item->title));
    QVERIFY2(item->why.contains("4.2 pixels"), qPrintable(item->why));
    QCOMPARE(findItem(report, "guide_dec_backlash_on")->likelihood, Likelihood::Likely);

    // PHD2's own reason is shown
    facts = withReason(GuideFailReason::Phd2CalibrationFailed);
    facts.guiderType = GuideDiagnosis::GUIDER_PHD2;
    facts.detail = "star did not move enough";
    report = GuideDiagnosis::diagnose(facts);
    QVERIFY(report.summary.contains("star did not move enough"));
    QVERIFY(findItem(report, "check_phd2")->why.contains("star did not move enough"));
    QCOMPARE(findItem(report, "check_ra_axis")->likelihood, Likelihood::Possible);
}

void TestGuideDiagnosis::testSuggestions()
{
    QCOMPARE(GuideDiagnosis::raisedPulse(1000), 2000);
    QCOMPARE(GuideDiagnosis::raisedPulse(2000), GuideDiagnosis::MAX_CALIBRATION_PULSE);
    QCOMPARE(GuideDiagnosis::raisedPulse(GuideDiagnosis::MAX_CALIBRATION_PULSE), 0);
    QCOMPARE(GuideDiagnosis::loweredPulse(1000), 500);
    QCOMPARE(GuideDiagnosis::loweredPulse(150), GuideDiagnosis::MIN_CALIBRATION_PULSE);
    QCOMPARE(GuideDiagnosis::loweredPulse(GuideDiagnosis::MIN_CALIBRATION_PULSE), 0);
    QCOMPARE(GuideDiagnosis::raisedExposure(0.5), 1.0);
    QCOMPARE(GuideDiagnosis::raisedExposure(2), GuideDiagnosis::MAX_GUIDE_EXPOSURE);
    QCOMPARE(GuideDiagnosis::raisedExposure(GuideDiagnosis::MAX_GUIDE_EXPOSURE), 0.0);
    QCOMPARE(GuideDiagnosis::raisedLostStarTimeout(10), 30.0);
    QCOMPARE(GuideDiagnosis::raisedLostStarTimeout(60), GuideDiagnosis::MAX_LOST_STAR_TIMEOUT);
    QCOMPARE(GuideDiagnosis::raisedLostStarTimeout(GuideDiagnosis::MAX_LOST_STAR_TIMEOUT), 0.0);
    QCOMPARE(GuideDiagnosis::raisedDitherIterations(10), 20);
    QCOMPARE(GuideDiagnosis::raisedDitherIterations(20), GuideDiagnosis::MAX_DITHER_ITERATIONS);
    QCOMPARE(GuideDiagnosis::raisedDitherIterations(GuideDiagnosis::MAX_DITHER_ITERATIONS), 0);

    // Nothing left to change: the setting items go
    auto facts = withReason(GuideFailReason::StarLost);
    facts.lostStarTimeout = GuideDiagnosis::MAX_LOST_STAR_TIMEOUT;
    facts.largerSquareSize = 0;
    QCOMPARE(ids(GuideDiagnosis::diagnose(facts)), QStringList({"check_guide_sky", "check_guide_cables"}));
}

void TestGuideDiagnosis::testReasonKeys()
{
    const QList<GuideFailReason> reasons({GuideFailReason::CalibrationRaDriftTooShort, GuideFailReason::CalibrationDecDriftTooShort,
                                          GuideFailReason::CalibrationDriftTooShort, GuideFailReason::CalibrationRaNoReturn,
                                          GuideFailReason::CalibrationDecNoReturn, GuideFailReason::CalibrationStarLost,
                                          GuideFailReason::CalibrationStartFailed, GuideFailReason::NoGuideStar, GuideFailReason::StarLost,
                                          GuideFailReason::CaptureTimeout, GuideFailReason::CameraLost, GuideFailReason::DitherFailed,
                                          GuideFailReason::Phd2CalibrationFailed});
    QSet<QString> keys({guideFailReasonKey(GuideFailReason::None)});
    for (const auto reason : reasons)
    {
        const QString key = guideFailReasonKey(reason);
        QVERIFY2(!keys.contains(key), qPrintable(key));
        keys << key;
        QVERIFY(GuideDiagnosis::isDiagnosable(reason));
        // A title of its own and at least one item
        const Report report = GuideDiagnosis::diagnose(withReason(reason));
        QVERIFY2(report.title != QString("Guiding failed"), qPrintable(key));
        QVERIFY2(!report.items.isEmpty(), qPrintable(key));
    }
    // An error state without a reason, e.g. after Stop, gets no diagnosis
    QVERIFY(!GuideDiagnosis::isDiagnosable(GuideFailReason::None));

    // The keys and values go out on D-Bus and EkosLive: they do not change
    QCOMPARE(guideFailReasonKey(GuideFailReason::CalibrationRaDriftTooShort), QString("calibration_ra_drift_too_short"));
    QCOMPARE(guideFailReasonKey(GuideFailReason::StarLost), QString("star_lost"));
    QCOMPARE(static_cast<int>(GuideFailReason::Phd2CalibrationFailed), 13);
}

void TestGuideDiagnosis::testEvidence()
{
    auto facts = withReason(GuideFailReason::CalibrationRaDriftTooShort);
    facts.failPulse = 2000;
    facts.failDrift = 0.3;
    Report report = GuideDiagnosis::diagnose(facts);
    QHash<QString, Evidence> byKey;
    for (const auto &evidence : report.evidence)
        byKey.insert(evidence.key, evidence);
    QCOMPARE(byKey["drift"].value.toString(), QString("0.3"));
    QCOMPARE(byKey["drift"].level, Evidence::Bad);
    QCOMPARE(byKey["pulse"].value.toInt(), 2000);
    QCOMPARE(byKey["guide_via"].value.toString(), QString("Telescope Simulator"));
    QVERIFY(report.ruledOut.join(" ").contains("mount is connected"));

    facts = withReason(GuideFailReason::NoGuideStar);
    report = GuideDiagnosis::diagnose(facts);
    byKey.clear();
    for (const auto &evidence : report.evidence)
        byKey.insert(evidence.key, evidence);
    QCOMPARE(byKey["exposure"].value.toDouble(), 1.0);
    QCOMPARE(byKey["square"].value.toInt(), 16);
    QVERIFY(report.notChecked.join(" ").contains("guide frame"));
}

void TestGuideDiagnosis::testCheckup()
{
    auto facts = healthy();
    Report report = GuideDiagnosis::checkup(facts);
    QVERIFY(isCheckup(report));
    QCOMPARE(report.module, QString("guide"));
    QVERIFY(report.items.isEmpty());
    QCOMPARE(report.title, QString("Guide looks ready"));
    QVERIFY(report.ruledOut.join(" ").contains("Guide Simulator is connected"));
    QVERIFY(report.ruledOut.join(" ").contains("Guide pulses go to Telescope Simulator"));

    // The guide rate the last calibration measured: 0.5x sidereal is fine, 0.1x barely moves the star
    facts.decRate = 0.5 * GuideDiagnosis::SIDEREAL_RATE;
    report = GuideDiagnosis::checkup(facts);
    QVERIFY(report.items.isEmpty());
    QVERIFY(report.ruledOut.join(" ").contains("0.50x sidereal"));
    facts.decRate = 0.1 * GuideDiagnosis::SIDEREAL_RATE;
    report = GuideDiagnosis::checkup(facts);
    QCOMPARE(ids(report), QStringList({"check_guide_rate"}));
    QVERIFY(report.items.first().why.contains("0.10x"));

    facts = healthy();
    facts.hasCamera = false;
    facts.hasGuider = false;
    facts.focalLength = 0;
    facts.mountConnected = false;
    report = GuideDiagnosis::checkup(facts);
    QCOMPARE(ids(report), QStringList({"train_select_camera", "train_select_guider", "mount_connect", "train_set_optics"}));
    for (const auto &item : report.items)
        QVERIFY2(!item.autoRetry, qPrintable(item.id));

    // PHD2 uses its own camera and optics
    facts.guiderType = GuideDiagnosis::GUIDER_PHD2;
    QCOMPARE(ids(GuideDiagnosis::checkup(facts)), QStringList({"mount_connect"}));

    // Dismissed items are not offered
    facts = healthy();
    facts.cameraConnected = false;
    QVERIFY(GuideDiagnosis::checkup(facts, {"camera_connect"}).items.isEmpty());
}

QTEST_GUILESS_MAIN(TestGuideDiagnosis)

#include "testguidediagnosis.moc"
