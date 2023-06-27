/*
    SPDX-FileCopyrightText: 2021 Kwon-Young Choi <kwon-young.choi@hotmail.fr>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef TEST_PLACEHOLDERPATH_H
#define TEST_PLACEHOLDERPATH_H

#include <QTest>
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

    void initTestCase();
    void testSchedulerProcessJobInfo_data();
    void testSchedulerProcessJobInfo();

//    void testCCDGenerateFilename_data();
//    void testCCDGenerateFilename();

//    void testFullNamingSequence_data();
//    void testFullNamingSequence();

    void testFlexibleNaming_data();
    void testFlexibleNaming();

    void testFlexibleNamingGlob_data();
    void testFlexibleNamingGlob();

    void testRemainingPlaceholders_data();
    void testRemainingPlaceholders();

    void testGetCompletedFileIds_data();
    void testGetCompletedFileIds();

    void cleanupTestCase();
};

#endif
