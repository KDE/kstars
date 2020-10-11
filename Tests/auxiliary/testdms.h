/*  KStars Testing - DMS
    Copyright (C) 2016 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
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
