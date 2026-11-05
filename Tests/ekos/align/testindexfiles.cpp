/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

/*
 * Tests for the astrometry.net index file helpers:
 * - required sizes for a field of view, with the rules of the Index Files page,
 * - complete and partial series on disk,
 * - the names and sizes of the series the diagnosis installs.
 */

#include "ekos/align/indexfiles.h"

#include <QFile>
#include <QMap>
#include <QTemporaryDir>
#include <QTest>

using namespace Ekos;

class TestIndexFiles : public QObject
{
        Q_OBJECT

    private slots:
        void testSizeRoleMatchesIndexPage();
        void testRequiredIndexSizes_data();
        void testRequiredIndexSizes();
        void testExpectedFileCount_data();
        void testExpectedFileCount();
        void testInstalledIndexSeries();
        void testInstallName_data();
        void testInstallName();
        void testEstimatedDownloadBytes();

    private:
        static void createFiles(const QString &directory, const QString &series, int count);
};

void TestIndexFiles::testSizeRoleMatchesIndexPage()
{
    // The loop OpsAstrometryIndexFiles::slotUpdate() used before IndexFiles::sizeRole() replaced it
    QMap<float, QString> astrometryIndex;
    const float skymarks[] = {2.8f, 4.0f, 5.6f, 8, 11, 16, 22, 30, 42, 60, 85, 120, 170, 240, 340, 480, 680, 1000, 1400, 2000};
    for (int i = 0; i < 20; i++)
        astrometryIndex[skymarks[i]] = QString("%1").arg(i, 2, 10, QLatin1Char('0'));

    for (double fov_check = 0.5; fov_check < 3000; fov_check *= 1.05)
    {
        float last_skymarksize = 2;
        for (auto &skymarksize : astrometryIndex.keys())
        {
            IndexFiles::SizeRole expected = IndexFiles::Optional;
            if ((skymarksize >= 0.40 * fov_check && skymarksize <= 0.9 * fov_check) ||
                    (fov_check > last_skymarksize && fov_check < skymarksize))
                expected = IndexFiles::Required;
            else if (skymarksize >= 0.10 * fov_check && skymarksize <= fov_check)
                expected = IndexFiles::Recommended;
            last_skymarksize = skymarksize;

            const int size = astrometryIndex.value(skymarksize).toInt();
            QVERIFY2(IndexFiles::sizeRole(size, fov_check) == expected,
                     qPrintable(QString("size %1 fov %2").arg(size).arg(fov_check)));
        }
    }

    QCOMPARE(IndexFiles::sizeRole(-1, 60), IndexFiles::Optional);
    QCOMPARE(IndexFiles::sizeRole(20, 60), IndexFiles::Optional);
    QCOMPARE(IndexFiles::skymarkSize(7), 30.0f);
    QCOMPARE(IndexFiles::skymarkSize(20), -1.0f);
}

void TestIndexFiles::testRequiredIndexSizes_data()
{
    QTest::addColumn<double>("fov");
    QTest::addColumn<QList<int>>("sizes");

    // 0.4 to 0.9 of the field, plus the size just above the field
    QTest::newRow("24 arcmin") << 24.1 << QList<int>({4, 5, 7});
    QTest::newRow("60 arcmin") << 60.0 << QList<int>({7, 8});
    QTest::newRow("6 degrees") << 360.0 << QList<int>({12, 13, 15});
    QTest::newRow("tiny") << 3.0 << QList<int>({1});
}

void TestIndexFiles::testRequiredIndexSizes()
{
    QFETCH(double, fov);
    QFETCH(QList<int>, sizes);
    QCOMPARE(IndexFiles::requiredIndexSizes(fov), sizes);
}

void TestIndexFiles::testExpectedFileCount_data()
{
    QTest::addColumn<QString>("name");
    QTest::addColumn<int>("count");

    QTest::newRow("4207 file") << "index-4207-03.fits" << 12;
    QTest::newRow("4207 pattern") << "index-4207-*.fits" << 12;
    QTest::newRow("4205") << "index-4205-00.fits" << 12;
    QTest::newRow("4200") << "index-4200-11.fits" << 48;
    QTest::newRow("5206") << "index-5206-*.fits" << 48;
    QTest::newRow("4208") << "index-4208.fits" << 1;
    QTest::newRow("4119") << "index-4119.fits" << 1;
}

void TestIndexFiles::testExpectedFileCount()
{
    QFETCH(QString, name);
    QFETCH(int, count);
    QCOMPARE(IndexFiles::expectedFileCount(name), count);
}

void TestIndexFiles::createFiles(const QString &directory, const QString &series, int count)
{
    for (int i = 0; i < count; i++)
    {
        const QString name = count == 1 ? QString("index-%1.fits").arg(series)
                             : QString("index-%1-%2.fits").arg(series).arg(i, 2, 10, QLatin1Char('0'));
        QFile file(directory + "/" + name);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();
    }
}

void TestIndexFiles::testInstalledIndexSeries()
{
    QTemporaryDir first, second;
    QVERIFY(first.isValid());
    QVERIFY(second.isValid());

    // Complete: 4208 (one file), 5206 (48 files) and 4119 (one file). Partial: 4207 (5 of 12).
    createFiles(first.path(), "4208", 1);
    createFiles(first.path(), "4207", 5);
    createFiles(second.path(), "5206", 48);
    createFiles(second.path(), "4119", 1);
    // Not an index file
    QFile other(first.path() + "/notes.fits");
    QVERIFY(other.open(QIODevice::WriteOnly));
    other.close();

    const QStringList directories({first.path(), second.path(), "/does/not/exist"});
    const QList<int> series = IndexFiles::installedIndexSeries(directories);
    QCOMPARE(series, QList<int>({4119, 4208, 5206}));
    QCOMPARE(IndexFiles::installedIndexSizes(series), QList<int>({6, 8, 19}));

    // The same series in two folders counts once; a complete copy beats a partial one
    createFiles(second.path(), "4207", 12);
    QCOMPARE(IndexFiles::installedIndexSeries(directories), QList<int>({4119, 4207, 4208, 5206}));

    QVERIFY(IndexFiles::installedIndexSeries(QStringList()).isEmpty());
    QVERIFY(IndexFiles::installedIndexSizes({1234, 4321}).isEmpty());
}

void TestIndexFiles::testInstallName_data()
{
    QTest::addColumn<int>("size");
    QTest::addColumn<QString>("name");
    QTest::addColumn<int>("series");

    // Q2: the LITE 52nn series up to size 06, 42nn above
    QTest::newRow("00") << 0 << "index-5200-*.fits" << 5200;
    QTest::newRow("06") << 6 << "index-5206-*.fits" << 5206;
    QTest::newRow("07") << 7 << "index-4207-*.fits" << 4207;
    QTest::newRow("08") << 8 << "index-4208.fits" << 4208;
    QTest::newRow("19") << 19 << "index-4219.fits" << 4219;
    QTest::newRow("negative") << -1 << "" << -1;
    QTest::newRow("too large") << 20 << "" << -1;
}

void TestIndexFiles::testInstallName()
{
    QFETCH(int, size);
    QFETCH(QString, name);
    QFETCH(int, series);
    QCOMPARE(IndexFiles::installName(size), name);
    QCOMPARE(IndexFiles::installSeries(size), series);
}

void TestIndexFiles::testEstimatedDownloadBytes()
{
    // The estimate OpsAstrometryIndexFiles makes for 4207: about 151 MB
    QVERIFY(std::abs(IndexFiles::estimatedDownloadBytes(7) - 151.5e6) < 1e6);
    QVERIFY(IndexFiles::estimatedDownloadBytes(0) > IndexFiles::estimatedDownloadBytes(19));
    QCOMPARE(IndexFiles::estimatedDownloadBytes(-1), qint64(0));
}

QTEST_GUILESS_MAIN(TestIndexFiles)

#include "testindexfiles.moc"
