/*  KStars UI tests
    SPDX-FileCopyrightText: 2020 Eric Dejouhanet <eric.dejouhanet@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kstars_ui_tests.h"
#include "test_ekos_simulator.h"
#include "ekos/guide/guide.h"

#include "test_ekos_helper.h"
#include "kstarsdata.h"

#include <cmath>
#include <limits>

#if defined(HAVE_INDI)

#include "test_ekos.h"
#include "ksmessagebox.h"

#include <QLabel>

TestEkosSimulator::TestEkosSimulator(QObject *parent) : QObject(parent)
{
}

void TestEkosSimulator::initTestCase()
{
    KTRY_OPEN_EKOS();
    KVERIFY_EKOS_IS_OPENED();
}

void TestEkosSimulator::cleanupTestCase()
{
    KTRY_CLOSE_EKOS();
    KVERIFY_EKOS_IS_HIDDEN();
}

void TestEkosSimulator::init()
{
    KTRY_EKOS_START_SIMULATORS();

    // HACK: Reset clock to initial conditions
    KHACK_RESET_EKOS_TIME();
}

void TestEkosSimulator::cleanup()
{
    for (auto d : KStars::Instance()->findChildren<QDialog * >())
        if (d->isVisible())
            d->hide();

    KTRY_EKOS_STOP_SIMULATORS();
}


void TestEkosSimulator::testMountSlew_data()
{
#if QT_VERSION < 0x050900
    QSKIP("Skipping fixture-based test on old QT version.");
#else
    QTest::addColumn<QString>("NAME");
    QTest::addColumn<QString>("RA");
    QTest::addColumn<QString>("DEC");

    // Altitude computation taken from SchedulerJob::findAltitude
    GeoLocation * const geo = KStarsData::Instance()->geo();
    KStarsDateTime const now(KStarsData::Instance()->lt());
    KSNumbers const numbers(now.djd());
    CachingDms const LST = geo->GSTtoLST(geo->LTtoUT(now).gst());

    const bool headless = !kstarsTestRequiresActiveWindow();
    int added = 0;
    // Build a list of Messier objects, 5 degree over the horizon
    for (int i = 1; i < 103; i += 20)
    {
        QString name = QString("M %1").arg(i);
        SkyObject const * const so = KStars::Instance()->data()->objectNamed(name);
        if (so != nullptr)
        {
            SkyObject o(*so);
            o.updateCoordsNow(&numbers);
            o.EquatorialToHorizontal(&LST, geo->lat());
            if (5.0 < o.alt().Degrees())
            {
                QTest::addRow("%s", name.toStdString().c_str())
                        << name
                        << o.ra().toHMSString()
                        << o.dec().toDMSString();
                if (headless && ++added >= 1)
                    break;
            }
        }
    }
#endif
}

void TestEkosSimulator::testMountSlew()
{
#if QT_VERSION < 0x050900
    QSKIP("Skipping fixture-based test on old QT version.");
#else
    Ekos::Manager * const ekos = Ekos::Manager::Instance();

    QFETCH(QString, NAME);
    QFETCH(QString, RA);
    QFETCH(QString, DEC);
    qDebug("Test slewing to '%s' RA '%s' DEC '%s'",
           NAME.toStdString().c_str(),
           RA.toStdString().c_str(),
           DEC.toStdString().c_str());

#if 0
    // In the mount tab, open the mount control
    KTRY_EKOS_CLICK("mountToolBoxB");
    QTextObject * raInput = Ekos::Manager::Instance()->findChild<QTextObject*>("targetRATextObject");
    QVERIFY(raInput != nullptr);
    raInput->setProperty("text", QVariant("07h 37m 30s"));
    QTest::qWait(1000);
    QTextObject * deInput = Ekos::Manager::Instance()->findChild<QTextObject*>("targetDETextObject");
    QVERIFY(deInput != nullptr);
    deInput->setProperty("text", QVariant("-14° 31' 50"));
    QTest::qWait(1000);

    // Too bad, not accessible...
    QPushButton * gotoButton = Ekos::Manager::Instance()->findChild<QPushButton*>("");

    QTest::qWait(5000);
    KTRY_EKOS_CLICK("mountToolBoxB");
#else
    QVERIFY(ekos->mountModule()->abort());
    // Catch the unexpected "under horizon" and the "sun is close" dialogs
    // This will not catch the altitude security interval
    bool under_horizon_or_close_to_sun = false;
    QTimer::singleShot(1000, [&]
    {
        QDialog * const dialog = qobject_cast <QDialog*> (QApplication::activeModalWidget());
        if(dialog != nullptr)
        {
            under_horizon_or_close_to_sun = true;
            Q_EMIT dialog->reject();
        }
    });
    dms ra = dms::fromString(RA, false);
    dms de = dms::fromString(DEC, true);
    bool const slew_result = ekos->mountModule()->slew(ra.Hours(), de.Degrees());
    if (under_horizon_or_close_to_sun)
        QEXPECT_FAIL(NAME.toStdString().c_str(),
                     QString("Slew target '%1' is expected to be over the horizon during night time.").arg(NAME).toStdString().c_str(), Abort);
    QVERIFY(slew_result);
#endif

    // DEC slews are precise at plus/minus one arcsecond - expected or not?
    auto clampRA = [](const QString & value)
    {
        return CachingDms(value, false).arcsec();
    };
    auto clampDE = [](const QString & value)
    {
        return CachingDms(value, true).arcsec();
    };
    auto labelHasCoordinateText = [](const QLabel * label)
    {
        if (label == nullptr)
            return false;

        const QString text = label->text().trimmed();
        return !text.isEmpty() && text != QStringLiteral("--") && text != QStringLiteral("-");
    };
    const double targetRA = clampRA(RA);
    QLabel *raOut = ekos->findChild<QLabel *>("raOUT");
    if (raOut == nullptr)
        QWARN("raOUT label missing; falling back to mount equatorial coords.");
    auto raTolerance = [&]() -> double
    {
        return labelHasCoordinateText(raOut) ? 2.0 : 120.0;
    };
    auto currentRaArcsec = [&]() -> double
    {
        if (labelHasCoordinateText(raOut))
            return clampRA(raOut->text());

        QList<double> coords = ekos->mountModule()->equatorialCoords();
        if (coords.size() >= 2)
        {
            return dms(coords[0] * 15.0).arcsec();
        }
        return std::numeric_limits<double>::quiet_NaN();
    };
    QTRY_VERIFY_WITH_TIMEOUT(!std::isnan(currentRaArcsec()) && std::abs(targetRA - currentRaArcsec()) <= raTolerance(), 15000);
    QTest::qWait(100);
    const double currentRA = currentRaArcsec();
    if (!std::isnan(currentRA) && targetRA != currentRA)
        QWARN(QString("Target '%1', RA %2, slewed to RA %3 with offset RA %4")
              .arg(NAME)
              .arg(targetRA)
              .arg(currentRA)
              .arg(std::abs(targetRA - currentRA)).toStdString().c_str());

    const double targetDE = clampDE(DEC);
    QLabel *deOut = ekos->findChild<QLabel *>("decOUT");
    if (deOut == nullptr)
        QWARN("decOUT label missing; falling back to mount equatorial coords.");
    auto deTolerance = [&]() -> double
    {
        return labelHasCoordinateText(deOut) ? 2.0 : 120.0;
    };
    auto currentDeArcsec = [&]() -> double
    {
        if (labelHasCoordinateText(deOut))
            return clampDE(deOut->text());

        QList<double> coords = ekos->mountModule()->equatorialCoords();
        if (coords.size() >= 2)
        {
            return dms(coords[1]).arcsec();
        }
        return std::numeric_limits<double>::quiet_NaN();
    };
    QTRY_VERIFY_WITH_TIMEOUT(!std::isnan(currentDeArcsec()) && std::abs(targetDE - currentDeArcsec()) <= deTolerance(), 20000);
    QTest::qWait(100);
    const double currentDE = currentDeArcsec();
    if (!std::isnan(currentDE) && targetDE != currentDE)
        QWARN(QString("Target '%1', DEC %2, slewed to DEC %3 with coordinate offset DEC %4")
              .arg(NAME)
              .arg(targetDE)
              .arg(currentDE)
              .arg(std::abs(targetDE - currentDE)).toStdString().c_str());

    QVERIFY(Ekos::Manager::Instance()->mountModule()->abort());
#endif
}

void TestEkosSimulator::testColorSchemes_data()
{
#if QT_VERSION < QT_VERSION_CHECK(5,9,0)
    QSKIP("Skipping fixture-based test on old QT version.");
#else
    QTest::addColumn<QString>("NAME");
    QTest::addColumn<QString>("FILENAME");

    QTest::addRow("Classic, friendly name") << "Default Colors" << "classic.colors";
    QTest::addRow("Chart, friendly name") << "Star Chart" << "chart.colors",
                                          QTest::addRow("Night, friendly name") << "Night Vision" << "night.colors",
                                          QTest::addRow("Moonless, friendly name") << "Moonless Night" << "moonless-night.colors";

    QTest::addRow("Classic, short name") << "classic" << "classic.colors";
    QTest::addRow("Chart, short name") << "chart" << "chart.colors",
                                       QTest::addRow("Night, short name") << "night" << "night.colors",
                                       QTest::addRow("Moonless, short name") << "moonless-night" << "moonless-night.colors";

    QTest::addRow("Classic, full name") << "classic.colors" << "classic.colors";
    QTest::addRow("Chart, full name") << "chart.colors" << "chart.colors",
                                      QTest::addRow("Night, full name") << "night.colors" << "night.colors",
                                      QTest::addRow("Moonless, full name") << "moonless-night.colors" << "moonless-night.colors";
#endif
}

void TestEkosSimulator::testColorSchemes()
{
#if QT_VERSION < QT_VERSION_CHECK(5,9,0)
    QSKIP("Skipping fixture-based test on old QT version.");
#else
    QFETCH(QString, NAME);
    QFETCH(QString, FILENAME);

    KStars::Instance()->loadColorScheme(NAME);
    QTRY_COMPARE_WITH_TIMEOUT(KStars::Instance()->colorScheme(), FILENAME, 1000);
    QVERIFY(KStars::Instance()->data()->colorScheme()->colorNamed("RAGuideError").isValid());
    QTRY_VERIFY_WITH_TIMEOUT(Ekos::Manager::Instance()->guideModule() != nullptr, 5000);
    QTRY_COMPARE_WITH_TIMEOUT(Ekos::Manager::Instance()->guideModule()->driftGraph->graph(0)->pen().color(),
                              KStars::Instance()->data()->colorScheme()->colorNamed("RAGuideError"), 1000);
    QTRY_COMPARE_WITH_TIMEOUT(Ekos::Manager::Instance()->guideModule()->driftGraph->graph(1)->pen().color(),
                              KStars::Instance()->data()->colorScheme()->colorNamed("DEGuideError"), 1000);
    QTRY_COMPARE_WITH_TIMEOUT(Ekos::Manager::Instance()->guideModule()->driftGraph->graph(2)->pen().color(),
                              KStars::Instance()->data()->colorScheme()->colorNamed("RAGuideError"), 1000);
    QTRY_COMPARE_WITH_TIMEOUT(Ekos::Manager::Instance()->guideModule()->driftGraph->graph(3)->pen().color(),
                              KStars::Instance()->data()->colorScheme()->colorNamed("DEGuideError"), 1000);
#endif
}

void TestEkosSimulator::testProfileStopClearsDriverManager()
{
    // Regression test for the crash:
    //   INDIListener::removeClient -> QObject::disconnect (SIGSEGV)
    //
    // Exact scenario reproduced here:
    //   1. A remote profile is started whose INDI server does not exist
    //      (host=localhost, port=9999 — guaranteed to have nothing listening).
    //   2. ClientManager tries to connect, retries, then fails.
    //      Manager::setClientFailed() is called → ekosStatus becomes Ekos::Error.
    //      Inside the failure path, removeManagedDriver calls
    //      driver->setClientState(false); because clientState was NEVER set to
    //      true (connection never succeeded), the idempotency guard returns early
    //      WITHOUT calling driver->setClientManager(nullptr).
    //      ClientManager is then deleted via deleteLater().
    //   3. Manager::stop() is called — exactly as EkosLive does on STOP_PROFILE.
    //      stopDevices() iterates managed drivers; getClientManager() returns the
    //      now-dangling pointer; INDIListener::removeClient(cm) dereferences it
    //      with cm->disconnect(this) → SIGSEGV.
    //
    // Fix:
    //   ClientManager::removeManagedDriver now calls driver->setClientManager(nullptr)
    //   unconditionally (bypassing the idempotency guard).
    //   stopDevices() uses disconnectAll() to arm m_PendingDisconnection before the
    //   background thread can call serverDisconnected(), and guards with
    //   clients.contains(cm) to skip already-cleaned-up drivers.
    //
    // Pass criteria: no crash, ekos returns to Idle state.

    Ekos::Manager *ekos = Ekos::Manager::Instance();
    QVERIFY(ekos != nullptr);

    const QString badProfileName = QStringLiteral("TestBadConnectionPort");

    // init() started the Simulators profile — stop them cleanly first
    // so we can switch to the failing profile.
    KTRY_EKOS_STOP_SIMULATORS();

    // ---- Create a remote profile pointing to a port with no INDI server ----
    // TestEkosHelper::ensureRemoteProfile opens the ProfileEditor dialog and uses
    // ProfileEditor::setSettings to configure all fields (name, mode=remote, host,
    // port, drivers) in a single clean call, then saves via the Save button.
    // "Telescope Simulator" is added so that a DriverInfo object is created and
    // assigned a ClientManager — this is the pointer that becomes dangling after
    // the failed connection, causing the crash before the fix.
    QVERIFY2(TestEkosHelper::ensureRemoteProfile(
                 badProfileName, QStringLiteral("localhost"), 9999,
    {QStringLiteral("Telescope Simulator"), QStringLiteral("CCD Simulator")}),
    "Failed to create test profile via ProfileEditor");

    // ---- Select and start the bad profile ----
    KTRY_EKOS_SELECT_PROFILE(badProfileName);
    KTRY_EKOS_CLICK(processINDIB);

    // Wait for connection failure (ClientManager retries a few times, ~4-8 s total).
    // After failure, Manager::setClientFailed() sets ekosStatus = Ekos::Error.
    // Before the fix, DriverInfo::clientManager held a dangling pointer to the
    // deleted ClientManager at this point.
    QTRY_VERIFY_WITH_TIMEOUT(ekos->ekosStatus() == Ekos::Error, 30000);

    // ---- Call Manager::stop() — the EXACT EkosLive STOP_PROFILE code path ----
    // Before the fix this would crash at:
    //   stopDevices → INDIListener::removeClient(cm) → cm->disconnect(this)
    // because cm was a dangling pointer.
    // After the fix, clientManager is nullptr and the driver is skipped.
    ekos->stop();  // Must NOT crash

    QTRY_VERIFY_WITH_TIMEOUT(ekos->indiStatus() == Ekos::Idle, 10000);

    // ---- Clean up the test profile ----
    ekos->deleteProfile(badProfileName);

    // ---- Re-start simulators so cleanup() -> KTRY_EKOS_STOP_SIMULATORS works ----
    KTRY_EKOS_START_SIMULATORS();
}

void TestEkosSimulator::testRapidProfileStartStop()
{
    // Regression test for the crash:
    //   ServerManager::~ServerManager() -> ~QList<QSharedPointer<DriverInfo>>()
    //   (SIGSEGV in std::__atomic_base<T>::fetch_sub)
    //
    // Exact scenario reproduced here:
    //   1. Manager::start() creates a ServerManager and calls
    //      serverManager->start(), which spawns the indiserver process.
    //      Once that process signals started(), DriverManager::setServerStarted()
    //      dispatches QtConcurrent::run(&ServerManager::startDriver, sm, driver)
    //      for the first driver, via a raw ServerManager* with a discarded
    //      QFuture (no lifetime tracking) — well before the drivers have
    //      actually finished connecting (Ekos::Success).
    //   2. Manager::stop() is called immediately afterwards, exactly as a
    //      real user clicking Stop right after Start (or EkosLive issuing
    //      STOP_PROFILE) would. This calls DriverManager::stopDevices() ->
    //      sm->deleteLater(), which can run ServerManager's destructor while
    //      the dispatched startDriver() call is still executing on the
    //      background thread.
    //   3. The destructor implicitly tears down m_ManagedDrivers /
    //      m_PendingDrivers (QList<QSharedPointer<DriverInfo>>) without
    //      holding m_DriverMutex/m_PendingMutex, racing the background
    //      thread's own locked list access and corrupting the shared_ptr
    //      refcount.
    //
    // Fix:
    //   ServerManager's destructor now takes m_DriverMutex and m_PendingMutex
    //   before tearing down the driver lists, so it cannot run while
    //   startDriver()/stopDriver()/restartDriver() is still "checked in"
    //   against them. This closes the race for a task that has already
    //   started; it does not (and cannot, without QFuture tracking) protect
    //   against a task that is dispatched but not yet running.
    //
    // This race is timing-dependent (it needs the indiserver process to have
    // signalled started() and the background startDriver() task to be
    // in-flight), so the Start/Stop cycle below is repeated with only a
    // short, deliberately tight wait in between, to make it very likely to
    // land inside the vulnerable window on an unfixed build; the fix makes
    // every iteration safe regardless of timing.
    //
    // Pass criteria: no crash, and the Simulators profile ends up Idle.

    Ekos::Manager * const ekos = Ekos::Manager::Instance();
    QVERIFY(ekos != nullptr);

    // init() already fully started the Simulators profile — stop it first so
    // we begin the rapid-cycle loop from a clean, known Idle state.
    KTRY_EKOS_STOP_SIMULATORS();

    KTRY_EKOS_SELECT_PROFILE("Simulators");

    for (int i = 0; i < 8; i++)
    {
        // Start dispatches the background driver-start task; give the
        // indiserver process just enough time to spawn and for that task to
        // be dispatched (but not to finish) before stopping again.
        ekos->start();
        QTest::qWait(50);

        // Stop immediately, racing ServerManager's destructor against the
        // still in-flight (or just-finished) background driver-start task.
        ekos->stop(); // Must NOT crash

        // Let the deferred ServerManager::deleteLater() actually run.
        QTest::qWait(50);

        QTRY_VERIFY_WITH_TIMEOUT(ekos->indiStatus() == Ekos::Idle, 10000);
        QTRY_VERIFY_WITH_TIMEOUT(ekos->ekosStatus() == Ekos::Idle, 10000);
    }

    // ---- Re-start simulators cleanly so cleanup() -> KTRY_EKOS_STOP_SIMULATORS works ----
    KTRY_EKOS_START_SIMULATORS();
}

QTEST_KSTARS_MAIN(TestEkosSimulator)

#endif // HAVE_INDI
