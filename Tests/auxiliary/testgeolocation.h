/*  KStars UI tests
    SPDX-FileCopyrightText: 2020 Eric Dejouhanet <eric.dejouhanet@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef TESTGEOLOCATION_H
#define TESTGEOLOCATION_H

#include <QtGlobal>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QtTest/QTest>
#else
#include <QTest>
#endif

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
