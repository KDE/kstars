/*  KStars UI tests
    Copyright (C) 2020
    Eric Dejouhanet <eric.dejouhanet@gmail.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#ifndef TESTGEOLOCATION_H
#define TESTGEOLOCATION_H

#include <QtTest>
#include <QObject>

class TestGeolocation : public QObject
{
    Q_OBJECT
public:
    explicit TestGeolocation(QObject *parent = nullptr);

private slots:
    void initTestCase();
    void cleanupTestCase();

    void init();
    void cleanup();

    void testGeolocation_data();
    void testGeolocation();
    void testParseCityDatabase();
    void testCoordinates();
};

#endif // TESTGEOLOCATION_H
