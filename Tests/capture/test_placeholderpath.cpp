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

TestPlaceholderPath::TestPlaceholderPath() : QObject()
{
}

TestPlaceholderPath::~TestPlaceholderPath()
{
}

void TestPlaceholderPath::testSequenceJobSignature_data()
{
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
}

Q_DECLARE_METATYPE(CCDFrameType)

void TestPlaceholderPath::testSequenceJobSignature()
{
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
}

QTEST_GUILESS_MAIN(TestPlaceholderPath)
