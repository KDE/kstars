/*  TestPolarAlign class.
    Copyright (C) 2021 Hy Murveit

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#pragma once

#include <QtTest/QtTest>
#include <QDebug>
#include <QString>

#define UNIT_TEST

#include "../../kstars/ekos/align/poleaxis.h"
#include "../../kstars/ekos/align/polaralign.h"

/**
 * @class TestPolarAlign
 * @short Tests for the PolarAlign class.
 * @author Hy Murveit
 */

class TestPolarAlign : public QObject
{
        Q_OBJECT

    public:
        TestPolarAlign();
        ~TestPolarAlign() override;

    private slots:
        void testRunPAA();
        void testAlt();
        void testRotate();
        void testRotate_data();

    private:
        void compare(double a, double e, QString msg = "", double tolerance = 0.0003);
        void compare(const QPointF &point, double x, double y, double tolerance = 0.0001);

        void getAzAlt(const KStarsDateTime &time, const GeoLocation &geo,
                      const QPointF &pixel, double ra, double dec, double orientation,
                      double pixScale, double *az, double *alt);

};
