/*
    SPDX-FileCopyrightText: 2021 Akarsh Simha <akarsh@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef TEST_STAROBJECT_H
#define TEST_STAROBJECT_H

#include <QtTest/QtTest>
#include <QDebug>

#define UNIT_TEST

#include "skyobjects/starobject.h"
#include "config-kstars.h"

/**
 * @class TestStarObject
 * @short Tests for some StarObject operations
 * @author Akarsh Simha <akarsh@kde.org>
 */

class TestStarObject : public QObject
{
        Q_OBJECT

    public:
        TestStarObject();
        ~TestStarObject() override;

    private:
        void compare(QString msg, double ra1, double dec1, double ra2, double dec2, double err = 0.0001);
        void compare(QString msg, double number1, double number2, double tolerance);

    private slots:
        void testUpdateCoordsStepByStep();
        void testUpdateCoords();
#ifdef HAVE_LIBERFA
        void compareProperMotionAgainstErfa_data();
        void compareProperMotionAgainstErfa();
#endif
    private:
        bool useRelativistic {false};
};


#endif
