/*
    KStars UI tests for automatic polar alignment correction with the PAC simulator

    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "test_ekos_pac_sim.h"

#if defined(HAVE_INDI)

#include "kstars_ui_tests.h"
#include "test_ekos.h"
#include "test_ekos_simulator.h"
#include "Options.h"
#include "ekos/align/align.h"
#include "ekos/align/polaralignmentassistant.h"
#include "ekos/mount/mount.h"
#include "kstarsdata.h"
#include "indi/guimanager.h"
#include "indi/indilistener.h"
#include "indi/indipac.h"

#include <QProcess>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QStackedWidget>
#include <QTimeZone>

#define PAC_DEVICE "Alignment Correction Simulator"
#define MOUNT_DEVICE "Telescope Simulator"
#define CCD_DEVICE "CCD Simulator"
#define PROFILE_NAME "PAC Simulator Test"

namespace
{
int envInt(const char *name, int defaultValue)
{
    bool ok = false;
    const int value = qEnvironmentVariable(name).toInt(&ok);
    return ok ? value : defaultValue;
}

ISD::PAC *findPAC()
{
    QSharedPointer<ISD::GenericDevice> device;
    if (!INDIListener::findDevice(PAC_DEVICE, device))
        return nullptr;
    return device->getPAC();
}
}

TestEkosPACSim::TestEkosPACSim(QObject *parent) : QObject(parent)
{
    m_Helper = new TestEkosHelper();
    m_Helper->m_MountDevice = MOUNT_DEVICE;
    m_Helper->m_CCDDevice = CCD_DEVICE;
    m_Port = envInt("PAC_TEST_PORT", 7630);
}

bool TestEkosPACSim::setProperty(const QString &property, const QString &value)
{
    const int rc = QProcess::execute("indi_setprop", {"-p", QString::number(m_Port), property + "=" + value});
    return rc == 0;
}

QString TestEkosPACSim::getProperty(const QString &property)
{
    QProcess p;
    p.start("indi_getprop", {"-p", QString::number(m_Port), "-1", property});
    p.waitForFinished(5000);
    return QString::fromLocal8Bit(p.readAllStandardOutput()).trimmed();
}

bool TestEkosPACSim::logContains(const QString &text) const
{
    for (const auto &line : m_AlignLog)
        if (line.contains(text))
            return true;
    return false;
}

void TestEkosPACSim::dumpLog() const
{
    for (const auto &line : m_AlignLog)
        if (line.contains("PAC") || line.contains("Refresh solver") || line.contains("error"))
            qCInfo(KSTARS_EKOS_TEST).noquote() << "PAALOG" << line;
}

bool TestEkosPACSim::measurePolarError(int ma, int me, double *az, double *alt)
{
    KVERIFY_SUB(setProperty(MOUNT_DEVICE ".MOUNT_MODEL.MM_MA;MM_ME", QString("%1;%2").arg(ma).arg(me)));

    Ekos::Mount *mount = Ekos::Manager::Instance()->mountModule();
    KVERIFY_SUB(mount->slew(KStarsData::Instance()->lst()->Hours(), 80.0));
    KTRY_VERIFY_WITH_TIMEOUT_SUB(mount->status() == ISD::Mount::MOUNT_SLEWING, 30000);
    KTRY_VERIFY_WITH_TIMEOUT_SUB(mount->status() == ISD::Mount::MOUNT_TRACKING, 120000);

    Ekos::PolarAlignmentAssistant *paa = Ekos::Manager::Instance()->alignModule()->polarAlignmentAssistant();
    auto algorithm = paa->findChild<QComboBox *>("pAHRefreshAlgorithm");
    algorithm->setCurrentIndex(Ekos::PolarAlignmentAssistant::PLATE_SOLVE_ALGORITHM);
    auto widgets = paa->findChild<QStackedWidget *>("PAHWidgets");
    auto refreshPage = paa->findChild<QWidget *>("PAHRefreshPage");
    auto startB = paa->findChild<QPushButton *>("PAHStartB");

    m_AlignLog.clear();
    m_LogClock.start();
    KTRY_VERIFY_WITH_TIMEOUT_SUB(startB->isEnabled(), 30000);
    QTest::mouseClick(startB, Qt::LeftButton);
    KTRY_VERIFY_WITH_TIMEOUT_SUB(widgets->currentWidget() == refreshPage, 300000);

    // e.g. "Polar Alignment Error:  00° 48' 11". Azimuth: -00° 48' 11"  Altitude:  00° 00' 36""
    static const QRegularExpression re(QStringLiteral(
                                           "Azimuth:\\s*(-?)(\\d+)° (\\d+)' (\\d+)\"\\s+Altitude:\\s*(-?)(\\d+)° (\\d+)' (\\d+)\""));
    for (const auto &line : m_AlignLog)
    {
        auto m = re.match(line);
        if (!m.hasMatch())
            continue;
        auto value = [&m](int i)
        {
            const double v = m.captured(i + 1).toDouble() * 60 + m.captured(i + 2).toDouble() + m.captured(i + 3).toDouble() / 60;
            return m.captured(i) == "-" ? -v : v;
        };
        *az = value(1);
        *alt = value(5);
        return true;
    }
    KVERIFY2_SUB(false, "No polar alignment error in the log");
    return false;
}

bool TestEkosPACSim::stopPAA()
{
    Ekos::PolarAlignmentAssistant *paa = Ekos::Manager::Instance()->alignModule()->polarAlignmentAssistant();
    auto stopB = paa->findChild<QPushButton *>("PAHStopB");
    if (stopB && stopB->isEnabled())
        QTest::mouseClick(stopB, Qt::LeftButton);
    auto startB = paa->findChild<QPushButton *>("PAHStartB");
    KTRY_VERIFY_WITH_TIMEOUT_SUB(startB->isEnabled(), 30000);
    return true;
}

void TestEkosPACSim::testDeadband()
{
    if (qEnvironmentVariableIsSet("PAC_TEST_SKIP_DEADBAND"))
        QSKIP("PAC_TEST_SKIP_DEADBAND is set");
    // Full gain at the slowest speed; the measured errors only need to be read once
    QVERIFY(setProperty(PAC_DEVICE ".CORRECTION_GAIN.GAIN", "100"));
    QVERIFY(setProperty(PAC_DEVICE ".PAC_SPEED.PAC_SPEED_VALUE", "1"));
    Options::setPAHAutoPACCorrection(true);
    Options::setPAHAutoChangeDirection(true);
    Options::setPAHCorrectionTimeout(600);
    Options::setPAHAutoPark(false);

    // The simulator does not map MM_MA/MM_ME onto the PAA axes consistently, so try several polar errors
    // until one axis measures below 7% of the total, which leaves room for a threshold that keeps
    // the total above it and that axis inside its 10% deadband.
    const QList<QPair<int, int>> errors = {{30, 0}, {0, 30}, {30, 30}, {30, -30}, {40, 10}, {10, 40}, {-30, 20}, {20, -40}};
    for (const auto &error : errors)
    {
        double az = 0, alt = 0;
        QVERIFY(measurePolarError(error.first, error.second, &az, &alt));
        const double total = std::hypot(az, alt);
        const double small = std::min(std::fabs(az), std::fabs(alt));
        qCInfo(KSTARS_EKOS_TEST) << "MA" << error.first << "ME" << error.second << "measured az" << az << "alt" << alt;
        if (small > 0.07 * total)
        {
            QVERIFY(stopPAA());
            continue;
        }

        const bool altSmall = std::fabs(alt) < std::fabs(az);
        const double threshold = std::max(1.0, std::min(small * 10 * 1.3, total * 0.8));
        Options::setPAHSuccessThreshold(threshold);
        qCInfo(KSTARS_EKOS_TEST) << "Success threshold" << threshold << "deadband" << threshold * 0.1
                                 << (altSmall ? "for altitude" : "for azimuth");

        auto refreshB =
            Ekos::Manager::Instance()->alignModule()->polarAlignmentAssistant()->findChild<QPushButton *>("PAHRefreshB");
        QTRY_VERIFY_WITH_TIMEOUT(refreshB->isEnabled(), 30000);
        QTest::mouseClick(refreshB, Qt::LeftButton);
        QTRY_VERIFY_WITH_TIMEOUT(logContains("PAC: Commanding corrector") || logContains("PAC: Polar alignment successful"),
                                 120000);
        dumpLog();
        QVERIFY(stopPAA());

        QVERIFY2(logContains(altSmall ? "not correcting altitude" : "not correcting azimuth"),
                 "The axis inside the deadband was sent to the PAC");
        QVERIFY2(logContains(altSmall ? "Alt: 0.0'" : "Az: 0.0'"), "The skipped axis was not commanded as 0");
        return;
    }
    QSKIP("No polar error measured with one axis small enough to test the deadband");
}

void TestEkosPACSim::testAutoCorrection()
{
    const int timeout = envInt("PAC_TEST_TIMEOUT", 600);
    const int gain = envInt("PAC_TEST_GAIN", 50);
    const double threshold = envInt("PAC_TEST_THRESHOLD", 5);

    // Polar axis 30' off in azimuth, none in altitude. The altitude error stays inside the deadband.
    QVERIFY(setProperty(MOUNT_DEVICE ".MOUNT_MODEL.MM_MA;MM_ME", QString("%1;%2")
                        .arg(envInt("PAC_TEST_MA", 30)).arg(envInt("PAC_TEST_ME", 0))));
    QVERIFY(setProperty(PAC_DEVICE ".CORRECTION_GAIN.GAIN", QString::number(gain)));
    // Slowest simulator speed: every correction takes 5 s
    QVERIFY(setProperty(PAC_DEVICE ".PAC_SPEED.PAC_SPEED_VALUE", "1"));

    Options::setPAHAutoPACCorrection(true);
    Options::setPAHAutoChangeDirection(true);
    Options::setPAHSuccessThreshold(threshold);
    Options::setPAHCorrectionTimeout(timeout);
    Options::setPAHAutoPark(false);

    Ekos::Align *align = Ekos::Manager::Instance()->alignModule();
    Ekos::PolarAlignmentAssistant *paa = align->polarAlignmentAssistant();
    QVERIFY(paa != nullptr);

    auto algorithm = paa->findChild<QComboBox *>("pAHRefreshAlgorithm");
    QVERIFY(algorithm != nullptr);
    algorithm->setCurrentIndex(Ekos::PolarAlignmentAssistant::PLATE_SOLVE_ALGORITHM);
    auto manualSlew = paa->findChild<QCheckBox *>("pAHManualSlew");
    if (manualSlew)
        manualSlew->setChecked(false);
    auto exposure = paa->findChild<QDoubleSpinBox *>("pAHExposure");
    if (exposure)
        exposure->setValue(2.0);

    auto widgets = paa->findChild<QStackedWidget *>("PAHWidgets");
    auto refreshPage = paa->findChild<QWidget *>("PAHRefreshPage");
    auto startB = paa->findChild<QPushButton *>("PAHStartB");
    QVERIFY(widgets && refreshPage && startB);

    // Slew near the pole on the meridian. The simulator only reports its position when it changes,
    // and the PAA needs valid mount coordinates to start.
    Ekos::Mount *mount = Ekos::Manager::Instance()->mountModule();
    QVERIFY(mount->slew(KStarsData::Instance()->lst()->Hours(), 80.0));
    QTRY_VERIFY_WITH_TIMEOUT(mount->status() == ISD::Mount::MOUNT_SLEWING, 30000);
    QTRY_VERIFY_WITH_TIMEOUT(mount->status() == ISD::Mount::MOUNT_TRACKING, 120000);

    m_AlignLog.clear();
    m_LogClock.start();

    QTRY_VERIFY_WITH_TIMEOUT(startB->isEnabled(), 30000);
    QTest::mouseClick(startB, Qt::LeftButton);

    // Three captures and solves with mount rotations, then the refresh page
    QTRY_VERIFY2_WITH_TIMEOUT(widgets->currentWidget() == refreshPage,
                              QString("PAA did not reach the refresh page:\n%1").arg(m_AlignLog.join("\n")).toLocal8Bit(),
                              300000);
    qCInfo(KSTARS_EKOS_TEST) << "PAA measured the polar error, starting the refresh";

    auto refreshB = paa->findChild<QPushButton *>("PAHRefreshB");
    QVERIFY(refreshB != nullptr);
    QTRY_VERIFY_WITH_TIMEOUT(refreshB->isEnabled(), 30000);
    QTest::mouseClick(refreshB, Qt::LeftButton);

    QTRY_VERIFY_WITH_TIMEOUT(logContains("PAC: Polar alignment successful") || logContains("PAC: Correction timeout")
                             || logContains("PAC: No improvement after") || logContains("PAC: Correction failed"),
                             (timeout + 600) * 1000);

    dumpLog();
    qCInfo(KSTARS_EKOS_TEST).noquote() << "Final mount model:" << getProperty(MOUNT_DEVICE ".MOUNT_MODEL.MM_MA")
                                       << getProperty(MOUNT_DEVICE ".MOUNT_MODEL.MM_ME");

    QVERIFY2(logContains("PAC: Polar alignment successful"), "The PAA did not reach the success threshold");
}

void TestEkosPACSim::testZeroCorrectionCompletes()
{
    ISD::PAC *pac = findPAC();
    QVERIFY(pac != nullptr);

    QSignalSpy spy(pac, &ISD::PAC::newStatus);
    for (int i = 0; i < 2; i++)
    {
        const int corrected = static_cast<int>(std::count_if(spy.cbegin(), spy.cend(), [](const QList<QVariant> &args)
        {
            return args.at(0).value<ISD::PAC::Status>() == ISD::PAC::PAC_CORRECTED;
        }));

        // Both axes 0: the interface completes the adjustment right away, without Busy
        QVERIFY(pac->startCorrection(0, 0));
        QTRY_VERIFY2_WITH_TIMEOUT(std::count_if(spy.cbegin(), spy.cend(), [](const QList<QVariant> &args)
        {
            return args.at(0).value<ISD::PAC::Status>() == ISD::PAC::PAC_CORRECTED;
        }) == corrected + 1, QString("Correction %1 without Busy did not complete").arg(i + 1).toLocal8Bit(), 5000);
    }
}

void TestEkosPACSim::alignLogReceived(const QString &text)
{
    m_AlignLog.append(QString("[%1s] %2").arg(m_LogClock.isValid() ? m_LogClock.elapsed() / 1000.0 : 0, 0, 'f', 1)
                      .arg(text));
}

void TestEkosPACSim::initTestCase()
{
    KVERIFY_EKOS_IS_HIDDEN();
    KTRY_OPEN_EKOS();
    KVERIFY_EKOS_IS_OPENED();
    QStandardPaths::setTestModeEnabled(true);

    Options::setAstrometryUseImageScale(true);
    Options::setAstrometryImageScaleUnits(1);
    Options::setAstrometryUsePosition(true);
    Options::setAstrometryDynamicThreshold(false);
    Options::setUseTimeUpdate(true);
    Options::setTimeSource("KStars");
    Options::setUseGeographicUpdate(true);
    Options::setLocationSource("KStars");
    const QString indexDir = qEnvironmentVariable("PAC_TEST_INDEX_DIR");
    if (!indexDir.isEmpty())
        Options::setAstrometryIndexFolderList({indexDir});

    // Remote profile on 127.0.0.1: Ekos does not manage (or kill) any INDI server
    QVERIFY(TestEkosHelper::ensureRemoteProfile(PROFILE_NAME, "127.0.0.1", m_Port,
    {MOUNT_DEVICE, CCD_DEVICE, PAC_DEVICE}));
    KTRY_EKOS_SELECT_PROFILE(PROFILE_NAME);
    KTRY_EKOS_CLICK(processINDIB);
    QTRY_VERIFY_WITH_TIMEOUT(Ekos::Manager::Instance()->indiStatus() == Ekos::Success, 30000);
    QTRY_VERIFY_WITH_TIMEOUT(findPAC() != nullptr, 30000);

    // The simulator applies the polar error in its own hour angle frame, so its clock must follow KStars.
    // Otherwise the error appears rotated by the difference in sidereal time. TIME_UTC holds the time KStars
    // sent and does not advance, so allow for the time since then; a wrong clock is off by hours or years.
    auto simulatorClockOffset = [this]()
    {
        // e.g. 2026-11-15T10:13:38+00:00; a value without an offset is UTC as well
        // indi_getprop -1 prints the bare value
        const QString utc = getProperty(MOUNT_DEVICE ".TIME_UTC.UTC").section('=', -1);
        QDateTime simulator = QDateTime::fromString(utc, Qt::ISODate);
        if (simulator.timeSpec() == Qt::LocalTime)
            simulator.setTimeZone(QTimeZone::utc());
        if (!simulator.isValid())
            return std::numeric_limits<qint64>::max();
        return std::abs(simulator.secsTo(KStarsData::Instance()->clock()->utc()));
    };
    QTRY_VERIFY2_WITH_TIMEOUT(simulatorClockOffset() < 600,
                              QString("The telescope simulator does not follow the KStars time: %1, KStars %2")
                              .arg(getProperty(MOUNT_DEVICE ".TIME_UTC.UTC"),
                                   KStarsData::Instance()->clock()->utc().toString(Qt::ISODate)).toLocal8Bit(), 30000);

    m_Helper->prepareOpticalTrains();
    m_Helper->prepareMountModule();
    m_Helper->prepareAlignmentModule();
    GUIManager::Instance()->close();

    connect(Ekos::Manager::Instance()->alignModule(), &Ekos::Align::newLog, this, &TestEkosPACSim::alignLogReceived,
            Qt::UniqueConnection);
}

void TestEkosPACSim::cleanupTestCase()
{
    disconnect(Ekos::Manager::Instance()->alignModule(), &Ekos::Align::newLog, this,
               &TestEkosPACSim::alignLogReceived);
    Ekos::Manager::Instance()->stop();
    QTRY_VERIFY_WITH_TIMEOUT(Ekos::Manager::Instance()->indiStatus() == Ekos::Idle, 10000);
    Ekos::Manager::Instance()->deleteProfile(PROFILE_NAME);
    KTRY_CLOSE_EKOS();
    KVERIFY_EKOS_IS_HIDDEN();
}

QTEST_KSTARS_MAIN(TestEkosPACSim)

#endif // HAVE_INDI
