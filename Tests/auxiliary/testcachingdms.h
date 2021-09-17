/*
    SPDX-FileCopyrightText: 2016 Akarsh Simha <akarsh.simha@kdemail.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QObject>

/**
 * @class TestCachingDms
 * @short Tests for CachingDms
 * @author Akarsh Simha <akarsh.simha@kdemail.net>
 */
class TestCachingDms : public QObject
{
    Q_OBJECT

  public:
    /** @short Constructor */
    TestCachingDms();

    /** @short Destructor */
    ~TestCachingDms() override = default;

  private slots:
    void defaultCtor();
    void explicitSexigesimalCtor();
    void angleCtor();
    void stringCtor();
    void setUsing_atan2();
    void setUsing_asin();
    void setUsing_acos();
    void additionOperator();
    void subtractionOperator();
    void unaryMinusOperator();
    void testFailsafeUseOfBaseClassPtr();
};
