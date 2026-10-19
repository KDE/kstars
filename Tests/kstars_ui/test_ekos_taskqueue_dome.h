/*  KStars task queue dome tests with the INDI simulators
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "config-kstars.h"

#if defined(HAVE_INDI)

#include <QObject>
#include <QtTest>

class TestEkosTaskQueueDome : public QObject
{
        Q_OBJECT

    public:
        explicit TestEkosTaskQueueDome(QObject *parent = nullptr);

    private Q_SLOTS:
        void init();

        // Dome Simulator has a shutter: the observatory collections unpark and open it,
        // then close and park it.
        void testDomeWithShutterSimulator();
        // RollOff Simulator has no shutter: the shutter tasks are skipped and the roof is
        // still unparked and parked.
        void testRollOffWithoutShutterSimulator();
};

#endif // HAVE_INDI
