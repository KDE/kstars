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
#include "indi/indimount.h"
#include "ksmessagebox.h"

#include <KConfigDialog>

#include <QJsonArray>
#include <QJsonDocument>
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
