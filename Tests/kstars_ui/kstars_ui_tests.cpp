/*  KStars UI tests
    Copyright (C) 2017 Csaba Kertesz <csaba.kertesz@gmail.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include "kstars_ui_tests.h"

#include "kswizard.h"
#include "config-kstars.h"
#include "auxiliary/kspaths.h"
#include "test_kstars_startup.h"

#if defined(HAVE_INDI)
#include "test_ekos_wizard.h"
#include "test_ekos.h"
#include "test_ekos_simulator.h"
#include "test_ekos_focus.h"
#include "ekos/manager.h"
#include "ekos/profileeditor.h"
#include "ekos/profilewizard.h"
#endif

#include <KActionCollection>
#include <KTipDialog>
#include <KCrash/KCrash>
#include <Options.h>

#include <QFuture>
#include <QtConcurrentRun>
#include <QtTest>
#include <QTest>
#include <QDateTime>
#include <QStandardPaths>
#include <QFileInfo>

#include <ctime>
#include <unistd.h>

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

    // Explicitly provide the RC file from the main app resources, not the user-customized one
    KStars::setResourceFile(":/kxmlgui5/kstars/kstarsui.rc");

    // This holds the final result of the test session
    int failure = 0;

    // Execute tests in sequence, eventually skipping sub-tests based on prior ones
    QTimer::singleShot(1000, &app, [&]
    {
        qDebug("Starting tests...");

        // This is a no-op test class for documentation
        KStarsUiTests * tc = new KStarsUiTests();
        failure |= QTest::qExec(tc, argc, argv);
        delete tc;

        // This cleans the test user settings, creates our instance and manages the startup wizard
        if (!failure)
        {
            TestKStarsStartup * ti = new TestKStarsStartup();
            failure |= QTest::qExec(ti, argc, argv);
            delete ti;
        }

#if defined(HAVE_INDI)
        if (!failure)
        {
            TestEkosWizard * ew = new TestEkosWizard();
            failure |= QTest::qExec(ew, argc, argv);
            delete ew;
        }

        if (!failure)
        {
            TestEkos * ek = new TestEkos();
            failure |= QTest::qExec(ek, argc, argv);
            delete ek;
        }

        if (!failure)
        {
            TestEkosSimulator * ek = new TestEkosSimulator();
            failure |= QTest::qExec(ek, argc, argv);
            delete ek;
        }

        if (!failure)
        {
            TestEkosFocus * ek = new TestEkosFocus();
            failure |= QTest::qExec(ek, argc, argv);
            delete ek;
        }
#endif

        // Done testing, successful or not
        qDebug("Tests are done.");
        app.quit();
    });

    // Limit execution duration
    QTimer::singleShot(5 * 60 * 1000, &app, &QCoreApplication::quit);

    app.exec();

    // Clean our instance up if it is still alive
    if( KStars::Instance() != nullptr)
    {
        KStars::Instance()->close();
        delete KStars::Instance();
    }

    return failure;
}

