/*
    SPDX-FileCopyrightText: 2026 Christian Kemper <ckemper@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef TEST_MOUNTMODEL_HALTON_H
#define TEST_MOUNTMODEL_HALTON_H

#include <QtGlobal>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QtTest/QTest>
#else
#include <QTest>
#endif

class TestMountModelHalton : public QObject
{
        Q_OBJECT

    public:
        TestMountModelHalton();
        ~TestMountModelHalton() override = default;

    private Q_SLOTS:
        void testPointsAboveHorizon_data();
        void testPointsAboveHorizon();

        void testPointsAwayFromPole_data();
        void testPointsAwayFromPole();

        void testStatefulHaltonSequence();
        void testHorizonRejection();
};

#endif
