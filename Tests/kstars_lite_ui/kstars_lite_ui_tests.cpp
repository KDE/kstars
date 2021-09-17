/*
    SPDX-FileCopyrightText: 2017 Csaba Kertesz <csaba.kertesz@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/


#include "kstars_lite_ui_tests.h"

#include "auxiliary/kspaths.h"
#include "kstarslite.h"

#include <QFuture>
#include <QtConcurrentRun>
#include <QtTest/QtTest>

#include <time.h>
#include <unistd.h>

namespace
{
KStarsLite *kstarsLiteInstance = nullptr;
QString testProfileName;
}

void waitForKStars()
{
    while (!kstarsLiteInstance->getMainWindow() && !kstarsLiteInstance->getMainWindow()->isActive())
    {
        QThread::msleep(1000);
    }
    QThread::msleep(2000);
}

KStarsLiteUiTests::KStarsLiteUiTests()
{
    srand((int)time(nullptr));
}

void KStarsLiteUiTests::initTestCase()
{
    kstarsLiteInstance = KStarsLite::createInstance(true);
}

void KStarsLiteUiTests::cleanupTestCase()
{
    kstarsLiteInstance = nullptr;
}

void openToolbarsTest()
{
    waitForKStars();

    QObject* topMenu = kstarsLiteInstance->getMainWindow()->findChild<QObject*>("topMenu");
    QObject* topMenuArrow = kstarsLiteInstance->getMainWindow()->findChild<QObject*>("arrowDownMouseArea");
    QObject* bottomMenu = kstarsLiteInstance->getMainWindow()->findChild<QObject*>("bottomMenu");
    QObject* bottomMenuArrow = kstarsLiteInstance->getMainWindow()->findChild<QObject*>("arrowUpMouseArea");

    QCOMPARE(topMenu != nullptr, true);
    QCOMPARE(topMenuArrow != nullptr, true);
    QCOMPARE(bottomMenu != nullptr, true);
    QCOMPARE(bottomMenuArrow != nullptr, true);

    // Open the top menu
    QCOMPARE(topMenu->property("state") == "closed", true);
    QMetaObject::invokeMethod(topMenuArrow, "manualPress");
    QThread::msleep(800);
    QCOMPARE(topMenu->property("state") == "open", true);
    // Close the top menu
    QMetaObject::invokeMethod(topMenuArrow, "manualPress");
    QThread::msleep(800);
    QCOMPARE(topMenu->property("state") == "closed", true);

    // Open the bottom menu
    QCOMPARE(bottomMenu->property("state") == "closed", true);
    QMetaObject::invokeMethod(bottomMenuArrow, "manualPress");
    QThread::msleep(800);
    QCOMPARE(bottomMenu->property("state") == "open", true);
    // Close the bottom menu
    QMetaObject::invokeMethod(bottomMenuArrow, "manualPress");
    QThread::msleep(800);
    QCOMPARE(bottomMenu->property("state") == "closed", true);

    QThread::msleep(1000);
}

void KStarsLiteUiTests::openToolbars()
{
    QFuture<void> testFuture = QtConcurrent::run(openToolbarsTest);

    while (!testFuture.isFinished())
    {
        QCoreApplication::instance()->processEvents();
        usleep(20*1000);
    }
}

QTEST_MAIN(KStarsLiteUiTests);
