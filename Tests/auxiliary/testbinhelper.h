/*  KStars UI tests
    SPDX-FileCopyrightText: 2020 Eric Dejouhanet <eric.dejouhanet@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef TESTBINHELPER_H
#define TESTBINHELPER_H

#include <QtGlobal>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QtTest/QTest>
#else
#include <QTest>
#endif

#include <QObject>

class TestBinHelper : public QObject
{
        Q_OBJECT
    public:
        explicit TestBinHelper(QObject *parent = nullptr);

    private slots:
        void initTestCase();
        void cleanupTestCase();

        void init();
        void cleanup();

        void testLoadBinary_data();
        void testLoadBinary();
};

#endif // TESTBINHELPER_H
