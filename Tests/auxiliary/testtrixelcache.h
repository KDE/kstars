/*
    SPDX-FileCopyrightText: 2021 Valentin Boettcher <hiro at protagon.space; @hiro98:tchncs.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QtTest/QtTest>
#include <QtConcurrent/QtConcurrentRun>
#include <qtestcase.h>
#include "trixelcache.h"

using TestCache = TrixelCache<std::vector<int>>;
class TestTrixelCache : public QObject
{
    Q_OBJECT
  private:
    TestCache m_cache{ 10, 5 };
  private slots:
    void init()
    {
        // a fresh cache for each test
        m_cache = { 10, 5 };
    };

    void noop()
    {
        m_cache = { 10, 10 };
        QVERIFY2(m_cache.noop(), "Is the constructed trixel cache a noop?");

        m_cache.set_size(5);
        QVERIFY2(!m_cache.noop(), "Is the cache no noop after resize?");

        m_cache.set_size(10);
        QVERIFY2(m_cache.noop(), "Is the cache no noop again after resize?");
    }

    void resize()
    {
        QVERIFY_EXCEPTION_THROWN(m_cache.set_size(20), std::range_error);
        m_cache.set_size(1);
        QCOMPARE(m_cache.size(), 1);

        m_cache.set_size(2);
        QCOMPARE(m_cache.size(), 2);
    };

    void setting()
    {
        QCOMPARE(m_cache.current_usage(), 0);

        m_cache[0] = { 1, 2, 3 };
        QCOMPARE(m_cache.current_usage(), 1);
        QCOMPARE(m_cache.primed_indices(), std::list<size_t>{ 0 });
        QVERIFY(m_cache[0].is_set());

        m_cache[2] = { 1, 2, 3 };
        QCOMPARE(m_cache.current_usage(), 2);
        QCOMPARE(m_cache.primed_indices(), (std::list<size_t>{ 2, 0 }));
        QVERIFY(m_cache[2].is_set());
    };

    void pruning()
    {
        m_cache    = { 10, 1 };
        m_cache[1] = { 4, 5, 6 };
        m_cache[0] = { 1, 2, 3 };

        QVERIFY2(m_cache[1].is_set(), "Index 1 should be set.");
        QVERIFY2(m_cache[0].is_set(), "Index 0 should be set.");

        m_cache.prune();

        QVERIFY2(!m_cache[1].is_set(), "Index 1 should be cleared.");
        QCOMPARE(m_cache[1].data(), std::vector<int>{});
        QVERIFY2(m_cache[0].is_set(), "Index 0 should be set.");
        QCOMPARE(m_cache[0].data(), (std::vector<int>{ 1, 2, 3 }));

        m_cache[0] = { 1, 2, 3 };
        m_cache[1] = { 4, 5, 6 };
        m_cache.prune(2);

        QVERIFY2(m_cache[1].is_set(), "Index 1 should be set.");
        QVERIFY2(m_cache[0].is_set(), "Index 0 should be set.");
    };

    void clear()
    {
        m_cache    = { 2, 1 };
        m_cache[1] = { 4, 5, 6 };
        m_cache[0] = { 1, 2, 3 };

        QVERIFY2(m_cache[1].is_set(), "Index 1 should be set.");
        QVERIFY2(m_cache[0].is_set(), "Index 0 should be set.");

        m_cache.clear();

        QCOMPARE(m_cache.current_usage(), 0);
        QCOMPARE(m_cache.primed_indices(), {});

        QVERIFY2(!m_cache[1].is_set(), "Index 1 should be cleared.");
        QCOMPARE(m_cache[1].data(), std::vector<int>{});
        QVERIFY2(!m_cache[0].is_set(), "Index 0 should be cleared.");
        QCOMPARE(m_cache[0].data(), std::vector<int>{});
    };
};

QTEST_GUILESS_MAIN(TestTrixelCache);
