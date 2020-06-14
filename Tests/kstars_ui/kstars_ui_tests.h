/*  KStars UI tests
    Copyright (C) 2017 Csaba Kertesz <csaba.kertesz@gmail.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#pragma once

#include "config-kstars.h"
#include "kstars.h"
#include "kstarsdata.h"

#include <QMutex>
#include <QObject>
#include <QTimer>
#include <QApplication>
#include <QtTest>

class KStars;

// All QTest features are macros returning with no error code.
// Therefore, in order to bail out at first failure, tests cannot use functions to run sub-tests and are required to use grouping macros too.
// Tests classes in this folder should attempt to provide new macro shortcuts once their details are properly validated

// This is an example of a test class.
// It inherits from QObject, and defines private slots as test functions
class KStarsUiTests : public QObject
{
    Q_OBJECT
public:
    explicit KStarsUiTests(QObject *parent = nullptr): QObject(parent) {};

private slots:

    /** @brief Members "initTestCase" and "cleanupTestCase" trigger when the framework processes this class.
     * Member "initTestCase" is executed before any other declared test.
     * Member "cleanupTestCase" is executed after all tests are done.
     */
    /** @{ */
    void initTestCase() {};
    void cleanupTestCase() {};
    /** @} */

    /** @brief Members "init" and "cleanup" trigger when the framework process one test from this class.
     * Member "init" is executed before each declared test.
     * Member "cleanup" is executed after each test is done.
     */
    /** @{ */
    void init() {};
    void cleanup() {};
    /** @} */

    /** @brief Tests should be defined as "test<a-particular-feature>" for homogeneity.
     * Use QVERIFY and others to validate tests that reply immediately.
     * If the application needs to process events while you wait for something, use QTRY_VERIFY_WITH_TIMEOUT.
     * If the application needs to process signals while you wait for something, use QTimer::singleShot to run your QVERIFY checks with an arbitrary delay.
     * If the application opens dialogs inside signals while you wait for something but you cannot determine the delay, use
     * QTimer::singleShot with a lambda, and retrigger the timer until your check can be verified.
     */

    void testSomethingThatWorks()
    {
        QVERIFY(QString("this string contains").contains("string"));
    };

    /** @brief Tests that require fixtures can define those in "test<a-particular-feature>_data" as a record list.
     * Condition your test with QT_VERSION >= 0x050900 as this is the minimal version that supports this.
     * Add data columns to your fixture with QTest::addColum<column-type>("column-name").
     * In the same order, add data rows to your fixture with QTest::addRow("<arbitrary-row-identifier>") << column1 << column2 << ...
     * Afterwards, in your test, use QFETCH(<column-type>, <column-name>) to obtain values for one row of your fixture.
     * Your test will be run as many times as there are rows, so use initTestCase and cleanupTestCase.
     */
    /** @{ */
    void testAnotherThingThatWorks_data()
    {
#if QT_VERSION < 0x050900
        QSKIP("Skipping fixture-based test on old QT version.");
#else
        QTest::addColumn <int> ("A");
        QTest::addColumn <int> ("B");
        QTest::addColumn <int> ("C");
        QTest::addRow("1+1=2") << 1 << 1 << 2;
        QTest::addRow("1+4=5") << 1 << 4 << 5;
#endif
    };

    void testAnotherThingThatWorks()
    {
#if QT_VERSION < 0x050900
        QSKIP("Skipping fixture-based test on old QT version.");
#else
        QFETCH(int, A);
        QFETCH(int, B);
        QFETCH(int, C);
        QVERIFY(A+B == C);
#endif
    };
    /** @} */

    /** @brief Tests that are built to reproduce a bug should expect failures with QEXPECT_FAIL.
     * When the bug is resolved, that test will trigger a failure to verify the fix and may then be updated to remove the expected failure.
     * The first argument of QEXPECT_FAIL provides a way to fail on a particular fixture only.
     * If the test is not using a fixture or if the failure should trigger for all fixtures, the first argument must be "".
     */
    void testSomethingThatFails()
    {
        QEXPECT_FAIL("", "The next verification will fail, but the test will continue", Continue);
        QVERIFY(1+1 == 3);
    }
};
