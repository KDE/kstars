/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

/*
 * Tests for the Align failure rules: one data row per rule of the catalog, facts in, item ids and order out.
 * Also the evidence, the ruled out and not checked lines, the tried and dismissed filtering and the reason keys,
 * how likely each item is, and the check-up when nothing failed.
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
        void testLikelihood();
        void testCheckup();
        void testCheckupItems_data();
        void testCheckupItems();

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

    // Order: likely causes first (here the trails and the missing index files), then possible ones;
    // within the same likelihood blockers before warnings, then hardware, configuration, data and tuning
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
        QStringList expected({"mount_tracking_on"});
        for (int size : facts.requiredSizes)
            expected << QString("index_install_%1").arg(size, 2, 10, QLatin1Char('0'));
        expected << "mount_time_sync" << "solver_profile_default" << "train_check_reducer";
        QTest::newRow("order") << facts << expected;
    }

    // Few stars in a short exposure explain the failure better than a clock that is off: they come first,
    // although the clock is a configuration item and the exposure a data item
    facts = withReason(AlignFailReason::SolverFailed);
    facts.exposure = 2;
    facts.image->verdict = ImageVerdict::FewStars;
    facts.image->stars = 4;
    facts.mountUtc = facts.mountUtcReceived.addSecs(600);
    facts.profile = "2-SingleThreadSolving";
    QTest::newRow("order by likelihood") << facts
                                         << QStringList({"align_exposure_5s", "align_bin_max", "mount_time_sync", "solver_profile_default"});

    // A mount that disconnected keeps its last position: Align goes on and fails later
    facts = withReason(AlignFailReason::SolverFailed);
    facts.mountConnected = false;
    QTest::newRow("solver failed, mount disconnected") << facts << QStringList({"mount_connect"});

    facts = withReason(AlignFailReason::SlewAlert);
    facts.slewErrors = 3;
    facts.mountConnected = false;
    QTest::newRow("slew alerts, mount disconnected") << facts << QStringList({"mount_connect", "slew_rejected_by_mount"});

    facts = withReason(AlignFailReason::CaptureError);
    facts.captureErrors = 3;
    facts.cameraConnected = false;
    QTest::newRow("capture errors, camera disconnected") << facts << QStringList({"camera_connect", "check_camera_usb_power"});

    // The default profile also for a timeout
    facts = withReason(AlignFailReason::SolverTimeout);
    facts.attempts.last().timedOut = true;
    facts.profile = "2-SingleThreadSolving";
    QTest::newRow("solver timeout, custom profile") << facts << QStringList({"solver_timeout_raise", "solver_profile_default"});

    // KStars does not keep the mount time: it becomes the time source
    facts = withReason(AlignFailReason::SolverFailed);
    facts.mountUtc = facts.mountUtcReceived.addSecs(600);
    facts.timeSource = "Mount";
    QTest::newRow("mount clock off, time source mount") << facts << QStringList({"mount_time_source_kstars"});

    facts = withReason(AlignFailReason::SolverFailed);
    facts.mountUtc = facts.mountUtcReceived.addSecs(600);
    facts.timeUpdate = false;
    QTest::newRow("mount clock off, no time updates") << facts << QStringList({"mount_time_source_kstars"});

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

void TestAlignDiagnosis::testLikelihood()
{
    auto likelihoodOf = [](const Report & report, const QString & id)
    {
        const Item *item = findItem(report, id);
        return item ? likelihoodKey(item->likelihood) : QString("missing");
    };

    // The failure reason names the device
    auto facts = withReason(AlignFailReason::CameraDisconnected);
    facts.cameraConnected = false;
    QCOMPARE(likelihoodOf(AlignDiagnosis::diagnose(facts), "camera_connect"), QString("likely"));

    facts = withReason(AlignFailReason::SyncAlert);
    facts.canSync = false;
    Report report = AlignDiagnosis::diagnose(facts);
    QCOMPARE(likelihoodOf(report, "slew_rejected_by_mount"), QString("likely"));
    QCOMPARE(likelihoodOf(report, "mount_no_sync"), QString("possible"));

    // A missing value is the cause; plausible values are only suspects
    facts = withReason(AlignFailReason::MissingOptics);
    facts.focalLength = 0;
    QCOMPARE(likelihoodOf(AlignDiagnosis::diagnose(facts), "train_set_optics"), QString("likely"));
    facts = withReason(AlignFailReason::FovOutOfBounds);
    facts.pixelW = facts.pixelH = 0.001;
    report = AlignDiagnosis::diagnose(facts);
    QCOMPARE(likelihoodOf(report, "train_set_optics"), QString("possible"));
    QCOMPARE(likelihoodOf(report, "camera_set_pixel_size"), QString("possible"));

    // Missing index files and a frame known to hold few stars
    facts = withReason(AlignFailReason::SolverFailed);
    facts.installedSizes.clear();
    facts.exposure = 1;
    facts.image->verdict = ImageVerdict::FewStars;
    facts.image->stars = 4;
    report = AlignDiagnosis::diagnose(facts);
    QCOMPARE(likelihoodOf(report, QString("index_install_%1").arg(facts.requiredSizes.first(), 2, 10, QLatin1Char('0'))),
             QString("likely"));
    QCOMPARE(likelihoodOf(report, "align_exposure_5s"), QString("likely"));
    QCOMPARE(likelihoodOf(report, "align_bin_max"), QString("likely"));

    // Unknown star count: only possible
    facts = withReason(AlignFailReason::SolverFailed);
    facts.exposure = 1;
    facts.image.reset();
    facts.attempts.clear();
    report = AlignDiagnosis::diagnose(facts);
    QCOMPARE(likelihoodOf(report, "align_exposure_5s"), QString("possible"));
    QCOMPARE(likelihoodOf(report, "align_bin_max"), QString("possible"));

    // The measured frame shows the problem
    facts = withReason(AlignFailReason::SolverFailed);
    facts.image->verdict = ImageVerdict::OutOfFocus;
    facts.image->concentration = 2.1;
    QCOMPARE(likelihoodOf(AlignDiagnosis::diagnose(facts), "check_focus"), QString("likely"));

    // Clocks, profile and reducer fit a failure but do not prove it; a narrow field is advice
    facts = withReason(AlignFailReason::SolverFailed);
    facts.mountUtc = facts.mountUtcReceived.addSecs(600);
    facts.profile = "2-SingleThreadSolving";
    facts.reducer = 0.3;
    facts.fovW = 12;
    facts.fovH = 8;
    facts.requiredSizes = IndexFiles::requiredIndexSizes(12);
    facts.installedSizes = facts.requiredSizes;
    report = AlignDiagnosis::diagnose(facts);
    QCOMPARE(likelihoodOf(report, "mount_time_sync"), QString("possible"));
    QCOMPARE(likelihoodOf(report, "solver_profile_default"), QString("possible"));
    QCOMPARE(likelihoodOf(report, "train_check_reducer"), QString("possible"));
    QCOMPARE(likelihoodOf(report, "fov_narrow"), QString("check"));

    // A solver timeout: raising the timeout is likely
    facts = withReason(AlignFailReason::SolverTimeout);
    facts.attempts.last().timedOut = true;
    QCOMPARE(likelihoodOf(AlignDiagnosis::diagnose(facts), "solver_timeout_raise"), QString("likely"));

    // The first item of every report is its most likely one
    for (int value = static_cast<int>(AlignFailReason::NoMountCoords); value < static_cast<int>(AlignFailReason::UserAbort);
            value++)
    {
        report = AlignDiagnosis::diagnose(withReason(static_cast<AlignFailReason>(value)));
        for (int i = 1; i < report.items.size(); i++)
            QVERIFY2(report.items[i - 1].likelihood <= report.items[i].likelihood, qPrintable(report.reasonKey));
    }
}

void TestAlignDiagnosis::testCheckup()
{
    // A setup that just solved with these settings: nothing to suggest
    auto facts = healthy();
    facts.attempts.last().success = true;
    facts.attempts.last().exposure = facts.exposure;
    facts.attempts.last().binning = facts.binIndex + 1;
    facts.attempts.last().profile = facts.profile;
    Report report = AlignDiagnosis::checkup(facts);
    QVERIFY(isCheckup(report));
    QCOMPARE(report.reasonKey, QString("checkup"));
    QCOMPARE(report.reason, 0);
    QCOMPARE(report.module, QString("align"));
    QCOMPARE(report.train, QString("Primary"));
    QVERIFY(report.items.isEmpty());
    QCOMPARE(report.title, QString("Align looks ready"));
    const QString ruledOut = report.ruledOut.join(" ");
    QVERIFY(ruledOut.contains("camera CCD Simulator is connected"));
    QVERIFY(ruledOut.contains("mount is connected"));
    QVERIFY(ruledOut.contains("Focal length, aperture and pixel size are set"));
    QVERIFY(ruledOut.contains("Index files for this field of view are installed"));
    QVERIFY(ruledOut.contains("mount clock matches"));
    QVERIFY(ruledOut.contains("Align solved"));

    // A failure reason in the facts does not matter
    facts.reason = AlignFailReason::SolverFailed;
    QVERIFY(AlignDiagnosis::checkup(facts).items.isEmpty());

    // The same exposure no longer solved with: suggestions again
    facts.exposure = 2;
    report = AlignDiagnosis::checkup(facts);
    QCOMPARE(ids(report), QStringList({"align_exposure_5s", "align_bin_max"}));
    QCOMPARE(report.title, QString("2 things to check"));
    for (const auto &item : report.items)
    {
        // Nothing failed: suggestions only, and Align is never started by itself
        QVERIFY2(!item.autoRetry, qPrintable(item.id));
        QCOMPARE(item.likelihood, Likelihood::Possible);
    }

    // Dismissed items are not offered
    report = AlignDiagnosis::checkup(facts, {"align_bin_max"});
    QCOMPARE(ids(report), QStringList({"align_exposure_5s"}));

    // Evidence of what was looked at
    QHash<QString, Evidence> byKey;
    for (const auto &evidence : report.evidence)
        byKey.insert(evidence.key, evidence);
    QCOMPARE(byKey["exposure"].value.toDouble(), 2.0);
    QCOMPARE(byKey["binning"].value.toString(), QString("1x1"));
    QVERIFY(byKey.contains("fov"));
}

void TestAlignDiagnosis::testCheckupItems_data()
{
    QTest::addColumn<AlignFacts>("facts");
    QTest::addColumn<QStringList>("expected");

    // Solved with these settings, so only the setup itself is checked
    AlignFacts solved = healthy();
    solved.attempts.last().success = true;
    solved.attempts.last().exposure = solved.exposure;
    solved.attempts.last().binning = 1;
    solved.attempts.last().profile = solved.profile;

    AlignFacts facts = solved;
    facts.hasCamera = false;
    QTest::newRow("no camera") << facts << QStringList({"train_select_camera"});

    facts = solved;
    facts.cameraConnected = false;
    facts.pixelW = facts.pixelH = 0;
    // The driver reports the pixel size once connected: connect first
    QTest::newRow("camera disconnected") << facts << QStringList({"camera_connect"});

    // A mount is optional, a disconnected one is not; its clock is not checked
    facts = solved;
    facts.hasMount = false;
    QTest::newRow("no mount") << facts << QStringList();

    facts = solved;
    facts.mountConnected = false;
    facts.mountUtc = facts.mountUtcReceived.addSecs(600);
    QTest::newRow("mount disconnected") << facts << QStringList({"mount_connect"});

    facts = solved;
    facts.mountCoordsValid = false;
    QTest::newRow("mount without coordinates") << facts << QStringList({"mount_check_coords"});

    facts = solved;
    facts.focalLength = 0;
    facts.pixelW = facts.pixelH = -1;
    QTest::newRow("optics and pixel size missing") << facts << QStringList({"train_set_optics", "camera_set_pixel_size"});

    facts = solved;
    facts.externalSolverMissing = true;
    QTest::newRow("external solver missing") << facts << QStringList({"solver_check_external"});

    facts = solved;
    facts.mountUtc = facts.mountUtcReceived.addSecs(600);
    QTest::newRow("mount clock off") << facts << QStringList({"mount_time_sync"});

    facts = solved;
    facts.mountUtc = facts.mountUtcReceived.addSecs(600);
    facts.timeSource = "Mount";
    QTest::newRow("mount clock off, time source mount") << facts << QStringList({"mount_time_source_kstars"});

    facts = solved;
    facts.clientUtc = facts.kstarsUtc.addSecs(600);
    QTest::newRow("KStars clock off") << facts << QStringList({"kstars_clock_wrong"});

    // Not solved with these settings: the settings of the Doc's table
    facts = healthy();
    facts.exposure = 1;
    facts.profile = "2-SingleThreadSolving";
    QTest::newRow("settings") << facts << QStringList({"align_exposure_5s", "align_bin_max", "solver_profile_default"});

    // The reducer and a narrow field are checked even after a solve
    facts = solved;
    facts.reducer = 0.3;
    QTest::newRow("reducer") << facts << QStringList({"train_check_reducer"});

    facts = solved;
    facts.fovW = 12;
    facts.fovH = 8;
    facts.requiredSizes = IndexFiles::requiredIndexSizes(12);
    facts.installedSizes = facts.requiredSizes;
    QTest::newRow("narrow field") << facts << QStringList({"fov_narrow"});

    // Most likely first: the disconnected camera and the missing index files, then the clock, then the hints
    facts = healthy();
    facts.cameraConnected = false;
    facts.installedSizes.clear();
    facts.mountUtc = facts.mountUtcReceived.addSecs(600);
    facts.exposure = 1;
    facts.reducer = 3;
    {
        QStringList expected({"camera_connect"});
        for (int size : facts.requiredSizes)
            expected << QString("index_install_%1").arg(size, 2, 10, QLatin1Char('0'));
        expected << "mount_time_sync" << "align_exposure_5s" << "align_bin_max" << "train_check_reducer";
        QTest::newRow("order") << facts << expected;
    }
}

void TestAlignDiagnosis::testCheckupItems()
{
    QFETCH(AlignFacts, facts);
    QFETCH(QStringList, expected);

    const Report report = AlignDiagnosis::checkup(facts);
    QCOMPARE(ids(report), expected);
    QVERIFY(isCheckup(report));
    QVERIFY(!report.title.isEmpty());

    for (const auto &item : report.items)
    {
        QVERIFY2(!item.autoRetry, qPrintable(item.id));
        QVERIFY2(!item.title.isEmpty(), qPrintable(item.id));
        QVERIFY2(!item.why.isEmpty(), qPrintable(item.id));
        if (item.kind == Kind::Manual)
            QVERIFY2(!item.steps.isEmpty(), qPrintable(item.id));
        if (item.kind == Kind::Open)
            QVERIFY2(!item.openTarget.isEmpty(), qPrintable(item.id));
    }
}

QTEST_GUILESS_MAIN(TestAlignDiagnosis)

#include "testaligndiagnosis.moc"
