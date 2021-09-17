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

QTEST_GUILESS_MAIN(TestPlaceholderPath)
