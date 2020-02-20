/*  KStars UI tests
    Copyright (C) 2017 Csaba Kertesz <csaba.kertesz@gmail.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include "kstars_ui_tests.h"

#include "auxiliary/kspaths.h"
#if defined(HAVE_INDI)
#include "ekos/manager.h"
#include "ekos/profileeditor.h"
#endif
#include "kstars.h"

#include <KActionCollection>
#include <KTipDialog>
#include <KCrash/KCrash>

#include <QFuture>
#include <QtConcurrentRun>
#include <QtTest>

#include <ctime>
#include <unistd.h>

KStarsUiTests::KStarsUiTests()
{
    srand((unsigned int)time(nullptr));
    // Create writable data dir if it does not exist
    QDir writableDir;

    writableDir.mkdir(KSPaths::writableLocation(QStandardPaths::GenericDataLocation));
    KCrash::initialize();
}

void KStarsUiTests::initTestCase()
{
    KTipDialog::setShowOnStart(false);
    K = KStars::createInstance(true);
}

void KStarsUiTests::cleanupTestCase()
{
    K->close();
    delete K;
    K = nullptr;
}

void KStarsUiTests::init()
{
    KStars* const K = KStars::Instance();
    QVERIFY(K != nullptr);
    KTRY_SHOW_KSTARS(K);

#if defined(HAVE_INDI)
    initEkos();
#endif
}

void KStarsUiTests::cleanup()
{
    foreach (QDialog * d, KStars::Instance()->findChildren<QDialog*>())
        if (d->isVisible())
            d->hide();

#if defined(HAVE_INDI)
    cleanupEkos();
#endif
}

// All QTest features are macros returning with no error code.
// Therefore, in order to bail out at first failure, tests cannot use functions to run sub-tests and are required to use grouping macros too.

 void KStarsUiTests::basicTest()
{
    QVERIFY(true);
}

void KStarsUiTests::warnTest()
{
    QWARN("This is a test expected to print a warning message.");
}

void KStarsUiTests::raiseKStarsTest()
{
    KTRY_SHOW_KSTARS(K);
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
    QApplication app(argc, argv);
    app.setAttribute(Qt::AA_Use96Dpi, true);
    QTEST_ADD_GPU_BLACKLIST_SUPPORT
    QApplication::processEvents();
    KStarsUiTests * tc = new KStarsUiTests();
    QTEST_SET_MAIN_SOURCE_PATH
    QTimer::singleShot(1000, &app, [=]() { return QTest::qExec(tc, argc, argv); });
    QTimer::singleShot(30000, &app, &QCoreApplication::quit);
    app.exec();
}

