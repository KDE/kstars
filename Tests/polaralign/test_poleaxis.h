/*
    SPDX-FileCopyrightText: 2020 Chris Rowland <chris.rowland@cherryfield.me.uk>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef TEST_POLEAXIS_H
#define TEST_POLEAXIS_H

#include <QtTest/QtTest>
#include <QDebug>
#include <QString>

#define UNIT_TEST

#include "../../kstars/ekos/align/poleaxis.h"

/**
 * @class TestPoleAxis
 * @short Tests for some polar align operations
 * @author Chris Rowland
 */

class TestPoleAxis : public QObject
{
        Q_OBJECT

    public:
        TestPoleAxis();
        ~TestPoleAxis() override;

    private slots:
        void testDirCos_data();
        void testDirCos();
        void testPriSec_data();
        void testPriSec();
        void testPoleAxis_data();
        void testPoleAxis();

    private:
        void compare(Rotations::V3 v, double x, double y, double z);
        void compare(double a, double e, QString msg = "");
        void compare(float a, double e, QString msg = "")
        {
            compare (static_cast<double>(a), e, msg);
        }

};


#endif
