/***************************************************************************
                 TestFWParser.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 2012/24/07
    copyright            : (C) 2012 by Rishab Arora
    email                : ra.rishab@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "testfwparser.h"

TestFWParser::TestFWParser(): QObject() {
}

void TestFWParser::initTestCase() {
  test_cases_.append(
    "this is an exam ple of 256 cases being tested -3.14       times\n");
  test_cases_.append(
    "                                                               \n");
  test_cases_.append("this is an ex\n\n");

  QString file_name("TestFW.txt");
  file_name = KStandardDirs::locateLocal("appdata", file_name);

  if (!file_name.isNull()) {
        test_file_.setFileName(file_name);
        if (!test_file_.open(QIODevice::WriteOnly)) {
          kWarning() << QString("Couldn't open(%1)").arg(file_name);
        }
  }

  QTextStream out_stream(&test_file_);
  foreach(const QString &test_case, test_cases_)
    out_stream << test_case;
  test_file_.close();

  //Building the sequence to be used. Includes all available types.
  sequence_.clear();
  sequence_.append(qMakePair(QString("field1"), KSParser::D_QSTRING));
  sequence_.append(qMakePair(QString("field2"), KSParser::D_QSTRING));
  sequence_.append(qMakePair(QString("field3"), KSParser::D_QSTRING));
  sequence_.append(qMakePair(QString("field4"), KSParser::D_QSTRING));
  sequence_.append(qMakePair(QString("field5"), KSParser::D_QSTRING));
  sequence_.append(qMakePair(QString("field6"), KSParser::D_INT));
  sequence_.append(qMakePair(QString("field7"), KSParser::D_QSTRING));
  sequence_.append(qMakePair(QString("field8"), KSParser::D_QSTRING));
  sequence_.append(qMakePair(QString("field9"), KSParser::D_QSTRING));
  sequence_.append(qMakePair(QString("field10"), KSParser::D_FLOAT));
  sequence_.append(qMakePair(QString("field11"), KSParser::D_QSTRING));
  sequence_.append(qMakePair(QString("field12"), KSParser::D_QSTRING));
  widths_.append(5);
  widths_.append(3);
  widths_.append(3);
  widths_.append(9);
  widths_.append(3);
  widths_.append(4);
  widths_.append(6);
  widths_.append(6);
  widths_.append(7);
  widths_.append(6);
  widths_.append(6);  //'repeatedly' doesn't need a width

  QString fname = KStandardDirs::locate( "appdata", file_name );
  test_parser_ = new KSParser(fname, '#', sequence_, widths_);
}

TestFWParser::~TestFWParser()
{
}

void TestFWParser::cleanupTestCase() {
  delete test_parser_;
}

void TestFWParser::MixedInputs() {
  /*
   * Test 1: Checks all conversions are working as expected
  */
  QHash<QString, QVariant> row_content = test_parser_->ReadNextRow();

  QVERIFY(row_content["field1"] == QString("this"));
  QVERIFY(row_content["field2"] == QString("is"));
  QVERIFY(row_content["field3"] == QString("an"));
  QVERIFY(row_content["field4"] == QString("exam ple"));
  QVERIFY(row_content["field5"] == QString("of"));
  QVERIFY(row_content["field6"].toInt() == 256);
  QVERIFY(row_content["field7"] == QString("cases"));
  QVERIFY(row_content["field8"] == QString("being"));
  QVERIFY(row_content["field9"] == QString("tested"));
  QVERIFY(row_content["field10"].toFloat() + 3.141 < 0.1);
  QVERIFY(row_content["field11"] == QString(""));
  QVERIFY(row_content["field12"] == QString("times"));
}

void TestFWParser::OnlySpaceRow() {
  /*
   * Test 2: Checks what happens in case of reading an empty space row
  */
  QHash<QString, QVariant> row_content = test_parser_->ReadNextRow();

  QVERIFY(row_content["field1"] == QString(""));
  QVERIFY(row_content["field2"] == QString(""));
  QVERIFY(row_content["field3"] == QString(""));
  QVERIFY(row_content["field4"] == QString(""));
  QVERIFY(row_content["field5"] == QString(""));
  QVERIFY(row_content["field6"].toInt() == 0);
  QVERIFY(row_content["field7"] == QString(""));
  QVERIFY(row_content["field8"] == QString(""));
  QVERIFY(row_content["field9"] == QString(""));
  QVERIFY(row_content["field10"].toFloat() == 0.0);
  QVERIFY(row_content["field11"] == QString(""));
  QVERIFY(row_content["field12"] == QString(""));
}

void TestFWParser::NoRow() {
  /*
   *  Test 3:
   *  This test also tests what happens if we have a partial row or a
   *  truncated row. It is simply skipped.
   *
   * It then reaches a point where the file ends.
   * We attempt reading a file after EOF 20 times
  */
  QHash<QString, QVariant> row_content;
  qDebug() << row_content["field12"];

  for (int times = 0; times < 20; times++) {
    row_content = test_parser_->ReadNextRow();
    QVERIFY(row_content["field1"] == QString("Null"));
    QVERIFY(row_content["field2"] == QString("Null"));
    QVERIFY(row_content["field3"] == QString("Null"));
    QVERIFY(row_content["field4"] == QString("Null"));
    QVERIFY(row_content["field5"] == QString("Null"));
    QVERIFY(row_content["field6"].toInt() == 0);
    QVERIFY(row_content["field7"] == QString("Null"));
    QVERIFY(row_content["field8"] == QString("Null"));
    QVERIFY(row_content["field9"] == QString("Null"));
    QVERIFY(row_content["field10"].toFloat() == 0.0);
    QVERIFY(row_content["field11"] == QString("Null"));
    QVERIFY(row_content["field12"] == QString("Null"));
  }
}

void TestFWParser::FWReadMissingFile()
{
  /*
   * Test 4:
   * This tests how the parser reacts if there is no file with the
   * given path.
  */
  QFile::remove(KStandardDirs::locateLocal("appdata","TestFW.txt"));

  KSParser missing_parser(QString("TestFW.txt"), '#', sequence_, widths_);
  QHash<QString, QVariant> row_content = missing_parser.ReadNextRow();

  for (int times = 0; times < 20; times++) {
    row_content = missing_parser.ReadNextRow();
    QVERIFY(row_content["field1"] == QString("Null"));
    QVERIFY(row_content["field2"] == QString("Null"));
    QVERIFY(row_content["field3"] == QString("Null"));
    QVERIFY(row_content["field4"] == QString("Null"));
    QVERIFY(row_content["field5"] == QString("Null"));
    QVERIFY(row_content["field6"].toInt() == 0);
    QVERIFY(row_content["field7"] == QString("Null"));
    QVERIFY(row_content["field8"] == QString("Null"));
    QVERIFY(row_content["field9"] == QString("Null"));
    QVERIFY(row_content["field10"].toFloat() == 0.0);
    QVERIFY(row_content["field11"] == QString("Null"));
    QVERIFY(row_content["field12"] == QString("Null"));
  }
}



QTEST_MAIN(TestFWParser)

#include "testfwparser.moc"

