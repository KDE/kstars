/*
    SPDX-FileCopyrightText: 2016 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QObject>

class TestDMS : public QObject
{
    Q_OBJECT
  public:
    TestDMS();
    ~TestDMS() override = default;

    void checkCtor_data();

  private slots:
    void defaultCtor();
    void explicitSexigesimalCtor();
    void angleCtor();
    void stringCtor();
    void testReduceToRange();
    void testSubstraction();
    void testDeltaAngle();
    void testUnitTransition();
    void testPrecisionTransition();
};
