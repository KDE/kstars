/*
    KStars UI tests for the Align failure diagnosis

    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "test_ekos_align_diagnosis.h"

#if defined(HAVE_INDI)

#include "kstars_ui_tests.h"
#include "test_ekos.h"
#include "Options.h"
#include "ekos/align/align.h"
#include "ekos/align/indexfiles.h"
#include "ekos/auxiliary/diagnosismanager.h"
#include "ekos/auxiliary/opticaltrainmanager.h"
#include "ekos/mount/mount.h"
#include "indi/clientmanager.h"
#include "indi/driverinfo.h"
#include "indi/guimanager.h"
#include "indi/indicamera.h"
#include "indi/indidome.h"
#include "indi/indilistener.h"
#include "indi/indimount.h"
#include "indi/indirotator.h"
#include "ksmessagebox.h"
#include "ksutils.h"

#include <KConfigDialog>

#include <QJsonArray>
#include <QJsonDocument>
#include <QSignalSpy>
#include <QTemporaryDir>

#define SOLVE_TIMEOUT (4 * 1000 * static_cast<int>(Options::astrometryTimeout()) + 60000)

TestEkosAlignDiagnosis::TestEkosAlignDiagnosis(QObject *parent) : QObject(parent)
{
    m_CaptureHelper = new TestEkosCaptureHelper();
}

void TestEkosAlignDiagnosis::testParkedMount()
{
    Ekos::Align *align = Ekos::Manager::Instance()->alignModule();
    Ekos::Mount *mountModule = Ekos::Manager::Instance()->mountModule();
    auto mount = Ekos::OpticalTrainManager::Instance()->getMount(m_CaptureHelper->m_primaryTrain);
    QVERIFY(mount != nullptr);

    QVERIFY(mountModule->park());
    QTRY_VERIFY_WITH_TIMEOUT(mount->parkStatus() == ISD::PARK_PARKED, 120000);

    // Before the fix, Align captured and solved, and only the sync failed on the parked mount
    align->setSolverAction(Ekos::Align::GOTO_SYNC);
    QVERIFY(!align->captureAndSolve());
    QCOMPARE(align->status(), Ekos::ALIGN_FAILED);
    QCOMPARE(align->lastFailReason(), static_cast<int>(Ekos::AlignFailReason::MountParked));

    QTRY_VERIFY_WITH_TIMEOUT(!m_Reports.isEmpty(), 5000);
    QCOMPARE(m_Reports.last()["reasonKey"].toString(), QString("mount_parked"));
    QCOMPARE(lastReportItems(), QStringList({"mount_unpark"}));

    // The report is in the Align log and on D-Bus
    QVERIFY(m_AlignLog.join("\n").contains("Align failed: The mount is parked."));
    const QJsonArray dbusReports = QJsonDocument::fromJson(Ekos::Manager::Instance()->diagnosis("align").toUtf8()).array();
    QCOMPARE(dbusReports.size(), 1);
    QCOMPARE(dbusReports.first().toObject()["id"].toString(), lastReportId());

    // Unparking moves the mount: refused without a confirmation, applied with one
    auto diagnosis = Ekos::Manager::Instance()->diagnosisManager();
    QCOMPARE(diagnosis->apply(lastReportId(), "mount_unpark", false)["reason"].toString(), QString("needs_confirm"));
    QVERIFY(Ekos::Manager::Instance()->applyDiagnosisItem(lastReportId(), "mount_unpark"));
    QVERIFY(waitForUpdate("mount_unpark", "done", 120000));
    QTRY_VERIFY_WITH_TIMEOUT(mount->parkStatus() == ISD::PARK_UNPARKED, 5000);

    // A hardware item never retries Align by itself
    QTest::qWait(1000);
    QCOMPARE(align->status(), Ekos::ALIGN_FAILED);
}

void TestEkosAlignDiagnosis::testImageTransferDisabled()
{
    Ekos::Align *align = Ekos::Manager::Instance()->alignModule();
    auto camera = Ekos::OpticalTrainManager::Instance()->getCamera(m_CaptureHelper->m_primaryTrain);
    QVERIFY(camera != nullptr);
    auto clientManager = camera->getDriverInfo()->getClientManager();
    QVERIFY(clientManager != nullptr);
    const QByteArray device = camera->getDeviceName().toLatin1();

    clientManager->setBLOBMode(B_NEVER, device.constData(), "CCD1");
    clientManager->setBLOBMode(B_NEVER, device.constData(), "CCD2");

    // Before the fix, a modal dialog blocked here until someone answered it
    align->setSolverAction(Ekos::Align::GOTO_NOTHING);
    QVERIFY(!align->captureAndSolve());
    QCOMPARE(align->status(), Ekos::ALIGN_FAILED);
    QCOMPARE(align->lastFailReason(), static_cast<int>(Ekos::AlignFailReason::BlobDisabled));

    // captureAndSolve() returned while the question is still open: it does not block. Answer it with No.
    QTRY_VERIFY_WITH_TIMEOUT(KSMessageBox::Instance()->isVisible(), 5000);
    KSMessageBox::Instance()->reject();
    QCOMPARE(clientManager->getBLOBMode(device.constData(), "CCD1"), B_NEVER);

    QTRY_VERIFY_WITH_TIMEOUT(!m_Reports.isEmpty(), 5000);
    QCOMPARE(lastReportItems(), QStringList({"camera_enable_blob"}));

    // Enabling it from the diagnosis retries Align, which now succeeds and reports the fix
    KTRY_SET_DOUBLESPINBOX(align, alignGain, 90.0);
    QVERIFY(Ekos::Manager::Instance()->applyDiagnosisItem(lastReportId(), "camera_enable_blob"));
    QCOMPARE(clientManager->getBLOBMode(device.constData(), "CCD1"), B_ONLY);
    QVERIFY(waitForUpdate("camera_enable_blob", "waiting", 5000));
    QTRY_VERIFY_WITH_TIMEOUT(m_AlignStatus == Ekos::ALIGN_COMPLETE || m_AlignStatus == Ekos::ALIGN_FAILED,
                             SOLVE_TIMEOUT);
    QVERIFY2(m_AlignStatus == Ekos::ALIGN_COMPLETE, qPrintable(m_AlignLog.join("\n")));
    QVERIFY(waitForUpdate("camera_enable_blob", "fixed", 5000));
    QVERIFY(Ekos::Manager::Instance()->diagnosisManager()->reports("align").isEmpty());
}

void TestEkosAlignDiagnosis::testUserAbort()
{
    Ekos::Align *align = Ekos::Manager::Instance()->alignModule();
    align->setSolverAction(Ekos::Align::GOTO_NOTHING);

    KTRY_SET_DOUBLESPINBOX(align, alignExposure, 10.0);
    QVERIFY(align->captureAndSolve());
    QTRY_VERIFY_WITH_TIMEOUT(m_AlignStatus == Ekos::ALIGN_PROGRESS, 10000);
    align->abort();

    QCOMPARE(align->status(), Ekos::ALIGN_ABORTED);
    QCOMPARE(align->lastFailReason(), static_cast<int>(Ekos::AlignFailReason::UserAbort));
    QTest::qWait(2000);
    QVERIFY(m_Reports.isEmpty());
}

void TestEkosAlignDiagnosis::testNoIndexFiles()
{
    Ekos::Align *align = Ekos::Manager::Instance()->alignModule();
    align->setSolverAction(Ekos::Align::GOTO_NOTHING);
    KTRY_SET_DOUBLESPINBOX(align, alignGain, 90.0);
    KTRY_SET_DOUBLESPINBOX(align, alignExposure, 3.0);

    QTemporaryDir emptyFolder;
    QVERIFY(emptyFolder.isValid());
    Options::setAstrometryIndexFolderList({emptyFolder.path()});

    QVERIFY(align->captureAndSolve());
    QTRY_VERIFY_WITH_TIMEOUT(m_AlignStatus == Ekos::ALIGN_FAILED || m_AlignStatus == Ekos::ALIGN_COMPLETE, SOLVE_TIMEOUT);
    QCOMPARE(m_AlignStatus, Ekos::ALIGN_FAILED);
    QCOMPARE(align->lastFailReason(), static_cast<int>(Ekos::AlignFailReason::SolverFailed));
    QVERIFY(!align->solverAttempts().isEmpty());

    // The report waits for the failed frame to be measured
    QTRY_VERIFY_WITH_TIMEOUT(!m_Reports.isEmpty(), 30000);
    const QJsonObject report = m_Reports.last();
    QCOMPARE(report["reasonKey"].toString(), QString("solver_failed"));

    const QList<double> fov = align->fov();
    QVERIFY(fov.size() >= 2);
    QStringList expected;
    for (int size : Ekos::IndexFiles::requiredIndexSizes(std::max(fov[0], fov[1])))
        expected << QString("index_install_%1").arg(size, 2, 10, QLatin1Char('0'));
    QVERIFY(!expected.isEmpty());
    for (const auto &id : expected)
        QVERIFY2(lastReportItems().contains(id), qPrintable(lastReportItems().join(", ")));

    // The frame was measured, so its stars are evidence
    bool hasStars = false;
    for (const auto &evidence : report["evidence"].toArray())
        hasStars = hasStars || evidence.toObject()["key"].toString() == "stars";
    QVERIFY(hasStars);
    for (const auto &line : report["notChecked"].toArray())
        QVERIFY2(!line.toString().startsWith("Image"), qPrintable(line.toString()));

    // KStars sent its time to the mount simulator when it connected, and there is no EkosLive client:
    // the mount clock is checked against this computer, from the time the mount reported back
    for (const auto &line : report["notChecked"].toArray())
        QVERIFY2(!line.toString().startsWith("Mount clock"), qPrintable(line.toString()));
    QStringList ruledOut;
    for (const auto &line : report["ruledOut"].toArray())
        ruledOut << line.toString();
    QVERIFY2(ruledOut.join(" ").contains("mount clock matches"), qPrintable(ruledOut.join(" ")));

    // AG4: the install ends with a result. The test has no network, so the download fails, and the
    // item reports it instead of waiting forever.
    const QString indexItem = expected.first();
    QCOMPARE(Ekos::Manager::Instance()->diagnosisManager()->apply(lastReportId(), indexItem)["state"].toString(),
             QString("running"));
    QVERIFY(waitForUpdate(indexItem, "failed", 60000));
    QVERIFY(!Ekos::Manager::Instance()->diagnosisManager()->isBusy());
    QVERIFY(Ekos::Manager::Instance()->diagnosisManager()->tried("align", m_CaptureHelper->m_primaryTrain).isEmpty());

    // Align opens its settings when no index file is found
    if (auto dialog = KConfigDialog::exists("alignsettings"))
        dialog->hide();
}

// Regression test: the diagnosis offered "Turn on mount tracking" with no action behind it, so applying it was
// refused as an unknown item
void TestEkosAlignDiagnosis::testTrackingOff()
{
    Ekos::Align *align = Ekos::Manager::Instance()->alignModule();
    Ekos::Mount *mountModule = Ekos::Manager::Instance()->mountModule();
    auto diagnosis = Ekos::Manager::Instance()->diagnosisManager();
    const QString warning = "Warning: the mount is not tracking.";

    // The simulator stays idle after the unpark of testParkedMount, and tracks after a slew
    if (mountModule->status() != ISD::Mount::MOUNT_IDLE)
        QVERIFY(mountModule->setTracking(false));
    QTRY_VERIFY_WITH_TIMEOUT(mountModule->status() == ISD::Mount::MOUNT_IDLE, 10000);

    // Align warns when it starts with the mount idle, and goes on
    align->setSolverAction(Ekos::Align::GOTO_NOTHING);
    QVERIFY(align->captureAndSolve());
    QVERIFY2(m_AlignLog.join("\n").contains(warning), qPrintable(m_AlignLog.join("\n")));
    QCOMPARE(align->status(), Ekos::ALIGN_PROGRESS);
    align->abort();
    QTRY_VERIFY_WITH_TIMEOUT(align->status() != Ekos::ALIGN_PROGRESS, 10000);
    QCOMPARE(mountModule->status(), ISD::Mount::MOUNT_IDLE);

    // The check-up offers the fix
    const QJsonArray reports = diagnosis->checkup("align");
    QCOMPARE(reports.size(), 1);
    const QJsonObject report = reports.first().toObject();
    QCOMPARE(report["reasonKey"].toString(), QString("checkup"));
    QStringList items;
    for (const auto &item : report["items"].toArray())
        items << item.toObject()["id"].toString();
    QVERIFY2(items.contains("mount_tracking_on"), qPrintable(items.join(", ")));

    // Tracking moves the mount: refused without a confirmation, applied with one, and done once the mount tracks
    QCOMPARE(diagnosis->apply(report["id"].toString(), "mount_tracking_on", false)["reason"].toString(),
             QString("needs_confirm"));
    const QJsonObject result = diagnosis->apply(report["id"].toString(), "mount_tracking_on", true);
    QVERIFY2(result["state"].toString() != "refused", qPrintable(QJsonDocument(result).toJson()));
    QVERIFY(waitForUpdate("mount_tracking_on", "done", 30000));
    QVERIFY(mountModule->activeMount()->isTracking());
    // The Mount module updates its status with the next coordinates
    QTRY_COMPARE_WITH_TIMEOUT(mountModule->status(), ISD::Mount::MOUNT_TRACKING, 5000);

    // Tracking again: nothing to offer, and no warning when Align starts
    items.clear();
    for (const auto &item : diagnosis->checkup("align").first().toObject()["items"].toArray())
        items << item.toObject()["id"].toString();
    QVERIFY2(!items.contains("mount_tracking_on"), qPrintable(items.join(", ")));
    m_AlignLog.clear();
    QVERIFY(align->captureAndSolve());
    QVERIFY2(!m_AlignLog.join("\n").contains(warning), qPrintable(m_AlignLog.join("\n")));
    align->abort();
}

// A slaved dome can take minutes to swing round after a meridian flip. Align waits for it in 10 s steps, and each
// step counted as a camera timeout: after four of them Align restarted the camera driver.
void TestEkosAlignDiagnosis::testDomeMotionIsNotCameraTimeout()
{
    Ekos::Align *align = Ekos::Manager::Instance()->alignModule();
    QSharedPointer<ISD::GenericDevice> device;
    QVERIFY(INDIListener::findDevice(m_CaptureHelper->m_DomeDevice, device));
    auto dome = device->getDome();
    QVERIFY(dome != nullptr);
    QVERIFY(dome->canAbsoluteMove());

    if (dome->isParked())
        QVERIFY(dome->unpark());
    QTRY_VERIFY_WITH_TIMEOUT(!dome->isParked() && !dome->isMoving(), 120000);

    // At 1°/s a 120° move keeps the dome busy for two minutes, three times the four retries that restarted the driver
    auto speeds = dome->getNumber("SPEEDS");
    QVERIFY(speeds.isValid());
    auto domeSpeed = speeds.findWidgetByName("DOME");
    QVERIFY(domeSpeed != nullptr);
    domeSpeed->setValue(1.0);
    dome->sendNewProperty(speeds);
    QTRY_VERIFY_WITH_TIMEOUT(std::abs(dome->getNumber("SPEEDS").findWidgetByName("DOME")->getValue() - 1.0) < 1e-6, 5000);

    QVERIFY(dome->setPosition(KSUtils::range360(dome->position() + 120)));
    QTRY_VERIFY_WITH_TIMEOUT(dome->isMoving(), 10000);

    QSignalSpy driverRestarts(align, &Ekos::Align::driverTimedout);
    KTRY_SET_DOUBLESPINBOX(align, alignExposure, 1.0);
    align->setSolverAction(Ekos::Align::GOTO_NOTHING);
    QVERIFY(align->captureAndSolve());

    // Align waits as long as the dome moves, without a camera timeout
    QTRY_VERIFY_WITH_TIMEOUT(!dome->isMoving() || !driverRestarts.isEmpty(), 180000);
    QVERIFY2(driverRestarts.isEmpty(), qPrintable(m_AlignLog.join("\n")));
    QVERIFY2(!m_AlignLog.join("\n").contains("Exposure timeout"), qPrintable(m_AlignLog.join("\n")));
    // Align reports no new state until it captures
    QCOMPARE(m_AlignStatus, Ekos::ALIGN_IDLE);
    QVERIFY(!m_AlignLog.contains("Capturing image..."));

    // Once the dome stops, the next retry captures
    QTRY_VERIFY_WITH_TIMEOUT(m_AlignLog.contains("Capturing image..."), 15000);
    QVERIFY(driverRestarts.isEmpty());
    align->abort();
}

// An autofocus run can take many minutes. Align waits for it in 10 s steps, and after four of them it restarted the
// camera driver, which the focus module was using.
void TestEkosAlignDiagnosis::testFocusBusyIsNotCameraTimeout()
{
    Ekos::Align *align = Ekos::Manager::Instance()->alignModule();
    QSignalSpy driverRestarts(align, &Ekos::Align::driverTimedout);
    KTRY_SET_DOUBLESPINBOX(align, alignExposure, 1.0);
    align->setSolverAction(Ekos::Align::GOTO_NOTHING);

    align->setFocusStatus(Ekos::FOCUS_PROGRESS);
    QVERIFY(align->captureAndSolve());

    // Five waits for the focus module: one more than restarted the driver
    QVERIFY2(!driverRestarts.wait(50000), qPrintable(m_AlignLog.join("\n")));
    QVERIFY2(!m_AlignLog.join("\n").contains("Exposure timeout"), qPrintable(m_AlignLog.join("\n")));
    QCOMPARE(m_AlignStatus, Ekos::ALIGN_IDLE);
    QVERIFY(!m_AlignLog.contains("Capturing image..."));

    // Once the focus module is done, the next retry captures
    align->setFocusStatus(Ekos::FOCUS_COMPLETE);
    QTRY_VERIFY_WITH_TIMEOUT(m_AlignLog.contains("Capturing image..."), 15000);
    QVERIFY(driverRestarts.isEmpty());
    align->abort();
}

// A rotator can take minutes to turn half a circle. Align waits for it with growing delays, and each one counted as a
// camera timeout: after four of them Align restarted the camera driver.
void TestEkosAlignDiagnosis::testRotatorBusyIsNotCameraTimeout()
{
    Ekos::Align *align = Ekos::Manager::Instance()->alignModule();
    QSharedPointer<ISD::GenericDevice> device;
    QVERIFY(INDIListener::findDevice(m_CaptureHelper->m_RotatorDevice, device));
    auto rotator = device->getRotator();
    QVERIFY(rotator != nullptr);
    QTRY_VERIFY_WITH_TIMEOUT(rotator->absoluteAngleState() != IPS_BUSY, 60000);

    // The simulator turns 10° per second, too fast to keep Align waiting. Report the rotator busy instead, as a slow
    // rotator does while it turns. The idle simulator sends no update that would end it.
    auto angle = rotator->getNumber("ABS_ROTATOR_ANGLE");
    QVERIFY(angle.isValid());
    angle.setState(IPS_BUSY);
    rotator->processNumber(angle);
    QCOMPARE(rotator->absoluteAngleState(), IPS_BUSY);

    QSignalSpy driverRestarts(align, &Ekos::Align::driverTimedout);
    KTRY_SET_DOUBLESPINBOX(align, alignExposure, 1.0);
    align->setSolverAction(Ekos::Align::GOTO_NOTHING);
    QVERIFY(align->captureAndSolve());

    // The retries come after 5, 0, 10, 15 and 20 s: the fourth restarted the driver after 30 s
    QVERIFY2(!driverRestarts.wait(55000), qPrintable(m_AlignLog.join("\n")));
    QVERIFY2(!m_AlignLog.join("\n").contains("Exposure timeout"), qPrintable(m_AlignLog.join("\n")));
    QCOMPARE(m_AlignStatus, Ekos::ALIGN_IDLE);
    QVERIFY(!m_AlignLog.contains("Capturing image..."));

    // Once the rotator is done, the next retry captures
    angle.setState(IPS_OK);
    rotator->processNumber(angle);
    QTRY_VERIFY_WITH_TIMEOUT(m_AlignLog.contains("Capturing image..."), 40000);
    QVERIFY(driverRestarts.isEmpty());
    align->abort();
}

/* *********************************************************************************
 *
 * Test helpers
 *
 * ********************************************************************************* */

bool TestEkosAlignDiagnosis::waitForUpdate(const QString &itemId, const QString &state, int timeout)
{
    auto found = [this, itemId, state]()
    {
        for (const auto &update : m_Updates)
            if (update["itemId"].toString() == itemId && update["state"].toString() == state)
                return true;
        return false;
    };
    KTRY_VERIFY_WITH_TIMEOUT_SUB(found(), timeout);
    return true;
}

QString TestEkosAlignDiagnosis::lastReportId() const
{
    return m_Reports.isEmpty() ? QString() : m_Reports.last()["id"].toString();
}

QStringList TestEkosAlignDiagnosis::lastReportItems() const
{
    QStringList ids;
    if (!m_Reports.isEmpty())
        for (const auto &item : m_Reports.last()["items"].toArray())
            ids << item.toObject()["id"].toString();
    return ids;
}

/* *********************************************************************************
 *
 * Test infrastructure
 *
 * ********************************************************************************* */

void TestEkosAlignDiagnosis::initTestCase()
{
    KVERIFY_EKOS_IS_HIDDEN();
    KTRY_OPEN_EKOS();
    KVERIFY_EKOS_IS_OPENED();
    QStandardPaths::setTestModeEnabled(true);

    Options::setAstrometryUseImageScale(true);
    Options::setAstrometryImageScaleUnits(1);
    Options::setAstrometryUsePosition(true);
    Options::setAstrometryDynamicThreshold(false);

    m_CaptureHelper->m_DomeDevice = "Dome Simulator";
    m_CaptureHelper->m_RotatorDevice = "Rotator Simulator";
    m_CaptureHelper->startEkosProfile();

    Options::setVerboseLogging(false);
    Options::setAlignmentLogging(false);
    Options::setLogToFile(false);
    m_CaptureHelper->prepareOpticalTrains();
    m_CaptureHelper->prepareMountModule();
    m_CaptureHelper->prepareAlignmentModule();
    m_CaptureHelper->init();
    GUIManager::Instance()->close();

    m_IndexFolders = Options::astrometryIndexFolderList();

    Ekos::Align *align = Ekos::Manager::Instance()->alignModule();
    connect(align, &Ekos::Align::newStatus, this, [this](Ekos::AlignState status)
    {
        m_AlignStatus = status;
    });
    connect(align, &Ekos::Align::newLog, this, [this](const QString & text)
    {
        m_AlignLog << text;
    });

    auto diagnosis = Ekos::Manager::Instance()->diagnosisManager();
    connect(diagnosis, &Ekos::DiagnosisManager::newDiagnosis, this, [this](const QString & module,
            const QJsonObject & report)
    {
        if (module == "align")
            m_Reports << report;
    });
    connect(diagnosis, &Ekos::DiagnosisManager::diagnosisUpdated, this, [this](const QJsonObject & update)
    {
        m_Updates << update;
    });
}

void TestEkosAlignDiagnosis::cleanupTestCase()
{
    disconnect(Ekos::Manager::Instance()->alignModule(), nullptr, this, nullptr);
    disconnect(Ekos::Manager::Instance()->diagnosisManager(), nullptr, this, nullptr);

    m_CaptureHelper->cleanup();
    QVERIFY(m_CaptureHelper->shutdownEkosProfile());
    KTRY_CLOSE_EKOS();
    KVERIFY_EKOS_IS_HIDDEN();
}

void TestEkosAlignDiagnosis::init()
{
    m_Reports.clear();
    m_Updates.clear();
    m_AlignLog.clear();
    m_AlignStatus = Ekos::ALIGN_IDLE;
    // Every test starts a new failure chain
    Ekos::Manager::Instance()->diagnosisManager()->clear();

    // Align needs the mount position, which the simulator reports once it tracks
    Ekos::Align *align = Ekos::Manager::Instance()->alignModule();
    if (!align->telescopeCoordinates().isValid())
        Ekos::Manager::Instance()->mountModule()->setTrackEnabled(true);
    QTRY_VERIFY_WITH_TIMEOUT(align->telescopeCoordinates().isValid(), 60000);
}

void TestEkosAlignDiagnosis::cleanup()
{
    Ekos::Align *align = Ekos::Manager::Instance()->alignModule();
    if (align->status() == Ekos::ALIGN_PROGRESS)
        align->abort();
    // Align stops waiting for a busy device only when it aborts or captures
    align->abort();
    align->setFocusStatus(Ekos::FOCUS_IDLE);

    Options::setAstrometryIndexFolderList(m_IndexFolders);

    auto camera = Ekos::OpticalTrainManager::Instance()->getCamera(m_CaptureHelper->m_primaryTrain);
    if (camera)
    {
        auto clientManager = camera->getDriverInfo()->getClientManager();
        if (clientManager)
        {
            clientManager->setBLOBMode(B_ONLY, camera->getDeviceName().toLatin1().constData(), "CCD1");
            clientManager->setBLOBMode(B_ONLY, camera->getDeviceName().toLatin1().constData(), "CCD2");
        }
    }

    auto mount = Ekos::OpticalTrainManager::Instance()->getMount(m_CaptureHelper->m_primaryTrain);
    if (mount && mount->parkStatus() == ISD::PARK_PARKED)
    {
        Ekos::Manager::Instance()->mountModule()->unpark();
        QTRY_VERIFY_WITH_TIMEOUT(mount->parkStatus() == ISD::PARK_UNPARKED, 60000);
    }

    if (KSMessageBox::Instance()->isVisible())
        KSMessageBox::Instance()->reject();
}

QTEST_KSTARS_MAIN(TestEkosAlignDiagnosis)

#endif // HAVE_INDI
