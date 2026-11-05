/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

/*
 * Tests for the Align failure rules: one data row per rule of the catalog, facts in, item ids and order out.
 * Also the evidence, the ruled out and not checked lines, the tried and dismissed filtering and the reason keys.
 */

#include "ekos/align/aligndiagnosis.h"
#include "ekos/align/indexfiles.h"

#include <QJsonArray>
#include <QSet>
#include <QTest>
#include <QTimeZone>

using namespace Ekos;
using namespace Ekos::Diagnosis;

Q_DECLARE_METATYPE(Ekos::AlignFacts)

class TestAlignDiagnosis : public QObject
{
        Q_OBJECT

    private slots:
        void testRules_data();
        void testRules();
        void testItemDetails();
        void testTriedAndDismissed();
        void testFewStarsAfterTries();
        void testRuledOut();
        void testClock();
        void testEvidence();
        void testReasonKeys();

    private:
        /** Facts of a working setup: every check passes. */
        static AlignFacts healthy();
        static AlignFacts withReason(AlignFailReason reason);
        static QStringList ids(const Report &report);
};

AlignFacts TestAlignDiagnosis::healthy()
{
    AlignFacts facts;
    facts.state = 2;
    facts.train = "Primary";
    facts.hasCamera = true;
    facts.cameraConnected = true;
    facts.cameraName = "CCD Simulator";
    facts.hasMount = true;
    facts.mountConnected = true;
    facts.mountCoordsValid = true;
    facts.canSync = true;
    facts.gotoMode = 0;
    facts.focalLength = 800;
    facts.aperture = 100;
    facts.reducer = 1;
    facts.pixelW = 3.76;
    facts.pixelH = 3.76;
    facts.width = 6000;
    facts.height = 4000;
    facts.fovW = 97;
    facts.fovH = 65;
    facts.exposure = 5;
    facts.binIndex = 0;
    facts.binCount = 4;
    facts.canBin = true;
    facts.localSolver = true;
    facts.indexSolver = true;
    facts.profile = AlignDiagnosis::defaultProfileName();
    facts.useScale = true;
    facts.usePosition = true;
    facts.timeoutSec = 180;
    facts.requiredSizes = IndexFiles::requiredIndexSizes(97);
    facts.installedSizes = facts.requiredSizes;
    facts.kstarsUtc = QDateTime(QDate(2026, 11, 4), QTime(21, 0, 0), QTimeZone::utc());
    facts.systemUtc = facts.kstarsUtc;
    facts.clientUtc = facts.kstarsUtc;
    // KStars sent its time to the mount an hour ago, and the mount reported it back
    facts.mountUtcReceived = facts.kstarsUtc.addSecs(-3600);
    facts.mountUtc = facts.mountUtcReceived;

    ImageStats image;
    image.verdict = ImageVerdict::Good;
    image.stars = 80;
    image.profiledStars = 30;
    image.hfr = 2;
    image.concentration = 3.1;
    image.elongation = 1.1;
    facts.image = image;

    AlignAttempt attempt;
    attempt.starsFound = 80;
    facts.attempts << attempt;
    return facts;
}

AlignFacts TestAlignDiagnosis::withReason(AlignFailReason reason)
{
    AlignFacts facts = healthy();
    facts.reason = reason;
    return facts;
}

QStringList TestAlignDiagnosis::ids(const Report &report)
{
    QStringList list;
    for (const auto &item : report.items)
        list << item.id;
    return list;
}

void TestAlignDiagnosis::testRules_data()
{
    QTest::addColumn<AlignFacts>("facts");
    QTest::addColumn<QStringList>("expected");

    AlignFacts facts;

    // Hardware and connection
    facts = withReason(AlignFailReason::NoCamera);
    facts.hasCamera = false;
    QTest::newRow("AL13 no camera") << facts << QStringList({"train_select_camera"});

    facts = withReason(AlignFailReason::CameraDisconnected);
    facts.cameraConnected = false;
    QTest::newRow("AL8 camera disconnected") << facts << QStringList({"camera_connect"});

    facts = withReason(AlignFailReason::NoMountCoords);
    facts.hasMount = false;
    QTest::newRow("AL6 no mount") << facts << QStringList({"train_select_mount"});

    facts = withReason(AlignFailReason::NoMountCoords);
    facts.mountConnected = false;
    QTest::newRow("AL6 mount disconnected") << facts << QStringList({"mount_connect"});

    facts = withReason(AlignFailReason::NoMountCoords);
    facts.mountCoordsValid = false;
    QTest::newRow("AL6 mount without coordinates") << facts << QStringList({"mount_check_coords"});

    facts = withReason(AlignFailReason::MountParked);
    facts.mountParked = true;
    QTest::newRow("row 4 mount parked") << facts << QStringList({"mount_unpark"});

    facts = withReason(AlignFailReason::SlewRejected);
    facts.altLimitsEnabled = true;
    facts.altMin = 20;
    facts.altMax = 85;
    facts.targetAlt = 10;
    QTest::newRow("AL10 below altitude limit") << facts << QStringList({"slew_alt_limit"});

    facts = withReason(AlignFailReason::SlewRejected);
    facts.altLimitsEnabled = true;
    facts.altMin = 20;
    facts.altMax = 85;
    facts.targetAlt = 45;
    QTest::newRow("AL10 rejected inside limits") << facts << QStringList({"slew_rejected_check"});

    facts = withReason(AlignFailReason::SlewAlert);
    facts.slewErrors = 3;
    QTest::newRow("AL10 slew alerts") << facts << QStringList({"slew_rejected_by_mount"});

    facts = withReason(AlignFailReason::SyncAlert);
    facts.slewErrors = 3;
    facts.canSync = false;
    QTest::newRow("row 32 sync alerts, no sync") << facts << QStringList({"slew_rejected_by_mount", "mount_no_sync"});

    facts = withReason(AlignFailReason::BlobDisabled);
    QTest::newRow("row 10 image transfer") << facts << QStringList({"camera_enable_blob"});

    facts = withReason(AlignFailReason::DustCapError);
    QTest::newRow("row 11 dust cap") << facts << QStringList({"remove_dust_cap"});

    facts = withReason(AlignFailReason::FilterWheelDisconnected);
    facts.hasFilterWheel = true;
    QTest::newRow("row 12 filter wheel disconnected") << facts << QStringList({"filter_connect"});

    facts = withReason(AlignFailReason::FilterChangeFailed);
    QTest::newRow("row 12 filter change") << facts << QStringList({"filter_test_move"});

    facts = withReason(AlignFailReason::CaptureError);
    facts.captureErrors = 3;
    QTest::newRow("row 13 capture errors") << facts << QStringList({"check_camera_usb_power"});

    facts = withReason(AlignFailReason::CaptureTimeout);
    facts.driverRestarts = 3;
    QTest::newRow("row 13 capture timeout") << facts << QStringList({"check_camera_usb_power"});

    facts = withReason(AlignFailReason::RotatorTimeout);
    facts.hasRotator = true;
    QTest::newRow("AL12 rotator") << facts << QStringList({"rotator_check"});

    facts = withReason(AlignFailReason::RemoteSolverTimeout);
    facts.localSolver = false;
    QTest::newRow("remote solver") << facts << QStringList({"remote_solver_check"});

    // Configuration
    facts = withReason(AlignFailReason::MissingOptics);
    facts.focalLength = 0;
    QTest::newRow("AL14 optics") << facts << QStringList({"train_set_optics"});

    facts = withReason(AlignFailReason::MissingPixelSize);
    facts.pixelW = facts.pixelH = -1;
    QTest::newRow("AL15 pixel size") << facts << QStringList({"camera_set_pixel_size"});

    facts = withReason(AlignFailReason::FovOutOfBounds);
    facts.pixelW = facts.pixelH = 0.001;
    facts.fovW = 0.03;
    facts.fovH = 0.02;
    // Both values look plausible, so both are suspects
    QTest::newRow("row 16 FOV out of bounds") << facts << QStringList({"train_set_optics", "camera_set_pixel_size"});

    facts = withReason(AlignFailReason::FovOutOfBounds);
    facts.focalLength = 0;
    facts.fovW = facts.fovH = 200 * 60;
    facts.pixelW = facts.pixelH = 0;
    QTest::newRow("row 16 FOV, both wrong") << facts << QStringList({"train_set_optics", "camera_set_pixel_size"});

    facts = withReason(AlignFailReason::FovOutOfBounds);
    facts.pixelW = facts.pixelH = 0;
    facts.fovW = facts.fovH = 0;
    QTest::newRow("row 16 FOV, pixel size 0") << facts << QStringList({"camera_set_pixel_size"});

    facts = withReason(AlignFailReason::SolverFailed);
    facts.solverType = 2;
    facts.indexSolver = false;
    facts.externalSolverMissing = true;
    QTest::newRow("row 19 external solver missing") << facts << QStringList({"solver_check_external"});

    // Data the solver needs
    facts = withReason(AlignFailReason::SolverFailed);
    facts.installedSizes.clear();
    {
        QStringList expected;
        for (int size : facts.requiredSizes)
            expected << QString("index_install_%1").arg(size, 2, 10, QLatin1Char('0'));
        QTest::newRow("AL9 no index files") << facts << expected;
    }

    // An 18 x 14 arcmin field with only the larger of its index sizes installed: the smaller ones are missing
    facts = withReason(AlignFailReason::SolverFailed);
    facts.fovW = 18.4;
    facts.fovH = 13.8;
    facts.requiredSizes = IndexFiles::requiredIndexSizes(18.4);
    facts.installedSizes = {5, 6, 7, 8, 9, 10};
    QTest::newRow("AL9 index sizes partly installed") << facts
            << QStringList({"index_install_03", "index_install_04", "fov_narrow"});

    facts = withReason(AlignFailReason::SolverFailed);
    facts.exposure = 1;
    facts.image->verdict = ImageVerdict::FewStars;
    facts.image->stars = 4;
    QTest::newRow("AL2 AL1 few stars") << facts << QStringList({"align_exposure_5s", "align_bin_max"});

    facts = withReason(AlignFailReason::SolverFailed);
    facts.exposure = 1;
    facts.image.reset();
    facts.attempts.clear();
    QTest::newRow("AL2 AL1 stars unknown") << facts << QStringList({"align_exposure_5s", "align_bin_max"});

    facts = withReason(AlignFailReason::SolverFailed);
    facts.image->verdict = ImageVerdict::FewStars;
    facts.image->stars = 4;
    facts.binIndex = 3;
    QTest::newRow("row 26 few stars, nothing left to raise") << facts << QStringList({"check_sky_and_cap"});

    facts = withReason(AlignFailReason::SolverTimeout);
    facts.attempts.last().timedOut = true;
    QTest::newRow("row 22 solver timeout") << facts << QStringList({"solver_timeout_raise"});

    facts = withReason(AlignFailReason::SolverTimeout);
    facts.timeoutSec = AlignDiagnosis::MAX_SOLVER_TIMEOUT;
    facts.attempts.last().timedOut = true;
    QTest::newRow("row 22 solver timeout at its limit") << facts << QStringList({"align_check_log"});

    facts = withReason(AlignFailReason::SolverFailed);
    facts.profile = "2-SingleThreadSolving";
    QTest::newRow("row 23 profile") << facts << QStringList({"solver_profile_default"});

    facts = withReason(AlignFailReason::SolverFailed);
    facts.image->verdict = ImageVerdict::OutOfFocus;
    facts.image->hfr = 3;
    facts.image->concentration = 2.1;
    QTest::newRow("AL17 out of focus, discs") << facts << QStringList({"check_focus"});

    facts = withReason(AlignFailReason::SolverFailed);
    facts.image->verdict = ImageVerdict::OutOfFocus;
    facts.image->hfr = 6;
    facts.image->centreDip = 0.4;
    QTest::newRow("AL17 out of focus, donuts") << facts << QStringList({"check_focus"});

    // Large stars with a bright core are focused, e.g. a long focal length with small pixels: not a cause
    facts = withReason(AlignFailReason::SolverFailed);
    facts.image->hfr = 6;
    QTest::newRow("AL17 large focused stars") << facts << QStringList({"align_check_log"});

    // 97 arcmin over 6000 pixels is 0.97"/px: an untracked star drifts 77.5 px in 5 s at the equator
    facts = withReason(AlignFailReason::SolverFailed);
    facts.image->verdict = ImageVerdict::Trailed;
    facts.image->trailLength = 12;
    facts.declination = 0;
    QTest::newRow("AL4 short trails") << facts << QStringList({"check_tracking"});

    facts = withReason(AlignFailReason::SolverFailed);
    facts.image->verdict = ImageVerdict::Trailed;
    facts.image->trailLength = 75;
    facts.declination = 0;
    QTest::newRow("AL4 trails of the sidereal drift") << facts << QStringList({"mount_tracking_on"});

    // At declination 60 the drift halves
    facts = withReason(AlignFailReason::SolverFailed);
    facts.image->verdict = ImageVerdict::Trailed;
    facts.image->trailLength = 75;
    facts.declination = 60;
    QTest::newRow("AL4 trails longer than the drift") << facts << QStringList({"check_tracking"});

    facts = withReason(AlignFailReason::SolverFailed);
    facts.image->verdict = ImageVerdict::Trailed;
    facts.image->trailLength = 75;
    QTest::newRow("AL4 trails, declination unknown") << facts << QStringList({"check_tracking"});

    facts = withReason(AlignFailReason::SolverFailed);
    facts.image->verdict = ImageVerdict::DoubleImages;
    facts.image->doubleOffset = 8;
    QTest::newRow("double images") << facts << QStringList({"check_mount_jump"});

    facts = withReason(AlignFailReason::SolverFailed);
    facts.fovW = 12;
    facts.fovH = 8;
    facts.requiredSizes = IndexFiles::requiredIndexSizes(12);
    facts.installedSizes = facts.requiredSizes;
    QTest::newRow("AL7 narrow field") << facts << QStringList({"fov_narrow", "align_check_log"});

    facts = withReason(AlignFailReason::SolverFailed);
    facts.reducer = 0.3;
    QTest::newRow("AL3 reducer") << facts << QStringList({"train_check_reducer"});

    // Order: hardware, then configuration, then data, then tuning; blockers before warnings
    facts = withReason(AlignFailReason::SolverFailed);
    facts.image->verdict = ImageVerdict::Trailed;
    facts.image->trailLength = 75;
    facts.declination = 0;
    facts.exposure = 5;
    facts.profile = "2-SingleThreadSolving";
    facts.reducer = 3;
    facts.mountUtc = facts.mountUtcReceived.addSecs(600);
    facts.installedSizes.clear();
    {
        QStringList expected({"mount_tracking_on", "mount_time_sync"});
        for (int size : facts.requiredSizes)
            expected << QString("index_install_%1").arg(size, 2, 10, QLatin1Char('0'));
        expected << "solver_profile_default" << "train_check_reducer";
        QTest::newRow("order") << facts << expected;
    }

    // Nothing specific
    facts = withReason(AlignFailReason::SolverFailed);
    QTest::newRow("solver failed, all checks pass") << facts << QStringList({"align_check_log"});

    facts = withReason(AlignFailReason::BusyTimeout);
    QTest::newRow("busy timeout") << facts << QStringList({"align_check_log"});
}

void TestAlignDiagnosis::testRules()
{
    QFETCH(AlignFacts, facts);
    QFETCH(QStringList, expected);

    const Report report = AlignDiagnosis::diagnose(facts);
    QCOMPARE(ids(report), expected);

    QCOMPARE(report.module, QString("align"));
    QCOMPARE(report.train, QString("Primary"));
    QCOMPARE(report.reason, static_cast<int>(facts.reason));
    QCOMPARE(report.reasonKey, alignFailReasonKey(facts.reason));
    QVERIFY(!report.title.isEmpty());
    // The caller fills the id and the creation time
    QVERIFY(report.id.isEmpty());

    for (const auto &item : report.items)
    {
        QVERIFY2(!item.title.isEmpty(), qPrintable(item.id));
        QVERIFY2(!item.why.isEmpty(), qPrintable(item.id));
        if (item.kind == Kind::Manual)
            QVERIFY2(!item.steps.isEmpty(), qPrintable(item.id));
        if (item.kind == Kind::Open)
            QVERIFY2(!item.openTarget.isEmpty(), qPrintable(item.id));
    }
}

void TestAlignDiagnosis::testItemDetails()
{
    // Unparking moves the mount: always a tap, and Align is not retried by itself
    auto facts = withReason(AlignFailReason::MountParked);
    Report report = AlignDiagnosis::diagnose(facts);
    QCOMPARE(report.items.size(), 1);
    QVERIFY(report.items[0].cost.testFlag(MovesHardware));
    QVERIFY(!report.items[0].autoRetry);
    QCOMPARE(report.items[0].kind, Kind::Auto);

    // The pixel size is set in the camera driver
    facts = withReason(AlignFailReason::MissingPixelSize);
    report = AlignDiagnosis::diagnose(facts);
    QCOMPARE(report.items[0].openTarget, QString("indi_panel:CCD Simulator:Image Info"));

    // Index files: one item per size, with the download estimate and the series to install
    facts = withReason(AlignFailReason::SolverFailed);
    facts.installedSizes.clear();
    report = AlignDiagnosis::diagnose(facts);
    QVERIFY(!report.items.isEmpty());
    const Item &index = report.items.first();
    QVERIFY(index.cost.testFlag(NeedsInternet));
    QCOMPARE(index.downloadBytes, IndexFiles::estimatedDownloadBytes(facts.requiredSizes.first()));
    QVERIFY(index.title.contains(QString::number(IndexFiles::installSeries(facts.requiredSizes.first()))));
    QVERIFY(index.verify);
    QCOMPARE(index.verify->action, QString("index_scan"));

    // Index files are only checked for the solvers that read them
    facts.indexSolver = false;
    report = AlignDiagnosis::diagnose(facts);
    QCOMPARE(ids(report), QStringList({"align_check_log"}));

    // Settings items change saved settings
    facts = withReason(AlignFailReason::SolverFailed);
    facts.exposure = 1;
    facts.image->verdict = ImageVerdict::FewStars;
    facts.image->stars = 2;
    report = AlignDiagnosis::diagnose(facts);
    for (const auto &item : report.items)
        QVERIFY2(item.cost.testFlag(ChangesSettings), qPrintable(item.id));

    // Warnings
    facts = withReason(AlignFailReason::SyncAlert);
    facts.canSync = false;
    report = AlignDiagnosis::diagnose(facts);
    QVERIFY(report.items[0].blocker);
    QVERIFY(!report.items[1].blocker);
}

void TestAlignDiagnosis::testTriedAndDismissed()
{
    auto facts = withReason(AlignFailReason::SolverFailed);
    facts.exposure = 1;
    facts.image->verdict = ImageVerdict::FewStars;
    facts.image->stars = 4;

    // One change per retry: the tried item is not offered again
    Report report = AlignDiagnosis::diagnose(facts, {"align_exposure_5s"});
    QCOMPARE(ids(report), QStringList({"align_bin_max"}));
    QCOMPARE(report.tried, QStringList({"align_exposure_5s"}));

    report = AlignDiagnosis::diagnose(facts, {}, {"align_bin_max"});
    QCOMPARE(ids(report), QStringList({"align_exposure_5s"}));
}

void TestAlignDiagnosis::testFewStarsAfterTries()
{
    // Row 26: once exposure and binning were tried, the sky or the cap is next
    auto facts = withReason(AlignFailReason::SolverFailed);
    facts.exposure = 1;
    facts.image->verdict = ImageVerdict::FewStars;
    facts.image->stars = 3;
    const Report report = AlignDiagnosis::diagnose(facts, {"align_exposure_5s", "align_bin_max"});
    QCOMPARE(ids(report), QStringList({"check_sky_and_cap"}));
}

void TestAlignDiagnosis::testRuledOut()
{
    auto facts = withReason(AlignFailReason::SolverFailed);
    AlignAttempt scale, position, both;
    scale.blindPass = "scale";
    position.blindPass = "position";
    both.blindPass = "scale_position";
    facts.attempts << scale << position << both;

    Report report = AlignDiagnosis::diagnose(facts);
    QCOMPARE(report.ruledOut.size(), 6);
    QVERIFY(report.ruledOut.join(" ").contains("The image is good"));
    QVERIFY(report.ruledOut[0].contains("Camera, optics and pixel size"));
    QVERIFY(report.ruledOut.join(" ").contains("reached KStars"));
    QVERIFY(report.ruledOut.join(" ").contains("without the scale and position hints"));
    QVERIFY(report.ruledOut.join(" ").contains("Index files for this field of view are installed"));

    // Defocused stars: the image is not ruled out, and the report says how the stars look
    const ImageStats good = *facts.image;
    facts.image->verdict = ImageVerdict::OutOfFocus;
    facts.image->concentration = 2.2;
    report = AlignDiagnosis::diagnose(facts);
    QVERIFY(!report.ruledOut.join(" ").contains("The image is good"));
    QVERIFY(!report.items.isEmpty() && report.items.first().id == "check_focus");
    QVERIFY(report.items.first().blocker);
    QVERIFY(report.items.first().why.contains("flat discs"));
    QCOMPARE(report.items.first().verify->metric, QString("concentration"));
    facts.image->centreDip = 0.5;
    report = AlignDiagnosis::diagnose(facts);
    QVERIFY(report.items.first().why.contains("rings"));
    *facts.image = good;

    // Without the blind passes, a wrong scale or position is not ruled out
    facts.attempts.clear();
    report = AlignDiagnosis::diagnose(facts);
    QVERIFY(!report.ruledOut.join(" ").contains("without the scale and position hints"));

    // The setup checks did not run before a missing camera
    facts = withReason(AlignFailReason::NoCamera);
    facts.hasCamera = false;
    report = AlignDiagnosis::diagnose(facts);
    QVERIFY(report.ruledOut.isEmpty());

    // An unmeasured frame is listed as not checked
    facts = withReason(AlignFailReason::SolverFailed);
    facts.image.reset();
    report = AlignDiagnosis::diagnose(facts);
    QVERIFY(report.notChecked.join(" ").contains("Image"));
}

void TestAlignDiagnosis::testClock()
{
    // No client device, e.g. KStars on the desktop: the mount clock is still checked, against this computer
    auto facts = withReason(AlignFailReason::SolverFailed);
    facts.clientUtc.reset();
    Report report = AlignDiagnosis::diagnose(facts);
    QVERIFY2(!report.notChecked.join(" ").contains("Mount clock"), qPrintable(report.notChecked.join(" ")));
    QVERIFY(report.ruledOut.join(" ").contains("mount clock matches"));
    QVERIFY(!ids(report).contains("mount_time_sync"));

    // No client device, and the KStars clock is off from this computer
    facts.kstarsUtc = facts.systemUtc.addSecs(600);
    report = AlignDiagnosis::diagnose(facts);
    QCOMPARE(ids(report), QStringList({"kstars_clock_wrong"}));

    // The mount does not report its time
    facts = withReason(AlignFailReason::SolverFailed);
    facts.mountUtc.reset();
    report = AlignDiagnosis::diagnose(facts);
    QVERIFY(report.notChecked.join(" ").contains("does not report its time"));

    // Mount clock off by 10 minutes when it reported its time
    facts = withReason(AlignFailReason::SolverFailed);
    facts.mountUtc = facts.mountUtcReceived.addSecs(-600);
    report = AlignDiagnosis::diagnose(facts);
    QCOMPARE(ids(report), QStringList({"mount_time_sync"}));

    // A correct time reported long ago is not a wrong clock: most drivers report their time once
    facts = withReason(AlignFailReason::SolverFailed);
    facts.mountUtcReceived = facts.kstarsUtc.addSecs(-5 * 3600);
    facts.mountUtc = facts.mountUtcReceived.addSecs(1);
    report = AlignDiagnosis::diagnose(facts);
    QVERIFY(!ids(report).contains("mount_time_sync"));
    QVERIFY(report.ruledOut.join(" ").contains("mount clock matches"));

    // KStars clock off: compared only, never applied (Q3)
    facts = withReason(AlignFailReason::SolverFailed);
    facts.clientUtc = facts.kstarsUtc.addSecs(600);
    report = AlignDiagnosis::diagnose(facts);
    QCOMPARE(ids(report), QStringList({"kstars_clock_wrong"}));
    QCOMPARE(report.items[0].kind, Kind::Manual);

    // Within the limit
    facts = withReason(AlignFailReason::SolverFailed);
    facts.mountUtc = facts.mountUtcReceived.addSecs(60);
    report = AlignDiagnosis::diagnose(facts);
    QVERIFY(report.ruledOut.join(" ").contains("mount clock matches"));

    // The clock does not matter for a missing camera
    facts = withReason(AlignFailReason::NoCamera);
    facts.hasCamera = false;
    facts.clientUtc.reset();
    report = AlignDiagnosis::diagnose(facts);
    QVERIFY(report.notChecked.isEmpty());
}

void TestAlignDiagnosis::testEvidence()
{
    auto facts = withReason(AlignFailReason::SolverFailed);
    facts.exposure = 1;
    facts.image->verdict = ImageVerdict::FewStars;
    facts.image->stars = 4;
    facts.installedSizes.clear();
    const Report report = AlignDiagnosis::diagnose(facts);

    QHash<QString, Evidence> byKey;
    for (const auto &evidence : report.evidence)
        byKey.insert(evidence.key, evidence);

    QVERIFY(byKey.contains("fov"));
    QCOMPARE(byKey["fov"].unit, QString("arcmin"));
    QCOMPARE(byKey["exposure"].level, Evidence::Bad);
    QCOMPARE(byKey["stars"].value.toInt(), 4);
    QCOMPARE(byKey["stars"].level, Evidence::Bad);
    QCOMPARE(byKey["index_required"].level, Evidence::Bad);
    QCOMPARE(byKey["camera"].value.toString(), QString("CCD Simulator"));

    // Values are numbers or untranslated strings
    QCOMPARE(byKey["exposure"].value.toDouble(), 1.0);
}

void TestAlignDiagnosis::testReasonKeys()
{
    QSet<QString> keys;
    for (int value = static_cast<int>(AlignFailReason::None); value <= static_cast<int>(AlignFailReason::UserAbort); value++)
    {
        const auto reason = static_cast<AlignFailReason>(value);
        const QString key = alignFailReasonKey(reason);
        QVERIFY2(!key.isEmpty(), qPrintable(QString::number(value)));
        QVERIFY2(!keys.contains(key), qPrintable(key));
        keys.insert(key);

        // Every diagnosable reason gets a title and at least one item
        if (AlignDiagnosis::isDiagnosable(reason))
        {
            const Report report = AlignDiagnosis::diagnose(withReason(reason));
            QVERIFY2(!report.title.isEmpty(), qPrintable(key));
            QVERIFY2(!report.items.isEmpty(), qPrintable(key));
        }
    }

    QVERIFY(!AlignDiagnosis::isDiagnosable(AlignFailReason::None));
    QVERIFY(!AlignDiagnosis::isDiagnosable(AlignFailReason::UserAbort));
    QVERIFY(AlignDiagnosis::isDiagnosable(AlignFailReason::SolverFailed));

    // Values go out on D-Bus and EkosLive and must not move
    QCOMPARE(static_cast<int>(AlignFailReason::MountParked), 23);
    QCOMPARE(static_cast<int>(AlignFailReason::UserAbort), 29);
    QCOMPARE(alignFailReasonKey(AlignFailReason::MountParked), QString("mount_parked"));
}

QTEST_GUILESS_MAIN(TestAlignDiagnosis)

#include "testaligndiagnosis.moc"
