/*
    SPDX-FileCopyrightText: 2016 Akarsh Simha <akarsh.simha@kdemail.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef TEST_SKYPOINT_H
#define TEST_SKYPOINT_H

#include <QtTest/QtTest>
#include <QDebug>

#define UNIT_TEST

#include "skyobjects/skypoint.h"
#include <libnova/ln_types.h>

/**
 * @class TestSkyPoint
 * @short Tests for some SkyPoint operations
 * @author Akarsh Simha <akarsh.simha@kdemail.net>
 */

class TestSkyPoint : public QObject
{
        Q_OBJECT

    public:
        TestSkyPoint();
        ~TestSkyPoint() override;

    private:
        void compare(QString msg, double ra1, double dec1, double ra2, double dec2, double err = 0.0001);
        void compare(QString msg, SkyPoint * sp);
        void compare(QString msg, SkyPoint * sp, SkyPoint * sp1);
        void compare(QString msg, SkyPoint * sp, ln_equ_posn * lnp);
        void ln_get_equ_nut(ln_equ_posn * posn, double jd, bool reverse = false);

    private slots:
        void testPrecession();

        void compareNovas();

        void testPrecess_data();
        void testPrecess();

        void testPrecessFromAnyEpoch_data();
        void testPrecessFromAnyEpoch();

        void testNutate_data();
        void testNutate();

        void testAberrate_data();
        void testAberrate();

        void testApparentCatalogue_data();
        void testApparentCatalogue();
        void testApparentCatalogueInversion_data();
        void testApparentCatalogueInversion();

        void compareSkyPointLibNova_data();
        void compareSkyPointLibNova();

        void testUpdateCoords();

    private:
        bool useRelativistic {false};
};


#endif
