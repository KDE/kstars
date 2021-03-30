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

TestPlaceholderPath::TestPlaceholderPath() : QObject()
{
}

TestPlaceholderPath::~TestPlaceholderPath()
{
}

void TestPlaceholderPath::testSchedulerProcessJobInfo_data()
{
#if QT_VERSION < 0x050900
    QSKIP("Skipping fixture-based test on old QT version.");
#else
  QTest::addColumn<QString>("Exposure");
  QTest::addColumn<QString>("Filter");
  QTest::addColumn<QString>("Type");
  QTest::addColumn<QString>("Prefix");
  QTest::addColumn<QString>("RawPrefix");
  QTest::addColumn<QString>("FilterEnabled");
  QTest::addColumn<QString>("ExpEnabled");
  QTest::addColumn<QString>("FITSDirectory");
  QTest::addColumn<QString>("targetName");
  QTest::addColumn<QString>("signature");

  QFile testDataFile(":/testSchedulerProcessJobInfo_data.csv");

  if (!testDataFile.open(QIODevice::ReadOnly)) {
    qDebug() << testDataFile.errorString();
    QFAIL("error");
  }

  int cpt = 0;
  while (!testDataFile.atEnd()) {
    QByteArray line = testDataFile.readLine().replace("\n", "");
    QTestData& row = QTest::addRow("%d", cpt);
    for (auto el: line.split(',')) {
      row << QString::fromStdString(el.toStdString());
    }
    cpt++;
  }
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

  Ekos::SequenceJob job(root);
  QCOMPARE(job.getFilterName(), Filter);
  auto placeholderPath = Ekos::PlaceholderPath();
  placeholderPath.processJobInfo(&job, targetName);

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
