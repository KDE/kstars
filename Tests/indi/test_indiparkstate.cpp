// SPDX-FileCopyrightText: 2026 Chris Madson
// SPDX-License-Identifier: GPL-2.0-or-later

#include "indiparkstate.h"

#include <QtTest>

class TestINDIParkState : public QObject
{
        Q_OBJECT

    private slots:
        void recoveryWithoutBusy_data()
        {
            QTest::addColumn<bool>("parkSwitchOn");
            QTest::addColumn<int>("healthyState");
            QTest::newRow("failed-park-already-unparked-idle") << false << int(IPS_IDLE);
            QTest::newRow("failed-unpark-already-parked-idle") << true << int(IPS_IDLE);
            QTest::newRow("failed-park-already-unparked-ok") << false << int(IPS_OK);
            QTest::newRow("failed-unpark-already-parked-ok") << true << int(IPS_OK);
        }

        void recoveryWithoutBusy()
        {
            QFETCH(bool, parkSwitchOn);
            QFETCH(int, healthyState);
            const auto stored = parkSwitchOn ? ISD::PARK_PARKED : ISD::PARK_UNPARKED;
            const auto moving = parkSwitchOn ? ISD::PARK_UNPARKING : ISD::PARK_PARKING;
            const auto alert = ISD::updateParkState(moving, false, IPS_ALERT, parkSwitchOn);
            // These are the stored state and predicate used by Mount::parkStatus()/isParked().
            QCOMPARE(alert.status, stored);
            QCOMPARE(ISD::isParkedState(alert.status), parkSwitchOn);
            QVERIFY(alert.notify);
            QCOMPARE(alert.notification, ISD::PARK_ERROR);
            QVERIFY(alert.recoveryPending);

            const auto healthy = ISD::updateParkState(alert.status, alert.recoveryPending,
                                 IPState(healthyState), parkSwitchOn);
            QCOMPARE(healthy.status, stored);
            QVERIFY(healthy.notify);
            QCOMPARE(healthy.notification, stored);
            QVERIFY(!healthy.recoveryPending);
            const auto duplicate = ISD::updateParkState(healthy.status, healthy.recoveryPending,
                                   IPState(healthyState), parkSwitchOn);
            QCOMPARE(duplicate.status, stored);
            QVERIFY(!duplicate.notify);
            QVERIFY(!duplicate.recoveryPending);
        }

        void busyClearsRecovery_data()
        {
            QTest::addColumn<bool>("parkSwitchOn");
            QTest::newRow("park") << true;
            QTest::newRow("unpark") << false;
        }

        void busyClearsRecovery()
        {
            QFETCH(bool, parkSwitchOn);
            const auto moving = parkSwitchOn ? ISD::PARK_PARKING : ISD::PARK_UNPARKING;
            const auto alert = ISD::updateParkState(ISD::PARK_UNKNOWN, false, IPS_ALERT, parkSwitchOn);
            const auto busy = ISD::updateParkState(alert.status, alert.recoveryPending, IPS_BUSY, parkSwitchOn);
            QCOMPARE(busy.status, moving);
            QVERIFY(busy.notify);
            QVERIFY(!busy.recoveryPending);
            const auto repeated = ISD::updateParkState(busy.status, busy.recoveryPending, IPS_BUSY, parkSwitchOn);
            QVERIFY(!repeated.notify);
            QVERIFY(!repeated.recoveryPending);
            // Even an unchanged Busy state consumes a pending recovery notification.
            const auto unchanged = ISD::updateParkState(moving, true, IPS_BUSY, parkSwitchOn);
            QVERIFY(!unchanged.notify);
            QVERIFY(!unchanged.recoveryPending);
        }

        void independentMounts()
        {
            const auto failed = ISD::updateParkState(ISD::PARK_UNPARKED, false, IPS_ALERT, false);
            const auto other = ISD::updateParkState(ISD::PARK_UNPARKED, false, IPS_IDLE, false);
            QVERIFY(failed.recoveryPending);
            QVERIFY(!other.recoveryPending);
            QVERIFY(!other.notify);
        }
};

QTEST_GUILESS_MAIN(TestINDIParkState)
#include "test_indiparkstate.moc"
