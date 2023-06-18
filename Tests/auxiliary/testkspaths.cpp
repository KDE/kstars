#include <QTemporaryFile>
#include "testkspaths.h"

#include "../testhelpers.h"

TestKSPaths::TestKSPaths(QObject * parent): QObject(parent)
{

}

void TestKSPaths::initTestCase()
{
}

void TestKSPaths::cleanupTestCase()
{
}

void TestKSPaths::init()
{
    KTEST_BEGIN();
}

void TestKSPaths::cleanup()
{
    KTEST_END();
}

void TestKSPaths::testStandardPaths_data()
{
#if QT_VERSION < 0x050900
    QSKIP("Skipping fixture-based test on old QT version.");
#else
    QTest::addColumn<QStandardPaths::StandardLocation>("TYPE");

    QTest::addColumn<bool>("ISALTERED");
    //bool const isAltered = true;
    bool const isStandard = false;

    QTest::addColumn<bool>("ISWRITABLE");
    bool const isWritable = true;
    //bool const isNotWritable = false;

    // This fixture verifies that generic locations get the application name appended (which is unexpected)
    QTest::addRow("DesktopLocation")        << QStandardPaths::DesktopLocation          << isStandard   << isWritable;
    QTest::addRow("DocumentsLocation")      << QStandardPaths::DocumentsLocation        << isStandard   << isWritable;
    QTest::addRow("FontsLocation")          << QStandardPaths::FontsLocation            << isStandard   << isWritable;
    QTest::addRow("ApplicationsLocation")   << QStandardPaths::ApplicationsLocation     << isStandard   << isWritable;
    QTest::addRow("MusicLocation")          << QStandardPaths::MusicLocation            << isStandard   << isWritable;
    QTest::addRow("MoviesLocation")         << QStandardPaths::MoviesLocation           << isStandard   << isWritable;
    QTest::addRow("PicturesLocation")       << QStandardPaths::PicturesLocation         << isStandard   << isWritable;
    QTest::addRow("TempLocation")           << QStandardPaths::TempLocation             << isStandard   << isWritable;
    QTest::addRow("HomeLocation")           << QStandardPaths::HomeLocation             << isStandard   << isWritable;
    QTest::addRow("DataLocation")           << QStandardPaths::DataLocation             << isStandard   << isWritable;
    QTest::addRow("CacheLocation")          << QStandardPaths::CacheLocation            << isStandard   << isWritable;
    QTest::addRow("GenericCacheLocation")   << QStandardPaths::GenericCacheLocation     << isStandard   << isWritable;
    QTest::addRow("GenericDataLocation")    << QStandardPaths::GenericDataLocation      << isStandard   << isWritable;
    // JM 2021.07.08 Both FreeBSD and OpenSUSE return RunTimeLocation with trailing slash.
    // /tmp/runtime-kdeci//test_file.XXXXXX
    // Do not know the cause, so disabling this test for now. Maybe test environment specific issue?
#if 0
    QTest::addRow("RuntimeLocation")        << QStandardPaths::RuntimeLocation          << isStandard   << isWritable;
#endif
    QTest::addRow("ConfigLocation")         << QStandardPaths::ConfigLocation           << isStandard   << isWritable;
    QTest::addRow("DownloadLocation")       << QStandardPaths::DownloadLocation         << isStandard   << isWritable;
    QTest::addRow("GenericConfigLocation")  << QStandardPaths::GenericConfigLocation    << isStandard   << isWritable;
    QTest::addRow("AppDataLocation")        << QStandardPaths::AppDataLocation          << isStandard   << isWritable;
    QTest::addRow("AppLocalDataLocation")   << QStandardPaths::AppLocalDataLocation     << isStandard   << isWritable;
    QTest::addRow("AppConfigLocation")      << QStandardPaths::AppConfigLocation        << isStandard   << isWritable;
#endif
}

void TestKSPaths::testStandardPaths()
{
#if QT_VERSION < 0x050900
    QSKIP("Skipping fixture-based test on old QT version.");
#else
    QFETCH(QStandardPaths::StandardLocation, TYPE);
    QFETCH(bool, ISALTERED);
    QFETCH(bool, ISWRITABLE);

    QString const tested_writable_dir = KSPaths::writableLocation(TYPE);

    if (ISALTERED)
        QEXPECT_FAIL("", qPrintable(QString("Location associated with QStandardPaths type %1 is altered by KSPaths.").arg(TYPE)),
                     Continue);

    QCOMPARE(tested_writable_dir, QStandardPaths::writableLocation(TYPE));

    if (ISALTERED)
        QCOMPARE(tested_writable_dir, QDir(QStandardPaths::writableLocation(TYPE)).path() + QDir::separator());

    if (ISWRITABLE)
        QVERIFY2(QDir(tested_writable_dir).mkpath("."),
                 qPrintable(QString("Directory '%1' must be writable.").arg(KSPaths::writableLocation(TYPE))));

    QTemporaryFile test_file(tested_writable_dir + "/test_file.XXXXXX");

    if (ISALTERED)
        QEXPECT_FAIL("", "Path built with KSPaths::writableLocation contains no duplicate separator", Continue);
    QVERIFY2(!test_file.fileTemplate().contains(QString("%1%1").arg(QDir::separator())),
             qPrintable(QString("%1 contains duplicate separator").arg(test_file.fileTemplate())));

    if (ISWRITABLE)
    {
        QDir const path_to_test_file = QFileInfo(test_file.fileName()).dir();
        QVERIFY2(path_to_test_file.mkpath("."), qPrintable(QString("Dir '%1' must be writable.").arg(path_to_test_file.path())));
        QVERIFY2(test_file.open(), qPrintable(QString("File '%1' must be writable.").arg(test_file.fileTemplate())));
    }

    if (test_file.isOpen())
    {
        QVERIFY(test_file.write(qPrintable(QApplication::applicationName())));
        test_file.close();

        QString recovered_test_file_name = QDir::cleanPath(KSPaths::locate(TYPE, QFileInfo(test_file.fileName()).fileName()));
        QCOMPARE(qPrintable(test_file.fileName()), qPrintable(recovered_test_file_name));

        QVERIFY(test_file.remove());
    }
#endif
}

void TestKSPaths::testKStarsInstallation_data()
{
#if QT_VERSION < 0x050900
    QSKIP("Skipping fixture-based test on old QT version.");
#else
    QTest::addColumn<QStandardPaths::StandardLocation>("TYPE");
    QTest::addColumn<QString>("FILE");

    typedef QPair <QStandardPaths::StandardLocation, QString> KPair;
    QList <KPair> fixtures;

    fixtures << KPair(QStandardPaths::AppLocalDataLocation, "TZrules.dat");
    fixtures << KPair(QStandardPaths::AppLocalDataLocation, "themes/whitebalance.colors");
    fixtures << KPair(QStandardPaths::AppLocalDataLocation, "textures/moon00.png");
    //fixtures << KPair(QStandardPaths::GenericDataLocation, "skycomponent.sqlite");
    fixtures << KPair(QStandardPaths::GenericDataLocation, "sounds/KDE-KStars-Alert.ogg");
    fixtures << KPair(QStandardPaths::AppLocalDataLocation, "kstars.png");

    for (auto pair : fixtures)
        QTest::addRow("%s", qPrintable(pair.second)) << pair.first << pair.second;
#endif
}

void TestKSPaths::testKStarsInstallation()
{
#if QT_VERSION < 0x050900
    QSKIP("Skipping fixture-based test on old QT version.");
#else
    // We can only verify the installation path of 'kstars', there is no install for the test
    // We don't execute kstars, so no user configuration is available

    QFETCH(QStandardPaths::StandardLocation, TYPE);
    QFETCH(QString, FILE);

    QVERIFY(!KSPaths::locate(TYPE, FILE).isEmpty());
    QVERIFY(!KSPaths::locateAll(TYPE, FILE).isEmpty());
#endif
}

QTEST_GUILESS_MAIN(TestKSPaths)
