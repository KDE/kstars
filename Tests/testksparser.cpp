/***************************************************************************
                 TestKSParser.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 2012/23/07
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

/*
  * Justification for not testing CombineQuoteParts separately:
  * Changing the structure of KSParser would solve this (by removing the
  * segments to be tested into separate classes) but would unnecessarily
  * complicate things resulting in a large number of small classes.
  * This is good from an OOD perspective, but would make the code unmanageable
*/

#include "testksparser.h"

TestKSParser::TestKSParser(): QObject() {
  /*
   * Justification for doing this instead of simply creating a file:
   * To add/change tests, we'll need to modify 2 places. The file and this class.
   * So we write the file from the class.
   */
  csv_test_cases_.append("\n");
  csv_test_cases_.append(QString(","
                         "isn't,"
                         "it,"
                         "\"amusing\","
                         "how,"
                         "3,"
                         "\"isn't, pi\","
                         "and,"
                         "\"\","
                         "-3.141,"
                         "isn't,"
                         "either\n"));
  csv_test_cases_.append(QString(","
                         "isn't,"
                         "it,"
                         "\"amusing\","
                         "how,"
                         "3,"
                         "\"isn't, pi\","
                         "and,"
                         "\"\","));  // less than required fields
  csv_test_cases_.append(QString(","
                         "isn't,"
                         "it,"
                         "\"amusing\","
                         "how,"
                         "3,"
                         "\"isn't, pi\","
                         "and,"
                         "\","  // no matching "
                         "-3.141,"
                         "isn't,"
                         "either\n"));
  csv_test_cases_.append(",,,,,,,,,,,\n");
  csv_test_cases_.append("\n");
  QString file_name("TestCSV.txt");
  file_name = KStandardDirs::locateLocal("appdata",file_name);
  if (!file_name.isNull()) {
        test_csv_file_.setFileName(file_name);
        if (!test_csv_file_.open(QIODevice::WriteOnly)) {
          kWarning() << QString("Couldn't open(%1)").arg(file_name);
        }
  }
  QTextStream out_stream(&test_csv_file_);
  foreach(const QString &test_case, csv_test_cases_)
    out_stream << test_case;
  test_csv_file_.close();

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
  test_parser_ = new KSParser(QString("TestCSV.txt"), '#', sequence_);
}

TestKSParser::~TestKSParser()
{
  delete test_parser_;
}

  /*
   * The following tests checks for the following cases for CSV files
   *  1. Mixed inputs (See test case for description)
   *  2. Empty Row
   *  3. No row (only a newline character)
   *  4. Truncated row
   *  5. Row with no matching quote
   *  6. Attempt to read missing file
   * 
  */


void TestKSParser::MixedInputs() {
  /*
   * Test 1. Includes input of the form: 
   * 
   * It starts with a newline char which should be skipped by virtue
   * of the design of the parser
   * 
   * Then a row with the following types of inputs:
   * 1. empty column
   * 2. simple single word
   * 3. single word in quotes
   * 4. multiple words with , in quotes
   * 5. integer
   * 6. float
  */
//   KSParser test_parser(QString("TestCSV.txt"), '#', sequence);
  QHash<QString, QVariant> row_content = test_parser_->ReadNextRow();
  qDebug() << row_content["field1"];
  QVERIFY(row_content["field1"] == QString(""));
  QVERIFY(row_content["field2"] == QString("isn't"));
  QVERIFY(row_content["field3"] == QString("it"));
  QVERIFY(row_content["field4"] == QString("amusing"));
  QVERIFY(row_content["field5"] == QString("how"));
  QVERIFY(row_content["field6"].toInt() == 3);
  QVERIFY(row_content["field7"] == QString("isn't, pi"));
  QVERIFY(row_content["field8"] == QString("and"));
  QVERIFY(row_content["field9"] == QString(""));
  QVERIFY(row_content["field10"].toFloat() + 3.141 < 0.1);
  QVERIFY(row_content["field11"] == QString("isn't"));
  QVERIFY(row_content["field12"] == QString("either"));
}

void TestKSParser::EmptyRow() {
  /* Test 2. Row with less rows than required (to be skipped)
   * Test 3. Row with truncated \" i.e. no matching "
   *         (should be skipped)
  */
  /*
   * Test 4. Attempt to read an empty but valid row
   * 
   * Also includes test for:
   * 1. missing integer
   * 2. missing float
   * 3. missing string
  */
//   KSParser test_parser(QString("TestCSV.txt"), '#', sequence);
  QHash<QString, QVariant> row_content = test_parser_->ReadNextRow();
  qDebug() << row_content["field1"];
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

void TestKSParser::NoRow() {
  /*
   * Test 3. Attempt to read a newline char instead of a row
   * The parser is designed to skip an empty row so we can
   * test this for a boundary case. i.e. newline at the end.
  */
//   KSParser test_parser(QString("TestCSV.txt"), '#', sequence);
  QHash<QString, QVariant> row_content = test_parser_->ReadNextRow();
  qDebug() << row_content["field1"];
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

void TestKSParser::ReadMissingFile() {
  /*
   * Test 6. Attempt to read a missing file repeatedly
  */
  QFile::remove(KStandardDirs::locateLocal("appdata","TestCSV.txt"));
  
  KSParser missing_parser(QString("TestCSV.txt"), '#', sequence_);
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


QTEST_MAIN(TestKSParser)

#include "testksparser.moc"
