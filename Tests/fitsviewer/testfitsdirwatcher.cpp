/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "fitsviewer/fitsdirwatcher.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

class TestFITSDirWatcher : public QObject
{
        Q_OBJECT

    private Q_SLOTS:
        void testKeepsEveryFileWithoutCutoff();
        void testSkipsFilesBeforeCutoff();
};

namespace
{
// Contents do not matter: the watcher only lists files and checks they stopped changing
bool writeFile(const QString &path, const QDateTime &modified = QDateTime())
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.write("SIMPLE  =                    T");
    // Flush first: writing the buffer on close() would set the modification time to now
    file.flush();
    if (modified.isValid() && !file.setFileTime(modified, QFileDevice::FileModificationTime))
        return false;
    file.close();
    return true;
}

QStringList fileNames(const QVector<LiveStackFile> &files)
{
    QStringList names;
    for (const auto &file : files)
        names << QFileInfo(file.file).fileName();
    return names;
}
}

void TestFITSDirWatcher::testKeepsEveryFileWithoutCutoff()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QVERIFY(writeFile(dir.filePath("old.fits"), QDateTime::currentDateTime().addDays(-42)));
    QVERIFY(writeFile(dir.filePath("new.fits")));

    FITSDirWatcher watcher;
    QVERIFY(watcher.watchDirs({dir.path()}));
    QCOMPARE(fileNames(watcher.getCurrentFiles()), QStringList({"old.fits", "new.fits"}));
}

void TestFITSDirWatcher::testSkipsFilesBeforeCutoff()
{
    // A capture folder that still holds a frame from an earlier session
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QDateTime cutoff = QDateTime::currentDateTime().addSecs(-3600);
    QVERIFY(writeFile(dir.filePath("Light_HA_IC_1848_001_october.fits"), cutoff.addDays(-42)));
    QVERIFY(writeFile(dir.filePath("Light_HA_IC_1848_001.fits")));

    FITSDirWatcher watcher;
    watcher.setIgnoreFilesBefore(cutoff);
    QVERIFY(watcher.watchDirs({dir.path()}));
    QCOMPARE(fileNames(watcher.getCurrentFiles()), QStringList({"Light_HA_IC_1848_001.fits"}));

    // A new frame makes the watcher list the folder again: the skipped file must not
    // come back as a new one
    QStringList detected;
    connect(&watcher, &FITSDirWatcher::newFilesDetected, this,
            [&detected](QDateTime, const QVector<LiveStackFile> &files)
    {
        detected << fileNames(files);
    });
    QVERIFY(writeFile(dir.filePath("Light_HA_IC_1848_002.fits"), QDateTime::currentDateTime().addSecs(-5)));
    QTRY_COMPARE_WITH_TIMEOUT(detected, QStringList({"Light_HA_IC_1848_002.fits"}), 10000);

    // Give a wrongly pending skipped file time to be reported too
    QTest::qWait(3000);
    QCOMPARE(detected, QStringList({"Light_HA_IC_1848_002.fits"}));
    QCOMPARE(watcher.getCurrentFiles().size(), 2);
}

QTEST_GUILESS_MAIN(TestFITSDirWatcher)

#include "testfitsdirwatcher.moc"
