/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "ekos/capture/exposurecalculator/fileutilitycameradata.h"
#include "ekos/capture/exposurecalculator/imagingcameradata.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTest>

// Tests for the camera data files of the Exposure Calculator. The data directories are redirected to a
// temporary directory in main(), before anything reads them.

namespace
{
QTemporaryDir *dataRoot = nullptr;

QString userDataDir()
{
    return dataRoot->filePath("user");
}

QString systemDataDir()
{
    return dataRoot->filePath("system");
}

void writeFile(const QString &path, const QByteArray &content)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(content);
}
}

class TestCameraData : public QObject
{
        Q_OBJECT

    private Q_SLOTS:
        void cleanup();

        void testFileNameConversion();
        void testParseRepositoryListing_data();
        void testParseRepositoryListing();
        void testGitBlobSha();
        void testLocalDirectoryIsListed();
        void testUserFileOverridesBundledFile();
        void testInstalledCameraDataFile();
        void testBundledFilesLoad_data();
        void testBundledFilesLoad();
};

void TestCameraData::cleanup()
{
    QDir(userDataDir()).removeRecursively();
    QDir(systemDataDir()).removeRecursively();
}

void TestCameraData::testFileNameConversion()
{
    using OptimalExposure::FileUtilityCameraData;
    QCOMPARE(FileUtilityCameraData::cameraIdToCameraDataFileName("ZWO CCD ASI071MC Pro"), "ZWO_CCD_ASI071MC_Pro.xml");
    QCOMPARE(FileUtilityCameraData::cameraDataFileNameToCameraId("/some/dir/ZWO_CCD_ASI071MC_Pro.xml"),
             "ZWO CCD ASI071MC Pro");
}

void TestCameraData::testParseRepositoryListing_data()
{
    QTest::addColumn<QByteArray>("json");

    // GitHub returns compact or indented JSON depending on the client; both must work.
    QTest::newRow("compact") << QByteArray(
                                 "{\"sha\":\"x\",\"tree\":[{\"path\":\"A_Cam.xml\",\"type\":\"blob\",\"sha\":\"111\"},"
                                 "{\"path\":\"README.md\",\"type\":\"blob\",\"sha\":\"222\"},"
                                 "{\"path\":\"sub\",\"type\":\"tree\",\"sha\":\"333\"},"
                                 "{\"path\":\"sub/B_Cam.xml\",\"type\":\"blob\",\"sha\":\"444\"},"
                                 "{\"path\":\"C_Cam.xml\",\"type\":\"blob\",\"sha\":\"555\"}],\"truncated\":false}");
    QTest::newRow("indented") << QByteArray(R"({
  "sha": "x",
  "tree": [
    {
      "path": "A_Cam.xml",
      "type": "blob",
      "sha": "111"
    },
    {
      "path": "README.md",
      "type": "blob",
      "sha": "222"
    },
    {
      "path": "sub",
      "type": "tree",
      "sha": "333"
    },
    {
      "path": "sub/B_Cam.xml",
      "type": "blob",
      "sha": "444"
    },
    {
      "path": "C_Cam.xml",
      "type": "blob",
      "sha": "555"
    }
  ],
  "truncated": false
}
)");
}

void TestCameraData::testParseRepositoryListing()
{
    QFETCH(QByteArray, json);

    const auto files = OptimalExposure::FileUtilityCameraData::parseRepositoryListing(json);
    QCOMPARE(files.size(), 2);
    QCOMPARE(files[0].fileName, "A_Cam.xml");
    QCOMPARE(files[0].sha, "111");
    QCOMPARE(files[1].fileName, "C_Cam.xml");
    QCOMPARE(files[1].sha, "555");

    QVERIFY(OptimalExposure::FileUtilityCameraData::parseRepositoryListing("not json").isEmpty());
}

void TestCameraData::testGitBlobSha()
{
    // Values from "git hash-object".
    QCOMPARE(OptimalExposure::FileUtilityCameraData::gitBlobSha(""), "e69de29bb2d1d6434b8b29ae775ad8c2e48c5391");
    QCOMPARE(OptimalExposure::FileUtilityCameraData::gitBlobSha("hello\n"), "ce013625030ba8dba906f756967f9e9ca394464a");
}

void TestCameraData::testLocalDirectoryIsListed()
{
    // Downloads are saved in the user's KStars data directory, which the calculator must list.
    const QString localDir = OptimalExposure::FileUtilityCameraData::localCameraDataDirectory();
    QCOMPARE(QDir::cleanPath(localDir), QDir::cleanPath(userDataDir() + "/kstars/cameradata"));

    writeFile(localDir + "Downloaded_Cam.xml", "<ExposureCalculatorCameraData/>");
    const QStringList files = OptimalExposure::FileUtilityCameraData::getAvailableCameraFilesList();
    QCOMPARE(files.size(), 1);
    QCOMPARE(QFileInfo(files.first()).fileName(), "Downloaded_Cam.xml");
}

void TestCameraData::testUserFileOverridesBundledFile()
{
    writeFile(systemDataDir() + "/kstars/cameradata/Some_Cam.xml", "bundled");
    writeFile(systemDataDir() + "/kstars/cameradata/Other_Cam.xml", "bundled");
    writeFile(userDataDir() + "/kstars/cameradata/Some_Cam.xml", "downloaded");

    const QStringList files = OptimalExposure::FileUtilityCameraData::getAvailableCameraFilesList();

    // Each camera is listed once, and the user's copy replaces the bundled one.
    QCOMPARE(files.size(), 2);
    for (const QString &file : files)
    {
        QFile f(file);
        QVERIFY(f.open(QIODevice::ReadOnly));
        if (QFileInfo(file).fileName() == "Some_Cam.xml")
            QCOMPARE(f.readAll(), QByteArray("downloaded"));
        else
            QCOMPARE(f.readAll(), QByteArray("bundled"));
    }
}

void TestCameraData::testInstalledCameraDataFile()
{
    using OptimalExposure::FileUtilityCameraData;
    writeFile(systemDataDir() + "/kstars/cameradata/Some_Cam.xml", "bundled");
    QCOMPARE(FileUtilityCameraData::installedCameraDataFile("Some Cam"),
             systemDataDir() + "/kstars/cameradata/Some_Cam.xml");
    QVERIFY(FileUtilityCameraData::installedCameraDataFile("Missing Cam").isEmpty());

    writeFile(FileUtilityCameraData::localCameraDataDirectory() + "Some_Cam.xml", "downloaded");
    QCOMPARE(QDir::cleanPath(FileUtilityCameraData::installedCameraDataFile("Some Cam")),
             QDir::cleanPath(FileUtilityCameraData::localCameraDataDirectory() + "Some_Cam.xml"));
}

void TestCameraData::testBundledFilesLoad_data()
{
    QTest::addColumn<QString>("path");
    const QDir dir(KSTARS_CAMERADATA_DIR);
    const QStringList files = dir.entryList({"*.xml"}, QDir::Files, QDir::Name);
    QVERIFY(!files.isEmpty());
    for (const QString &file : files)
        QTest::newRow(file.toLatin1().constData()) << dir.filePath(file);
}

void TestCameraData::testBundledFilesLoad()
{
    QFETCH(QString, path);

    OptimalExposure::ImagingCameraData data;
    OptimalExposure::FileUtilityCameraData::readCameraDataFile(path, &data);

    const auto modes = data.getCameraGainReadModeVector();
    QVERIFY(!modes.isEmpty());
    for (int index = 0; index < modes.size(); index++)
    {
        // The calculator uses the mode number as the position in this list.
        auto mode = modes[index];
        QCOMPARE(mode.getCameraGainReadModeNumber(), index);
        auto table = mode.getCameraGainReadNoiseVector();
        QVERIFY(!table.isEmpty());
        for (int row = 1; row < table.size(); row++)
            QVERIFY2(table[row - 1].getGain() < table[row].getGain(), "gains must increase");
        for (auto value : table)
            QVERIFY(value.getReadNoise() > 0);
    }
    QVERIFY(data.getCameraPixelSize() > 0);
    QVERIFY(data.getCameraQuantumEfficiency() > 0 && data.getCameraQuantumEfficiency() <= 1);
}

int main(int argc, char *argv[])
{
    QTemporaryDir root;
    dataRoot = &root;
    qputenv("XDG_DATA_HOME", QFile::encodeName(userDataDir()));
    qputenv("XDG_DATA_DIRS", QFile::encodeName(systemDataDir()));

    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("kstars");

    TestCameraData test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_cameradata.moc"
