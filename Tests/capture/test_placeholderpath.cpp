/*
    SPDX-FileCopyrightText: 2021 Kwon-Young Choi <kwon-young.choi@hotmail.fr>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

/* Project Includes */
#include "test_placeholderpath.h"
#include "ekos/capture/sequencejob.h"
#include "ekos/scheduler/schedulerjob.h"
#include "ekos/capture/placeholderpath.h"

#include <QFile>
#include <QRegularExpression>
#include <QRegularExpressionMatch>

TestPlaceholderPath::TestPlaceholderPath() : QObject()
{
}

TestPlaceholderPath::~TestPlaceholderPath()
{
}

// helper functions
void parseCSV(const QString filename, const QList<const char*> columns)
{
    QFile testDataFile(filename);

    if (!testDataFile.open(QIODevice::ReadOnly))
    {
        qDebug() << testDataFile.errorString();
        QFAIL("error");
    }

    // checking csv header
    int columnCpt = 0;
    QByteArray line = testDataFile.readLine().replace("\n", "");

    for (auto el : line.split(','))
    {
        if (columns.size() <= columnCpt)
            QFAIL("too many csv columns");
        else if (el != columns[columnCpt])
        {
            QFAIL(QString("csv columns incorrect %1 %2").arg(el, columns[columnCpt]).toStdString().c_str());
        }
        columnCpt++;
    }

    if (columns.size() != columnCpt)
        QFAIL("not enough csv columns");

    int cpt = 0;

    while (!testDataFile.atEnd())
    {
        QByteArray line = testDataFile.readLine().replace("\n", "");
        QTestData &row = QTest::addRow("%d", cpt);
        for (auto el : line.split(','))
        {
            row << QString::fromStdString(el.toStdString());
        }
        cpt++;
    }
}

XMLEle* buildXML(
    QString Exposure,
    QString Filter,
    QString Type,
    QString Prefix,
    QString RawPrefix,
    QString FilterEnabled,
    QString ExpEnabled,
    QString TimeStampEnabled,
    QString FITSDirectory
)
{
    XMLEle *root = NULL;
    XMLEle *ep, *subEP;
    root = addXMLEle(root, "root");

    if (!Exposure.isEmpty())
    {
        ep = addXMLEle(root, "Exposure");
        editXMLEle(ep, Exposure.toStdString().c_str());
    }

    if (!Filter.isEmpty())
    {
        ep = addXMLEle(root, "Filter");
        editXMLEle(ep, Filter.toStdString().c_str());
    }

    if (!Type.isEmpty())
    {
        ep = addXMLEle(root, "Type");
        editXMLEle(ep, Type.toStdString().c_str());
    }

    if (!Prefix.isEmpty())
    {
        ep = addXMLEle(root, "Prefix");
        if (!RawPrefix.isEmpty())
        {
            subEP = addXMLEle(ep, "RawPrefix");
            editXMLEle(subEP, RawPrefix.toStdString().c_str());
        }
        if (!FilterEnabled.isEmpty())
        {
            subEP = addXMLEle(ep, "FilterEnabled");
            editXMLEle(subEP, FilterEnabled.toStdString().c_str());
        }
        if (!ExpEnabled.isEmpty())
        {
            subEP = addXMLEle(ep, "ExpEnabled");
            editXMLEle(subEP, ExpEnabled.toStdString().c_str());
        }
        if (!TimeStampEnabled.isEmpty())
        {
            subEP = addXMLEle(ep, "TimeStampEnabled");
            editXMLEle(subEP, TimeStampEnabled.toStdString().c_str());
        }
    }

    if (!FITSDirectory.isEmpty())
    {
        ep = addXMLEle(root, "FITSDirectory");
        editXMLEle(ep, FITSDirectory.toStdString().c_str());
    }

    //prXMLEle(stdout, root, 0);

    return root;
}

void TestPlaceholderPath::testSchedulerProcessJobInfo_data()
{
#if QT_VERSION < 0x050900
    QSKIP("Skipping fixture-based test on old QT version.");
#else
    const QList<const char*> columns =
    {
        "Exposure",
        "Filter",
        "Type",
        "Prefix",
        "RawPrefix",
        "FilterEnabled",
        "ExpEnabled",
        "FITSDirectory",
        "targetName",
        "signature",
    };
    for (const auto &column : columns)
    {
        QTest::addColumn<QString>(column);
    }
    parseCSV(":/testSchedulerProcessJobInfo_data.csv", columns);

#endif
}

void TestPlaceholderPath::testSchedulerProcessJobInfo()
{
#if QT_VERSION < 0x050900
    QSKIP("Skipping fixture-based test on old QT version.");
#else
    QFETCH(QString, Exposure);
    QFETCH(QString, Filter);
    QFETCH(QString, Type);
    QFETCH(QString, Prefix);
    QFETCH(QString, RawPrefix);
    QFETCH(QString, FilterEnabled);
    QFETCH(QString, ExpEnabled);
    QFETCH(QString, FITSDirectory);
    /*
     * replace \s / ( ) : * ~ " characters by _
     * Remove any two or more __ by _
     * Remove any _ at the end
     */
    QFETCH(QString, targetName);
    QFETCH(QString, signature);

    XMLEle *root = buildXML(
                       Exposure,
                       Filter,
                       Type,
                       Prefix,
                       RawPrefix,
                       FilterEnabled,
                       ExpEnabled,
                       "",
                       FITSDirectory);

    Ekos::SequenceJob job(root);
    QCOMPARE(job.getFilterName(), Filter);
    auto placeholderPath = Ekos::PlaceholderPath();
    placeholderPath.processJobInfo(&job, targetName);

    QCOMPARE(job.getSignature(), signature);

    delXMLEle(root);
#endif
}

void TestPlaceholderPath::testCaptureAddJob_data()
{
#if QT_VERSION < 0x050900
    QSKIP("Skipping fixture-based test on old QT version.");
#else
    const QList<const char*> columns =
    {
        "Exposure",
        "Filter",
        "Type",
        "Prefix",
        "RawPrefix",
        "FilterEnabled",
        "ExpEnabled",
        "FITSDirectory",
        "targetName",
        "signature",
    };
    for (const auto &column : columns)
    {
        QTest::addColumn<QString>(column);
    }
    parseCSV(":/testSchedulerProcessJobInfo_data.csv", columns);
#endif
}

void TestPlaceholderPath::testCaptureAddJob()
{
#if QT_VERSION < 0x050900
    QSKIP("Skipping fixture-based test on old QT version.");
#else
    QFETCH(QString, Exposure);
    QFETCH(QString, Filter);
    QFETCH(QString, Type);
    QFETCH(QString, Prefix);
    QFETCH(QString, RawPrefix);
    QFETCH(QString, FilterEnabled);
    QFETCH(QString, ExpEnabled);
    QFETCH(QString, FITSDirectory);
    /*
     * replace \s / ( ) : * ~ " characters by _
     * Remove any two or more __ by _
     * Remove any _ at the end
     */
    QFETCH(QString, targetName);
    QFETCH(QString, signature);

    XMLEle *root = buildXML(
                       Exposure,
                       Filter,
                       Type,
                       Prefix,
                       RawPrefix,
                       FilterEnabled,
                       ExpEnabled,
                       "",
                       FITSDirectory);

    // for addJob, targetName should already be sanitized
    // taken from scheduler.cpp:2491-2495
    targetName = targetName.replace( QRegularExpression("\\s|/|\\(|\\)|:|\\*|~|\"" ), "_" )
                 // Remove any two or more __
                 .replace( QRegularExpression("_{2,}"), "_")
                 // Remove any _ at the end
                 .replace( QRegularExpression("_$"), "");
    Ekos::SequenceJob job(root);
    QCOMPARE(job.getFilterName(), Filter);
    auto placeholderPath = Ekos::PlaceholderPath();
    placeholderPath.addJob(&job, targetName);

    QCOMPARE(job.getSignature(), signature);

    delXMLEle(root);
#endif
}

void TestPlaceholderPath::testCCDGenerateFilename_data()
{
#if QT_VERSION < 0x050900
    QSKIP("Skipping fixture-based test on old QT version.");
#else
    QTest::addColumn<QString>("format");
    QTest::addColumn<bool>("batch_mode");
    QTest::addColumn<QString>("fitsDir");
    QTest::addColumn<QString>("seqPrefix");
    QTest::addColumn<int>("nextSequenceID");
    QTest::addColumn<QString>("desiredFilename");

    // format
    QTest::addRow("0")  << ""      << false << ""    << ""            << 0 << "/.+/kstars/000";
    QTest::addRow("1")  << ""      << false << ""    << ""            << 1 << "/.+/kstars/001";
    QTest::addRow("2")  << ""      << false << ""    << "_ISO8601"    << 0 <<
                        "/.+/kstars/_\\d{4}-\\d{2}-\\d{2}T\\d{2}-\\d{2}-\\d{2}_000";
    QTest::addRow("3")  << ""      << false << ""    << "_ISO8601"    << 1 <<
                        "/.+/kstars/_\\d{4}-\\d{2}-\\d{2}T\\d{2}-\\d{2}-\\d{2}_001";
    QTest::addRow("4")  << ""      << false << ""    << "_ISO8601bar" << 0 <<
                        "/.+/kstars/_\\d{4}-\\d{2}-\\d{2}T\\d{2}-\\d{2}-\\d{2}bar_000";
    QTest::addRow("5")  << ""      << false << ""    << "_ISO8601bar" << 1 <<
                        "/.+/kstars/_\\d{4}-\\d{2}-\\d{2}T\\d{2}-\\d{2}-\\d{2}bar_001";
    QTest::addRow("6")  << ""      << false << ""    << "bar"         << 0 << "/.+/kstars/bar_000";
    QTest::addRow("7")  << ""      << false << ""    << "bar"         << 1 << "/.+/kstars/bar_001";
    QTest::addRow("8")  << ""      << false << "foo" << ""            << 0 << "/.+/kstars/000";
    QTest::addRow("9")  << ""      << false << "foo" << ""            << 1 << "/.+/kstars/001";
    QTest::addRow("10") << ""      << false << "foo" << "_ISO8601"    << 0 <<
                        "/.+/kstars/_\\d{4}-\\d{2}-\\d{2}T\\d{2}-\\d{2}-\\d{2}_000";
    QTest::addRow("11") << ""      << false << "foo" << "_ISO8601"    << 1 <<
                        "/.+/kstars/_\\d{4}-\\d{2}-\\d{2}T\\d{2}-\\d{2}-\\d{2}_001";
    QTest::addRow("12") << ""      << false << "foo" << "_ISO8601bar" << 0 <<
                        "/.+/kstars/_\\d{4}-\\d{2}-\\d{2}T\\d{2}-\\d{2}-\\d{2}bar_000";
    QTest::addRow("13") << ""      << false << "foo" << "_ISO8601bar" << 1 <<
                        "/.+/kstars/_\\d{4}-\\d{2}-\\d{2}T\\d{2}-\\d{2}-\\d{2}bar_001";
    QTest::addRow("14") << ""      << false << "foo" << "bar"         << 0 << "/.+/kstars/bar_000";
    QTest::addRow("15") << ""      << false << "foo" << "bar"         << 1 << "/.+/kstars/bar_001";
    QTest::addRow("16") << ""      << true  << ""    << ""            << 0 << "/.+/000";
    QTest::addRow("17") << ""      << true  << ""    << ""            << 1 << "/.+/001";
    QTest::addRow("18") << ""      << true  << ""    << "_ISO8601"    << 0 <<
                        "/.+/_\\d{4}-\\d{2}-\\d{2}T\\d{2}-\\d{2}-\\d{2}_000";
    QTest::addRow("19") << ""      << true  << ""    << "_ISO8601"    << 1 <<
                        "/.+/_\\d{4}-\\d{2}-\\d{2}T\\d{2}-\\d{2}-\\d{2}_001";
    QTest::addRow("20") << ""      << true  << ""    << "_ISO8601bar" << 0 <<
                        "/.+/_\\d{4}-\\d{2}-\\d{2}T\\d{2}-\\d{2}-\\d{2}bar_000";
    QTest::addRow("21") << ""      << true  << ""    << "_ISO8601bar" << 1 <<
                        "/.+/_\\d{4}-\\d{2}-\\d{2}T\\d{2}-\\d{2}-\\d{2}bar_001";
    QTest::addRow("22") << ""      << true  << ""    << "bar"         << 0 << "/.+/bar_000";
    QTest::addRow("23") << ""      << true  << ""    << "bar"         << 1 << "/.+/bar_001";
    QTest::addRow("24") << ""      << true  << "foo" << ""            << 0 << "foo/000";
    QTest::addRow("25") << ""      << true  << "foo" << ""            << 1 << "foo/001";
    QTest::addRow("26") << ""      << true  << "foo" << "_ISO8601"    << 0 <<
                        "foo/_\\d{4}-\\d{2}-\\d{2}T\\d{2}-\\d{2}-\\d{2}_000";
    QTest::addRow("27") << ""      << true  << "foo" << "_ISO8601"    << 1 <<
                        "foo/_\\d{4}-\\d{2}-\\d{2}T\\d{2}-\\d{2}-\\d{2}_001";
    QTest::addRow("28") << ""      << true  << "foo" << "_ISO8601bar" << 0 <<
                        "foo/_\\d{4}-\\d{2}-\\d{2}T\\d{2}-\\d{2}-\\d{2}bar_000";
    QTest::addRow("29") << ""      << true  << "foo" << "_ISO8601bar" << 1 <<
                        "foo/_\\d{4}-\\d{2}-\\d{2}T\\d{2}-\\d{2}-\\d{2}bar_001";
    QTest::addRow("30") << ""      << true  << "foo" << "bar"         << 0 << "foo/bar_000";
    QTest::addRow("31") << ""      << true  << "foo" << "bar"         << 1 << "foo/bar_001";
    QTest::addRow("32") << ".fits" << false << ""    << ""            << 0 << "/.+/kstars/000.fits";
    QTest::addRow("33") << ".fits" << false << ""    << ""            << 1 << "/.+/kstars/001.fits";
    QTest::addRow("34") << ".fits" << false << ""    << "_ISO8601"    << 0 <<
                        "/.+/kstars/_\\d{4}-\\d{2}-\\d{2}T\\d{2}-\\d{2}-\\d{2}_000.fits";
    QTest::addRow("35") << ".fits" << false << ""    << "_ISO8601"    << 1 <<
                        "/.+/kstars/_\\d{4}-\\d{2}-\\d{2}T\\d{2}-\\d{2}-\\d{2}_001.fits";
    QTest::addRow("36") << ".fits" << false << ""    << "_ISO8601bar" << 0 <<
                        "/.+/kstars/_\\d{4}-\\d{2}-\\d{2}T\\d{2}-\\d{2}-\\d{2}bar_000.fits";
    QTest::addRow("37") << ".fits" << false << ""    << "_ISO8601bar" << 1 <<
                        "/.+/kstars/_\\d{4}-\\d{2}-\\d{2}T\\d{2}-\\d{2}-\\d{2}bar_001.fits";
    QTest::addRow("38") << ".fits" << false << ""    << "bar"         << 0 << "/.+/kstars/bar_000.fits";
    QTest::addRow("39") << ".fits" << false << ""    << "bar"         << 1 << "/.+/kstars/bar_001.fits";
    QTest::addRow("40") << ".fits" << false << "foo" << ""            << 0 << "/.+/kstars/000.fits";
    QTest::addRow("41") << ".fits" << false << "foo" << ""            << 1 << "/.+/kstars/001.fits";
    QTest::addRow("42") << ".fits" << false << "foo" << "_ISO8601"    << 0 <<
                        "/.+/kstars/_\\d{4}-\\d{2}-\\d{2}T\\d{2}-\\d{2}-\\d{2}_000.fits";
    QTest::addRow("43") << ".fits" << false << "foo" << "_ISO8601"    << 1 <<
                        "/.+/kstars/_\\d{4}-\\d{2}-\\d{2}T\\d{2}-\\d{2}-\\d{2}_001.fits";
    QTest::addRow("44") << ".fits" << false << "foo" << "_ISO8601bar" << 0 <<
                        "/.+/kstars/_\\d{4}-\\d{2}-\\d{2}T\\d{2}-\\d{2}-\\d{2}bar_000.fits";
    QTest::addRow("45") << ".fits" << false << "foo" << "_ISO8601bar" << 1 <<
                        "/.+/kstars/_\\d{4}-\\d{2}-\\d{2}T\\d{2}-\\d{2}-\\d{2}bar_001.fits";
    QTest::addRow("46") << ".fits" << false << "foo" << "bar"         << 0 << "/.+/kstars/bar_000.fits";
    QTest::addRow("47") << ".fits" << false << "foo" << "bar"         << 1 << "/.+/kstars/bar_001.fits";
    QTest::addRow("48") << ".fits" << true  << ""    << ""            << 0 << "/.+/000.fits";
    QTest::addRow("49") << ".fits" << true  << ""    << ""            << 1 << "/.+/001.fits";
    QTest::addRow("50") << ".fits" << true  << ""    << "_ISO8601"    << 0 <<
                        "/.+/_\\d{4}-\\d{2}-\\d{2}T\\d{2}-\\d{2}-\\d{2}_000.fits";
    QTest::addRow("51") << ".fits" << true  << ""    << "_ISO8601"    << 1 <<
                        "/.+/_\\d{4}-\\d{2}-\\d{2}T\\d{2}-\\d{2}-\\d{2}_001.fits";
    QTest::addRow("52") << ".fits" << true  << ""    << "_ISO8601bar" << 0 <<
                        "/.+/_\\d{4}-\\d{2}-\\d{2}T\\d{2}-\\d{2}-\\d{2}bar_000.fits";
    QTest::addRow("53") << ".fits" << true  << ""    << "_ISO8601bar" << 1 <<
                        "/.+/_\\d{4}-\\d{2}-\\d{2}T\\d{2}-\\d{2}-\\d{2}bar_001.fits";
    QTest::addRow("54") << ".fits" << true  << ""    << "bar"         << 0 << "/.+/bar_000.fits";
    QTest::addRow("55") << ".fits" << true  << ""    << "bar"         << 1 << "/.+/bar_001.fits";
    QTest::addRow("56") << ".fits" << true  << "foo" << ""            << 0 << "foo/000.fits";
    QTest::addRow("57") << ".fits" << true  << "foo" << ""            << 1 << "foo/001.fits";
    QTest::addRow("58") << ".fits" << true  << "foo" << "_ISO8601"    << 0 <<
                        "foo/_\\d{4}-\\d{2}-\\d{2}T\\d{2}-\\d{2}-\\d{2}_000.fits";
    QTest::addRow("59") << ".fits" << true  << "foo" << "_ISO8601"    << 1 <<
                        "foo/_\\d{4}-\\d{2}-\\d{2}T\\d{2}-\\d{2}-\\d{2}_001.fits";
    QTest::addRow("60") << ".fits" << true  << "foo" << "_ISO8601bar" << 0 <<
                        "foo/_\\d{4}-\\d{2}-\\d{2}T\\d{2}-\\d{2}-\\d{2}bar_000.fits";
    QTest::addRow("61") << ".fits" << true  << "foo" << "_ISO8601bar" << 1 <<
                        "foo/_\\d{4}-\\d{2}-\\d{2}T\\d{2}-\\d{2}-\\d{2}bar_001.fits";
    QTest::addRow("62") << ".fits" << true  << "foo" << "bar"         << 0 << "foo/bar_000.fits";
    QTest::addRow("63") << ".fits" << true  << "foo" << "bar"         << 1 << "foo/bar_001.fits";
#endif
}

void TestPlaceholderPath::testCCDGenerateFilename()
{
#if QT_VERSION < 0x050900
    QSKIP("Skipping fixture-based test on old QT version.");
#else
    QFETCH(QString, format);
    QFETCH(bool, batch_mode);
    QFETCH(QString, fitsDir);
    QFETCH(QString, seqPrefix);
    QFETCH(int, nextSequenceID);
    QFETCH(QString, desiredFilename);

    QString filename;
    auto placeholderPath = Ekos::PlaceholderPath();
    placeholderPath.generateFilenameOld(format, batch_mode, &filename, fitsDir, seqPrefix, nextSequenceID);

    QVERIFY(QRegularExpression(desiredFilename).match(filename).hasMatch());
#endif
}

void TestPlaceholderPath::testSequenceJobSignature_data()
{
#if QT_VERSION < 0x050900
    QSKIP("Skipping fixture-based test on old QT version.");
#else
    QTest::addColumn<QString>("localDir");
    QTest::addColumn<QString>("directoryPostfix");
    QTest::addColumn<QString>("fullPrefix");
    QTest::addColumn<QString>("signature");

    QTest::addRow("empty")                                << ""     << ""    << ""     << "/";
    QTest::addRow("localDir")                             << "/foo" << ""    << ""     << "/foo/";
    QTest::addRow("directoryPostfix")                     << ""     << "bar" << ""     << "bar/";
    QTest::addRow("fullPrefix")                           << ""     << ""    << "toto" << "/toto";
    QTest::addRow("localDir+directoryPostfix")            << "/foo" << "bar" << ""     << "/foobar/";
    QTest::addRow("directoryPostfix+fullPrefix")          << ""     << "bar" << "toto" << "bar/toto";
    QTest::addRow("localDir+fullPrefix")                  << "/foo" << ""    << "toto" << "/foo/toto";
    QTest::addRow("localDir+directoryPostfix+fullPrefix") << "/foo" << "bar" << "toto" << "/foobar/toto";
#endif
}

Q_DECLARE_METATYPE(CCDFrameType)

void TestPlaceholderPath::testSequenceJobSignature()
{
#if QT_VERSION < 0x050900
    QSKIP("Skipping fixture-based test on old QT version.");
#else
    QFETCH(QString, localDir);
    QFETCH(QString, directoryPostfix);
    QFETCH(QString, fullPrefix);
    QFETCH(QString, signature);

    auto job = new Ekos::SequenceJob();
    job->setLocalDir(localDir);
    job->setDirectoryPostfix(directoryPostfix);
    job->setFullPrefix(fullPrefix);

    QCOMPARE(job->getSignature(), signature);

    delete job;
#endif
}

void TestPlaceholderPath::testFullNamingSequence_data()
{
#if QT_VERSION < 0x050900
    QSKIP("Skipping fixture-based test on old QT version.");
#else
    const QList<const char*> columns =
    {
        "Exposure",
        "Filter",
        "Type",
        "Prefix",
        "RawPrefix",
        "FilterEnabled",
        "ExpEnabled",
        "TimeStampEnabled",
        "FITSDirectory",
        "targetName",
        "batch_mode",
        "nextSequenceID",
        "result",
    };
    for (const auto &column : columns)
    {
        QTest::addColumn<QString>(column);
    }
    parseCSV(":/testFullNamingSequence_data.csv", columns);
#endif
}

void TestPlaceholderPath::testFullNamingSequence()
{
#if QT_VERSION < 0x050900
    QSKIP("Skipping fixture-based test on old QT version.");
#else

    QFETCH(QString, Exposure);
    QFETCH(QString, Filter);
    QFETCH(QString, Type);
    QFETCH(QString, Prefix);
    QFETCH(QString, RawPrefix);
    QFETCH(QString, FilterEnabled);
    QFETCH(QString, ExpEnabled);
    QFETCH(QString, TimeStampEnabled);
    QFETCH(QString, FITSDirectory);
    QFETCH(QString, targetName);
    QFETCH(QString, batch_mode);
    QFETCH(QString, nextSequenceID);
    QFETCH(QString, result);

    XMLEle *root = buildXML(
                       Exposure,
                       Filter,
                       Type,
                       Prefix,
                       RawPrefix,
                       FilterEnabled,
                       ExpEnabled,
                       TimeStampEnabled,
                       FITSDirectory);

    Ekos::SequenceJob job(root);
    auto placeholderPath = Ekos::PlaceholderPath();
    // for addJob, targetName should already be sanitized
    // taken from scheduler.cpp:2491-2495
    targetName = targetName.replace( QRegularExpression("\\s|/|\\(|\\)|:|\\*|~|\"" ), "_" )
                 // Remove any two or more __
                 .replace( QRegularExpression("_{2,}"), "_")
                 // Remove any _ at the end
                 .replace( QRegularExpression("_$"), "");
    placeholderPath.addJob(&job, targetName);
    QString fitsDir, filename;
    // from sequencejob.cpp:302-303
    if (job.getLocalDir().isEmpty() == false)
        fitsDir = job.getLocalDir() + job.getDirectoryPostfix();
    placeholderPath.generateFilenameOld(".fits", bool(batch_mode.toInt()), &filename, fitsDir, job.getFullPrefix(),
                                        nextSequenceID.toInt());
    //QCOMPARE(filename, result);
    QVERIFY2(QRegularExpression(result).match(filename).hasMatch(),
             QString("\nExpected: %1\nObtained: %2\n").arg(result, filename).toStdString().c_str());

#endif
}

void TestPlaceholderPath::testFlexibleNaming_data()
{
#if QT_VERSION < 0x050900
    QSKIP("Skipping fixture-based test on old QT version.");
#else
    const QList<const char*> columns =
    {
        "Exposure",
        "Filter",
        "Type",
        "Prefix",
        "RawPrefix",
        "FilterEnabled",
        "ExpEnabled",
        "TimeStampEnabled",
        "seqFilename",
        "targetName",
        "batch_mode",
        "nextSequenceID",
        "format",
        "result",
    };
    for (const auto &column : columns)
    {
        QTest::addColumn<QString>(column);
    }
    QTest::addRow("infinite")  << "" << "" << "" << "" << "" << "" << "" << "" << "/home/user/Images/NGC7635/%f_100x30s_RGB.esq"
                               << "" << "1" << "" << "%f"  << "^%f_100x30s_RGB\\.fits$";
    QTest::addRow("f")  << "" << "" << "" << "" << "" << "" << "" << "" << "/home/user/Images/NGC7635/100x30s_RGB.esq" << "" <<
                        "1" << "" << "%f"  << "^100x30s_RGB\\.fits$";
    QTest::addRow("p")  << "" << "" << "" << "" << "" << "" << "" << "" << "/home/user/Images/NGC7635/100x30s_RGB.esq" << "" <<
                        "1" << "" << "%p"  << "^/home/user/Images/NGC7635\\.fits$";
    QTest::addRow("p1") << "" << "" << "" << "" << "" << "" << "" << "" << "/home/user/Images/NGC7635/100x30s_RGB.esq" << "" <<
                        "1" << "" << "%p1" << "^/home/user/Images/NGC7635\\.fits$";
    QTest::addRow("p2") << "" << "" << "" << "" << "" << "" << "" << "" << "/home/user/Images/NGC7635/100x30s_RGB.esq" << "" <<
                        "1" << "" << "%p2" << "^/home/user/Images\\.fits$";
    QTest::addRow("p3") << "" << "" << "" << "" << "" << "" << "" << "" << "/home/user/Images/NGC7635/100x30s_RGB.esq" << "" <<
                        "1" << "" << "%p3" << "^/home/user\\.fits$";
    QTest::addRow("p4") << "" << "" << "" << "" << "" << "" << "" << "" << "/home/user/Images/NGC7635/100x30s_RGB.esq" << "" <<
                        "1" << "" << "%p4" << "^/home\\.fits$";
    QTest::addRow("d")  << "" << "" << "" << "" << "" << "" << "" << "" << "/home/user/Images/NGC7635/100x30s_RGB.esq" << "" <<
                        "1" << "" << "%d"  << "^NGC7635\\.fits$";
    QTest::addRow("d1") << "" << "" << "" << "" << "" << "" << "" << "" << "/home/user/Images/NGC7635/100x30s_RGB.esq" << "" <<
                        "1" << "" << "%d1" << "^NGC7635\\.fits$";
    QTest::addRow("d2") << "" << "" << "" << "" << "" << "" << "" << "" << "/home/user/Images/NGC7635/100x30s_RGB.esq" << "" <<
                        "1" << "" << "%d2" << "^Images\\.fits$";
    QTest::addRow("d3") << "" << "" << "" << "" << "" << "" << "" << "" << "/home/user/Images/NGC7635/100x30s_RGB.esq" << "" <<
                        "1" << "" << "%d3" << "^user\\.fits$";
    QTest::addRow("d4") << "" << "" << "" << "" << "" << "" << "" << "" << "/home/user/Images/NGC7635/100x30s_RGB.esq" << "" <<
                        "1" << "" << "%d4" << "^home\\.fits$";

    QTest::addRow("t")  << ""      << ""      << ""      << ""       << "" << ""  << ""  << ""  << "" << "target" << "" << ""
                        << "%t"  << "^/tmp/kstars/target\\.fits$";
    QTest::addRow("T")  << ""      << ""      << "Light" << ""       << "" << ""  << ""  << ""  << "" << ""       << "" << ""
                        << "%T"  << "^/tmp/kstars/Light\\.fits$";
    QTest::addRow("F")  << ""      << "sobel" << ""      << "prefix" << "" << "1" << ""  << ""  << "" << ""       << "" << ""
                        << "%F"  << "^/tmp/kstars/sobel\\.fits$";
    QTest::addRow("e")  << "0.001" << ""      << ""      << "prefix" << "" << ""  << "1" << ""  << "" << ""       << "" << ""
                        << "%e"  << "^/tmp/kstars/0.001_secs\\.fits$";
    QTest::addRow("D")  << ""      << ""      << ""      << "prefix" << "" << ""  << ""  << "1" << "" << ""       << "" << ""
                        << "%D"  << "^/tmp/kstars/\\d{4}-\\d{2}-\\d{2}T\\d{2}-\\d{2}-\\d{2}\\.fits$";
    QTest::addRow("s")  << ""      << ""      << ""      << ""       << "" << ""  << ""  << ""  << "" << ""       << "" << "1"
                        << "%s"  << "^/tmp/kstars/1\\.fits$";
    QTest::addRow("s1") << ""      << ""      << ""      << ""       << "" << ""  << ""  << ""  << "" << ""       << "" << "1"
                        << "%s1" << "^/tmp/kstars/1\\.fits$";
    QTest::addRow("s2") << ""      << ""      << ""      << ""       << "" << ""  << ""  << ""  << "" << ""       << "" << "1"
                        << "%s2" << "^/tmp/kstars/01\\.fits$";
    QTest::addRow("s3") << ""      << ""      << ""      << ""       << "" << ""  << ""  << ""  << "" << ""       << "" << "1"
                        << "%s3" << "^/tmp/kstars/001\\.fits$";
    QTest::addRow("s4") << ""      << ""      << ""      << ""       << "" << ""  << ""  << ""  << "" << ""       << "" << "1"
                        << "%s4" << "^/tmp/kstars/0001\\.fits$";

    QTest::addRow("_s") << "" << "" << "" << "" << "" << "" << "" << "" << "" << "" << "" << "1" << "_%s" <<
                        "^/tmp/kstars/_1\\.fits$";
    QTest::addRow("unknown1") << "" << "" << "" << "" << "" << "" << "" << "" << "" << "" << "" << "" << "%b" <<
                              "^/tmp/kstars/%b\\.fits$";
    QTest::addRow("unknown2") << "" << "" << "" << "" << "" << "" << "" << "" << "" << "" << "" << "" << "%f_%a_%t" <<
                              "^/tmp/kstars/%a_\\.fits$";

    parseCSV(":/testFlexibleNaming_data.csv", columns);
#endif
}

void TestPlaceholderPath::testFlexibleNaming()
{
#if QT_VERSION < 0x050900
    QSKIP("Skipping fixture-based test on old QT version.");
#else
    QFETCH(QString, Exposure);
    QFETCH(QString, Filter);
    QFETCH(QString, Type);
    QFETCH(QString, Prefix);
    QFETCH(QString, RawPrefix);
    QFETCH(QString, FilterEnabled);
    QFETCH(QString, ExpEnabled);
    QFETCH(QString, TimeStampEnabled);
    QFETCH(QString, seqFilename);
    QFETCH(QString, targetName);
    QFETCH(QString, batch_mode);
    QFETCH(QString, nextSequenceID);
    QFETCH(QString, format);
    QFETCH(QString, result);

    XMLEle *root = buildXML(
                       Exposure,
                       Filter,
                       Type,
                       Prefix,
                       RawPrefix,
                       FilterEnabled,
                       ExpEnabled,
                       TimeStampEnabled,
                       "");

    Ekos::SequenceJob job(root);
    auto placeholderPath = Ekos::PlaceholderPath(seqFilename);
    QString filename;
    bool bm = bool(batch_mode.toInt());
    int i = nextSequenceID.toInt();
    placeholderPath.generateFilename(format, job, targetName, bm, i, ".fits", &filename);
    QVERIFY2(QRegularExpression(result).match(filename).hasMatch(),
             QString("\nExpected: %1\nObtained: %2\n").arg(result, filename).toStdString().c_str());

    job.setTargetName(targetName);
    placeholderPath.setGenerateFilenameSettings(job);
    placeholderPath.generateFilename(format, job.isTimestampPrefixEnabled(), bm, i, ".fits", &filename);
    QVERIFY2(QRegularExpression(result).match(filename).hasMatch(),
             QString("\nExpected: %1\nObtained: %2\n").arg(result, filename).toStdString().c_str());
#endif
}

void TestPlaceholderPath::testFlexibleNamingChangeBehavior_data()
{
#if QT_VERSION < 0x050900
    QSKIP("Skipping fixture-based test on old QT version.");
#else
    const QList<const char*> columns =
    {
        "Exposure",
        "Filter",
        "Type",
        "Prefix",
        "RawPrefix",
        "FilterEnabled",
        "ExpEnabled",
        "TimeStampEnabled",
        "seqFilename",
        "targetName",
        "batch_mode",
        "nextSequenceID",
        "format",
        "old_result",
        "new_result",
    };
    for (const auto &column : columns)
    {
        QTest::addColumn<QString>(column);
    }
    parseCSV(":/testFlexibleNamingChangeBehavior_data.csv", columns);
#endif
}

void TestPlaceholderPath::testFlexibleNamingChangeBehavior()
{
#if QT_VERSION < 0x050900
    QSKIP("Skipping fixture-based test on old QT version.");
#else
    QFETCH(QString, Exposure);
    QFETCH(QString, Filter);
    QFETCH(QString, Type);
    QFETCH(QString, Prefix);
    QFETCH(QString, RawPrefix);
    QFETCH(QString, FilterEnabled);
    QFETCH(QString, ExpEnabled);
    QFETCH(QString, TimeStampEnabled);
    QFETCH(QString, seqFilename);
    QFETCH(QString, targetName);
    QFETCH(QString, batch_mode);
    QFETCH(QString, nextSequenceID);
    QFETCH(QString, format);
    QFETCH(QString, old_result);
    QFETCH(QString, new_result);

    XMLEle *root = buildXML(
                       Exposure,
                       Filter,
                       Type,
                       Prefix,
                       RawPrefix,
                       FilterEnabled,
                       ExpEnabled,
                       TimeStampEnabled,
                       "");

    Ekos::SequenceJob job(root);
    auto placeholderPath = Ekos::PlaceholderPath(seqFilename);
    QString filename;
    bool bm = bool(batch_mode.toInt());
    int i = nextSequenceID.toInt();
    placeholderPath.generateFilename(format, job, targetName, bm, i, ".fits", &filename);
    QEXPECT_FAIL("", "original filename had __ or // or /_ that are now removed", Continue);
    QVERIFY2(QRegularExpression(old_result).match(filename).hasMatch(),
             QString("\nNot Expected: %1\nObtained: %2\n").arg(old_result, filename).toStdString().c_str());
    QVERIFY2(QRegularExpression(new_result).match(filename).hasMatch(),
             QString("\nExpected: %1\nObtained: %2\n").arg(new_result, filename).toStdString().c_str());

    job.setTargetName(targetName);
    placeholderPath.setGenerateFilenameSettings(job);
    placeholderPath.generateFilename(format, job.isTimestampPrefixEnabled(), bm, i, ".fits", &filename);
    QEXPECT_FAIL("", "original filename had __ or // or /_ that are now removed", Continue);
    QVERIFY2(QRegularExpression(old_result).match(filename).hasMatch(),
             QString("\nNot Expected: %1\nObtained: %2\n").arg(old_result, filename).toStdString().c_str());
    QVERIFY2(QRegularExpression(new_result).match(filename).hasMatch(),
             QString("\nExpected: %1\nObtained: %2\n").arg(new_result, filename).toStdString().c_str());
#endif
}

void TestPlaceholderPath::testRemainingPlaceholders_data()
{
#if QT_VERSION < 0x050900
    QSKIP("Skipping fixture-based test on old QT version.");
#else
    QTest::addColumn<QString>("Filename");
    QTest::addColumn<QStringList>("Result");
    QTest::addRow("0")  << "test.fits" << QStringList();
    QTest::addRow("1")  << "%f_test.fits" << QStringList({"%f"});
    QTest::addRow("2")  << "%p/%f_test.fits" << QStringList({"%p", "%f"});
#endif
}

void TestPlaceholderPath::testRemainingPlaceholders()
{
#if QT_VERSION < 0x050900
    QSKIP("Skipping fixture-based test on old QT version.");
#else
    QFETCH(QString, Filename);
    QFETCH(QStringList, Result);

    auto remainingPlaceholders = Ekos::PlaceholderPath::remainingPlaceholders(Filename);
    QCOMPARE(remainingPlaceholders, Result);
#endif
}
QTEST_GUILESS_MAIN(TestPlaceholderPath)
