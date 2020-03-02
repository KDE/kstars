/*  KStars UI tests
    Copyright (C) 2017 Csaba Kertesz <csaba.kertesz@gmail.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include "kstars_ui_tests.h"
#include "test_ekos.h"
#include "test_ekos_simulator.h"

#include "auxiliary/kspaths.h"
#if defined(HAVE_INDI)
#include "ekos/manager.h"
#include "ekos/profileeditor.h"
#endif

#include <KActionCollection>
#include <KTipDialog>
#include <KCrash/KCrash>

#include <QFuture>
#include <QtConcurrentRun>
#include <QtTest>
#include <QTest>
#include <QDateTime>

#include <ctime>
#include <unistd.h>

struct KStarsUiTests::_InitialConditions const KStarsUiTests::m_InitialConditions;

KStarsUiTests::KStarsUiTests(QObject *parent): QObject(parent)
{
}

void KStarsUiTests::initTestCase()
{
}

void KStarsUiTests::cleanupTestCase()
{
}

void KStarsUiTests::init()
{
    if (KStars::Instance() != nullptr)
        KTRY_SHOW_KSTARS();
}

void KStarsUiTests::cleanup()
{
    foreach (QDialog * d, KStars::Instance()->findChildren<QDialog*>())
        if (d->isVisible())
            d->hide();
}

// All QTest features are macros returning with no error code.
// Therefore, in order to bail out at first failure, tests cannot use functions to run sub-tests and are required to use grouping macros too.

void KStarsUiTests::createInstanceTest()
{
    // Initialize our instance
    /*QBENCHMARK_ONCE*/ {
        // Create our test instance
        KTipDialog::setShowOnStart(false);
        KStars::createInstance(true, KStarsUiTests::m_InitialConditions.clockRunning, KStarsUiTests::m_InitialConditions.dateTime.toString());
        QApplication::processEvents();
    }
    QVERIFY(KStars::Instance() != nullptr);

    // Initialize our location
    GeoLocation * const g = KStars::Instance()->data()->locationNamed("Greenwich");
    QVERIFY(g != nullptr);
    KStars::Instance()->data()->setLocation(*g);
    QCOMPARE(KStars::Instance()->data()->geo()->lat()->Degrees(), g->lat()->Degrees());
    QCOMPARE(KStars::Instance()->data()->geo()->lng()->Degrees(), g->lng()->Degrees());
}

void KStarsUiTests::raiseKStarsTest()
{
    KTRY_SHOW_KSTARS();
}

void KStarsUiTests::initialConditionsTest()
{
    QVERIFY(KStars::Instance() != nullptr);
    QVERIFY(KStars::Instance()->data() != nullptr);
    QVERIFY(KStars::Instance()->data()->clock() != nullptr);

    QCOMPARE(KStars::Instance()->data()->clock()->isActive(), m_InitialConditions.clockRunning);

    QEXPECT_FAIL("", "Initial KStars clock is set from system local time, not geolocation, and is untestable for now.", Continue);
    QCOMPARE(KStars::Instance()->data()->clock()->utc().toString(), m_InitialConditions.dateTime.toString());

    QEXPECT_FAIL("", "Precision of KStars local time conversion to local time does not allow strict millisecond comparison.", Continue);
    QCOMPARE(KStars::Instance()->data()->clock()->utc().toLocalTime(), m_InitialConditions.dateTime);

    // However comparison down to nearest second is expected to be OK
    QCOMPARE(llround(KStars::Instance()->data()->clock()->utc().toLocalTime().toMSecsSinceEpoch()/1000.0), m_InitialConditions.dateTime.toSecsSinceEpoch());
}

// We want to launch the application before running our tests
// Thus we want to explicitly call QApplication::exec(), and run our tests in parallel of the event loop
// We then reimplement QTEST_MAIN(KStarsUiTests);
// The same will have to be done when interacting with a modal dialog: exec() in main thread, tests in timer-based thread

QT_BEGIN_NAMESPACE
QTEST_ADD_GPU_BLACKLIST_SUPPORT_DEFS
QT_END_NAMESPACE

int main(int argc, char *argv[])
{
    // Create our test application
    QApplication app(argc, argv);
    app.setAttribute(Qt::AA_Use96Dpi, true);
    QTEST_ADD_GPU_BLACKLIST_SUPPORT
    QTEST_SET_MAIN_SOURCE_PATH
    QApplication::processEvents();

    // Prepare our configuration
    srand((unsigned int)time(nullptr));
    QDir writableDir;
    writableDir.mkdir(KSPaths::writableLocation(QStandardPaths::GenericDataLocation));
    KCrash::initialize();

    // The instance will be created (and benchmarked) as first test in KStarsUiTests
    qDebug("Deferring instance creation to tests.");

    // This holds the final result of the test session
    int result = 0;

    // Execute tests in sequence, eventually skipping sub-tests based on prior ones
    QTimer::singleShot(1000, &app, [&]
    {
        qDebug("Starting tests...");

        // This creates our KStars instance
        KStarsUiTests * tc = new KStarsUiTests();
        result = QTest::qExec(tc, argc, argv);

#if defined(HAVE_INDI)
        if (!result)
        {
            TestEkos * ek = new TestEkos();
            result |= QTest::qExec(ek, argc, argv);
        }

        if (!result)
        {
            TestEkosSimulator * eks = new TestEkosSimulator();
            result |= QTest::qExec(eks, argc, argv);
        }
#endif

        // Done testing, successful or not
        qDebug("Tests are done.");
        app.quit();
    });

    // Limit execution duration
    QTimer::singleShot(5*60*1000, &app, &QCoreApplication::quit);

    app.exec();
    KStars::Instance()->close();
    delete KStars::Instance();

    return result;
}

