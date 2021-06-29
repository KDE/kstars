/***************************************************************************
                     test_placeholderpath.h  -  KStars Planetarium
                             -------------------
    begin                : Mon 18 Jan 2021 11:51:21 CDT
    copyright            : (c) 2021 by Kwon-Young Choi
    email                : kwon-young.choi@hotmail.fr
***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

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

    void testSequenceJobSignature_data();
    void testSequenceJobSignature();

};

#endif
