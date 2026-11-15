/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "time/simclock.h"

#include <QSignalSpy>
#include <QTest>

class TestSimClock : public QObject
{
        Q_OBJECT

    private slots:
        /** @brief Switching to real time announces the system time, not the time the clock had before. */
        void testRealTimeAnnouncesSystemTime()
        {
            const KStarsDateTime past(QDateTime(QDate(2020, 1, 1), QTime(23, 0, 0), QTimeZone::utc()));
            SimClock clock(nullptr, past);
            QVERIFY(!clock.isRealTime());
            QVERIFY(std::abs(clock.utc().toSecsSinceEpoch() - past.toSecsSinceEpoch()) <= 1);

            // Listeners such as INDI devices read the clock when timeChanged() arrives
            qint64 announced = 0;
            connect(&clock, &SimClock::timeChanged, this, [&]()
            {
                announced = clock.utc().toSecsSinceEpoch();
            });

            clock.setRealTime(true);
            const qint64 now = QDateTime::currentDateTimeUtc().toSecsSinceEpoch();
            QVERIFY2(std::abs(announced - now) <= 2,
                     QString("timeChanged() announced %1, the system time is %2")
                     .arg(QDateTime::fromSecsSinceEpoch(announced, QTimeZone::utc()).toString(Qt::ISODate),
                          QDateTime::fromSecsSinceEpoch(now, QTimeZone::utc()).toString(Qt::ISODate)).toLocal8Bit());
        }
};

QTEST_GUILESS_MAIN(TestSimClock)

#include "test_simclock.moc"
