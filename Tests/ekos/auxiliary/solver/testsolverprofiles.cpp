/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

/*
 * Tests for the plate solving defaults:
 * - SolverUtils threading selection and blind field width limits.
 * - Migration of saved align profiles to the current built-in profiles.
 */

#include "ekos/auxiliary/solverutils.h"
#include "ekos/auxiliary/stellarsolverprofile.h"
#include "kspaths.h"
#include "Options.h"

#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QTest>

class TestSolverProfiles : public QObject
{
        Q_OBJECT

    private slots:
        void initTestCase();

        void testSelectMultiAlgorithm_data();
        void testSelectMultiAlgorithm();
        void testApplyExpectedFieldWidth_data();
        void testApplyExpectedFieldWidth();
        void testScaleSearchRange_data();
        void testScaleSearchRange();

        void testBuiltInProfiles();
        void testMigrateStockFile_data();
        void testMigrateStockFile();
        void testMigrateModifiedBuiltIn();
        void testMigrateCurrentProfilesUnchanged();
        void testLoadMigratesFileOnce();

    private:
        static QList<SSolver::Parameters> loadFixture(const QString &name);
        static int indexOf(const QList<SSolver::Parameters> &profiles, const QString &name);
};

void TestSolverProfiles::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
}

void TestSolverProfiles::testSelectMultiAlgorithm_data()
{
    QTest::addColumn<int>("requested");
    QTest::addColumn<bool>("useScale");
    QTest::addColumn<bool>("usePosition");
    QTest::addColumn<int>("expected");

    QTest::newRow("auto, scale and position") << int(SSolver::MULTI_AUTO) << true << true << int(SSolver::NOT_MULTI);
    QTest::newRow("auto, scale only") << int(SSolver::MULTI_AUTO) << true << false << int(SSolver::NOT_MULTI);
    QTest::newRow("auto, position only") << int(SSolver::MULTI_AUTO) << false << true << int(SSolver::NOT_MULTI);
    QTest::newRow("auto, blind") << int(SSolver::MULTI_AUTO) << false << false << int(SSolver::MULTI_SCALES);
    QTest::newRow("explicit depths kept") << int(SSolver::MULTI_DEPTHS) << false << true << int(SSolver::MULTI_DEPTHS);
    QTest::newRow("explicit single kept") << int(SSolver::NOT_MULTI) << false << false << int(SSolver::NOT_MULTI);
}

void TestSolverProfiles::testSelectMultiAlgorithm()
{
    QFETCH(int, requested);
    QFETCH(bool, useScale);
    QFETCH(bool, usePosition);
    QFETCH(int, expected);

    QCOMPARE(int(SolverUtils::selectMultiAlgorithm(static_cast<SSolver::MultiAlgo>(requested), useScale, usePosition)),
             expected);
}

void TestSolverProfiles::testApplyExpectedFieldWidth_data()
{
    QTest::addColumn<double>("width");
    QTest::addColumn<double>("minWidth");
    QTest::addColumn<double>("maxWidth");

    // the profile limits are 0.1 to 10 degrees
    QTest::newRow("unknown width keeps profile") << 0.0 << 0.1 << 10.0;
    QTest::newRow("telescope 1 deg") << 1.0 << 0.25 << 4.0;
    QTest::newRow("narrow field clamped") << 0.2 << 0.1 << 0.8;
    QTest::newRow("camera lens 20 deg") << 20.0 << 5.0 << 80.0;
    QTest::newRow("all sky clamped") << 90.0 << 22.5 << 180.0;
}

void TestSolverProfiles::testApplyExpectedFieldWidth()
{
    QFETCH(double, width);
    QFETCH(double, minWidth);
    QFETCH(double, maxWidth);

    SSolver::Parameters parameters = Ekos::getDefaultAlignOptionsProfiles().first();
    SolverUtils::applyExpectedFieldWidth(parameters, width);
    QCOMPARE(parameters.minwidth, minWidth);
    QCOMPARE(parameters.maxwidth, maxWidth);
}

void TestSolverProfiles::testScaleSearchRange_data()
{
    QTest::addColumn<double>("low");
    QTest::addColumn<double>("high");
    QTest::addColumn<double>("tolerance");
    QTest::addColumn<double>("searchLow");
    QTest::addColumn<double>("searchHigh");

    // The scale range is widened once, by the tolerance only
    QTest::newRow("default tolerance") << 2.0 << 2.0 << 0.2 << 1.6 << 2.4;
    QTest::newRow("tight stacking subframe") << 2.0 << 2.0 << 0.05 << 1.9 << 2.1;
    QTest::newRow("image overlay") << 2.0 << 2.0 << 0.25 << 1.5 << 2.5;
    QTest::newRow("range") << 1.0 << 2.0 << 0.2 << 0.8 << 2.4;
}

void TestSolverProfiles::testScaleSearchRange()
{
    QFETCH(double, low);
    QFETCH(double, high);
    QFETCH(double, tolerance);
    QFETCH(double, searchLow);
    QFETCH(double, searchHigh);

    double actualLow = 0, actualHigh = 0;
    SolverUtils::scaleSearchRange(low, high, tolerance, actualLow, actualHigh);
    QVERIFY(qFuzzyCompare(actualLow, searchLow));
    QVERIFY(qFuzzyCompare(actualHigh, searchHigh));
}

void TestSolverProfiles::testBuiltInProfiles()
{
    const auto profiles = Ekos::getDefaultAlignOptionsProfiles();
    QCOMPARE(profiles.size(), 1);
    const auto &profile = profiles.first();
    QCOMPARE(profile.listName, Ekos::ALIGN_DEFAULT_NAME);
    // Settings that solved nebulous and faint frames the former profiles failed on
    QCOMPARE(profile.fwhm, 1.0);
    QCOMPARE(profile.keepNum, 50);
    QCOMPARE(profile.initialKeep, SSolver::Parameters().initialKeep);
    QCOMPARE(profile.maxEllipse, 0.0);
    QCOMPARE(int(profile.multiAlgorithm), int(SSolver::MULTI_AUTO));
}

void TestSolverProfiles::testMigrateStockFile_data()
{
    QTest::addColumn<QString>("fixture");
    QTest::addColumn<QString>("selected");
    QTest::addColumn<QStringList>("expectedNames");
    QTest::addColumn<QString>("expectedSelected");
    QTest::addColumn<double>("searchRadius");

    // Saved by KStars 3.6 - 3.8 with the default search radius changed to 30 degrees
    QTest::newRow("default selected, custom search radius") << "savedalignprofiles_searchradius.ini" << "1-Default"
            << QStringList{"1-Default"} << "1-Default" << 30.0;
    QTest::newRow("retired profile selected") << "savedalignprofiles_searchradius.ini" << "3-LargeScaleSolving"
            << QStringList{"1-Default"} << "1-Default" << 30.0;
    // Saved by KStars 3.6 - 3.8 with a modified 1-Default (fwhm 7) and an additional user profile
    const QStringList customNames {"1-Default", "1-Default (Custom)", "PushTo"};
    QTest::newRow("user profile selected") << "savedalignprofiles_custom.ini" << "PushTo"
                                           << customNames << "PushTo" << 15.0;
    QTest::newRow("modified default selected") << "savedalignprofiles_custom.ini" << "1-Default"
            << customNames << "1-Default (Custom)" << 15.0;
    QTest::newRow("retired profile selected, user profiles kept") << "savedalignprofiles_custom.ini"
            << "4-SmallScaleSolving" << customNames << "1-Default" << 15.0;
}

void TestSolverProfiles::testMigrateStockFile()
{
    QFETCH(QString, fixture);
    QFETCH(QString, selected);
    QFETCH(QStringList, expectedNames);
    QFETCH(QString, expectedSelected);
    QFETCH(double, searchRadius);

    const auto saved = loadFixture(fixture);
    int selectedIndex = indexOf(saved, selected);
    QVERIFY(selectedIndex >= 0);

    QList<SSolver::Parameters> migrated;
    QVERIFY(Ekos::migrateAlignOptionsProfiles(saved, selectedIndex, migrated));

    QStringList names;
    for (const auto &profile : migrated)
        names << profile.listName;
    QCOMPARE(names, expectedNames);
    QCOMPARE(migrated[selectedIndex].listName, expectedSelected);

    // The default profile is the current built-in one, with the user's search radius
    auto expected = Ekos::getDefaultAlignOptionsProfiles().first();
    auto actual = migrated[indexOf(migrated, "1-Default")];
    QCOMPARE(actual.search_radius, searchRadius);
    expected.search_radius = searchRadius;
    QVERIFY(expected == actual);
    QCOMPARE(actual.maxwidth, 10.0);
    QCOMPARE(actual.keepNum, 50);
}

void TestSolverProfiles::testMigrateModifiedBuiltIn()
{
    auto saved = loadFixture("savedalignprofiles_custom.ini");
    const int small = indexOf(saved, "4-SmallScaleSolving");
    saved[small].fwhm = 3;
    int selectedIndex = small;

    QList<SSolver::Parameters> migrated;
    QVERIFY(Ekos::migrateAlignOptionsProfiles(saved, selectedIndex, migrated));

    // The modified profile is kept under a new name and stays selected
    const int custom = indexOf(migrated, "4-SmallScaleSolving (Custom)");
    QVERIFY(custom >= 0);
    QCOMPARE(selectedIndex, custom);
    QCOMPARE(migrated[custom].fwhm, 3.0);
    QCOMPARE(indexOf(migrated, "4-SmallScaleSolving"), -1);
    QVERIFY(indexOf(migrated, "1-Default") >= 0);
}

void TestSolverProfiles::testMigrateCurrentProfilesUnchanged()
{
    // A migrated file needs no further changes
    const auto saved = loadFixture("savedalignprofiles_custom.ini");
    int selectedIndex = indexOf(saved, "PushTo");
    QList<SSolver::Parameters> migrated;
    QVERIFY(Ekos::migrateAlignOptionsProfiles(saved, selectedIndex, migrated));

    int secondIndex = selectedIndex;
    QList<SSolver::Parameters> second;
    QVERIFY(!Ekos::migrateAlignOptionsProfiles(migrated, secondIndex, second));
    QCOMPARE(secondIndex, selectedIndex);

    // The built-in profiles alone need no changes either
    int index = 0;
    QVERIFY(!Ekos::migrateAlignOptionsProfiles(Ekos::getDefaultAlignOptionsProfiles(), index, second));
    QCOMPARE(index, 0);
}

void TestSolverProfiles::testLoadMigratesFileOnce()
{
    const QString dir = KSPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QVERIFY(QDir().mkpath(dir));
    const QString file = QDir(dir).filePath("SavedAlignProfiles.ini");
    QFile::remove(file);

    // Without a saved file, the built-in profiles are used and no file is written. A retired built-in
    // profile, here 4-SmallScaleSolving, may still be selected.
    Options::setSolveOptionsProfile(3);
    QCOMPARE(Ekos::loadAlignOptionsProfiles().size(), 1);
    QCOMPARE(Options::solveOptionsProfile(), 0u);
    QVERIFY(!QFile::exists(file));

    QVERIFY(QFile::copy(QFINDTESTDATA("savedalignprofiles_custom.ini"), file));
    QFile::setPermissions(file, QFile::ReadOwner | QFile::WriteOwner);
    // PushTo is the fifth profile in the saved file
    Options::setSolveOptionsProfile(4);

    auto profiles = Ekos::loadAlignOptionsProfiles();
    QCOMPARE(profiles.size(), 3);
    QCOMPARE(profiles[Options::solveOptionsProfile()].listName, QString("PushTo"));

    // The file was rewritten, loading it again changes nothing
    const auto saved = StellarSolver::loadSavedOptionsProfiles(file);
    QCOMPARE(saved.size(), 3);
    QFile f(file);
    QVERIFY(f.open(QIODevice::ReadOnly));
    const QByteArray content = f.readAll();
    f.close();

    profiles = Ekos::loadAlignOptionsProfiles();
    QCOMPARE(profiles.size(), 3);
    QCOMPARE(profiles[Options::solveOptionsProfile()].listName, QString("PushTo"));
    QVERIFY(f.open(QIODevice::ReadOnly));
    QCOMPARE(f.readAll(), content);
    f.close();

    QFile::remove(file);
}

QList<SSolver::Parameters> TestSolverProfiles::loadFixture(const QString &name)
{
    const QString path = QFINDTESTDATA(name);
    if (path.isEmpty())
        return QList<SSolver::Parameters>();
    return StellarSolver::loadSavedOptionsProfiles(path);
}

int TestSolverProfiles::indexOf(const QList<SSolver::Parameters> &profiles, const QString &name)
{
    for (int i = 0; i < profiles.size(); i++)
    {
        if (profiles[i].listName == name)
            return i;
    }
    return -1;
}

QTEST_GUILESS_MAIN(TestSolverProfiles)

#include "testsolverprofiles.moc"
