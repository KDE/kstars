/***************************************************************************
                   test_placeholderpath.cpp  -  KStars Planetarium
                             -------------------
    begin                : Mon 18 Jan 2021 11:51:21 CDT
    copyright            : (c) 2021 by Kwon-Young Choi
    email                : kwon-young.choi@hotmail.fr
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

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

  if (!testDataFile.open(QIODevice::ReadOnly)) {
    qDebug() << testDataFile.errorString();
    QFAIL("error");
  }

  // checking csv header
  int columnCpt = 0;
  QByteArray line = testDataFile.readLine().replace("\n", "");

  for (auto el: line.split(',')) {
    if (columns.size() <= columnCpt)
      QFAIL("too many csv columns");
    else if (el != columns[columnCpt]) {
      QFAIL(QString("csv columns incorrect %1 %2").arg(el, columns[columnCpt]).toStdString().c_str());
    }
    columnCpt++;
  }

  if (columns.size() != columnCpt)
    QFAIL("not enough csv columns");

  int cpt = 0;

  while (!testDataFile.atEnd()) {
    QByteArray line = testDataFile.readLine().replace("\n", "");
    QTestData& row = QTest::addRow("%d", cpt);
    for (auto el: line.split(',')) {
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

  if (!Exposure.isEmpty()) {
    ep = addXMLEle(root, "Exposure");
    editXMLEle(ep, Exposure.toStdString().c_str());
  }

  if (!Filter.isEmpty()) {
    ep = addXMLEle(root, "Filter");
    editXMLEle(ep, Filter.toStdString().c_str());
  }

  if (!Type.isEmpty()) {
    ep = addXMLEle(root, "Type");
    editXMLEle(ep, Type.toStdString().c_str());
  }

  if (!Prefix.isEmpty()) {
    ep = addXMLEle(root, "Prefix");
    if (!RawPrefix.isEmpty()) {
      subEP = addXMLEle(ep, "RawPrefix");
      editXMLEle(subEP, RawPrefix.toStdString().c_str());
    }
    if (!FilterEnabled.isEmpty()) {
      subEP = addXMLEle(ep, "FilterEnabled");
      editXMLEle(subEP, FilterEnabled.toStdString().c_str());
    }
    if (!ExpEnabled.isEmpty()) {
      subEP = addXMLEle(ep, "ExpEnabled");
      editXMLEle(subEP, ExpEnabled.toStdString().c_str());
    }
    if (!TimeStampEnabled.isEmpty()) {
      subEP = addXMLEle(ep, "TimeStampEnabled");
      editXMLEle(subEP, TimeStampEnabled.toStdString().c_str());
    }
  }

  if (!FITSDirectory.isEmpty()) {
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
  const QList<const char*> columns = {
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
  for (const auto &column: columns) {
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
  const QList<const char*> columns = {
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
  for (const auto &column: columns) {
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
  QTest::addRow("2")  << ""      << false << ""    << "_ISO8601"    << 0 << "/.+/kstars/_\\d{4}-\\d{2}-\\d{2}T\\d{2}-\\d{2}-\\d{2}_000";
  QTest::addRow("3")  << ""      << false << ""    << "_ISO8601"    << 1 << "/.+/kstars/_\\d{4}-\\d{2}-\\d{2}T\\d{2}-\\d{2}-\\d{2}_001";
  QTest::addRow("4")  << ""      << false << ""    << "_ISO8601bar" << 0 << "/.+/kstars/_\\d{4}-\\d{2}-\\d{2}T\\d{2}-\\d{2}-\\d{2}bar_000";
  QTest::addRow("5")  << ""      << false << ""    << "_ISO8601bar" << 1 << "/.+/kstars/_\\d{4}-\\d{2}-\\d{2}T\\d{2}-\\d{2}-\\d{2}bar_001";
  QTest::addRow("6")  << ""      << false << ""    << "bar"         << 0 << "/.+/kstars/bar_000";
  QTest::addRow("7")  << ""      << false << ""    << "bar"         << 1 << "/.+/kstars/bar_001";
  QTest::addRow("8")  << ""      << false << "foo" << ""            << 0 << "/.+/kstars/000";
  QTest::addRow("9")  << ""      << false << "foo" << ""            << 1 << "/.+/kstars/001";
  QTest::addRow("10") << ""      << false << "foo" << "_ISO8601"    << 0 << "/.+/kstars/_\\d{4}-\\d{2}-\\d{2}T\\d{2}-\\d{2}-\\d{2}_000";
  QTest::addRow("11") << ""      << false << "foo" << "_ISO8601"    << 1 << "/.+/kstars/_\\d{4}-\\d{2}-\\d{2}T\\d{2}-\\d{2}-\\d{2}_001";
  QTest::addRow("12") << ""      << false << "foo" << "_ISO8601bar" << 0 << "/.+/kstars/_\\d{4}-\\d{2}-\\d{2}T\\d{2}-\\d{2}-\\d{2}bar_000";
  QTest::addRow("13") << ""      << false << "foo" << "_ISO8601bar" << 1 << "/.+/kstars/_\\d{4}-\\d{2}-\\d{2}T\\d{2}-\\d{2}-\\d{2}bar_001";
  QTest::addRow("14") << ""      << false << "foo" << "bar"         << 0 << "/.+/kstars/bar_000";
  QTest::addRow("15") << ""      << false << "foo" << "bar"         << 1 << "/.+/kstars/bar_001";
  QTest::addRow("16") << ""      << true  << ""    << ""            << 0 << "/.+/000";
  QTest::addRow("17") << ""      << true  << ""    << ""            << 1 << "/.+/001";
  QTest::addRow("18") << ""      << true  << ""    << "_ISO8601"    << 0 << "/.+/_\\d{4}-\\d{2}-\\d{2}T\\d{2}-\\d{2}-\\d{2}_000";
  QTest::addRow("19") << ""      << true  << ""    << "_ISO8601"    << 1 << "/.+/_\\d{4}-\\d{2}-\\d{2}T\\d{2}-\\d{2}-\\d{2}_001";
  QTest::addRow("20") << ""      << true  << ""    << "_ISO8601bar" << 0 << "/.+/_\\d{4}-\\d{2}-\\d{2}T\\d{2}-\\d{2}-\\d{2}bar_000";
  QTest::addRow("21") << ""      << true  << ""    << "_ISO8601bar" << 1 << "/.+/_\\d{4}-\\d{2}-\\d{2}T\\d{2}-\\d{2}-\\d{2}bar_001";
  QTest::addRow("22") << ""      << true  << ""    << "bar"         << 0 << "/.+/bar_000";
  QTest::addRow("23") << ""      << true  << ""    << "bar"         << 1 << "/.+/bar_001";
  QTest::addRow("24") << ""      << true  << "foo" << ""            << 0 << "foo/000";
  QTest::addRow("25") << ""      << true  << "foo" << ""            << 1 << "foo/001";
  QTest::addRow("26") << ""      << true  << "foo" << "_ISO8601"    << 0 << "foo/_\\d{4}-\\d{2}-\\d{2}T\\d{2}-\\d{2}-\\d{2}_000";
  QTest::addRow("27") << ""      << true  << "foo" << "_ISO8601"    << 1 << "foo/_\\d{4}-\\d{2}-\\d{2}T\\d{2}-\\d{2}-\\d{2}_001";
  QTest::addRow("28") << ""      << true  << "foo" << "_ISO8601bar" << 0 << "foo/_\\d{4}-\\d{2}-\\d{2}T\\d{2}-\\d{2}-\\d{2}bar_000";
  QTest::addRow("29") << ""      << true  << "foo" << "_ISO8601bar" << 1 << "foo/_\\d{4}-\\d{2}-\\d{2}T\\d{2}-\\d{2}-\\d{2}bar_001";
  QTest::addRow("30") << ""      << true  << "foo" << "bar"         << 0 << "foo/bar_000";
  QTest::addRow("31") << ""      << true  << "foo" << "bar"         << 1 << "foo/bar_001";
  QTest::addRow("32") << ".fits" << false << ""    << ""            << 0 << "/.+/kstars/000.fits";
  QTest::addRow("33") << ".fits" << false << ""    << ""            << 1 << "/.+/kstars/001.fits";
  QTest::addRow("34") << ".fits" << false << ""    << "_ISO8601"    << 0 << "/.+/kstars/_\\d{4}-\\d{2}-\\d{2}T\\d{2}-\\d{2}-\\d{2}_000.fits";
  QTest::addRow("35") << ".fits" << false << ""    << "_ISO8601"    << 1 << "/.+/kstars/_\\d{4}-\\d{2}-\\d{2}T\\d{2}-\\d{2}-\\d{2}_001.fits";
  QTest::addRow("36") << ".fits" << false << ""    << "_ISO8601bar" << 0 << "/.+/kstars/_\\d{4}-\\d{2}-\\d{2}T\\d{2}-\\d{2}-\\d{2}bar_000.fits";
  QTest::addRow("37") << ".fits" << false << ""    << "_ISO8601bar" << 1 << "/.+/kstars/_\\d{4}-\\d{2}-\\d{2}T\\d{2}-\\d{2}-\\d{2}bar_001.fits";
  QTest::addRow("38") << ".fits" << false << ""    << "bar"         << 0 << "/.+/kstars/bar_000.fits";
  QTest::addRow("39") << ".fits" << false << ""    << "bar"         << 1 << "/.+/kstars/bar_001.fits";
  QTest::addRow("40") << ".fits" << false << "foo" << ""            << 0 << "/.+/kstars/000.fits";
  QTest::addRow("41") << ".fits" << false << "foo" << ""            << 1 << "/.+/kstars/001.fits";
  QTest::addRow("42") << ".fits" << false << "foo" << "_ISO8601"    << 0 << "/.+/kstars/_\\d{4}-\\d{2}-\\d{2}T\\d{2}-\\d{2}-\\d{2}_000.fits";
  QTest::addRow("43") << ".fits" << false << "foo" << "_ISO8601"    << 1 << "/.+/kstars/_\\d{4}-\\d{2}-\\d{2}T\\d{2}-\\d{2}-\\d{2}_001.fits";
  QTest::addRow("44") << ".fits" << false << "foo" << "_ISO8601bar" << 0 << "/.+/kstars/_\\d{4}-\\d{2}-\\d{2}T\\d{2}-\\d{2}-\\d{2}bar_000.fits";
  QTest::addRow("45") << ".fits" << false << "foo" << "_ISO8601bar" << 1 << "/.+/kstars/_\\d{4}-\\d{2}-\\d{2}T\\d{2}-\\d{2}-\\d{2}bar_001.fits";
  QTest::addRow("46") << ".fits" << false << "foo" << "bar"         << 0 << "/.+/kstars/bar_000.fits";
  QTest::addRow("47") << ".fits" << false << "foo" << "bar"         << 1 << "/.+/kstars/bar_001.fits";
  QTest::addRow("48") << ".fits" << true  << ""    << ""            << 0 << "/.+/000.fits";
  QTest::addRow("49") << ".fits" << true  << ""    << ""            << 1 << "/.+/001.fits";
  QTest::addRow("50") << ".fits" << true  << ""    << "_ISO8601"    << 0 << "/.+/_\\d{4}-\\d{2}-\\d{2}T\\d{2}-\\d{2}-\\d{2}_000.fits";
  QTest::addRow("51") << ".fits" << true  << ""    << "_ISO8601"    << 1 << "/.+/_\\d{4}-\\d{2}-\\d{2}T\\d{2}-\\d{2}-\\d{2}_001.fits";
  QTest::addRow("52") << ".fits" << true  << ""    << "_ISO8601bar" << 0 << "/.+/_\\d{4}-\\d{2}-\\d{2}T\\d{2}-\\d{2}-\\d{2}bar_000.fits";
  QTest::addRow("53") << ".fits" << true  << ""    << "_ISO8601bar" << 1 << "/.+/_\\d{4}-\\d{2}-\\d{2}T\\d{2}-\\d{2}-\\d{2}bar_001.fits";
  QTest::addRow("54") << ".fits" << true  << ""    << "bar"         << 0 << "/.+/bar_000.fits";
  QTest::addRow("55") << ".fits" << true  << ""    << "bar"         << 1 << "/.+/bar_001.fits";
  QTest::addRow("56") << ".fits" << true  << "foo" << ""            << 0 << "foo/000.fits";
  QTest::addRow("57") << ".fits" << true  << "foo" << ""            << 1 << "foo/001.fits";
  QTest::addRow("58") << ".fits" << true  << "foo" << "_ISO8601"    << 0 << "foo/_\\d{4}-\\d{2}-\\d{2}T\\d{2}-\\d{2}-\\d{2}_000.fits";
  QTest::addRow("59") << ".fits" << true  << "foo" << "_ISO8601"    << 1 << "foo/_\\d{4}-\\d{2}-\\d{2}T\\d{2}-\\d{2}-\\d{2}_001.fits";
  QTest::addRow("60") << ".fits" << true  << "foo" << "_ISO8601bar" << 0 << "foo/_\\d{4}-\\d{2}-\\d{2}T\\d{2}-\\d{2}-\\d{2}bar_000.fits";
  QTest::addRow("61") << ".fits" << true  << "foo" << "_ISO8601bar" << 1 << "foo/_\\d{4}-\\d{2}-\\d{2}T\\d{2}-\\d{2}-\\d{2}bar_001.fits";
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
  placeholderPath.generateFilename(format, batch_mode, &filename, fitsDir, seqPrefix, nextSequenceID);

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
  const QList<const char*> columns = {
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
  for (const auto &column: columns) {
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
  placeholderPath.generateFilename(".fits", bool(batch_mode.toInt()), &filename, fitsDir, job.getFullPrefix(), nextSequenceID.toInt());
  //QCOMPARE(filename, result);
  QVERIFY2(QRegularExpression(result).match(filename).hasMatch(),
           QString("\nExpected: %1\nObtained: %2\n").arg(result, filename).toStdString().c_str());

#endif
}

QTEST_GUILESS_MAIN(TestPlaceholderPath)
