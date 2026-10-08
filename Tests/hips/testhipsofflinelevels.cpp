/*
    SPDX-FileCopyrightText: 2026 KStars Developers

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "../testhelpers.h"

#include "hips/hipsmanager.h"
#include "Options.h"

#include <QDir>
#include <QObject>
#include <QRegularExpression>
#include <QTemporaryDir>

/**
 * @brief Tests the mapping of requested HiPS orders to the orders available in an offline storage folder.
 *
 * The offline DSS storage (e.g. StellarMate's /opt/hips) usually holds only a few orders (Norder3, Norder5).
 * HIPSManager maps every order 0-20 to one that exists on disk.
 */
class TestHIPSOfflineLevels : public QObject
{
        Q_OBJECT

    private Q_SLOTS:
        void initTestCase();
        void cleanupTestCase();
        void cleanup();

        void testComputeOfflineLevels_data();
        void testComputeOfflineLevels();

        void testSetOfflineLevelsReplacesPreviousMap();
        void testLoadOfflineLevelsFromDirectory();
        void testLoadOfflineLevelsSilentWhenDisabled();
        void testLoadOfflineLevelsWarnsWhenEnabled();

    private:
        static QMap<int, int> expectedMap(const QList<int> &values);
};

void TestHIPSOfflineLevels::initTestCase()
{
    KTEST_BEGIN();
    Options::setHIPSUseOfflineSource(false);
}

void TestHIPSOfflineLevels::cleanupTestCase()
{
    KTEST_END();
}

void TestHIPSOfflineLevels::cleanup()
{
    Options::setHIPSUseOfflineSource(false);
}

// Build the expected map from a list of 21 values for orders 0..20
QMap<int, int> TestHIPSOfflineLevels::expectedMap(const QList<int> &values)
{
    QMap<int, int> map;
    for (int i = 0; i < values.size(); i++)
        map[i] = values[i];
    return map;
}

void TestHIPSOfflineLevels::testComputeOfflineLevels_data()
{
    QTest::addColumn<QStringList>("directories");
    QTest::addColumn<QList<int>>("expected");

    // No offline data at all: every order falls back to 1 (rendered as all-sky)
    QTest::newRow("empty") << QStringList() << QList<int>(21, 1);

    // StellarMate smosdata layout: Norder3 + Norder5
    QTest::newRow("norder3-norder5")
            << QStringList{"Norder3", "Norder5"}
            << QList<int> {3, 3, 3, 3, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5};

    // Unrelated folders and malformed names are ignored
    QTest::newRow("junk-ignored")
            << QStringList{"Dir0", "Norder", "Norderx", "Norder3abc", "foo", "Norder4", "Norder99"}
            << QList<int> {4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4};

    // A single low order is used for all higher orders
    QTest::newRow("norder0-only")
            << QStringList{"Norder0"}
            << QList<int>(21, 0);

    // Gaps are filled with the next higher available order
    QTest::newRow("gaps")
            << QStringList{"Norder9", "Norder3", "Norder6"}
            << QList<int> {3, 3, 3, 3, 6, 6, 6, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9};
}

void TestHIPSOfflineLevels::testComputeOfflineLevels()
{
    QFETCH(QStringList, directories);
    QFETCH(QList<int>, expected);

    QCOMPARE(HIPSManager::computeOfflineLevels(directories), expectedMap(expected));
}

// Regression: changing the offline path at runtime (HiPS Settings dialog) used to merge the new
// levels into the old map. After a missing path (all orders -> 1), fixing the path left every
// order except the exact available ones mapped to 1, so the overlay stayed at low all-sky resolution.
void TestHIPSOfflineLevels::testSetOfflineLevelsReplacesPreviousMap()
{
    auto *manager = HIPSManager::Instance();
    QVERIFY(manager != nullptr);

    manager->setOfflineLevels(QStringList());
    QCOMPARE(manager->getUsableOfflineLevel(4), 1);

    manager->setOfflineLevels(QStringList{"Norder3", "Norder5"});
    QCOMPARE(manager->getUsableOfflineLevel(0), 3);
    QCOMPARE(manager->getUsableOfflineLevel(3), 3);
    QCOMPARE(manager->getUsableOfflineLevel(4), 5);
    QCOMPARE(manager->getUsableOfflineLevel(5), 5);
    QCOMPARE(manager->getUsableOfflineLevel(12), 5);
    QCOMPARE(manager->getUsableOfflineLevel(20), 5);
}

void TestHIPSOfflineLevels::testLoadOfflineLevelsFromDirectory()
{
    QTemporaryDir storage(KTest::tempDirPattern("hips"));
    QVERIFY(storage.isValid());
    QVERIFY(QDir(storage.path()).mkpath("Norder3/Dir0"));
    QVERIFY(QDir(storage.path()).mkpath("Norder5/Dir0"));

    Options::setHIPSUseOfflineSource(true);

#if QT_VERSION >= QT_VERSION_CHECK(6, 3, 0)
    QTest::failOnWarning(QRegularExpression("HiPS offline source"));
#endif

    auto *manager = HIPSManager::Instance();
    manager->loadOfflineLevels(storage.path());

    QCOMPARE(manager->getUsableOfflineLevel(2), 3);
    QCOMPARE(manager->getUsableOfflineLevel(4), 5);
    QCOMPARE(manager->getUsableOfflineLevel(9), 5);
}

void TestHIPSOfflineLevels::testLoadOfflineLevelsSilentWhenDisabled()
{
    QTemporaryDir parent(KTest::tempDirPattern("hips"));
    QVERIFY(parent.isValid());
    const QString missing = QDir(parent.path()).filePath("does-not-exist");

    auto *manager = HIPSManager::Instance();

    // No warning while the offline source is disabled
    Options::setHIPSUseOfflineSource(false);
#if QT_VERSION >= QT_VERSION_CHECK(6, 3, 0)
    QTest::failOnWarning(QRegularExpression("HiPS offline source"));
#endif
    manager->loadOfflineLevels(missing);
    QCOMPARE(manager->getUsableOfflineLevel(5), 1);
}

void TestHIPSOfflineLevels::testLoadOfflineLevelsWarnsWhenEnabled()
{
    auto *manager = HIPSManager::Instance();
    Options::setHIPSUseOfflineSource(true);

    // Missing folder
    QTemporaryDir parent(KTest::tempDirPattern("hips"));
    QVERIFY(parent.isValid());
    const QString missing = QDir(parent.path()).filePath("does-not-exist");
    QTest::ignoreMessage(QtWarningMsg,
                         QRegularExpression("HiPS offline source is enabled but the offline path does not exist"));
    manager->loadOfflineLevels(missing);
    QCOMPARE(manager->getUsableOfflineLevel(5), 1);

    // Existing folder without any Norder* sub-folder
    QVERIFY(QDir(parent.path()).mkpath("Dir0"));
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("HiPS offline source is enabled but no Norder\\* directories"));
    manager->loadOfflineLevels(parent.path());
    QCOMPARE(manager->getUsableOfflineLevel(5), 1);
}

QTEST_GUILESS_MAIN(TestHIPSOfflineLevels)

#include "testhipsofflinelevels.moc"
