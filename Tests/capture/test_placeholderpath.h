/*
    SPDX-FileCopyrightText: 2021 Kwon-Young Choi <kwon-young.choi@hotmail.fr>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef TEST_PLACEHOLDERPATH_H
#define TEST_PLACEHOLDERPATH_H

#include <QtTest/QtTest>
#include <QDebug>

#define UNIT_TEST

/**
 * @class TestPlaceholderPath
 * @short Tests for some PlaceholderPath operations
 * @author Kwon-Young Choi <kwon-young.choi@hotmail.fr>
 */

class TestPlaceholderPath : public QObject
{
    Q_OBJECT

  public:
    TestPlaceholderPath();
    ~TestPlaceholderPath() override;

  private slots:

    void testSchedulerProcessJobInfo_data();
    void testSchedulerProcessJobInfo();

    void testCaptureAddJob_data();
    void testCaptureAddJob();

    void testCCDGenerateFilename_data();
    void testCCDGenerateFilename();

    void testSequenceJobSignature_data();
    void testSequenceJobSignature();

    void testFullNamingSequence_data();
    void testFullNamingSequence();

    void testFlexibleNaming_data();
    void testFlexibleNaming();
    void testFlexibleNamingChangeBehavior_data();
    void testFlexibleNamingChangeBehavior();

    void testRemainingPlaceholders_data();
    void testRemainingPlaceholders();
};

#endif
