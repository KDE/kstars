/*
    SPDX-FileCopyrightText: 2016 Akarsh Simha <akarsh.simha@kdemail.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "testcachingdms.h"

#include "auxiliary/cachingdms.h"

#include <QtTest>

#include <ctime>
#include <cstdlib>
#include <cstdint>

TestCachingDms::TestCachingDms() : QObject()
{
}

void TestCachingDms::defaultCtor()
{
    /*
     * Test 1: Check Default Constructor
    */

    // Check default empty constructor
    CachingDms d;
    QVERIFY(std::isnan(d.Degrees()));
    QVERIFY(std::isnan(d.sin()));
    QVERIFY(std::isnan(d.cos()));
}

void TestCachingDms::explicitSexigesimalCtor()
{
    /*
     * Test 2: Checks Sexigesimal Ctor
    */

    // DD:MM:SS
    // 14:55:20

    CachingDms d(14, 55, 20);

    QVERIFY(d.degree() == 14);
    QVERIFY(d.arcmin() == 55);
    QVERIFY(d.arcsec() == 20);
    QVERIFY(qFuzzyCompare(d.Degrees(), (14.0 + 55.0 / 60.0 + 20.0 / 3600.0)));
    QVERIFY(fabs(d.sin() - .25750758368074941632) < 1e-9);
    QVERIFY(fabs(d.cos() - .96627627744186177805) < 1e-9);
}

void TestCachingDms::angleCtor()
{
    /*
     * Test 3: Checks Angle Ctor
    */

    // Angle = -112.56 Degrees ---> HMS (16:29:45)

    double angle = -112.56;

    CachingDms d(angle);

    QVERIFY(d.degree() == (int)angle);

    QVERIFY(qFuzzyCompare(d.Hours(), (angle + 360) / 15.0));
    QVERIFY(d.hour() == 16);
    QVERIFY(d.minute() == 29);
    QVERIFY(d.second() == 45);
    QVERIFY(fabs(d.sin() + 0.92347828085768229015) < 1e-9);
    QVERIFY(fabs(d.cos() + 0.38365070674265630377) < 1e-9);
}

void TestCachingDms::stringCtor()
{
    QString hms("14:55:20");

    // From Degree
    CachingDms d(hms);

    QVERIFY(d.degree() == 14);
    QVERIFY(d.arcmin() == 55);
    QVERIFY(d.arcsec() == 20);
    QVERIFY(qFuzzyCompare(d.Degrees(), (14.0 + 55.0 / 60.0 + 20.0 / 3600.0)));
    QVERIFY(fabs(d.sin() - .25750758368074941632) < 1e-9);
    QVERIFY(fabs(d.cos() - .96627627744186177805) < 1e-9);

    // From Hours
    CachingDms h(hms, false);
    QVERIFY(qFuzzyCompare(h.Degrees(), d.Degrees() * 15.0));
    QVERIFY(qFuzzyCompare(h.Hours(), d.Degrees()));
}

void TestCachingDms::setUsing_asin()
{
    // Test case in first quadrant: 56.3 degrees
    CachingDms d;
    d.setUsing_asin(.83195412213048254606);
    QVERIFY(fabs(d.Degrees() - 56.3) < 1e-7);
    QVERIFY(fabs(d.cos() - .55484442744799927555) < 1e-9);

    // Test case in fourth quadrant: -56.3 degrees
    d.setUsing_asin(-.83195412213048254606);
    QVERIFY(fabs(d.Degrees() + 56.3) < 1e-7);
    QVERIFY(fabs(d.cos() - .55484442744799927555) < 1e-9);
}

void TestCachingDms::setUsing_acos()
{
    CachingDms d;

    // Test case in first quadrant: 56.3 degrees
    d.setUsing_acos(.55484442744799927555);
    QVERIFY(fabs(d.Degrees() - 56.3) < 1e-7);
    QVERIFY(fabs(d.sin() - .83195412213048254606) < 1e-9);

    // Test case in second quadrant: 123.7 degrees
    d.setUsing_acos(-0.55484442744799927555);
    QVERIFY(fabs(d.Degrees() - 123.7) < 1e-7);
    QVERIFY(fabs(d.sin() - .83195412213048254606) < 1e-9);
}

void TestCachingDms::setUsing_atan2()
{
    // Test case in first quadrant: 56.3 degrees
    CachingDms d;
    d.setUsing_atan2(2.73701935509448143467, 1.82536500102022632674);
    QVERIFY(fabs(d.Degrees() - 56.3) < 1e-7);
    QVERIFY(fabs(d.sin() - .83195412213048254606) < 1e-9);
    QVERIFY(fabs(d.cos() - .55484442744799927555) < 1e-9);

    // Test case in third quadrant: -123.7 degrees
    d.setUsing_atan2(-2.73701935509448143467, -1.82536500102022632674);
    QVERIFY(fabs(d.Degrees() + 123.7) < 1e-7);
    QVERIFY(fabs(d.sin() + .83195412213048254606) < 1e-9);
    QVERIFY(fabs(d.cos() + .55484442744799927555) < 1e-9);

    // Test case in second quadrant: 123.7 degrees
    d.setUsing_atan2(2.73701935509448143467, -1.82536500102022632674);
    QVERIFY(fabs(d.Degrees() - 123.7) < 1e-7);
    QVERIFY(fabs(d.sin() - .83195412213048254606) < 1e-9);
    QVERIFY(fabs(d.cos() + .55484442744799927555) < 1e-9);

    // Test case in fourth quadrant: -56.3 degrees
    d.setUsing_atan2(-2.73701935509448143467, +1.82536500102022632674);
    QVERIFY(fabs(d.Degrees() + 56.3) < 1e-7);
    QVERIFY(fabs(d.sin() + .83195412213048254606) < 1e-9);
    QVERIFY(fabs(d.cos() - .55484442744799927555) < 1e-9);

    // Edge case test: angle = 0
    d.setUsing_atan2(0., 1.33);
    QVERIFY(fabs(d.Degrees() - 0.) < 1e-7);
    QVERIFY(fabs(d.sin() - 0.) < 1e-9);
    QVERIFY(fabs(d.cos() - 1.) < 1e-9);

    // Edge case test: angle = 90 degrees
    d.setUsing_atan2(10.12, 0.);
    QVERIFY(fabs(d.Degrees() - 90.) < 1e-7);
    QVERIFY(fabs(d.sin() - 1.) < 1e-9);
    QVERIFY(fabs(d.cos() - 0.) < 1e-9);

    // Edge case test: angle = -90 degrees
    d.setUsing_atan2(-3.1415, 0.);
    QVERIFY(fabs(d.Degrees() + 90.) < 1e-7);
    QVERIFY(fabs(d.sin() + 1.) < 1e-9);
    QVERIFY(fabs(d.cos() - 0.) < 1e-9);

    // Edge case test: angle = 180 degrees
    d.setUsing_atan2(0., -724.);
    QVERIFY(fabs(d.Degrees() - 180.) < 1e-7);
    QVERIFY(fabs(d.sin() - 0.) < 1e-9);
    QVERIFY(fabs(d.cos() + 1.) < 1e-9);
}

void TestCachingDms::unaryMinusOperator()
{
    CachingDms d(56.3);
    qDebug() << (-d).Degrees();
    QVERIFY(qFuzzyCompare((-d).Degrees(), -56.3));
    QVERIFY(qFuzzyCompare((-d).cos(), d.cos()));
    QVERIFY(qFuzzyCompare((-d).sin(), -d.sin()));
}

void TestCachingDms::additionOperator()
{
    const double a = 123.7;
    const double b = 89.5;
    CachingDms d1(a);
    CachingDms d2(b);
    CachingDms ds       = d1 + d2;
    const double sinapb = std::sin((a + b) * dms::DegToRad);
    const double cosapb = std::cos((a + b) * dms::DegToRad);
    QVERIFY(fabs(ds.sin() - sinapb) < 1e-9);
    QVERIFY(fabs(ds.cos() - cosapb) < 1e-9);

    const double c = -34.7;
    const double d = 233.6;
    CachingDms d3(c);
    CachingDms d4(d);
    CachingDms ds2      = d3 + d4;
    const double sincpd = std::sin((c + d) * dms::DegToRad);
    const double coscpd = std::cos((c + d) * dms::DegToRad);
    QVERIFY(fabs(ds2.sin() - sincpd) < 1e-9);
    QVERIFY(fabs(ds2.cos() - coscpd) < 1e-9);
}

void TestCachingDms::subtractionOperator()
{
    const double a = 123.7;
    const double b = 89.5;
    CachingDms d1(a);
    CachingDms d2(b);
    CachingDms ds       = d1 - d2;
    const double sinamb = std::sin((a - b) * dms::DegToRad);
    const double cosamb = std::cos((a - b) * dms::DegToRad);
    QVERIFY(fabs(ds.sin() - sinamb) < 1e-9);
    QVERIFY(fabs(ds.cos() - cosamb) < 1e-9);

    const double c = -34.7;
    const double d = 233.6;
    CachingDms d3(c);
    CachingDms d4(d);
    CachingDms ds2      = d3 - d4;
    const double sincmd = std::sin((c - d) * dms::DegToRad);
    const double coscmd = std::cos((c - d) * dms::DegToRad);
    QVERIFY(fabs(ds2.sin() - sincmd) < 1e-9);
    QVERIFY(fabs(ds2.cos() - coscmd) < 1e-9);
}

void TestCachingDms::testFailsafeUseOfBaseClassPtr()
{
    typedef union angle {
        double x;
        int64_t y;
    } angle;
    const int testCases = 5000;
    std::srand(std::time(nullptr));
    for (int k = 0; k < testCases; ++k)
    {
        angle a { 0 };
        CachingDms _a;
        dms __a;
        a.y = std::rand();
        _a.setD(a.x);
        __a.setD(a.x);
        dms *d;
        if (std::rand() % 10 > 5)
            d = &_a;
        else
            d = &__a;
        angle b;
        b.y = std::rand();
        switch (std::rand() % 7)
        {
            case 0:
                d->setD(b.x);
                break;
            case 1:
                d->setH(b.x / 15.);
                break;
            case 2:
            {
                dms x(b.x);
                d->setD(x.degree(), x.arcmin(), x.arcsec(), x.marcsec());
                break;
            }
            case 3:
            {
                dms x(b.x);
                d->setFromString(x.toDMSString());
                break;
            }
            case 4:
            {
                dms x(b.x);
                d->setFromString(x.toHMSString(), false);
                break;
            }
            case 5:
            {
                dms x(b.x);
                dms y(0.0);
                *d = x + y;
                break;
            }
            case 6:
            default:
                d->setRadians(b.x * dms::DegToRad);
                break;
        }
        QVERIFY(fabs(d->sin() - sin(b.x * dms::DegToRad)) < 1e-12);
        QVERIFY(fabs(d->cos() - cos(b.x * dms::DegToRad)) < 1e-12);
    }
}

QTEST_GUILESS_MAIN(TestCachingDms)
